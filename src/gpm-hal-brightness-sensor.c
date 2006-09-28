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
#include <gtk/gtk.h>

#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-hal.h"
#include "gpm-hal-brightness-sensor.h"
#include "gpm-proxy.h"
#include "gpm-marshal.h"

#define POLL_INTERVAL		10000 /* ms */

#define GPM_HAL_BRIGHTNESS_SENSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_BRIGHTNESS_SENSOR, GpmHalBrightnessSensorPrivate))

struct GpmHalBrightnessSensorPrivate
{
	guint			 current_hw;		/* hardware */
	guint			 levels;
	gchar			*udi;
	GpmProxy		*gproxy;
	GpmHal			*hal;
};

G_DEFINE_TYPE (GpmHalBrightnessSensor, gpm_hal_brightness_sensor, G_TYPE_OBJECT)

/**
 * gpm_hal_brightness_sensor_get_hw:
 * @brightness: This brightness class instance
 *
 * Updates the private local value of brightness_level_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_hal_brightness_sensor_get_hw (GpmHalBrightnessSensor *brightness,
			   	  guint			 *brightness_level_hw)
{
	GError     *error = NULL;
	gboolean    ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_SENSOR (brightness), FALSE);

	proxy = gpm_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected to HAL");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, brightness_level_hw,
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
	return TRUE;
}

/**
 * gpm_hal_brightness_sensor_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_hal_brightness_sensor_get (GpmHalBrightnessSensor *brightness,
			       guint		      *brightness_level)
{
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_SENSOR (brightness), FALSE);

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
					      brightness->priv->levels);
	*brightness_level = percentage;
	return TRUE;
}

/**
 * gpm_hal_brightness_sensor_constructor:
 **/
static GObject *
gpm_hal_brightness_sensor_constructor (GType		  type,
				       guint		  n_construct_properties,
				       GObjectConstructParam *construct_properties)
{
	GpmHalBrightnessSensor      *brightness;
	GpmHalBrightnessSensorClass *klass;
	klass = GPM_HAL_BRIGHTNESS_SENSOR_CLASS (g_type_class_peek (GPM_TYPE_HAL_BRIGHTNESS_SENSOR));
	brightness = GPM_HAL_BRIGHTNESS_SENSOR (G_OBJECT_CLASS (gpm_hal_brightness_sensor_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

/**
 * gpm_hal_brightness_sensor_finalize:
 **/
static void
gpm_hal_brightness_sensor_finalize (GObject *object)
{
	GpmHalBrightnessSensor *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL_BRIGHTNESS_SENSOR (object));
	brightness = GPM_HAL_BRIGHTNESS_SENSOR (object);

	g_free (brightness->priv->udi);
	g_object_unref (brightness->priv->gproxy);
	g_object_unref (brightness->priv->hal);
	g_object_unref (brightness->priv->feedback);

	g_return_if_fail (brightness->priv != NULL);
	G_OBJECT_CLASS (gpm_hal_brightness_sensor_parent_class)->finalize (object);
}

/**
 * gpm_hal_brightness_sensor_class_init:
 **/
static void
gpm_hal_brightness_sensor_class_init (GpmHalBrightnessSensorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_hal_brightness_sensor_finalize;
	object_class->constructor  = gpm_hal_brightness_sensor_constructor;

	g_type_class_add_private (klass, sizeof (GpmHalBrightnessSensorPrivate));
}

/**
 * gpm_hal_brightness_sensor_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect light_sensor objects
 * to *NOT* be removed or added during the session.
 * We only control the first light_sensor object if there are more than one.
 **/
static void
gpm_hal_brightness_sensor_init (GpmHalBrightnessSensor *brightness)
{
	gchar **names;
	gchar *manufacturer_string = NULL;
	gboolean res;

	brightness->priv = GPM_HAL_BRIGHTNESS_SENSOR_GET_PRIVATE (brightness);

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

	/* this changes under our feet */
	gpm_hal_brightness_sensor_get_hw (brightness, &brightness->priv->current_hw);
}

/**
 * gpm_hal_brightness_sensor_has_hw:
 *
 * Self contained function that works out if we have the hardware.
 * If not, we return FALSE and the module is unloaded.
 **/
static gboolean
gpm_hal_brightness_sensor_has_hw (void)
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
 * gpm_hal_brightness_sensor_new:
 * Return value: A new brightness class instance.
 **/
GpmHalBrightnessSensor *
gpm_hal_brightness_sensor_new (void)
{
	GpmHalBrightnessSensor *brightness;

	/* only load an instance of this module if we have the hardware */
	if (gpm_hal_brightness_sensor_has_hw () == FALSE) {
		return FALSE;
	}

	brightness = g_object_new (GPM_TYPE_HAL_BRIGHTNESS_SENSOR, NULL);
	return GPM_HAL_BRIGHTNESS_SENSOR (brightness);
}
