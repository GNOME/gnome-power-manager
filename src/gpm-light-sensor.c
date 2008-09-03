/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <libhal-gdevice.h>
#include <libhal-gmanager.h>
#include <libdbus-proxy.h>

#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-light-sensor.h"
#include "gpm-conf.h"
#include "gpm-marshal.h"

#define GPM_LIGHT_SENSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_LIGHT_SENSOR, GpmLightSensorPrivate))

struct GpmLightSensorPrivate
{
	GpmConf			*conf;
	guint			 current_hw;		/* hardware */
	guint			 levels;
	gfloat			 calibration_abs;
	gchar			*udi;
	gboolean		 has_sensor;
	DbusProxy		*gproxy;
};

enum {
	SENSOR_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_sensor_object = NULL;

G_DEFINE_TYPE (GpmLightSensor, gpm_light_sensor, G_TYPE_OBJECT)

/**
 * gpm_light_sensor_get_hw:
 * @sensor: This sensor class instance
 *
 * Updates the private local value of brightness_level_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_light_sensor_get_hw (GpmLightSensor *sensor)
{
	guint	    sensor_level_hw;
	GError     *error = NULL;
	gboolean    ret;
	DBusGProxy *proxy;
	GArray     *levels;
	int         i;

	g_return_val_if_fail (sensor != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_LIGHT_SENSOR (sensor), FALSE);

	proxy = dbus_proxy_get_proxy (sensor->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected to HAL");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 DBUS_TYPE_G_INT_ARRAY, &levels,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("GetBrightness failed!");
		return FALSE;
	}

	/* work out average */
	sensor_level_hw = 0;
	for (i = 0; i < levels->len; i++) {
		sensor_level_hw += g_array_index (levels, gint, i);
	}
	sensor_level_hw /= levels->len;

	/* save */
	sensor->priv->current_hw = sensor_level_hw;

	g_array_free (levels, TRUE);

	return TRUE;
}

/**
 * gpm_light_sensor_get_absolute:
 * @sensor: This sensor class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_light_sensor_get_absolute (GpmLightSensor *sensor,
			       guint	      *sensor_level)
{
	g_return_val_if_fail (sensor != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_LIGHT_SENSOR (sensor), FALSE);

	if (sensor->priv->has_sensor == FALSE) {
		egg_warning ("no hardware!");
		return FALSE;
	}

	*sensor_level = gpm_discrete_to_percent (sensor->priv->current_hw,
					         sensor->priv->levels);
	return TRUE;
}

/**
 * gpm_light_sensor_calibrate:
 * @sensor: This sensor class instance
 *
 * Calibrates the initial reading for relative changes.
 **/
gboolean
gpm_light_sensor_calibrate (GpmLightSensor *sensor)
{
	gfloat fraction;

	g_return_val_if_fail (sensor != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_LIGHT_SENSOR (sensor), FALSE);

	if (sensor->priv->has_sensor == FALSE) {
		egg_warning ("no hardware!");
		return FALSE;
	}

	fraction = gpm_discrete_to_fraction (sensor->priv->current_hw,
					     sensor->priv->levels);
	sensor->priv->calibration_abs = fraction;
	egg_debug ("calibrating to %f", fraction);
	return TRUE;
}

/**
 * gpm_light_sensor_get_relative:
 * @sensor: This sensor class instance
 *
 * Gets the relative brightness, centered around 1.0.
 **/
gboolean
gpm_light_sensor_get_relative (GpmLightSensor *sensor,
			       gfloat	      *difference)
{
	gfloat absolute;
	g_return_val_if_fail (sensor != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_LIGHT_SENSOR (sensor), FALSE);

	if (sensor->priv->has_sensor == FALSE) {
		egg_warning ("no hardware!");
		return FALSE;
	}
	if (sensor->priv->calibration_abs < 0.01 && sensor->priv->calibration_abs > -0.01) {
		egg_warning ("not calibrated!");
		return FALSE;
	}

	absolute = gpm_discrete_to_percent (sensor->priv->current_hw,
					    sensor->priv->levels);
	*difference = (absolute - sensor->priv->calibration_abs) + 1.0;
	return TRUE;
}

/**
 * gpm_light_sensor_finalize:
 **/
static void
gpm_light_sensor_finalize (GObject *object)
{
	GpmLightSensor *sensor;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_LIGHT_SENSOR (object));
	sensor = GPM_LIGHT_SENSOR (object);

	g_free (sensor->priv->udi);
	if (sensor->priv->gproxy != NULL) {
		g_object_unref (sensor->priv->gproxy);
	}

	g_return_if_fail (sensor->priv != NULL);
	G_OBJECT_CLASS (gpm_light_sensor_parent_class)->finalize (object);
}

/**
 * gpm_light_sensor_class_init:
 **/
static void
gpm_light_sensor_class_init (GpmLightSensorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_light_sensor_finalize;

	signals [SENSOR_CHANGED] =
		g_signal_new ("sensor-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmLightSensorClass, sensor_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmLightSensorPrivate));
}

/**
 * gpm_light_sensor_has_hw:
 */
gboolean
gpm_light_sensor_has_hw (GpmLightSensor *sensor)
{
	g_return_val_if_fail (sensor != NULL, FALSE);
	if (sensor->priv->has_sensor) {
		return TRUE;
	}
	return FALSE;
}

/**
 * gpm_light_sensor_poll_cb:
 * @userdata: userdata; a sensor sensor class instance
 */
static gboolean
gpm_light_sensor_poll_cb (gpointer userdata)
{
	guint new;
	gboolean ret;
	gboolean enable;
	GpmLightSensor *sensor;

	g_return_val_if_fail (userdata != NULL, TRUE);

	sensor = GPM_LIGHT_SENSOR (userdata);

	/* check if we should poll the h/w */
	gpm_conf_get_bool (sensor->priv->conf, GPM_CONF_AMBIENT_ENABLE, &enable);
	if (enable == FALSE) {
		/* don't bother polling */
		return TRUE;
	}

	if (sensor->priv->has_sensor) {
		/* fairly slow */
		ret = gpm_light_sensor_get_hw (sensor);

		/* this could fail if hal refuses us */
		if (ret) {
			gpm_light_sensor_get_absolute (sensor, &new);
			egg_debug ("brightness = %i, %i", sensor->priv->current_hw, new);
			g_signal_emit (sensor, signals [SENSOR_CHANGED], 0, new);
		}
	}

	return TRUE;
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmLightSensor *sensor)
{
	//
}

/**
 * gpm_light_sensor_init:
 * @sensor: This sensor class instance
 *
 * initialises the sensor class. NOTE: We expect light_sensor objects
 * to *NOT* be removed or added during the session.
 * We only control the first light_sensor object if there are more than one.
 **/
static void
gpm_light_sensor_init (GpmLightSensor *sensor)
{
	gchar **names;
	HalGManager *manager;
	HalGDevice *device;
	guint timeout;

	sensor->priv = GPM_LIGHT_SENSOR_GET_PRIVATE (sensor);
	sensor->priv->udi = NULL;
	sensor->priv->gproxy = NULL;
	sensor->priv->udi = NULL;
	sensor->priv->calibration_abs = 0.0f;

	sensor->priv->conf = gpm_conf_new ();
	g_signal_connect (sensor->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), sensor);

	/* see if we can find  */
	manager = hal_gmanager_new ();
	hal_gmanager_find_capability (manager, "light_sensor", &names, NULL);
	g_object_unref (manager);

	/* Significant found */
	if (names != NULL && names[0] != NULL) {
		/* We only want first light_sensor object (should only be one) */
		sensor->priv->udi = g_strdup (names[0]);
		sensor->priv->has_sensor = TRUE;
	}
	hal_gmanager_free_capability (names);

	/* connect to the devices */
	if (sensor->priv->has_sensor) {
		egg_debug ("Using proper brightness sensor");
		/* get a managed proxy */
		sensor->priv->gproxy = dbus_proxy_new ();
		dbus_proxy_assign (sensor->priv->gproxy,
				  DBUS_PROXY_SYSTEM,
				  HAL_DBUS_SERVICE,
				  sensor->priv->udi,
				  HAL_DBUS_INTERFACE_LIGHT_SENSOR);

		/* get levels that the adapter supports -- this does not change ever */
		device = hal_gdevice_new ();
		hal_gdevice_set_udi (device, sensor->priv->udi);
		hal_gdevice_get_uint (device, "light_sensor.num_levels",
				      &sensor->priv->levels, NULL);
		g_object_unref (device);

		/* this changes under our feet */
		gpm_light_sensor_get_hw (sensor);
	}

	/* do we have a info source? */
	if (sensor->priv->has_sensor) {
		egg_debug ("current brightness is %i%%", sensor->priv->current_hw);

		/* get poll timeout */
		gpm_conf_get_uint (sensor->priv->conf, GPM_CONF_AMBIENT_POLL, &timeout);
		g_timeout_add (timeout * 1000, gpm_light_sensor_poll_cb, sensor);
	}
}

/**
 * gpm_light_sensor_new:
 * Return value: A new sensor class instance.
 * Can return NULL if no suitable hardware is found.
 **/
GpmLightSensor *
gpm_light_sensor_new (void)
{
	if (gpm_sensor_object != NULL) {
		g_object_ref (gpm_sensor_object);
	} else {
		gpm_sensor_object = g_object_new (GPM_TYPE_LIGHT_SENSOR, NULL);
		g_object_add_weak_pointer (gpm_sensor_object, &gpm_sensor_object);
	}
	return GPM_LIGHT_SENSOR (gpm_sensor_object);
}
