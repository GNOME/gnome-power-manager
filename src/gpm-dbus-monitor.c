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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-marshal.h"
#include "gpm-debug.h"

#include "gpm-dbus-monitor.h"

static void     gpm_dbus_monitor_class_init (GpmDbusMonitorClass *klass);
static void     gpm_dbus_monitor_init       (GpmDbusMonitor      *dbus_session_monitor);
static void     gpm_dbus_monitor_finalize   (GObject		 	*object);

#define GPM_DBUS_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_DBUS_MONITOR, GpmDbusMonitorPrivate))

struct GpmDbusMonitorPrivate
{
	DBusGProxy	*proxy_ses;
	DBusGProxy	*proxy_sys;
};

enum {
	NOC_SESSON,
	NOC_SYSTEM,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmDbusMonitor, gpm_dbus_monitor, G_TYPE_OBJECT)

static void
gpm_dbus_monitor_class_init (GpmDbusMonitorClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_dbus_monitor_finalize;
	g_type_class_add_private (klass, sizeof (GpmDbusMonitorPrivate));

	signals [NOC_SESSON] =
		g_signal_new ("noc-session",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDbusMonitorClass, dbus_noc_session),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	signals [NOC_SYSTEM] =
		g_signal_new ("noc-system",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDbusMonitorClass, dbus_noc_system),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

static void
dbus_noc_session_cb (DBusGProxy     *proxy,
		     const gchar    *name,
		     const gchar    *prev,
		     const gchar    *new,
		     GpmDbusMonitor *monitor)
{
	g_signal_emit (monitor, signals [NOC_SESSON], 0, name, prev, new);
}

static void
dbus_noc_system_cb (DBusGProxy     *proxy,
		    const gchar    *name,
		    const gchar    *prev,
		    const gchar    *new,
		    GpmDbusMonitor *monitor)
{
	g_signal_emit (monitor, signals [NOC_SYSTEM], 0, name, prev, new);
}

static void
gpm_dbus_monitor_init (GpmDbusMonitor *monitor)
{
	GError *error = NULL;
	DBusGConnection *connection;
	monitor->priv = GPM_DBUS_MONITOR_GET_PRIVATE (monitor);

	/* connect to session manager */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		gpm_warning ("Cannot connect to session bus: %s", error->message);
		g_error_free (error);
		return;
	}
	monitor->priv->proxy_ses = dbus_g_proxy_new_for_name_owner (connection,
								DBUS_SERVICE_DBUS,
								DBUS_PATH_DBUS,
						 		DBUS_INTERFACE_DBUS,
								&error);
	if (error) {
		gpm_warning ("Cannot connect to session manager: %s", error->message);
		g_error_free (error);
		return;
	}
	dbus_g_proxy_add_signal (monitor->priv->proxy_ses, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->proxy_ses, "NameOwnerChanged",
				     G_CALLBACK (dbus_noc_session_cb),
				     monitor, NULL);

	/* connect to system manager */
	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		gpm_warning ("Cannot connect to system bus: %s", error->message);
		g_error_free (error);
		return;
	}
	monitor->priv->proxy_sys = dbus_g_proxy_new_for_name_owner (connection,
								DBUS_SERVICE_DBUS,
								DBUS_PATH_DBUS,
						 		DBUS_INTERFACE_DBUS,
								&error);
	if (error) {
		gpm_warning ("Cannot connect to system manager: %s", error->message);
		g_error_free (error);
		return;
	}
	dbus_g_proxy_add_signal (monitor->priv->proxy_sys, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (monitor->priv->proxy_sys, "NameOwnerChanged",
				     G_CALLBACK (dbus_noc_system_cb),
				     monitor, NULL);
}

static void
gpm_dbus_monitor_finalize (GObject *object)
{
	GpmDbusMonitor *monitor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_DBUS_MONITOR (object));

	monitor = GPM_DBUS_MONITOR (object);

	if (monitor->priv->proxy_ses) {
		g_object_unref (monitor->priv->proxy_ses);
		g_object_unref (monitor->priv->proxy_sys);
	}
	G_OBJECT_CLASS (gpm_dbus_monitor_parent_class)->finalize (object);
}

GpmDbusMonitor *
gpm_dbus_monitor_new (void)
{
	static GpmDbusMonitor *monitor = NULL;
	if (monitor != NULL) {
		g_object_ref (monitor);
	} else {
		monitor = g_object_new (GPM_TYPE_DBUS_MONITOR, NULL);
	}
	return GPM_DBUS_MONITOR (monitor);
}
