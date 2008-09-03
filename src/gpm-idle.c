/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#include "gpm-idle.h"
#include "gpm-load.h"
#include "egg-debug.h"
#include "gpm-screensaver.h"

static void	gpm_idle_reset (GpmIdle *idle);

#define GPM_IDLE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_IDLE, GpmIdlePrivate))

/* How many seconds between polling? */
#define POLL_FREQUENCY	5

/* Sets the idle percent limit, i.e. how hard the computer can work
   while considered "at idle" */
#define IDLE_LIMIT	5

struct GpmIdlePrivate
{
	GpmLoad		*load;
	GpmScreensaver	*screensaver;
	GpmIdleMode	 mode;

	guint		 system_timeout;	/* in seconds */
	guint		 system_timer_id;
	guint		 system_idle_timer_id;

	gboolean	 check_type_cpu;

	gboolean	 init;
	long unsigned	 old_idle;
	long unsigned	 old_total;
};

enum {
	IDLE_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_idle_object = NULL;

G_DEFINE_TYPE (GpmIdle, gpm_idle, G_TYPE_OBJECT)

/**
 * gpm_idle_set_mode:
 * @idle: This class instance
 * @mode: The new mode, e.g. GPM_IDLE_MODE_SYSTEM
 **/
void
gpm_idle_set_mode (GpmIdle    *idle,
		   GpmIdleMode mode)
{
	g_return_if_fail (GPM_IS_IDLE (idle));

	if (mode != idle->priv->mode) {
		idle->priv->mode = mode;

		gpm_idle_reset (idle);

		egg_debug ("Doing a state transition: %d", mode);
		g_signal_emit (idle,
			       signals [IDLE_CHANGED],
			       0,
			       mode);
	}
}

/**
 * gpm_idle_poll_system_timer:
 * @idle: This class instance
 * Return value: If the current load is low enough for the transition to occur.
 **/
static gboolean
gpm_idle_poll_system_timer (GpmIdle *idle)
{
	gdouble  load;
	gboolean do_action = TRUE;

	/* get our computed load value */
	if (idle->priv->check_type_cpu) {

		load = gpm_load_get_current (idle->priv->load);

		/* FIXME: should this stay below this level for a certain time? */
		if (load > IDLE_LIMIT) {
			/* check if system is "idle" enough */
			egg_debug ("Detected that the CPU is busy");
			do_action = FALSE;
		}
	}

	if (do_action) {
		gpm_idle_set_mode (idle, GPM_IDLE_MODE_SYSTEM);
		idle->priv->system_idle_timer_id = 0;
		return FALSE;
	}

	return TRUE;
}

/**
 * gpm_idle_add_gpm_idle_poll_system_timer:
 **/
static void
gpm_idle_add_gpm_idle_poll_system_timer (GpmIdle *idle,
		       glong	timeout)
{
	idle->priv->system_idle_timer_id = g_timeout_add (timeout, (GSourceFunc)gpm_idle_poll_system_timer, idle);
}

/**
 * gpm_idle_remove_gpm_idle_poll_system_timer:
 **/
static void
gpm_idle_remove_gpm_idle_poll_system_timer (GpmIdle *idle)
{
	if (idle->priv->system_idle_timer_id != 0) {
		g_source_remove (idle->priv->system_idle_timer_id);
		idle->priv->system_idle_timer_id = 0;
	}
}

/**
 * system_timer:
 * @idle: This class instance
 *
 * Instead of doing the state transition directly we wait until the
 * system is quiet
 **/
static gboolean
system_timer (GpmIdle *idle)
{
	egg_debug ("System idle timeout");

	gpm_idle_remove_gpm_idle_poll_system_timer (idle);
	gpm_idle_add_gpm_idle_poll_system_timer (idle, POLL_FREQUENCY * 1000);

	idle->priv->system_timer_id = 0;
	return FALSE;
}

/**
 * gpm_idle_remove_system_timer:
 * @idle: This class instance
 **/
static void
gpm_idle_remove_system_timer (GpmIdle *idle)
{
	if (idle->priv->system_timer_id != 0) {
		g_source_remove (idle->priv->system_timer_id);
		idle->priv->system_timer_id = 0;
	}
}

/**
 * gpm_idle_remove_all_timers:
 * @idle: This class instance
 **/
static void
gpm_idle_remove_all_timers (GpmIdle *idle)
{
	gpm_idle_remove_gpm_idle_poll_system_timer (idle);
	gpm_idle_remove_system_timer (idle);
}

/**
 * add_system_timer:
 * @idle: This class instance
 *
 * Adds a idle timeout if the value is greater than zero
 **/
static void
add_system_timer (GpmIdle *idle)
{
	guint64 msecs;

	msecs = idle->priv->system_timeout * 1000;

	if (idle->priv->system_timeout > 0) {
		idle->priv->system_timer_id = g_timeout_add (msecs,
							     (GSourceFunc)system_timer, idle);
	} else {
		egg_debug ("System idle disabled");
	}
}

/**
 * gpm_idle_set_check_cpu:
 * @idle: This class instance
 * @check_type_cpu: If we should check the CPU before mode becomes
 *		    GPM_IDLE_MODE_SYSTEM and the event is done.
 **/
void
gpm_idle_set_check_cpu (GpmIdle    *idle,
			gboolean    check_type_cpu)
{
	g_return_if_fail (GPM_IS_IDLE (idle));
	egg_debug ("Setting the CPU load check to %i", check_type_cpu);
	idle->priv->check_type_cpu = check_type_cpu;
}

/**
 * gpm_idle_reset:
 * @idle: This class instance
 *
 * Reset the idle timer.
 **/
static void
gpm_idle_reset (GpmIdle *idle)
{
	g_return_if_fail (GPM_IS_IDLE (idle));

	gpm_idle_remove_all_timers (idle);

	if (idle->priv->mode == GPM_IDLE_MODE_NORMAL) {
		/* do nothing */
	} else if (idle->priv->mode == GPM_IDLE_MODE_SESSION) {
		/* restart system idle timer */
		add_system_timer (idle);
	} else if (idle->priv->mode == GPM_IDLE_MODE_SYSTEM) {
		/* do nothing? */
	}
}

/**
 * gpm_idle_get_mode:
 * @idle: This class instance
 * Return value: The current mode, e.g. GPM_IDLE_MODE_SYSTEM
 **/
GpmIdleMode
gpm_idle_get_mode (GpmIdle *idle)
{
	GpmIdleMode mode;
	mode = idle->priv->mode;
	return mode;
}

/**
 * gpm_idle_set_system_timeout:
 * @idle: This class instance
 * @timeout: The new timeout we want to set, in seconds
 **/
void
gpm_idle_set_system_timeout (GpmIdle	*idle,
			     guint	 timeout)
{
	g_return_if_fail (GPM_IS_IDLE (idle));

	egg_debug ("Setting system idle timeout: %d", timeout);
	if (idle->priv->system_timeout != timeout) {
		idle->priv->system_timeout = timeout;

		/* restart the timers if necessary */
		gpm_idle_reset (idle);
	}
}

/**
 * session_idle_changed_cb:
 * @is_idle: If the session is idle
 * @idle: This class instance
 *
 * The SessionIdleChanged callback from gnome-screensaver.
 **/
static void
session_idle_changed_cb (GpmScreensaver *screensaver,
			 gboolean	 is_idle,
			 GpmIdle	*idle)
{
	GpmIdleMode mode;

	egg_debug ("Received GS session idle changed: %d", is_idle);

	if (is_idle) {
		mode = GPM_IDLE_MODE_SESSION;
	} else {
		mode = GPM_IDLE_MODE_NORMAL;
	}

	gpm_idle_set_mode (idle, mode);
}

/**
 * powersave_idle_changed_cb:
 * @is_idle: If the session is idle
 * @idle: This class instance
 *
 * The SessionIdleChanged callback from gnome-screensaver.
 **/
static void
powersave_idle_changed_cb (GpmScreensaver *screensaver,
			  gboolean	 is_idle,
			  GpmIdle	*idle)
{
	GpmIdleMode mode;

	egg_debug ("Received GS powesave idle changed: %d", is_idle);

	if (is_idle) {
		mode = GPM_IDLE_MODE_POWERSAVE;
	} else {
		mode = GPM_IDLE_MODE_NORMAL;
	}

	gpm_idle_set_mode (idle, mode);
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

	g_object_unref (idle->priv->load);
	g_object_unref (idle->priv->screensaver);
	gpm_idle_remove_all_timers (idle);

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
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1, G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (GpmIdlePrivate));
}

/**
 * gpm_idle_init:
 * @idle: This class instance
 *
 * Gets a DBUS connection, and aquires the screensaver connection so we can
 * get session changed events.
 *
 **/
static void
gpm_idle_init (GpmIdle *idle)
{
	idle->priv = GPM_IDLE_GET_PRIVATE (idle);

	idle->priv->system_timeout = G_MAXUINT;
	idle->priv->load = gpm_load_new ();
	idle->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (idle->priv->screensaver, "session-idle-changed",
			  G_CALLBACK (session_idle_changed_cb), idle);
	g_signal_connect (idle->priv->screensaver, "powersave-idle-changed",
			  G_CALLBACK (powersave_idle_changed_cb), idle);
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

