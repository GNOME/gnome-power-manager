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
#include "gpm-common.h"

/** Handle a glib error, freeing if needed
 *
 */
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
 *  @param  value	return value, passed by ref
 *  @return		TRUE for success, FALSE for failure
 */
gboolean
hal_device_get_bool (const gchar *udi, const gchar *key, gboolean *value)
{
	g_return_val_if_fail (udi, FALSE);
	g_return_val_if_fail (key, FALSE);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_DEVICE);
	GError *error = NULL;
	gboolean retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_BOOLEAN, value, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** glib libhal replacement to get string type
 *
 *  @param  udi		The UDI of the device
 *  @param  key		The key to query
 *  @param  value	return value, passed by ref
 *  @return		TRUE for success, FALSE for failure
 */
gboolean
hal_device_get_string (const gchar *udi, const gchar *key, gchar **value)
{
	g_return_val_if_fail (udi, FALSE);
	g_return_val_if_fail (key, FALSE);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_DEVICE);
	GError *error = NULL;
	gboolean retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_STRING, value, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** glib libhal replacement to get integer type
 *
 *  @param  udi		The UDI of the device
 *  @param  key		The key to query
 *  @param  value	return value, passed by ref
 *  @return		TRUE for success, FALSE for failure
 */
gboolean
hal_device_get_int (const gchar *udi, const gchar *key, gint *value)
{
	g_return_val_if_fail (udi, 0);
	g_return_val_if_fail (key, 0);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_DEVICE);
	GError *error = NULL;
	gboolean retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error, 
			G_TYPE_STRING, key, G_TYPE_INVALID,
			G_TYPE_INT, value, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		*value = 0;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** glib libhal replacement to get devices with capability
 *
 *  @param  capability	The capability, e.g. "battery"
 *  @param  value	return value, passed by ref
 *  @return		TRUE for success, FALSE for failure
 */
gboolean
hal_find_device_capability (const gchar *capability, gchar ***value)
{
	g_return_val_if_fail (capability, NULL);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, 
		HAL_DBUS_PATH_MANAGER, 
		HAL_DBUS_INTERFACE_MANAGER);
	GError *error = NULL;
	gboolean retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "FindDeviceByCapability", &error, 
			G_TYPE_STRING, capability, G_TYPE_INVALID,
			G_TYPE_STRV, value, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** frees value result of hal_find_device_capability
 *
 */
void
hal_free_capability (gchar **value)
{
	gint i;
	for (i = 0; value[i]; i++) {
		g_free (value[i]);
	}
	g_free (value);
}

/** Uses org.freedesktop.Hal.Device.LCDPanel.SetBrightness ()
 *
 *  @param  brightness		LCD Brightness to set to
 */
static void
hal_set_brightness_item (const char *udi, int brightness)
{
	GError *error = NULL;
	gint ret;
	g_debug ("Setting %s to brightness %i", udi, brightness);
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_LCD);
	if (!dbus_g_proxy_call (pm_proxy, "SetBrightness", &error, 
			G_TYPE_INT, brightness, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_LCD ".SetBrightness failed (HAL error?)");
	}
	if (ret != 0)
		g_warning (HAL_DBUS_INTERFACE_LCD ".SetBrightness call failed (%i)", ret);
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
}

/** Sets *all* the lcdpanel objects to the required brightness level
 *
 *  @param  brightness		LCD Brightness to set to
 */
void
hal_set_brightness (int brightness)
{
	gint i;
	char **names;
	hal_find_device_capability ("lcdpanel", &names);
	if (!names) {
		g_debug ("No devices of capability lcdpanel");
		return;
	}
	/* iterate to seteach lcdpanel object */
	for (i = 0; names[i]; i++)
		hal_set_brightness_item (names[i], brightness);
	hal_free_capability (names);
}


/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 *
 *  @param  wakeup		Seconds to wakeup, currently unsupported
 */
void
hal_suspend (int wakeup)
{
	GError *error = NULL;
	gint ret;
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, HAL_DBUS_PATH_COMPUTER, HAL_DBUS_INTERFACE_PM);
	if (!dbus_g_proxy_call (pm_proxy, "Suspend", &error, 
			G_TYPE_INT, wakeup, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_PM ".Suspend failed (HAL error?)");
	}
	if (ret != 0)
		g_warning (HAL_DBUS_INTERFACE_PM ".Suspend call failed (%i)", ret);
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 *
 */
void
hal_hibernate (void)
{
	GError *error = NULL;
	gint ret;
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, HAL_DBUS_PATH_COMPUTER, HAL_DBUS_INTERFACE_PM);
	if (!dbus_g_proxy_call (pm_proxy, "Hibernate", &error, 
			G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_PM ".Hibernate failed (HAL error?)");
	}
	if (ret != 0)
		g_warning (HAL_DBUS_INTERFACE_PM ".Hibernate call failed (%i)", ret);
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
}

void
hal_setlowpowermode (gboolean set)
{
	GError *error = NULL;
	gint ret;
	DBusGConnection *system_connection = get_system_connection ();
	DBusGProxy *pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, HAL_DBUS_PATH_COMPUTER, HAL_DBUS_INTERFACE_PM);
	if (!dbus_g_proxy_call (pm_proxy, "SetPowerSave", &error, 
			G_TYPE_BOOLEAN, set, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_PM ".SetPowerSave failed (HAL error?)");
	}
	if (ret != 0)
		g_warning (HAL_DBUS_INTERFACE_PM ".SetPowerSave call failed (%i)", ret);
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
}
