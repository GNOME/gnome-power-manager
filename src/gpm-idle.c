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

static void     gpm_idle_class_init (GpmIdleClass *klass);
static void     gpm_idle_init       (GpmIdle      *idle);
static void     gpm_idle_finalize   (GObject      *object);

static gdouble  cpu_update_data     (GpmIdle      *idle);

#define GPM_IDLE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_IDLE, GpmIdlePrivate))

/*  How many seconds between polling? */
#define POLL_FREQUENCY	5

/*
 * Sets the idle percent limit, i.e. how hard the computer can work
 * while considered "at idle"
 */
#define IDLE_LIMIT	5

/* The cached cpu statistics used to work out the difference */
typedef struct {
	long unsigned user;	/**< The CPU user time			*/
	long unsigned nice;	/**< The CPU nice time			*/
	long unsigned system;	/**< The CPU system time		*/
	long unsigned idle;	/**< The CPU idle time			*/
	long unsigned total;	/**< The CPU total time (uptime)	*/
} cpudata;

struct GpmIdlePrivate
{
	DBusGConnection *connection;
	DBusGProxy	*screensaver_object;

	GpmIdleMode	 mode;

	guint		 system_timeout;	/* in seconds */
	guint		 system_timer_id;
	guint		 system_idle_timer_id;

	gboolean	 init;
	cpudata		 cpu_old;
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

#define GS_DBUS_SERVICE	  "org.gnome.ScreenSaver"
#define GS_DBUS_PATH	  "/org/gnome/ScreenSaver"
#define GS_DBUS_INTERFACE "org.gnome.ScreenSaver"

static GObjectClass *parent_class = NULL;
static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmIdle, gpm_idle, G_TYPE_OBJECT)

static gboolean
poll_system_timer (GpmIdle *idle)
{
	gdouble load;

	/* get our computed load value */
	load = cpu_update_data (idle);

	/* check if system is "idle" enough */

	/* FIXME: should this stay below this level for a certain time? */
	/* FIXME: check that we are on console? */

	if (load < IDLE_LIMIT) {
		gpm_debug ("Detected that the CPU is quiet");

		gpm_idle_set_mode (idle, GPM_IDLE_MODE_SYSTEM);

		idle->priv->system_idle_timer_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
add_poll_system_timer (GpmIdle *idle,
		       glong	timeout)
{
	idle->priv->system_idle_timer_id = g_timeout_add (timeout, (GSourceFunc)poll_system_timer, idle);
}

static void
remove_poll_system_timer (GpmIdle *idle)
{
	if (idle->priv->system_idle_timer_id != 0) {
		g_source_remove (idle->priv->system_idle_timer_id);
		idle->priv->system_idle_timer_id = 0;
	}
}

static gboolean
system_timer (GpmIdle *idle)
{
	gpm_debug ("System idle timeout");

	/* instead of doing the state transition directly
	 * we wait until the system is quiet */
	remove_poll_system_timer (idle);
	add_poll_system_timer (idle, POLL_FREQUENCY * 1000);

	idle->priv->system_timer_id = 0;
	return FALSE;
}

static void
remove_system_timer (GpmIdle *idle)
{
	if (idle->priv->system_timer_id != 0) {
		g_source_remove (idle->priv->system_timer_id);
		idle->priv->system_timer_id = 0;
	}
}

static void
remove_all_timers (GpmIdle *idle)
{
	remove_poll_system_timer (idle);
	remove_system_timer (idle);
}

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

GpmIdleMode
gpm_idle_get_mode (GpmIdle *idle)
{
	GpmIdleMode mode;

	mode = idle->priv->mode;

	return mode;
}

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

static void
gpm_idle_class_init (GpmIdleClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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

static gboolean
acquire_screensaver (GpmIdle *idle)
{
	idle->priv->screensaver_object = dbus_g_proxy_new_for_name (idle->priv->connection,
								    GS_DBUS_SERVICE,
								    GS_DBUS_PATH,
								    GS_DBUS_INTERFACE);
	if (! idle->priv->screensaver_object) {
		g_warning ("Could not connect to screensaver");
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


static void
gpm_idle_init (GpmIdle *idle)
{
	GError *error = NULL;

	idle->priv = GPM_IDLE_GET_PRIVATE (idle);

	idle->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		g_warning ("gpm_idle_init: %s", error->message);
		g_error_free (error);
	}

	acquire_screensaver (idle);
}

static void
gpm_idle_finalize (GObject *object)
{
	GpmIdle *idle;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_IDLE (object));

	idle = GPM_IDLE (object);

	g_return_if_fail (idle->priv != NULL);

	remove_all_timers (idle);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmIdle *
gpm_idle_new (void)
{
	GpmIdle *idle;

	idle = g_object_new (GPM_TYPE_IDLE, NULL);

	return GPM_IDLE (idle);
}


/** Gets the raw CPU values from /proc/stat
 *
 *  @param	data		An empty, pre-allocated CPU object
 *  @return			If we can read the /proc/stat file
 *
 * @note	- user		Time spent in user space (for all processes)
 *		- nice		Time spent in niced tasks
				(tasks with positive nice value)
 *		- system	Time spent in Kernel space
 *		- idle		Time the processor was not busy.
 */
static gboolean
cpudata_get_values (cpudata *data)
{
	int len;
	char tmp[5];
	char str[80];
	FILE *fd;
	char *suc;

	/* assertion checks */
	g_assert (data);

	fd = fopen("/proc/stat", "r");
	if (!fd)
		return FALSE;
	suc = fgets (str, 80, fd);
	len = sscanf (str, "%s %lu %lu %lu %lu", tmp,
		      &data->user, &data->nice, &data->system, &data->idle);
	fclose (fd);
	/*
	 * Summing up all these times gives you the system uptime in jiffies.
	 * This is what the uptime command does.
	 */
	data->total = data->user + data->nice + data->system + data->idle;
	return TRUE;
}

/** Diff two CPU structures
 *
 *  @param	data1		The newer CPU data struct
 *  @param	data2		The older CPU data struct
 *  @param	diff		The difference CPU data struct
 */
static void
cpudata_diff (cpudata *data2, cpudata *data1, cpudata *diff)
{
	/* assertion checks */
	g_assert (data1);
	g_assert (data2);
	g_assert (diff);

	diff->user = data1->user - data2->user;
	diff->nice = data1->nice - data2->nice;
	diff->system = data1->system - data2->system;
	diff->idle = data1->idle - data2->idle;
	diff->total = data1->total - data2->total;
}

/** Diff two CPU structures
 *
 *  @param	to		The new CPU data struct
 *  @param	from		The old CPU data struct
 */
static void
cpudata_copy (cpudata *to, cpudata *from)
{
	/* assertion checks */
	g_assert (to);
	g_assert (from);

	to->user = from->user;
	to->nice = from->nice;
	to->system = from->system;
	to->idle = from->idle;
	to->total = from->total;
}

#if 0
/** Normalise a cpudata structure to Hz.
 *
 *  @param	data		The CPU data struct
 *
 *  @note	This precision is required because 100 / POLL_FREQUENCY may
 *		be non-integer.
 *		Also, we cannot assume 100 / POLL_FREQUENCY == total, as we
 *		may have taken longer to do the read than we had planned.
 */
static void
cpudata_normalize (cpudata *data)
{
	/* assertion checks */
	g_assert (data);

	double factor = 100.0 / (double) data->total;
	data->user = (double) data->user * factor;
	data->nice = (double) data->nice * factor;
	data->system = (double) data->system * factor;
	data->idle = (double) data->idle * factor;
}
#endif

/** Returns the CPU's load
 *
 *  @return			The CPU load
 */
static gdouble
cpu_update_data (GpmIdle *idle)
{
	cpudata new;
	cpudata diff;
	double	loadpercentage;

	/* fill "old" value manually */
	if (! idle->priv->init) {
		idle->priv->init = TRUE;
		cpudata_get_values (&idle->priv->cpu_old);
		return 0;
	}

	/* work out the differences */
	cpudata_get_values (&new);
	cpudata_diff (&idle->priv->cpu_old, &new, &diff);

	/* Copy into old */
	cpudata_copy (&idle->priv->cpu_old, &new);

	/* If we divide the total time by idle time we get the load. */
	if (diff.idle > 0)
		loadpercentage = (double) diff.total / (double) diff.idle;
	else
		loadpercentage = 100;

	return loadpercentage;
}
