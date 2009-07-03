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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "egg-debug.h"
#include "egg-dbus-proxy.h"

#include "hal-marshal.h"
#include "hal-device.h"
#include "hal-manager.h"

static void     hal_manager_class_init (HalManagerClass *klass);
static void     hal_manager_init       (HalManager      *hal_manager);
static void     hal_manager_finalize   (GObject	  *object);

#define HAL_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), HAL_TYPE_MANAGER, HalManagerPrivate))

struct HalManagerPrivate
{
	DBusGConnection		*connection;
	HalDevice		*computer;
	EggDbusProxy		*gproxy;
};

/* Signals emitted from HalManager are:
 *
 * device-added
 * device-removed
 * daemon-start
 * daemon-stop
 */
enum {
	DEVICE_ADDED,
	DEVICE_REMOVED,
	DAEMON_START,
	DAEMON_STOP,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer hal_manager_object = NULL;

G_DEFINE_TYPE (HalManager, hal_manager, G_TYPE_OBJECT)

/**
 * hal_manager_find_capability:
 *
 * @hal_manager: This class instance
 * @capability: The capability, e.g. "battery"
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_manager_find_capability (HalManager *manager,
			      const gchar *capability,
			      gchar     ***value,
			      GError     **error)
{
	DBusGProxy *proxy = NULL;
	gboolean ret;

	g_return_val_if_fail (HAL_IS_MANAGER (manager), FALSE);
	g_return_val_if_fail (capability != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	proxy = dbus_g_proxy_new_for_name (manager->priv->connection,
					   HAL_DBUS_SERVICE,
					   HAL_DBUS_PATH_MANAGER,
					   HAL_DBUS_INTERFACE_MANAGER);
	ret = dbus_g_proxy_call (proxy, "FindDeviceByCapability", error,
				 G_TYPE_STRING, capability,
				 G_TYPE_INVALID,
				 G_TYPE_STRV, value,
				 G_TYPE_INVALID);
	if (!ret) {
		*value = NULL;
	}
	return ret;
}

/**
 * hal_manager_class_init:
 * @klass: This class instance
 **/
static void
hal_manager_class_init (HalManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = hal_manager_finalize;
	g_type_class_add_private (klass, sizeof (HalManagerPrivate));

	signals [DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalManagerClass, device_added),
			      NULL,
			      NULL,
			      hal_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signals [DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalManagerClass, device_removed),
			      NULL,
			      NULL,
			      hal_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signals [DAEMON_START] =
		g_signal_new ("daemon-start",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalManagerClass, daemon_start),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	signals [DAEMON_STOP] =
		g_signal_new ("daemon-stop",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalManagerClass, daemon_stop),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/**
 * hal_manager_device_added_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @manager: This class instance
 *
 * Invoked when a device is added.
 */
static void
hal_manager_device_added_cb (DBusGProxy  *proxy,
		              const gchar *udi,
		              HalManager *manager)
{
	g_signal_emit (manager, signals [DEVICE_ADDED], 0, udi);
}

/**
 * hal_manager_device_removed_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @manager: This class instance
 *
 * Invoked when a device is removed.
 */
static void
hal_manager_device_removed_cb (DBusGProxy  *proxy,
		                const gchar *udi,
		                HalManager *manager)
{
	g_signal_emit (manager, signals [DEVICE_REMOVED], 0, udi);
}

/**
 * hal_manager_proxy_connect_more:
 *
 * @manager: This class instance
 * Return value: Success
 *
 * Connect the manager proxy to HAL and register some basic callbacks
 */
static gboolean
hal_manager_proxy_connect_more (HalManager *manager)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (HAL_IS_MANAGER (manager), FALSE);

	proxy = egg_dbus_proxy_get_proxy (manager->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}

	/* connect the org.freedesktop.Hal.Manager signals */
	dbus_g_proxy_add_signal (proxy, "DeviceAdded",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "DeviceAdded",
				     G_CALLBACK (hal_manager_device_added_cb), manager, NULL);

	dbus_g_proxy_add_signal (proxy, "DeviceRemoved",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "DeviceRemoved",
				     G_CALLBACK (hal_manager_device_removed_cb), manager, NULL);

	return TRUE;
}

/**
 * hal_manager_proxy_disconnect_more:
 *
 * @manager: This class instance
 * Return value: Success
 *
 * Disconnect the manager proxy to HAL_MANAGER and disconnect some basic callbacks
 */
static gboolean
hal_manager_proxy_disconnect_more (HalManager *manager)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (HAL_IS_MANAGER (manager), FALSE);

	proxy = egg_dbus_proxy_get_proxy (manager->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}

	dbus_g_proxy_disconnect_signal (proxy, "DeviceRemoved",
					G_CALLBACK (hal_manager_device_removed_cb), manager);

	return TRUE;
}

/**
 * proxy_status_cb:
 * @proxy: The dbus raw proxy
 * @status: The status of the service, where TRUE is connected
 * @manager: This class instance
 **/
static void
proxy_status_cb (DBusGProxy    *proxy,
		 gboolean       status,
		 HalManager *manager)
{
	g_return_if_fail (HAL_IS_MANAGER (manager));
	if (status) {
		g_signal_emit (manager, signals [DAEMON_START], 0);
		hal_manager_proxy_connect_more (manager);
	} else {
		g_signal_emit (manager, signals [DAEMON_STOP], 0);
		hal_manager_proxy_disconnect_more (manager);
	}
}

/**
 * hal_manager_init:
 *
 * @manager: This class instance
 **/
static void
hal_manager_init (HalManager *manager)
{
	GError *error = NULL;
	DBusGProxy *proxy;
	DBusGConnection *connection;
	gboolean ret;

	manager->priv = HAL_MANAGER_GET_PRIVATE (manager);

	manager->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	/* get the manager connection */
	manager->priv->gproxy = egg_dbus_proxy_new ();
	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
	proxy = egg_dbus_proxy_assign (manager->priv->gproxy, connection,
				       HAL_DBUS_SERVICE, HAL_DBUS_PATH_MANAGER,
				       HAL_DBUS_INTERFACE_MANAGER);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("Either HAL or DBUS are not working!");
		exit (0);
	}

	g_signal_connect (manager->priv->gproxy, "proxy-status",
			  G_CALLBACK (proxy_status_cb), manager);

	/* use the computer object */
	manager->priv->computer = hal_device_new();
	ret = hal_device_set_udi (manager->priv->computer, HAL_ROOT_COMPUTER);
	if (!ret) {
		g_warning ("failed to get Computer root object");
	}

	/* blindly try to connect, assuming HAL is alive */
	hal_manager_proxy_connect_more (manager);
}

/**
 * hal_manager_finalize:
 * @object: This class instance
 **/
static void
hal_manager_finalize (GObject *object)
{
	HalManager *manager;
	g_return_if_fail (object != NULL);
	g_return_if_fail (HAL_IS_MANAGER (object));

	manager = HAL_MANAGER (object);
	manager->priv = HAL_MANAGER_GET_PRIVATE (manager);

	g_object_unref (manager->priv->gproxy);
	g_object_unref (manager->priv->computer);

	G_OBJECT_CLASS (hal_manager_parent_class)->finalize (object);
}

/**
 * hal_manager_new:
 * Return value: new HalManager instance.
 **/
HalManager *
hal_manager_new (void)
{
	if (hal_manager_object != NULL) {
		g_object_ref (hal_manager_object);
	} else {
		hal_manager_object = g_object_new (HAL_TYPE_MANAGER, NULL);
		g_object_add_weak_pointer (hal_manager_object, &hal_manager_object);
	}
	return HAL_MANAGER (hal_manager_object);
}

