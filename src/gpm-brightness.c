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
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-brightness.h"
#include "gpm-hal.h"
#include "gpm-marshal.h"

#define DIM_INTERVAL		10 /* ms */

static void	gpm_brightness_class_init (GpmBrightnessClass *klass);
static void	gpm_brightness_init	  (GpmBrightness      *brightness);
static void	gpm_brightness_finalize	  (GObject	      *object);
static gboolean	gpm_brightness_update_hw  (GpmBrightness      *brightness);
static int	gpm_brightness_hw_to_percent (int hw, int levels);

#define GPM_BRIGHTNESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS, GpmBrightnessPrivate))

struct GpmBrightnessPrivate
{
	gboolean		 has_hardware;
	gboolean		 does_own_updates;	/* keys are hardwired */
	gboolean		 does_own_dimming;	/* hardware auto-fades */
	int			 current_hw;		/* hardware */
	int			 level_dim_hw;
	int			 level_std_hw;
	int			 levels;
	char			*udi;
	DBusGProxy		*proxy;
	GpmHal			*hal;
};

enum {
	BRIGHTNESS_STEP_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmBrightness, gpm_brightness, G_TYPE_OBJECT)

/**
 * gpm_brightness_constructor:
 **/
static GObject *
gpm_brightness_constructor (GType		  type,
			    guint		  n_construct_properties,
			    GObjectConstructParam *construct_properties)
{
	GpmBrightness      *brightness;
	GpmBrightnessClass *klass;
	klass = GPM_BRIGHTNESS_CLASS (g_type_class_peek (GPM_TYPE_BRIGHTNESS));
	brightness = GPM_BRIGHTNESS (G_OBJECT_CLASS (gpm_brightness_parent_class)->constructor
			      	     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

/**
 * gpm_brightness_class_init:
 **/
static void
gpm_brightness_class_init (GpmBrightnessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_brightness_finalize;
	object_class->constructor  = gpm_brightness_constructor;

	g_type_class_add_private (klass, sizeof (GpmBrightnessPrivate));

	signals [BRIGHTNESS_STEP_CHANGED] =
		g_signal_new ("brightness-step-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBrightnessClass, lcd_step_changed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
}

/**
 * gpm_brightness_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect laptop_panel objects
 * to *NOT* be removed or added during the session.
 * We only control the first laptop_panel object if there are more than one.
 **/
static void
gpm_brightness_init (GpmBrightness *brightness)
{
	DBusGConnection *system_connection = NULL;
	gchar  **names;
	char    *manufacturer_string = NULL;
	gboolean res;
	GError  *error = NULL;

	brightness->priv = GPM_BRIGHTNESS_GET_PRIVATE (brightness);

	brightness->priv->hal = gpm_hal_new ();

	/* save udi of lcd adapter */
	gpm_hal_device_find_capability (brightness->priv->hal, "laptop_panel", &names);
	if (names == NULL || names[0] == NULL) {
		brightness->priv->has_hardware = FALSE;
		gpm_debug ("No devices of capability laptop_panel");
		return;
	}

	/* We only want first laptop_panel object (should only be one) */
	brightness->priv->udi = g_strdup (names[0]);
	gpm_hal_free_capability (brightness->priv->hal, names);

	brightness->priv->has_hardware = TRUE;
	brightness->priv->does_own_dimming = FALSE;

	/* If the manufacturer is IBM, then assume we are a ThinkPad,
	 * and don't do the new-fangled dimming routine. The ThinkPad dims
	 * gently itself and the two dimming routines just get messy.
	 * https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=173382 */
	gpm_hal_device_get_string (brightness->priv->hal, HAL_ROOT_COMPUTER,
				   "smbios.system.manufacturer",
				   &manufacturer_string);
	if (manufacturer_string) {
		/* FIXME: This should be a HAL property */
		if (strcmp (manufacturer_string, "IBM") == 0) {
			brightness->priv->does_own_dimming = TRUE;
		}
		g_free (manufacturer_string);
	}

	/* We only want to change the brightness if the machine does not
	   do it on it's own updates, as this can make the panel flash in a
	   feedback loop. */
	res = gpm_hal_device_get_bool (brightness->priv->hal, brightness->priv->udi,
				       "laptop_panel.brightness_in_hardware",
				       &brightness->priv->does_own_updates);
	/* This key does not exist on normal machines */
	if (!res) {
		brightness->priv->does_own_updates = FALSE;
	}

	/* get proxy once and store */
	system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
		return;
	}
	brightness->priv->proxy = dbus_g_proxy_new_for_name (system_connection,
							     HAL_DBUS_SERVICE,
							     brightness->priv->udi,
							     HAL_DBUS_INTERFACE_LAPTOP_PANEL);

	/* get levels that the adapter supports -- this does not change ever */
	gpm_hal_device_get_int (brightness->priv->hal, brightness->priv->udi, "laptop_panel.num_levels",
				&brightness->priv->levels);

	/* this changes under our feet */
	gpm_brightness_update_hw (brightness);

	/* set to known value */
	brightness->priv->level_dim_hw = 1;
	brightness->priv->level_std_hw = 1;

	gpm_debug ("Starting: (%i of %i)", brightness->priv->current_hw,
		   brightness->priv->levels - 1);
}

/**
 * gpm_brightness_finalize:
 **/
static void
gpm_brightness_finalize (GObject *object)
{
	GpmBrightness *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS (object));
	brightness = GPM_BRIGHTNESS (object);

	g_free (brightness->priv->udi);
	g_object_unref (G_OBJECT (brightness->priv->proxy));
	g_object_unref (brightness->priv->hal);

	g_return_if_fail (brightness->priv != NULL);
	G_OBJECT_CLASS (gpm_brightness_parent_class)->finalize (object);
}

/**
 * gpm_brightness_new:
 * Return value: A new brightness class instance.
 **/
GpmBrightness *
gpm_brightness_new (void)
{
	GpmBrightness *brightness;
	brightness = g_object_new (GPM_TYPE_BRIGHTNESS, NULL);
	return GPM_BRIGHTNESS (brightness);
}

/**
 * gpm_brightness_update_hw:
 * @brightness: This brightness class instance
 *
 * Updates the private local value of brightness_level_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_brightness_update_hw (GpmBrightness *brightness)
{
	GError  *error = NULL;
	gboolean retval;
	gint     brightness_hw = 0;

	retval = TRUE;
	if (!dbus_g_proxy_call (brightness->priv->proxy, "GetBrightness", &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, &brightness_hw, G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		retval = FALSE;
	}

	brightness->priv->current_hw = brightness_hw;
	return retval;
}

/**
 * gpm_brightness_set_hw:
 * @brightness: This brightness class instance
 * @brightness_level_hw: The hardware level in raw units
 *
 * Sets the hardware value to a new number.
 *
 * Return value: Success.
 **/
static gboolean
gpm_brightness_set_hw (GpmBrightness *brightness,
		       int	      brightness_level_hw)
{
	GError  *error = NULL;
	gint     ret;
	gboolean retval;

	if (brightness_level_hw < 0 ||
	    brightness_level_hw > brightness->priv->levels - 1) {
		gpm_warning ("set outside range (%i of %i)",
			     brightness_level_hw, brightness->priv->levels - 1);
		return FALSE;
	}

	gpm_debug ("Setting %i of %i", brightness_level_hw, brightness->priv->levels - 1);

	retval = TRUE;
	if (!dbus_g_proxy_call (brightness->priv->proxy, "SetBrightness", &error,
				G_TYPE_INT, brightness_level_hw, G_TYPE_INVALID,
				G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
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


/**
 * gpm_brightness_percent_to_hw:
 * @percentage: The percentage to convert
 * @levels: The number of hardware levels for our hardware
 *
 * We have to be carefull when converting from %->hw as precision is very
 * important if we want the highest value.
 *
 * Return value: The hardware value for this percentage.
 **/
static int
gpm_brightness_percent_to_hw (int percentage,
			      int levels)
{
	/* check we are in range */
	if (percentage < 0) {
		return 0;
	} else if (percentage > 100) {
		return levels;
	}
	return ( (float) percentage * (float) (levels - 1)) / 100.0f;
}

/**
 * gpm_brightness_dim_hw:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 *
 * Just do the step up and down, after knowing the step interval
 **/
static void
gpm_brightness_dim_hw_step (GpmBrightness *brightness,
		            int            new_level_hw,
		            int		   step_interval)
{
	int current_hw;
	int a;
	current_hw = brightness->priv->current_hw;

	/* we do the step interval as we can have insane levels of brightness */
	if (new_level_hw > current_hw) {
		/* going up */
		for (a=current_hw; a <= new_level_hw; a+=step_interval) {
			gpm_brightness_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	} else {
		/* going down */
		for (a=current_hw; a >= new_level_hw; a-=step_interval) {
			gpm_brightness_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	}
}

/**
 * gpm_brightness_get_step:
 * @brightness: This brightness class instance
 * Return value: the amount of hardware steps to do on each update
 **/
static int
gpm_brightness_get_step (GpmBrightness *brightness)
{
	int step;
	if (brightness->priv->levels < 20) {
		/* less than 20 states should do every state */
		step = 1;
	} else {
		/* macbook pro has a bazzillion brightness levels, do in 5% steps */
		step = brightness->priv->levels / 20;
	}
	return step;
}

/**
 * gpm_brightness_dim_hw:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 **/
static void
gpm_brightness_dim_hw (GpmBrightness *brightness,
		       int	      new_level_hw)
{
	int step;

	if (! brightness->priv->has_hardware) {
		return;
	}

	/* some machines don't take kindly to auto-dimming */
	if (brightness->priv->does_own_dimming) {
		gpm_brightness_set_hw (brightness, new_level_hw);
		return;
	}

	/* macbook pro has a bazzillion brightness levels, be a bit clever */
	step = gpm_brightness_get_step (brightness);
	gpm_brightness_dim_hw_step (brightness, new_level_hw, step);
}

/**
 * gpm_brightness_hw_to_percent:
 * @hw: The hardware level
 * @levels: The number of hardware levels for our hardware
 *
 * We have to be carefull when converting from hw->%.
 *
 * Return value: The percentage for this hardware value.
 **/
static int
gpm_brightness_hw_to_percent (int hw,
			      int levels)
{
	/* check we are in range */
	if (hw < 0) {
		return 0;
	} else if (hw > levels) {
		return 100;
	}
	return (int) ((float) hw * (100.0f / (float) (levels - 1)));
}

/**
 * gpm_brightness_set_level_dim:
 * @brightness: This brightness class instance
 * @brightness_level: The percentage brightness
 **/
void
gpm_brightness_set_level_dim (GpmBrightness *brightness,
			      int	     brightness_level)
{
	int level_hw;
	level_hw = gpm_brightness_percent_to_hw (brightness_level, brightness->priv->levels);

	/* If the current brightness is less than the dim brightness then just
	 * use the current brightness so that we don't *increase* in brightness
	 * on idle. See #338630 for more details */
	if (brightness->priv->level_std_hw > level_hw) {
		brightness->priv->level_dim_hw = level_hw;
	} else {
		gpm_warning ("Current brightness is %i, dim brightness is %i.",
			     brightness->priv->level_std_hw, level_hw);
		brightness->priv->level_dim_hw = brightness->priv->level_std_hw;
	}
}

/**
 * gpm_brightness_set_level_dim:
 * @brightness: This brightness class instance
 * @brightness_level: The percentage brightness
 **/
void
gpm_brightness_set_level_std (GpmBrightness *brightness,
			      int	     brightness_level)
{
	int level_hw;
	level_hw = gpm_brightness_percent_to_hw (brightness_level,
						 brightness->priv->levels);
	brightness->priv->level_std_hw = level_hw;
}

/**
 * gpm_brightness_dim:
 * @brightness: This brightness class instance
 *
 * Sets the screen into dim mode, where the dim brightness is used.
 **/
void
gpm_brightness_dim (GpmBrightness *brightness)
{
	gpm_brightness_dim_hw (brightness, brightness->priv->level_dim_hw);
}

/**
 * gpm_brightness_undim:
 * @brightness: This brightness class instance
 *
 * Sets the screen into normal mode, where the startdard brightness is used.
 **/
void
gpm_brightness_undim (GpmBrightness *brightness)
{
	gpm_brightness_dim_hw (brightness, brightness->priv->level_std_hw);
}

/**
 * gpm_brightness_set:
 * @brightness: This brightness class instance
 **/
void
gpm_brightness_set (GpmBrightness *brightness)
{
	gpm_brightness_dim_hw (brightness, brightness->priv->level_std_hw);
}

/**
 * gpm_brightness_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
int
gpm_brightness_get (GpmBrightness *brightness)
{
	int percentage;
	if (! brightness->priv->has_hardware) {
		return -1;
	}
	percentage = gpm_brightness_hw_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);
	return percentage;
}

/**
 * gpm_brightness_up:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
void
gpm_brightness_up (GpmBrightness *brightness)
{
	int step;

	if (! brightness->priv->has_hardware) {
		return;
	}

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_brightness_update_hw (brightness);
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_get_step (brightness);
		gpm_brightness_set_hw (brightness, brightness->priv->current_hw + step);
	}

	int percentage;
	percentage = gpm_brightness_hw_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);
	gpm_debug ("emitting brightness-step-changed : %i", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_STEP_CHANGED], 0, percentage);
}

/**
 * gpm_brightness_down:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
void
gpm_brightness_down (GpmBrightness *brightness)
{
	int step;

	if (! brightness->priv->has_hardware) {
		return;
	}

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_brightness_update_hw (brightness);
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_get_step (brightness);
		gpm_brightness_set_hw (brightness, brightness->priv->current_hw - 1);
	}

	int percentage;
	percentage = gpm_brightness_hw_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);
	gpm_debug ("emitting brightness-step-changed : %i", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_STEP_CHANGED], 0, percentage);
}
