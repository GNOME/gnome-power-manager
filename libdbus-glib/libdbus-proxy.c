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

#include <libdbus-proxy.h>
#include <libdbus-monitor-session.h>
#include <libdbus-monitor-system.h>

static void     dbus_proxy_class_init (DbusProxyClass *klass);
static void     dbus_proxy_init       (DbusProxy      *proxy);
static void     dbus_proxy_finalize   (GObject        *object);

#define DBUS_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DBUS_TYPE_PROXY, DbusProxyPrivate))

/* this is a managed proxy, i.e. a proxy that handles messagebus and DBUS service restarts. */
struct DbusProxyPrivate
{
	DbusProxyType		 bus_type;
	gchar			*service;
	gchar			*interface;
	gchar			*path;
	DBusGProxy		*proxy;
	DbusMonitorSession	*monitor_session;
	DbusMonitorSystem	*monitor_system;
	gboolean		 assigned;
	DBusGConnection		*connection;
	gulong			 ses_sig_id;
	gulong			 sys_sig_id;
};

enum {
	PROXY_STATUS,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DbusProxy, dbus_proxy, G_TYPE_OBJECT)

/**
 * dbus_proxy_connect:
 * @dbus_proxy: This class instance
 * Return value: success
 **/
static gboolean
dbus_proxy_connect (DbusProxy *dbus_proxy)
{
	GError *error = NULL;

	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), FALSE);

	/* are already connected? */
	if (dbus_proxy->priv->proxy != NULL) {
		g_debug ("already connected to %s", dbus_proxy->priv->service);
		return FALSE;
	}

	dbus_proxy->priv->proxy = dbus_g_proxy_new_for_name_owner (dbus_proxy->priv->connection,
							       dbus_proxy->priv->service,
							       dbus_proxy->priv->path,
							       dbus_proxy->priv->interface,
							       &error);
	/* check for any possible error */
	if (error) {
		g_warning ("DBUS error: %s", error->message);
		g_error_free (error);
		dbus_proxy->priv->proxy = NULL;
	}

	/* shouldn't be, but make sure proxy valid */
	if (dbus_proxy->priv->proxy == NULL) {
		g_debug ("proxy is NULL, maybe the daemon responsible "
			   "for %s is not running?", dbus_proxy->priv->service);
		return FALSE;
	}

	g_signal_emit (dbus_proxy, signals [PROXY_STATUS], 0, TRUE);

	return TRUE;
}

/**
 * dbus_proxy_disconnect:
 * @dbus_proxy: This class instance
 * Return value: success
 **/
static gboolean
dbus_proxy_disconnect (DbusProxy *dbus_proxy)
{
	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), FALSE);

	/* are already disconnected? */
	if (dbus_proxy->priv->proxy == NULL) {
		if (dbus_proxy->priv->service)
			g_debug ("already disconnected from %s", dbus_proxy->priv->service);
		else 
			g_debug ("already disconnected.");
		return FALSE;
	}

	g_signal_emit (dbus_proxy, signals [PROXY_STATUS], 0, FALSE);

	g_object_unref (dbus_proxy->priv->proxy);
	dbus_proxy->priv->proxy = NULL;

	return TRUE;
}


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
		     DbusProxy	    *dbus_proxy)
{
	g_return_if_fail (DBUS_IS_PROXY (dbus_proxy));
	if (dbus_proxy->priv->assigned == FALSE) {
		return;
	}
	if (dbus_proxy->priv->bus_type == DBUS_PROXY_SYSTEM) {
		return;
	}

	if (strcmp (name, dbus_proxy->priv->service) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0) {
			dbus_proxy_disconnect (dbus_proxy);
		}
		if (strlen (prev) == 0 && strlen (new) != 0) {
			dbus_proxy_connect (dbus_proxy);
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
		    DbusProxy	   *dbus_proxy)
{
	g_return_if_fail (DBUS_IS_PROXY (dbus_proxy));
	if (dbus_proxy->priv->assigned == FALSE) {
		return;
	}
	if (dbus_proxy->priv->bus_type == DBUS_PROXY_SESSION) {
		return;
	}

	if (strcmp (name, dbus_proxy->priv->service) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0) {
			dbus_proxy_disconnect (dbus_proxy);
		}
		if (strlen (prev) == 0 && strlen (new) != 0) {
			dbus_proxy_connect (dbus_proxy);
		}
	}
}

/**
 * dbus_proxy_assign:
 * @dbus_proxy: This class instance
 * @bus_type: The bus type, either DBUS_PROXY_SESSION or DBUS_PROXY_SYSTEM
 * @service: The DBUS service name
 * @interface: The DBUS interface
 * @path: The DBUS path
 * Return value: The DBUS proxy, or NULL if we haven't connected yet.
 **/
DBusGProxy *
dbus_proxy_assign (DbusProxy	 *dbus_proxy,
		  DbusProxyType bus_type,
		  const gchar	 *service,
		  const gchar	 *path,
		  const gchar	 *interface)
{
	GError *error = NULL;

	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), NULL);
	g_return_val_if_fail (service != NULL, NULL);
	g_return_val_if_fail (interface != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	if (dbus_proxy->priv->assigned) {
		g_warning ("already assigned proxy!");
		return NULL;
	}

	/* get the DBUS connection */
	if (bus_type == DBUS_PROXY_SESSION) {
		dbus_proxy->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	} else {
		dbus_proxy->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	}

	if (error) {
		g_warning ("Could not connect to DBUS daemon: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	if (dbus_proxy->priv->connection == NULL) {
		g_warning ("Could not connect to DBUS daemon!");
		return NULL;
	}

	dbus_proxy->priv->service = g_strdup (service);
	dbus_proxy->priv->interface = g_strdup (interface);
	dbus_proxy->priv->path = g_strdup (path);
	dbus_proxy->priv->bus_type = bus_type;
	dbus_proxy->priv->assigned = TRUE;

	/* We have to save the connection and remove the signal id later as
	   instances of this object are likely to be registering with a
	   singleton object many times */
	if (bus_type == DBUS_PROXY_SESSION) {
		dbus_proxy->priv->monitor_session = dbus_monitor_session_new ();
		dbus_proxy->priv->ses_sig_id = g_signal_connect (dbus_proxy->priv->monitor_session,
								 "name-owner-changed",
								 G_CALLBACK (dbus_noc_session_cb),
								 dbus_proxy);
	} else {
		dbus_proxy->priv->monitor_system = dbus_monitor_system_new ();
		dbus_proxy->priv->sys_sig_id = g_signal_connect (dbus_proxy->priv->monitor_system,
								 "name-owner-changed",
								 G_CALLBACK (dbus_noc_system_cb),
								 dbus_proxy);
	}

	/* try to connect and return proxy (or NULL if invalid) */
	dbus_proxy_connect (dbus_proxy);

	return dbus_proxy->priv->proxy;
}

/**
 * dbus_proxy_get_service:
 * @dbus_proxy: This class instance
 * Return value: The DBUS proxy, or NULL if we are not connected
 **/
DBusGProxy *
dbus_proxy_get_proxy (DbusProxy *dbus_proxy)
{
	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), NULL);
	if (dbus_proxy->priv->assigned == FALSE) {
		return NULL;
	}
	return dbus_proxy->priv->proxy;
}

/**
 * dbus_proxy_get_service:
 * @dbus_proxy: This class instance
 * Return value: The DBUS service name
 **/
gchar *
dbus_proxy_get_service (DbusProxy *dbus_proxy)
{
	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), NULL);
	if (dbus_proxy->priv->assigned == FALSE) {
		g_warning ("Cannot get service, not assigned");
		return NULL;
	}
	return dbus_proxy->priv->service;
}

/**
 * dbus_proxy_get_interface:
 * @dbus_proxy: This class instance
 * Return value: The DBUS interface
 **/
gchar *
dbus_proxy_get_interface (DbusProxy *dbus_proxy)
{
	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), NULL);
	if (dbus_proxy->priv->assigned == FALSE) {
		g_warning ("Cannot get interface, not assigned");
		return NULL;
	}
	return dbus_proxy->priv->interface;
}

/**
 * dbus_proxy_get_path:
 * @dbus_proxy: This class instance
 * Return value: The DBUS path
 **/
gchar *
dbus_proxy_get_path (DbusProxy *dbus_proxy)
{
	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), NULL);
	if (dbus_proxy->priv->assigned == FALSE) {
		g_warning ("Cannot get path, not assigned");
		return NULL;
	}
	return dbus_proxy->priv->path;
}

/**
 * dbus_proxy_get_bus_type:
 * @dbus_proxy: This class instance
 * Return value: The bus type, either DBUS_PROXY_SESSION or DBUS_PROXY_SYSTEM
 **/
DbusProxyType
dbus_proxy_get_bus_type (DbusProxy *dbus_proxy)
{
	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), DBUS_PROXY_UNKNOWN);
	if (dbus_proxy->priv->assigned == FALSE) {
		g_warning ("Cannot get bus type, not assigned");
		return DBUS_PROXY_UNKNOWN;
	}
	return dbus_proxy->priv->bus_type;
}

/**
 * dbus_proxy_is_connected:
 * @dbus_proxy: This class instance
 * Return value: if we are connected to a valid proxy
 **/
gboolean
dbus_proxy_is_connected (DbusProxy *dbus_proxy)
{
	g_return_val_if_fail (DBUS_IS_PROXY (dbus_proxy), FALSE);
	if (dbus_proxy->priv->assigned == FALSE) {
		return FALSE;
	}
	if (dbus_proxy->priv->proxy == NULL) {
		return FALSE;
	}
	return TRUE;
}

/**
 * dbus_proxy_class_init:
 * @dbus_proxy: This class instance
 **/
static void
dbus_proxy_class_init (DbusProxyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dbus_proxy_finalize;
	g_type_class_add_private (klass, sizeof (DbusProxyPrivate));

	signals [PROXY_STATUS] =
		g_signal_new ("proxy-status",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DbusProxyClass, proxy_status),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/**
 * dbus_proxy_init:
 * @dbus_proxy: This class instance
 **/
static void
dbus_proxy_init (DbusProxy *dbus_proxy)
{
	dbus_proxy->priv = DBUS_PROXY_GET_PRIVATE (dbus_proxy);

	dbus_proxy->priv->connection = NULL;
	dbus_proxy->priv->proxy = NULL;
	dbus_proxy->priv->service = NULL;
	dbus_proxy->priv->interface = NULL;
	dbus_proxy->priv->path = NULL;
	dbus_proxy->priv->bus_type = DBUS_PROXY_UNKNOWN;
	dbus_proxy->priv->assigned = FALSE;
	dbus_proxy->priv->monitor_session = NULL;
	dbus_proxy->priv->monitor_system = NULL;
	dbus_proxy->priv->ses_sig_id = 0;
	dbus_proxy->priv->sys_sig_id = 0;
}

/**
 * dbus_proxy_finalize:
 * @object: This class instance
 **/
static void
dbus_proxy_finalize (GObject *object)
{
	DbusProxy *dbus_proxy;
	g_return_if_fail (object != NULL);
	g_return_if_fail (DBUS_IS_PROXY (object));

	dbus_proxy = DBUS_PROXY (object);
	dbus_proxy->priv = DBUS_PROXY_GET_PRIVATE (dbus_proxy);

	if (dbus_proxy->priv->ses_sig_id != 0) {
		g_signal_handler_disconnect (dbus_proxy->priv->monitor_session, dbus_proxy->priv->ses_sig_id);
	}
	if (dbus_proxy->priv->sys_sig_id != 0) {
		g_signal_handler_disconnect (dbus_proxy->priv->monitor_system, dbus_proxy->priv->sys_sig_id);
	}

	dbus_proxy_disconnect (dbus_proxy);

	if (dbus_proxy->priv->proxy != NULL) {
		g_object_unref (dbus_proxy->priv->proxy);
	}
	if (dbus_proxy->priv->monitor_session != NULL) {
		g_object_unref (dbus_proxy->priv->monitor_session);
	}
	if (dbus_proxy->priv->monitor_system != NULL) {
		g_object_unref (dbus_proxy->priv->monitor_system);
	}
	if (dbus_proxy->priv->service != NULL) {
		g_free (dbus_proxy->priv->service);
	}
	if (dbus_proxy->priv->interface != NULL) {
		g_free (dbus_proxy->priv->interface);
	}
	if (dbus_proxy->priv->path != NULL) {
		g_free (dbus_proxy->priv->path);
	}
	G_OBJECT_CLASS (dbus_proxy_parent_class)->finalize (object);
}

/**
 * dbus_proxy_new:
 * Return value: new class instance.
 **/
DbusProxy *
dbus_proxy_new (void)
{
	DbusProxy *dbus_proxy;
	dbus_proxy = g_object_new (DBUS_TYPE_PROXY, NULL);
	return DBUS_PROXY (dbus_proxy);
}

