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
#include <libdbus-proxy.h>

#include "libhal-marshal.h"
#include "libhal-gpower.h"
#include "libhal-gdevice.h"
#include "libhal-gmanager.h"

static void     hal_gmanager_class_init (HalGManagerClass *klass);
static void     hal_gmanager_init       (HalGManager      *hal_gmanager);
static void     hal_gmanager_finalize   (GObject	  *object);

#define LIBHAL_GMANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), LIBHAL_TYPE_GMANAGER, HalGManagerPrivate))

struct HalGManagerPrivate
{
	DBusGConnection		*connection;
	HalGDevice		*computer;
	DbusProxy		*gproxy;
};

/* Signals emitted from HalGManager are:
 *
 * device-added
 * device-removed
 * new-capability
 * lost-capability
 * daemon-start
 * daemon-stop
 */
enum {
	DEVICE_ADDED,
	DEVICE_REMOVED,
	NEW_CAPABILITY,
	LOST_CAPABILITY,
	DAEMON_START,
	DAEMON_STOP,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer hal_gmanager_object = NULL;

G_DEFINE_TYPE (HalGManager, hal_gmanager, G_TYPE_OBJECT)

/**
 * hal_gmanager_is_running:
 *
 * @hal_gmanager: This class instance
 * Return value: TRUE if hal_gmanagerdaemon is running
 *
 * Finds out if hal_gmanager is running
 **/
gboolean
hal_gmanager_is_running (HalGManager *manager)
{
	gchar *udi = NULL;
	gboolean running;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), FALSE);

	running = hal_gdevice_get_string (manager->priv->computer, "info.udi", &udi, NULL);
	g_free (udi);
	return running;
}

/**
 * hal_gmanager_find_capability:
 *
 * @hal_gmanager: This class instance
 * @capability: The capability, e.g. "battery"
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_gmanager_find_capability (HalGManager *manager,
			      const gchar *capability,
			      gchar     ***value,
			      GError     **error)
{
	DBusGProxy *proxy = NULL;
	gboolean ret;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), FALSE);
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
 * hal_gmanager_find_device_string_match:
 *
 * @hal_gmanager: This class instance
 * @key: The key, e.g. "battery.type"
 * @value: The value, e.g. "primary"
 * @devices: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_gmanager_find_device_string_match (HalGManager *manager,
			               const gchar *key,
			               const gchar *value,
			               gchar     ***devices,
			               GError     **error)
{
	DBusGProxy *proxy = NULL;
	gboolean ret;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (devices != NULL, FALSE);

	proxy = dbus_g_proxy_new_for_name (manager->priv->connection,
					   HAL_DBUS_SERVICE,
					   HAL_DBUS_PATH_MANAGER,
					   HAL_DBUS_INTERFACE_MANAGER);
	ret = dbus_g_proxy_call (proxy, "FindDeviceStringMatch", error,
				 G_TYPE_STRING, key,
				 G_TYPE_STRING, value,
				 G_TYPE_INVALID,
				 G_TYPE_STRV, devices,
				 G_TYPE_INVALID);
	if (!ret) {
		*devices = NULL;
	}
	return ret;
}

/**
 * hal_gmanager_free_capability:
 *
 * @value: The list of strings to free
 *
 * Frees value result of hal_gmanager_find_capability. Safe to call with NULL.
 **/
void
hal_gmanager_free_capability (gchar **value)
{
	gint i;

	if (value == NULL) {
		return;
	}
	for (i=0; value[i]; i++) {
		g_free (value[i]);
	}
	g_free (value);
}

/**
 * hal_gmanager_num_devices_of_capability:
 *
 * @manager: This class instance
 * @capability: The capability, e.g. "battery"
 * Return value: Number of devices of that capability
 *
 * Get the number of devices on system with a specific capability
 **/
gint
hal_gmanager_num_devices_of_capability (HalGManager *manager,
					const gchar *capability)
{
	gint i;
	gchar **names;
	gboolean ret;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), 0);
	g_return_val_if_fail (capability != NULL, 0);

	ret = hal_gmanager_find_capability (manager, capability, &names, NULL);
	if (!ret) {
		return 0;
	}
	/* iterate to find number of items */
	for (i = 0; names[i]; i++) {};
	hal_gmanager_free_capability (names);
	return i;
}

/**
 * hal_gmanager_num_devices_of_capability_with_value:
 *
 * @manager: This class instance
 * @capability: The capability, e.g. "battery"
 * @key: The key to match, e.g. "button.type"
 * @value: The key match, e.g. "power"
 * Return value: Number of devices of that capability
 *
 * Get the number of devices on system with a specific capability and key value
 **/
gint
hal_gmanager_num_devices_of_capability_with_value (HalGManager *manager,
					      const gchar *capability,
					      const gchar *key,
					      const gchar *value)
{
	gint i;
	gint valid = 0;
	gchar **names;
	gboolean ret;
	HalGDevice *hal_device;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), 0);
	g_return_val_if_fail (capability != NULL, 0);
	g_return_val_if_fail (key != NULL, 0);
	g_return_val_if_fail (value != NULL, 0);

	ret = hal_gmanager_find_capability (manager, capability, &names, NULL);
	if (!ret) {
		return 0;
	}
	for (i = 0; names[i]; i++) {
		gchar *type = NULL;
		hal_device = hal_gdevice_new ();
		hal_gdevice_set_udi (hal_device, names[i]);
		hal_gdevice_get_string (hal_device, key, &type, NULL);
		g_object_unref (hal_device);
		if (type != NULL) {
			if (strcmp (type, value) == 0)
				valid++;
			g_free (type);
		}
	}
	hal_gmanager_free_capability (names);
	return valid;
}

/**
 * hal_gmanager_class_init:
 * @klass: This class instance
 **/
static void
hal_gmanager_class_init (HalGManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = hal_gmanager_finalize;
	g_type_class_add_private (klass, sizeof (HalGManagerPrivate));

	signals [DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGManagerClass, device_added),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signals [DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGManagerClass, device_removed),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signals [NEW_CAPABILITY] =
		g_signal_new ("new-capability",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGManagerClass, new_capability),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);

	signals [LOST_CAPABILITY] =
		g_signal_new ("lost-capability",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGManagerClass, lost_capability),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);

	signals [DAEMON_START] =
		g_signal_new ("daemon-start",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGManagerClass, daemon_start),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	signals [DAEMON_STOP] =
		g_signal_new ("daemon-stop",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGManagerClass, daemon_stop),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/**
 * hal_gmanager_device_added_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @manager: This class instance
 *
 * Invoked when a device is added.
 */
static void
hal_gmanager_device_added_cb (DBusGProxy  *proxy,
		              const gchar *udi,
		              HalGManager *manager)
{
	g_signal_emit (manager, signals [DEVICE_ADDED], 0, udi);
}

/**
 * hal_gmanager_device_removed_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @manager: This class instance
 *
 * Invoked when a device is removed.
 */
static void
hal_gmanager_device_removed_cb (DBusGProxy  *proxy,
		                const gchar *udi,
		                HalGManager *manager)
{
	g_signal_emit (manager, signals [DEVICE_REMOVED], 0, udi);
}

/**
 * hal_gmanager_new_capability_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @capability: The new capability, e.g. "battery"
 * @manager: This class instance
 *
 * Invoked when a device gets a new condition.
 */
static void
hal_gmanager_new_capability_cb (DBusGProxy  *proxy,
		                const gchar *udi,
		                const gchar *capability,
		                HalGManager *manager)
{
	g_signal_emit (manager, signals [NEW_CAPABILITY], 0, udi, capability);
}

/**
 * hal_gmanager_proxy_connect_more:
 *
 * @manager: This class instance
 * Return value: Success
 *
 * Connect the manager proxy to HAL and register some basic callbacks
 */
static gboolean
hal_gmanager_proxy_connect_more (HalGManager *manager)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), FALSE);

	proxy = dbus_proxy_get_proxy (manager->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}

	/* connect the org.freedesktop.Hal.Manager signals */
	dbus_g_proxy_add_signal (proxy, "DeviceAdded",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "DeviceAdded",
				     G_CALLBACK (hal_gmanager_device_added_cb), manager, NULL);

	dbus_g_proxy_add_signal (proxy, "DeviceRemoved",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "DeviceRemoved",
				     G_CALLBACK (hal_gmanager_device_removed_cb), manager, NULL);

	dbus_g_object_register_marshaller (libhal_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy, "NewCapability",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "NewCapability",
				     G_CALLBACK (hal_gmanager_new_capability_cb), manager, NULL);

	return TRUE;
}

/**
 * hal_gmanager_proxy_disconnect_more:
 *
 * @manager: This class instance
 * Return value: Success
 *
 * Disconnect the manager proxy to HAL_GMANAGER and disconnect some basic callbacks
 */
static gboolean
hal_gmanager_proxy_disconnect_more (HalGManager *manager)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), FALSE);

	proxy = dbus_proxy_get_proxy (manager->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}

	dbus_g_proxy_disconnect_signal (proxy, "DeviceRemoved",
					G_CALLBACK (hal_gmanager_device_removed_cb), manager);
	dbus_g_proxy_disconnect_signal (proxy, "NewCapability",
					G_CALLBACK (hal_gmanager_new_capability_cb), manager);

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
		 HalGManager *manager)
{
	g_return_if_fail (LIBHAL_IS_GMANAGER (manager));
	if (status) {
		g_signal_emit (manager, signals [DAEMON_START], 0);
		hal_gmanager_proxy_connect_more (manager);
	} else {
		g_signal_emit (manager, signals [DAEMON_STOP], 0);
		hal_gmanager_proxy_disconnect_more (manager);
	}
}

/**
 * hal_gmanager_init:
 *
 * @manager: This class instance
 **/
static void
hal_gmanager_init (HalGManager *manager)
{
	GError *error = NULL;
	DBusGProxy *proxy;
	gboolean ret;

	manager->priv = LIBHAL_GMANAGER_GET_PRIVATE (manager);

	manager->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	/* get the manager connection */
	manager->priv->gproxy = dbus_proxy_new ();
	proxy = dbus_proxy_assign (manager->priv->gproxy,
				  DBUS_PROXY_SYSTEM,
				  HAL_DBUS_SERVICE,
				  HAL_DBUS_PATH_MANAGER,
				  HAL_DBUS_INTERFACE_MANAGER);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("Either HAL or DBUS are not working!");
		exit (0);
	}

	g_signal_connect (manager->priv->gproxy, "proxy-status",
			  G_CALLBACK (proxy_status_cb), manager);

	/* use the computer object */
	manager->priv->computer = hal_gdevice_new();
	ret = hal_gdevice_set_udi (manager->priv->computer, HAL_ROOT_COMPUTER);
	if (!ret) {
		g_warning ("failed to get Computer root object");
	}

	/* blindly try to connect, assuming HAL is alive */
	hal_gmanager_proxy_connect_more (manager);
}

/**
 * hal_gmanager_is_laptop:
 *
 * @manager: This class instance
 * Return value: TRUE is computer is identified as a laptop
 *
 * Returns true if system.formfactor is "laptop"
 **/
gboolean
hal_gmanager_is_laptop (HalGManager *manager)
{
	gboolean ret = TRUE;
	gchar *formfactor = NULL;

	g_return_val_if_fail (LIBHAL_IS_GMANAGER (manager), FALSE);

	/* always present */
	hal_gdevice_get_string (manager->priv->computer, "system.formfactor", &formfactor, NULL);
	if (formfactor == NULL) {
		/* no need to free */
		return FALSE;
	}
	if (strcmp (formfactor, "laptop") != 0) {
		g_warning ("This machine is not identified as a laptop."
			   "system.formfactor is %s.", formfactor);
		ret = FALSE;
	}
	g_free (formfactor);
	return ret;
}

/**
 * hal_gmanager_finalize:
 * @object: This class instance
 **/
static void
hal_gmanager_finalize (GObject *object)
{
	HalGManager *manager;
	g_return_if_fail (object != NULL);
	g_return_if_fail (LIBHAL_IS_GMANAGER (object));

	manager = LIBHAL_GMANAGER (object);
	manager->priv = LIBHAL_GMANAGER_GET_PRIVATE (manager);

	g_object_unref (manager->priv->gproxy);
	g_object_unref (manager->priv->computer);

	G_OBJECT_CLASS (hal_gmanager_parent_class)->finalize (object);
}

/**
 * hal_gmanager_new:
 * Return value: new HalGManager instance.
 **/
HalGManager *
hal_gmanager_new (void)
{
	if (hal_gmanager_object != NULL) {
		g_object_ref (hal_gmanager_object);
	} else {
		hal_gmanager_object = g_object_new (LIBHAL_TYPE_GMANAGER, NULL);
		g_object_add_weak_pointer (hal_gmanager_object, &hal_gmanager_object);
	}
	return LIBHAL_GMANAGER (hal_gmanager_object);
}

