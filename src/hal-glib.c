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
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (udi, FALSE);
	g_return_val_if_fail (key, FALSE);

	system_connection = get_system_connection ();
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
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
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (udi, FALSE);
	g_return_val_if_fail (key, FALSE);

	system_connection = get_system_connection ();
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
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
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (udi, 0);
	g_return_val_if_fail (key, 0);

	system_connection = get_system_connection ();
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
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
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (capability, FALSE);

	system_connection = get_system_connection ();
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, 
		HAL_DBUS_PATH_MANAGER, 
		HAL_DBUS_INTERFACE_MANAGER);
	retval = TRUE;
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
	g_return_if_fail (value);
	for (i = 0; value[i]; i++) {
		g_free (value[i]);
	}
	g_free (value);
}

/** Uses org.freedesktop.Hal.Device.LaptopPanel.SetBrightness ()
 *
 *  @param  brightness		LCD Brightness to set to
 *  @return			Success
 */
static gboolean
hal_set_brightness_item (const char *udi, int brightness)
{
	GError *error = NULL;
	gint ret;
	gboolean retval;
	DBusGConnection *system_connection;
	DBusGProxy *pm_proxy;

	g_return_val_if_fail (udi, FALSE);

	g_debug ("Setting %s to brightness %i", udi, brightness);

	system_connection = get_system_connection ();
	pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_LCD);
	retval = TRUE;
	if (!dbus_g_proxy_call (pm_proxy, "SetBrightness", &error, 
			G_TYPE_INT, brightness, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_LCD ".SetBrightness failed (HAL error?)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning (HAL_DBUS_INTERFACE_LCD ".SetBrightness call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
	return retval;
}

/** Sets *all* the laptop_panel objects to the required brightness level
 *
 *  @param  brightness		LCD Brightness to set to
 *  @return			Success, true if *all* adaptors changed
 */
gboolean
hal_set_brightness (int brightness)
{
	gint i;
	gchar **names;
	gboolean retval;

	hal_find_device_capability ("laptop_panel", &names);
	if (!names) {
		g_debug ("No devices of capability laptop_panel");
		return FALSE;
	}
	retval = TRUE;
	/* iterate to seteach laptop_panel object */
	for (i = 0; names[i]; i++)
		if (!hal_set_brightness_item (names[i], brightness))
			retval = FALSE;
	hal_free_capability (names);
	return retval;
}


/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 *
 *  @param  wakeup		Seconds to wakeup, currently unsupported
 */
gboolean
hal_suspend (int wakeup)
{
	gint ret;
	DBusGConnection *system_connection;
	DBusGProxy *pm_proxy;
	GError *error = NULL;
	gboolean retval;

	system_connection = get_system_connection ();
	pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, HAL_DBUS_PATH_COMPUTER, HAL_DBUS_INTERFACE_PM);
	retval = TRUE;
	if (!dbus_g_proxy_call (pm_proxy, "Suspend", &error, 
			G_TYPE_INT, wakeup, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_PM ".Suspend failed (HAL error?)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning (HAL_DBUS_INTERFACE_PM ".Suspend call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
	return retval;
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 *
 */
gboolean
hal_hibernate (void)
{
	gint ret;
	DBusGConnection *system_connection;
	DBusGProxy *pm_proxy;
	GError *error = NULL;
	gboolean retval;

	system_connection = get_system_connection ();
	pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, HAL_DBUS_PATH_COMPUTER, HAL_DBUS_INTERFACE_PM);
	retval = TRUE;
	if (!dbus_g_proxy_call (pm_proxy, "Hibernate", &error, 
			G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_PM ".Hibernate failed (HAL error?)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning (HAL_DBUS_INTERFACE_PM ".Hibernate call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
	return retval;
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.SetPowerSave ()
 *
 *  @param  set		Set for low power mode
 */
gboolean
hal_setlowpowermode (gboolean set)
{
	gint ret;
	DBusGConnection *system_connection;
	DBusGProxy *pm_proxy;
	GError *error = NULL;
	gboolean retval;

	system_connection = get_system_connection ();
	pm_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE, HAL_DBUS_PATH_COMPUTER, HAL_DBUS_INTERFACE_PM);
	retval = TRUE;
	if (!dbus_g_proxy_call (pm_proxy, "SetPowerSave", &error, 
			G_TYPE_BOOLEAN, set, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		dbus_glib_error (error);
		g_warning (HAL_DBUS_INTERFACE_PM ".SetPowerSave failed (HAL error?)");
		retval = FALSE;
	}
	if (ret != 0) {
		g_warning (HAL_DBUS_INTERFACE_PM ".SetPowerSave call failed (%i)", ret);
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (pm_proxy));
	dbus_g_connection_unref (system_connection);
	return retval;
}

/** Get the number of devices on system with a specific capability
 *
 *  @param  capability		The capability, e.g. "battery"
 *  @return			Number of devices of that capability
 */
gint
hal_num_devices_of_capability (const gchar *capability)
{
	gint i;
	gchar **names;
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
 *  @param  capability		The capability, e.g. "battery"
 *  @param  key			The key to match, e.g. "button.type"
 *  @param  value		The key match, e.g. "power"
 *  @return			Number of devices of that capability
 */
gint
hal_num_devices_of_capability_with_value (const gchar *capability, const gchar *key, const gchar *value)
{
	gint i;
	gint valid = 0;
	gchar **names;
	gchar *type;
	hal_find_device_capability (capability, &names);
	if (!names) {
		g_debug ("No devices of capability %s", capability);
		return 0;
	}
	for (i = 0; names[i]; i++) {
		hal_device_get_string (names[i], key, &type);
		if (strcmp (type, value) == 0)
			valid++;
		g_free (type);
	};
	hal_free_capability (names);
	g_debug ("%i devices of capability %s where %s is %s", valid, capability, key, value);
	return valid;
}

/** Gets the number of brightness steps the (first) LCD adaptor supports
 *
 *  @return  			Number of steps
 */
gint
hal_get_brightness_steps (void)
{
	gchar **names;
	gint levels = 0;
	hal_find_device_capability ("laptop_panel", &names);
	if (!names) {
		g_debug ("No devices of capability laptop_panel");
		return 0;
	}
	/* only use the first one */
	hal_device_get_int (names[0], "laptop_panel.num_levels", &levels);
	hal_free_capability (names);
	return levels;
}
