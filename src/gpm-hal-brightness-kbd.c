/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2006 David Zeuthen <davidz@redhat.com>
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
#include "gpm-hal.h"
#include "gpm-hal-brightness-kbd.h"
#include "gpm-proxy.h"
#include "gpm-marshal.h"
#include "gpm-feedback-widget.h"

#define DIM_INTERVAL		10 /* ms */

#define GPM_HAL_BRIGHTNESS_KBD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL_BRIGHTNESS_KBD, GpmHalBrightnessKbdPrivate))

struct GpmHalBrightnessKbdPrivate
{
	gboolean		 does_own_updates;	/* keys are hardwired */
	gboolean		 does_own_dimming;	/* hardware auto-fades */
	gboolean		 is_dimmed;
	guint			 current_hw;		/* hardware */
	guint			 level_dim_hw;
	guint			 level_std_hw;
	guint			 levels;
	gchar			*udi;
	GpmProxy		*gproxy;
	GpmHal			*hal;
	GpmFeedback		*feedback;
};

G_DEFINE_TYPE (GpmHalBrightnessKbd, gpm_hal_brightness_kbd, G_TYPE_OBJECT)

/**
 * gpm_hal_brightness_kbd_get_hw:
 * @brightness: This brightness class instance
 *
 * Updates the private local value of brightness_level_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_hal_brightness_kbd_get_hw (GpmHalBrightnessKbd *brightness,
			       guint	    *brightness_level_hw)
{
	GError     *error = NULL;
	gboolean    ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

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
 * gpm_hal_brightness_kbd_set_hw:
 * @brightness_kbd: This brightness_kbd class instance
 * @brightness_level_hw: The hardware level in raw units
 *
 * Sets the hardware value to a new number.
 *
 * Return value: Success.
 **/
static gboolean
gpm_hal_brightness_kbd_set_hw (GpmHalBrightnessKbd *brightness,
			   guint	     brightness_level_hw)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

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
				 G_TYPE_INT, brightness_level_hw,
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
 * gpm_hal_brightness_kbd_dim_hw:
 * @brightness_kbd: This brightness_kbd class instance
 * @new_level_hw: The new hardware level
 *
 * Just do the step up and down, after knowing the step interval
 **/
static gboolean
gpm_hal_brightness_kbd_dim_hw_step (GpmHalBrightnessKbd *brightness,
				guint             new_level_hw,
				guint		  step_interval)
{
	guint current_hw;
	gint a;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	current_hw = brightness->priv->current_hw;
	gpm_debug ("new_level_hw=%i, current_hw=%i", new_level_hw, current_hw);

	/* we do the step interval as we can have insane levels of brightness_kbd */
	if (new_level_hw == current_hw) {
		return FALSE;
	}

	if (new_level_hw > current_hw) {
		/* going up */
		for (a=current_hw; a <= new_level_hw; a+=step_interval) {
			gpm_hal_brightness_kbd_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	} else {
		/* going down */
		for (a=current_hw; (gint) (a + 1) > (gint) new_level_hw; a-=step_interval) {
			gpm_hal_brightness_kbd_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	}
	return TRUE;
}

/**
 * gpm_hal_brightness_kbd_get_step:
 * @brightness_kbd: This brightness_kbd class instance
 * Return value: the amount of hardware steps to do on each update or
 * zero for error.
 **/
static guint
gpm_hal_brightness_kbd_get_step (GpmHalBrightnessKbd *brightness)
{
	int step;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), 0);

	if (brightness->priv->levels < 20) {
		/* less than 20 states should do every state */
		step = 1;
	} else {
		/* macbook pro has a bazzillion brightness_kbd levels, do in 5% steps */
		step = brightness->priv->levels / 20;
	}
	return step;
}

/**
 * gpm_hal_brightness_kbd_dim_hw:
 * @brightness_kbd: This brightness_kbd class instance
 * @new_level_hw: The new hardware level
 **/
static gboolean
gpm_hal_brightness_kbd_dim_hw (GpmHalBrightnessKbd *brightness,
			   guint	     new_level_hw)
{
	guint step;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	gpm_debug ("new_level_hw=%i", new_level_hw);

	/* some machines don't take kindly to auto-dimming */
	if (brightness->priv->does_own_dimming) {
		gpm_hal_brightness_kbd_set_hw (brightness, new_level_hw);
		return FALSE;
	}

	/* macbook pro has a bazzillion brightness_kbd levels, be a bit clever */
	step = gpm_hal_brightness_kbd_get_step (brightness);
	gpm_hal_brightness_kbd_dim_hw_step (brightness, new_level_hw, step);

	return TRUE;
}

/**
 * gpm_hal_brightness_kbd_set_dim:
 * @brightness_kbd: This brightness_kbd class instance
 * @brightness_level: The percentage brightness_kbd
 **/
gboolean
gpm_hal_brightness_kbd_set_dim (GpmHalBrightnessKbd *brightness,
				  guint		    brightness_level)
{
	guint level_hw;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	level_hw = gpm_percent_to_discrete (brightness_level, brightness->priv->levels);

	/* If the current brightness_kbd is less than the dim brightness_kbd then just
	 * use the current brightness_kbd so that we don't *increase* in brightness_kbd
	 * on idle. See #338630 for more details */
	if (brightness->priv->level_std_hw > level_hw) {
		brightness->priv->level_dim_hw = level_hw;
	} else {
		gpm_warning ("Current brightness_kbd is %i, dim brightness_kbd is %i.",
			     brightness->priv->level_std_hw, level_hw);
		brightness->priv->level_dim_hw = brightness->priv->level_std_hw;
	}
	/* if in this state, then update */
	if (brightness->priv->is_dimmed == TRUE) {
		gpm_hal_brightness_kbd_dim_hw (brightness, brightness->priv->level_dim_hw);
	}
	return TRUE;
}

/**
 * gpm_hal_brightness_kbd_set_dim:
 * @brightness_kbd: This brightness_kbd class instance
 * @brightness_level: The percentage brightness_kbd
 **/
gboolean
gpm_hal_brightness_kbd_set_std (GpmHalBrightnessKbd *brightness,
				  guint		    brightness_level)
{
	guint level_hw;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	level_hw = gpm_percent_to_discrete (brightness_level,
						 brightness->priv->levels);
	brightness->priv->level_std_hw = level_hw;

	/* if in this state, then update */
	if (brightness->priv->is_dimmed == FALSE) {
		gpm_hal_brightness_kbd_dim_hw (brightness, brightness->priv->level_std_hw);
	}
	return TRUE;
}

/**
 * gpm_hal_brightness_kbd_dim:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * Sets the screen into dim mode, where the dim brightness_kbd is used.
 **/
gboolean
gpm_hal_brightness_kbd_dim (GpmHalBrightnessKbd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	/* check to see if we are already dimmed */
	if (brightness->priv->is_dimmed == TRUE) {
		gpm_warning ("already dim'ed");
		return FALSE;
	}
	brightness->priv->is_dimmed = TRUE;
	return gpm_hal_brightness_kbd_dim_hw (brightness, brightness->priv->level_dim_hw);
}

/**
 * gpm_hal_brightness_kbd_undim:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * Sets the screen into normal mode, where the startdard brightness_kbd is used.
 **/
gboolean
gpm_hal_brightness_kbd_undim (GpmHalBrightnessKbd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	/* check to see if we are already dimmed */
	if (brightness->priv->is_dimmed == FALSE) {
		gpm_warning ("already undim'ed");
		return FALSE;
	}
	brightness->priv->is_dimmed = FALSE;
	return gpm_hal_brightness_kbd_dim_hw (brightness, brightness->priv->level_std_hw);
}

/**
 * gpm_hal_brightness_kbd_get:
 * @brightness_kbd: This brightness_kbd class instance
 * Return value: The percentage brightness_kbd, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness_kbd. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_hal_brightness_kbd_get (GpmHalBrightnessKbd *brightness,
			guint		 *brightness_level)
{
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
						       brightness->priv->levels);
	*brightness_level = percentage;
	return TRUE;
}

/**
 * gpm_hal_brightness_kbd_up:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * If possible, put the brightness_kbd of the KBD up one unit.
 **/
gboolean
gpm_hal_brightness_kbd_up (GpmHalBrightnessKbd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_hal_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion brightness_kbd levels, be a bit clever */
		step = gpm_hal_brightness_kbd_get_step (brightness);
		/* don't overflow */
		if (brightness->priv->current_hw + step > brightness->priv->levels - 1) {
			step = (brightness->priv->levels - 1) - brightness->priv->current_hw;
		}
		gpm_hal_brightness_kbd_set_hw (brightness, brightness->priv->current_hw + step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);

	gpm_debug ("Need to diplay backlight feedback value %i", percentage);
	gpm_feedback_display_value (brightness->priv->feedback, (float) percentage / 100.0f);
	return TRUE;
}

/**
 * gpm_hal_brightness_kbd_down:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * If possible, put the brightness_kbd of the KBD down one unit.
 **/
gboolean
gpm_hal_brightness_kbd_down (GpmHalBrightnessKbd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_hal_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion brightness_kbd levels, be a bit clever */
		step = gpm_hal_brightness_kbd_get_step (brightness);
		/* don't underflow */
		if (brightness->priv->current_hw < step) {
			step = brightness->priv->current_hw;
		}
		gpm_hal_brightness_kbd_set_hw (brightness, brightness->priv->current_hw - step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);

	gpm_debug ("Need to diplay backlight feedback value %i", percentage);
	gpm_feedback_display_value (brightness->priv->feedback, (float) percentage / 100.0f);
	return TRUE;
}

/**keyboard_backlight
 * gpm_hal_brightness_kbd_constructor:
 **/
static GObject *
gpm_hal_brightness_kbd_constructor (GType		  type,
			    guint		  n_construct_properties,
			    GObjectConstructParam *construct_properties)
{
	GpmHalBrightnessKbd      *brightness;
	GpmHalBrightnessKbdClass *klass;
	klass = GPM_HAL_BRIGHTNESS_KBD_CLASS (g_type_class_peek (GPM_TYPE_HAL_BRIGHTNESS_KBD));
	brightness = GPM_HAL_BRIGHTNESS_KBD (G_OBJECT_CLASS (gpm_hal_brightness_kbd_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

/**
 * gpm_hal_brightness_kbd_finalize:
 **/
static void
gpm_hal_brightness_kbd_finalize (GObject *object)
{
	GpmHalBrightnessKbd *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL_BRIGHTNESS_KBD (object));
	brightness = GPM_HAL_BRIGHTNESS_KBD (object);

	g_free (brightness->priv->udi);
	g_object_unref (brightness->priv->gproxy);
	g_object_unref (brightness->priv->hal);
	g_object_unref (brightness->priv->feedback);

	g_return_if_fail (brightness->priv != NULL);
	G_OBJECT_CLASS (gpm_hal_brightness_kbd_parent_class)->finalize (object);
}

/**
 * gpm_hal_brightness_kbd_class_init:
 **/
static void
gpm_hal_brightness_kbd_class_init (GpmHalBrightnessKbdClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_hal_brightness_kbd_finalize;
	object_class->constructor  = gpm_hal_brightness_kbd_constructor;

	g_type_class_add_private (klass, sizeof (GpmHalBrightnessKbdPrivate));
}

/**
 * gpm_hal_brightness_kbd_init:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * initialises the brightness_kbd class. NOTE: We expect keyboard_backlight objects
 * to *NOT* be removed or added during the session.
 * We only control the first keyboard_backlight object if there are more than one.
 **/
static void
gpm_hal_brightness_kbd_init (GpmHalBrightnessKbd *brightness)
{
	gchar **names;

	brightness->priv = GPM_HAL_BRIGHTNESS_KBD_GET_PRIVATE (brightness);

	brightness->priv->hal = gpm_hal_new ();

	brightness->priv->feedback = gpm_feedback_new ();
	gpm_feedback_set_icon_name (brightness->priv->feedback,
				    GPM_STOCK_BRIGHTNESS_KBD);

	/* save udi of kbd adapter */
	gpm_hal_device_find_capability (brightness->priv->hal, "keyboard_backlight", &names);
	if (names == NULL || names[0] == NULL) {
		gpm_warning ("No devices of capability keyboard_backlight");
		return;
	}

	/* We only want first keyboard_backlight object (should only be one) */
	brightness->priv->udi = g_strdup (names[0]);
	gpm_hal_free_capability (brightness->priv->hal, names);

	brightness->priv->does_own_dimming = FALSE;
	brightness->priv->does_own_updates = FALSE;

	/* get a managed proxy */
	brightness->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (brightness->priv->gproxy,
			  GPM_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  brightness->priv->udi,
			  HAL_DBUS_INTERFACE_KBD_BACKLIGHT);

	/* get levels that the adapter supports -- this does not change ever */
	gpm_hal_device_get_uint (brightness->priv->hal, brightness->priv->udi, "keyboard_backlight.num_levels",
				 &brightness->priv->levels);

	/* this changes under our feet */
	gpm_hal_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);

	/* set to known value */
	brightness->priv->level_dim_hw = 1;
	brightness->priv->level_std_hw = 1;

	gpm_debug ("Starting: (%i of %i)",
		   brightness->priv->current_hw,
		   brightness->priv->levels - 1);
}

/**
 * gpm_hal_brightness_kbd_has_hw:
 *
 * Self contained function that works out if we have the hardware.
 * If not, we return FALSE and the module is unloaded.
 **/
static gboolean
gpm_hal_brightness_kbd_has_hw (void)
{
	GpmHal *hal;
	gchar **names;
	gboolean ret = TRUE;

	/* okay, as singleton - so we don't allocate more memory */
	hal = gpm_hal_new ();
	gpm_hal_device_find_capability (hal, "keyboard_backlight", &names);

	/* nothing found */
	if (names == NULL || names[0] == NULL) {
		ret = FALSE;
	}

	gpm_hal_free_capability (hal, names);
	g_object_unref (hal);
	return ret;
}

/**
 * gpm_hal_brightness_kbd_new:
 * Return value: A new brightness_kbd class instance.
 **/
GpmHalBrightnessKbd *
gpm_hal_brightness_kbd_new (void)
{
	GpmHalBrightnessKbd *brightness;

	/* only load an instance of this module if we have the hardware */
	if (gpm_hal_brightness_kbd_has_hw () == FALSE) {
		return NULL;
	}

	brightness = g_object_new (GPM_TYPE_HAL_BRIGHTNESS_KBD, NULL);
	return GPM_HAL_BRIGHTNESS_KBD (brightness);
}
