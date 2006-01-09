/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2005 William Jon McCann <mccann@jhu.edu>
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
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
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
#include <X11/Xproto.h>			/* for CARD16 */
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsstr.h>
#endif

#include "gpm-dpms.h"

static void     gpm_dpms_class_init (GpmDpmsClass *klass);
static void     gpm_dpms_init       (GpmDpms      *power);
static void     gpm_dpms_finalize   (GObject      *object);

#define GPM_DPMS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_DPMS, GpmDpmsPrivate))

struct GpmDpmsPrivate
{
	gboolean       enabled;
	gboolean       active;

	guint	       standby_timeout;
	guint	       suspend_timeout;
	guint	       off_timeout;

	GpmDpmsMode    mode;

	guint	       timer_id;
};

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0
};

static GObjectClass *parent_class = NULL;
static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmDpms, gpm_dpms, G_TYPE_OBJECT)

/* the following function is derived from
   xscreensaver Copyright (C) Jamie Zawinski
*/
static void
x11_sync_server_dpms_settings (Display *dpy,
			       gboolean enabled,
			       int	standby_secs,
			       int	suspend_secs,
			       int	off_secs)
{
#ifdef HAVE_DPMS_EXTENSION

	int	 event = 0, error = 0;
	BOOL	 o_enabled = FALSE;
	CARD16	 o_power = 0;
	CARD16	 o_standby = 0, o_suspend = 0, o_off = 0;
	gboolean bogus = FALSE;

	if (standby_secs == 0 && suspend_secs == 0 && off_secs == 0)
		/* all zero implies "DPMS disabled" */
		enabled = FALSE;

	else if ((standby_secs != 0 && standby_secs < 10) ||
		 (suspend_secs != 0 && suspend_secs < 10) ||
		 (off_secs     != 0 && off_secs	    < 10))
		/* any negative, or any positive-and-less-than-10-seconds, is crazy. */
		bogus = TRUE;

	if (bogus)
		enabled = FALSE;

	g_debug ("Syncing DPMS settings enabled=%d timeouts=%d %d %d",
		 enabled, standby_secs, suspend_secs, off_secs);

	if (! DPMSQueryExtension (dpy, &event, &error)) {
		g_debug ("XDPMS extension not supported.");

		return;
	}

	if (! DPMSCapable (dpy)) {
		g_debug ("DPMS not supported.");

		return;
	}

	if (! DPMSInfo (dpy, &o_power, &o_enabled)) {
		g_debug ("unable to get DPMS state.");

		return;
	}

	if (o_enabled != enabled) {
		int res;

		if (enabled) {
			res = DPMSEnable (dpy);
		} else {
			res = DPMSDisable (dpy);
		}

		if (! res) {
			g_debug ("unable to set DPMS state.");

			return;
		} else {
			g_debug ("turned DPMS %s", enabled ? "ON" : "OFF");
		}
	}

	if (bogus) {
		g_debug ("not setting bogus DPMS timeouts: %d %d %d.",
			  standby_secs, suspend_secs, off_secs);
		return;
	}

	if (! DPMSGetTimeouts (dpy, &o_standby, &o_suspend, &o_off)) {
		g_debug ("unable to get DPMS timeouts.");

		return;
	}

	if (o_standby != standby_secs ||
	    o_suspend != suspend_secs ||
	    o_off != off_secs) {
		if (! DPMSSetTimeouts (dpy, standby_secs, suspend_secs, off_secs)) {
			g_debug ("unable to set DPMS timeouts.");

			return;
		} else {
			g_debug ("set DPMS timeouts: %d %d %d.", 
				  standby_secs, suspend_secs, off_secs);
		}
	}

# else	/* !HAVE_DPMS_EXTENSION */

	g_debug ("DPMS support not compiled in.");

# endif /* HAVE_DPMS_EXTENSION */
}

#ifdef HAVE_DPMS_EXTENSION

static GpmDpmsMode
x11_get_mode (GpmDpms *power)
{
	GpmDpmsMode result;
	int	    event_number;
	int	    error_number;
	BOOL	    enabled = FALSE;
	CARD16	    state;

	if (! DPMSQueryExtension (GDK_DISPLAY (), &event_number, &error_number)) {
		/* Server doesn't know -- assume the monitor is on. */
		result = GPM_DPMS_MODE_ON;

	} else if (! DPMSCapable (GDK_DISPLAY ())) {
		/* Server says the monitor doesn't do power management -- so it's on. */
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

	return result;
}

static void
x11_set_mode (GpmDpms	 *power,
	      GpmDpmsMode mode)
{
	GpmDpmsMode current_mode;
	CARD16	    state;
	CARD16	    current_state;
	BOOL	    current_enabled;
	int	    event_number;
	int	    error_number;

	if (! DPMSQueryExtension (GDK_DISPLAY (), &event_number, &error_number)) {
		g_debug ("unable to query DPMS extention");
		return;
	}

	if (! DPMSCapable (GDK_DISPLAY ())) {
		g_debug ("not DPMS capable");
		return;
	}

	if (! DPMSInfo (GDK_DISPLAY (), &current_state, &current_enabled)) {
		g_debug ("couldn't get DPMS info");
		return;
	}

	if (! current_enabled) {
		g_debug ("DPMS not enabled");
		return;
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

	current_mode = x11_get_mode (power);

	if (current_mode != mode) {
		DPMSForceLevel (GDK_DISPLAY (), state);
		XSync (GDK_DISPLAY (), FALSE);
	}
}

#else  /* HAVE_DPMS_EXTENSION */

static GpmDpmsMode
x11_get_mode (GpmDpms *power) 
{
	return GPM_DPMS_MODE_ON; 
}

static void
x11_set_mode (GpmDpms	 *power,
	      GpmDpmsMode mode)
{
	return; 
}

#endif /* !HAVE_DPMS_EXTENSION */

static void
sync_settings (GpmDpms *power)
{
	gboolean permitted;

	/* to use power management it has to be
	   allowed by policy (ie. enabled) and
	   be requested at this particular time (ie. active)
	*/

	/* FIXME: should we require the session to be on console? */

	permitted = power->priv->enabled && power->priv->active;

	x11_sync_server_dpms_settings (GDK_DISPLAY (),
				       permitted,
				       power->priv->standby_timeout,
				       power->priv->suspend_timeout,
				       power->priv->off_timeout);
}

gboolean
gpm_dpms_get_enabled (GpmDpms *power)
{
	g_return_val_if_fail (GPM_IS_DPMS (power), FALSE);

	return power->priv->enabled;
}

void
gpm_dpms_set_enabled (GpmDpms *power,
		      gboolean enabled)
{
	g_return_if_fail (GPM_IS_DPMS (power));

	if (power->priv->enabled != enabled) {
		g_debug ("setting DPMS enabled: %d", enabled);

		power->priv->enabled = enabled;

		sync_settings (power);
	}
}

gboolean
gpm_dpms_get_active (GpmDpms *power)
{
	g_return_val_if_fail (GPM_IS_DPMS (power), FALSE);

	return power->priv->active;
}

gboolean
gpm_dpms_set_active (GpmDpms *power,
		     gboolean active)
{
	g_return_val_if_fail (GPM_IS_DPMS (power), FALSE);

	if (power->priv->active != active) {
		g_debug ("setting DPMS active: %d", active);

		power->priv->active = active;

		sync_settings (power);
	}

	return TRUE;
}

/* time specified in seconds */
void
gpm_dpms_set_timeouts (GpmDpms	 *power,
		       guint	  standby,
		       guint	  suspend,
		       guint	  off)
{
	g_return_if_fail (GPM_IS_DPMS (power));

	power->priv->standby_timeout = standby;
	power->priv->suspend_timeout = suspend;
	power->priv->off_timeout     = off;

	sync_settings (power);
}

GpmDpmsMode
gpm_dpms_mode_from_string (const char *str)
{
	if (str == NULL) {
		return GPM_DPMS_MODE_UNKNOWN;
	}

	if (strcmp (str, "on") == 0) {
		return GPM_DPMS_MODE_OFF;
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

const char *
gpm_dpms_mode_to_string (GpmDpmsMode mode)
{
	const char *str = NULL;

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

void
gpm_dpms_set_mode (GpmDpms    *power,
		   GpmDpmsMode mode)
{
	g_return_if_fail (GPM_IS_DPMS (power));

	if (! power->priv->enabled) {
		return;
	}

	if (! power->priv->active) {
		return;
	}

	if (mode == GPM_DPMS_MODE_UNKNOWN) {
		return;
	}

	x11_set_mode (power, mode);
}

GpmDpmsMode
gpm_dpms_get_mode (GpmDpms *power)
{
	GpmDpmsMode mode;

	mode = x11_get_mode (power);

	return mode;
}

static void
gpm_dpms_set_property (GObject		  *object,
		       guint		   prop_id,
		       const GValue	  *value,
		       GParamSpec	  *pspec)
{
	switch (prop_id) {
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
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_dpms_class_init (GpmDpmsClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize	   = gpm_dpms_finalize;
	object_class->get_property = gpm_dpms_get_property;
	object_class->set_property = gpm_dpms_set_property;

	signals [CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDpmsClass, changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1, G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (GpmDpmsPrivate));
}

static gboolean
poll_power_mode (GpmDpms *power)
{
	GpmDpmsMode mode;

#ifndef HAVE_DPMS_EXTENSION
	return FALSE;
#endif

	mode = x11_get_mode (power);
	if (mode != power->priv->mode) {
		power->priv->mode = mode;

		g_signal_emit (power,
			       signals [CHANGED],
			       0,
			       mode);
	}

	/* FIXME: check that we are on console? */

	return TRUE;
}


static void
remove_poll_timer (GpmDpms *power)
{
	if (power->priv->timer_id != 0) {
		g_source_remove (power->priv->timer_id);
		power->priv->timer_id = 0;
	}
}

static void
add_poll_timer (GpmDpms *power,
		glong	 timeout)
{
	power->priv->timer_id = g_timeout_add (timeout, (GSourceFunc)poll_power_mode, power);
}

static void
gpm_dpms_init (GpmDpms *power)
{
	power->priv = GPM_DPMS_GET_PRIVATE (power);

	add_poll_timer (power, 500);
}

static void
gpm_dpms_finalize (GObject *object)
{
	GpmDpms *power;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_DPMS (object));

	power = GPM_DPMS (object);

	g_return_if_fail (power->priv != NULL);

	remove_poll_timer (power);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmDpms *
gpm_dpms_new (void)
{
	GpmDpms *power;

	power = g_object_new (GPM_TYPE_DPMS, NULL);

	return GPM_DPMS (power);
}
