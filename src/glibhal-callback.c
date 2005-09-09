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

HalContext ctx;

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

	if (!ctx.device_property_modified) {
		g_warning ("signal_handler_PropertyModified when no function!");
		return;
	}

	udi = dbus_g_proxy_get_path (proxy);
	for (i = 0; i < properties->len; i++) {
		array = g_ptr_array_index (properties, i);
		if (array->n_values != 3) {
			g_warning ("array->n_values invalid (!3)");
			return;
		}
		key = g_value_get_string (g_value_array_get_nth (array, 0));
		removed = g_value_get_boolean (g_value_array_get_nth (array, 1));
		added = g_value_get_boolean (g_value_array_get_nth (array, 2));
		ctx.device_property_modified (udi, key, removed, added);
	}
}

/* DeviceRemoved */
static void
signal_handler_DeviceRemoved (DBusGProxy *proxy, char *udi)
{
	if (!ctx.device_removed) {
		g_warning ("signal_handler_DeviceRemoved when no function!");
		return;
	}
	ctx.device_removed (udi);
}

/* NewCapability */
static void
signal_handler_NewCapability (DBusGProxy *proxy, char *udi, char *capability)
{
	if (!ctx.device_new_capability) {
		g_warning ("signal_handler_NewCapability when no function!");
		return;
	}
	ctx.device_new_capability (udi, capability);
}

/* Condition */
static void
signal_handler_Condition (DBusGProxy *proxy, char *name, char *details)
{
	const char *udi;
	if (!ctx.device_condition) {
		g_warning ("signal_handler_Condition when no function!");
		return;
	}
	udi = dbus_g_proxy_get_path (proxy);
	ctx.device_condition (udi, name, details);
}

/****************************************************************************
 *    Register functions to set up DBUS
 ****************************************************************************/

/* DeviceRemoved */
static void
libhal_register_removed (void)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;

	g_assert (ctx.initialized);
	g_assert (!ctx.device_removed);
	g_assert (!ctx.registered_device_removed);

	g_debug ("DeviceRemoved: Registered");
	ctx.registered_device_removed = TRUE;
	/*dbus_get_system_connection (&system_connection);*/
	system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

	hal_proxy = dbus_g_proxy_new_for_name_owner (system_connection,
		"org.freedesktop.Hal", 
		"/org/freedesktop/Hal/Manager", 
		"org.freedesktop.Hal.Manager", &error);
	dbus_g_proxy_add_signal (hal_proxy, "DeviceRemoved", 
		G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (hal_proxy, "DeviceRemoved", 
		G_CALLBACK (signal_handler_DeviceRemoved), NULL, NULL);
#if 0
	g_object_unref (G_OBJECT (hal_proxy));
#endif
}

/* NewCapability */
static void
libhal_register_new_capability (void)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;

	g_assert (ctx.initialized);
	g_assert (!ctx.device_new_capability);
	g_assert (!ctx.registered_device_new_capability);

	g_debug ("NewCapability: Registered");
	ctx.registered_device_new_capability = TRUE;
	dbus_get_system_connection (&system_connection);
	hal_proxy = dbus_g_proxy_new_for_name_owner (system_connection,
		"org.freedesktop.Hal",
		"/org/freedesktop/Hal/Manager", 
		"org.freedesktop.Hal.Manager", &error);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING, G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (hal_proxy, "NewCapability",
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (hal_proxy, "NewCapability",
		G_CALLBACK (signal_handler_NewCapability), NULL, NULL);
#if 0
	g_object_unref (G_OBJECT (hal_proxy));
#endif
}

/* Condition */
void
libhal_register_condition (const char *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;

	g_assert (ctx.initialized);
	g_debug ("Condition: Registered UDI '%s'", udi);
	ctx.registered_device_condition = TRUE;
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
#if 0
	g_object_unref (G_OBJECT (hal_proxy));
#endif
}

/* PropertyModified */
void
libhal_register_property_modified (const char *udi)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GType struct_array_type;
	GError *error = NULL;

	g_assert (ctx.initialized);
	g_debug ("PropertyModified: Registered UDI '%s'", udi);
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
#if 0
	g_object_unref (G_OBJECT (hal_proxy));
#endif
}

/****************************************************************************
 *    Functions to assign callbacks
 ****************************************************************************/

gboolean
libhal_device_removed (HalDeviceRemoved callback)
{
	g_assert (ctx.initialized);
	if (!ctx.registered_device_removed)
		libhal_register_removed ();
	ctx.device_removed = callback;
	return TRUE;
}

gboolean
libhal_device_added (HalDeviceAdded callback)
{
	g_assert (ctx.initialized);
	ctx.device_added = callback;
	return TRUE;
}

gboolean
libhal_device_new_capability (HalDeviceNewCapability callback)
{
	g_assert (ctx.initialized);
	if (!ctx.registered_device_new_capability)
		libhal_register_new_capability ();
	ctx.device_new_capability = callback;
	return TRUE;
}

gboolean
libhal_device_lost_capability (HalDeviceLostCapability callback)
{
	g_assert (ctx.initialized);
	ctx.device_lost_capability = callback;
	return TRUE;
}

gboolean
libhal_device_property_modified (HalDevicePropertyModified callback)
{
	g_assert (ctx.initialized);
	g_debug ("PropertyModified: Registered");
	ctx.device_property_modified = callback;
	return TRUE;
}

gboolean
libhal_device_condition (HalDeviceCondition callback)
{
	g_assert (ctx.initialized);
	g_debug ("Condition: Registered");
	ctx.device_condition = callback;
	return TRUE;
}

/****************************************************************************
 *    General stuff
 ****************************************************************************/

gboolean
libhal_glib_init (void)
{
	ctx.initialized = TRUE;
	ctx.device_added = NULL;
	ctx.device_removed = NULL;
	ctx.device_new_capability = NULL;
	ctx.device_lost_capability = NULL;
	ctx.device_property_modified = NULL;
	ctx.device_condition = NULL;

	ctx.registered_device_added = FALSE;
	ctx.registered_device_removed = FALSE;
	ctx.registered_device_new_capability = FALSE;
	ctx.registered_device_lost_capability = FALSE;
	ctx.registered_device_condition = FALSE;
	return TRUE;
}
