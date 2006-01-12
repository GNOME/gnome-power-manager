/** @file	gpm-hal.h
 *  @brief	Common HAL functions used by GPM
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-12-18
 *
 * This module uses the functionality of HAL to do some clever
 * things to HAL objects, for example dimming the LCD screen.
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
 * @addtogroup	gpmhal
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "gpm-hal.h"

#define DIM_INTERVAL		10

typedef gboolean (*hal_lp_func) (const gchar *udi, const gint number);

gboolean
gpm_hal_get_dbus_connection (DBusGConnection **connection)
{
	GError *error = NULL;
	DBusGConnection *systemconnection = NULL;

	systemconnection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		g_warning ("gpm_hal_get_dbus_connection: %s", error->message);
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
		g_debug ("Couldn't obtain list of ac_adapters");
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
		g_debug ("system.formfactor not set!"
			 "If you have PMU, please update HAL to get the latest fixes.");
		/* no need to free */
		return FALSE;
	}
	if (strcmp (formfactor, "laptop") != 0) {
		g_debug ("This machine is not identified as a laptop."
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
		g_debug ("Power management type : %s", ptype);
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
		g_warning ("gpm_hal_can_suspend: Key can_suspend_to_ram missing");
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
		g_warning ("gpm_hal_can_hibernate: Key can_suspend_to_disk missing");
		return FALSE;
	}
	return can_hibernate;
}

/** Uses org.freedesktop.Hal.Device.LaptopPanel.SetBrightness ()
 *
 *  @param	brightness	LCD Brightness to set to
 *  @param	udi		A valid HAL UDI
 *  @return			Success
 */
static gboolean
hal_get_brightness_item (const gchar *udi, gint *brightness)
{
	GError *error = NULL;
	gboolean retval;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *hal_proxy = NULL;

	/* assertion checks */
	g_assert (brightness);
	g_assert (udi);

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE,
		udi,
		HAL_DBUS_INTERFACE_LAPTOP_PANEL);
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "GetBrightness", &error,
			G_TYPE_INVALID,
			G_TYPE_UINT, brightness, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("hal_get_brightness_item: %s", error->message);
			g_error_free (error);
		}
		g_warning (HAL_DBUS_INTERFACE_LAPTOP_PANEL ".GetBrightness"
			   "failed (HAL error)");
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Uses org.freedesktop.Hal.Device.LaptopPanel.SetBrightness ()
 *
 *  @param	brightness	LCD Brightness to set to
 *  @param	udi		A valid HAL UDI
 *  @return			Success
 */
static gboolean
gpm_hal_set_brightness_item (const gchar *udi, const gint brightness)
{
	GError *error = NULL;
	gint ret;
	gint levels;
	gboolean retval;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *hal_proxy = NULL;

	/* assertion checks */
	g_assert (udi);

	gpm_hal_device_get_int (udi, "laptop_panel.num_levels", &levels);
	if (brightness >= levels || brightness < 0 ) {
		g_warning ("Tried to set brightness %i outside range (0..%i)",
			brightness, levels - 1);
		return FALSE;
	}
	g_debug ("Setting %s to brightness %i", udi, brightness);

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;
	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
		HAL_DBUS_SERVICE,
		udi,
		HAL_DBUS_INTERFACE_LAPTOP_PANEL);
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "SetBrightness", &error,
			G_TYPE_INT, brightness, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("gpm_hal_set_brightness_item: %s", error->message);
			g_error_free (error);
		}
		g_warning (HAL_DBUS_INTERFACE_LAPTOP_PANEL ".SetBrightness"
			   "failed (HAL error)");
		retval = FALSE;
	}
	if (ret != 0)
		retval = FALSE;
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Calls a function on all udi's of type laptop_panel (utility function)
 *
 *  @param	function	The (char, int) funtion to call
 *  @param	number		The parameter to pass to the funtion
 *  @return			Success
 */
static gboolean
for_each_lp (hal_lp_func function, const gint number)
{
	gint i;
	gchar **names;
	gboolean retval;

	gpm_hal_find_device_capability ("laptop_panel", &names);
	if (!names) {
		g_debug ("No devices of capability laptop_panel");
		return FALSE;
	}
	retval = TRUE;
	/* iterate to seteach laptop_panel object */
	for (i = 0; names[i]; i++)
		if (!function (names[i], number))
			retval = FALSE;
	gpm_hal_free_capability (names);
	return retval;
}

/** Sets the LCD brightness a few steps upwards or downwards
 *
 *  @param	udi		A valid HAL UDI
 *  @param	step		the number of steps to go up or down,
 * 				e.g. -2 is two steps down.
 *  @return			Success
 */
static gboolean
hal_set_brightness_step_item (const gchar *udi, const gint step)
{
	gint brightness;

	/* get current brightness */
	hal_get_brightness_item (udi, &brightness);

	/* make the screen brighter or dimmer */
	brightness = brightness + step;

	return gpm_hal_set_brightness_item (udi, brightness);
}

/** Sets the LCD brightness one step upwards
 *
 *  @return			Success
 */
gboolean
gpm_hal_set_brightness_up (void)
{
	/* do for each panel */
	return for_each_lp ((hal_lp_func) hal_set_brightness_step_item, 1);
}

/** Sets the LCD brightness one step downwards
 *
 *  @return			Success
 */
gboolean
gpm_hal_set_brightness_down (void)
{
	/* do for each panel */
	return for_each_lp ((hal_lp_func) hal_set_brightness_step_item, -1);
}


/** Sets *all* the laptop_panel objects to the required brightness level
 *
 *  @param	brightness	LCD Brightness to set to
 *  @return			Success, true if *all* adaptors changed
 */
gboolean
gpm_hal_set_brightness (gint brightness)
{
	/* do for each panel */
	return for_each_lp ((hal_lp_func) gpm_hal_set_brightness_item, brightness);
}

/** Sets *all* the laptop_panel objects to the required brightness level
 *
 * We also use the fancy new dimming functionality (going through all steps)
 *
 *  @param	brightness	LCD Brightness to set to
 *  @return			Success, true if *all* adaptors changed
 */
gboolean
gpm_hal_set_brightness_dim (gint brightness)
{
	gint i, a;
	gchar **names = NULL;
	gboolean retval;
	gint levels, old_level;
	gchar *returnstring = NULL;

	gpm_hal_find_device_capability ("laptop_panel", &names);
	if (!names) {
		g_debug ("No devices of capability laptop_panel");
		return FALSE;
	}

	/* If the manufacturer is IBM, then assume we are a ThinkPad,
	 * and don't do the new-fangled dimming routine. The ThinkPad dims
	 * gently itself and the two dimming routines just get messy.
	 * This should fix the bug:
	 * https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=173382
	 */
	gpm_hal_device_get_string (HAL_ROOT_COMPUTER, "smbios.system.manufacturer",
			       &returnstring);
	if (returnstring) {
		if (strcmp (returnstring, "IBM") == 0) {
			retval = gpm_hal_set_brightness (brightness);
			g_free (returnstring);
			return retval;
		}
		g_free (returnstring);
	}

	retval = TRUE;
	/* iterate to seteach laptop_panel object */
	for (i = 0; names[i]; i++) {
		/* get old values so we can dim */
		hal_get_brightness_item (names[i], &old_level);
		gpm_hal_device_get_int (names[i], "laptop_panel.num_levels", &levels);
		g_debug ("old_level=%i, levels=%i, new=%i", old_level, levels, brightness);
		/* do each step down to the new value */
		if (brightness < old_level) {
			for (a=old_level-1; a >= brightness; a--) {
				if (!gpm_hal_set_brightness_item (names[i], a))
					break;
				g_usleep (1000 * DIM_INTERVAL);
			}
		} else {
			for (a=old_level+1; a <= brightness; a++) {
				if (!gpm_hal_set_brightness_item (names[i], a))
					break;
				g_usleep (1000 * DIM_INTERVAL);
			}
		}
	}
	gpm_hal_free_capability (names);
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
	gint ret;
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
	if (!dbus_g_proxy_call (hal_proxy, "Suspend", &error,
			G_TYPE_INT, wakeup, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("gpm_hal_suspend: %s", error->message);
			g_error_free (error);
		}
		g_warning (HAL_DBUS_INTERFACE_POWER ".Suspend failed (HAL error)");
		retval = FALSE;
	}
	if (ret != 0)
		retval = FALSE;
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
	gint ret;
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
	if (!dbus_g_proxy_call (hal_proxy, method, &error,
			G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("hal_pm_method_void: %s", error->message);
			g_error_free (error);
		}
		g_warning (HAL_DBUS_INTERFACE_POWER
			   ".%s failed (HAL error)", method);
		retval = FALSE;
	}
	if (ret != 0)
		retval = FALSE;
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
	gint ret;
	DBusGConnection *system_connection = NULL;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	/* abort if we are not a "qualified" laptop */
	if (!gpm_hal_is_laptop ()) {
		g_debug ("We are not a laptop, so not even trying");
		return FALSE;
	}

	if (!gpm_hal_get_dbus_connection (&system_connection))
		return FALSE;

	hal_proxy = dbus_g_proxy_new_for_name (system_connection,
					       HAL_DBUS_SERVICE,
					       HAL_ROOT_COMPUTER,
					       HAL_DBUS_INTERFACE_POWER);
	retval = TRUE;
	if (!dbus_g_proxy_call (hal_proxy, "SetPowerSave", &error,
			G_TYPE_BOOLEAN, enable, G_TYPE_INVALID,
			G_TYPE_UINT, &ret, G_TYPE_INVALID)) {
		if (error) {
			g_warning ("gpm_hal_enable_power_save: %s", error->message);
			g_error_free (error);
		}
		g_debug (HAL_DBUS_INTERFACE_POWER ".SetPowerSave failed (HAL error)");
		retval = FALSE;
	}
	if (ret != 0)
		retval = FALSE;
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/** Gets the number of brightness steps the (first) LCD adaptor supports
 *
 *  @param	steps		The number of steps, returned by ref.
 *  @return			Success, if we found laptop_panel hardware
 */
gboolean
gpm_hal_get_brightness_steps (gint *steps)
{
	gchar **names = NULL;

	/* assertion checks */
	g_assert (steps);

	gpm_hal_find_device_capability ("laptop_panel", &names);
	if (!names || !names[0]) {
		g_debug ("No devices of capability laptop_panel");
		*steps = 0;
		return FALSE;
	}
	/* only use the first one */
	gpm_hal_device_get_int (names[0], "laptop_panel.num_levels", steps);
	gpm_hal_free_capability (names);
	return TRUE;
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
			g_debug ("gpm_hal_device_get_bool: %s", error->message);
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
			g_debug ("gpm_hal_device_get_string: %s", error->message);
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
			g_debug ("gpm_hal_device_get_int: %s", error->message);
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
			g_debug ("gpm_hal_find_device_capability: %s", error->message);
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
		g_debug ("No devices of capability %s", capability);
		return 0;
	}
	/* iterate to find number of items */
	for (i = 0; names[i]; i++) {};
	gpm_hal_free_capability (names);
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
gpm_hal_num_devices_of_capability_with_value (const gchar *capability, 
	const gchar *key, const gchar *value)
{
	gint i;
	gint valid = 0;
	gchar **names;

	gpm_hal_find_device_capability (capability, &names);
	if (!names) {
		g_debug ("No devices of capability %s", capability);
		return 0;
	}
	for (i = 0; names[i]; i++) {
		gchar *type = NULL;
		gpm_hal_device_get_string (names[i], key, &type);
		if (strcmp (type, value) == 0)
			valid++;
		g_free (type);
	};
	gpm_hal_free_capability (names);
	g_debug ("%i devices of capability %s where %s is %s", 
		valid, capability, key, value);
	return valid;
}

/** @} */
