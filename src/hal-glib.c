/***************************************************************************
 *
 * hal-glib.c : GLIB replacement for libhal, unfinished
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
#include <dbus/dbus-glib.h>

void
dbus_glib_error (GError *error)
{
	g_return_if_fail (error);
	if (error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
		g_printerr ("Caught remote method exception %s: %s\n",
					dbus_g_error_get_name (error),
					error->message);
	else
		g_printerr ("Error: %s\n", error->message);
	g_error_free (error);
}

/** Get the system connection, abort if not possible
 *
 *  @return		A valid DBusGConnection
 */
DBusGConnection *
get_system_connection (void)
{
	GError *error = NULL;
	DBusGConnection *connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (connection == NULL) {
		g_printerr ("Failed to open connection to system bus: %s\n", error->message);
		g_error_free (error);
		g_assert_not_reached ();
	}
	return connection;
}

/** Get the session connection, abort if not possible
 *
 *  @return		A valid DBusGConnection
 */
DBusGConnection *
get_session_connection (void)
{
	GError *error = NULL;
	DBusGConnection *connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		g_printerr ("Failed to open connection to session bus: %s\n", error->message);
		g_error_free (error);
		g_assert_not_reached ();
	}
	return connection;
}

/** glib libhal replacement to get boolean type
 *
 *  @param  udi		The UDI of the device
 *  @param  key		The key to query
 */
gboolean
hal_device_get_bool (const gchar *udi, const gchar *key)
{
	g_return_val_if_fail (udi, FALSE);
	g_return_val_if_fail (key, FALSE);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", udi, "org.freedesktop.Hal.Device");
	GError *error = NULL;
	gboolean value = FALSE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_BOOLEAN, &value, G_TYPE_INVALID))
		dbus_glib_error (error);
	g_object_unref (G_OBJECT (hal_proxy));
	return value;
}

/** glib libhal replacement to get string type
 *
 *  @param  udi		The UDI of the device
 *  @param  key		The key to query
 */
gchar *
hal_device_get_string (const gchar *udi, const gchar *key)
{
	g_return_val_if_fail (udi, NULL);
	g_return_val_if_fail (key, NULL);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", udi, "org.freedesktop.Hal.Device");
	GError *error = NULL;
	gchar *value = NULL;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_STRING, &value, G_TYPE_INVALID))
		dbus_glib_error (error);
	g_object_unref (G_OBJECT (hal_proxy));
	return value;
}

/** glib libhal replacement to get integer type
 *
 *  @param  udi		The UDI of the device
 *  @param  key		The key to query
 */
gint
hal_device_get_int (const gchar *udi, const gchar *key)
{
	g_return_val_if_fail (udi, 0);
	g_return_val_if_fail (key, 0);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", udi, "org.freedesktop.Hal.Device");
	GError *error = NULL;
	gint value = 0;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_INT, &value, G_TYPE_INVALID))
		dbus_glib_error (error);
	g_object_unref (G_OBJECT (hal_proxy));
	return value;
}

gchar **
hal_find_device_capability (const gchar *capability)
{
	g_return_val_if_fail (capability, NULL);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		"org.freedesktop.Hal", 
		"/org/freedesktop/Hal/Manager", 
		"org.freedesktop.Hal.Manager");
	GError *error = NULL;
	char **value = 0;
	if (!dbus_g_proxy_call (hal_proxy, "FindDeviceByCapability", &error, 
			G_TYPE_STRING, capability, G_TYPE_INVALID,
			G_TYPE_STRV, &value, G_TYPE_INVALID))
		dbus_glib_error (error);
	g_object_unref (G_OBJECT (hal_proxy));
	return value;
}
