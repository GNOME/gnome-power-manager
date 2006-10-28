/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-hal.h"
#include "gpm-light-sensor.h"
#include "gpm-proxy.h"
#include "gpm-marshal.h"

#define POLL_INTERVAL		10000 /* ms */

#define GPM_LIGHT_SENSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_LIGHT_SENSOR, GpmLightSensorPrivate))

struct GpmLightSensorPrivate
{
	guint			 current_hw;		/* hardware */
	guint			 levels;
	gchar			*udi;
	GpmProxy		*gproxy;
	GpmHal			*hal;
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };


G_DEFINE_TYPE (GpmLightSensor, gpm_light_sensor, G_TYPE_OBJECT)

/**
 * gpm_light_sensor_get_hw:
 * @brightness: This brightness class instance
 *
 * Updates the private local value of brightness_level_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_light_sensor_get_hw (GpmLightSensor *brightness,
		         guint	        *brightness_level_hw)
{
	GError     *error = NULL;
	gboolean    ret;
	DBusGProxy *proxy;
	GArray     *levels;
	int         i;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_LIGHT_SENSOR (brightness), FALSE);

	proxy = gpm_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected to HAL");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 DBUS_TYPE_G_INT_ARRAY, &levels,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetBrightness failed!");
		return FALSE;
	}

	*brightness_level_hw = 0;
	for (i = 0; i < levels->len; i++ ) {
		*brightness_level_hw += g_array_index (levels, gint, i);
	}
	*brightness_level_hw /= levels->len;

	g_array_free (levels, TRUE);

	return TRUE;
}

/**
 * gpm_light_sensor_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_light_sensor_get (GpmLightSensor *brightness,
		      guint	     *brightness_level)
{
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_LIGHT_SENSOR (brightness), FALSE);

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
					      brightness->priv->levels);
	*brightness_level = percentage;
	return TRUE;
}

/**
 * gpm_light_sensor_constructor:
 **/
static GObject *
gpm_light_sensor_constructor (GType		  type,
			      guint		  n_construct_properties,
			      GObjectConstructParam *construct_properties)
{
	GpmLightSensor      *brightness;
	GpmLightSensorClass *klass;
	klass = GPM_LIGHT_SENSOR_CLASS (g_type_class_peek (GPM_TYPE_LIGHT_SENSOR));
	brightness = GPM_LIGHT_SENSOR (G_OBJECT_CLASS (gpm_light_sensor_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

/**
 * gpm_light_sensor_finalize:
 **/
static void
gpm_light_sensor_finalize (GObject *object)
{
	GpmLightSensor *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_LIGHT_SENSOR (object));
	brightness = GPM_LIGHT_SENSOR (object);

	g_free (brightness->priv->udi);
	g_object_unref (brightness->priv->gproxy);
	g_object_unref (brightness->priv->hal);

	g_return_if_fail (brightness->priv != NULL);
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
	object_class->constructor  = gpm_light_sensor_constructor;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmLightSensorClass, brightness_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmLightSensorPrivate));
}

/**
 * gpm_light_sensor_poll_hardware
 * @brightness: This brightness class instance
 */
gboolean
gpm_light_sensor_poll_hardware (GpmLightSensor *brightness)
{
	gboolean ret;
	guint old;
	guint new;

	gpm_light_sensor_get (brightness, &old);

	ret = gpm_light_sensor_get_hw (brightness, &brightness->priv->current_hw);

	if (ret) {
		gpm_light_sensor_get (brightness, &new);
		if (new != old) {
			g_signal_emit (brightness,
				       signals [BRIGHTNESS_CHANGED], 0, new, old);
		}
	}

	return ret;
}

/**
 * gpm_light_sensor_poll_cb:
 * @userdata: userdata; a brightness sensor class instance
 */
static gboolean
gpm_light_sensor_poll_cb (gpointer userdata)
{
	GpmLightSensor *brightness;
	g_return_val_if_fail (userdata != NULL, TRUE);
	g_return_val_if_fail (GPM_IS_LIGHT_SENSOR (userdata), TRUE);
	brightness = GPM_LIGHT_SENSOR (userdata);

	gpm_light_sensor_poll_hardware (brightness);

	return TRUE;
}

/**
 * gpm_light_sensor_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect light_sensor objects
 * to *NOT* be removed or added during the session.
 * We only control the first light_sensor object if there are more than one.
 **/
static void
gpm_light_sensor_init (GpmLightSensor *brightness)
{
	gchar **names;

	brightness->priv = GPM_LIGHT_SENSOR_GET_PRIVATE (brightness);

	brightness->priv->hal = gpm_hal_new ();

	/* save udi of lcd adapter */
	gpm_hal_device_find_capability (brightness->priv->hal, "light_sensor", &names);
	if (names == NULL || names[0] == NULL) {
		gpm_warning ("No devices of capability light_sensor");
		return;
	}

	/* We only want first light_sensor object (should only be one) */
	brightness->priv->udi = g_strdup (names[0]);
	gpm_hal_free_capability (brightness->priv->hal, names);

	/* get a managed proxy */
	brightness->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (brightness->priv->gproxy,
			  GPM_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  brightness->priv->udi,
			  HAL_DBUS_INTERFACE_LIGHT_SENSOR);

	/* get levels that the adapter supports -- this does not change ever */
	gpm_hal_device_get_uint (brightness->priv->hal, brightness->priv->udi, "light_sensor.num_levels",
				 &brightness->priv->levels);

	/* this changes under our feet */
	gpm_light_sensor_get_hw (brightness, &brightness->priv->current_hw);

	g_timeout_add (2000, gpm_light_sensor_poll_cb, brightness);
}

/**
 * gpm_light_sensor_has_hw:
 *
 * Self contained function that works out if we have the hardware.
 * If not, we return FALSE and the module is unloaded.
 **/
static gboolean
gpm_light_sensor_has_hw (void)
{
	GpmHal *hal;
	gchar **names;
	gboolean ret = TRUE;

	/* okay, as singleton - so we don't allocate more memory */
	hal = gpm_hal_new ();
	gpm_hal_device_find_capability (hal, "light_sensor", &names);

	/* nothing found */
	if (names == NULL || names[0] == NULL) {
		ret = FALSE;
	}

	gpm_hal_free_capability (hal, names);
	g_object_unref (hal);
	return ret;
}

/**
 * gpm_light_sensor_new:
 * Return value: A new brightness class instance.
 **/
GpmLightSensor *
gpm_light_sensor_new (void)
{
	static GpmLightSensor *brightness = NULL;

	if (brightness != NULL) {
		g_object_ref (brightness);
		return brightness;
	}

	/* only load an instance of this module if we have the hardware */
	if (gpm_light_sensor_has_hw () == FALSE) {
		return NULL;
	}

	brightness = g_object_new (GPM_TYPE_LIGHT_SENSOR, NULL);
	return brightness;
}
