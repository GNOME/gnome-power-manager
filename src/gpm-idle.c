/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2009 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>

#include "egg-debug.h"
#include "egg-idletime.h"

#include "gpm-idle.h"
#include "gpm-load.h"
#include "gpm-session.h"

#define GPM_IDLE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_IDLE, GpmIdlePrivate))

/* Sets the idle percent limit, i.e. how hard the computer can work
   while considered "at idle" */
#define GPM_IDLE_CPU_LIMIT			5
#define GPM_IDLE_TIMEOUT_IGNORE_DPMS_CHANGE	1.0f /* seconds */

struct GpmIdlePrivate
{
	EggIdletime	*idletime;
	GpmLoad		*load;
	GpmSession	*session;
	GpmIdleMode	 mode;
	guint		 timeout_dim;		/* in seconds */
	guint		 timeout_blank;		/* in seconds */
	guint		 timeout_sleep;		/* in seconds */
	guint		 timeout_blank_id;
	guint		 timeout_sleep_id;
	guint		 idletime_id;
	guint		 idletime_ignore_id;
	gboolean	 x_idle;
	gboolean	 check_type_cpu;
	GTimer		*timer;
};

enum {
	IDLE_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_idle_object = NULL;

G_DEFINE_TYPE (GpmIdle, gpm_idle, G_TYPE_OBJECT)

/**
 * gpm_idle_mode_to_text:
 **/
static const gchar *
gpm_idle_mode_to_text (GpmIdleMode mode)
{
	if (mode == GPM_IDLE_MODE_NORMAL)
		return "normal";
	if (mode == GPM_IDLE_MODE_DIM)
		return "dim";
	if (mode == GPM_IDLE_MODE_BLANK)
		return "blank";
	if (mode == GPM_IDLE_MODE_SLEEP)
		return "sleep";
	return "unknown";
}

/**
 * gpm_idle_set_mode:
 * @idle: This class instance
 * @mode: The new mode, e.g. GPM_IDLE_MODE_SLEEP
 **/
static void
gpm_idle_set_mode (GpmIdle *idle, GpmIdleMode mode)
{
	g_return_if_fail (GPM_IS_IDLE (idle));

	if (mode != idle->priv->mode) {
		idle->priv->mode = mode;

		/* we save the time of the last state, so we can ignore the X11
		 * timer reset when we change brightness or do DPMS actions */
		g_timer_reset (idle->priv->timer);

		egg_debug ("Doing a state transition: %s", gpm_idle_mode_to_text (mode));
		g_signal_emit (idle, signals [IDLE_CHANGED], 0, mode);
	}
}

/**
 * gpm_idle_set_check_cpu:
 * @idle: This class instance
 * @check_type_cpu: If we should check the CPU before mode becomes
 *		    GPM_IDLE_MODE_SLEEP and the event is done.
 **/
void
gpm_idle_set_check_cpu (GpmIdle *idle, gboolean check_type_cpu)
{
	g_return_if_fail (GPM_IS_IDLE (idle));
	egg_debug ("Setting the CPU load check to %i", check_type_cpu);
	idle->priv->check_type_cpu = check_type_cpu;
}

/**
 * gpm_idle_get_mode:
 * @idle: This class instance
 * Return value: The current mode, e.g. GPM_IDLE_MODE_SLEEP
 **/
GpmIdleMode
gpm_idle_get_mode (GpmIdle *idle)
{
	return idle->priv->mode;
}

/**
 * gpm_idle_blank_cb:
 **/
static gboolean
gpm_idle_blank_cb (GpmIdle *idle)
{
	if (idle->priv->mode > GPM_IDLE_MODE_BLANK) {
		egg_debug ("ignoring current mode %s", gpm_idle_mode_to_text (idle->priv->mode));
		return FALSE;
	}
	gpm_idle_set_mode (idle, GPM_IDLE_MODE_BLANK);
	return FALSE;
}

/**
 * gpm_idle_sleep_cb:
 **/
static gboolean
gpm_idle_sleep_cb (GpmIdle *idle)
{
	gdouble load;
	gboolean ret = FALSE;

	/* get our computed load value */
	if (idle->priv->check_type_cpu) {
		load = gpm_load_get_current (idle->priv->load);
		/* FIXME: should this stay below this level for a certain time? */
		if (load > GPM_IDLE_CPU_LIMIT) {
			/* check if system is "idle" enough */
			egg_debug ("Detected that the CPU is busy");
			ret = TRUE;
			goto out;
		}
	}
	gpm_idle_set_mode (idle, GPM_IDLE_MODE_SLEEP);
out:
	return ret;
}

/**
 * gpm_idle_evaluate:
 **/
static void
gpm_idle_evaluate (GpmIdle *idle)
{
	gboolean is_idle;
	gboolean is_inhibited;

	is_idle = gpm_session_get_idle (idle->priv->session);
	is_inhibited = gpm_session_get_inhibited (idle->priv->session);
	egg_debug ("is_idle=%i, is_inhibited=%i", is_idle, is_inhibited);

	/* check we are really idle */
	if (!idle->priv->x_idle) {
		gpm_idle_set_mode (idle, GPM_IDLE_MODE_NORMAL);
		egg_debug ("X not idle");
		if (idle->priv->timeout_blank_id != 0) {
			g_source_remove (idle->priv->timeout_blank_id);
			idle->priv->timeout_blank_id = 0;
		}
		if (idle->priv->timeout_sleep_id != 0) {
			g_source_remove (idle->priv->timeout_sleep_id);
			idle->priv->timeout_sleep_id = 0;
		}
		goto out;
	}

	/* are we inhibited */
	if (is_inhibited) {
		egg_debug ("inhibited, so using normal state");
		gpm_idle_set_mode (idle, GPM_IDLE_MODE_NORMAL);
		if (idle->priv->timeout_blank_id != 0) {
			g_source_remove (idle->priv->timeout_blank_id);
			idle->priv->timeout_blank_id = 0;
		}
		if (idle->priv->timeout_sleep_id != 0) {
			g_source_remove (idle->priv->timeout_sleep_id);
			idle->priv->timeout_sleep_id = 0;
		}
		goto out;
	}

	/* normal to dim */
	if (idle->priv->mode == GPM_IDLE_MODE_NORMAL) {
		egg_debug ("normal to dim");
		gpm_idle_set_mode (idle, GPM_IDLE_MODE_DIM);
	}

	/* set up blank callback even when session is not idle,
	 * but only if we actually want to blank. */
	if (idle->priv->timeout_blank_id == 0 &&
	    idle->priv->timeout_blank != 0) {
		egg_debug ("setting up blank callback");
		idle->priv->timeout_blank_id = g_timeout_add_seconds (idle->priv->timeout_blank, (GSourceFunc) gpm_idle_blank_cb, idle);
	}

	/* only do the sleep timeout when the session is idle */
	if (is_idle) {
		if (idle->priv->timeout_sleep_id == 0 &&
		    idle->priv->timeout_sleep != 0) {
			egg_debug ("setting up sleep callback = %i", idle->priv->timeout_blank);
			idle->priv->timeout_sleep_id = g_timeout_add_seconds (idle->priv->timeout_sleep, (GSourceFunc) gpm_idle_sleep_cb, idle);
		}
	}
out:
	return;
}

/**
 * gpm_idle_set_timeout_dim:
 * @idle: This class instance
 * @timeout: The new timeout we want to set, in seconds
 **/
gboolean
gpm_idle_set_timeout_dim (GpmIdle *idle, guint timeout)
{
	g_return_val_if_fail (GPM_IS_IDLE (idle), FALSE);

	egg_debug ("Setting dim idle timeout: %ds", timeout);
	if (idle->priv->timeout_dim != timeout) {
		idle->priv->timeout_dim = timeout;

		/* remove old id */
		if (idle->priv->idletime_id != 0)
			egg_idletime_remove_watch (idle->priv->idletime, idle->priv->idletime_id);
		idle->priv->idletime_id =
			egg_idletime_add_watch (idle->priv->idletime, timeout * 1000);
	}
	return TRUE;
}

/**
 * gpm_idle_set_timeout_blank:
 * @idle: This class instance
 * @timeout: The new timeout we want to set, in seconds
 **/
gboolean
gpm_idle_set_timeout_blank (GpmIdle *idle, guint timeout)
{
	g_return_val_if_fail (GPM_IS_IDLE (idle), FALSE);

	egg_debug ("Setting blank idle timeout: %ds", timeout);
	if (idle->priv->timeout_blank != timeout) {
		idle->priv->timeout_blank = timeout;
		gpm_idle_evaluate (idle);
	}
	return TRUE;
}

/**
 * gpm_idle_set_timeout_sleep:
 * @idle: This class instance
 * @timeout: The new timeout we want to set, in seconds
 **/
gboolean
gpm_idle_set_timeout_sleep (GpmIdle *idle, guint timeout)
{
	g_return_val_if_fail (GPM_IS_IDLE (idle), FALSE);

	egg_debug ("Setting sleep idle timeout: %ds", timeout);
	if (idle->priv->timeout_sleep != timeout) {
		idle->priv->timeout_sleep = timeout;
		gpm_idle_evaluate (idle);
	}
	return TRUE;
}

/**
 * gpm_idle_session_idle_changed_cb:
 * @is_idle: If the session is idle
 * @idle: This class instance
 *
 * The SessionIdleChanged callback from gnome-session.
 **/
static void
gpm_idle_session_idle_changed_cb (GpmSession *session, gboolean is_idle, GpmIdle *idle)
{
	egg_debug ("Received gnome session idle changed: %i", is_idle);
	gpm_idle_evaluate (idle);
}

/**
 * gpm_idle_session_inhibited_changed_cb:
 **/
static void
gpm_idle_session_inhibited_changed_cb (GpmSession *session, gboolean is_inhibited, GpmIdle *idle)
{
	egg_debug ("Received gnome session inhibited changed: %i", is_inhibited);
	gpm_idle_evaluate (idle);
}

/**
 * gpm_idle_idletime_alarm_expired_cb:
 *
 * We're idle, something timed out
 **/
static void
gpm_idle_idletime_alarm_expired_cb (EggIdletime *idletime, guint alarm_id, GpmIdle *idle)
{
	egg_debug ("idletime alarm: %i", alarm_id);

	if (alarm_id == idle->priv->idletime_ignore_id)
		egg_debug ("expired 1ms timeout");

	/* set again */
	idle->priv->x_idle = TRUE;
	gpm_idle_evaluate (idle);
}

/**
 * gpm_idle_idletime_reset_cb:
 *
 * We're no longer idle, the user moved
 **/
static void
gpm_idle_idletime_reset_cb (EggIdletime *idletime, GpmIdle *idle)
{
	gdouble elapsed;
	elapsed = g_timer_elapsed (idle->priv->timer, NULL);

	/* have we just chaged state? */
	if (idle->priv->mode == GPM_IDLE_MODE_BLANK &&
	    elapsed < GPM_IDLE_TIMEOUT_IGNORE_DPMS_CHANGE) {
		egg_debug ("ignoring reset, as we've just done a state change");
		/* make sure we trigger a short 1ms timeout so we can get the expired signal */
		if (idle->priv->idletime_ignore_id != 0)
			egg_idletime_remove_watch (idle->priv->idletime, idle->priv->idletime_ignore_id);
		idle->priv->idletime_ignore_id =
			egg_idletime_add_watch (idle->priv->idletime, 1);
		return;
	}

	/* remove 1ms timeout */
	if (idle->priv->idletime_ignore_id != 0) {
		egg_debug ("removing 1ms timeout");
		egg_idletime_remove_watch (idle->priv->idletime, idle->priv->idletime_ignore_id);
		idle->priv->idletime_ignore_id = 0;
	}

	idle->priv->x_idle = FALSE;
	gpm_idle_evaluate (idle);
}

/**
 * gpm_idle_finalize:
 * @object: This class instance
 **/
static void
gpm_idle_finalize (GObject *object)
{
	GpmIdle *idle;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_IDLE (object));

	idle = GPM_IDLE (object);

	g_return_if_fail (idle->priv != NULL);

	if (idle->priv->timeout_blank_id != 0)
		g_source_remove (idle->priv->timeout_blank_id);
	if (idle->priv->timeout_sleep_id != 0)
		g_source_remove (idle->priv->timeout_sleep_id);

	g_timer_destroy (idle->priv->timer);
	g_object_unref (idle->priv->load);
	g_object_unref (idle->priv->session);

	if (idle->priv->idletime_id != 0)
		egg_idletime_remove_watch (idle->priv->idletime, idle->priv->idletime_id);
	if (idle->priv->idletime_ignore_id != 0)
		egg_idletime_remove_watch (idle->priv->idletime, idle->priv->idletime_ignore_id);
	g_object_unref (idle->priv->idletime);

	G_OBJECT_CLASS (gpm_idle_parent_class)->finalize (object);
}

/**
 * gpm_idle_class_init:
 * @klass: This class instance
 **/
static void
gpm_idle_class_init (GpmIdleClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_idle_finalize;

	signals [IDLE_CHANGED] =
		g_signal_new ("idle-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmIdleClass, idle_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (GpmIdlePrivate));
}

/**
 * gpm_idle_init:
 * @idle: This class instance
 *
 * Gets a DBUS connection, and aquires the session connection so we can
 * get session changed events.
 *
 **/
static void
gpm_idle_init (GpmIdle *idle)
{
	idle->priv = GPM_IDLE_GET_PRIVATE (idle);

	idle->priv->timeout_dim = G_MAXUINT;
	idle->priv->timeout_blank = G_MAXUINT;
	idle->priv->timeout_sleep = G_MAXUINT;
	idle->priv->timeout_blank_id = 0;
	idle->priv->timeout_sleep_id = 0;
	idle->priv->idletime_id = 0;
	idle->priv->idletime_ignore_id = 0;
	idle->priv->x_idle = FALSE;
	idle->priv->timer = g_timer_new ();
	idle->priv->load = gpm_load_new ();
	idle->priv->session = gpm_session_new ();
	g_signal_connect (idle->priv->session, "idle-changed", G_CALLBACK (gpm_idle_session_idle_changed_cb), idle);
	g_signal_connect (idle->priv->session, "inhibited-changed", G_CALLBACK (gpm_idle_session_inhibited_changed_cb), idle);

	idle->priv->idletime = egg_idletime_new ();
	g_signal_connect (idle->priv->idletime, "reset", G_CALLBACK (gpm_idle_idletime_reset_cb), idle);
	g_signal_connect (idle->priv->idletime, "alarm-expired", G_CALLBACK (gpm_idle_idletime_alarm_expired_cb), idle);

	gpm_idle_evaluate (idle);
}

/**
 * gpm_idle_new:
 * Return value: A new GpmIdle instance.
 **/
GpmIdle *
gpm_idle_new (void)
{
	if (gpm_idle_object != NULL) {
		g_object_ref (gpm_idle_object);
	} else {
		gpm_idle_object = g_object_new (GPM_TYPE_IDLE, NULL);
		g_object_add_weak_pointer (gpm_idle_object, &gpm_idle_object);
	}
	return GPM_IDLE (gpm_idle_object);
}

