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
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <libhal-gmanager.h>

#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-backlight.h"
#include "gpm-brightness.h"
#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-feedback-widget.h"
#include "gpm-dpms.h"
#include "gpm-idle.h"
#include "gpm-light-sensor.h"
#include "gpm-marshal.h"
#include "gpm-stock-icons.h"
#include "gpm-prefs-server.h"

#define DIM_INTERVAL		10 /* ms */

#define GPM_BACKLIGHT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BACKLIGHT, GpmBacklightPrivate))

struct GpmBacklightPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmBrightness		*brightness;
	GpmButton		*button;
	GpmConf			*conf;
	GpmFeedback		*feedback;
	GpmControl		*control;
	GpmDpms			*dpms;
	GpmIdle			*idle;
	GpmLightSensor		*light_sensor;
	gboolean		 can_dim;
	gboolean		 can_sense;
	gboolean		 can_dpms;
	gboolean		 is_laptop;
	gboolean		 system_is_idle;
	GTimer			*idle_timer;
	gfloat			 ambient_sensor_value;
	guint			 idle_dim_timeout;
	guint			 master_percentage;
};

enum {
	MODE_CHANGED,
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmBacklight, gpm_backlight, G_TYPE_OBJECT)

/**
 * gpm_backlight_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_backlight_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("gpm_backlight_error");
	}
	return quark;
}

/**
 * gpm_backlight_sync_policy:
 * @backlight: This class instance
 *
 * Sync the BACKLIGHT policy with what we have set in gconf.
 **/
static void
gpm_backlight_sync_policy (GpmBacklight *backlight)
{
	GError  *error;
	gboolean res;
	guint    timeout = 0;
	guint    standby = 0;
	guint    suspend = 0;
	guint    off = 0;
	gchar   *dpms_method;
	GpmDpmsMethod method;
	gboolean on_ac;

	/* no point processing if we can't do the dpms action */
	if (backlight->priv->can_dpms == FALSE) {
		return;
	}

	/* get the ac state */
	on_ac = gpm_ac_adapter_is_present (backlight->priv->ac_adapter);

	error = NULL;

	if (on_ac) {
		gpm_conf_get_uint (backlight->priv->conf, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_AC, &timeout);
		gpm_conf_get_string (backlight->priv->conf, GPM_CONF_BACKLIGHT_DPMS_METHOD_AC, &dpms_method);
	} else {
		gpm_conf_get_uint (backlight->priv->conf, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_BATT, &timeout);
		gpm_conf_get_string (backlight->priv->conf, GPM_CONF_BACKLIGHT_DPMS_METHOD_BATT, &dpms_method);
	}

	/* convert the string types to standard types */
	method = gpm_dpms_method_from_string (dpms_method);
	g_free (dpms_method);

	/* check if method is valid */
	if (method == GPM_DPMS_METHOD_UNKNOWN) {
		egg_warning ("BACKLIGHT method unknown. Possible schema problem!");
		return;
	}

	/* choose a sensible default */
	if (method == GPM_DPMS_METHOD_DEFAULT) {
		egg_debug ("choosing sensible default");
		if (backlight->priv->is_laptop) {
			egg_debug ("laptop, so use GPM_DPMS_METHOD_OFF");
			method = GPM_DPMS_METHOD_OFF;
		} else {
			egg_debug ("not laptop, so use GPM_BACKLIGHT_METHOD_STAGGER");
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
		egg_warning ("unknown backlight mode!");
	}

	egg_debug ("BACKLIGHT parameters %d %d %d, method '%i'", standby, suspend, off, method);

	error = NULL;
	res = gpm_dpms_set_enabled (backlight->priv->dpms, TRUE, &error);
	if (error) {
		egg_warning ("Unable to enable BACKLIGHT: %s", error->message);
		g_error_free (error);
		return;
	}

	error = NULL;
	res = gpm_dpms_set_timeouts (backlight->priv->dpms, standby, suspend, off, &error);
	if (error) {
		egg_warning ("Unable to get BACKLIGHT timeouts: %s", error->message);
		g_error_free (error);
		return;
	}
}

/* dbus methods shouldn't use enumerated types, but should use textual descriptors */
gboolean
gpm_backlight_set_mode (GpmBacklight *backlight,
			const gchar  *mode_str,
			GError      **error)
{
	gboolean ret;
	GpmDpmsMode mode;

	g_return_val_if_fail (GPM_IS_BACKLIGHT (backlight), FALSE);

	/* check if we have the hw */
	if (backlight->priv->can_dpms == FALSE) {
		*error = g_error_new (gpm_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "DPMS capable hardware not present");
		return FALSE;
	}

	/* convert mode to an enumerated type */
	mode = gpm_dpms_mode_from_string (mode_str);

	ret = gpm_dpms_set_mode_enum (backlight->priv->dpms, mode, error);
	return ret;
}

/* dbus methods shouldn't use enumerated types, but should use textual descriptors */
gboolean
gpm_backlight_get_mode (GpmBacklight *backlight,
			const gchar **mode_str,
			GError      **error)
{
	gboolean ret;
	GpmDpmsMode mode;

	g_return_val_if_fail (GPM_IS_BACKLIGHT (backlight), FALSE);
	g_return_val_if_fail (mode_str != NULL, FALSE);

	/* check if we have the hw */
	if (backlight->priv->can_dpms == FALSE) {
		*error = g_error_new (gpm_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "DPMS capable hardware not present");
		return FALSE;
	}

	ret = gpm_dpms_get_mode_enum (backlight->priv->dpms, &mode, error);
	if (ret) {
		*mode_str = g_strdup (gpm_dpms_mode_to_string (mode));
	}
	return ret;
}

/**
 * gpm_backlight_get_brightness:
 **/
gboolean
gpm_backlight_get_brightness (GpmBacklight *backlight, guint *brightness, GError **error)
{
	guint level;
	gboolean ret;
	g_return_val_if_fail (backlight != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BACKLIGHT (backlight), FALSE);
	g_return_val_if_fail (brightness != NULL, FALSE);

	/* check if we have the hw */
	if (backlight->priv->can_dim == FALSE) {
		*error = g_error_new (gpm_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "Dim capable hardware not present");
		return FALSE;
	}

	/* gets the current brightness */
	ret = gpm_brightness_get (backlight->priv->brightness, &level);
	if (ret) {
		*brightness = level;
	} else {
		*error = g_error_new (gpm_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_DATA_NOT_AVAILABLE,
				      "Data not available");
	}
	return ret;
}

/**
 * gpm_backlight_set_brightness:
 **/
gboolean
gpm_backlight_set_brightness (GpmBacklight *backlight, guint percentage, GError **error)
{
	gboolean ret;
	gboolean hw_changed;

	g_return_val_if_fail (backlight != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BACKLIGHT (backlight), FALSE);

	/* check if we have the hw */
	if (backlight->priv->can_dim == FALSE) {
		*error = g_error_new (gpm_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
				      "Dim capable hardware not present");
		return FALSE;
	}

	/* just set the master percentage for now, don't try to be clever */
	backlight->priv->master_percentage = percentage;

	/* sets the current policy brightness */
	ret = gpm_brightness_set (backlight->priv->brightness, percentage, &hw_changed);
	if (!ret) {
		*error = g_error_new (gpm_backlight_error_quark (),
				      GPM_BACKLIGHT_ERROR_GENERAL,
				      "Cannot set policy brightness");
	}
	/* we emit a signal for the brightness applet */
	if (ret && hw_changed) {
		egg_debug ("emitting brightness-changed : %i", percentage);
		g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, percentage);
	}
	return ret;
}

/**
 * gpm_common_sum_scale:
 *
 * Finds the average between value1 and value2 set on a scale factor
 **/
inline static gfloat
gpm_common_sum_scale (gfloat value1, gfloat value2, gfloat factor)
{
	gfloat diff;
	diff = value1 - value2;
	return value2 + (diff * factor);
}

/**
 * gpm_backlight_brightness_evaluate_and_set:
 **/
static gboolean
gpm_backlight_brightness_evaluate_and_set (GpmBacklight *backlight, gboolean interactive)
{
	gfloat brightness;
	gfloat scale;
	gboolean ret;
	gboolean on_ac;
	gboolean do_laptop_lcd;
	gboolean enable_action;
	gboolean battery_reduce;
	gboolean hw_changed;
	guint value;
	guint old_value;

	if (backlight->priv->can_dim == FALSE) {
		egg_warning ("no dimming hardware");
		return FALSE;
	}

	gpm_conf_get_bool (backlight->priv->conf, GPM_CONF_BACKLIGHT_ENABLE, &do_laptop_lcd);
	if (do_laptop_lcd == FALSE) {
		egg_warning ("policy is no dimming");
		return FALSE;
	}

	/* get the last set brightness */
	brightness = backlight->priv->master_percentage / 100.0f;
	egg_debug ("1. main brightness %f", brightness);

	/* get AC status */
	on_ac = gpm_ac_adapter_is_present (backlight->priv->ac_adapter);

	/* reduce if on battery power if we should */
	gpm_conf_get_bool (backlight->priv->conf, GPM_CONF_BACKLIGHT_BATTERY_REDUCE, &battery_reduce);
	if (on_ac == FALSE && battery_reduce) {
		gpm_conf_get_uint (backlight->priv->conf, GPM_CONF_BACKLIGHT_BRIGHTNESS_DIM_BATT, &value);
		if (value > 100) {
			egg_warning ("cannot use battery brightness value %i, correcting to 50", value);
			value = 50;
		}
		scale = (100 - value) / 100.0f;
		brightness *= scale;
	} else {
		scale = 1.0f;
	}
	egg_debug ("2. battery scale %f, brightness %f", scale, brightness);

	/* reduce if system is momentarily idle */
	if (on_ac) {
		gpm_conf_get_bool (backlight->priv->conf, GPM_CONF_BACKLIGHT_IDLE_DIM_AC, &enable_action);
	} else {
		gpm_conf_get_bool (backlight->priv->conf, GPM_CONF_BACKLIGHT_IDLE_DIM_BATT, &enable_action);
	}
	if (enable_action && backlight->priv->system_is_idle == TRUE) {
		gpm_conf_get_uint (backlight->priv->conf, GPM_CONF_BACKLIGHT_IDLE_BRIGHTNESS, &value);
		if (value > 100) {
			egg_warning ("cannot use idle brightness value %i, correcting to 50", value);
			value = 50;
		}
		scale = value / 100.0f;
		brightness *= scale;
	} else {
		scale = 1.0f;
	}
	egg_debug ("3. idle scale %f, brightness %f", scale, brightness);

	/* reduce if ambient is low */
	gpm_conf_get_bool (backlight->priv->conf, GPM_CONF_AMBIENT_ENABLE, &enable_action);
	if (backlight->priv->can_sense && enable_action == TRUE) {
		gpm_conf_get_uint (backlight->priv->conf, GPM_CONF_AMBIENT_SCALE, &value);
		scale = backlight->priv->ambient_sensor_value * (value / 100.0f);
		gpm_conf_get_uint (backlight->priv->conf, GPM_CONF_AMBIENT_FACTOR, &value);
		scale = gpm_common_sum_scale (brightness, scale, value / 100.0f);
		if (scale > 1.0f) {
			scale = 1.0f;
		}
		if (scale < 0.80f) {
			brightness *= scale;
		} else {
			scale = 1.0f;
		}
	} else {
		scale = 1.0f;
	}
	egg_debug ("4. ambient scale %f, brightness %f", scale, brightness);

	/* convert to percentage */
	value = (guint) ((brightness * 100.0f) + 0.5);

	/* only do stuff if the brightness is different */
	gpm_brightness_get (backlight->priv->brightness, &old_value);
	if (old_value == value) {
		egg_debug ("values are the same, no action");
		return FALSE;
	}

	/* only show dialog if interactive */
	if (interactive) {
		gpm_feedback_display_value (backlight->priv->feedback, (float) brightness);
	}

	ret = gpm_brightness_set (backlight->priv->brightness, value, &hw_changed);
	/* we emit a signal for the brightness applet */
	if (ret && hw_changed) {
		egg_debug ("emitting brightness-changed : %i", value);
		g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, value);
	}
	return TRUE;
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf *conf, const gchar *key, GpmBacklight *backlight)
{
	gboolean on_ac;
	on_ac = gpm_ac_adapter_is_present (backlight->priv->ac_adapter);

	if (on_ac && strcmp (key, GPM_CONF_BACKLIGHT_BRIGHTNESS_AC) == 0) {
		gpm_conf_get_uint (backlight->priv->conf,
				   GPM_CONF_BACKLIGHT_BRIGHTNESS_AC,
				   &backlight->priv->master_percentage);
		gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);

	} else if (!on_ac && strcmp (key, GPM_CONF_BACKLIGHT_BRIGHTNESS_DIM_BATT) == 0) {
		gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);
	}

	else if (strcmp (key, GPM_CONF_BACKLIGHT_IDLE_DIM_AC) == 0 ||
	         strcmp (key, GPM_CONF_AMBIENT_ENABLE) == 0 ||
	         strcmp (key, GPM_CONF_AMBIENT_FACTOR) == 0 ||
	         strcmp (key, GPM_CONF_AMBIENT_SCALE) == 0 ||
	         strcmp (key, GPM_CONF_BACKLIGHT_ENABLE) == 0 ||
	         strcmp (key, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_BATT) == 0 ||
	         strcmp (key, GPM_CONF_BACKLIGHT_BATTERY_REDUCE) == 0 ||
	         strcmp (key, GPM_CONF_BACKLIGHT_IDLE_BRIGHTNESS) == 0) {
		gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);

	} else if (strcmp (key, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_BATT) == 0 ||
	           strcmp (key, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_AC) == 0 ||
	           strcmp (key, GPM_CONF_BACKLIGHT_DPMS_METHOD_AC) == 0 ||
	           strcmp (key, GPM_CONF_BACKLIGHT_DPMS_METHOD_BATT) == 0) {
		gpm_backlight_sync_policy (backlight);
	} else if (strcmp (key, GPM_CONF_BACKLIGHT_IDLE_DIM_TIME) == 0) {
		gpm_conf_get_uint (backlight->priv->conf,
				   GPM_CONF_BACKLIGHT_IDLE_DIM_TIME,
				   &backlight->priv->idle_dim_timeout);
	} else {
		egg_debug ("unknown key %s", key);
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
ac_adapter_changed_cb (GpmAcAdapter     *ac_adapter,
		       gboolean		 on_ac,
		       GpmBacklight     *backlight)
{
	gpm_backlight_brightness_evaluate_and_set (backlight, TRUE);
}

/**
 * gpm_backlight_button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @brightness: This class instance
 **/
static void
gpm_backlight_button_pressed_cb (GpmButton *button, const gchar *type, GpmBacklight *backlight)
{
	guint percentage;
	gboolean ret;
	gboolean hw_changed;
	egg_debug ("Button press event type=%s", type);

	if (strcmp (type, GPM_BUTTON_BRIGHT_UP) == 0) {
		/* go up one step */
		ret = gpm_brightness_up (backlight->priv->brightness, &hw_changed);

		/* show the new value */
		if (ret) {
			gpm_brightness_get (backlight->priv->brightness, &percentage);
			gpm_feedback_display_value (backlight->priv->feedback, (float) percentage/100.0f);
			/* save the new percentage */
			backlight->priv->master_percentage = percentage;
		}
		/* we emit a signal for the brightness applet */
		if (ret && hw_changed) {
			egg_debug ("emitting brightness-changed : %i", percentage);
			g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, percentage);
		}
	} else if (strcmp (type, GPM_BUTTON_BRIGHT_DOWN) == 0) {
		/* go up down step */
		ret = gpm_brightness_down (backlight->priv->brightness, &hw_changed);

		/* show the new value */
		if (ret) {
			gpm_brightness_get (backlight->priv->brightness, &percentage);
			gpm_feedback_display_value (backlight->priv->feedback, (float) percentage/100.0f);
			/* save the new percentage */
			backlight->priv->master_percentage = percentage;
		}
		/* we emit a signal for the brightness applet */
		if (ret && hw_changed) {
			egg_debug ("emitting brightness-changed : %i", percentage);
			g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, percentage);
		}
	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {
		/* make sure we undim when we lift the lid */
		gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);
		gpm_backlight_sync_policy (backlight);
	}
}

/**
 * gpm_backlight_notify_system_idle_changed:
 **/
static gboolean
gpm_backlight_notify_system_idle_changed (GpmBacklight *backlight, gboolean is_idle)
{
	gdouble elapsed;

	/* no point continuing */
	if (backlight->priv->system_is_idle == is_idle) {
		egg_debug ("state not changed");
		return FALSE;
	}

	/* get elapsed time and reset timer */
	elapsed = g_timer_elapsed (backlight->priv->idle_timer, NULL);
	g_timer_reset (backlight->priv->idle_timer);

	if (is_idle == FALSE) {
		egg_debug ("we have just been idle for %lfs", elapsed);

		/* The user immediatly undimmed the screen!
		 * We should double the timeout to avoid this happening again */
		if (elapsed < 10) {
			/* double the event time */
			backlight->priv->idle_dim_timeout *= 2.0;
			gpm_conf_set_uint (backlight->priv->conf,
					   GPM_CONF_GNOME_SS_PM_DELAY,
					   backlight->priv->idle_dim_timeout);
			egg_debug ("increasing idle dim time to %is",
				   backlight->priv->idle_dim_timeout);
		}

		/* We reset the dimming after 2 minutes of idle,
		 * as the user will have changed tasks */
		if (elapsed > 2*60) {
			/* reset back to our default dimming */
			gpm_conf_get_uint (backlight->priv->conf,
					   GPM_CONF_BACKLIGHT_IDLE_DIM_TIME,
					   &backlight->priv->idle_dim_timeout);
			gpm_conf_set_uint (backlight->priv->conf,
					   GPM_CONF_GNOME_SS_PM_DELAY,
					   backlight->priv->idle_dim_timeout);
			egg_debug ("resetting idle dim time to %is",
				   backlight->priv->idle_dim_timeout);
		}
	} else {
		egg_debug ("we were active for %lfs", elapsed);
	}

	egg_debug ("changing powersave idle status to %i", is_idle);
	backlight->priv->system_is_idle = is_idle;
	return TRUE;
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
idle_changed_cb (GpmIdle      *idle,
		 GpmIdleMode   mode,
		 GpmBacklight *backlight)
{
	GError *error;

	/* don't dim or undim the screen when the lid is closed */
	if (gpm_button_is_lid_closed (backlight->priv->button)) {
		return;
	}

	if (mode == GPM_IDLE_MODE_NORMAL) {
		/* deactivate display power management */
		if (backlight->priv->can_dpms) {
			error = NULL;
			gpm_dpms_set_active (backlight->priv->dpms, FALSE, &error);
			if (error) {
				egg_debug ("Unable to set DPMS not active: %s", error->message);
				g_error_free (error);
			}
		}

		/* sync lcd brightness */
		gpm_backlight_notify_system_idle_changed (backlight, FALSE);
		gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);

		/* sync timeouts */
		gpm_backlight_sync_policy (backlight);

	} else if (mode == GPM_IDLE_MODE_SESSION) {
		/* activate display power management */
		if (backlight->priv->can_dpms) {
			error = NULL;
			gpm_dpms_set_active (backlight->priv->dpms, TRUE, &error);
			if (error) {
				egg_debug ("Unable to set DPMS active: %s", error->message);
				g_error_free (error);
			}
		}

		/* sync lcd brightness */
		gpm_backlight_notify_system_idle_changed (backlight, FALSE);
		gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);

		/* sync timeouts */
		gpm_backlight_sync_policy (backlight);

	} else if (mode == GPM_IDLE_MODE_POWERSAVE) {

		/* sync lcd brightness */
		gpm_backlight_notify_system_idle_changed (backlight, TRUE);
		gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);
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
mode_changed_cb (GpmDpms      *dpms,
		 GpmDpmsMode   mode,
		 GpmBacklight *backlight)
{
	egg_debug ("emitting mode-changed : %s", gpm_dpms_mode_to_string (mode));
	g_signal_emit (backlight, signals [MODE_CHANGED], 0, gpm_dpms_mode_to_string (mode));
}

/**
 * brightness_changed_cb:
 * @brightness: The GpmBrightness class instance
 * @percentage: The new percentage brightness
 * @brightness: This class instance
 *
 * This callback is called when the brightness value changes.
 **/
static void
brightness_changed_cb (GpmBrightness *brightness, guint percentage, GpmBacklight *backlight)
{
	/* display the widget when something else changed the backlight */
	egg_debug ("Need to display backlight feedback value %i", percentage);
	gpm_feedback_display_value (backlight->priv->feedback, (float) percentage / 100.0f);

	/* save the new percentage */
	backlight->priv->master_percentage = percentage;

	/* we emit a signal for the brightness applet */
	egg_debug ("emitting brightness-changed : %i", percentage);
	g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, percentage);
}

/**
 * brightness_changed_cb:
 * @brightness: The GpmBrightness class instance
 * @percentage: The new percentage brightness
 * @brightness: This class instance
 *
 * This callback is called when the brightness value changes.
 **/
static void
sensor_changed_cb (GpmLightSensor *sensor,
		   guint           percentage,
		   GpmBacklight   *backlight)
{
	egg_debug ("sensor changed! %i", percentage);
	backlight->priv->ambient_sensor_value = percentage / 100.0f;
	gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);
}

/**
 * control_resume_cb:
 * @control: The control class instance
 * @power: This power class instance
 *
 * We have to update the caches on resume
 **/
static void
control_resume_cb (GpmControl      *control,
		   GpmControlAction action,
		   GpmBacklight    *backlight)
{
	gpm_backlight_sync_policy (backlight);
}

/**
 * gpm_backlight_finalize:
 **/
static void
gpm_backlight_finalize (GObject *object)
{
	GpmBacklight *backlight;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BACKLIGHT (object));
	backlight = GPM_BACKLIGHT (object);

	g_timer_destroy (backlight->priv->idle_timer);

	if (backlight->priv->light_sensor != NULL) {
		g_object_unref (backlight->priv->light_sensor);
	}
	if (backlight->priv->feedback != NULL) {
		g_object_unref (backlight->priv->feedback);
	}
	if (backlight->priv->dpms != NULL) {
		g_object_unref (backlight->priv->dpms);
	}
	if (backlight->priv->control != NULL) {
		g_object_unref (backlight->priv->control);
	}
	if (backlight->priv->conf != NULL) {
		g_object_unref (backlight->priv->conf);
	}
	if (backlight->priv->ac_adapter != NULL) {
		g_object_unref (backlight->priv->ac_adapter);
	}
	if (backlight->priv->button != NULL) {
		g_object_unref (backlight->priv->button);
	}
	if (backlight->priv->idle != NULL) {
		g_object_unref (backlight->priv->idle);
	}
	if (backlight->priv->brightness != NULL) {
		g_object_unref (backlight->priv->brightness);
	}

	g_return_if_fail (backlight->priv != NULL);
	G_OBJECT_CLASS (gpm_backlight_parent_class)->finalize (object);
}

/**
 * gpm_backlight_class_init:
 **/
static void
gpm_backlight_class_init (GpmBacklightClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_backlight_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBacklightClass, brightness_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [MODE_CHANGED] =
		g_signal_new ("mode-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBacklightClass, mode_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GpmBacklightPrivate));
}

/**
 * gpm_backlight_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect laptop_panel objects
 * to *NOT* be removed or added during the session.
 * We only control the first laptop_panel object if there are more than one.
 **/
static void
gpm_backlight_init (GpmBacklight *backlight)
{
	HalGManager *hal_manager;
	guint value;
	GpmPrefsServer *prefs_server;

	backlight->priv = GPM_BACKLIGHT_GET_PRIVATE (backlight);

	/* record our idle time */
	backlight->priv->idle_timer = g_timer_new ();

	/* this has a delay.. */
	backlight->priv->light_sensor = gpm_light_sensor_new ();
	g_signal_connect (backlight->priv->light_sensor, "sensor-changed",
			  G_CALLBACK (sensor_changed_cb), backlight);

	/* watch for manual brightness changes (for the feedback widget) */
	backlight->priv->brightness = gpm_brightness_new ();
	g_signal_connect (backlight->priv->brightness, "brightness-changed",
			  G_CALLBACK (brightness_changed_cb), backlight);

	/* gets caps */
	backlight->priv->can_dim = gpm_brightness_has_hw (backlight->priv->brightness);
	backlight->priv->can_dpms = gpm_dpms_has_hw ();
	backlight->priv->can_sense = gpm_light_sensor_has_hw (backlight->priv->light_sensor);

	/* we use hal to see if we are a laptop */
	hal_manager = hal_gmanager_new ();
	backlight->priv->is_laptop = hal_gmanager_is_laptop (hal_manager);
	g_object_unref (hal_manager);

	/* expose ui in prefs program */
	prefs_server = gpm_prefs_server_new ();
	if (backlight->priv->is_laptop) {
		gpm_prefs_server_set_capability (prefs_server, GPM_PREFS_SERVER_LID);
	}
	if (backlight->priv->can_dim) {
		gpm_prefs_server_set_capability (prefs_server, GPM_PREFS_SERVER_BACKLIGHT);
	}
	if (backlight->priv->can_sense) {
		gpm_prefs_server_set_capability (prefs_server, GPM_PREFS_SERVER_AMBIENT);
	}
	g_object_unref (prefs_server);

	/* watch for dim value changes */
	backlight->priv->conf = gpm_conf_new ();
	g_signal_connect (backlight->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), backlight);

	/* get and set the default idle dim timeout */
	gpm_conf_get_uint (backlight->priv->conf,
			   GPM_CONF_BACKLIGHT_IDLE_DIM_TIME,
			   &backlight->priv->idle_dim_timeout);
	gpm_conf_set_uint (backlight->priv->conf,
			   GPM_CONF_GNOME_SS_PM_DELAY,
			   backlight->priv->idle_dim_timeout);

	/* set the main brightness, this is designed to be updated if the user changes the
	 * brightness so we can undim to the 'correct' value */
	gpm_conf_get_uint (backlight->priv->conf,
			   GPM_CONF_BACKLIGHT_BRIGHTNESS_AC,
			   &backlight->priv->master_percentage);

	/* watch for brightness up and down buttons and also check lid state */
	backlight->priv->button = gpm_button_new ();
	g_signal_connect (backlight->priv->button, "button-pressed",
			  G_CALLBACK (gpm_backlight_button_pressed_cb), backlight);

	/* we use ac_adapter for the ac-adapter-changed signal */
	backlight->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (backlight->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), backlight);

	/* assumption */
	backlight->priv->system_is_idle = FALSE;
	gpm_conf_get_uint (backlight->priv->conf,
			   GPM_CONF_BACKLIGHT_IDLE_DIM_TIME,
			   &backlight->priv->idle_dim_timeout);
	gpm_conf_set_uint (backlight->priv->conf,
			   GPM_CONF_GNOME_SS_PM_DELAY,
			   backlight->priv->idle_dim_timeout);

	/* use a visual widget */
	backlight->priv->feedback = gpm_feedback_new ();
	gpm_feedback_set_icon_name (backlight->priv->feedback,
				    GPM_STOCK_BRIGHTNESS_LCD);

	if (backlight->priv->can_dpms) {
		/* DPMS mode poll class */
		backlight->priv->dpms = gpm_dpms_new ();
		g_signal_connect (backlight->priv->dpms, "mode-changed",
				  G_CALLBACK (mode_changed_cb), backlight);

		/* we refresh DPMS on resume */
		backlight->priv->control = gpm_control_new ();
		g_signal_connect (backlight->priv->control, "resume",
				  G_CALLBACK (control_resume_cb), backlight);
	}

	/* watch for idle mode changes */
	backlight->priv->idle = gpm_idle_new ();
	g_signal_connect (backlight->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), backlight);

	/* sync at startup */
	gpm_light_sensor_get_absolute (backlight->priv->light_sensor, &value);
	backlight->priv->ambient_sensor_value = value / 100.0f;
	gpm_backlight_brightness_evaluate_and_set (backlight, FALSE);
	gpm_backlight_sync_policy (backlight);
}

/**
 * gpm_backlight_new:
 * Return value: A new brightness class instance.
 **/
GpmBacklight *
gpm_backlight_new (void)
{
	GpmBacklight *backlight = g_object_new (GPM_TYPE_BACKLIGHT, NULL);
	return backlight;
}

