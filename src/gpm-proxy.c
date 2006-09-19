/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#include "gpm-proxy.h"
#include "gpm-debug.h"
#include "gpm-dbus-monitor.h"

static void     gpm_proxy_class_init (GpmProxyClass *klass);
static void     gpm_proxy_init       (GpmProxy      *proxy);
static void     gpm_proxy_finalize   (GObject		*object);

#define GPM_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PROXY, GpmProxyPrivate))

/* this is a managed proxy, i.e. a proxy that handles messagebus and DBUS service restarts. */

struct GpmProxyPrivate
{
	GpmProxyBusType  bus_type;
	gchar		*service;
	gchar		*interface;
	gchar		*path;
	DBusGProxy	*proxy;
	GpmDbusMonitor	*dbus_monitor;
	gboolean	 assigned;
	DBusGConnection	*connection;
};

enum {
	PROXY_STATUS,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmProxy, gpm_proxy, G_TYPE_OBJECT)

/**
 * gpm_proxy_connect:
 * @gproxy: This class instance
 * Return value: success
 **/
static gboolean
gpm_proxy_connect (GpmProxy *gproxy)
{
	GError     *error = NULL;

	g_return_val_if_fail (GPM_IS_PROXY (gproxy), FALSE);

	/* are already connected? */
	if (gproxy->priv->proxy != NULL) {
		gpm_debug ("already connected to %s", gproxy->priv->service);
		return FALSE;
	}

	gproxy->priv->proxy = dbus_g_proxy_new_for_name_owner (gproxy->priv->connection,
							       gproxy->priv->service,
							       gproxy->priv->path,
							       gproxy->priv->interface,
							       &error);
	/* check for any possible error */
	if (error) {
		gpm_warning ("DBUS error: %s", error->message);
		g_error_free (error);
		gproxy->priv->proxy = NULL;
	}

	/* shouldn't be, but make sure proxy valid */
	if (gproxy->priv->proxy == NULL) {
		gpm_debug ("proxy is NULL, maybe the daemon responsible "
			   "for %s is not running?", gproxy->priv->service);
		return FALSE;
	}

	gpm_debug ("emitting proxy-status TRUE: %s", gproxy->priv->service);
	g_signal_emit (gproxy, signals [PROXY_STATUS], 0, TRUE);

	return TRUE;
}

/**
 * gpm_proxy_disconnect:
 * @gproxy: This class instance
 * Return value: success
 **/
static gboolean
gpm_proxy_disconnect (GpmProxy *gproxy)
{
	g_return_val_if_fail (GPM_IS_PROXY (gproxy), FALSE);

	/* are already disconnected? */
	if (gproxy->priv->proxy == NULL) {
		gpm_debug ("already disconnected from %s", gproxy->priv->service);
		return FALSE;
	}

	gpm_debug ("emitting proxy-status FALSE: %s", gproxy->priv->service);
	g_signal_emit (gproxy, signals [PROXY_STATUS], 0, FALSE);
	gproxy->priv->proxy = NULL;

	return TRUE;
}

/**
 * gpm_proxy_assign:
 * @gproxy: This class instance
 * @bus_type: The bus type, either GPM_PROXY_SESSION or GPM_PROXY_SYSTEM
 * @service: The DBUS service name
 * @interface: The DBUS interface
 * @path: The DBUS path
 * Return value: The DBUS proxy, or NULL if we haven't connected yet.
 **/
DBusGProxy *
gpm_proxy_assign (GpmProxy	 *gproxy,
		  GpmProxyBusType bus_type,
		  const gchar	 *service,
		  const gchar	 *path,
		  const gchar	 *interface)
{
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_PROXY (gproxy), NULL);
	g_return_val_if_fail (service != NULL, NULL);
	g_return_val_if_fail (interface != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	if (gproxy->priv->assigned == TRUE) {
		gpm_warning ("already assigned proxy!");
		return NULL;
	}

	/* get the DBUS connection */
	if (bus_type == GPM_PROXY_SESSION) {
		gproxy->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	} else {
		gproxy->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	}

	if (error) {
		gpm_warning ("Could not connect to DBUS daemon: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	if (gproxy->priv->connection == NULL) {
		gpm_warning ("Could not connect to DBUS daemon!");
		return NULL;
	}

	gproxy->priv->service = g_strdup (service);
	gproxy->priv->interface = g_strdup (interface);
	gproxy->priv->path = g_strdup (path);
	gproxy->priv->bus_type = bus_type;
	gproxy->priv->assigned = TRUE;

	/* try to connect and return proxy (or NULL if invalid) */
	gpm_proxy_connect (gproxy);

	return gproxy->priv->proxy;
}

/**
 * gpm_proxy_get_service:
 * @gproxy: This class instance
 * Return value: The DBUS proxy, or NULL if we are not connected
 **/
DBusGProxy *
gpm_proxy_get_proxy (GpmProxy *gproxy)
{
	g_return_val_if_fail (GPM_IS_PROXY (gproxy), NULL);
	g_return_val_if_fail (gproxy->priv->assigned == TRUE, NULL);
	return gproxy->priv->proxy;
}

/**
 * gpm_proxy_get_service:
 * @gproxy: This class instance
 * Return value: The DBUS service name
 **/
gchar *
gpm_proxy_get_service (GpmProxy *gproxy)
{
	g_return_val_if_fail (GPM_IS_PROXY (gproxy), NULL);
	g_return_val_if_fail (gproxy->priv->assigned == TRUE, NULL);
	return gproxy->priv->service;
}

/**
 * gpm_proxy_get_interface:
 * @gproxy: This class instance
 * Return value: The DBUS interface
 **/
gchar *
gpm_proxy_get_interface (GpmProxy *gproxy)
{
	g_return_val_if_fail (GPM_IS_PROXY (gproxy), NULL);
	g_return_val_if_fail (gproxy->priv->assigned == TRUE, NULL);
	return gproxy->priv->interface;
}

/**
 * gpm_proxy_get_path:
 * @gproxy: This class instance
 * Return value: The DBUS path
 **/
gchar *
gpm_proxy_get_path (GpmProxy *gproxy)
{
	g_return_val_if_fail (GPM_IS_PROXY (gproxy), NULL);
	g_return_val_if_fail (gproxy->priv->assigned == TRUE, NULL);
	return gproxy->priv->path;
}

/**
 * gpm_proxy_get_bus_type:
 * @gproxy: This class instance
 * Return value: The bus type, either GPM_PROXY_SESSION or GPM_PROXY_SYSTEM
 **/
GpmProxyBusType
gpm_proxy_get_bus_type (GpmProxy *gproxy)
{
	g_return_val_if_fail (GPM_IS_PROXY (gproxy), GPM_PROXY_UNKNOWN);
	g_return_val_if_fail (gproxy->priv->assigned == TRUE, GPM_PROXY_UNKNOWN);
	return gproxy->priv->bus_type;
}

/**
 * gpm_proxy_class_init:
 * @gproxy: This class instance
 **/
static void
gpm_proxy_class_init (GpmProxyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_proxy_finalize;
	g_type_class_add_private (klass, sizeof (GpmProxyPrivate));

	signals [PROXY_STATUS] =
		g_signal_new ("proxy-status",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmProxyClass, proxy_status),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
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
dbus_noc_session_cb (GpmDbusMonitor *dbus_monitor,
		     const gchar    *name,
		     const gchar    *prev,
		     const gchar    *new,
		     GpmProxy	    *gproxy)
{
	g_return_if_fail (GPM_IS_PROXY (gproxy));
	if (gproxy->priv->assigned == FALSE) {
		return;
	}
	if (gproxy->priv->bus_type == GPM_PROXY_SYSTEM) {
		return;
	}

	if (strcmp (name, gproxy->priv->service) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0 ) {
			gpm_proxy_disconnect (gproxy);
		}
		if (strlen (prev) == 0 && strlen (new) != 0 ) {
			gpm_proxy_connect (gproxy);
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
dbus_noc_system_cb (GpmDbusMonitor *dbus_monitor,
		    const gchar	   *name,
		    const gchar    *prev,
		    const gchar    *new,
		    GpmProxy	   *gproxy)
{
	g_return_if_fail (GPM_IS_PROXY (gproxy));
	if (gproxy->priv->assigned == FALSE) {
		return;
	}
	if (gproxy->priv->bus_type == GPM_PROXY_SESSION) {
		return;
	}

	if (strcmp (name, gproxy->priv->service) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0 ) {
			gpm_proxy_disconnect (gproxy);
		}
		if (strlen (prev) == 0 && strlen (new) != 0 ) {
			gpm_proxy_connect (gproxy);
		}
	}
}

/**
 * gpm_proxy_init:
 * @gproxy: This class instance
 **/
static void
gpm_proxy_init (GpmProxy *gproxy)
{
	gproxy->priv = GPM_PROXY_GET_PRIVATE (gproxy);

	gproxy->priv->connection = NULL;
	gproxy->priv->proxy = NULL;
	gproxy->priv->service = NULL;
	gproxy->priv->interface = NULL;
	gproxy->priv->path = NULL;
	gproxy->priv->bus_type = GPM_PROXY_UNKNOWN;
	gproxy->priv->assigned = FALSE;

	gproxy->priv->dbus_monitor = gpm_dbus_monitor_new ();
	g_signal_connect (gproxy->priv->dbus_monitor, "noc-session",
			  G_CALLBACK (dbus_noc_session_cb), gproxy);
	g_signal_connect (gproxy->priv->dbus_monitor, "noc-system",
			  G_CALLBACK (dbus_noc_system_cb), gproxy);
}

/**
 * gpm_proxy_finalize:
 * @object: This class instance
 **/
static void
gpm_proxy_finalize (GObject *object)
{
	GpmProxy *gproxy;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_PROXY (object));

	gproxy = GPM_PROXY (object);
	gproxy->priv = GPM_PROXY_GET_PRIVATE (gproxy);

	gpm_proxy_disconnect (gproxy);
	g_object_unref (gproxy->priv->proxy);
	g_object_unref (gproxy->priv->dbus_monitor);
	g_free (gproxy->priv->service);
	g_free (gproxy->priv->interface);
	g_free (gproxy->priv->path);
}

/**
 * gpm_proxy_new:
 * Return value: new class instance.
 **/
GpmProxy *
gpm_proxy_new (void)
{
	GpmProxy *proxy;
	proxy = g_object_new (GPM_TYPE_PROXY, NULL);
	return GPM_PROXY (proxy);
}
