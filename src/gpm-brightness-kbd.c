/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#include <libhal-gdevice.h>
#include <libhal-gmanager.h>
#include <libdbus-proxy.h>

#include "gpm-brightness-kbd.h"
#include "gpm-conf.h"
#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-light-sensor.h"
#include "gpm-marshal.h"

#define DIM_INTERVAL		10 /* ms */

#define GPM_BRIGHTNESS_KBD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS_KBD, GpmBrightnessKbdPrivate))

struct GpmBrightnessKbdPrivate
{
	gboolean		 does_own_updates;	/* keys are hardwired */
	gboolean		 does_own_dimming;	/* hardware auto-fades */
	gboolean		 is_dimmed;
	gboolean		 is_disabled;
	guint			 current_hw;		/* hardware */
	guint			 level_dim_hw;
	guint			 level_std_hw;
	guint			 levels;
	gchar			*udi;
	GpmConf			*conf;
	GpmLightSensor		*sensor;
	DbusProxy		*gproxy;
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (GpmBrightnessKbd, gpm_brightness_kbd, G_TYPE_OBJECT)
static guint	     signals [LAST_SIGNAL] = { 0 };

/**
 * gpm_brightness_kbd_get_hw:
 * @brightness: This brightness class instance
 *
 * Updates the private local value of brightness_level_hw as it may have
 * changed on some h/w
 * Return value: Success.
 **/
static gboolean
gpm_brightness_kbd_get_hw (GpmBrightnessKbd *brightness,
			   guint	    *brightness_level_hw)
{
	GError     *error = NULL;
	gboolean    ret;
	DBusGProxy *proxy;
	gint brightness_level;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	proxy = dbus_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected to HAL");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 G_TYPE_INT, &brightness_level,
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

	*brightness_level_hw = (guint)brightness_level;
	return TRUE;
}

/**
 * gpm_brightness_kbd_set_hw:
 * @brightness_kbd: This brightness_kbd class instance
 * @brightness_level_hw: The hardware level in raw units
 *
 * Sets the hardware value to a new number.
 *
 * Return value: Success.
 **/
static gboolean
gpm_brightness_kbd_set_hw (GpmBrightnessKbd *brightness,
			   guint	     brightness_level_hw)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	proxy = dbus_proxy_get_proxy (brightness->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected to HAL");
		return FALSE;
	}

	if (brightness_level_hw < 0 ||
	    brightness_level_hw > brightness->priv->levels - 1) {
		egg_warning ("set outside range (%i of %i)",
			     brightness_level_hw, brightness->priv->levels - 1);
		return FALSE;
	}

	egg_debug ("Setting %i of %i", brightness_level_hw, brightness->priv->levels - 1);

	ret = dbus_g_proxy_call (proxy, "SetBrightness", &error,
				 G_TYPE_INT, brightness_level_hw,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("SetBrightness failed!");
		return FALSE;
	}
	brightness->priv->current_hw = brightness_level_hw;
	return TRUE;
}

/**
 * gpm_brightness_kbd_dim_hw:
 * @brightness_kbd: This brightness_kbd class instance
 * @new_level_hw: The new hardware level
 *
 * Just do the step up and down, after knowing the step interval
 **/
static gboolean
gpm_brightness_kbd_dim_hw_step (GpmBrightnessKbd *brightness,
				guint             new_level_hw,
				guint		  step_interval)
{
	guint current_hw;
	gint a;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	current_hw = brightness->priv->current_hw;
	egg_debug ("new_level_hw=%i, current_hw=%i", new_level_hw, current_hw);

	/* we do the step interval as we can have insane levels of brightness_kbd */
	if (new_level_hw == current_hw) {
		return FALSE;
	}

	if (new_level_hw > current_hw) {
		/* going up */
		for (a=current_hw; a <= new_level_hw; a+=step_interval) {
			gpm_brightness_kbd_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	} else {
		/* going down */
		for (a=current_hw; (gint) (a + 1) > (gint) new_level_hw; a-=step_interval) {
			gpm_brightness_kbd_set_hw (brightness, a);
			g_usleep (1000 * DIM_INTERVAL);
		}
	}
	return TRUE;
}

/**
 * gpm_brightness_kbd_get_step:
 * @brightness_kbd: This brightness_kbd class instance
 * Return value: the amount of hardware steps to do on each update or
 * zero for error.
 **/
static guint
gpm_brightness_kbd_get_step (GpmBrightnessKbd *brightness)
{
	int step;

	g_return_val_if_fail (brightness != NULL, 0);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), 0);

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
 * gpm_brightness_kbd_dim_hw:
 * @brightness_kbd: This brightness_kbd class instance
 * @new_level_hw: The new hardware level
 **/
static gboolean
gpm_brightness_kbd_dim_hw (GpmBrightnessKbd *brightness,
			   guint	     new_level_hw)
{
	guint step;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	egg_debug ("new_level_hw=%i", new_level_hw);

	/* some machines don't take kindly to auto-dimming */
	if (brightness->priv->does_own_dimming) {
		gpm_brightness_kbd_set_hw (brightness, new_level_hw);
		return FALSE;
	}

	/* macbook pro has a bazzillion brightness_kbd levels, be a bit clever */
	step = gpm_brightness_kbd_get_step (brightness);
	gpm_brightness_kbd_dim_hw_step (brightness, new_level_hw, step);

	return TRUE;
}

/**
 * gpm_brightness_kbd_set_dim:
 * @brightness_kbd: This brightness_kbd class instance
 * @brightness_level: The percentage brightness_kbd
 **/
gboolean
gpm_brightness_kbd_set_dim (GpmBrightnessKbd *brightness,
			    guint	      brightness_level)
{
	guint level_hw;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	level_hw = gpm_percent_to_discrete (brightness_level, brightness->priv->levels);

	/* If the current brightness_kbd is less than the dim brightness_kbd then just
	 * use the current brightness_kbd so that we don't *increase* in brightness_kbd
	 * on idle. See #338630 for more details */
	if (brightness->priv->level_std_hw > level_hw) {
		brightness->priv->level_dim_hw = level_hw;
	} else {
		egg_warning ("Current brightness_kbd is %i, dim brightness_kbd is %i.",
			     brightness->priv->level_std_hw, level_hw);
		brightness->priv->level_dim_hw = brightness->priv->level_std_hw;
	}
	/* if in this state, then update */
	if (brightness->priv->is_dimmed) {
		gpm_brightness_kbd_dim_hw (brightness, brightness->priv->level_dim_hw);
	}
	return TRUE;
}

/**
 * gpm_brightness_kbd_set_std:
 * @brightness_kbd: This brightness_kbd class instance
 * @brightness_level: The percentage brightness_kbd
 **/
gboolean
gpm_brightness_kbd_set_std (GpmBrightnessKbd *brightness,
			    guint	      brightness_level)
{
	guint level_hw;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	level_hw = gpm_percent_to_discrete (brightness_level,
						 brightness->priv->levels);
	brightness->priv->level_std_hw = level_hw;

	/* if in this state, then update */
	if (brightness->priv->is_dimmed == FALSE) {
		gpm_brightness_kbd_dim_hw (brightness, brightness->priv->level_std_hw);
	}
	return TRUE;
}

/**
 * gpm_brightness_kbd_dim:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * Sets the screen into dim mode, where the dim brightness_kbd is used.
 **/
gboolean
gpm_brightness_kbd_dim (GpmBrightnessKbd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	/* check to see if we are already dimmed */
	if (brightness->priv->is_dimmed) {
		egg_warning ("already dim'ed");
		return FALSE;
	}
	brightness->priv->is_dimmed = TRUE;
//need to save old value
	return gpm_brightness_kbd_dim_hw (brightness, brightness->priv->level_dim_hw);
}

/**
 * gpm_brightness_kbd_undim:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * Sets the screen into normal mode, where the startdard brightness_kbd is used.
 **/
gboolean
gpm_brightness_kbd_undim (GpmBrightnessKbd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	/* check to see if we are already dimmed */
	if (brightness->priv->is_dimmed == FALSE) {
		egg_warning ("already undim'ed");
		return FALSE;
	}
	brightness->priv->is_dimmed = FALSE;
//need to restore old value
	return gpm_brightness_kbd_dim_hw (brightness, brightness->priv->level_std_hw);
}

/**
 * gpm_brightness_kbd_get:
 * @brightness_kbd: This brightness_kbd class instance
 * Return value: The percentage brightness_kbd, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness_kbd. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_brightness_kbd_get (GpmBrightnessKbd *brightness,
			guint		 *brightness_level)
{
	guint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
						       brightness->priv->levels);
	*brightness_level = percentage;
	return TRUE;
}

/**
 * gpm_brightness_kbd_up:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * If possible, put the brightness_kbd of the KBD up one unit.
 **/
gboolean
gpm_brightness_kbd_up (GpmBrightnessKbd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion brightness_kbd levels, be a bit clever */
		step = gpm_brightness_kbd_get_step (brightness);
		/* don't overflow */
		if (brightness->priv->current_hw + step > brightness->priv->levels - 1) {
			step = (brightness->priv->levels - 1) - brightness->priv->current_hw;
		}
		gpm_brightness_kbd_set_hw (brightness, brightness->priv->current_hw + step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
					      brightness->priv->levels);
	egg_debug ("emitting brightness-changed (%i)", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);

	return TRUE;
}

/**
 * gpm_brightness_kbd_down:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * If possible, put the brightness_kbd of the KBD down one unit.
 **/
gboolean
gpm_brightness_kbd_down (GpmBrightnessKbd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion brightness_kbd levels, be a bit clever */
		step = gpm_brightness_kbd_get_step (brightness);
		/* don't underflow */
		if (brightness->priv->current_hw < step) {
			step = brightness->priv->current_hw;
		}
		gpm_brightness_kbd_set_hw (brightness, brightness->priv->current_hw - step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
					      brightness->priv->levels);
	egg_debug ("emitting brightness-changed (%i)", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);

	return TRUE;
}

/**
 * gpm_brightness_kbd_finalize:
 **/
static void
gpm_brightness_kbd_finalize (GObject *object)
{
	GpmBrightnessKbd *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS_KBD (object));
	brightness = GPM_BRIGHTNESS_KBD (object);

	if (brightness->priv->udi != NULL) {
		g_free (brightness->priv->udi);
	}
	if (brightness->priv->gproxy != NULL) {
		g_object_unref (brightness->priv->gproxy);
	}
	if (brightness->priv->conf != NULL) {
		g_object_unref (brightness->priv->conf);
	}
	if (brightness->priv->sensor != NULL) {
		g_object_unref (brightness->priv->sensor);
	}

	g_return_if_fail (brightness->priv != NULL);
	G_OBJECT_CLASS (gpm_brightness_kbd_parent_class)->finalize (object);
}

/**
 * gpm_brightness_kbd_class_init:
 **/
static void
gpm_brightness_kbd_class_init (GpmBrightnessKbdClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_brightness_kbd_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBrightnessKbdClass, brightness_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmBrightnessKbdPrivate));
}

enum {
	STATE_FORCED_UNKNOWN,
	STATE_FORCED_ON,
	STATE_FORCED_OFF
};

/**
 * adjust_kbd_brightness_according_to_ambient_light:
 * @brightness: class instance
 * @startup: whether we should set the backlight depending on the
 * current ambient light level
 *
 * This function adjusts the keyboard backlight according to ambient
 * light. It tries to be smart about things. So, if we detect that
 * the light changes to very dark (30%) we force the backlight on
 * and if we detect that it changes to very bright (70%) we force
 * the backlight off. The reason for this is that we want to
 * respect the users settings and try to change as little as possible,
 * e.g. should it get dark (30%) we know we've already forced it off
 * so the user can e.g. change it himself too as we won't do anything
 * until it's very bright again.
 *
 * For startup conditions we look at whether the ambient light is
 * greater or lower than 50% to set force keyboard backlight on/off.
 * Note that enabling the keyboard backlight after disabling it is
 * a startup condition too.
 */
static gboolean
adjust_kbd_brightness_according_to_ambient_light (GpmBrightnessKbd *brightness,
						  gboolean startup)
{
	guint ambient_light;
	static int state = STATE_FORCED_UNKNOWN;

	if (brightness->priv->sensor == NULL) {
		return FALSE;
	}

	gpm_light_sensor_get_absolute (brightness->priv->sensor, &ambient_light);

	/* this is also used if user reenables the keyboard backlight */
	if (startup) {
		state = STATE_FORCED_UNKNOWN;
	}

 	egg_debug ("ambient light percent = %d", ambient_light);

	if (state == STATE_FORCED_UNKNOWN) {
		/* if this is the first time we're launched with ambient light data... */
		if (ambient_light < 50) {
			gpm_brightness_kbd_set_std (brightness, 100);
			state = STATE_FORCED_ON;
		} else {
			gpm_brightness_kbd_set_std (brightness, 0);
			state = STATE_FORCED_OFF;
		}
	} else {
		if (ambient_light < 30 && state != STATE_FORCED_ON) {
			/* if it's dark.. and we haven't already turned light on...
			 *   => turn it on.. full blast! */
			gpm_brightness_kbd_set_std (brightness, 100);
			state = STATE_FORCED_ON;
		} else if (ambient_light > 70 && state != STATE_FORCED_OFF) {
			/* if it's bright... and we haven't already turned light off...
			 *   => turn it off */
			gpm_brightness_kbd_set_std (brightness, 0);
			state = STATE_FORCED_OFF;
		}
	}
	return TRUE;
}

/**
 * sensor_changed_cb:
 * @sensor: the brightness sensor
 * @ambient_light: ambient light percentage (0: dark, 100: bright)
 * @brightness: the keyboard brightness instance
 *
 * Called when the reading from the ambient light sensor changes.
 **/
static void
sensor_changed_cb (GpmLightSensor	*sensor,
		   guint	         ambient_light,
		   GpmBrightnessKbd     *brightness)
{
	if (brightness->priv->is_disabled == FALSE) {
		adjust_kbd_brightness_according_to_ambient_light (brightness, FALSE);
	}
}
#if 0
/**
 * gpm_brightness_kbd_is_disabled:
 * @brightness: the instance
 * @is_disabled: out value
 *
 * Returns whether the keyboard backlight is disabled by the user
 */
gboolean
gpm_brightness_kbd_is_disabled  (GpmBrightnessKbd	*brightness,
				     gboolean                   *is_disabled)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);
	g_return_val_if_fail (is_disabled != NULL, FALSE);

	*is_disabled = brightness->priv->is_disabled;

	return TRUE;
}
#endif

/**
 * gpm_brightness_kbd_toggle:
 * @brightness: the instance
 * @is_disabled: whether keyboard backlight is disabled by the user
 * @do_startup_on_enable: whether we should automatically select the
 * keyboard backlight depending on the ambient light when enabling it.
 *
 * Set whether keyboard backlight is disabled by the user. Note that
 * do_startup_on_enable only makes sense if is_disables is FALSE. Typically
 * one wants do_startup_on_enable=TRUE when handling the keyboard backlight
 * is already disabled and the user presses illum+ and you want to enable
 * the backlight in response to that.
 **/
gboolean
gpm_brightness_kbd_toggle (GpmBrightnessKbd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_KBD (brightness), FALSE);

	if (brightness->priv->is_disabled == FALSE) {
		/* go dark, that's what the user wants */
		gpm_brightness_kbd_set_std (brightness, 0);
	} else {
		/* select the appropriate level just as when we're starting up */
//		if (do_startup_on_enable) {
			adjust_kbd_brightness_according_to_ambient_light (brightness, TRUE);
			gpm_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
//		}
	}
	return TRUE;
}

/**
 * gpm_brightness_kbd_init:
 * @brightness_kbd: This brightness_kbd class instance
 *
 * initialises the brightness_kbd class. NOTE: We expect keyboard_backlight objects
 * to *NOT* be removed or added during the session.
 * We only control the first keyboard_backlight object if there are more than one.
 **/
static void
gpm_brightness_kbd_init (GpmBrightnessKbd *brightness)
{
	gchar **names;
	HalGManager *manager;
	HalGDevice *device;

	brightness->priv = GPM_BRIGHTNESS_KBD_GET_PRIVATE (brightness);

	brightness->priv->conf = gpm_conf_new ();

	/* listen for ambient light changes.. if we have an ambient light sensor */
	brightness->priv->sensor = gpm_light_sensor_new ();
	if (brightness->priv->sensor != NULL) {
		g_signal_connect (brightness->priv->sensor, "brightness-changed",
				  G_CALLBACK (sensor_changed_cb), brightness);
	}

	/* save udi of kbd adapter */
	manager = hal_gmanager_new ();
	hal_gmanager_find_capability (manager, "keyboard_backlight", &names, NULL);
	g_object_unref (manager);
	if (names == NULL || names[0] == NULL) {
		egg_warning ("No devices of capability keyboard_backlight");
		return;
	}

	/* We only want first keyboard_backlight object (should only be one) */
	brightness->priv->udi = g_strdup (names[0]);
	hal_gmanager_free_capability (names);

	brightness->priv->does_own_dimming = FALSE;
	brightness->priv->does_own_updates = FALSE;
	brightness->priv->is_disabled = FALSE;

	/* get a managed proxy */
	brightness->priv->gproxy = dbus_proxy_new ();
	dbus_proxy_assign (brightness->priv->gproxy,
			  DBUS_PROXY_SYSTEM,
			  HAL_DBUS_SERVICE,
			  brightness->priv->udi,
			  HAL_DBUS_INTERFACE_KBD_BACKLIGHT);

	/* get levels that the adapter supports -- this does not change ever */
	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, brightness->priv->udi);
	hal_gdevice_get_uint (device, "keyboard_backlight.num_levels",
			      &brightness->priv->levels, NULL);
	g_object_unref (device);

	/* this changes under our feet */
	gpm_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);

	/* set to known value */
	brightness->priv->level_dim_hw = 1;
	brightness->priv->level_std_hw = 1;

	egg_debug ("Starting: (%i of %i)",
		   brightness->priv->current_hw,
		   brightness->priv->levels - 1);

	/* choose a start value */
	adjust_kbd_brightness_according_to_ambient_light (brightness, TRUE);
}

/**
 * gpm_brightness_kbd_has_hw:
 *
 * Self contained function that works out if we have the hardware.
 * If not, we return FALSE and the module is unloaded.
 **/
gboolean
gpm_brightness_kbd_has_hw (void)
{
	HalGManager *manager;
	gchar **names;
	gboolean ret = TRUE;

	/* okay, as singleton - so we don't allocate more memory */
	manager = hal_gmanager_new ();
	hal_gmanager_find_capability (manager, "keyboard_backlight", &names, NULL);
	g_object_unref (manager);

	/* nothing found */
	if (names == NULL || names[0] == NULL) {
		ret = FALSE;
	}

	hal_gmanager_free_capability (names);
	return ret;
}

/**
 * gpm_brightness_kbd_new:
 * Return value: A new brightness_kbd class instance.
 **/
GpmBrightnessKbd *
gpm_brightness_kbd_new (void)
{
	GpmBrightnessKbd *brightness;

	/* only load an instance of this module if we have the hardware */
	if (gpm_brightness_kbd_has_hw () == FALSE) {
		return NULL;
	}

	brightness = g_object_new (GPM_TYPE_BRIGHTNESS_KBD, NULL);
	return GPM_BRIGHTNESS_KBD (brightness);
}
