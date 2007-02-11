/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
#include "gpm-srv-backlight.h"
#include "gpm-brightness-lcd.h"
#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-feedback-widget.h"
#include "gpm-dpms.h"
#include "gpm-hal.h"
#include "gpm-idle.h"
#include "gpm-light-sensor.h"
#include "gpm-marshal.h"
#include "gpm-proxy.h"
#include "gpm-stock-icons.h"

#define DIM_INTERVAL		10 /* ms */

#define GPM_SRV_BACKLIGHT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SRV_BACKLIGHT, GpmSrvBacklightPrivate))

struct GpmSrvBacklightPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmBrightnessLcd	*brightness;
	GpmButton		*button;
	GpmConf			*conf;
	GpmFeedback		*feedback;
	GpmControl		*control;
	GpmDpms			*dpms;
	GpmHal			*hal;
	GpmIdle			*idle;
	gboolean		 can_dim;
	gboolean		 can_dpms;
};

enum {
	MODE_CHANGED,
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmSrvBacklight, gpm_srv_backlight, G_TYPE_OBJECT)

/**
 * gpm_srv_backlight_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_srv_backlight_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("gpm_srv_backlight_error");
	}
	return quark;
}

/**
 * gpm_srv_backlight_sync_policy:
 * @srv_backlight: This class instance
 *
 * Sync the SRV_BACKLIGHT policy with what we have set in gconf.
 **/
static void
gpm_srv_backlight_sync_policy (GpmSrvBacklight *srv_backlight)
{
	GError  *error;
	gboolean res;
	guint    timeout = 0;
	guint    standby = 0;
	guint    suspend = 0;
	guint    off = 0;
	gchar   *dpms_method;
	GpmDpmsMethod method;
	GpmAcAdapterState state;

	/* no point processing if we can't do the dpms action */
	if (srv_backlight->priv->can_dpms == FALSE) {
		return;
	}

	/* get the ac state */
	gpm_ac_adapter_get_state (srv_backlight->priv->ac_adapter, &state);

	error = NULL;

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_uint (srv_backlight->priv->conf, GPM_CONF_AC_SLEEP_DISPLAY, &timeout);
		gpm_conf_get_string (srv_backlight->priv->conf, GPM_CONF_AC_DPMS_METHOD, &dpms_method);
	} else {
		gpm_conf_get_uint (srv_backlight->priv->conf, GPM_CONF_BATTERY_SLEEP_DISPLAY, &timeout);
		gpm_conf_get_string (srv_backlight->priv->conf, GPM_CONF_BATTERY_DPMS_METHOD, &dpms_method);
	}

	/* convert the string types to standard types */
	method = gpm_dpms_method_from_string (dpms_method);
	g_free (dpms_method);

	/* check if method is valid */
	if (method == GPM_DPMS_METHOD_UNKNOWN) {
		gpm_warning ("SRV_BACKLIGHT method unknown. Possible schema problem!");
		return;
	}

	/* choose a sensible default */
	if (method == GPM_DPMS_METHOD_DEFAULT) {
		gpm_debug ("choosing sensible default");
		if (gpm_hal_is_laptop (srv_backlight->priv->hal)) {
			gpm_debug ("laptop, so use GPM_DPMS_METHOD_OFF");
			method = GPM_DPMS_METHOD_OFF;
		} else {
			gpm_debug ("not laptop, so use GPM_SRV_BACKLIGHT_METHOD_STAGGER");
			method = GPM_DPMS_METHOD_STAGGER;
		}
	}

	/* Some monitors do not support certain suspend states, so we have to
	 * provide a way to only use the one that works. */
	if (method == GPM_DPMS_METHOD_STAGGER) {
		/* suspend after one timeout, turn off after another */
		standby = timeout;
		suspend = timeout;
		off     = timeout * 2;
	} else if (method == GPM_DPMS_METHOD_STANDBY) {
		standby = timeout;
		suspend = 0;
		off     = 0;
	} else if (method == GPM_DPMS_METHOD_SUSPEND) {
		standby = 0;
		suspend = timeout;
		off     = 0;
	} else if (method == GPM_DPMS_METHOD_OFF) {
		standby = 0;
		suspend = 0;
		off     = timeout;
	} else {
		/* wtf? */
		gpm_warning ("unknown srv_backlight mode!");
	}

	gpm_debug ("SRV_BACKLIGHT parameters %d %d %d, method '%i'", standby, suspend, off, method);

	error = NULL;
	res = gpm_dpms_set_enabled (srv_backlight->priv->dpms, TRUE, &error);
	if (error) {
		gpm_warning ("Unable to enable SRV_BACKLIGHT: %s", error->message);
		g_error_free (error);
		return;
	}

	error = NULL;
	res = gpm_dpms_set_timeouts (srv_backlight->priv->dpms, standby, suspend, off, &error);
	if (error) {
		gpm_warning ("Unable to get SRV_BACKLIGHT timeouts: %s", error->message);
		g_error_free (error);
		return;
	}
}

/* dbus methods shouldn't use enumerated types, but should use textual descriptors */
gboolean
gpm_backlight_set_mode (GpmSrvBacklight *srv_backlight,
			const gchar     *mode_str,
			GError          **error)
{
	gboolean ret;
	GpmDpmsMode mode;

	g_return_val_if_fail (GPM_IS_SRV_BACKLIGHT (srv_backlight), FALSE);

	/* check if we have the hw */
	if (srv_backlight->priv->can_dpms == FALSE) {
		*error = g_error_new (gpm_srv_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "DPMS capable hardware not present");
		return FALSE;
	}

	/* convert mode to an enumerated type */
	mode = gpm_dpms_mode_from_string (mode_str);

	ret = gpm_dpms_set_mode_enum (srv_backlight->priv->dpms, mode, error);
	return ret;
}

/* dbus methods shouldn't use enumerated types, but should use textual descriptors */
gboolean
gpm_backlight_get_mode (GpmSrvBacklight *srv_backlight,
			const gchar    **mode_str,
			GError         **error)
{
	gboolean ret;
	GpmDpmsMode mode;

	g_return_val_if_fail (GPM_IS_SRV_BACKLIGHT (srv_backlight), FALSE);
	g_return_val_if_fail (mode_str != NULL, FALSE);

	/* check if we have the hw */
	if (srv_backlight->priv->can_dpms == FALSE) {
		*error = g_error_new (gpm_srv_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "DPMS capable hardware not present");
		return FALSE;
	}

	ret = gpm_dpms_get_mode_enum (srv_backlight->priv->dpms, &mode, error);
	if (ret == TRUE) {
		*mode_str = g_strdup (gpm_dpms_mode_to_string (mode));
	}
	return ret;
}

/**
 * gpm_backlight_get_brightness:
 **/
gboolean
gpm_backlight_get_brightness (GpmSrvBacklight	*srv_backlight,
			      guint		*brightness,
			      GError		**error)
{
	guint level;
	gboolean ret;
	g_return_val_if_fail (srv_backlight != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BACKLIGHT (srv_backlight), FALSE);
	g_return_val_if_fail (brightness != NULL, FALSE);

	/* check if we have the hw */
	if (srv_backlight->priv->can_dim == FALSE) {
		*error = g_error_new (gpm_srv_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "Dim capable hardware not present");
		return FALSE;
	}

	/* gets the current brightness */
	ret = gpm_brightness_lcd_get (srv_backlight->priv->brightness, &level);
	if (ret == TRUE) {
		*brightness = level;
	} else {
		*error = g_error_new (gpm_srv_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_DATA_NOT_AVAILABLE,
				      "Data not available");
	}
	return ret;
}

/**
 * gpm_backlight_set_brightness:
 **/
gboolean gpm_backlight_set_brightness (GpmSrvBacklight	*srv_backlight,
				       guint		 brightness,
				       GError		**error)
{
	gboolean ret;
	g_return_val_if_fail (srv_backlight != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_SRV_BACKLIGHT (srv_backlight), FALSE);

	/* check if we have the hw */
	if (srv_backlight->priv->can_dim == FALSE) {
		*error = g_error_new (gpm_srv_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "Dim capable hardware not present");
		return FALSE;
	}

	/* sets the current policy brightness */
	ret = gpm_brightness_lcd_set_std (srv_backlight->priv->brightness, brightness);
	if (ret == FALSE) {
		*error = g_error_new (gpm_srv_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_GENERAL,
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
		     GpmSrvBacklight *srv_backlight)
{
	gint value;
	GpmAcAdapterState state;

	gpm_ac_adapter_get_state (srv_backlight->priv->ac_adapter, &state);

	if (strcmp (key, GPM_CONF_AC_BRIGHTNESS) == 0) {

		if (srv_backlight->priv->can_dim == TRUE) {
			gpm_conf_get_int (srv_backlight->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
			if (state == GPM_AC_ADAPTER_PRESENT) {
				gpm_brightness_lcd_set_std (srv_backlight->priv->brightness, value);
			}
		}

	} else if (strcmp (key, GPM_CONF_BATTERY_BRIGHTNESS) == 0) {

		if (srv_backlight->priv->can_dim == TRUE) {
			gpm_conf_get_int (srv_backlight->priv->conf, GPM_CONF_BATTERY_BRIGHTNESS, &value);
			if (state == GPM_AC_ADAPTER_MISSING) {
				gpm_brightness_lcd_set_std (srv_backlight->priv->brightness, value);
			}
		}

	} else if (strcmp (key, GPM_CONF_PANEL_DIM_BRIGHTNESS) == 0) {

		if (srv_backlight->priv->can_dim == TRUE) {
			gpm_conf_get_int (srv_backlight->priv->conf, GPM_CONF_PANEL_DIM_BRIGHTNESS, &value);
			gpm_brightness_lcd_set_dim (srv_backlight->priv->brightness, value);
		}
	}

	if (strcmp (key, GPM_CONF_BATTERY_SLEEP_DISPLAY) == 0 ||
	    strcmp (key, GPM_CONF_AC_SLEEP_DISPLAY) == 0 ||
	    strcmp (key, GPM_CONF_AC_DPMS_METHOD) == 0 ||
	    strcmp (key, GPM_CONF_BATTERY_DPMS_METHOD) == 0) {
		gpm_srv_backlight_sync_policy (srv_backlight);
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
			GpmSrvBacklight *srv_backlight)
{
	gboolean do_laptop_lcd;
	guint value;

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_uint (srv_backlight->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
	} else {
		gpm_conf_get_uint (srv_backlight->priv->conf, GPM_CONF_BATTERY_BRIGHTNESS, &value);
	}

	/* only do brightness changes if we have the hardware */
	gpm_conf_get_bool (srv_backlight->priv->conf, GPM_CONF_DISPLAY_STATE_CHANGE, &do_laptop_lcd);
	if (do_laptop_lcd) {
		gpm_brightness_lcd_set_std (srv_backlight->priv->brightness, value);
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
		   GpmSrvBacklight *srv_backlight)
{
	gpm_debug ("Button press event type=%s", type);

	if (strcmp (type, GPM_BUTTON_BRIGHT_UP) == 0) {

		gpm_brightness_lcd_up (srv_backlight->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_BRIGHT_DOWN) == 0) {

		gpm_brightness_lcd_down (srv_backlight->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {

		/* make sure we undim when we lift the lid */
		gpm_brightness_lcd_undim (srv_backlight->priv->brightness);
		gpm_srv_backlight_sync_policy (srv_backlight);	
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
idle_changed_cb (GpmIdle         *idle,
		 GpmIdleMode      mode,
		 GpmSrvBacklight *srv_backlight)
{
	GpmAcAdapterState state;
	gboolean laptop_do_dim;
	GError *error;

	gpm_ac_adapter_get_state (srv_backlight->priv->ac_adapter, &state);
	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_bool (srv_backlight->priv->conf, GPM_CONF_AC_IDLE_DIM, &laptop_do_dim);
	} else {
		gpm_conf_get_bool (srv_backlight->priv->conf, GPM_CONF_BATTERY_IDLE_DIM, &laptop_do_dim);
	}

	/* don't dim or undim the screen when the lid is closed */
	if (gpm_button_is_lid_closed (srv_backlight->priv->button) == TRUE) {
		return;
	}

	if (mode == GPM_IDLE_MODE_NORMAL) {

		/* deactivate display power management */
		if (srv_backlight->priv->can_dpms == TRUE) {
			error = NULL;
			gpm_dpms_set_active (srv_backlight->priv->dpms, FALSE, &error);
			if (error) {
				gpm_debug ("Unable to set DPMS not active: %s", error->message);
				g_error_free (error);
			}
		}

		/* sync timeouts */
		gpm_srv_backlight_sync_policy (srv_backlight);

		if (laptop_do_dim == TRUE && srv_backlight->priv->can_dim == TRUE) {
			gpm_brightness_lcd_undim (srv_backlight->priv->brightness);
		}

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		/* activate display power management */
		if (srv_backlight->priv->can_dpms == TRUE) {
			error = NULL;
			gpm_dpms_set_active (srv_backlight->priv->dpms, TRUE, &error);
			if (error) {
				gpm_debug ("Unable to set DPMS active: %s", error->message);
				g_error_free (error);
			}
		}

		/* sync timeouts */
		gpm_srv_backlight_sync_policy (srv_backlight);

		/* Dim the screen, fixes #328564 */
		if (laptop_do_dim == TRUE && srv_backlight->priv->can_dim == TRUE) {
			gpm_brightness_lcd_dim (srv_backlight->priv->brightness);
		}
	}
}

/**
 * mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @manager: This class instance
 *
 * What happens when the DPMS mode is changed.
 **/
static void
mode_changed_cb (GpmDpms    *dpms,
		 GpmDpmsMode mode,
		 GpmSrvBacklight *srv_backlight)
{
	gpm_debug ("emitting mode-changed : %s", gpm_dpms_mode_to_string (mode));
	g_signal_emit (srv_backlight, signals [MODE_CHANGED], 0, gpm_dpms_mode_to_string (mode));
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
		       GpmSrvBacklight *srv_backlight)
{
	gpm_debug ("Need to display backlight feedback value %i", percentage);
	gpm_feedback_display_value (srv_backlight->priv->feedback, (float) percentage / 100.0f);

	/* we emit a signal for the brightness applet */
	gpm_debug ("emitting brightness-changed : %i", percentage);
	g_signal_emit (srv_backlight, signals [BRIGHTNESS_CHANGED], 0, percentage);
}

/**
 * control_resume_cb:
 * @control: The control class instance
 * @power: This power class instance
 *
 * We have to update the caches on resume
 **/
static void
control_resume_cb (GpmControl *control,
		   GpmControlAction action,
		   GpmSrvBacklight *srv_backlight)
{
	gpm_srv_backlight_sync_policy (srv_backlight);
}

/**
 * gpm_srv_backlight_finalize:
 **/
static void
gpm_srv_backlight_finalize (GObject *object)
{
	GpmSrvBacklight *srv_backlight;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SRV_BACKLIGHT (object));
	srv_backlight = GPM_SRV_BACKLIGHT (object);

	if (srv_backlight->priv->feedback != NULL) {
		g_object_unref (srv_backlight->priv->feedback);
	}
	if (srv_backlight->priv->dpms != NULL) {
		g_object_unref (srv_backlight->priv->dpms);
	}
	if (srv_backlight->priv->control != NULL) {
		g_object_unref (srv_backlight->priv->control);
	}
	if (srv_backlight->priv->conf != NULL) {
		g_object_unref (srv_backlight->priv->conf);
	}
	if (srv_backlight->priv->ac_adapter != NULL) {
		g_object_unref (srv_backlight->priv->ac_adapter);
	}
	if (srv_backlight->priv->hal != NULL) {
		g_object_unref (srv_backlight->priv->hal);
	}
	if (srv_backlight->priv->button != NULL) {
		g_object_unref (srv_backlight->priv->button);
	}
	if (srv_backlight->priv->idle != NULL) {
		g_object_unref (srv_backlight->priv->idle);
	}
	if (srv_backlight->priv->brightness != NULL) {
		g_object_unref (srv_backlight->priv->brightness);
	}

	g_return_if_fail (srv_backlight->priv != NULL);
	G_OBJECT_CLASS (gpm_srv_backlight_parent_class)->finalize (object);
}

/**
 * gpm_srv_backlight_class_init:
 **/
static void
gpm_srv_backlight_class_init (GpmSrvBacklightClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_srv_backlight_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSrvBacklightClass, brightness_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [MODE_CHANGED] =
		g_signal_new ("mode-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSrvBacklightClass, mode_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GpmSrvBacklightPrivate));
}

/**
 * gpm_srv_backlight_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect laptop_panel objects
 * to *NOT* be removed or added during the session.
 * We only control the first laptop_panel object if there are more than one.
 **/
static void
gpm_srv_backlight_init (GpmSrvBacklight *srv_backlight)
{
	GpmAcAdapterState state;
	guint value;

	srv_backlight->priv = GPM_SRV_BACKLIGHT_GET_PRIVATE (srv_backlight);

	/* gets caps */
	srv_backlight->priv->can_dim = gpm_brightness_lcd_has_hw ();
	srv_backlight->priv->can_dpms = gpm_dpms_has_hw ();

	/* we use hal to see if we are a laptop */
	srv_backlight->priv->hal = gpm_hal_new ();

	/* watch for dim value changes */
	srv_backlight->priv->conf = gpm_conf_new ();
	g_signal_connect (srv_backlight->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), srv_backlight);

	/* watch for brightness up and down buttons and also check lid state */
	srv_backlight->priv->button = gpm_button_new ();
	g_signal_connect (srv_backlight->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), srv_backlight);

	/* we use ac_adapter for the ac-adapter-changed signal */
	srv_backlight->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (srv_backlight->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), srv_backlight);

	if (srv_backlight->priv->can_dim == TRUE) {
		/* watch for manual brightness changes (for the feedback widget) */
		srv_backlight->priv->brightness = gpm_brightness_lcd_new ();
		g_signal_connect (srv_backlight->priv->brightness, "brightness-changed",
				  G_CALLBACK (brightness_changed_cb), srv_backlight);

		/* set the standard setting */
		gpm_ac_adapter_get_state (srv_backlight->priv->ac_adapter, &state);
		if (state == GPM_AC_ADAPTER_PRESENT) {
			gpm_conf_get_uint (srv_backlight->priv->conf, GPM_CONF_AC_BRIGHTNESS, &value);
		} else {
			gpm_conf_get_uint (srv_backlight->priv->conf, GPM_CONF_BATTERY_BRIGHTNESS, &value);
		}
		gpm_brightness_lcd_set_std (srv_backlight->priv->brightness, value);

		/* set the default dim */
		gpm_conf_get_uint (srv_backlight->priv->conf, GPM_CONF_PANEL_DIM_BRIGHTNESS, &value);
		gpm_brightness_lcd_set_dim (srv_backlight->priv->brightness, value);

		/* use a visual widget */
		srv_backlight->priv->feedback = gpm_feedback_new ();
		gpm_feedback_set_icon_name (srv_backlight->priv->feedback,
					    GPM_STOCK_BRIGHTNESS_LCD);
	}

	if (srv_backlight->priv->can_dpms == TRUE) {
		/* DPMS mode poll class */
		srv_backlight->priv->dpms = gpm_dpms_new ();
		g_signal_connect (srv_backlight->priv->dpms, "mode-changed",
				  G_CALLBACK (mode_changed_cb), srv_backlight);

		/* we refresh DPMS on resume */
		srv_backlight->priv->control = gpm_control_new ();
		g_signal_connect (srv_backlight->priv->control, "resume",
				  G_CALLBACK (control_resume_cb), srv_backlight);
	}

	/* watch for idle mode changes */
	srv_backlight->priv->idle = gpm_idle_new ();
	g_signal_connect (srv_backlight->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), srv_backlight);
}

/**
 * gpm_srv_backlight_new:
 * Return value: A new brightness class instance.
 **/
GpmSrvBacklight *
gpm_srv_backlight_new (void)
{
	GpmSrvBacklight *srv_backlight = g_object_new (GPM_TYPE_SRV_BACKLIGHT, NULL);
	return srv_backlight;
}
