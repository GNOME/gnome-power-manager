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

#include "gpm-dbus-monitor.h"

static void     gpm_dbus_monitor_class_init (GpmDbusMonitorClass *klass);
static void     gpm_dbus_monitor_init       (GpmDbusMonitor      *dbus_monitor);
static void     gpm_dbus_monitor_finalize   (GObject	     *object);

#define GPM_DBUS_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_DBUS_MONITOR, GpmDbusMonitorPrivate))

struct GpmDbusMonitorPrivate
{
	DBusGConnection *system_connection;
	DBusGConnection *session_connection;
	DBusGProxy	*system_proxy;
	DBusGProxy	*session_proxy;
};

enum {
	NAME_OWNER_CHANGED_SESSION,
	NAME_OWNER_CHANGED_SYSTEM,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmDbusMonitor, gpm_dbus_monitor, G_TYPE_OBJECT)

static void
gpm_dbus_monitor_class_init (GpmDbusMonitorClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize	   = gpm_dbus_monitor_finalize;

	g_type_class_add_private (klass, sizeof (GpmDbusMonitorPrivate));

	signals [NAME_OWNER_CHANGED_SYSTEM] =
		g_signal_new ("name-owner-changed-system",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDbusMonitorClass, dbus_name_owner_changed_system),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	signals [NAME_OWNER_CHANGED_SESSION] =
		g_signal_new ("name-owner-changed-session",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDbusMonitorClass, dbus_name_owner_changed_session),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

static void
dbus_name_owner_changed_system (DBusGProxy	*system_proxy,
				const char	*name,
				const char	*prev,
				const char	*new,
				GpmDbusMonitor	*monitor)
{
	gpm_debug ("emitting name-owner-changed-system : %s (%s->%s)", name, prev, new);
	g_signal_emit (monitor, signals [NAME_OWNER_CHANGED_SYSTEM], 0, name, prev, new);
}

static void
dbus_name_owner_changed_session (DBusGProxy	*session_proxy,
				 const char	*name,
				 const char	*prev,
				 const char	*new,
				 GpmDbusMonitor	*monitor)
{
	gpm_debug ("emitting name-owner-changed-session : %s (%s->%s)", name, prev, new);
	g_signal_emit (monitor, signals [NAME_OWNER_CHANGED_SESSION], 0, name, prev, new);
}

static void
gpm_dbus_monitor_init (GpmDbusMonitor *monitor)
{
	GError *error = NULL;

	monitor->priv = GPM_DBUS_MONITOR_GET_PRIVATE (monitor);

	/* session connection */
	monitor->priv->system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	/* FIXME: check errors */
	monitor->priv->system_proxy = dbus_g_proxy_new_for_name_owner (monitor->priv->system_connection,
								DBUS_SERVICE_DBUS,
								DBUS_PATH_DBUS,
						 		DBUS_INTERFACE_DBUS,
								&error);
	dbus_g_proxy_add_signal (monitor->priv->system_proxy, "NameOwnerChanged",
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->system_proxy, "NameOwnerChanged",
					    G_CALLBACK (dbus_name_owner_changed_system),
					    monitor, NULL);

	/* session connection */
	monitor->priv->session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	/* FIXME: check errors */
	monitor->priv->session_proxy = dbus_g_proxy_new_for_name_owner (monitor->priv->session_connection,
								DBUS_SERVICE_DBUS,
								DBUS_PATH_DBUS,
						 		DBUS_INTERFACE_DBUS,
								&error);
	dbus_g_proxy_add_signal (monitor->priv->session_proxy, "NameOwnerChanged",
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->session_proxy, "NameOwnerChanged",
					    G_CALLBACK (dbus_name_owner_changed_session),
					    monitor, NULL);

}

static void
gpm_dbus_monitor_finalize (GObject *object)
{
	GpmDbusMonitor *monitor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_DBUS_MONITOR (object));

	monitor = GPM_DBUS_MONITOR (object);

	g_return_if_fail (monitor->priv != NULL);

	if (monitor->priv->system_proxy) {
		g_object_unref (monitor->priv->system_proxy);
		monitor->priv->system_proxy = NULL;
	}
	if (monitor->priv->session_proxy) {
		g_object_unref (monitor->priv->session_proxy);
		monitor->priv->session_proxy = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmDbusMonitor *
gpm_dbus_monitor_new (void)
{
	return GPM_DBUS_MONITOR (g_object_new (GPM_TYPE_DBUS_MONITOR, NULL));
}
