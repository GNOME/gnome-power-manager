/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#include "gpm-brightness.h"
#include "gpm-brightness-hal.h"
#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-marshal.h"

#define GPM_BRIGHTNESS_HAL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS_HAL, GpmBrightnessHalPrivate))

struct GpmBrightnessHalPrivate
{
	guint			 last_set_hw;
	guint			 level_std_hw;
	guint			 levels;
	gchar			*udi;
	gboolean		 hw_changed;
	DbusProxy		*gproxy;

 	/* true if hardware automatically sets brightness in response to
 	 * key press events */
 	gboolean		 does_own_updates;
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (GpmBrightnessHal, gpm_brightness_hal, G_TYPE_OBJECT)
static guint signals [LAST_SIGNAL] = { 0 };

/**
 * gpm_brightness_hal_get_hw:
 * @brightness: This brightness class instance
 *
 * Updates the private local value of value_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_brightness_hal_get_hw (GpmBrightnessHal *brightness, guint *value_hw)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;
	gint level = 0;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);

	proxy = dbus_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected to HAL");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 G_TYPE_INT, &level,
				 G_TYPE_INVALID);

	if (value_hw != NULL) {
		*value_hw = (guint)level;
	}

	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("GetBrightness failed!");
		return FALSE;
	}
	egg_debug ("GetBrightness returned level: %i", level);

	return TRUE;
}

/**
 * gpm_brightness_hal_set_hw:
 * @brightness: This brightness class instance
 * @value_hw: The hardware level in raw units
 *
 * Sets the hardware value to a new number.
 *
 * Return value: Success.
 **/
static gboolean
gpm_brightness_hal_set_hw (GpmBrightnessHal *brightness, guint value_hw)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;
	gint retval;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);

	proxy = dbus_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected to HAL");
		return FALSE;
	}

	if (value_hw < 0 ||
	    value_hw > brightness->priv->levels - 1) {
		egg_warning ("set outside range (%i of %i)",
			     value_hw, brightness->priv->levels - 1);
		return FALSE;
	}

	egg_debug ("Setting %i of %i", value_hw, brightness->priv->levels - 1);

	ret = dbus_g_proxy_call (proxy, "SetBrightness", &error,
				 G_TYPE_INT, (gint)value_hw,
				 G_TYPE_INVALID,
				 G_TYPE_INT, &retval,
				 G_TYPE_INVALID);
	/* retval is ignored, the HAL API is broken... */

	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("SetBrightness failed!");
		return FALSE;
	}

	/* we changed the hardware */
	if (ret) {
		brightness->priv->hw_changed = TRUE;
	}

	brightness->priv->last_set_hw = value_hw;
	return TRUE;
}

/**
 * gpm_brightness_hal_dim_hw_step:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 *
 * Just do the step up and down, after knowing the step interval
 **/
static gboolean
gpm_brightness_hal_dim_hw_step (GpmBrightnessHal *brightness, guint new_level_hw, guint step_interval)
{
	guint last_set_hw;
	gint a;
	gboolean ret = FALSE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);

	last_set_hw = brightness->priv->last_set_hw;
	egg_debug ("new_level_hw=%i, last_set_hw=%i", new_level_hw, last_set_hw);

	/* we do the step interval as we can have insane levels of brightness */
	if (new_level_hw == last_set_hw) {
		return FALSE;
	}

	if (new_level_hw > last_set_hw) {
		/* going up */
		for (a=last_set_hw; a <= new_level_hw; a+=step_interval) {
			ret = gpm_brightness_hal_set_hw (brightness, a);
			/* we failed the last brightness set command, don't keep trying */
			if (!ret) {
				break;
			}
			g_usleep (1000 * GPM_BRIGHTNESS_DIM_INTERVAL);
		}
	} else {
		/* going down */
		for (a=last_set_hw; (gint) (a + 1) > (gint) new_level_hw; a-=step_interval) {
			ret = gpm_brightness_hal_set_hw (brightness, a);
			/* we failed the last brightness set command, don't keep trying */
			if (!ret) {
				break;
			}
			g_usleep (1000 * GPM_BRIGHTNESS_DIM_INTERVAL);
		}
	}
	return ret;
}

/**
 * gpm_brightness_hal_dim_hw:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 **/
static gboolean
gpm_brightness_hal_dim_hw (GpmBrightnessHal *brightness, guint new_level_hw)
{
	guint step;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);

	egg_debug ("new_level_hw=%i", new_level_hw);

	/* macbook pro has a bazzillion brightness levels, be a bit clever */
	step = gpm_brightness_get_step (brightness->priv->levels);
	return gpm_brightness_hal_dim_hw_step (brightness, new_level_hw, step);
}

/**
 * gpm_brightness_hal_set:
 * @brightness: This brightness class instance
 * @percentage: The percentage brightness
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 **/
gboolean
gpm_brightness_hal_set (GpmBrightnessHal *brightness, guint percentage, gboolean *hw_changed)
{
	guint level_hw;
	gboolean ret;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);
	g_return_val_if_fail (hw_changed != NULL, FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

	level_hw = gpm_percent_to_discrete (percentage, brightness->priv->levels);
	brightness->priv->level_std_hw = level_hw;

	/* update */
	ret = gpm_brightness_hal_dim_hw (brightness, level_hw);

	/* did the hardware have to be modified? */
	*hw_changed = brightness->priv->hw_changed;
	return ret;
}

/**
 * gpm_brightness_hal_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_brightness_hal_get (GpmBrightnessHal *brightness, guint *percentage)
{
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);

	*percentage = gpm_discrete_to_percent (brightness->priv->last_set_hw,
					       brightness->priv->levels);
	return TRUE;
}

/**
 * gpm_brightness_hal_up:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
gboolean
gpm_brightness_hal_up (GpmBrightnessHal *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gint step;
	guint current_hw;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);
	g_return_val_if_fail (hw_changed != NULL, FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

	/* check to see if the panel has changed */
	gpm_brightness_hal_get_hw (brightness, &current_hw);

	/* the panel has been updated in firmware */
	if (current_hw != brightness->priv->last_set_hw || 
            brightness->priv->does_own_updates) {
		brightness->priv->last_set_hw = current_hw;
		ret = TRUE;
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_get_step (brightness->priv->levels);
		/* don't overflow */
		if (brightness->priv->last_set_hw + step > brightness->priv->levels - 1) {
			step = (brightness->priv->levels - 1) - brightness->priv->last_set_hw;
		}
		ret = gpm_brightness_hal_set_hw (brightness, brightness->priv->last_set_hw + step);
	}

	/* did the hardware have to be modified? */
	*hw_changed = brightness->priv->hw_changed;
	return ret;
}

/**
 * gpm_brightness_hal_down:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
gboolean
gpm_brightness_hal_down (GpmBrightnessHal *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gint step;
	guint current_hw;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_HAL (brightness), FALSE);
	g_return_val_if_fail (hw_changed != NULL, FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

	/* check to see if the panel has changed */
	gpm_brightness_hal_get_hw (brightness, &current_hw);

	/* the panel has been updated in firmware */
 	if (current_hw != brightness->priv->last_set_hw ||
              brightness->priv->does_own_updates) {
		brightness->priv->last_set_hw = current_hw;
		ret = TRUE;
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_get_step (brightness->priv->levels);
		/* don't underflow */
		if (brightness->priv->last_set_hw < step) {
			step = brightness->priv->last_set_hw;
		}
		ret = gpm_brightness_hal_set_hw (brightness, brightness->priv->last_set_hw - step);
	}


	/* did the hardware have to be modified? */
	*hw_changed = brightness->priv->hw_changed;
	return ret;
}

/**
 * gpm_brightness_hal_has_hw:
 **/
gboolean
gpm_brightness_hal_has_hw (GpmBrightnessHal *brightness)
{
	return (brightness->priv->gproxy != NULL);
}

/**
 * gpm_brightness_hal_finalize:
 **/
static void
gpm_brightness_hal_finalize (GObject *object)
{
	GpmBrightnessHal *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS_HAL (object));
	brightness = GPM_BRIGHTNESS_HAL (object);

	if (brightness->priv->udi != NULL) {
		g_free (brightness->priv->udi);
	}
	if (brightness->priv->gproxy != NULL) {
		g_object_unref (brightness->priv->gproxy);
	}

	G_OBJECT_CLASS (gpm_brightness_hal_parent_class)->finalize (object);
}

/**
 * gpm_brightness_hal_class_init:
 **/
static void
gpm_brightness_hal_class_init (GpmBrightnessHalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_brightness_hal_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBrightnessHalClass, brightness_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmBrightnessHalPrivate));
}

/**
 * gpm_brightness_hal_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect laptop_panel objects
 * to *NOT* be removed or added during the session.
 * We only control the first laptop_panel object if there are more than one.
 **/
static void
gpm_brightness_hal_init (GpmBrightnessHal *brightness)
{
	gchar **names;
	HalGManager *manager;
	HalGDevice *device;
	gboolean res;

	brightness->priv = GPM_BRIGHTNESS_HAL_GET_PRIVATE (brightness);
	brightness->priv->gproxy = NULL;
	brightness->priv->hw_changed = FALSE;

	/* save udi of lcd adapter */
	manager = hal_gmanager_new ();
	hal_gmanager_find_capability (manager, "laptop_panel", &names, NULL);
	g_object_unref (manager);
	if (names == NULL || names[0] == NULL) {
		egg_warning ("No devices of capability laptop_panel");
		return;
	}

	/* We only want first laptop_panel object (should only be one) */
	brightness->priv->udi = g_strdup (names[0]);
	hal_gmanager_free_capability (names);

	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, brightness->priv->udi);

	/* get levels that the adapter supports -- this does not change ever */
	hal_gdevice_get_uint (device, "laptop_panel.num_levels",
			      &brightness->priv->levels, NULL);
	egg_debug ("Laptop panel levels: %i", brightness->priv->levels);
	if (brightness->priv->levels == 0 || brightness->priv->levels > 256) {
		egg_warning ("Laptop panel levels are invalid!");
	}

	/* Check if hardware handles brightness changes automatically */
	res = hal_gdevice_get_bool (device, 
				    "laptop_panel.brightness_in_hardware",
			            &brightness->priv->does_own_updates, NULL);

	if (!res) {
		brightness->priv->does_own_updates = FALSE;
		egg_debug ("laptop_panel.brightness_in_hardware not found. "
			   "Assuming false");
	} else {
		if (brightness->priv->does_own_updates) {
			egg_debug ("laptop_panel.brightness_in_hardware: True");
		} else {
			egg_debug ("laptop_panel.brightness_in_hardware: False");
		}
	}

	g_object_unref (device);

	/* get a managed proxy */
	brightness->priv->gproxy = dbus_proxy_new ();
	dbus_proxy_assign (brightness->priv->gproxy, DBUS_PROXY_SYSTEM, HAL_DBUS_SERVICE,
			   brightness->priv->udi, HAL_DBUS_INTERFACE_LAPTOP_PANEL);

	/* this changes under our feet */
	gpm_brightness_hal_get_hw (brightness, &brightness->priv->last_set_hw);

	/* set to known value */
	brightness->priv->level_std_hw = 0;

	egg_debug ("Starting: (%i of %i)", brightness->priv->last_set_hw,
		   brightness->priv->levels - 1);
}

/**
 * gpm_brightness_hal_new:
 * Return value: A new brightness class instance.
 * Can return NULL if no suitable hardware is found.
 **/
GpmBrightnessHal *
gpm_brightness_hal_new (void)
{
	GpmBrightnessHal *brightness;
	brightness = g_object_new (GPM_TYPE_BRIGHTNESS_HAL, NULL);
	return GPM_BRIGHTNESS_HAL (brightness);
}

