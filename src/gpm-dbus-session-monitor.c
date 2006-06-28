/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-marshal.h"
#include "gpm-debug.h"

#include "gpm-dbus-session-monitor.h"

static void     gpm_dbus_session_monitor_class_init (GpmDbusSessionMonitorClass *klass);
static void     gpm_dbus_session_monitor_init       (GpmDbusSessionMonitor      *dbus_session_monitor);
static void     gpm_dbus_session_monitor_finalize   (GObject		 	*object);

#define GPM_DBUS_SESSION_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_DBUS_SESSION_MONITOR, GpmDbusSessionMonitorPrivate))

struct GpmDbusSessionMonitorPrivate
{
	DBusGConnection *connection;
	DBusGProxy	*proxy;
};

enum {
	NAME_OWNER_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };
static gpointer      gpm_session_monitor_object = NULL;

G_DEFINE_TYPE (GpmDbusSessionMonitor, gpm_dbus_session_monitor, G_TYPE_OBJECT)

static void
gpm_dbus_session_monitor_class_init (GpmDbusSessionMonitorClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_dbus_session_monitor_finalize;
	g_type_class_add_private (klass, sizeof (GpmDbusSessionMonitorPrivate));

	signals [NAME_OWNER_CHANGED] =
		g_signal_new ("name-owner-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDbusSessionMonitorClass, dbus_name_owner_changed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

static void
dbus_name_owner_changed (DBusGProxy	*proxy,
			 const char	*name,
			 const char	*prev,
			 const char	*new,
			 GpmDbusSessionMonitor	*monitor)
{
	gpm_debug ("emitting name-owner-changed : %s (%s->%s)", name, prev, new);
	g_signal_emit (monitor, signals [NAME_OWNER_CHANGED], 0, name, prev, new);
}

static void
gpm_dbus_session_monitor_init (GpmDbusSessionMonitor *monitor)
{
	GError *error = NULL;
	monitor->priv = GPM_DBUS_SESSION_MONITOR_GET_PRIVATE (monitor);

	monitor->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	/* FIXME: check errors */
	monitor->priv->proxy = dbus_g_proxy_new_for_name_owner (monitor->priv->connection,
								DBUS_SERVICE_DBUS,
								DBUS_PATH_DBUS,
						 		DBUS_INTERFACE_DBUS,
								&error);
	dbus_g_proxy_add_signal (monitor->priv->proxy, "NameOwnerChanged",
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->proxy, "NameOwnerChanged",
					    G_CALLBACK (dbus_name_owner_changed),
					    monitor, NULL);
}

static void
gpm_dbus_session_monitor_finalize (GObject *object)
{
	GpmDbusSessionMonitor *monitor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_DBUS_SESSION_MONITOR (object));

	monitor = GPM_DBUS_SESSION_MONITOR (object);

	if (monitor->priv->proxy) {
		g_object_unref (monitor->priv->proxy);
		monitor->priv->proxy = NULL;
	}
	G_OBJECT_CLASS (gpm_dbus_session_monitor_parent_class)->finalize (object);
}

GpmDbusSessionMonitor *
gpm_dbus_session_monitor_new (void)
{
	if (gpm_session_monitor_object) {
		g_object_ref (gpm_session_monitor_object);
	} else {
		gpm_session_monitor_object = g_object_new (GPM_TYPE_DBUS_SESSION_MONITOR, NULL);
		g_object_add_weak_pointer (gpm_session_monitor_object,
					   (gpointer *) &gpm_session_monitor_object);
	}
	return GPM_DBUS_SESSION_MONITOR (gpm_session_monitor_object);
}
