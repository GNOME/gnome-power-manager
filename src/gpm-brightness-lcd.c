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

#include "gpm-brightness-lcd.h"
#include "gpm-conf.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-hal.h"
#include "gpm-marshal.h"
#include "gpm-proxy.h"

#define DIM_INTERVAL		10 /* ms */

#define GPM_BRIGHTNESS_LCD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS_LCD, GpmBrightnessLcdPrivate))

struct GpmBrightnessLcdPrivate
{
	gboolean		 does_own_updates;	/* keys are hardwired */
	gboolean		 does_own_dimming;	/* hardware auto-fades */
	gboolean		 is_dimmed;
	guint			 current_hw;		/* hardware */
	guint			 level_dim_hw;
	guint			 level_std_hw;
	guint			 levels;
	gchar			*udi;
	GpmConf			*conf;
	GpmProxy		*gproxy;
	GpmHal			*hal;
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (GpmBrightnessLcd, gpm_brightness_lcd, G_TYPE_OBJECT)
static guint	     signals [LAST_SIGNAL] = { 0, };

/**
 * gpm_brightness_lcd_get_hw:
 * @brightness: This brightness class instance
 *
 * Updates the private local value of brightness_level_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_brightness_lcd_get_hw (GpmBrightnessLcd *brightness,
			   guint	    *brightness_level_hw)
{
	GError     *error = NULL;
	gboolean    ret;
	DBusGProxy *proxy;
	int         level;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	proxy = gpm_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected to HAL");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 G_TYPE_INT, &level,
				 G_TYPE_INVALID);
	if (brightness_level_hw != NULL) {
		*brightness_level_hw = (guint)level;
	}

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
 * gpm_brightness_lcd_set_hw:
 * @brightness: This brightness class instance
 * @brightness_level_hw: The hardware level in raw units
 *
 * Sets the hardware value to a new number.
 *
 * Return value: Success.
 **/
static gboolean
gpm_brightness_lcd_set_hw (GpmBrightnessLcd *brightness,
			   guint	     brightness_level_hw)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	proxy = gpm_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected to HAL");
		return FALSE;
	}	

	if (brightness_level_hw < 0 ||
	    brightness_level_hw > brightness->priv->levels - 1) {
		gpm_warning ("set outside range (%i of %i)",
			     brightness_level_hw, brightness->priv->levels - 1);
		return FALSE;
	}

	gpm_debug ("Setting %i of %i", brightness_level_hw, brightness->priv->levels - 1);

	ret = dbus_g_proxy_call (proxy, "SetBrightness", &error,
				 G_TYPE_INT, (int)brightness_level_hw,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("SetBrightness failed!");
		return FALSE;
	}

	brightness->priv->current_hw = brightness_level_hw;
	return TRUE;
}

/**
 * gpm_brightness_lcd_dim_hw:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 *
 * Just do the step up and down, after knowing the step interval
 **/
static gboolean
gpm_brightness_lcd_dim_hw_step (GpmBrightnessLcd *brightness,
				guint             new_level_hw,
				guint		  step_interval)
{
	guint current_hw;
	gint a;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	current_hw = brightness->priv->current_hw;
	gpm_debug ("new_level_hw=%i, current_hw=%i", new_level_hw, current_hw);

	/* we do the step interval as we can have insane levels of brightness */
	if (new_level_hw == current_hw) {
		return FALSE;
	}

	if (new_level_hw > current_hw) {
		/* going up */
		for (a=current_hw; a <= new_level_hw; a+=step_interval) {
			gpm_brightness_lcd_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	} else {
		/* going down */
		for (a=current_hw; (gint) (a + 1) > (gint) new_level_hw; a-=step_interval) {
			gpm_brightness_lcd_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	}
	return TRUE;
}

/**
 * gpm_brightness_lcd_get_step:
 * @brightness: This brightness class instance
 * Return value: the amount of hardware steps to do on each update or
 * zero for error.
 **/
static guint
gpm_brightness_lcd_get_step (GpmBrightnessLcd *brightness)
{
	int step;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), 0);

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
 * gpm_brightness_lcd_dim_hw:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 **/
static gboolean
gpm_brightness_lcd_dim_hw (GpmBrightnessLcd *brightness,
			   guint	     new_level_hw)
{
	guint step;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	gpm_debug ("new_level_hw=%i", new_level_hw);

	/* some machines don't take kindly to auto-dimming */
	if (brightness->priv->does_own_dimming) {
		gpm_brightness_lcd_set_hw (brightness, new_level_hw);
		return FALSE;
	}

	/* macbook pro has a bazzillion brightness levels, be a bit clever */
	step = gpm_brightness_lcd_get_step (brightness);
	gpm_brightness_lcd_dim_hw_step (brightness, new_level_hw, step);

	return TRUE;
}

/**
 * gpm_brightness_lcd_set_dim:
 * @brightness: This brightness class instance
 * @brightness_level: The percentage brightness
 **/
gboolean
gpm_brightness_lcd_set_dim (GpmBrightnessLcd *brightness,
			    guint             brightness_level)
{
	guint level_hw;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	level_hw = gpm_percent_to_discrete (brightness_level,
					    brightness->priv->levels);

	/* If the current brightness is less than the dim brightness then just
	 * use the current brightness so that we don't *increase* in brightness
	 * on idle. See #338630 for more details */
	if (brightness->priv->level_std_hw > level_hw) {
		brightness->priv->level_dim_hw = level_hw;
	} else {
		gpm_debug ("Current brightness is %i, dim brightness is %i, "
			   "so we'll use the current as the dim brightness.",
			   brightness->priv->level_std_hw, level_hw);
		brightness->priv->level_dim_hw = brightness->priv->level_std_hw;
	}

	/* if in this state, then update */
	if (brightness->priv->is_dimmed == TRUE) {
		gpm_brightness_lcd_dim_hw (brightness, brightness->priv->level_dim_hw);
	}
	return TRUE;
}

/**
 * gpm_brightness_lcd_set_std:
 * @brightness: This brightness class instance
 * @brightness_level: The percentage brightness
 **/
gboolean
gpm_brightness_lcd_set_std (GpmBrightnessLcd *brightness,
			    guint	      brightness_level)
{
	guint level_hw;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	level_hw = gpm_percent_to_discrete (brightness_level,
						     brightness->priv->levels);
	brightness->priv->level_std_hw = level_hw;

	/* if in this state, then update */
	if (brightness->priv->is_dimmed == FALSE) {
		gpm_brightness_lcd_dim_hw (brightness, brightness->priv->level_std_hw);
	}
	return TRUE;
}

/**
 * gpm_brightness_lcd_dim:
 * @brightness: This brightness class instance
 *
 * Sets the screen into dim mode, where the dim brightness is used.
 **/
gboolean
gpm_brightness_lcd_dim (GpmBrightnessLcd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	/* check to see if we are already dimmed */
	if (brightness->priv->is_dimmed == TRUE) {
		gpm_warning ("already dim'ed");
		return FALSE;
	}
	brightness->priv->is_dimmed = TRUE;
	return gpm_brightness_lcd_dim_hw (brightness, brightness->priv->level_dim_hw);
}

/**
 * gpm_brightness_lcd_undim:
 * @brightness: This brightness class instance
 *
 * Sets the screen into normal mode, where the startdard brightness is used.
 **/
gboolean
gpm_brightness_lcd_undim (GpmBrightnessLcd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	/* check to see if we are already dimmed */
	if (brightness->priv->is_dimmed == FALSE) {
		gpm_warning ("already undim'ed");
		return FALSE;
	}
	brightness->priv->is_dimmed = FALSE;
	return gpm_brightness_lcd_dim_hw (brightness, brightness->priv->level_std_hw);
}

/**
 * gpm_brightness_lcd_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_brightness_lcd_get (GpmBrightnessLcd *brightness,
			guint		 *brightness_level)
{
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
					      brightness->priv->levels);
	*brightness_level = percentage;
	return TRUE;
}

/**
 * gpm_brightness_lcd_up:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
gboolean
gpm_brightness_lcd_up (GpmBrightnessLcd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_brightness_lcd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_lcd_get_step (brightness);
		/* don't overflow */
		if (brightness->priv->current_hw + step > brightness->priv->levels - 1) {
			step = (brightness->priv->levels - 1) - brightness->priv->current_hw;
		}
		gpm_brightness_lcd_set_hw (brightness, brightness->priv->current_hw + step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
					      brightness->priv->levels);
	gpm_debug ("emitting brightness-changed (%i)", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);

	return TRUE;
}

/**
 * gpm_brightness_lcd_down:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
gboolean
gpm_brightness_lcd_down (GpmBrightnessLcd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_LCD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_brightness_lcd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_lcd_get_step (brightness);
		/* don't underflow */
		if (brightness->priv->current_hw < step) {
			step = brightness->priv->current_hw;
		}
		gpm_brightness_lcd_set_hw (brightness, brightness->priv->current_hw - step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
					      brightness->priv->levels);
	gpm_debug ("emitting brightness-changed (%i)", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);

	return TRUE;
}

/**
 * gpm_brightness_lcd_constructor:
 **/
static GObject *
gpm_brightness_lcd_constructor (GType		   type,
			        guint		   n_construct_properties,
			        GObjectConstructParam *construct_properties)
{
	GpmBrightnessLcd      *brightness;
	GpmBrightnessLcdClass *klass;
	klass = GPM_BRIGHTNESS_LCD_CLASS (g_type_class_peek (GPM_TYPE_BRIGHTNESS_LCD));
	brightness = GPM_BRIGHTNESS_LCD (G_OBJECT_CLASS (gpm_brightness_lcd_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

/**
 * gpm_brightness_lcd_finalize:
 **/
static void
gpm_brightness_lcd_finalize (GObject *object)
{
	GpmBrightnessLcd *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS_LCD (object));
	brightness = GPM_BRIGHTNESS_LCD (object);

	if (brightness->priv->udi != NULL) {
		g_free (brightness->priv->udi);
	}
	if (brightness->priv->gproxy != NULL) {
		g_object_unref (brightness->priv->gproxy);
	}
	if (brightness->priv->hal != NULL) {
		g_object_unref (brightness->priv->hal);
	}
	if (brightness->priv->conf != NULL) {
		g_object_unref (brightness->priv->conf);
	}

	g_return_if_fail (brightness->priv != NULL);
	G_OBJECT_CLASS (gpm_brightness_lcd_parent_class)->finalize (object);
}

/**
 * gpm_brightness_lcd_class_init:
 **/
static void
gpm_brightness_lcd_class_init (GpmBrightnessLcdClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_brightness_lcd_finalize;
	object_class->constructor  = gpm_brightness_lcd_constructor;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBrightnessLcdClass, brightness_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmBrightnessLcdPrivate));
}

/**
 * gpm_brightness_lcd_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect laptop_panel objects
 * to *NOT* be removed or added during the session.
 * We only control the first laptop_panel object if there are more than one.
 **/
static void
gpm_brightness_lcd_init (GpmBrightnessLcd *brightness)
{
	gchar **names;
	gchar *manufacturer_string = NULL;
	gboolean res;
	guint value;

	brightness->priv = GPM_BRIGHTNESS_LCD_GET_PRIVATE (brightness);

	brightness->priv->hal = gpm_hal_new ();
	brightness->priv->conf = gpm_conf_new ();

	/* set the default dim */
	gpm_conf_get_uint (brightness->priv->conf, GPM_CONF_PANEL_DIM_BRIGHTNESS, &value);
	gpm_brightness_lcd_set_dim (brightness, value);

	/* save udi of lcd adapter */
	gpm_hal_device_find_capability (brightness->priv->hal, "laptop_panel", &names);
	if (names == NULL || names[0] == NULL) {
		gpm_warning ("No devices of capability laptop_panel");
		return;
	}

	/* We only want first laptop_panel object (should only be one) */
	brightness->priv->udi = g_strdup (names[0]);
	gpm_hal_free_capability (brightness->priv->hal, names);

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

	/* get a managed proxy */
	brightness->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (brightness->priv->gproxy,
			  GPM_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  brightness->priv->udi,
			  HAL_DBUS_INTERFACE_LAPTOP_PANEL);

	/* get levels that the adapter supports -- this does not change ever */
	gpm_hal_device_get_uint (brightness->priv->hal,
				 brightness->priv->udi,
				 "laptop_panel.num_levels",
				 &brightness->priv->levels);

	/* this changes under our feet */
	gpm_brightness_lcd_get_hw (brightness, &brightness->priv->current_hw);

	/* set to known value */
	brightness->priv->level_dim_hw = 1;
	brightness->priv->level_std_hw = 1;

	gpm_debug ("Starting: (%i of %i)",
		   brightness->priv->current_hw,
		   brightness->priv->levels - 1);
}

/**
 * gpm_brightness_lcd_has_hw:
 *
 * Self contained function that works out if we have the hardware.
 * If not, we return FALSE and the module is unloaded.
 **/
gboolean
gpm_brightness_lcd_has_hw (void)
{
	GpmHal *hal;
	gchar **names;
	gboolean ret = TRUE;

	/* okay, as singleton - so we don't allocate more memory */
	hal = gpm_hal_new ();
	gpm_hal_device_find_capability (hal, "laptop_panel", &names);

	/* nothing found */
	if (names == NULL || names[0] == NULL) {
		ret = FALSE;
	}

	gpm_hal_free_capability (hal, names);
	g_object_unref (hal);
	return ret;
}

/**
 * gpm_brightness_lcd_new:
 * Return value: A new brightness class instance.
 **/
GpmBrightnessLcd *
gpm_brightness_lcd_new (void)
{
	static GpmBrightnessLcd *brightness = NULL;
	if (brightness != NULL) {
		g_object_ref (brightness);
	} else {
		if (gpm_brightness_lcd_has_hw () == TRUE) {
			brightness = g_object_new (GPM_TYPE_BRIGHTNESS_LCD, NULL);
		}
	}
	return GPM_BRIGHTNESS_LCD (brightness);
}
