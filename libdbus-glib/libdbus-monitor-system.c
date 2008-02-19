/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <libdbus-marshal.h>
#include <libdbus-monitor-system.h>

static void     dbus_monitor_system_class_init (DbusMonitorSystemClass *klass);
static void     dbus_monitor_system_init       (DbusMonitorSystem      *dbus_system_monitor);
static void     dbus_monitor_system_finalize   (GObject		       *object);

#define DBUS_MONITOR_SYSTEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DBUS_TYPE_MONITOR_SYSTEM, DbusMonitorSystemPrivate))

struct DbusMonitorSystemPrivate
{
	DBusGProxy	*proxy;
};

enum {
	NAME_OWNER_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer dbus_monitor_system = NULL;

G_DEFINE_TYPE (DbusMonitorSystem, dbus_monitor_system, G_TYPE_OBJECT)

static void
dbus_monitor_system_class_init (DbusMonitorSystemClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = dbus_monitor_system_finalize;
	g_type_class_add_private (klass, sizeof (DbusMonitorSystemPrivate));

	signals [NAME_OWNER_CHANGED] =
		g_signal_new ("name-owner-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DbusMonitorSystemClass, name_owner_changed),
			      NULL,
			      NULL,
			      libdbus_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

static void
name_owner_changed_cb (DBusGProxy     *proxy,
		       const gchar    *name,
		       const gchar    *prev,
		       const gchar    *new,
		       DbusMonitorSystem *monitor)
{
	g_signal_emit (monitor, signals [NAME_OWNER_CHANGED], 0, name, prev, new);
}

static void
dbus_monitor_system_init (DbusMonitorSystem *monitor)
{
	GError *error = NULL;
	DBusGConnection *connection;
	monitor->priv = DBUS_MONITOR_SYSTEM_GET_PRIVATE (monitor);

	/* connect to system manager */
	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		g_warning ("Cannot connect to system bus: %s", error->message);
		g_error_free (error);
		return;
	}
	monitor->priv->proxy = dbus_g_proxy_new_for_name_owner (connection,
								DBUS_SERVICE_DBUS,
								DBUS_PATH_DBUS,
						 		DBUS_INTERFACE_DBUS,
								&error);
	if (error) {
		g_warning ("Cannot connect to system manager: %s", error->message);
		g_error_free (error);
		return;
	}
	dbus_g_proxy_add_signal (monitor->priv->proxy, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->proxy, "NameOwnerChanged",
				     G_CALLBACK (name_owner_changed_cb),
				     monitor, NULL);
}

static void
dbus_monitor_system_finalize (GObject *object)
{
	DbusMonitorSystem *monitor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (DBUS_IS_MONITOR_SYSTEM (object));

	monitor = DBUS_MONITOR_SYSTEM (object);
	if (monitor->priv->proxy != NULL) {
		g_object_unref (monitor->priv->proxy);
	}
	G_OBJECT_CLASS (dbus_monitor_system_parent_class)->finalize (object);
}

DbusMonitorSystem *
dbus_monitor_system_new (void)
{
	if (dbus_monitor_system != NULL) {
		g_object_ref (dbus_monitor_system);
	} else {
		dbus_monitor_system = g_object_new (DBUS_TYPE_MONITOR_SYSTEM, NULL);
		g_object_add_weak_pointer (dbus_monitor_system, &dbus_monitor_system);
	}
	return DBUS_MONITOR_SYSTEM (dbus_monitor_system);
}

