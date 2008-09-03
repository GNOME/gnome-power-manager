/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#include "config.h"

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

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#ifdef HAVE_DPMS_EXTENSION
#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsstr.h>
#endif

#include "gpm-conf.h"
#include "egg-debug.h"
#include "gpm-dpms.h"

static void     gpm_dpms_class_init (GpmDpmsClass *klass);
static void     gpm_dpms_init       (GpmDpms      *dpms);
static void     gpm_dpms_finalize   (GObject      *object);

#define GPM_DPMS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_DPMS, GpmDpmsPrivate))

/* until we get a nice event-emitting DPMS extension, we have to poll... */
#define GPM_DPMS_POLL_TIME	10*1000

struct GpmDpmsPrivate
{
	gboolean		 enabled;
	gboolean		 active;
	gboolean		 dpms_capable;

	guint			 standby_timeout;
	guint			 suspend_timeout;
	guint			 off_timeout;

	GpmConf			*conf;
	GpmDpmsMode		 mode;

	guint			 timer_id;
};

enum {
	MODE_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_STANDBY_TIMEOUT,
	PROP_SUSPEND_TIMEOUT,
	PROP_OFF_TIMEOUT
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_dpms_object = NULL;

G_DEFINE_TYPE (GpmDpms, gpm_dpms, G_TYPE_OBJECT)

GQuark
gpm_dpms_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gpm_dpms_error");

	return quark;
}

gboolean
gpm_dpms_has_hw (void)
{
#ifdef HAVE_DPMS_EXTENSION
	return TRUE;
#else
	return FALSE;
#endif
}

/* the following function is derived from
   xscreensaver Copyright (C) Jamie Zawinski
*/
static gboolean
x11_sync_server_dpms_settings (Display *dpy,
			       gboolean enabled,
			       guint	standby_secs,
			       guint	suspend_secs,
			       guint	off_secs,
			       GError **error)
{
#ifdef HAVE_DPMS_EXTENSION
	BOOL o_enabled = FALSE;
	CARD16 o_power = 0;
	CARD16 o_standby = 0;
	CARD16 o_suspend = 0;
	CARD16 o_off = 0;

	egg_debug ("Syncing DPMS settings enabled=%d timeouts=%d %d %d",
		   enabled, standby_secs, suspend_secs, off_secs);

	if (! DPMSInfo (dpy, &o_power, &o_enabled)) {
		egg_debug ("unable to get DPMS state.");
		g_set_error (error,
			     GPM_DPMS_ERROR,
			     GPM_DPMS_ERROR_GENERAL,
			     "Unable to get DPMS state");

		return FALSE;
	}

	if (o_enabled != enabled) {
		int res;

		if (enabled) {
			res = DPMSEnable (dpy);
		} else {
			res = DPMSDisable (dpy);
		}

		if (! res) {
			egg_debug ("unable to set DPMS state.");
			g_set_error (error,
				     GPM_DPMS_ERROR,
				     GPM_DPMS_ERROR_GENERAL,
				     "Unable to set DPMS state");

			return FALSE;
		} else {
			egg_debug ("turned DPMS %s", enabled ? "ON" : "OFF");
		}
	}

	if (! DPMSGetTimeouts (dpy, &o_standby, &o_suspend, &o_off)) {
		egg_debug ("unable to get DPMS timeouts.");
		g_set_error (error,
			     GPM_DPMS_ERROR,
			     GPM_DPMS_ERROR_GENERAL,
			     "Unable to get DPMS timeouts");

		return FALSE;
	}

	if (o_standby != standby_secs ||
	    o_suspend != suspend_secs ||
	    o_off != off_secs) {
		if (! DPMSSetTimeouts (dpy, standby_secs, suspend_secs, off_secs)) {
			egg_debug ("unable to set DPMS timeouts.");
			g_set_error (error,
				     GPM_DPMS_ERROR,
				     GPM_DPMS_ERROR_GENERAL,
				     "Unable to set DPMS timeouts");

			return FALSE;
		} else {
			egg_debug ("set DPMS timeouts: %d %d %d.",
				  standby_secs, suspend_secs, off_secs);
		}
	}

	return TRUE;
# else	/* !HAVE_DPMS_EXTENSION */

	egg_debug ("DPMS support not compiled in.");
	return FALSE;
# endif /* HAVE_DPMS_EXTENSION */
}

#ifdef HAVE_DPMS_EXTENSION

static gboolean
x11_get_mode (GpmDpms     *dpms,
	      GpmDpmsMode *mode,
	      GError     **error)
{
	GpmDpmsMode result;
	BOOL enabled = FALSE;
	CARD16 state;

	if (dpms->priv->dpms_capable == FALSE) {
		/* Server or monitor can't DPMS -- assume the monitor is on. */
		result = GPM_DPMS_MODE_ON;
	} else {
		DPMSInfo (GDK_DISPLAY (), &state, &enabled);
		if (! enabled) {
			/* Server says DPMS is disabled -- so the monitor is on. */
			result = GPM_DPMS_MODE_ON;
		} else {
			switch (state) {
			case DPMSModeOn:
				result = GPM_DPMS_MODE_ON;
				break;
			case DPMSModeStandby:
				result = GPM_DPMS_MODE_STANDBY;
				break;
			case DPMSModeSuspend:
				result = GPM_DPMS_MODE_SUSPEND;
				break;
			case DPMSModeOff:
				result = GPM_DPMS_MODE_OFF;
				break;
			default:
				result = GPM_DPMS_MODE_ON;
				break;
			}
		}
	}

	if (mode) {
		*mode = result;
	}

	return TRUE;
}

static gboolean
x11_set_mode (GpmDpms	 *dpms,
	      GpmDpmsMode mode,
	      GError    **error)
{
	GpmDpmsMode current_mode;
	CARD16 state;
	CARD16 current_state;
	BOOL current_enabled;

	if (dpms->priv->dpms_capable == FALSE) {
		egg_debug ("not DPMS capable");
		g_set_error (error,
			     GPM_DPMS_ERROR,
			     GPM_DPMS_ERROR_GENERAL,
			     "Display is not DPMS capable");
		return FALSE;
	}

	if (! DPMSInfo (GDK_DISPLAY (), &current_state, &current_enabled)) {
		egg_debug ("couldn't get DPMS info");
		g_set_error (error,
			     GPM_DPMS_ERROR,
			     GPM_DPMS_ERROR_GENERAL,
			     "Unable to get DPMS state");
		return FALSE;
	}

	if (! current_enabled) {
		egg_debug ("DPMS not enabled");
		g_set_error (error,
			     GPM_DPMS_ERROR,
			     GPM_DPMS_ERROR_GENERAL,
			     "DPMS is not enabled");
		return FALSE;
	}

	switch (mode) {
	case GPM_DPMS_MODE_ON:
		state = DPMSModeOn;
		break;
	case GPM_DPMS_MODE_STANDBY:
		state = DPMSModeStandby;
		break;
	case GPM_DPMS_MODE_SUSPEND:
		state = DPMSModeSuspend;
		break;
	case GPM_DPMS_MODE_OFF:
		state = DPMSModeOff;
		break;
	default:
		state = DPMSModeOn;
		break;
	}

	x11_get_mode (dpms, &current_mode, NULL);

	if (current_mode != mode) {
		if (! DPMSForceLevel (GDK_DISPLAY (), state)) {
			g_set_error (error,
				     GPM_DPMS_ERROR,
				     GPM_DPMS_ERROR_GENERAL,
				     "Could not change DPMS mode");
			return FALSE;
		}

		XSync (GDK_DISPLAY (), FALSE);
	}

	return TRUE;
}

#else  /* HAVE_DPMS_EXTENSION */

static gboolean
x11_get_mode (GpmDpms     *dpms,
	      GpmDpmsMode *mode,
	      GError     **error)
{
	if (mode) {
		*mode = GPM_DPMS_MODE_ON;
	}

	return TRUE;
}

static gboolean
x11_set_mode (GpmDpms	 *dpms,
	      GpmDpmsMode mode,
	      GError    **error)
{
	return FALSE;
}

#endif /* !HAVE_DPMS_EXTENSION */

static gboolean
sync_settings (GpmDpms *dpms,
	       GError **error)
{
	guint    standby;
	guint    suspend;
	guint    off;

	if (dpms->priv->active) {
		standby = dpms->priv->standby_timeout;
		suspend = dpms->priv->suspend_timeout;
		off     = dpms->priv->off_timeout;
	} else {
		standby = 0;
		suspend = 0;
		off     = 0;
	}

	if (dpms->priv->dpms_capable == FALSE) {
		egg_debug ("Display is not DPMS capable");
		g_set_error (error,
			     GPM_DPMS_ERROR,
			     GPM_DPMS_ERROR_GENERAL,
			     "Display is not DPMS capable");

		return FALSE;
	}

	/* We always try to keep the DPMS enabled so that
	   we can use set the mode manually.  We will
	   use zero values for the timeouts when the
	   we aren't active in order to prevent it
	   from activating.  */
	return x11_sync_server_dpms_settings (GDK_DISPLAY (),
					      dpms->priv->enabled,
					      standby,
					      suspend,
					      off,
					      error);
}

gboolean
gpm_dpms_get_enabled (GpmDpms  *dpms,
		      gboolean *enabled,
		      GError  **error)
{
	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

	if (enabled) {
		*enabled = dpms->priv->enabled;
	}

	return TRUE;
}

gboolean
gpm_dpms_set_enabled (GpmDpms *dpms,
		      gboolean enabled,
		      GError **error)
{
	gboolean ret;

	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

	ret = FALSE;
	if (dpms->priv->enabled != enabled) {
		egg_debug ("setting DPMS enabled: %d", enabled);

		dpms->priv->enabled = enabled;

		ret = sync_settings (dpms, error);
	}

	return ret;
}

gboolean
gpm_dpms_get_active (GpmDpms  *dpms,
		     gboolean *active,
		     GError  **error)
{
	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

	if (active) {
		*active = dpms->priv->active;
	}

	return TRUE;
}

gboolean
gpm_dpms_set_active (GpmDpms *dpms,
		     gboolean active,
		     GError **error)
{
	gboolean ret;

	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

	ret = FALSE;
	if (dpms->priv->active != active) {
		egg_debug ("setting DPMS active: %d", active);

		dpms->priv->active = active;

		ret = sync_settings (dpms, error);
	}

	return ret;
}

/* time specified in seconds */
gboolean
gpm_dpms_set_timeouts (GpmDpms	 *dpms,
		       guint	  standby,
		       guint	  suspend,
		       guint	  off,
		       GError   **error)
{
	gboolean ret;

	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

	dpms->priv->standby_timeout = standby;
	dpms->priv->suspend_timeout = suspend;
	dpms->priv->off_timeout     = off;

	ret = sync_settings (dpms, error);

	return ret;
}

/**
 * gpm_dpms_method_from_string:
 * @dpms_method: Method type, e.g. "off" or "staggered"
 *
 * Convert descriptive types to enumerated values.
 **/
GpmDpmsMethod
gpm_dpms_method_from_string (const gchar *dpms_method)
{
	GpmDpmsMethod method;

	/* default to unknown */
	method = GPM_DPMS_METHOD_UNKNOWN;
	if (dpms_method == NULL) {
		return method;
	}

	/* convert descriptive types to enumerated values */
	if (strcmp (dpms_method, "default") == 0) {
		method = GPM_DPMS_METHOD_DEFAULT;
	} else if (strcmp (dpms_method, "stagger") == 0) {
		method = GPM_DPMS_METHOD_STAGGER;
	} else if (strcmp (dpms_method, "standby") == 0) {
		method = GPM_DPMS_METHOD_STANDBY;
	} else if (strcmp (dpms_method, "suspend") == 0) {
		method = GPM_DPMS_METHOD_SUSPEND;
	} else if (strcmp (dpms_method, "off") == 0) {
		method = GPM_DPMS_METHOD_OFF;
	} else {
		egg_warning ("dpms_method '%s' not recognised", dpms_method);
	}

	return method;
}

GpmDpmsMode
gpm_dpms_mode_from_string (const gchar *str)
{
	if (str == NULL) {
		return GPM_DPMS_MODE_UNKNOWN;
	}

	if (strcmp (str, "on") == 0) {
		return GPM_DPMS_MODE_ON;
	} else if (strcmp (str, "standby") == 0) {
		return GPM_DPMS_MODE_STANDBY;
	} else if (strcmp (str, "suspend") == 0) {
		return GPM_DPMS_MODE_SUSPEND;
	} else if (strcmp (str, "off") == 0) {
		return GPM_DPMS_MODE_OFF;
	} else {
		return GPM_DPMS_MODE_UNKNOWN;
	}
}

const gchar *
gpm_dpms_mode_to_string (GpmDpmsMode mode)
{
	const gchar *str = NULL;

	switch (mode) {
	case GPM_DPMS_MODE_ON:
		str = "on";
		break;
	case GPM_DPMS_MODE_STANDBY:
		str = "standby";
		break;
	case GPM_DPMS_MODE_SUSPEND:
		str = "suspend";
		break;
	case GPM_DPMS_MODE_OFF:
		str = "off";
		break;
	default:
		str = NULL;
		break;
	}

	return str;
}

gboolean
gpm_dpms_set_mode_enum (GpmDpms    *dpms,
		        GpmDpmsMode mode,
		        GError    **error)
{
	gboolean ret;

	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

	if (mode == GPM_DPMS_MODE_UNKNOWN) {
		egg_debug ("mode unknown");
		g_set_error (error,
			     GPM_DPMS_ERROR,
			     GPM_DPMS_ERROR_GENERAL,
			     "Unknown DPMS mode");
		return FALSE;
	}

	ret = x11_set_mode (dpms, mode, error);

	return ret;
}

gboolean
gpm_dpms_get_mode_enum (GpmDpms     *dpms,
		        GpmDpmsMode *mode,
		        GError     **error)
{
	gboolean ret;

	if (mode) {
		*mode = GPM_DPMS_MODE_UNKNOWN;
	}

	ret = x11_get_mode (dpms, mode, error);

	return ret;
}

static void
gpm_dpms_set_standby_timeout (GpmDpms *dpms,
			      guint    timeout)
{
	g_return_if_fail (GPM_IS_DPMS (dpms));

	if (dpms->priv->standby_timeout != timeout) {
		dpms->priv->standby_timeout = timeout;
		sync_settings (dpms, NULL);
	}
}

static void
gpm_dpms_set_suspend_timeout (GpmDpms *dpms,
			      guint    timeout)
{
	g_return_if_fail (GPM_IS_DPMS (dpms));

	if (dpms->priv->suspend_timeout != timeout) {
		dpms->priv->suspend_timeout = timeout;
		sync_settings (dpms, NULL);
	}
}

static void
gpm_dpms_set_off_timeout (GpmDpms *dpms,
			  guint	   timeout)
{
	g_return_if_fail (GPM_IS_DPMS (dpms));

	if (dpms->priv->off_timeout != timeout) {
		dpms->priv->off_timeout = timeout;
		sync_settings (dpms, NULL);
	}
}

static void
gpm_dpms_set_property (GObject		  *object,
		       guint		   prop_id,
		       const GValue	  *value,
		       GParamSpec	  *pspec)
{
	GpmDpms *self;

	self = GPM_DPMS (object);

	switch (prop_id) {
	case PROP_STANDBY_TIMEOUT:
		gpm_dpms_set_standby_timeout (self, g_value_get_uint (value));
		break;
	case PROP_SUSPEND_TIMEOUT:
		gpm_dpms_set_suspend_timeout (self, g_value_get_uint (value));
		break;
	case PROP_OFF_TIMEOUT:
		gpm_dpms_set_off_timeout (self, g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_dpms_get_property (GObject		  *object,
		       guint		   prop_id,
		       GValue		  *value,
		       GParamSpec	  *pspec)
{
	GpmDpms *self;

	self = GPM_DPMS (object);

	switch (prop_id) {
	case PROP_STANDBY_TIMEOUT:
		g_value_set_uint (value, self->priv->standby_timeout);
		break;
	case PROP_SUSPEND_TIMEOUT:
		g_value_set_uint (value, self->priv->suspend_timeout);
		break;
	case PROP_OFF_TIMEOUT:
		g_value_set_uint (value, self->priv->off_timeout);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_dpms_class_init (GpmDpmsClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = gpm_dpms_finalize;
	object_class->get_property = gpm_dpms_get_property;
	object_class->set_property = gpm_dpms_set_property;

	signals [MODE_CHANGED] =
		g_signal_new ("mode-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDpmsClass, mode_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1, G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_STANDBY_TIMEOUT,
					 g_param_spec_uint ("standby-timeout",
							    NULL,
							    NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SUSPEND_TIMEOUT,
					 g_param_spec_uint ("suspend-timeout",
							    NULL,
							    NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_OFF_TIMEOUT,
					 g_param_spec_uint ("off-timeout",
							    NULL,
							    NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (GpmDpmsPrivate));
}

static gboolean
poll_dpms_mode (GpmDpms *dpms)
{
	gboolean    res;
	GpmDpmsMode mode;
	GError     *error;

#ifndef HAVE_DPMS_EXTENSION
	return FALSE;
#endif

	error = NULL;
	res = x11_get_mode (dpms, &mode, &error);

	/* Try again */
	if (! res) {
		g_clear_error (&error);

		return TRUE;
	}

	if (mode != dpms->priv->mode) {
		dpms->priv->mode = mode;

		g_signal_emit (dpms,
			       signals [MODE_CHANGED],
			       0,
			       mode);
	}

	/* FIXME: check that we are on console? */

	return TRUE;
}

static void
remove_poll_timer (GpmDpms *dpms)
{
	if (dpms->priv->timer_id != 0) {
		g_source_remove (dpms->priv->timer_id);
		dpms->priv->timer_id = 0;
	}
}

static void
add_poll_timer (GpmDpms *dpms,
		glong	 timeout)
{
	dpms->priv->timer_id = g_timeout_add (timeout, (GSourceFunc)poll_dpms_mode, dpms);
}

static void
gpm_dpms_init (GpmDpms *dpms)
{
	dpms->priv = GPM_DPMS_GET_PRIVATE (dpms);

	/* DPMSCapable() can never change for a given display */
	dpms->priv->dpms_capable = DPMSCapable (GDK_DISPLAY ());

	add_poll_timer (dpms, GPM_DPMS_POLL_TIME);
}

static void
gpm_dpms_finalize (GObject *object)
{
	GpmDpms *dpms;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_DPMS (object));

	dpms = GPM_DPMS (object);

	g_return_if_fail (dpms->priv != NULL);

	remove_poll_timer (dpms);

	G_OBJECT_CLASS (gpm_dpms_parent_class)->finalize (object);
}

GpmDpms *
gpm_dpms_new (void)
{
	if (gpm_dpms_object != NULL) {
		g_object_ref (gpm_dpms_object);
	} else {
		gpm_dpms_object = g_object_new (GPM_TYPE_DPMS, NULL);
		g_object_add_weak_pointer (gpm_dpms_object, &gpm_dpms_object);
	}
	return GPM_DPMS (gpm_dpms_object);
}
