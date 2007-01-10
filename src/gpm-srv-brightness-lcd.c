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

#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-srv-brightness-lcd.h"
#include "gpm-brightness-lcd.h"
#include "gpm-conf.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-feedback-widget.h"
#include "gpm-hal.h"
#include "gpm-idle.h"
#include "gpm-light-sensor.h"
#include "gpm-marshal.h"
#include "gpm-proxy.h"
#include "gpm-stock-icons.h"

#define DIM_INTERVAL		10 /* ms */

#define GPM_SRV_BRIGHTNESS_LCD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SRV_BRIGHTNESS_LCD, GpmSrvBrightnessLcdPrivate))

struct GpmSrvBrightnessLcdPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmBrightnessLcd	*brightness;
	GpmButton		*button;
	GpmConf			*conf;
	GpmFeedback		*feedback;
	GpmIdle			*idle;
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmSrvBrightnessLcd, gpm_srv_brightness_lcd, G_TYPE_OBJECT)

/**
 * gpm_srv_brightness_lcd_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_srv_brightness_lcd_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("gpm_brightness_error");
	}
	return quark;
}

/**
 * gpm_brightness_lcd_get_policy:
 **/
gboolean
gpm_brightness_lcd_get_brightness (GpmSrvBrightnessLcd	*srv_brightness,
			           guint		*brightness,
			           GError		**error)
{
	guint level;
	gboolean ret;
	g_return_val_if_fail (srv_brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BRIGHTNESS_LCD (srv_brightness), FALSE);
	g_return_val_if_fail (brightness != NULL, FALSE);

	/* gets the current brightness */
	ret = gpm_brightness_lcd_get (srv_brightness->priv->brightness, &level);
	if (ret == TRUE) {
		*brightness = level;
	} else {
		*error = g_error_new (gpm_srv_brightness_lcd_error_quark (),
				      GPM_BRIGHTNESS_LCD_ERROR_DATA_NOT_AVAILABLE,
				      "Data not available");
	}
	return ret;
}

/**
 * gpm_brightness_lcd_set_brightness:
 **/
gboolean gpm_brightness_lcd_set_brightness (GpmSrvBrightnessLcd	*srv_brightness,
				            guint		 brightness,
				            GError		**error)
{
	gboolean ret;
	g_return_val_if_fail (srv_brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BRIGHTNESS_LCD (srv_brightness), FALSE);

	/* sets the current policy brightness */
	ret = gpm_brightness_lcd_set_std (srv_brightness->priv->brightness, brightness);
	if (ret == FALSE) {
		*error = g_error_new (gpm_srv_brightness_lcd_error_quark (),
				      GPM_BRIGHTNESS_LCD_ERROR_GENERAL,
				      "Cannot set policy brightness");
	}
	return ret;
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf          *conf,
		     const gchar      *key,
		     GpmSrvBrightnessLcd *srv_brightness)
{
	gint value;
	GpmAcAdapterState state;

	gpm_ac_adapter_get_state (srv_brightness->priv->ac_adapter, &state);

	if (strcmp (key, GPM_CONF_AC_BRIGHTNESS) == 0) {

		gpm_conf_get_int (srv_brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
		if (state == GPM_AC_ADAPTER_PRESENT) {
			gpm_brightness_lcd_set_std (srv_brightness->priv->brightness, value);
		}

	} else if (strcmp (key, GPM_CONF_BATTERY_BRIGHTNESS) == 0) {

		gpm_conf_get_int (srv_brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
		if (state == GPM_AC_ADAPTER_MISSING) {
			gpm_brightness_lcd_set_std (srv_brightness->priv->brightness, value);
		}

	} else if (strcmp (key, GPM_CONF_PANEL_DIM_BRIGHTNESS) == 0) {

		gpm_conf_get_int (srv_brightness->priv->conf, GPM_CONF_PANEL_DIM_BRIGHTNESS, &value);
		gpm_brightness_lcd_set_dim (srv_brightness->priv->brightness, value);
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
			GpmSrvBrightnessLcd *srv_brightness)
{
	gboolean do_laptop_lcd;
	guint value;

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_uint (srv_brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
	} else {
		gpm_conf_get_uint (srv_brightness->priv->conf, GPM_CONF_BATTERY_BRIGHTNESS, &value);
	}

	/* only do brightness changes if we have the hardware */
	gpm_conf_get_bool (srv_brightness->priv->conf, GPM_CONF_DISPLAY_STATE_CHANGE, &do_laptop_lcd);
	if (do_laptop_lcd) {
		gpm_brightness_lcd_set_std (srv_brightness->priv->brightness, value);
	}
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
		   GpmSrvBrightnessLcd *srv_brightness)
{
	gpm_debug ("Button press event type=%s", type);

	if (strcmp (type, GPM_BUTTON_BRIGHT_UP) == 0) {

		gpm_brightness_lcd_up (srv_brightness->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_BRIGHT_DOWN) == 0) {

		gpm_brightness_lcd_down (srv_brightness->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {

		/* make sure we undim when we lift the lid */
		gpm_brightness_lcd_undim (srv_brightness->priv->brightness);
	}
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
idle_changed_cb (GpmIdle             *idle,
		 GpmIdleMode          mode,
		 GpmSrvBrightnessLcd *srv_brightness)
{
	GpmAcAdapterState state;
	gboolean laptop_do_dim;

	gpm_ac_adapter_get_state (srv_brightness->priv->ac_adapter, &state);
	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_bool (srv_brightness->priv->conf, GPM_CONF_AC_IDLE_DIM, &laptop_do_dim);
	} else {
		gpm_conf_get_bool (srv_brightness->priv->conf, GPM_CONF_BATTERY_IDLE_DIM, &laptop_do_dim);
	}

	/* should we ignore this? */
	if (laptop_do_dim == FALSE) {
		return;
	}

	/* don't dim or undim the screen when the lid is closed */
	if (button_is_lid_closed (srv_brightness->priv->button) == TRUE) {
		return;
	}

	if (mode == GPM_IDLE_MODE_NORMAL) {

		gpm_brightness_lcd_undim (srv_brightness->priv->brightness);

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		/* Dim the screen, fixes #328564 */
		gpm_brightness_lcd_dim (srv_brightness->priv->brightness);
	}
}

/**
 * brightness_changed_cb:
 * @brightness: The GpmBrightnessLcd class instance
 * @percentage: The new percentage brightness
 * @brightness: This class instance
 *
 * This callback is called when the brightness value changes.
 **/
static void
brightness_changed_cb (GpmBrightnessLcd    *brightness,
		       guint                percentage,
		       GpmSrvBrightnessLcd *srv_brightness)
{
	gpm_debug ("Need to display backlight feedback value %i", percentage);
	gpm_feedback_display_value (srv_brightness->priv->feedback, (float) percentage / 100.0f);

	/* we emit a signal for the brightness applet */
	gpm_debug ("emitting brightness-changed : %i", percentage);
	g_signal_emit (srv_brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);
}

/**
 * gpm_srv_brightness_lcd_finalize:
 **/
static void
gpm_srv_brightness_lcd_finalize (GObject *object)
{
	GpmSrvBrightnessLcd *srv_brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_BRIGHTNESS_LCD (object));
	srv_brightness = GPM_SRV_BRIGHTNESS_LCD (object);

	if (srv_brightness->priv->feedback != NULL) {
		g_object_unref (srv_brightness->priv->feedback);
	}
	if (srv_brightness->priv->conf != NULL) {
		g_object_unref (srv_brightness->priv->conf);
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
	G_OBJECT_CLASS (gpm_srv_brightness_lcd_parent_class)->finalize (object);
}

/**
 * gpm_srv_brightness_lcd_class_init:
 **/
static void
gpm_srv_brightness_lcd_class_init (GpmSrvBrightnessLcdClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_srv_brightness_lcd_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSrvBrightnessLcdClass, brightness_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmSrvBrightnessLcdPrivate));
}

/**
 * gpm_srv_brightness_lcd_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect laptop_panel objects
 * to *NOT* be removed or added during the session.
 * We only control the first laptop_panel object if there are more than one.
 **/
static void
gpm_srv_brightness_lcd_init (GpmSrvBrightnessLcd *srv_brightness)
{
	gint value;

	srv_brightness->priv = GPM_SRV_BRIGHTNESS_LCD_GET_PRIVATE (srv_brightness);

	/* watch for dim value changes */
	srv_brightness->priv->conf = gpm_conf_new ();
	g_signal_connect (srv_brightness->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), srv_brightness);

	/* watch for manual brightness changes (for the feedback widget) */
	srv_brightness->priv->brightness = gpm_brightness_lcd_new ();
	g_signal_connect (srv_brightness->priv->brightness, "brightness-changed",
			  G_CALLBACK (brightness_changed_cb), srv_brightness);

	/* set the default dim */
	gpm_conf_get_int (srv_brightness->priv->conf, GPM_CONF_PANEL_DIM_BRIGHTNESS, &value);
	gpm_brightness_lcd_set_dim (srv_brightness->priv->brightness, value);

	/* use a visual widget */
	srv_brightness->priv->feedback = gpm_feedback_new ();
	gpm_feedback_set_icon_name (srv_brightness->priv->feedback,
				    GPM_STOCK_BRIGHTNESS_LCD);

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
 * gpm_srv_brightness_lcd_new:
 * Return value: A new brightness class instance.
 **/
GpmSrvBrightnessLcd *
gpm_srv_brightness_lcd_new (void)
{
	GpmSrvBrightnessLcd *srv_brightness = NULL;
	if (gpm_brightness_lcd_has_hw () == TRUE) {
		srv_brightness = g_object_new (GPM_TYPE_SRV_BRIGHTNESS_LCD, NULL);
	}
	return srv_brightness;
}
