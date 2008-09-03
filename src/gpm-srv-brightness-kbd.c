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

#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-brightness-kbd.h"
#include "gpm-srv-brightness-kbd.h"
#include "gpm-conf.h"
#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-feedback-widget.h"
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
	GpmLightSensor		*sensor;
};

G_DEFINE_TYPE (GpmSrvBrightnessKbd, gpm_srv_brightness_kbd, G_TYPE_OBJECT)

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf          *conf,
		     const gchar      *key,
		     GpmSrvBrightnessKbd *srv_brightness)
{
	gint value;
	gboolean on_ac;

	on_ac = gpm_ac_adapter_is_present (srv_brightness->priv->ac_adapter);

	if (strcmp (key, GPM_CONF_KEYBOARD_BRIGHTNESS_AC) == 0) {

		gpm_conf_get_int (srv_brightness->priv->conf, GPM_CONF_KEYBOARD_BRIGHTNESS_AC, &value);
		if (on_ac) {
			gpm_brightness_kbd_set_std (srv_brightness->priv->brightness, value);
		}

	} else if (strcmp (key, GPM_CONF_KEYBOARD_BRIGHTNESS_BATT) == 0) {

		gpm_conf_get_int (srv_brightness->priv->conf, GPM_CONF_KEYBOARD_BRIGHTNESS_AC, &value);
		if (on_ac == FALSE) {
			gpm_brightness_kbd_set_std (srv_brightness->priv->brightness, value);
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
			gboolean	  on_ac,
			GpmSrvBrightnessKbd *srv_brightness)
{
	guint value;

	if (on_ac) {
		gpm_conf_get_uint (srv_brightness->priv->conf, GPM_CONF_KEYBOARD_BRIGHTNESS_AC, &value);
	} else {
		gpm_conf_get_uint (srv_brightness->priv->conf, GPM_CONF_KEYBOARD_BRIGHTNESS_BATT, &value);
	}

	gpm_brightness_kbd_set_std (srv_brightness->priv->brightness, value);
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
		   GpmSrvBrightnessKbd *srv_brightness)
{
	egg_debug ("Button press event type=%s", type);

	if ((strcmp (type, GPM_BUTTON_KBD_BRIGHT_UP) == 0)) {
		gpm_brightness_kbd_up (srv_brightness->priv->brightness);

	} else if ((strcmp (type, GPM_BUTTON_KBD_BRIGHT_DOWN) == 0)) {
		gpm_brightness_kbd_down (srv_brightness->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_KBD_BRIGHT_TOGGLE) == 0) {
		gpm_brightness_kbd_toggle (srv_brightness->priv->brightness);

	}
}

/**
 * gpm_srv_brightness_kbd_finalize:
 **/
static void
gpm_srv_brightness_kbd_finalize (GObject *object)
{
	GpmSrvBrightnessKbd *srv_brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_BRIGHTNESS_KBD (object));
	srv_brightness = GPM_SRV_BRIGHTNESS_KBD (object);

	if (srv_brightness->priv->feedback != NULL) {
		g_object_unref (srv_brightness->priv->feedback);
	}
	if (srv_brightness->priv->conf != NULL) {
		g_object_unref (srv_brightness->priv->conf);
	}
	if (srv_brightness->priv->sensor != NULL) {
		g_object_unref (srv_brightness->priv->sensor);
	}
	if (srv_brightness->priv->ac_adapter != NULL) {
		g_object_unref (srv_brightness->priv->ac_adapter);
	}
	if (srv_brightness->priv->button != NULL) {
		g_object_unref (srv_brightness->priv->button);
	}
	if (srv_brightness->priv->idle != NULL) {
		g_object_unref (srv_brightness->priv->idle);
	}
	if (srv_brightness->priv->brightness != NULL) {
		g_object_unref (srv_brightness->priv->brightness);
	}

	g_return_if_fail (srv_brightness->priv != NULL);
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

	g_type_class_add_private (klass, sizeof (GpmSrvBrightnessKbdPrivate));
}

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
		 GpmSrvBrightnessKbd *srv_brightness)
{
	if (mode == GPM_IDLE_MODE_NORMAL) {

		gpm_brightness_kbd_undim (srv_brightness->priv->brightness);

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		gpm_brightness_kbd_dim (srv_brightness->priv->brightness);
	}
}

/**
 * brightness_changed_cb:
 * @brightness: The GpmBrightnessKbd class instance
 * @percentage: The new percentage brightness
 * @brightness: This class instance
 *
 * This callback is called when the brightness value changes.
 **/
static void
brightness_changed_cb (GpmBrightnessKbd    *brightness,
		       gint                 percentage,
		       GpmSrvBrightnessKbd *srv_brightness)
{
	egg_debug ("Need to display backlight feedback value %i", percentage);
	gpm_feedback_display_value (srv_brightness->priv->feedback, (float) percentage / 100.0f);
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
gpm_srv_brightness_kbd_init (GpmSrvBrightnessKbd *srv_brightness)
{
	srv_brightness->priv = GPM_SRV_BRIGHTNESS_KBD_GET_PRIVATE (srv_brightness);

	srv_brightness->priv->conf = gpm_conf_new ();

	/* watch for dim value changes */
	g_signal_connect (srv_brightness->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), srv_brightness);

	/* watch for manual brightness changes (for the feedback widget) */
	srv_brightness->priv->brightness = gpm_brightness_kbd_new ();
	g_signal_connect (srv_brightness->priv->brightness, "brightness-changed",
			  G_CALLBACK (brightness_changed_cb), srv_brightness);

	/* use a visual widget */
	srv_brightness->priv->feedback = gpm_feedback_new ();
	gpm_feedback_set_icon_name (srv_brightness->priv->feedback,
				    GPM_STOCK_BRIGHTNESS_KBD);

	/* we use ac_adapter for the ac-adapter-changed signal */
	srv_brightness->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (srv_brightness->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), srv_brightness);

	/* watch for brightness up and down buttons */
	srv_brightness->priv->button = gpm_button_new ();
	g_signal_connect (srv_brightness->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), srv_brightness);

	/* watch for idle mode changes */
	srv_brightness->priv->idle = gpm_idle_new ();
	g_signal_connect (srv_brightness->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), srv_brightness);
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
	if (gpm_brightness_kbd_has_hw ()) {
		srv_brightness = g_object_new (GPM_TYPE_SRV_BRIGHTNESS_KBD, NULL);
	}

	return srv_brightness;
}
