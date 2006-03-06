/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "gpm-hal.h"
#include "gpm-debug.h"

#define DIM_INTERVAL		10

typedef gboolean (*hal_lp_func) (const gchar *udi, const gint number);

gboolean
gpm_hal_get_dbus_connection (DBusGConnection **connection)
{
	GError *error = NULL;
	DBusGConnection *systemconnection = NULL;

	systemconnection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		gpm_warning ("gpm_hal_get_dbus_connection: %s", error->message);
		g_error_free (error);
		return FALSE;
	}
	*connection = systemconnection;
	return TRUE;
}

/** Finds out if hal is running
 *
 *  @return		TRUE if haldaemon is running
 */
gboolean
gpm_hal_is_running (void)
{
	gchar *udi = NULL;
	gboolean running;
	running = gpm_hal_device_get_string (HAL_ROOT_COMPUTER, "info.udi", &udi);
	g_free (udi);
	return running;
}

/** Returns true if running on ac
 *
 *  @return			TRUE is computer is running on AC
 */
gboolean
gpm_hal_is_on_ac (void)
{
	gboolean is_on_ac;
	gchar **device_names = NULL;

	/* find ac_adapter */
	gpm_hal_find_device_capability ("ac_adapter", &device_names);
	if (!device_names || !device_names[0]) {
		gpm_debug ("Couldn't obtain list of ac_adapters");
		/*
		 * If we do not have an AC adapter, then assume we are a
		 * desktop and return true
		 */
		return TRUE;
	}
	/* assume only one */
	gpm_hal_device_get_bool (device_names[0], "ac_adapter.present", &is_on_ac);
	gpm_hal_free_capability (device_names);
	return is_on_ac;
}


/** Returns true if system.formfactor == "laptop"
 *
 *  @return			TRUE is computer is identified as a laptop
 */
gboolean
gpm_hal_is_laptop (void)
{
	gboolean ret = TRUE;
	gchar *formfactor = NULL;

	/* always present */
	gpm_hal_device_get_string (HAL_ROOT_COMPUTER, "system.formfactor", &formfactor);
	if (!formfactor) {
		gpm_debug ("system.formfactor not set! "
			   "If you have PMU, please update HAL to get the latest fixes.");
		/* no need to free */
		return FALSE;
	}
	if (strcmp (formfactor, "laptop") != 0) {
		gpm_debug ("This machine is not identified as a laptop."
			   "system.formfactor is %s.", formfactor);
		ret = FALSE;
	}
	g_free (formfactor);
	return ret;
}

/** Finds out if power management functions are running (only ACPI, PMU, APM)
 *
 *  @return		TRUE if haldaemon has power management capability
 */
gboolean
gpm_hal_has_power_management (void)
{
	gchar *ptype = NULL;
	gpm_hal_device_get_string (HAL_ROOT_COMPUTER, "power_management.type", &ptype);
	/* this key only has to exist to be pm okay */
	if (ptype) {
		gpm_debug ("Power management type : %s", ptype);
		g_free (ptype);
		return TRUE;
	}
	return FALSE;
}

/** Finds out if HAL indicates that we can suspend
 *
 *  @return		TRUE if kernel suspend support is compiled in
 */
gboolean
gpm_hal_can_suspend (void)
{
	gboolean exists;
	gboolean can_suspend;
	exists = gpm_hal_device_get_bool (HAL_ROOT_COMPUTER,
					  "power_management.can_suspend_to_ram",
					  &can_suspend);
	if (!exists) {
		gpm_warning ("gpm_hal_can_suspend: Key can_suspend_to_ram missing");
		return FALSE;
	}
	return can_suspend;
}

/** Finds out if HAL indicates that we can hibernate
 *
 *  @return		TRUE if kernel hibernation support is compiled in
 */
gboolean
gpm_hal_can_hibernate (void)
{
	gboolean exists;
	gboolean can_hibernate;
	exists = gpm_hal_device_get_bool (HAL_ROOT_COMPUTER,
					  "power_management.can_suspend_to_disk",
					  &can_hibernate);
	if (!exists) {
		gpm_warning ("gpm_hal_can_hibernate: Key can_suspend_to_disk missing");
		return FALSE;
	}
	return can_hibernate;
}

/* we have to be clever, as hal can pass back two types of errors, and we have
   to ignore dbus timeouts */
static gboolean
gpm_hal_handle_error (guint ret, GError *error, const char *method)
{
	gboolean retval = TRUE;
	if (error) {
		/* DBUS might time out, which is okay. We can remove this code
		   when the dbus glib bindings are fixed. See #332888 */
		if (g_error_matches (error, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
			gpm_debug ("DBUS timed out, but recovering");
			retval = TRUE;
		} else {
			gpm_warning ("%s failed\n(%s)",
				     method,
				     error->message);
			retval = FALSE;
		}
		g_error_free (error);
	} else if (ret != 0) {
		/* we might not get an error set */
		gpm_warning ("%s failed (Unknown error)", method);
		retval = FALSE;
	}
	return retval;
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 *
 *  @param	wakeup		Seconds to wakeup, currently unsupported
 *  @return			Success, true if we suspended OK
 */
gboolean
gpm_hal_suspend (gint wakeup)
{
	guint ret = 0;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       HAL_ROOT_COMPUTER,
					       HAL_DBUS_INTERFACE_POWER);
	retval = TRUE;
	dbus_g_proxy_call (hal_proxy, "Suspend", &error,
			   G_TYPE_INT, wakeup, G_TYPE_INVALID,
			   G_TYPE_UINT, &ret, G_TYPE_INVALID);
	retval = gpm_hal_handle_error (ret, error, "suspend");

	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Do a method on org.freedesktop.Hal.Device.SystemPowerManagement.*
 *  with no arguments.
 *
 *  @param	method		The method name, e.g. "Hibernate"
 *  @return			Success, true if we did OK
 */
static gboolean
hal_pm_method_void (const gchar* method)
{
	guint ret = 0;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       HAL_ROOT_COMPUTER,
					       HAL_DBUS_INTERFACE_POWER);
	retval = TRUE;
	dbus_g_proxy_call (hal_proxy, method, &error,
			   G_TYPE_INVALID,
			   G_TYPE_UINT, &ret, G_TYPE_INVALID);
	retval = gpm_hal_handle_error (ret, error, method);

	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 *
 *  @return			Success, true if we hibernated OK
 */
gboolean
gpm_hal_hibernate (void)
{
	return hal_pm_method_void ("Hibernate");
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.Shutdown ()
 *
 *  @return			Success, true if we shutdown OK
 */
gboolean
gpm_hal_shutdown (void)
{
	return hal_pm_method_void ("Shutdown");
}

/** Uses org.freedesktop.Hal.Device.SystemPowerManagement.SetPowerSave ()
 *
 *  @param	enable		True to enable low power mode
 *  @return			Success, true if we set the mode
 */
gboolean
gpm_hal_enable_power_save (gboolean enable)
{
	gint ret = 0;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	/* abort if we are not a "qualified" laptop */
	if (!gpm_hal_is_laptop ()) {
		gpm_debug ("We are not a laptop, so not even trying");
		return FALSE;
	}

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;

	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       HAL_ROOT_COMPUTER,
					       HAL_DBUS_INTERFACE_POWER);
	retval = TRUE;
	dbus_g_proxy_call (hal_proxy, "SetPowerSave", &error,
			   G_TYPE_BOOLEAN, enable, G_TYPE_INVALID,
			   G_TYPE_UINT, &ret, G_TYPE_INVALID);
	retval = gpm_hal_handle_error (ret, error, "power save");

	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** HAL: get boolean type
 *
 *  @param	udi		The UDI of the device
 *  @param	key		The key to query
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 */
gboolean
gpm_hal_device_get_bool (const gchar *udi, const gchar *key, gboolean *value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error,
				G_TYPE_STRING, key, G_TYPE_INVALID,
				G_TYPE_BOOLEAN, value, G_TYPE_INVALID)) {
		if (error) {
			gpm_debug ("Error: %s", error->message);
			g_error_free (error);
		}
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** HAL: get string type

 *  @param	udi		The UDI of the device
 *  @param	key		The key to query
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 *
 *  @note	You must g_free () the return value.
 */
gboolean
gpm_hal_device_get_string (const gchar *udi, const gchar *key, gchar **value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error,
				G_TYPE_STRING, key, G_TYPE_INVALID,
				G_TYPE_STRING, value, G_TYPE_INVALID)) {
		if (error) {
			gpm_debug ("Error: %s", error->message);
			g_error_free (error);
		}
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** HAL: get integer type
 *
 *  @param	udi		The UDI of the device
 *  @param	key		The key to query
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 */
gboolean
gpm_hal_device_get_int (const gchar *udi, const gchar *key, gint *value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error,
				G_TYPE_STRING, key, G_TYPE_INVALID,
				G_TYPE_INT, value, G_TYPE_INVALID)) {
		if (error) {
			gpm_debug ("Error: %s", error->message);
			g_error_free (error);
		}
		*value = 0;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** HAL: get devices with capability
 *
 *  @param	capability	The capability, e.g. "battery"
 *  @param	value		return value, passed by ref
 *  @return			TRUE for success, FALSE for failure
 */
gboolean
gpm_hal_find_device_capability (const gchar *capability, gchar ***value)
{
	DBusGConnection *system_connection;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       HAL_DBUS_PATH_MANAGER,
					       HAL_DBUS_INTERFACE_MANAGER);
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "FindDeviceByCapability", &error,
				G_TYPE_STRING, capability, G_TYPE_INVALID,
				G_TYPE_STRV, value, G_TYPE_INVALID)) {
		if (error) {
			gpm_debug ("Error: %s", error->message);
			g_error_free (error);
		}
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** frees value result of gpm_hal_find_device_capability
 *
 *  @param	value		The list of strings to free
 */
void
gpm_hal_free_capability (gchar **value)
{
	gint i;
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
gpm_hal_num_devices_of_capability (const gchar *capability)
{
	gint i;
	gchar **names;

	gpm_hal_find_device_capability (capability, &names);
	if (!names) {
		gpm_debug ("No devices of capability %s", capability);
		return 0;
	}
	/* iterate to find number of items */
	for (i = 0; names[i]; i++) {};
	gpm_hal_free_capability (names);
	gpm_debug ("%i devices of capability %s", i, capability);
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
gpm_hal_num_devices_of_capability_with_value (const gchar *capability,
	const gchar *key, const gchar *value)
{
	gint i;
	gint valid = 0;
	gchar **names;

	gpm_hal_find_device_capability (capability, &names);
	if (!names) {
		gpm_debug ("No devices of capability %s", capability);
		return 0;
	}
	for (i = 0; names[i]; i++) {
		gchar *type = NULL;
		gpm_hal_device_get_string (names[i], key, &type);
		if (type != NULL) {
			if (strcmp (type, value) == 0)
				valid++;
			g_free (type);
		}
	}
	gpm_hal_free_capability (names);
	gpm_debug ("%i devices of capability %s where %s is %s",
		   valid, capability, key, value);
	return valid;
}
