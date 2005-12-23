/** @file	glibhal-main.c
 *  @brief	GLIB replacement for libhal, the extra stuff
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module is a direct replacement for libhal, and allows the user to
 * query and set keys in a more glib way than using libhal.
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
 * @addtogroup	glibhal		GLIB based HAL methods
 * @brief			This provides more sanitied and GNOME friendly
 * 				interface to query and command HAL.
 *
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

/** Finds out if hal is running
 *
 *  @return		TRUE if haldaemon is running
 */
gboolean
is_hald_running (void)
{
	gchar *udi = NULL;
	gboolean running;
	running = hal_device_get_string (
		"/org/freedesktop/Hal/devices/computer",
		"info.udi", &udi);
	g_free (udi);
	return running;
}

/** glib libhal replacement to get boolean type
 *
 *  @param	udi		The UDI of the device
 *  @param	key		The key to query
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 */
gboolean
hal_device_get_bool (const gchar *udi, const gchar *key, gboolean *value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	/* assertion checks */
	g_assert (udi);
	g_assert (key);
	g_assert (value);

	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", udi, "org.freedesktop.Hal.Device");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_BOOLEAN, value, G_TYPE_INVALID)) {
		gpm_dbus_glib_error (error);
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** glib libhal replacement to get string type

 *  @param	udi		The UDI of the device
 *  @param	key		The key to query
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 *
 *  @note	You must g_free () the return value.
 */
gboolean
hal_device_get_string (const gchar *udi, const gchar *key, gchar **value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	/* assertion checks */
	g_assert (udi);
	g_assert (key);
	g_assert (value);

	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", udi, "org.freedesktop.Hal.Device");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_STRING, value, G_TYPE_INVALID)) {
		gpm_dbus_glib_error (error);
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** glib libhal replacement to get integer type
 *
 *  @param	udi		The UDI of the device
 *  @param	key		The key to query
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 */
gboolean
hal_device_get_int (const gchar *udi, const gchar *key, gint *value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	/* assertion checks */
	g_assert (udi);
	g_assert (key);
	g_assert (value);

	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", udi, "org.freedesktop.Hal.Device");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_INT, value, G_TYPE_INVALID)) {
		gpm_dbus_glib_error (error);
		*value = 0;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** glib libhal replacement to get devices with capability
 *
 *  @param	capability	The capability, e.g. "battery"
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 */
gboolean
hal_find_device_capability (const gchar *capability, gchar ***value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	/* assertion checks */
	g_assert (capability);
	g_assert (value);

	if (!gpm_dbus_get_system_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", 
		"/org/freedesktop/Hal/Manager", 
		"org.freedesktop.Hal.Manager");
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "FindDeviceByCapability", &error, 
			G_TYPE_STRING, capability, G_TYPE_INVALID,
			G_TYPE_STRV, value, G_TYPE_INVALID)) {
		gpm_dbus_glib_error (error);
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** frees value result of hal_find_device_capability
 *
 *  @param	value		The list of strings to free
 */
void
hal_free_capability (gchar **value)
{
	gint i;

	/* assertion checks */
	g_assert (value);

	for (i=0; value[i]; i++)
		g_free (value[i]);
	g_free (value);
}

/** Get the number of devices on system with a specific capability
 *
 *  @param	capability	The capability, e.g. "battery"
 *  @return			Number of devices of that capability
 */
gint
hal_num_devices_of_capability (const gchar *capability)
{
	gint i;
	gchar **names;

	/* assertion checks */
	g_assert (capability);

	hal_find_device_capability (capability, &names);
	if (!names) {
		g_debug ("No devices of capability %s", capability);
		return 0;
	}
	/* iterate to find number of items */
	for (i = 0; names[i]; i++) {};
	hal_free_capability (names);
	g_debug ("%i devices of capability %s", i, capability);
	return i;
}

/** Get the number of devices on system with a specific capability
 *
 *  @param	capability	The capability, e.g. "battery"
 *  @param	key		The key to match, e.g. "button.type"
 *  @param	value		The key match, e.g. "power"
 *  @return			Number of devices of that capability
 */
gint
hal_num_devices_of_capability_with_value (const gchar *capability, 
	const gchar *key, const gchar *value)
{
	gint i;
	gint valid = 0;
	gchar **names;

	/* assertion checks */
	g_assert (capability);
	g_assert (key);
	g_assert (value);

	hal_find_device_capability (capability, &names);
	if (!names) {
		g_debug ("No devices of capability %s", capability);
		return 0;
	}
	for (i = 0; names[i]; i++) {
		gchar *type = NULL;
		hal_device_get_string (names[i], key, &type);
		if (strcmp (type, value) == 0)
			valid++;
		g_free (type);
	};
	hal_free_capability (names);
	g_debug ("%i devices of capability %s where %s is %s", 
		valid, capability, key, value);
	return valid;
}
/** @} */
