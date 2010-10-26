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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include "gpm-idletime.h"

#include "gpm-idle.h"
#include "gpm-load.h"

#define GPM_IDLE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_IDLE, GpmIdlePrivate))

/* Sets the idle percent limit, i.e. how hard the computer can work
   while considered "at idle" */
#define GPM_IDLE_CPU_LIMIT			5
#define	GPM_IDLE_IDLETIME_ID			1

struct GpmIdlePrivate
{
	GpmIdletime		*idletime;
	GpmLoad			*load;
	GDBusProxy		*proxy;
	GDBusProxy		*proxy_presence;
	GpmIdleMode		 mode;
	guint			 timeout_dim;		/* in seconds */
	guint			 timeout_blank;		/* in seconds */
	guint			 timeout_sleep;		/* in seconds */
	guint			 timeout_blank_id;
	guint			 timeout_sleep_id;
	gboolean		 x_idle;
	gboolean		 check_type_cpu;
};

enum {
	IDLE_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_idle_object = NULL;

G_DEFINE_TYPE (GpmIdle, gpm_idle, G_TYPE_OBJECT)

/**
 * gpm_idle_mode_to_string:
 **/
const gchar *
gpm_idle_mode_to_string (GpmIdleMode mode)
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
 * @mode: The new mode, e.g. GPM_IDLE_MODE_SLEEP
 **/
static void
gpm_idle_set_mode (GpmIdle *idle, GpmIdleMode mode)
{
	g_return_if_fail (GPM_IS_IDLE (idle));

	if (mode != idle->priv->mode) {
		idle->priv->mode = mode;
		g_debug ("Doing a state transition: %s", gpm_idle_mode_to_string (mode));
		g_signal_emit (idle, signals [IDLE_CHANGED], 0, mode);
	}
}

/**
 * gpm_idle_set_check_cpu:
 * @check_type_cpu: If we should check the CPU before mode becomes
 *		    GPM_IDLE_MODE_SLEEP and the event is done.
 **/
void
gpm_idle_set_check_cpu (GpmIdle *idle, gboolean check_type_cpu)
{
	g_return_if_fail (GPM_IS_IDLE (idle));
	g_debug ("Setting the CPU load check to %i", check_type_cpu);
	idle->priv->check_type_cpu = check_type_cpu;
}

/**
 * gpm_idle_get_mode:
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
		g_debug ("ignoring current mode %s", gpm_idle_mode_to_string (idle->priv->mode));
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
		if (load > GPM_IDLE_CPU_LIMIT) {
			/* check if system is "idle" enough */
			g_debug ("Detected that the CPU is busy");
			ret = TRUE;
			goto out;
		}
	}
	gpm_idle_set_mode (idle, GPM_IDLE_MODE_SLEEP);
out:
	return ret;
}

typedef enum {
	GPM_IDLE_STATUS_ENUM_AVAILABLE = 0,
	GPM_IDLE_STATUS_ENUM_INVISIBLE,
	GPM_IDLE_STATUS_ENUM_BUSY,
	GPM_IDLE_STATUS_ENUM_IDLE,
	GPM_IDLE_STATUS_ENUM_UNKNOWN
} GpmIdleStatusEnum;

/**
 * gpm_idle_is_session_idle:
 **/
static gboolean
gpm_idle_is_session_idle (GpmIdle *idle)
{
	gboolean ret = FALSE;
	GVariant *result;
	guint status;

	/* get the session status */
	result = g_dbus_proxy_get_cached_property (idle->priv->proxy_presence, "status");
	if (result == NULL)
		goto out;

	g_variant_get (result, "u", &status);
	ret = (status == GPM_IDLE_STATUS_ENUM_IDLE);
	g_variant_unref (result);
out:
	return ret;
}

typedef enum {
	GPM_IDLE_INHIBIT_MASK_LOGOUT = 1,
	GPM_IDLE_INHIBIT_MASK_SWITCH = 2,
	GPM_IDLE_INHIBIT_MASK_SUSPEND = 4,
	GPM_IDLE_INHIBIT_MASK_IDLE = 8
} GpmIdleInhibitMask;

/**
 * gpm_idle_is_session_inhibited:
 **/
static gboolean
gpm_idle_is_session_inhibited (GpmIdle *idle, guint mask)
{
	gboolean ret = FALSE;
	GVariant *retval = NULL;
	GError *error = NULL;

	retval = g_dbus_proxy_call_sync (idle->priv->proxy,
					 "IsInhibited",
					 g_variant_new ("(u)",
							mask),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1, NULL,
					 &error);
	if (retval == NULL) {
		/* abort as the DBUS method failed */
		g_warning ("IsInhibited failed: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_variant_get (retval, "(b)", &ret);
out:
	if (retval != NULL)
		g_variant_unref (retval);
	return ret;
}

/**
 * gpm_idle_evaluate:
 **/
static void
gpm_idle_evaluate (GpmIdle *idle)
{
	gboolean is_idle;
	gboolean is_idle_inhibited;
	gboolean is_suspend_inhibited;

	/* check we are really idle */
	if (!idle->priv->x_idle) {
		gpm_idle_set_mode (idle, GPM_IDLE_MODE_NORMAL);
		g_debug ("X not idle");
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

	/* are we inhibited from going idle */
	is_idle_inhibited = gpm_idle_is_session_inhibited (idle, GPM_IDLE_INHIBIT_MASK_IDLE);
	if (is_idle_inhibited) {
		g_debug ("inhibited, so using normal state");
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
		g_debug ("normal to dim");
		gpm_idle_set_mode (idle, GPM_IDLE_MODE_DIM);
	}

	/* set up blank callback even when session is not idle,
	 * but only if we actually want to blank. */
	if (idle->priv->timeout_blank_id == 0 &&
	    idle->priv->timeout_blank != 0) {
		g_debug ("setting up blank callback for %is", idle->priv->timeout_blank);
		idle->priv->timeout_blank_id = g_timeout_add_seconds (idle->priv->timeout_blank,
								      (GSourceFunc) gpm_idle_blank_cb, idle);
		g_source_set_name_by_id (idle->priv->timeout_blank_id, "[GpmIdle] blank");
	}

	/* are we inhibited from sleeping */
	is_idle = gpm_idle_is_session_idle (idle);
	is_suspend_inhibited = gpm_idle_is_session_inhibited (idle, GPM_IDLE_INHIBIT_MASK_SUSPEND);
	if (is_suspend_inhibited) {
		g_debug ("suspend inhibited");
		if (idle->priv->timeout_sleep_id != 0) {
			g_source_remove (idle->priv->timeout_sleep_id);
			idle->priv->timeout_sleep_id = 0;
		}
	} else if (is_idle) {
		/* only do the sleep timeout when the session is idle and we aren't inhibited from sleeping */
		if (idle->priv->timeout_sleep_id == 0 &&
		    idle->priv->timeout_sleep != 0) {
			g_debug ("setting up sleep callback %is", idle->priv->timeout_sleep);
			idle->priv->timeout_sleep_id = g_timeout_add_seconds (idle->priv->timeout_sleep,
									      (GSourceFunc) gpm_idle_sleep_cb, idle);
			g_source_set_name_by_id (idle->priv->timeout_sleep_id, "[GpmIdle] sleep");
		}
	}
out:
	return;
}

/**
 *  gpm_idle_adjust_timeout_dim:
 *  @idle_time: The new timeout we want to set, in seconds.
 *  @timeout: Current idle time, in seconds.
 *
 *  On slow machines, or machines that have lots to load duing login,
 *  the current idle time could be bigger than the requested timeout.
 *  In this case the scheduled idle timeout will never fire, unless
 *  some user activity (keyboard, mouse) resets the current idle time.
 *  Instead of relying on user activity to correct this issue, we need
 *  to adjust timeout, as related to current idle time, so the idle
 *  timeout will fire as designed.
 *
 *  Return value: timeout to set, adjusted acccording to current idle time.
 **/
static guint
gpm_idle_adjust_timeout_dim (guint idle_time, guint timeout)
{
	/* allow 2 sec margin for messaging delay. */
	idle_time += 2;

	/* Double timeout until it's larger than current idle time.
	 * Give up for ultra slow machines. (86400 sec = 24 hours) */
	while (timeout < idle_time && timeout < 86400 && timeout > 0) {
		timeout *= 2;
	}
	return timeout;
}

/**
 * gpm_idle_set_timeout_dim:
 * @timeout: The new timeout we want to set, in seconds
 **/
gboolean
gpm_idle_set_timeout_dim (GpmIdle *idle, guint timeout)
{
	gint64 idle_time_in_msec;
	guint timeout_adjusted;

	g_return_val_if_fail (GPM_IS_IDLE (idle), FALSE);

	idle_time_in_msec = gpm_idletime_get_time (idle->priv->idletime);
	timeout_adjusted  = gpm_idle_adjust_timeout_dim (idle_time_in_msec / 1000, timeout);
	g_debug ("Current idle time=%lldms, timeout was %us, becomes %us after adjustment",
		   (long long int)idle_time_in_msec, timeout, timeout_adjusted);
	timeout = timeout_adjusted;

	g_debug ("Setting dim idle timeout: %ds", timeout);
	if (idle->priv->timeout_dim != timeout) {
		idle->priv->timeout_dim = timeout;

		if (timeout > 0)
			gpm_idletime_alarm_set (idle->priv->idletime, GPM_IDLE_IDLETIME_ID, timeout * 1000);
		else
			gpm_idletime_alarm_remove (idle->priv->idletime, GPM_IDLE_IDLETIME_ID);
	}
	return TRUE;
}

/**
 * gpm_idle_set_timeout_blank:
 * @timeout: The new timeout we want to set, in seconds
 **/
gboolean
gpm_idle_set_timeout_blank (GpmIdle *idle, guint timeout)
{
	g_return_val_if_fail (GPM_IS_IDLE (idle), FALSE);

	g_debug ("Setting blank idle timeout: %ds", timeout);
	if (idle->priv->timeout_blank != timeout) {
		idle->priv->timeout_blank = timeout;
		gpm_idle_evaluate (idle);
	}
	return TRUE;
}

/**
 * gpm_idle_set_timeout_sleep:
 * @timeout: The new timeout we want to set, in seconds
 **/
gboolean
gpm_idle_set_timeout_sleep (GpmIdle *idle, guint timeout)
{
	g_return_val_if_fail (GPM_IS_IDLE (idle), FALSE);

	g_debug ("Setting sleep idle timeout: %ds", timeout);
	if (idle->priv->timeout_sleep != timeout) {
		idle->priv->timeout_sleep = timeout;
		gpm_idle_evaluate (idle);
	}
	return TRUE;
}

/**
 * gpm_idle_idletime_alarm_expired_cb:
 *
 * We're idle, something timed out
 **/
static void
gpm_idle_idletime_alarm_expired_cb (GpmIdletime *idletime, guint alarm_id, GpmIdle *idle)
{
	g_debug ("idletime alarm: %i", alarm_id);

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
gpm_idle_idletime_reset_cb (GpmIdletime *idletime, GpmIdle *idle)
{
	g_debug ("idletime reset");

	idle->priv->x_idle = FALSE;
	gpm_idle_evaluate (idle);
}

/**
 * gpm_idle_dbus_signal_cb:
 **/
static void
gpm_idle_dbus_signal_cb (GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer user_data)
{
	GpmIdle *idle = GPM_IDLE (user_data);

	if (g_strcmp0 (signal_name, "InhibitorAdded") == 0 ||
	    g_strcmp0 (signal_name, "InhibitorRemoved") == 0) {
		g_debug ("Received gnome session inhibitor change");
		gpm_idle_evaluate (idle);
		return;
	}
	if (g_strcmp0 (signal_name, "StatusChanged") == 0) {
		g_debug ("Received gnome session status change");
		gpm_idle_evaluate (idle);
		return;
	}
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

	g_object_unref (idle->priv->load);
	if (idle->priv->proxy != NULL)
		g_object_unref (idle->priv->proxy);
	if (idle->priv->proxy_presence != NULL)
		g_object_unref (idle->priv->proxy_presence);

	gpm_idletime_alarm_remove (idle->priv->idletime, GPM_IDLE_IDLETIME_ID);
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
 *
 * Gets a DBUS connection, and aquires the session connection so we can
 * get session changed events.
 *
 **/
static void
gpm_idle_init (GpmIdle *idle)
{
	GDBusConnection *connection;
	GError *error = NULL;

	idle->priv = GPM_IDLE_GET_PRIVATE (idle);

	idle->priv->timeout_dim = G_MAXUINT;
	idle->priv->timeout_blank = G_MAXUINT;
	idle->priv->timeout_sleep = G_MAXUINT;
	idle->priv->timeout_blank_id = 0;
	idle->priv->timeout_sleep_id = 0;
	idle->priv->x_idle = FALSE;
	idle->priv->load = gpm_load_new ();

	idle->priv->idletime = gpm_idletime_new ();
	g_signal_connect (idle->priv->idletime, "reset", G_CALLBACK (gpm_idle_idletime_reset_cb), idle);
	g_signal_connect (idle->priv->idletime, "alarm-expired", G_CALLBACK (gpm_idle_idletime_alarm_expired_cb), idle);

	/* get connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		g_warning ("Failed to get session connection: %s", error->message);
		g_error_free (error);
		return;
	}

	/* get org.gnome.Session main interface */
	idle->priv->proxy =
		g_dbus_proxy_new_sync (connection,
			0, NULL,
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager",
			"org.gnome.SessionManager",
			NULL, &error);
	if (idle->priv->proxy == NULL) {
		g_warning ("Cannot connect to session manager: %s", error->message);
		g_error_free (error);
		return;
	}
	g_signal_connect (idle->priv->proxy, "g-signal", G_CALLBACK (gpm_idle_dbus_signal_cb), idle);

	/* get org.gnome.Session.Presence interface */
	idle->priv->proxy_presence =
		g_dbus_proxy_new_sync (connection,
			0, NULL,
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager/Presence",
			"org.gnome.SessionManager.Presence",
			NULL, &error);
	if (idle->priv->proxy_presence == NULL) {
		g_warning ("Cannot connect to session manager: %s", error->message);
		g_error_free (error);
		return;
	}
	g_signal_connect (idle->priv->proxy_presence, "g-signal", G_CALLBACK (gpm_idle_dbus_signal_cb), idle);

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

