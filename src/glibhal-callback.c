/***************************************************************************
 *
 * glibhal-callbacks.c : GLIB replacement for libhal, providing callbacks
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include <dbus/dbus-glib.h>

#include "dbus-common.h"
#include "glibhal-main.h"
#include "glibhal-callback.h"
#include "gpm_marshal.h"

HalFunctions function;
HalRegistered reg;
HalConnections proxy;

/****************************************************************************
 *    Signal handlers to assign callbacks
 ****************************************************************************/

/* PropertyModified */
static void
signal_handler_PropertyModified (DBusGProxy *proxy, gint type, GPtrArray *properties)
{
	GValueArray *array;
	guint i;
	const char *udi;
	const char *key;
	gboolean added, removed;

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

/* DeviceRemoved */
static void
signal_handler_DeviceRemoved (DBusGProxy *proxy, char *udi)
{
	if (!function.device_removed) {
		g_warning ("glibhal: signal_handler_DeviceRemoved when no function!");
		return;
	}
	g_debug ("glibhal: device removed '%s'", udi);
	function.device_removed (udi);
}

/* NewCapability */
static void
signal_handler_NewCapability (DBusGProxy *proxy, char *udi, char *capability)
{
	if (!function.device_new_capability) {
		g_warning ("glibhal: signal_handler_NewCapability when no function!");
		return;
	}
	g_debug ("glibhal: new capability '%s'", udi);
	function.device_new_capability (udi, capability);
}

/* Condition */
static void
signal_handler_Condition (DBusGProxy *proxy, char *name, char *details)
{
	const char *udi;
	if (!function.device_condition) {
		g_warning ("glibhal: signal_handler_Condition when no function!");
		return;
	}
	udi = dbus_g_proxy_get_path (proxy);
	g_debug ("glibhal: condition '%s'", udi);
	function.device_condition (udi, name, details);
}

/****************************************************************************
 *    Watch add functions to set up DBUS
 ****************************************************************************/

/* DeviceRemoved */
static void
glibhal_watch_add_device_removed (void)
{
	DBusGConnection *system_connection;
	GError *error = NULL;

	g_assert (function.initialized);
	g_assert (!function.device_removed);
	g_assert (!reg.device_removed);

	g_debug ("glibhal: DeviceRemoved: Registered");
	reg.device_removed = TRUE;
	/*dbus_get_system_connection (&system_connection);*/
	system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

	proxy.device_removed = dbus_g_proxy_new_for_name_owner (system_connection,
		"org.freedesktop.Hal", 
		"/org/freedesktop/Hal/Manager", 
		"org.freedesktop.Hal.Manager", &error);
	dbus_g_proxy_add_signal (proxy.device_removed, "DeviceRemoved", 
		G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy.device_removed, "DeviceRemoved", 
		G_CALLBACK (signal_handler_DeviceRemoved), NULL, NULL);
}

/* NewCapability */
static void
glibhal_watch_add_device_new_capability (void)
{
	DBusGConnection *system_connection;
	GError *error = NULL;

	g_assert (function.initialized);
	g_assert (!function.device_new_capability);
	g_assert (!reg.device_new_capability);

	g_debug ("glibhal: NewCapability: Registered");
	reg.device_new_capability = TRUE;
	dbus_get_system_connection (&system_connection);
	proxy.device_new_capability = dbus_g_proxy_new_for_name_owner (system_connection,
		"org.freedesktop.Hal",
		"/org/freedesktop/Hal/Manager", 
		"org.freedesktop.Hal.Manager", &error);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING, G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy.device_new_capability, "NewCapability",
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy.device_new_capability, "NewCapability",
		G_CALLBACK (signal_handler_NewCapability), NULL, NULL);
}

/* Condition */
gboolean
glibhal_watch_add_device_condition (const char *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	int a;
	UdiProxy *udiproxy;

	g_assert (function.initialized);

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
	dbus_get_system_connection (&system_connection);
	hal_proxy = dbus_g_proxy_new_for_name_owner  (system_connection,
		"org.freedesktop.Hal", 
		udi,
		"org.freedesktop.Hal.Device", &error);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING, G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
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

/* PropertyModified */
gboolean
glibhal_watch_add_device_property_modified (const char *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GType struct_array_type;
	GError *error = NULL;
	int a;
	UdiProxy *udiproxy;

	g_assert (function.initialized);

	/* need to check for previous add */
	for (a=0;a < proxy.device_property_modified->len;a++) {
		udiproxy = g_ptr_array_index (proxy.device_property_modified, a);
		if (strcmp (udi, udiproxy->udi) == 0) {
			g_warning ("glibhal: PropertyModified: Already registered UDI '%s'!", udi);
			return FALSE;
		}
	}
	g_debug ("glibhal: PropertyModified: Registered UDI '%s'", udi);
	dbus_get_system_connection (&system_connection);
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

/****************************************************************************
 *    Watch remove functions to pull down DBUS
 ****************************************************************************/

/* DeviceRemoved */
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

gboolean
glibhal_watch_remove_device_added ()
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

gboolean
glibhal_watch_remove_device_new_capability ()
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

gboolean
glibhal_watch_remove_device_lost_capability ()
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

/* PropertyModified */
gboolean
glibhal_watch_remove_device_property_modified (const char *udi)
{
	int a;
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

/* Condition */
gboolean
glibhal_watch_remove_device_condition (const char *udi)
{
	int a;
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

/****************************************************************************
 *    Functions to assign callbacks
 ****************************************************************************/

gboolean
glibhal_method_device_removed (HalDeviceRemoved callback)
{
	g_assert (function.initialized);
	if (!reg.device_removed)
		glibhal_watch_add_device_removed ();
	function.device_removed = callback;
	return TRUE;
}

gboolean
glibhal_method_device_added (HalDeviceAdded callback)
{
	g_assert (function.initialized);
	function.device_added = callback;
	return TRUE;
}

gboolean
glibhal_method_device_new_capability (HalDeviceNewCapability callback)
{
	g_assert (function.initialized);
	if (!reg.device_new_capability)
		glibhal_watch_add_device_new_capability ();
	function.device_new_capability = callback;
	return TRUE;
}

gboolean
glibhal_method_device_lost_capability (HalDeviceLostCapability callback)
{
	g_assert (function.initialized);
	function.device_lost_capability = callback;
	return TRUE;
}

gboolean
glibhal_method_device_property_modified (HalDevicePropertyModified callback)
{
	g_assert (function.initialized);
	g_debug ("glibhal: PropertyModified: Registered");
	function.device_property_modified = callback;
	return TRUE;
}

gboolean
glibhal_method_device_condition (HalDeviceCondition callback)
{
	g_assert (function.initialized);
	g_debug ("glibhal: Condition: Registered");
	function.device_condition = callback;
	return TRUE;
}

/****************************************************************************
 *    General stuff
 ****************************************************************************/

gboolean
glibhal_init (void)
{
	g_debug ("glibhal: init");
	function.initialized = TRUE;

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

	return TRUE;
}

gboolean
glibhal_shutdown (void)
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
