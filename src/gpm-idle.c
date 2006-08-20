/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "gpm-idle.h"
#include "gpm-debug.h"

static void     gpm_idle_class_init 	(GpmIdleClass *klass);
static void     gpm_idle_init       	(GpmIdle      *idle);
static void     gpm_idle_finalize   	(GObject      *object);
static gdouble  gpm_idle_compute_load	(GpmIdle      *idle);

#define GPM_IDLE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_IDLE, GpmIdlePrivate))

/* How many seconds between polling? */
#define POLL_FREQUENCY	5

/* Sets the idle percent limit, i.e. how hard the computer can work
   while considered "at idle" */
#define IDLE_LIMIT	5

struct GpmIdlePrivate
{
	DBusGConnection *connection;
	DBusGProxy	*screensaver_object;

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
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_MODE,
	PROP_SYSTEM_TIMEOUT
};

#define GS_DBUS_SERVICE		"org.gnome.ScreenSaver"
#define GS_DBUS_PATH		"/org/gnome/ScreenSaver"
#define GS_DBUS_INTERFACE	"org.gnome.ScreenSaver"

static guint	signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmIdle, gpm_idle, G_TYPE_OBJECT)

/**
 * poll_system_timer:
 * @idle: This idle class instance
 * Return value: If the current load is low enough for the transition to occur.
 **/
static gboolean
poll_system_timer (GpmIdle *idle)
{
	gdouble  load;
	gboolean do_action = TRUE;

	/* get our computed load value */
	if (idle->priv->check_type_cpu) {

		load = gpm_idle_compute_load (idle);

		/* FIXME: should this stay below this level for a certain time? */
		if (load > IDLE_LIMIT) {
			/* check if system is "idle" enough */
			gpm_debug ("Detected that the CPU is busy");
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
 * add_poll_system_timer:
 **/
static void
add_poll_system_timer (GpmIdle *idle,
		       glong	timeout)
{
	idle->priv->system_idle_timer_id = g_timeout_add (timeout, (GSourceFunc)poll_system_timer, idle);
}

/**
 * remove_poll_system_timer:
 **/
static void
remove_poll_system_timer (GpmIdle *idle)
{
	if (idle->priv->system_idle_timer_id != 0) {
		g_source_remove (idle->priv->system_idle_timer_id);
		idle->priv->system_idle_timer_id = 0;
	}
}

/**
 * system_timer:
 * @idle: This idle class instance
 *
 * Instead of doing the state transition directly we wait until the
 * system is quiet
 **/
static gboolean
system_timer (GpmIdle *idle)
{
	gpm_debug ("System idle timeout");

	remove_poll_system_timer (idle);
	add_poll_system_timer (idle, POLL_FREQUENCY * 1000);

	idle->priv->system_timer_id = 0;
	return FALSE;
}

/**
 * remove_system_timer:
 * @idle: This idle class instance
 **/
static void
remove_system_timer (GpmIdle *idle)
{
	if (idle->priv->system_timer_id != 0) {
		g_source_remove (idle->priv->system_timer_id);
		idle->priv->system_timer_id = 0;
	}
}

/**
 * remove_all_timers:
 * @idle: This idle class instance
 **/
static void
remove_all_timers (GpmIdle *idle)
{
	remove_poll_system_timer (idle);
	remove_system_timer (idle);
}

/**
 * add_system_timer:
 * @idle: This idle class instance
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
		gpm_debug ("System idle disabled");
	}
}

/**
 * gpm_idle_set_check_cpu:
 * @idle: This idle class instance
 * @check_type_cpu: If we should check the CPU before mode becomes
 *		    GPM_IDLE_MODE_SYSTEM and the event is done.
 **/
void
gpm_idle_set_check_cpu (GpmIdle    *idle,
			gboolean    check_type_cpu)
{
	g_return_if_fail (GPM_IS_IDLE (idle));
	gpm_debug ("Setting the CPU load check to %i", check_type_cpu);
	idle->priv->check_type_cpu = check_type_cpu;
}

/**
 * gpm_idle_set_mode:
 * @idle: This idle class instance
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

		gpm_debug ("Doing a state transition: %d", mode);
		g_signal_emit (idle,
			       signals [CHANGED],
			       0,
			       mode);
	}
}

/**
 * gpm_idle_get_mode:
 * @idle: This idle class instance
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
 * gpm_idle_reset:
 * @idle: This idle class instance
 *
 * Reset the idle timer.
 **/
void
gpm_idle_reset (GpmIdle *idle)
{
	g_return_if_fail (GPM_IS_IDLE (idle));

	remove_all_timers (idle);

	switch (idle->priv->mode) {
	case GPM_IDLE_MODE_NORMAL:

		break;
	case GPM_IDLE_MODE_SESSION:
		/* restart system idle timer */
		add_system_timer (idle);

		break;
	case GPM_IDLE_MODE_SYSTEM:
		/* nothing? */
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

/**
 * gpm_idle_set_system_timeout:
 * @idle: This idle class instance
 * @timeout: The new timeout we want to set, in seconds
 **/
void
gpm_idle_set_system_timeout (GpmIdle	*idle,
			     guint	 timeout)
{
	g_return_if_fail (GPM_IS_IDLE (idle));

	gpm_debug ("Setting system idle timeout: %d", timeout);

	if (idle->priv->system_timeout != timeout) {
		idle->priv->system_timeout = timeout;

		/* restart the timers if necessary */
		gpm_idle_reset (idle);
	}
}

/**
 * gpm_idle_set_property:
 **/
static void
gpm_idle_set_property (GObject		  *object,
		       guint		   prop_id,
		       const GValue	  *value,
		       GParamSpec	  *pspec)
{
	GpmIdle *idle;

	idle = GPM_IDLE (object);

	switch (prop_id) {
	case PROP_SYSTEM_TIMEOUT:
		gpm_idle_set_system_timeout (idle, g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gpm_idle_get_property:
 **/
static void
gpm_idle_get_property (GObject		  *object,
		       guint		   prop_id,
		       GValue		  *value,
		       GParamSpec	  *pspec)
{
	GpmIdle *idle;

	idle = GPM_IDLE (object);

	switch (prop_id) {
	case PROP_SYSTEM_TIMEOUT:
		g_value_set_uint (value, idle->priv->system_timeout);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gpm_idle_class_init:
 * @klass: This idle class instance
 **/
static void
gpm_idle_class_init (GpmIdleClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = gpm_idle_finalize;
	object_class->get_property = gpm_idle_get_property;
	object_class->set_property = gpm_idle_set_property;

	signals [CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmIdleClass, changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1, G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_MODE,
					 g_param_spec_uint ("mode",
							    NULL,
							    NULL,
							    0,
							    G_MAXUINT,
							    GPM_IDLE_MODE_NORMAL,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SYSTEM_TIMEOUT,
					 g_param_spec_uint ("system-timeout",
							    NULL,
							    NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (GpmIdlePrivate));
}

/**
 * session_idle_changed_handler:
 * @is_idle: If the session is idle
 * @idle: This idle class instance
 *
 * The SessionIdleChanged callback from gnome-screensaver.
 **/
static void
session_idle_changed_handler (DBusGProxy *proxy,
			      gboolean	  is_idle,
			      GpmIdle	 *idle)
{
	GpmIdleMode mode;

	gpm_debug ("Received GS idle changed: %d", is_idle);

	if (is_idle) {
		mode = GPM_IDLE_MODE_SESSION;
	} else {
		mode = GPM_IDLE_MODE_NORMAL;
	}

	gpm_idle_set_mode (idle, mode);
}

/**
 * acquire_screensaver:
 * @idle: This idle class instance
 *
 * Aquires a connection to gnome-screensaver so we can get SessionIdleChanged
 * dubs events.
 *
 * Return value: If we could connect to gnome-screensaver.
 **/
static gboolean
acquire_screensaver (GpmIdle *idle)
{
	idle->priv->screensaver_object = dbus_g_proxy_new_for_name (idle->priv->connection,
								    GS_DBUS_SERVICE,
								    GS_DBUS_PATH,
								    GS_DBUS_INTERFACE);
	if (! idle->priv->screensaver_object) {
		gpm_warning ("Could not connect to screensaver");
		return FALSE;
	}

	dbus_g_proxy_add_signal (idle->priv->screensaver_object,
				 "SessionIdleChanged", G_TYPE_BOOLEAN, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (idle->priv->screensaver_object,
				     "SessionIdleChanged",
				     G_CALLBACK (session_idle_changed_handler),
				     idle,
				     NULL);

	return TRUE;
}

/**
 * gpm_idle_init:
 * @idle: This idle class instance
 *
 * Gets a DBUS connection, and aquires the screensaver connection so we can
 * get session changed events.
 *
 **/
static void
gpm_idle_init (GpmIdle *idle)
{
	GError *error = NULL;

	idle->priv = GPM_IDLE_GET_PRIVATE (idle);

	idle->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
	}

	idle->priv->mode = GPM_IDLE_MODE_NORMAL;
	idle->priv->system_timeout = 0;	/* in seconds */
	idle->priv->system_timer_id = 0;
	idle->priv->system_idle_timer_id = 0;
	idle->priv->check_type_cpu = FALSE;
	idle->priv->init = FALSE;

	acquire_screensaver (idle);
}

/**
 * gpm_idle_finalize:
 * @object: This idle class instance
 **/
static void
gpm_idle_finalize (GObject *object)
{
	GpmIdle *idle;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_IDLE (object));

	idle = GPM_IDLE (object);

	g_return_if_fail (idle->priv != NULL);

	remove_all_timers (idle);

	G_OBJECT_CLASS (gpm_idle_parent_class)->finalize (object);
}

/**
 * gpm_idle_new:
 * Return value: A new GpmIdle instance.
 **/
GpmIdle *
gpm_idle_new (void)
{
	GpmIdle *idle;
	idle = g_object_new (GPM_TYPE_IDLE, NULL);
	return GPM_IDLE (idle);
}

/**
 * gpm_idle_get_cpu_values:
 * @cpu_idle: The idle time reported by the CPU
 * @cpu_total: The total time reported by the CPU
 * Return value: Success of reading /proc/stat.
 **/
static gboolean
gpm_idle_get_cpu_values (long unsigned *cpu_idle, long unsigned *cpu_total)
{
	long unsigned user;
	long unsigned nice;
	long unsigned system;
	int len;
	char tmp[5];
	char str[80];
	FILE *fd;
	char *suc;

	fd = fopen("/proc/stat", "r");
	if (! fd) {
		return FALSE;
	}
	suc = fgets (str, 80, fd);
	len = sscanf (str, "%s %lu %lu %lu %lu", tmp,
		      &user, &nice, &system, cpu_idle);
	fclose (fd);
	/*
	 * Summing up all these times gives you the system uptime in jiffies.
	 * This is what the uptime command does.
	 */
	*cpu_total = user + nice + system + *cpu_idle;
	return TRUE;
}

/**
 * gpm_idle_compute_load:
 * @idle: This idle class instance
 * Return value: The CPU idle load
 **/
static gdouble
gpm_idle_compute_load (GpmIdle *idle)
{
	double	      percentage_load;
	long unsigned cpu_idle;
	long unsigned cpu_total;
	long unsigned diff_idle;
	long unsigned diff_total;

	/* fill "old" value manually */
	if (! idle->priv->init) {
		idle->priv->init = TRUE;
		if (!gpm_idle_get_cpu_values (&idle->priv->old_idle, &idle->priv->old_total))
		    gpm_warning ("Failed to read CPU values");
		return 0;
	}

	/* work out the differences */
	if (!gpm_idle_get_cpu_values (&cpu_idle, &cpu_total)) {
	    gpm_warning ("Failed to read CPU values");
	    return 0;
	}
	diff_idle = cpu_idle - idle->priv->old_idle;
	diff_total = cpu_total - idle->priv->old_total;

	/* If we divide the total time by idle time we get the load. */
	if (diff_idle > 0) {
		percentage_load = (double) diff_total / (double) diff_idle;
	} else {
		percentage_load = 100;
	}

	idle->priv->old_idle = cpu_idle;
	idle->priv->old_total = cpu_total;

	return percentage_load;
}
