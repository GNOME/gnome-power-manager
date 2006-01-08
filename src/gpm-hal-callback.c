/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/** @file	gpm-hal-callback.c
 *  @brief	GLIB replacement for libhal, providing callbacks
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module handles the callbacks for HAL, and lets a program register
 * a hook, and assign a callback. It is designed as a more robust framework
 * than using libhal.
 */
/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/**
 * @addtogroup	hal
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include <dbus/dbus-glib.h>

#include "gpm-dbus-common.h"
#include "gpm-hal-callback.h"
#include "gpm-marshal.h"

HalFunctions function;
HalRegistered reg;
HalConnections proxy;
gpointer cb_user_data;

/** PropertyModified signal handler
 *
 *  @param	proxy		A valid DBUS Proxy
 *  @param	type		Unused
 *  @param	properties	A linked list of type UdiProxy
 */
static void
signal_handler_PropertyModified (DBusGProxy *proxy, gint type, GPtrArray *properties)
{
	GValueArray *array = NULL;
	guint i;
	const gchar *udi;
	const gchar *key;
	gboolean added;
	gboolean removed;

	if (!function.device_property_modified) {
		g_warning ("gpm_hal: signal_handler_PropertyModified when no function!");
		return;
	}

	udi = dbus_g_proxy_get_path (proxy);
	g_debug ("gpm_hal: property modified '%s'", udi);
	for (i = 0; i < properties->len; i++) {
		array = g_ptr_array_index (properties, i);
		if (array->n_values != 3) {
			g_warning ("array->n_values invalid (!3)");
			return;
		}
		key = g_value_get_string (g_value_array_get_nth (array, 0));
		removed = g_value_get_boolean (g_value_array_get_nth (array, 1));
		added = g_value_get_boolean (g_value_array_get_nth (array, 2));
		function.device_property_modified (udi, key, removed, added, cb_user_data);
	}
}

/** DeviceRemoved signal handler
 *
 *  @param	proxy		A valid DBUS Proxy
 *  @param	udi		The HAL UDI
 */
static void
signal_handler_DeviceRemoved (DBusGProxy *proxy, gchar *udi)
{
	if (!function.device_removed) {
		g_warning ("gpm_hal: signal_handler_DeviceRemoved when no function!");
		return;
	}
	g_debug ("gpm_hal: device removed '%s'", udi);
	function.device_removed (udi, cb_user_data);
}

/** NewCapability signal handler
 *
 *  @param	proxy		A valid DBUS Proxy
 *  @param	udi		The HAL UDI
 *  @param	capability	The HAL capability
 */
static void
signal_handler_NewCapability (DBusGProxy *proxy, gchar *udi, gchar *capability)
{
	if (!function.device_new_capability) {
		g_warning ("gpm_hal: signal_handler_NewCapability when no function!");
		return;
	}
	g_debug ("gpm_hal: new capability '%s'", udi);
	function.device_new_capability (udi, capability, cb_user_data);
}

/** Condition signal handler
 *
 *  @param	proxy		A valid DBUS Proxy
 *  @param	name		The Condition name, e.g. ButtonPressed
 *  @param	details		Unknown
 */
static void
signal_handler_Condition (DBusGProxy *proxy, gchar *name, gchar *details)
{
	const gchar *udi;
	if (!function.device_condition) {
		g_warning ("gpm_hal: signal_handler_Condition when no function!");
		return;
	}
	udi = dbus_g_proxy_get_path (proxy);
	g_debug ("gpm_hal: condition '%s'", udi);
	function.device_condition (udi, name, details, cb_user_data);
}

/** Removed watch removal 
 *
 *  @return			If we removed the watch successfully
 */
static gboolean
gpm_hal_watch_add_device_removed (void)
{
	DBusGConnection *system_connection;
	GError *error = NULL;

	g_assert (!function.device_removed);
	g_assert (!reg.device_removed);

	g_debug ("gpm_hal: DeviceRemoved: Registered");
	reg.device_removed = TRUE;
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;

	proxy.device_removed = dbus_g_proxy_new_for_name_owner (system_connection,
		HAL_DBUS_SERVICE,
		HAL_DBUS_PATH_MANAGER,
		HAL_DBUS_INTERFACE_MANAGER, &error);
	dbus_g_proxy_add_signal (proxy.device_removed, "DeviceRemoved",
		G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy.device_removed, "DeviceRemoved",
		G_CALLBACK (signal_handler_DeviceRemoved), NULL, NULL);
	return TRUE;
}

/** NewCapability watch add 
 *
 *  @return			If we added the watch successfully
 */
static gboolean
gpm_hal_watch_add_device_new_capability (void)
{
	DBusGConnection *system_connection;
	GError *error = NULL;

	g_assert (!function.device_new_capability);
	g_assert (!reg.device_new_capability);

	g_debug ("gpm_hal: NewCapability: Registered");
	reg.device_new_capability = TRUE;
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	proxy.device_new_capability = dbus_g_proxy_new_for_name_owner (system_connection,
		HAL_DBUS_SERVICE,
		HAL_DBUS_PATH_MANAGER,
		HAL_DBUS_INTERFACE_MANAGER, &error);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING,
		G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy.device_new_capability, "NewCapability",
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy.device_new_capability, "NewCapability",
		G_CALLBACK (signal_handler_NewCapability), NULL, NULL);
	return TRUE;
}

/** Condition watch add 
 *
 *  @param	udi		The HAL UDI
 *  @return			If we added the watch successfully
 */
gboolean
gpm_hal_watch_add_device_condition (const gchar *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	int a;
	UdiProxy *udiproxy;
	/* need to check for previous add */
	for (a=0;a < proxy.device_condition->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_condition, a);
		if (strcmp (udi, udiproxy->udi) == 0) {
			g_warning ("gpm_hal: Condition: Already registered UDI '%s'!", udi);
			return FALSE;
		}
	}

	g_debug ("gpm_hal: Condition: Registered UDI '%s'", udi);
	reg.device_condition = TRUE;
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name_owner  (system_connection,
		HAL_DBUS_SERVICE,
		udi,
		HAL_DBUS_INTERFACE_DEVICE, &error);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING,
		G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (hal_proxy, "Condition",
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (hal_proxy, "Condition",
		G_CALLBACK (signal_handler_Condition), NULL, NULL);

	/* allocate and add to array */
	udiproxy = g_new0 (UdiProxy, 1);
	strncpy (udiproxy->udi, udi, 128);
	udiproxy->proxy = hal_proxy;
	g_ptr_array_add (proxy.device_condition, (gpointer) udiproxy);

	return TRUE;
}

/** PropertyModified watch add
 *
 *  @param	udi		The HAL UDI
 *  @return			If we added the watch successfully
 */
gboolean
gpm_hal_watch_add_device_property_modified (const gchar *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GType struct_array_type;
	GError *error = NULL;
	int a;
	UdiProxy *udiproxy;
	/* need to check for previous add */
	for (a=0;a < proxy.device_property_modified->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_property_modified, a);
		if (strcmp (udi, udiproxy->udi) == 0) {
			g_warning ("gpm_hal: PropertyModified: Already registered UDI '%s'!", udi);
			return FALSE;
		}
	}
	g_debug ("gpm_hal: PropertyModified: Registered UDI '%s'", udi);
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name_owner  (system_connection,
		HAL_DBUS_SERVICE,
		udi,
		HAL_DBUS_INTERFACE_DEVICE, &error);

	struct_array_type = dbus_g_type_get_collection ("GPtrArray", G_TYPE_VALUE_ARRAY);
	dbus_g_object_register_marshaller (gpm_marshal_VOID__INT_BOXED,
		G_TYPE_NONE, G_TYPE_INT, struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (hal_proxy, "PropertyModified",
		G_TYPE_INT, struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (hal_proxy, "PropertyModified",
		G_CALLBACK (signal_handler_PropertyModified), NULL, NULL);

	/* allocate and add to array */
	udiproxy = g_new0 (UdiProxy, 1);
	strncpy (udiproxy->udi, udi, 128);
	udiproxy->proxy = hal_proxy;
	g_ptr_array_add (proxy.device_property_modified, (gpointer) udiproxy);

	return TRUE;
}

/** DeviceRemoved watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
gpm_hal_watch_remove_device_removed (void)
{
	if (!proxy.device_removed) {
		g_warning ("gpm_hal: gpm_hal_watch_remove_removed when no watch!");
		return FALSE;
	}
	g_debug ("gpm_hal: watch remove removed");
	g_object_unref (G_OBJECT (proxy.device_removed));
	proxy.device_removed = NULL;
	return TRUE;
}

/** DeviceAdded watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
gpm_hal_watch_remove_device_added (void)
{
	if (!proxy.device_added) {
		g_warning ("gpm_hal: gpm_hal_watch_remove_added when no watch!");
		return FALSE;
	}
	g_debug ("gpm_hal: watch remove added");
	g_object_unref (G_OBJECT (proxy.device_added));
	proxy.device_added = NULL;
	return TRUE;
}

/** NewCapability watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
gpm_hal_watch_remove_device_new_capability (void)
{
	if (!proxy.device_new_capability) {
		g_warning ("gpm_hal: gpm_hal_watch_remove_new_capability when no watch!");
		return FALSE;
	}
	g_debug ("gpm_hal: watch remove new_capability");
	g_object_unref (G_OBJECT (proxy.device_new_capability));
	proxy.device_new_capability = NULL;
	return TRUE;
}

/** LostCapability watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
gpm_hal_watch_remove_device_lost_capability (void)
{
	if (!proxy.device_lost_capability) {
		g_warning ("gpm_hal: gpm_hal_watch_remove_lost_capability when no watch!");
		return FALSE;
	}
	g_debug ("gpm_hal: watch remove lost_capability");
	g_object_unref (G_OBJECT (proxy.device_lost_capability));
	proxy.device_lost_capability = NULL;
	return TRUE;
}

/** PropertyModified watch callback handler
 *
 *  @param	udi		The HAL UDI
 *  @return			If we handled the watch callback okay
 */
gboolean
gpm_hal_watch_remove_device_property_modified (const gchar *udi)
{
	int a;
	UdiProxy *udiproxy = NULL;
	for (a=0;a < proxy.device_property_modified->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_property_modified, a);
		if (strcmp (udi, udiproxy->udi) == 0)
			break;
	}
	if (a == proxy.device_property_modified->len || !udiproxy) {
		g_warning ("gpm_hal: gpm_hal_watch_remove_property_modified when no watch on '%s'!", udi);
		return FALSE;
	}

	g_debug ("gpm_hal: watch remove property_modified on '%s'", udi);
	g_object_unref (G_OBJECT (udiproxy->proxy));
	g_ptr_array_remove_fast (proxy.device_property_modified, (gpointer) udiproxy);
	g_free (udiproxy);

	return TRUE;
}

/** Condition watch callback handler
 *
 *  @param	udi		The HAL UDI
 *  @return			If we handled the watch callback okay
 */
gboolean
gpm_hal_watch_remove_device_condition (const gchar *udi)
{
	int a;
	UdiProxy *udiproxy = NULL;
	for (a=0;a < proxy.device_condition->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_condition, a);
		if (strcmp (udi, udiproxy->udi) == 0)
			break;
	}
	if (a == proxy.device_condition->len || !udiproxy) {
		g_warning ("gpm_hal: gpm_hal_watch_remove_condition when no watch on '%s'!", udi);
		return FALSE;
	}

	g_debug ("gpm_hal: watch remove condition on '%s'", udi);
	g_object_unref (G_OBJECT (udiproxy->proxy));
	g_ptr_array_remove_fast (proxy.device_condition, (gpointer) udiproxy);
	g_free (udiproxy);

	return TRUE;
}

/** DeviceRemoved callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
gpm_hal_method_device_removed (HalDeviceRemoved callback)
{
	if (!reg.device_removed)
		gpm_hal_watch_add_device_removed ();
	function.device_removed = callback;
	return TRUE;
}

/** DeviceAdded callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
gpm_hal_method_device_added (HalDeviceAdded callback)
{
	function.device_added = callback;
	return TRUE;
}

/** NewCapability callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
gpm_hal_method_device_new_capability (HalDeviceNewCapability callback)
{
	if (!reg.device_new_capability)
		gpm_hal_watch_add_device_new_capability ();
	function.device_new_capability = callback;
	return TRUE;
}

/** LostCapability callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
gpm_hal_method_device_lost_capability (HalDeviceLostCapability callback)
{
	function.device_lost_capability = callback;
	return TRUE;
}

/** PropertyModified callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
gpm_hal_method_device_property_modified (HalDevicePropertyModified callback)
{
	g_debug ("gpm_hal: PropertyModified: Registered");
	function.device_property_modified = callback;
	return TRUE;
}

/** Condition callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
gpm_hal_method_device_condition (HalDeviceCondition callback)
{
	g_debug ("gpm_hal: Condition: Registered");
	function.device_condition = callback;
	return TRUE;
}

/** Initialise callback support
 *
 *  @return			If we initialised callbacks okay
 */
gboolean
gpm_hal_callback_init (gpointer data)
{
	g_debug ("gpm_hal_callback: init");
	function.device_added = NULL;
	function.device_removed = NULL;
	function.device_new_capability = NULL;
	function.device_lost_capability = NULL;
	function.device_property_modified = NULL;
	function.device_condition = NULL;

	reg.device_added = FALSE;
	reg.device_removed = FALSE;
	reg.device_new_capability = FALSE;
	reg.device_lost_capability = FALSE;
	reg.device_condition = FALSE;

	proxy.device_added = NULL;
	proxy.device_removed = NULL;
	proxy.device_new_capability = NULL;
	proxy.device_lost_capability = NULL;

	/* array types */
	proxy.device_condition = g_ptr_array_new ();
	proxy.device_property_modified = g_ptr_array_new ();

	cb_user_data = data;

	return TRUE;
}

/** Shutdown callback support, freeing memory
 *
 *  @return			If we shutdown callbacks okay
 */
gboolean
gpm_hal_callback_shutdown (void)
{
	int a;
	UdiProxy *udiproxy;

	g_debug ("gpm_hal: shutdown");
	if (proxy.device_added)
		gpm_hal_watch_remove_device_added ();
	if (proxy.device_removed)
		gpm_hal_watch_remove_device_removed ();
	if (proxy.device_new_capability)
		gpm_hal_watch_remove_device_new_capability ();
	if (proxy.device_lost_capability)
		gpm_hal_watch_remove_device_lost_capability ();

	/* array types */
	for (a=0;a < proxy.device_condition->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_condition, a);
		gpm_hal_watch_remove_device_condition (udiproxy->udi);
	}
	for (a=0;a < proxy.device_property_modified->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_property_modified, a);
		gpm_hal_watch_remove_device_property_modified (udiproxy->udi);
	}
	g_ptr_array_free (proxy.device_condition, TRUE);
	g_ptr_array_free (proxy.device_property_modified, TRUE);

	return TRUE;
}
/** @} */
