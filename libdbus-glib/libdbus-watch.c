/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <libdbus-watch.h>
#include <libdbus-monitor-session.h>
#include <libdbus-monitor-system.h>

static void     dbus_watch_class_init (DbusWatchClass *klass);
static void     dbus_watch_init       (DbusWatch      *watch);
static void     dbus_watch_finalize   (GObject        *object);

#define DBUS_WATCH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DBUS_TYPE_WATCH, DbusWatchPrivate))

/* this is a watch that handles messagebus and DBUS service restarts. */
struct DbusWatchPrivate
{
	DbusWatchType		 bus_type;
	gchar			*service;
	DbusMonitorSession	*monitor_session;
	DbusMonitorSystem	*monitor_system;
	gboolean		 assigned;
	gulong			 ses_sig_id;
	gulong			 sys_sig_id;
};

enum {
	CONNECTION_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DbusWatch, dbus_watch, G_TYPE_OBJECT)

/**
 * dbus_noc_session_cb:
 * @power: The power class instance
 * @name: The DBUS name, e.g. hal.freedesktop.org
 * @prev: The previous name, e.g. :0.13
 * @new: The new name, e.g. :0.14
 * @inhibit: This inhibit class instance
 *
 * The noc session DBUS callback.
 **/
static void
dbus_noc_session_cb (DbusMonitorSession *monitor_session,
		     const gchar    *name,
		     const gchar    *prev,
		     const gchar    *new,
		     DbusWatch	    *dbus_watch)
{
	g_return_if_fail (DBUS_IS_WATCH (dbus_watch));
	if (dbus_watch->priv->assigned == FALSE) {
		return;
	}
	if (dbus_watch->priv->bus_type == DBUS_WATCH_SYSTEM) {
		return;
	}

	if (strcmp (name, dbus_watch->priv->service) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0) {
			g_debug ("emitting connection-changed FALSE: %s", dbus_watch->priv->service);
			g_signal_emit (dbus_watch, signals [CONNECTION_CHANGED], 0, FALSE);
		}
		if (strlen (prev) == 0 && strlen (new) != 0) {
			g_debug ("emitting connection-changed TRUE: %s", dbus_watch->priv->service);
			g_signal_emit (dbus_watch, signals [CONNECTION_CHANGED], 0, TRUE);
		}
	}
}

/**
 * dbus_noc_system_cb:
 * @power: The power class instance
 * @name: The DBUS name, e.g. hal.freedesktop.org
 * @prev: The previous name, e.g. :0.13
 * @new: The new name, e.g. :0.14
 * @inhibit: This inhibit class instance
 *
 * The noc session DBUS callback.
 **/
static void
dbus_noc_system_cb (DbusMonitorSystem *monitor_system,
		    const gchar	   *name,
		    const gchar    *prev,
		    const gchar    *new,
		    DbusWatch	   *dbus_watch)
{
	g_return_if_fail (DBUS_IS_WATCH (dbus_watch));
	if (dbus_watch->priv->assigned == FALSE) {
		return;
	}
	if (dbus_watch->priv->bus_type == DBUS_WATCH_SESSION) {
		return;
	}

	if (strcmp (name, dbus_watch->priv->service) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0) {
			g_debug ("emitting connection-changed FALSE: %s", dbus_watch->priv->service);
			g_signal_emit (dbus_watch, signals [CONNECTION_CHANGED], 0, FALSE);
		}
		if (strlen (prev) == 0 && strlen (new) != 0) {
			g_debug ("emitting connection-changed TRUE: %s", dbus_watch->priv->service);
			g_signal_emit (dbus_watch, signals [CONNECTION_CHANGED], 0, TRUE);
		}
	}
}

/**
 * dbus_watch_assign:
 * @dbus_watch: This class instance
 * @bus_type: The bus type, either DBUS_WATCH_SESSION or DBUS_WATCH_SYSTEM
 * @service: The DBUS service name
 * Return value: success
 **/
gboolean
dbus_watch_assign (DbusWatch   *dbus_watch,
		  DbusWatchType bus_type,
		  const gchar  *service)
{
	g_return_val_if_fail (DBUS_IS_WATCH (dbus_watch), FALSE);
	g_return_val_if_fail (service != NULL, FALSE);

	if (dbus_watch->priv->assigned) {
		g_warning ("already assigned watch!");
		return FALSE;
	}

	dbus_watch->priv->service = g_strdup (service);
	dbus_watch->priv->bus_type = bus_type;
	dbus_watch->priv->assigned = TRUE;

	/* We have to save the connection and remove the signal id later as
	   instances of this object are likely to be registering with a
	   singleton object many times */
	if (bus_type == DBUS_WATCH_SESSION) {
		dbus_watch->priv->monitor_session = dbus_monitor_session_new ();
		dbus_watch->priv->ses_sig_id = g_signal_connect (dbus_watch->priv->monitor_session,
								 "name-owner-changed",
								 G_CALLBACK (dbus_noc_session_cb),
								 dbus_watch);
	} else {
		dbus_watch->priv->monitor_system = dbus_monitor_system_new ();
		dbus_watch->priv->sys_sig_id = g_signal_connect (dbus_watch->priv->monitor_system,
								 "name-owner-changed",
								 G_CALLBACK (dbus_noc_system_cb),
								 dbus_watch);
	}

	return TRUE;
}

/**
 * dbus_watch_is_connected:
 * @dbus_watch: This class instance
 * Return value: if we are connected to a valid watch
 **/
gboolean
dbus_watch_is_connected (DbusWatch *dbus_watch)
{
	g_return_val_if_fail (DBUS_IS_WATCH (dbus_watch), FALSE);
	return dbus_watch->priv->assigned;
}

/**
 * dbus_watch_class_init:
 * @dbus_watch: This class instance
 **/
static void
dbus_watch_class_init (DbusWatchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dbus_watch_finalize;
	g_type_class_add_private (klass, sizeof (DbusWatchPrivate));

	signals [CONNECTION_CHANGED] =
		g_signal_new ("connection-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DbusWatchClass, connection_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/**
 * dbus_watch_init:
 * @dbus_watch: This class instance
 **/
static void
dbus_watch_init (DbusWatch *dbus_watch)
{
	dbus_watch->priv = DBUS_WATCH_GET_PRIVATE (dbus_watch);

	dbus_watch->priv->service = NULL;
	dbus_watch->priv->bus_type = DBUS_WATCH_SESSION;
	dbus_watch->priv->assigned = FALSE;
	dbus_watch->priv->monitor_session = NULL;
	dbus_watch->priv->monitor_system = NULL;
	dbus_watch->priv->ses_sig_id = 0;
	dbus_watch->priv->sys_sig_id = 0;
}

/**
 * dbus_watch_finalize:
 * @object: This class instance
 **/
static void
dbus_watch_finalize (GObject *object)
{
	DbusWatch *dbus_watch;
	g_return_if_fail (object != NULL);
	g_return_if_fail (DBUS_IS_WATCH (object));

	dbus_watch = DBUS_WATCH (object);
	dbus_watch->priv = DBUS_WATCH_GET_PRIVATE (dbus_watch);

	if (dbus_watch->priv->ses_sig_id != 0) {
		g_signal_handler_disconnect (dbus_watch->priv->monitor_session, dbus_watch->priv->ses_sig_id);
	}
	if (dbus_watch->priv->sys_sig_id != 0) {
		g_signal_handler_disconnect (dbus_watch->priv->monitor_system, dbus_watch->priv->sys_sig_id);
	}

	if (dbus_watch->priv->monitor_session != NULL) {
		g_object_unref (dbus_watch->priv->monitor_session);
	}
	if (dbus_watch->priv->monitor_system != NULL) {
		g_object_unref (dbus_watch->priv->monitor_system);
	}
	if (dbus_watch->priv->service != NULL) {
		g_free (dbus_watch->priv->service);
	}
	G_OBJECT_CLASS (dbus_watch_parent_class)->finalize (object);
}

/**
 * dbus_watch_new:
 * Return value: new class instance.
 **/
DbusWatch *
dbus_watch_new (void)
{
	DbusWatch *dbus_watch;
	dbus_watch = g_object_new (DBUS_TYPE_WATCH, NULL);
	return DBUS_WATCH (dbus_watch);
}

