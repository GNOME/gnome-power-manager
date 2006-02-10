/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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
#include <gtk/gtk.h>

#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-brightness.h"
#include "gpm-hal.h"

#define DIM_INTERVAL		10 /* ms */

static void     gpm_brightness_class_init (GpmBrightnessClass *klass);
static void     gpm_brightness_init       (GpmBrightness      *brightness);
static void     gpm_brightness_finalize   (GObject	      *object);

#define GPM_BRIGHTNESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS, GpmBrightnessPrivate))

struct GpmBrightnessPrivate
{
	gboolean	    has_hardware;
	int	    current_hw;
	int	    levels;
	char	   *udi;
	DBusGProxy *proxy;
};

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (GpmBrightness, gpm_brightness, G_TYPE_OBJECT)

static gboolean
gpm_brightness_level_update_hw (GpmBrightness *brightness);

static void
gpm_brightness_set_property (GObject		  *object,
			     guint		   prop_id,
			     const GValue	  *value,
			     GParamSpec		  *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_brightness_get_property (GObject		  *object,
			     guint		   prop_id,
			     GValue		  *value,
			     GParamSpec		  *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GObject *
gpm_brightness_constructor (GType                  type,
			    guint                  n_construct_properties,
			    GObjectConstructParam *construct_properties)
{
	GpmBrightness      *brightness;
	GpmBrightnessClass *klass;
	klass = GPM_BRIGHTNESS_CLASS (g_type_class_peek (GPM_TYPE_BRIGHTNESS));
	brightness = GPM_BRIGHTNESS (G_OBJECT_CLASS (gpm_brightness_parent_class)->constructor
			      	     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

static void
gpm_brightness_class_init (GpmBrightnessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize	   = gpm_brightness_finalize;
	object_class->get_property = gpm_brightness_get_property;
	object_class->set_property = gpm_brightness_set_property;
	object_class->constructor  = gpm_brightness_constructor;
	g_type_class_add_private (klass, sizeof (GpmBrightnessPrivate));
}

static void
gpm_brightness_init (GpmBrightness *brightness)
{
	DBusGConnection *system_connection = NULL;
	gchar          **names;

	brightness->priv = GPM_BRIGHTNESS_GET_PRIVATE (brightness);

	/* save udi of lcd adapter */
	gpm_hal_find_device_capability ("laptop_panel", &names);
	if (names == NULL || names[0] == NULL) {
		brightness->priv->has_hardware = FALSE;
		gpm_debug ("No devices of capability laptop_panel");
		return;
	}

	brightness->priv->has_hardware = TRUE;
	/* We only want first laptop_panel object (should only be one) */
	brightness->priv->udi = g_strdup (names[0]);
	gpm_hal_free_capability (names);

	/* get proxy once and store */
	gpm_hal_get_dbus_connection (&system_connection);
	brightness->priv->proxy = dbus_g_proxy_new_for_name (system_connection,
							     HAL_DBUS_SERVICE,
							     brightness->priv->udi,
							     HAL_DBUS_INTERFACE_LAPTOP_PANEL);

	/* get levels that the adapter supports -- this does not change ever */
	gpm_hal_device_get_int (brightness->priv->udi, "laptop_panel.num_levels",
				&brightness->priv->levels);

	/* this changes under our feet */
	gpm_brightness_level_update_hw (brightness);

	gpm_debug ("Starting: (%i of %i)", brightness->priv->current_hw,
		   brightness->priv->levels - 1);
}

static void
gpm_brightness_finalize (GObject *object)
{
	GpmBrightness *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS (object));
	brightness = GPM_BRIGHTNESS (object);

	g_free (brightness->priv->udi);
	g_object_unref (G_OBJECT (brightness->priv->proxy));

	g_return_if_fail (brightness->priv != NULL);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmBrightness *
gpm_brightness_new (void)
{
	GpmBrightness *brightness;
	brightness = g_object_new (GPM_TYPE_BRIGHTNESS, NULL);
	return GPM_BRIGHTNESS (brightness);
}

/* updates the value of brightness_level_hw as it may have changed on some h/w */
static gboolean
gpm_brightness_level_update_hw (GpmBrightness *brightness)
{
	GError  *error = NULL;
	gboolean retval;
	gint     brightness_hw = 0;

	retval = TRUE;
	if (!dbus_g_proxy_call (brightness->priv->proxy, "GetBrightness", &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, &brightness_hw, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("gpm_brightness_level_update_hw: %s", error->message);
			g_error_free (error);
		}
		retval = FALSE;
	}

	brightness->priv->current_hw = brightness_hw;
	return retval;
}


static gboolean
gpm_brightness_level_set_hw (GpmBrightness *brightness,
			     int	    brightness_level_hw)
{
	GError  *error = NULL;
	gint     ret;
	gboolean retval;

	if (brightness_level_hw < 0 ||
	    brightness_level_hw > brightness->priv->levels - 1) {
		g_warning ("gpm_brightness_level_set_hw: set outside range (%i of %i)",
			   brightness_level_hw, brightness->priv->levels - 1);
		return FALSE;
	}

	gpm_debug ("Setting %i of %i", brightness_level_hw, brightness->priv->levels - 1);

	retval = TRUE;
	if (!dbus_g_proxy_call (brightness->priv->proxy, "SetBrightness", &error,
				G_TYPE_INT, brightness_level_hw, G_TYPE_INVALID,
				G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("gpm_brightness_level_set_hw: %s", error->message);
			g_error_free (error);
		}
		retval = FALSE;
	}
	if (ret != 0) {
		retval = FALSE;
	}

	brightness->priv->current_hw = brightness_level_hw;
	return retval;
}

void
gpm_brightness_level_up (GpmBrightness *brightness)
{
	if (! brightness->priv->has_hardware) {
		return;
	}
	int current_hw = brightness->priv->current_hw;
	gpm_brightness_level_set_hw (brightness, current_hw + 1);
}

void
gpm_brightness_level_down (GpmBrightness *brightness)
{
	if (! brightness->priv->has_hardware) {
		return;
	}
	int current_hw = brightness->priv->current_hw;
	gpm_brightness_level_set_hw (brightness, current_hw - 1);
}

/* brightness_level is a percentage */
void
gpm_brightness_level_set (GpmBrightness *brightness,
			  int            brightness_level)
{
	int brightness_level_hw;
	int levels;

	if (! brightness->priv->has_hardware) {
		return;
	}
	levels = brightness->priv->levels;
	brightness_level_hw = ( (float) brightness_level * (float) (levels - 1)) / 100.0f;
	/* only set if different */
	if (brightness_level_hw != brightness->priv->current_hw) {
		gpm_brightness_level_set_hw (brightness, brightness_level_hw);
	}
}

/* brightness_level is a percentage */
void
gpm_brightness_level_dim (GpmBrightness *brightness,
			  int            brightness_level)
{
	char *manufacturer_string = NULL;
	int   new_level_hw;
	int   current_hw;
	int   levels;
	int   a;

	if (! brightness->priv->has_hardware) {
		return;
	}
	/* If the manufacturer is IBM, then assume we are a ThinkPad,
	 * and don't do the new-fangled dimming routine. The ThinkPad dims
	 * gently itself and the two dimming routines just get messy.
	 * https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=173382
	 */
	gpm_hal_device_get_string (HAL_ROOT_COMPUTER,
				   "smbios.system.manufacturer",
				   &manufacturer_string);
	if (manufacturer_string) {
		if (strcmp (manufacturer_string, "IBM") == 0) {
			gpm_brightness_level_set (brightness, brightness_level);
			g_free (manufacturer_string);
			return;
		}
		g_free (manufacturer_string);
	}

	levels = brightness->priv->levels;
	new_level_hw = ( (float) brightness_level * (float) (levels - 1)) / 100.0f;
	current_hw = brightness->priv->current_hw;

	if (new_level_hw > current_hw) {
		/* going up */
		for (a=current_hw; a <= new_level_hw; a++) {
			gpm_brightness_level_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	} else {
		/* going down */
		for (a=current_hw; a >= new_level_hw; a--) {
			gpm_brightness_level_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	}
}
