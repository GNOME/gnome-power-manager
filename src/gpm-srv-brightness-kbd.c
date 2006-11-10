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

#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-brightness-kbd.h"
#include "gpm-srv-brightness-kbd.h"
#include "gpm-brightness-kbd.h"
#include "gpm-conf.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-feedback-widget.h"
#include "gpm-hal.h"
#include "gpm-idle.h"
#include "gpm-light-sensor.h"
#include "gpm-stock-icons.h"

#define DIM_INTERVAL		10 /* ms */

#define GPM_SRV_BRIGHTNESS_KBD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SRV_BRIGHTNESS_KBD, GpmSrvBrightnessKbdPrivate))

struct GpmSrvBrightnessKbdPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmButton		*button;
	GpmBrightnessKbd	*brightness;
	GpmConf			*conf;
	GpmFeedback		*feedback;
	GpmIdle			*idle;
	GpmHal			*hal;
	GpmLightSensor		*sensor;
};

G_DEFINE_TYPE (GpmSrvBrightnessKbd, gpm_srv_brightness_kbd, G_TYPE_OBJECT)

#if 0
/**
 * gpm_srv_brightness_kbd_up:
 * @srv_brightness_kbd: This srv_brightness_kbd class instance
 *
 * If possible, put the srv_brightness_kbd of the KBD up one unit.
 **/
gboolean
gpm_srv_brightness_kbd_up (GpmSrvBrightnessKbd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BRIGHTNESS_KBD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_srv_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion srv_brightness_kbd levels, be a bit clever */
		step = gpm_srv_brightness_kbd_get_step (brightness);
		/* don't overflow */
		if (brightness->priv->current_hw + step > brightness->priv->levels - 1) {
			step = (brightness->priv->levels - 1) - brightness->priv->current_hw;
		}
		gpm_srv_brightness_kbd_set_hw (brightness, brightness->priv->current_hw + step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);

	gpm_debug ("Need to diplay backlight feedback value %i", percentage);
	gpm_feedback_display_value (brightness->priv->feedback, (float) percentage / 100.0f);
	return TRUE;
}

/**
 * gpm_srv_brightness_kbd_down:
 * @srv_brightness_kbd: This srv_brightness_kbd class instance
 *
 * If possible, put the srv_brightness_kbd of the KBD down one unit.
 **/
gboolean
gpm_srv_brightness_kbd_down (GpmSrvBrightnessKbd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BRIGHTNESS_KBD (brightness), FALSE);

	/* Do we find the new value, or set the new value */
	if (brightness->priv->does_own_updates) {
		gpm_srv_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
	} else {
		/* macbook pro has a bazzillion srv_brightness_kbd levels, be a bit clever */
		step = gpm_srv_brightness_kbd_get_step (brightness);
		/* don't underflow */
		if (brightness->priv->current_hw < step) {
			step = brightness->priv->current_hw;
		}
		gpm_srv_brightness_kbd_set_hw (brightness, brightness->priv->current_hw - step);
	}

	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);

	gpm_debug ("Need to diplay backlight feedback value %i", percentage);
	gpm_feedback_display_value (brightness->priv->feedback, (float) percentage / 100.0f);
	return TRUE;
}
#endif

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf          *conf,
		     const gchar      *key,
		     GpmSrvBrightnessKbd *brightness)
{
	gint value;
	GpmAcAdapterState state;

	gpm_ac_adapter_get_state (brightness->priv->ac_adapter, &state);

	if (strcmp (key, GPM_CONF_AC_BRIGHTNESS_KBD) == 0) {

		gpm_conf_get_int (brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
		if (state == GPM_AC_ADAPTER_PRESENT) {
			gpm_brightness_kbd_set_std (brightness->priv->brightness, value);
		}

	} else if (strcmp (key, GPM_CONF_BATTERY_BRIGHTNESS_KBD) == 0) {

		gpm_conf_get_int (brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
		if (state == GPM_AC_ADAPTER_MISSING) {
			gpm_brightness_kbd_set_std (brightness->priv->brightness, value);
		}

	}
}

/**
 * ac_adapter_changed_cb:
 * @ac_adapter: The ac_adapter class instance
 * @on_ac: if we are on AC power
 * @brightness: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter      *ac_adapter,
			GpmAcAdapterState state,
			GpmSrvBrightnessKbd *brightness)
{
	guint value;

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_uint (brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS_KBD, &value);
	} else {
		gpm_conf_get_uint (brightness->priv->conf, GPM_CONF_BATTERY_BRIGHTNESS_KBD, &value);
	}

	gpm_brightness_kbd_set_std (brightness->priv->brightness, value);
}

/**
 * button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @brightness: This class instance
 **/
static void
button_pressed_cb (GpmButton        *button,
		   const gchar      *type,
		   GpmSrvBrightnessKbd *brightness)
{
	gpm_debug ("Button press event type=%s", type);

	if ((strcmp (type, GPM_BUTTON_KBD_BRIGHT_UP) == 0)) {
		gpm_brightness_kbd_up (brightness->priv->brightness);

	} else if ((strcmp (type, GPM_BUTTON_KBD_BRIGHT_UP) == 0)) {
		gpm_brightness_kbd_down (brightness->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_KBD_BRIGHT_TOGGLE) == 0) {
		gpm_brightness_kbd_toggle (brightness->priv->brightness);

	}
}

/**keyboard_backlight
 * gpm_srv_brightness_kbd_constructor:
 **/
static GObject *
gpm_srv_brightness_kbd_constructor (GType type,
			        guint n_construct_properties,
			        GObjectConstructParam *construct_properties)
{
	GpmSrvBrightnessKbd      *brightness;
	GpmSrvBrightnessKbdClass *klass;
	klass = GPM_SRV_BRIGHTNESS_KBD_CLASS (g_type_class_peek (GPM_TYPE_SRV_BRIGHTNESS_KBD));
	brightness = GPM_SRV_BRIGHTNESS_KBD (G_OBJECT_CLASS (gpm_srv_brightness_kbd_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

/**
 * gpm_srv_brightness_kbd_finalize:
 **/
static void
gpm_srv_brightness_kbd_finalize (GObject *object)
{
	GpmSrvBrightnessKbd *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_BRIGHTNESS_KBD (object));
	brightness = GPM_SRV_BRIGHTNESS_KBD (object);

	if (brightness->priv->feedback != NULL) {
		g_object_unref (brightness->priv->feedback);
	}
	if (brightness->priv->conf != NULL) {
		g_object_unref (brightness->priv->conf);
	}
	if (brightness->priv->sensor != NULL) {
		g_object_unref (brightness->priv->sensor);
	}
	if (brightness->priv->ac_adapter != NULL) {
		g_object_unref (brightness->priv->ac_adapter);
	}
	if (brightness->priv->button != NULL) {
		g_object_unref (brightness->priv->button);
	}
	if (brightness->priv->idle != NULL) {
		g_object_unref (brightness->priv->idle);
	}

	g_return_if_fail (brightness->priv != NULL);
	G_OBJECT_CLASS (gpm_srv_brightness_kbd_parent_class)->finalize (object);
}

/**
 * gpm_srv_brightness_kbd_class_init:
 **/
static void
gpm_srv_brightness_kbd_class_init (GpmSrvBrightnessKbdClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_srv_brightness_kbd_finalize;
	object_class->constructor  = gpm_srv_brightness_kbd_constructor;

	g_type_class_add_private (klass, sizeof (GpmSrvBrightnessKbdPrivate));
}

#if 0
/**
 * gpm_srv_brightness_kbd_is_disabled:
 * @brightness: the instance
 * @is_disabled: out value
 *
 * Returns whether the keyboard backlight is disabled by the user
 */
gboolean
gpm_srv_brightness_kbd_is_disabled  (GpmSrvBrightnessKbd	*brightness,
				     gboolean                   *is_disabled)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BRIGHTNESS_KBD (brightness), FALSE);
	g_return_val_if_fail (is_disabled != NULL, FALSE);

	*is_disabled = brightness->priv->is_disabled;

	return TRUE;
}
#endif

#if 0
/**
 * gpm_srv_brightness_kbd_toggle:
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
gpm_srv_brightness_kbd_toggle (GpmSrvBrightnessKbd *brightness)
{
	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BRIGHTNESS_KBD (brightness), FALSE);

	if (brightness->priv->is_disabled == FALSE) {
		/* go dark, that's what the user wants */
		gpm_srv_brightness_kbd_set_std (brightness, 0);
		gpm_feedback_display_value (brightness->priv->feedback, 0.0f);
	} else {
		/* select the appropriate level just as when we're starting up */
//		if (do_startup_on_enable) {
			adjust_kbd_brightness_according_to_ambient_light (brightness, TRUE);
			gpm_srv_brightness_kbd_get_hw (brightness, &brightness->priv->current_hw);
			gpm_feedback_display_value (brightness->priv->feedback,
						    (gfloat) gpm_discrete_to_percent (brightness->priv->current_hw,
										     brightness->priv->levels) / 100.0f);
//		}
	}
	return TRUE;
}
#endif

/**
 * idle_changed_cb:
 * @idle: The idle class instance
 * @mode: The idle mode, e.g. GPM_IDLE_MODE_SESSION
 * @manager: This class instance
 *
 * This callback is called when gnome-screensaver detects that the idle state
 * has changed. GPM_IDLE_MODE_SESSION is when the session has become inactive,
 * and GPM_IDLE_MODE_SYSTEM is where the session has become inactive, AND the
 * session timeout has elapsed for the idle action.
 **/
static void
idle_changed_cb (GpmIdle          *idle,
		 GpmIdleMode       mode,
		 GpmSrvBrightnessKbd *brightness)
{
	if (mode == GPM_IDLE_MODE_NORMAL) {

		gpm_brightness_kbd_undim (brightness->priv->brightness);

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		gpm_brightness_kbd_dim (brightness->priv->brightness);
	}
}

/**
 * gpm_srv_brightness_kbd_init:
 * @srv_brightness_kbd: This srv_brightness_kbd class instance
 *
 * initialises the srv_brightness_kbd class. NOTE: We expect keyboard_backlight objects
 * to *NOT* be removed or added during the session.
 * We only control the first keyboard_backlight object if there are more than one.
 **/
static void
gpm_srv_brightness_kbd_init (GpmSrvBrightnessKbd *brightness)
{
	brightness->priv = GPM_SRV_BRIGHTNESS_KBD_GET_PRIVATE (brightness);

	brightness->priv->hal = gpm_hal_new ();
	brightness->priv->conf = gpm_conf_new ();
	g_signal_connect (brightness->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), brightness);

	brightness->priv->feedback = gpm_feedback_new ();
	gpm_feedback_set_icon_name (brightness->priv->feedback,
				    GPM_STOCK_BRIGHTNESS_KBD);

	/* we use ac_adapter for the ac-adapter-changed signal */
	brightness->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (brightness->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), brightness);

	/* watch for brightness up and down buttons */
	brightness->priv->button = gpm_button_new ();
	g_signal_connect (brightness->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), brightness);

	/* watch for idle mode changes */
	brightness->priv->idle = gpm_idle_new ();
	g_signal_connect (brightness->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), brightness);
}

/**
 * gpm_srv_brightness_kbd_new:
 * Return value: A new srv_brightness_kbd class instance.
 **/
GpmSrvBrightnessKbd *
gpm_srv_brightness_kbd_new (void)
{
	GpmSrvBrightnessKbd *srv_brightness = NULL;

	/* only load an instance of this module if we have the hardware */
	if (gpm_brightness_kbd_has_hw () == TRUE) {
		srv_brightness = g_object_new (GPM_TYPE_SRV_BRIGHTNESS_KBD, NULL);
	}

	return srv_brightness;
}
