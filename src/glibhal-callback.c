/** @file	glibhal-callback.c
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
 * @addtogroup	glibhal
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include <dbus/dbus-glib.h>

#include "gpm-dbus-common.h"
#include "glibhal-main.h"
#include "glibhal-callback.h"
#include "gpm_marshal.h"

HalFunctions function;
HalRegistered reg;
HalConnections proxy;

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

	/* assertion checks */
	g_assert (proxy);
	g_assert (properties);

	if (!function.device_property_modified) {
		g_warning ("glibhal: signal_handler_PropertyModified when no function!");
		return;
	}

	udi = dbus_g_proxy_get_path (proxy);
	g_debug ("glibhal: property modified '%s'", udi);
	for (i = 0; i < properties->len; i++) {
		array = g_ptr_array_index (properties, i);
		if (array->n_values != 3) {
			g_warning ("array->n_values invalid (!3)");
			return;
		}
		key = g_value_get_string (g_value_array_get_nth (array, 0));
		removed = g_value_get_boolean (g_value_array_get_nth (array, 1));
		added = g_value_get_boolean (g_value_array_get_nth (array, 2));
		function.device_property_modified (udi, key, removed, added);
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
	/* assertion checks */
	g_assert (proxy);
	g_assert (udi);

	if (!function.device_removed) {
		g_warning ("glibhal: signal_handler_DeviceRemoved when no function!");
		return;
	}
	g_debug ("glibhal: device removed '%s'", udi);
	function.device_removed (udi);
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
	/* assertion checks */
	g_assert (capability);
	g_assert (proxy);
	g_assert (udi);

	if (!function.device_new_capability) {
		g_warning ("glibhal: signal_handler_NewCapability when no function!");
		return;
	}
	g_debug ("glibhal: new capability '%s'", udi);
	function.device_new_capability (udi, capability);
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
	/* assertion checks */
	g_assert (proxy);
	g_assert (name);
	g_assert (details);

	const gchar *udi;
	if (!function.device_condition) {
		g_warning ("glibhal: signal_handler_Condition when no function!");
		return;
	}
	udi = dbus_g_proxy_get_path (proxy);
	g_debug ("glibhal: condition '%s'", udi);
	function.device_condition (udi, name, details);
}

/** NameOwnerChanged signal handler
 *
 *  @param	proxy		A valid DBUS Proxy
 *  @param	name		The Condition name, e.g. ButtonPressed
 *  @param	prev		The previous name
 *  @param	new		The new name
 *  @param	user_data	Unused
 */
static void
signal_handler_NameOwnerChanged (DBusGProxy *proxy, 
	const char *name,
	const char *prev,
	const char *new,
	gpointer user_data)
{
	/* assertion checks */
	g_assert (proxy);
	g_assert (name);
	g_assert (prev);
	g_assert (new);

	if (!function.device_condition) {
		g_warning ("glibhal: signal_handler_Condition when no function!");
		return;
	}
	if (strlen (new) == 0)
		function.device_noc (name, FALSE);
	else if (strlen (prev) == 0)
		function.device_noc (name, TRUE);
}

/** Removed watch removal 
 *
 *  @return			If we removed the watch successfully
 */
static gboolean
glibhal_watch_add_device_removed (void)
{
	DBusGConnection *system_connection;
	GError *error = NULL;

	/* assertion checks */
	g_assert (!function.device_removed);
	g_assert (!reg.device_removed);

	g_debug ("glibhal: DeviceRemoved: Registered");
	reg.device_removed = TRUE;
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;

	proxy.device_removed = dbus_g_proxy_new_for_name_owner (system_connection,
		"org.freedesktop.Hal",
		"/org/freedesktop/Hal/Manager",
		"org.freedesktop.Hal.Manager", &error);
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
glibhal_watch_add_device_new_capability (void)
{
	DBusGConnection *system_connection;
	GError *error = NULL;

	/* assertion checks */
	g_assert (!function.device_new_capability);
	g_assert (!reg.device_new_capability);

	g_debug ("glibhal: NewCapability: Registered");
	reg.device_new_capability = TRUE;
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	proxy.device_new_capability = dbus_g_proxy_new_for_name_owner (system_connection,
		"org.freedesktop.Hal",
		"/org/freedesktop/Hal/Manager",
		"org.freedesktop.Hal.Manager", &error);

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
glibhal_watch_add_device_condition (const gchar *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	int a;
	UdiProxy *udiproxy;

	/* assertion checks */
	g_assert (udi);

	/* need to check for previous add */
	for (a=0;a < proxy.device_condition->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_condition, a);
		if (strcmp (udi, udiproxy->udi) == 0) {
			g_warning ("glibhal: Condition: Already registered UDI '%s'!", udi);
			return FALSE;
		}
	}

	g_debug ("glibhal: Condition: Registered UDI '%s'", udi);
	reg.device_condition = TRUE;
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name_owner  (system_connection,
		"org.freedesktop.Hal",
		udi,
		"org.freedesktop.Hal.Device", &error);

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
glibhal_watch_add_device_property_modified (const gchar *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GType struct_array_type;
	GError *error = NULL;
	int a;
	UdiProxy *udiproxy;

	/* assertion checks */
	g_assert (udi);

	/* need to check for previous add */
	for (a=0;a < proxy.device_property_modified->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_property_modified, a);
		if (strcmp (udi, udiproxy->udi) == 0) {
			g_warning ("glibhal: PropertyModified: Already registered UDI '%s'!", udi);
			return FALSE;
		}
	}
	g_debug ("glibhal: PropertyModified: Registered UDI '%s'", udi);
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name_owner  (system_connection,
		"org.freedesktop.Hal",
		udi,
		"org.freedesktop.Hal.Device", &error);

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

/** NameOwnerChanged watch removal 
 *
 *  @return			If we removed the watch successfully
 */
static gboolean
glibhal_watch_add_noc (void)
{
	DBusGConnection *system_connection;
	GError *error = NULL;

	/* assertion checks */
	g_assert (!function.device_noc);
	g_assert (!reg.device_noc);

	g_debug ("glibhal: NameOwnerChanged: Registered");
	reg.device_noc = TRUE;
	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;

	proxy.device_noc = dbus_g_proxy_new_for_name_owner (system_connection,
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS, &error);
	dbus_g_proxy_add_signal (proxy.device_noc, "NameOwnerChanged",
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy.device_noc, "NameOwnerChanged",
		G_CALLBACK (signal_handler_NameOwnerChanged), NULL, NULL);
	return TRUE;
}


/** DeviceRemoved watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
glibhal_watch_remove_device_removed (void)
{
	if (!proxy.device_removed) {
		g_warning ("glibhal: glibhal_watch_remove_removed when no watch!");
		return FALSE;
	}
	g_debug ("glibhal: watch remove removed");
	g_object_unref (G_OBJECT (proxy.device_removed));
	proxy.device_removed = NULL;
	return TRUE;
}

/** DeviceAdded watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
glibhal_watch_remove_device_added (void)
{
	if (!proxy.device_added) {
		g_warning ("glibhal: glibhal_watch_remove_added when no watch!");
		return FALSE;
	}
	g_debug ("glibhal: watch remove added");
	g_object_unref (G_OBJECT (proxy.device_added));
	proxy.device_added = NULL;
	return TRUE;
}

/** NewCapability watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
glibhal_watch_remove_device_new_capability (void)
{
	if (!proxy.device_new_capability) {
		g_warning ("glibhal: glibhal_watch_remove_new_capability when no watch!");
		return FALSE;
	}
	g_debug ("glibhal: watch remove new_capability");
	g_object_unref (G_OBJECT (proxy.device_new_capability));
	proxy.device_new_capability = NULL;
	return TRUE;
}

/** LostCapability watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
glibhal_watch_remove_device_lost_capability (void)
{
	if (!proxy.device_lost_capability) {
		g_warning ("glibhal: glibhal_watch_remove_lost_capability when no watch!");
		return FALSE;
	}
	g_debug ("glibhal: watch remove lost_capability");
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
glibhal_watch_remove_device_property_modified (const gchar *udi)
{
	int a;

	/* assertion checks */
	g_assert (udi);

	UdiProxy *udiproxy = NULL;
	for (a=0;a < proxy.device_property_modified->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_property_modified, a);
		if (strcmp (udi, udiproxy->udi) == 0)
			break;
	}
	if (a == proxy.device_property_modified->len || !udiproxy) {
		g_warning ("glibhal: glibhal_watch_remove_property_modified when no watch on '%s'!", udi);
		return FALSE;
	}

	g_debug ("glibhal: watch remove property_modified on '%s'", udi);
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
glibhal_watch_remove_device_condition (const gchar *udi)
{
	int a;

	/* assertion checks */
	g_assert (udi);

	UdiProxy *udiproxy = NULL;
	for (a=0;a < proxy.device_condition->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_condition, a);
		if (strcmp (udi, udiproxy->udi) == 0)
			break;
	}
	if (a == proxy.device_condition->len || !udiproxy) {
		g_warning ("glibhal: glibhal_watch_remove_condition when no watch on '%s'!", udi);
		return FALSE;
	}

	g_debug ("glibhal: watch remove condition on '%s'", udi);
	g_object_unref (G_OBJECT (udiproxy->proxy));
	g_ptr_array_remove_fast (proxy.device_condition, (gpointer) udiproxy);
	g_free (udiproxy);

	return TRUE;
}

/** NameOwnerChanged watch callback handler
 *
 *  @return			If we handled the watch callback okay
 */
gboolean
glibhal_watch_remove_noc (void)
{
	if (!proxy.device_noc) {
		g_warning ("glibhal: glibhal_watch_remove_noc when no watch!");
		return FALSE;
	}
	g_debug ("glibhal: watch NameOwnerChanged removed");
	g_object_unref (G_OBJECT (proxy.device_noc));
	proxy.device_noc = NULL;
	return TRUE;
}

/** DeviceRemoved callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
glibhal_method_device_removed (HalDeviceRemoved callback)
{
	/* assertion checks */
	g_assert (callback);

	if (!reg.device_removed)
		glibhal_watch_add_device_removed ();
	function.device_removed = callback;
	return TRUE;
}

/** DeviceAdded callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
glibhal_method_device_added (HalDeviceAdded callback)
{
	/* assertion checks */
	g_assert (callback);

	function.device_added = callback;
	return TRUE;
}

/** NewCapability callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
glibhal_method_device_new_capability (HalDeviceNewCapability callback)
{
	/* assertion checks */
	g_assert (callback);

	if (!reg.device_new_capability)
		glibhal_watch_add_device_new_capability ();
	function.device_new_capability = callback;
	return TRUE;
}

/** LostCapability callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
glibhal_method_device_lost_capability (HalDeviceLostCapability callback)
{
	/* assertion checks */
	g_assert (callback);

	function.device_lost_capability = callback;
	return TRUE;
}

/** PropertyModified callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
glibhal_method_device_property_modified (HalDevicePropertyModified callback)
{
	/* assertion checks */
	g_assert (callback);

	g_debug ("glibhal: PropertyModified: Registered");
	function.device_property_modified = callback;
	return TRUE;
}

/** Condition callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
glibhal_method_device_condition (HalDeviceCondition callback)
{
	/* assertion checks */
	g_assert (callback);

	g_debug ("glibhal: Condition: Registered");
	function.device_condition = callback;
	return TRUE;
}

/** NameOwnerChanged callback assignment
 *
 *  @return			If we assigned the callback okay
 */
gboolean
glibhal_method_noc (HalNameOwnerChanged callback)
{
	/* assertion checks */
	g_assert (callback);

	if (!reg.device_noc)
		glibhal_watch_add_noc ();
	function.device_noc = callback;
	return TRUE;
}

/** Initialise glibhal callback support
 *
 *  @return			If we initialised glibhal callbacks okay
 */
gboolean
glibhal_callback_init (void)
{
	g_debug ("glibhal_callback: init");
	function.device_added = NULL;
	function.device_removed = NULL;
	function.device_new_capability = NULL;
	function.device_lost_capability = NULL;
	function.device_property_modified = NULL;
	function.device_condition = NULL;
	function.device_noc = NULL;

	reg.device_added = FALSE;
	reg.device_removed = FALSE;
	reg.device_new_capability = FALSE;
	reg.device_lost_capability = FALSE;
	reg.device_condition = FALSE;
	reg.device_noc = FALSE;

	proxy.device_added = NULL;
	proxy.device_removed = NULL;
	proxy.device_new_capability = NULL;
	proxy.device_lost_capability = NULL;
	proxy.device_noc = NULL;

	/* array types */
	proxy.device_condition = g_ptr_array_new ();
	proxy.device_property_modified = g_ptr_array_new ();

	return TRUE;
}

/** Shutdown glibhal callback support, freeing memory
 *
 *  @return			If we shutdown glibhal callbacks okay
 */
gboolean
glibhal_callback_shutdown (void)
{
	int a;
	UdiProxy *udiproxy;

	g_debug ("glibhal: shutdown");
	if (proxy.device_added)
		glibhal_watch_remove_device_added ();
	if (proxy.device_removed)
		glibhal_watch_remove_device_removed ();
	if (proxy.device_new_capability)
		glibhal_watch_remove_device_new_capability ();
	if (proxy.device_lost_capability)
		glibhal_watch_remove_device_lost_capability ();
	if (proxy.device_noc)
		glibhal_watch_remove_noc ();

	/* array types */
	for (a=0;a < proxy.device_condition->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_condition, a);
		glibhal_watch_remove_device_condition (udiproxy->udi);
	}
	for (a=0;a < proxy.device_property_modified->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_property_modified, a);
		glibhal_watch_remove_device_property_modified (udiproxy->udi);
	}
	g_ptr_array_free (proxy.device_condition, TRUE);
	g_ptr_array_free (proxy.device_property_modified, TRUE);

	return TRUE;
}
/** @} */
