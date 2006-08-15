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

#include "config.h"

#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-marshal.h"
#include "gpm-hal.h"
#include "gpm-debug.h"
#include "gpm-dbus-system-monitor.h"

static void     gpm_hal_class_init (GpmHalClass *klass);
static void     gpm_hal_init       (GpmHal      *hal);
static void     gpm_hal_finalize   (GObject	*object);

#define GPM_HAL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL, GpmHalPrivate))

struct GpmHalPrivate
{
	gboolean		 is_connected;
	GpmDbusSystemMonitor	*dbus_system;
	DBusGConnection		*connection;
	DBusGProxy		*manager_proxy;
	GHashTable		*watch_device_property_modified;
	GHashTable		*watch_device_condition;
};

/* Signals emitted from GpmHal are:
 *
 * device-added
 * device-removed
 * device-property-modified
 * device-condition
 * new-capability
 * lost-capability
 * daemon-start
 * daemon-stop
 */
enum {
	DEVICE_ADDED,
	DEVICE_REMOVED,
	DEVICE_PROPERTY_MODIFIED,
	DEVICE_CONDITION,
	NEW_CAPABILITY,
	LOST_CAPABILITY,
	DAEMON_START,
	DAEMON_STOP,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };
static gpointer      gpm_hal_object = NULL;

G_DEFINE_TYPE (GpmHal, gpm_hal, G_TYPE_OBJECT)

/**
 * gpm_hal_is_running:
 *
 * @hal: This hal class instance
 * Return value: TRUE if haldaemon is running
 *
 * Finds out if hal is running
 **/
gboolean
gpm_hal_is_running (GpmHal *hal)
{
	gchar *udi = NULL;
	gboolean running;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	running = gpm_hal_device_get_string (hal, HAL_ROOT_COMPUTER, "info.udi", &udi);
	g_free (udi);
	return running;
}

/**
 * gpm_hal_is_on_ac:
 *
 * @hal: This hal class instance
 * Return value: TRUE is computer is running on AC
 **/
gboolean
gpm_hal_is_on_ac (GpmHal *hal)
{
	gboolean is_on_ac;
	gchar **device_names = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	/* find ac_adapter */
	gpm_hal_device_find_capability (hal, "ac_adapter", &device_names);
	if (device_names == NULL || device_names[0] == NULL) {
		gpm_debug ("Couldn't obtain list of ac_adapters");
		/* If we do not have an AC adapter, then assume we are a
		 * desktop and return true */
		return TRUE;
	}
	/* assume only one */
	gpm_hal_device_get_bool (hal, device_names[0], "ac_adapter.present", &is_on_ac);
	gpm_hal_free_capability (hal, device_names);
	return is_on_ac;
}

/**
 * gpm_hal_is_laptop:
 *
 * @hal: This hal class instance
 * Return value: TRUE is computer is identified as a laptop
 *
 * Returns true if system.formfactor is "laptop"
 **/
gboolean
gpm_hal_is_laptop (GpmHal *hal)
{
	gboolean ret = TRUE;
	gchar *formfactor = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	/* always present */
	gpm_hal_device_get_string (hal, HAL_ROOT_COMPUTER, "system.formfactor", &formfactor);
	if (formfactor == NULL) {
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

/**
 * gpm_hal_has_power_management:
 *
 * @hal: This hal class instance
 * Return value: TRUE if haldaemon has power management capability
 *
 * Finds out if power management functions are running (only ACPI, PMU, APM)
 **/
gboolean
gpm_hal_has_power_management (GpmHal *hal)
{
	gchar *ptype = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	gpm_hal_device_get_string (hal, HAL_ROOT_COMPUTER, "power_management.type", &ptype);
	/* this key only has to exist to be pm okay */
	if (ptype) {
		gpm_debug ("Power management type : %s", ptype);
		g_free (ptype);
		return TRUE;
	}
	return FALSE;
}

/**
 * gpm_hal_can_suspend:
 *
 * @hal: This hal class instance
 * Return value: TRUE if kernel suspend support is compiled in
 *
 * Finds out if HAL indicates that we can suspend
 **/
gboolean
gpm_hal_can_suspend (GpmHal *hal)
{
	gboolean exists;
	gboolean can_suspend;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	/* TODO: Change to can_suspend when rely on newer HAL */
	exists = gpm_hal_device_get_bool (hal, HAL_ROOT_COMPUTER,
					  "power_management.can_suspend_to_ram",
					  &can_suspend);
	if (exists == FALSE) {
		gpm_warning ("gpm_hal_can_suspend: Key can_suspend_to_ram missing");
		return FALSE;
	}
	return can_suspend;
}

/**
 * gpm_hal_can_hibernate:
 *
 * @hal: This hal class instance
 * Return value: TRUE if kernel hibernation support is compiled in
 *
 * Finds out if HAL indicates that we can hibernate
 **/
gboolean
gpm_hal_can_hibernate (GpmHal *hal)
{
	gboolean exists;
	gboolean can_hibernate;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	/* TODO: Change to can_hibernate when rely on newer HAL */
	exists = gpm_hal_device_get_bool (hal, HAL_ROOT_COMPUTER,
					  "power_management.can_suspend_to_disk",
					  &can_hibernate);
	if (exists == FALSE) {
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

	g_return_val_if_fail (method != NULL, FALSE);

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

/**
 * gpm_hal_suspend:
 *
 * @hal: This hal class instance
 * @wakeup: Seconds to wakeup, currently unsupported
 * Return value: Success, true if we suspended OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 **/
gboolean
gpm_hal_suspend (GpmHal *hal, gint wakeup)
{
	guint ret = 0;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
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

/**
 * hal_pm_method_void:
 *
 * @hal: This hal class instance
 * @method: The method name, e.g. "Hibernate"
 * Return value: Success, true if we did OK
 *
 * Do a method on org.freedesktop.Hal.Device.SystemPowerManagement.*
 * with no arguments.
 **/
static gboolean
hal_pm_method_void (GpmHal *hal, const gchar* method)
{
	guint ret = 0;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (method != NULL, FALSE);

	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
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

/**
 * gpm_hal_hibernate:
 *
 * @hal: This hal class instance
 * Return value: Success, true if we hibernated OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 **/
gboolean
gpm_hal_hibernate (GpmHal *hal)
{
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	return hal_pm_method_void (hal, "Hibernate");
}

/**
 * gpm_hal_shutdown:
 *
 * Return value: Success, true if we shutdown OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Shutdown ()
 **/
gboolean
gpm_hal_shutdown (GpmHal *hal)
{
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	return hal_pm_method_void (hal, "Shutdown");
}

/**
 * gpm_hal_reboot:
 *
 * @hal: This hal class instance
 * Return value: Success, true if we shutdown OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Reboot ()
 **/
gboolean
gpm_hal_reboot (GpmHal *hal)
{
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	return hal_pm_method_void (hal, "Reboot");
}

/**
 * gpm_hal_enable_power_save:
 *
 * @hal: This hal class instance
 * @enable: True to enable low power mode
 * Return value: Success, true if we set the mode
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.SetPowerSave ()
 **/
gboolean
gpm_hal_enable_power_save (GpmHal *hal, gboolean enable)
{
	gint ret = 0;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	/* abort if we are not a "qualified" laptop */
	if (gpm_hal_is_laptop (hal) == FALSE) {
		gpm_debug ("We are not a laptop, so not even trying");
		return FALSE;
	}

	gpm_debug ("Doing SetPowerSave (%i)", enable);
	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
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

/**
 * gpm_hal_device_rescan:
 *
 * @hal: This hal class instance
 * @udi: The HAL UDI
 * Return value: Success, true if we rescan'd
 *
 * Rescans a HAL device manually
 **/
static gboolean
gpm_hal_device_rescan (GpmHal *hal, const char *udi)
{
	gint ret = 0;
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	gpm_debug ("Doing Rescan (%s)", udi);
	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
	dbus_g_proxy_call (hal_proxy, "Rescan", &error,
			   G_TYPE_INVALID,
			   G_TYPE_BOOLEAN, &ret, G_TYPE_INVALID);
	retval = gpm_hal_handle_error (ret, error, "rescan");

	g_object_unref (G_OBJECT (hal_proxy));
	return retval;

}

/**
 * gpm_hal_device_rescan_capability:
 *
 * @hal: This hal class instance
 * @capability: The HAL capability, e.g. laptop_panel
 * Return value: Success, true if we found and devices
 *
 * Rescans all devices of a specific capability manually
 **/
gboolean
gpm_hal_device_rescan_capability (GpmHal *hal, const char *capability)
{
	gchar **devices;
	int i;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (capability != NULL, FALSE);

	gpm_hal_device_find_capability (hal, capability, &devices);
	if (devices == NULL) {
		gpm_debug ("No devices of capability %s", capability);
		return FALSE;
	}
	for (i = 0; devices[i]; i++) {
		gpm_hal_device_rescan (hal, devices[i]);
	}
	gpm_hal_free_capability (hal, devices);
	return TRUE;
}

/**
 * gpm_hal_device_get_bool:
 *
 * @hal: This hal class instance
 * @udi: The UDI of the device
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_hal_device_get_bool (GpmHal      *hal,
			 const gchar *udi,
			 const gchar *key,
			 gboolean    *value)
{
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
	if (dbus_g_proxy_call (hal_proxy, "GetPropertyBoolean", &error,
			       G_TYPE_STRING, key, G_TYPE_INVALID,
			       G_TYPE_BOOLEAN, value, G_TYPE_INVALID) == FALSE) {
		if (error) {
			gpm_debug ("%s", error->message);
			g_error_free (error);
		}
		*value = FALSE;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/**
 * gpm_hal_device_get_string:
 *
 * @hal: This hal class instance
 * @udi: The UDI of the device
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 *
 * You must g_free () the return value.
 **/
gboolean
gpm_hal_device_get_string (GpmHal      *hal,
			   const gchar *udi,
			   const gchar *key,
			   gchar      **value)
{
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
	if (dbus_g_proxy_call (hal_proxy, "GetPropertyString", &error,
			       G_TYPE_STRING, key, G_TYPE_INVALID,
			       G_TYPE_STRING, value, G_TYPE_INVALID) == FALSE) {
		if (error) {
			gpm_debug ("%s", error->message);
			g_error_free (error);
		}
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/**
 * gpm_hal_device_get_int:
 *
 * @hal: This hal class instance
 * @udi: The UDI of the device
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_hal_device_get_int (GpmHal      *hal,
			const gchar *udi,
			const gchar *key,
			gint        *value)
{
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	retval = TRUE;
	if (dbus_g_proxy_call (hal_proxy, "GetPropertyInteger", &error,
				G_TYPE_STRING, key, G_TYPE_INVALID,
				G_TYPE_INT, value, G_TYPE_INVALID) == FALSE) {
		if (error) {
			gpm_debug ("%s", error->message);
			g_error_free (error);
		}
		*value = 0;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/**
 * gpm_hal_device_find_capability:
 *
 * @hal: This hal class instance
 * @capability: The capability, e.g. "battery"
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_hal_device_find_capability (GpmHal      *hal,
				const gchar *capability,
				gchar     ***value)
{
	DBusGProxy *hal_proxy = NULL;
	GError *error = NULL;
	gboolean retval;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (capability != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	hal_proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       HAL_DBUS_PATH_MANAGER,
					       HAL_DBUS_INTERFACE_MANAGER);
	retval = TRUE;
	if (dbus_g_proxy_call (hal_proxy, "FindDeviceByCapability", &error,
			        G_TYPE_STRING, capability, G_TYPE_INVALID,
			        G_TYPE_STRV, value, G_TYPE_INVALID) == FALSE) {
		if (error) {
			gpm_debug ("%s", error->message);
			g_error_free (error);
		}
		*value = NULL;
		retval = FALSE;
	}
	g_object_unref (G_OBJECT (hal_proxy));
	return retval;
}

/**
 * gpm_hal_free_capability:
 *
 * @hal: This hal class instance
 * @value: The list of strings to free
 *
 * Frees value result of gpm_hal_device_find_capability
 **/
void
gpm_hal_free_capability (GpmHal *hal, gchar **value)
{
	gint i;

	g_return_if_fail (GPM_IS_HAL (hal));
	g_return_if_fail (value != NULL);

	for (i=0; value[i]; i++) {
		g_free (value[i]);
	}
	g_free (value);
}

/**
 * gpm_hal_num_devices_of_capability:
 *
 * @hal: This hal class instance
 * @capability: The capability, e.g. "battery"
 * Return value: Number of devices of that capability
 *
 * Get the number of devices on system with a specific capability
 **/
gint
gpm_hal_num_devices_of_capability (GpmHal *hal, const gchar *capability)
{
	gint i;
	gchar **names;

	g_return_val_if_fail (GPM_IS_HAL (hal), 0);
	g_return_val_if_fail (capability != NULL, 0);

	gpm_hal_device_find_capability (hal, capability, &names);
	if (names == NULL) {
		gpm_debug ("No devices of capability %s", capability);
		return 0;
	}
	/* iterate to find number of items */
	for (i = 0; names[i]; i++) {};
	gpm_hal_free_capability (hal, names);
	gpm_debug ("%i devices of capability %s", i, capability);
	return i;
}

/**
 * gpm_hal_num_devices_of_capability_with_value:
 *
 * @hal: This hal class instance
 * @capability: The capability, e.g. "battery"
 * @key: The key to match, e.g. "button.type"
 * @value: The key match, e.g. "power"
 * Return value: Number of devices of that capability
 *
 * Get the number of devices on system with a specific capability and key value
 **/
gint
gpm_hal_num_devices_of_capability_with_value (GpmHal      *hal,
					      const gchar *capability,
					      const gchar *key,
					      const gchar *value)
{
	gint i;
	gint valid = 0;
	gchar **names;

	g_return_val_if_fail (GPM_IS_HAL (hal), 0);
	g_return_val_if_fail (capability != NULL, 0);
	g_return_val_if_fail (key != NULL, 0);
	g_return_val_if_fail (value != NULL, 0);

	gpm_hal_device_find_capability (hal, capability, &names);
	if (names == NULL) {
		gpm_debug ("No devices of capability %s", capability);
		return 0;
	}
	for (i = 0; names[i]; i++) {
		gchar *type = NULL;
		gpm_hal_device_get_string (hal, names[i], key, &type);
		if (type != NULL) {
			if (strcmp (type, value) == 0)
				valid++;
			g_free (type);
		}
	}
	gpm_hal_free_capability (hal, names);
	gpm_debug ("%i devices of capability %s where %s is %s",
		   valid, capability, key, value);
	return valid;
}

/**
 * watch_device_property_modified:
 *
 * @udi: The HAL UDI
 * @key: Property key
 * @is_added: If the key was added
 * @is_removed: If the key was removed
 *
 * Invoked when a property of a device in the Global Device List is
 * changed, and we have we have subscribed to changes for that device.
 */
static void
watch_device_property_modified (DBusGProxy *proxy,
				const char *udi,
				const char *key,
				gboolean    is_added,
				gboolean    is_removed,
				gboolean    finally,
				GpmHal	   *hal)
{
	gpm_debug ("emitting property-modified : udi=%s, key=%s, "
		   "added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	g_signal_emit (hal, signals [DEVICE_PROPERTY_MODIFIED], 0,
		       udi, key, is_added, is_removed, finally);
}

/**
 * watch_device_properties_modified_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @hal: This class instance
 *
 * Demultiplex the composite PropertyModified events here.
 */
static void
watch_device_properties_modified_cb (DBusGProxy *proxy,
				     gint	 type,
				     GPtrArray  *properties,
				     GpmHal     *hal)
{
	GValueArray *array;
	const char  *udi;
	const char  *key;
	gboolean     added;
	gboolean     removed;
	gboolean     finally = FALSE;
	guint	     i;

	udi = dbus_g_proxy_get_path (proxy);
	gpm_debug ("property modified '%s'", udi);

	array = NULL;

	for (i = 0; i < properties->len; i++) {
		array = g_ptr_array_index (properties, i);
		if (array->n_values != 3) {
			gpm_warning ("array->n_values invalid (!3)");
			return;
		}

		key = g_value_get_string (g_value_array_get_nth (array, 0));
		removed = g_value_get_boolean (g_value_array_get_nth (array, 1));
		added = g_value_get_boolean (g_value_array_get_nth (array, 2));

		/* Work out if this PropertyModified is the last to be sent as
		 * sometimes we only want to refresh caches when we have all
		 * the info from a UDI */
		if (i == properties->len - 1) {
			finally = TRUE;
		}

		watch_device_property_modified (proxy, udi, key,
						added, removed, finally, hal);
	}
}

/**
 * gpm_hal_device_watch_propery_modified:
 *
 * @udi: The HAL UDI
 *
 * Watch the specified device, so it emits device-property-modified
 */
gboolean
gpm_hal_device_watch_propery_modified (GpmHal     *hal,
				       const char *udi)
{
	DBusGProxy *proxy;
	GError     *error = NULL;
	GType       struct_array_type, struct_type;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	/* Check we are not already monitoring this device */
	proxy = g_hash_table_lookup (hal->priv->watch_device_property_modified, udi);
	if (proxy != NULL) {
		gpm_debug ("Device is already being watched for PropertyModified: %s", udi);
		return FALSE;
	}

	/* get a new proxy */
	proxy = dbus_g_proxy_new_for_name_owner (hal->priv->connection,
						 HAL_DBUS_SERVICE, udi,
						 HAL_DBUS_INTERFACE_DEVICE,
						 &error);
	if (proxy == NULL) {
		gpm_warning ("Could not create proxy for UDI: %s: %s", udi, error->message);
		g_error_free (error);
		return FALSE;
	}
	g_hash_table_insert (hal->priv->watch_device_property_modified, g_strdup (udi), proxy);

#if (DBUS_VERSION_MAJOR == 0) && (DBUS_VERSION_MINOR < 61)
	struct_type = G_TYPE_VALUE_ARRAY;
#else
	struct_type = dbus_g_type_get_struct ("GValueArray", 
					      G_TYPE_STRING, 
					      G_TYPE_BOOLEAN, 
					      G_TYPE_BOOLEAN, 
					      G_TYPE_INVALID);
#endif
	struct_array_type = dbus_g_type_get_collection ("GPtrArray", struct_type);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__INT_BOXED,
					   G_TYPE_NONE, G_TYPE_INT,
					   struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy, "PropertyModified",
				 G_TYPE_INT, struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "PropertyModified",
				     G_CALLBACK (watch_device_properties_modified_cb), hal, NULL);
	return TRUE;
}

/**
 * watch_device_condition_cb:
 *
 * @udi: Univerisal Device Id
 * @name: Name of condition
 * @details: D-BUS message with parameters
 *
 * Invoked when a property of a device in the Global Device List is
 * changed, and we have we have subscribed to changes for that device.
 */
static void
watch_device_condition_cb (DBusGProxy *proxy,
			   const char *condition,
			   const char *details,
			   GpmHal     *hal)
{
	const char *udi;
	udi = dbus_g_proxy_get_path (proxy);

	gpm_debug ("emitting device-condition : %s, %s (%s)", udi, condition, details);
	g_signal_emit (hal, signals [DEVICE_CONDITION], 0, udi, condition, details);
}

/**
 * gpm_hal_device_watch_condition:
 * @udi: The HAL UDI
 *
 * Watch the specified device, so it emits a device-condition
 */
gboolean
gpm_hal_device_watch_condition (GpmHal     *hal,
				const char *udi)
{
	DBusGProxy *proxy;
	GError     *error = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	/* Check we are not already monitoring this device */
	proxy = g_hash_table_lookup (hal->priv->watch_device_condition, udi);
	if (proxy != NULL) {
		gpm_debug ("Device is already being watched for NewCondition: %s", udi);
		return FALSE;
	}

	/* get a new proxy */
	proxy = dbus_g_proxy_new_for_name_owner (hal->priv->connection,
						 HAL_DBUS_SERVICE, udi,
						 HAL_DBUS_INTERFACE_DEVICE,
						 &error);
	if (proxy == NULL) {
		gpm_warning ("Could not create proxy for UDI: %s: %s", udi, error->message);
		g_error_free (error);
		return FALSE;
	}
	g_hash_table_insert (hal->priv->watch_device_condition, g_strdup (udi), proxy);


	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING,
					   G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy, "Condition",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "Condition",
				     G_CALLBACK (watch_device_condition_cb), hal, NULL);
	return TRUE;
}

/**
 * gpm_hal_device_remove_condition:
 *
 * @udi: The HAL UDI
 *
 * Remove the specified device, so it does not emit device-condition signals.
 */
gboolean
gpm_hal_device_remove_condition (GpmHal     *hal,
				 const char *udi)
{
	gpointer key, value;
	gboolean present;
	char *udi_key;
	DBusGProxy *proxy = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	present = g_hash_table_lookup_extended (hal->priv->watch_device_condition, udi, &key, &value);
	if (present == FALSE) {
		gpm_debug ("Device is not being watched for DeviceCondition: %s", udi);
		return FALSE;
	}

	udi_key = key;
	proxy = value;

	dbus_g_proxy_disconnect_signal (proxy, "Condition",
					G_CALLBACK (watch_device_condition_cb), hal);

	g_hash_table_remove (hal->priv->watch_device_condition, udi);

	g_object_unref (proxy);
	g_free (udi_key);
	return TRUE;
}

/**
 * gpm_hal_device_remove_propery_modified:
 *
 * @udi: The HAL UDI
 *
 * Remove the specified device, so it does not emit device-propery-modified.
 */
gboolean
gpm_hal_device_remove_propery_modified (GpmHal     *hal,
				        const char *udi)
{
	gpointer key, value;
	gboolean present;
	char *udi_key;
	DBusGProxy *proxy = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	present = g_hash_table_lookup_extended (hal->priv->watch_device_property_modified, udi, &key, &value);
	if (present == FALSE) {
		gpm_debug ("Device is not being watched for PropertyModified: %s", udi);
		return FALSE;
	}

	udi_key = key;
	proxy = value;

	dbus_g_proxy_disconnect_signal (proxy, "PropertyModified",
				        G_CALLBACK (watch_device_properties_modified_cb), hal);

	g_hash_table_remove (hal->priv->watch_device_property_modified, udi);

	g_object_unref (proxy);
	g_free (udi_key);
	return TRUE;
}

/**
 * gpm_hal_class_init:
 * @klass: This hal class instance
 **/
static void
gpm_hal_class_init (GpmHalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_hal_finalize;
	g_type_class_add_private (klass, sizeof (GpmHalPrivate));

	signals [DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, device_added),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signals [DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, device_removed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	signals [DEVICE_PROPERTY_MODIFIED] =
		g_signal_new ("property-modified",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, device_property_modified),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 5, G_TYPE_STRING, G_TYPE_STRING,
			      G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

	signals [DEVICE_CONDITION] =
		g_signal_new ("device-condition",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, device_condition),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	signals [NEW_CAPABILITY] =
		g_signal_new ("new-capability",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, new_capability),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);

	signals [LOST_CAPABILITY] =
		g_signal_new ("lost-capability",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, lost_capability),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);

	signals [DAEMON_START] =
		g_signal_new ("daemon-start",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, daemon_start),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	signals [DAEMON_STOP] =
		g_signal_new ("daemon-stop",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmHalClass, daemon_stop),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/**
 * gpm_hal_device_added_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @hal: This class instance
 *
 * Invoked when a device is added.
 */
static void
gpm_hal_device_added_cb (DBusGProxy *proxy,
		           const char *udi,
		           GpmHal     *hal)
{
	gpm_debug ("emitting device-added : %s", udi);
	g_signal_emit (hal, signals [DEVICE_ADDED], 0, udi);
}

/**
 * gpm_hal_device_removed_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @hal: This class instance
 *
 * Invoked when a device is removed.
 */
static void
gpm_hal_device_removed_cb (DBusGProxy *proxy,
		           const char *udi,
		           GpmHal     *hal)
{
	gpm_debug ("emitting device-removed : %s", udi);
	g_signal_emit (hal, signals [DEVICE_REMOVED], 0, udi);
}

/**
 * gpm_hal_new_capability_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @udi: Univerisal Device Id
 * @capability: The new capability, e.g. "battery"
 * @hal: This class instance
 *
 * Invoked when a device gets a new condition.
 */
static void
gpm_hal_new_capability_cb (DBusGProxy *proxy,
		           const char *udi,
		           const char *capability,
		           GpmHal     *hal)
{
	g_debug ("emitting new-capability : %s, %s", udi, capability);
	g_signal_emit (hal, signals [NEW_CAPABILITY], 0, udi, capability);
}

/**
 * gpm_hal_connect:
 *
 * @hal: This class instance
 * Return value: Success
 *
 * Connect the manager proxy to HAL and register some basic callbacks
 */
static gboolean
gpm_hal_connect (GpmHal *hal)
{
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	/* sometimes we get repeat notifications from DBUS */
	if (hal->priv->is_connected) {
		g_debug ("Trying to connect already connected hal");
		return FALSE;
	}

	hal->priv->manager_proxy = dbus_g_proxy_new_for_name_owner (hal->priv->connection,
								    HAL_DBUS_SERVICE,
								    HAL_DBUS_PATH_MANAGER,
								    HAL_DBUS_INTERFACE_MANAGER,
								    &error);
	/* if any error is set, then print */
	if (error) {
		g_warning ("cannot connect to HAL: %s", error->message);
		g_error_free (error);
	}

	/* we failed to connect */
	if (hal->priv->manager_proxy == NULL) {
		hal->priv->is_connected = FALSE;
		return FALSE;
	}

	/* connect the org.freedesktop.Hal.Manager signals */
	dbus_g_proxy_add_signal (hal->priv->manager_proxy, "DeviceAdded",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (hal->priv->manager_proxy, "DeviceAdded",
				     G_CALLBACK (gpm_hal_device_added_cb), hal, NULL);

	dbus_g_proxy_add_signal (hal->priv->manager_proxy, "DeviceRemoved",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (hal->priv->manager_proxy, "DeviceRemoved",
				     G_CALLBACK (gpm_hal_device_removed_cb), hal, NULL);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (hal->priv->manager_proxy, "NewCapability",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (hal->priv->manager_proxy, "NewCapability",
				     G_CALLBACK (gpm_hal_new_capability_cb), hal, NULL);

	hal->priv->is_connected = TRUE;
	return TRUE;
}

/**
 * gpm_hal_disconnect:
 *
 * @hal: This class instance
 * Return value: Success
 *
 * Disconnect the manager proxy to HAL and disconnect some basic callbacks
 */
static gboolean
gpm_hal_disconnect (GpmHal *hal)
{
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	if (hal->priv->is_connected == FALSE) {
		g_debug ("Trying to disconnect already disconnected hal");
		return FALSE;
	}

	if (hal->priv->manager_proxy == NULL) {
		g_debug ("The proxy is null, but connected. Odd.");
		return FALSE;
	}

	dbus_g_proxy_disconnect_signal (hal->priv->manager_proxy, "DeviceRemoved",
					G_CALLBACK (gpm_hal_device_removed_cb), hal);
	dbus_g_proxy_disconnect_signal (hal->priv->manager_proxy, "NewCapability",
					G_CALLBACK (gpm_hal_new_capability_cb), hal);

	g_object_unref (hal->priv->manager_proxy);
	hal->priv->manager_proxy = NULL;

	hal->priv->is_connected = FALSE;
	return TRUE;
}

/**
 * dbus_name_owner_changed_system_cb:
 *
 * @name: The DBUS name, e.g. hal.freedesktop.org
 * @prev: The previous name, e.g. :0.13
 * @new: The new name, e.g. :0.14
 *
 * The name-owner-changed system DBUS callback.
 **/
static void
dbus_name_owner_changed_system_cb (GpmDbusSystemMonitor *dbus_monitor,
				   const char	  *name,
				   const char     *prev,
				   const char     *new,
				   GpmHal         *hal)
{
	g_return_if_fail (GPM_IS_HAL (hal));
	g_return_if_fail (name != NULL);
	g_return_if_fail (prev != NULL);
	g_return_if_fail (new != NULL);

	if (strcmp (name, HAL_DBUS_SERVICE) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0 ) {
			gpm_hal_disconnect (hal);
			g_debug ("emitting daemon-stop");
			g_signal_emit (hal, signals [DAEMON_STOP], 0);
		}
		if (strlen (prev) == 0 && strlen (new) != 0 ) {
			gpm_hal_connect (hal);
			g_debug ("emitting daemon-start");
			g_signal_emit (hal, signals [DAEMON_START], 0);
		}
	}
}

/**
 * gpm_hal_init:
 *
 * @hal: This hal class instance
 **/
static void
gpm_hal_init (GpmHal *hal)
{
	GError *error = NULL;
	hal->priv = GPM_HAL_GET_PRIVATE (hal);

	hal->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (hal->priv->connection == NULL) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_critical_error ("Cannot connect to DBUS System Daemon");
	}

	/* monitor the system bus so that we can detect when HAL is restarted */
	hal->priv->dbus_system = gpm_dbus_system_monitor_new ();
	g_signal_connect (hal->priv->dbus_system, "name-owner-changed",
			  G_CALLBACK (dbus_name_owner_changed_system_cb), hal);
	/* FIXME: Reconnect the indervidual devices on HAL restart */

	hal->priv->watch_device_property_modified = g_hash_table_new (g_str_hash, g_str_equal);
	hal->priv->watch_device_condition = g_hash_table_new (g_str_hash, g_str_equal);

	/* blindly try to connect, assuming HAL is alive */
	gpm_hal_connect (hal);
}

/**
 * remove_device_property_modified_in_hash:
 *
 * @udi: The HAL UDI
 *
 * HashFunc so we can remove all the device-propery-modified devices
 */
static void
remove_device_property_modified_in_hash (const char *udi, gpointer value, GpmHal *hal)
{
	gpm_hal_device_remove_propery_modified (hal, udi);
}

/**
 * remove_device_condition_in_hash:
 *
 * @udi: The HAL UDI
 *
 * HashFunc so we can remove all the device-condition devices
 */
static void
remove_device_condition_in_hash (const char *udi, gpointer value, GpmHal *hal)
{
	gpm_hal_device_remove_condition (hal, udi);
}

/**
 * gpm_hal_finalize:
 * @object: This hal class instance
 **/
static void
gpm_hal_finalize (GObject *object)
{
	GpmHal *hal;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL (object));

	hal = GPM_HAL (object);
	hal->priv = GPM_HAL_GET_PRIVATE (hal);

	if (hal->priv->is_connected) {
		gpm_hal_disconnect (hal);
	}

	g_hash_table_foreach (hal->priv->watch_device_property_modified,
			      (GHFunc) remove_device_property_modified_in_hash, hal);
	g_hash_table_foreach (hal->priv->watch_device_condition,
			      (GHFunc) remove_device_condition_in_hash, hal);
	g_hash_table_destroy (hal->priv->watch_device_property_modified);
	g_hash_table_destroy (hal->priv->watch_device_condition);

	g_object_unref (hal->priv->dbus_system);

	G_OBJECT_CLASS (gpm_hal_parent_class)->finalize (object);
}

/**
 * gpm_hal_new:
 * Return value: new GpmHal instance.
 **/
GpmHal *
gpm_hal_new (void)
{
	if (gpm_hal_object) {
		g_object_ref (gpm_hal_object);
	} else {
		gpm_hal_object = g_object_new (GPM_TYPE_HAL, NULL);
		g_object_add_weak_pointer (gpm_hal_object,
					   (gpointer *) &gpm_hal_object);
	}
	return GPM_HAL (gpm_hal_object);
}
