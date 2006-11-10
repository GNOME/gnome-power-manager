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

G_DEFINE_TYPE (GpmSrvBrightnessLcd, gpm_srv_brightness_lcd, G_TYPE_OBJECT)

#if 0
/**
 * gpm_srv_brightness_lcd_up:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
static gboolean
gpm_srv_brightness_lcd_up (GpmSrvBrightnessLcd *brightness)
{
	percentage = gpm_discrete_to_percent (brightness->priv->current_hw,
						   brightness->priv->levels);

	gpm_debug ("Need to diplay backlight feedback value %i", percentage);
	gpm_feedback_display_value (brightness->priv->feedback, (float) percentage / 100.0f);
	return TRUE;
}

/**
 * gpm_srv_brightness_lcd_down:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
static gboolean
gpm_srv_brightness_lcd_down (GpmSrvBrightnessLcd *brightness)
{
	gint step;
	gint percentage;

	g_return_val_if_fail (brightness != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BRIGHTNESS_LCD (brightness), FALSE);

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
		     GpmSrvBrightnessLcd *brightness)
{
	gint value;
	GpmAcAdapterState state;

	gpm_ac_adapter_get_state (brightness->priv->ac_adapter, &state);

	if (strcmp (key, GPM_CONF_AC_BRIGHTNESS) == 0) {

		gpm_conf_get_int (brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
		if (state == GPM_AC_ADAPTER_PRESENT) {
			gpm_brightness_lcd_set_std (brightness->priv->brightness, value);
		}

	} else if (strcmp (key, GPM_CONF_BATTERY_BRIGHTNESS) == 0) {

		gpm_conf_get_int (brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
		if (state == GPM_AC_ADAPTER_MISSING) {
			gpm_brightness_lcd_set_std (brightness->priv->brightness, value);
		}

	} else if (strcmp (key, GPM_CONF_PANEL_DIM_BRIGHTNESS) == 0) {

		gpm_conf_get_int (brightness->priv->conf, GPM_CONF_PANEL_DIM_BRIGHTNESS, &value);
		gpm_brightness_lcd_set_dim (brightness->priv->brightness, value);
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
			GpmSrvBrightnessLcd *brightness)
{
	gboolean do_laptop_lcd;
	guint value;

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_uint (brightness->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
	} else {
		gpm_conf_get_uint (brightness->priv->conf, GPM_CONF_BATTERY_BRIGHTNESS, &value);
	}

	/* only do brightness changes if we have the hardware */
	gpm_conf_get_bool (brightness->priv->conf, GPM_CONF_DISPLAY_STATE_CHANGE, &do_laptop_lcd);
	if (do_laptop_lcd) {
		gpm_brightness_lcd_set_std (brightness->priv->brightness, value);
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
		   GpmSrvBrightnessLcd *brightness)
{
	gpm_debug ("Button press event type=%s", type);

	if (strcmp (type, GPM_BUTTON_BRIGHT_UP) == 0) {

		gpm_brightness_lcd_up (brightness->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_BRIGHT_DOWN) == 0) {

		gpm_brightness_lcd_down (brightness->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {

		/* make sure we undim when we lift the lid */
		gpm_brightness_lcd_undim (brightness->priv->brightness);
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
idle_changed_cb (GpmIdle          *idle,
		 GpmIdleMode       mode,
		 GpmSrvBrightnessLcd *brightness)
{
	gboolean laptop_do_dim;

	gpm_conf_get_bool (brightness->priv->conf, GPM_CONF_DISPLAY_IDLE_DIM, &laptop_do_dim);

	/* should we ignore this? */
	if (laptop_do_dim == FALSE) {
		return;
	}

	/* don't dim or undim the screen when the lid is closed */
	if (button_is_lid_closed (brightness->priv->button) == TRUE) {
		return;
	}

	if (mode == GPM_IDLE_MODE_NORMAL) {

		gpm_brightness_lcd_undim (brightness->priv->brightness);

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		/* Dim the screen, fixes #328564 */
		gpm_brightness_lcd_dim (brightness->priv->brightness);
	}
}

/**
 * gpm_srv_brightness_lcd_constructor:
 **/
static GObject *
gpm_srv_brightness_lcd_constructor (GType		   type,
			        guint		   n_construct_properties,
			        GObjectConstructParam *construct_properties)
{
	GpmSrvBrightnessLcd      *brightness;
	GpmSrvBrightnessLcdClass *klass;
	klass = GPM_SRV_BRIGHTNESS_LCD_CLASS (g_type_class_peek (GPM_TYPE_SRV_BRIGHTNESS_LCD));
	brightness = GPM_SRV_BRIGHTNESS_LCD (G_OBJECT_CLASS (gpm_srv_brightness_lcd_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (brightness);
}

/**
 * gpm_srv_brightness_lcd_finalize:
 **/
static void
gpm_srv_brightness_lcd_finalize (GObject *object)
{
	GpmSrvBrightnessLcd *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_BRIGHTNESS_LCD (object));
	brightness = GPM_SRV_BRIGHTNESS_LCD (object);

	if (brightness->priv->feedback != NULL) {
		g_object_unref (brightness->priv->feedback);
	}
	if (brightness->priv->conf != NULL) {
		g_object_unref (brightness->priv->conf);
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
	object_class->constructor  = gpm_srv_brightness_lcd_constructor;

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
gpm_srv_brightness_lcd_init (GpmSrvBrightnessLcd *brightness)
{
	gint value;

	brightness->priv = GPM_SRV_BRIGHTNESS_LCD_GET_PRIVATE (brightness);

	brightness->priv->conf = gpm_conf_new ();
	g_signal_connect (brightness->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), brightness);

	/* set the default dim */
	gpm_conf_get_int (brightness->priv->conf, GPM_CONF_PANEL_DIM_BRIGHTNESS, &value);
	gpm_brightness_lcd_set_dim (brightness->priv->brightness, value);

	brightness->priv->feedback = gpm_feedback_new ();
	gpm_feedback_set_icon_name (brightness->priv->feedback,
				    GPM_STOCK_BRIGHTNESS_LCD);

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
