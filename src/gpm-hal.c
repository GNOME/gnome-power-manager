/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-marshal.h"
#include "gpm-hal.h"
#include "gpm-debug.h"
#include "gpm-proxy.h"

static void     gpm_hal_class_init (GpmHalClass *klass);
static void     gpm_hal_init       (GpmHal      *hal);
static void     gpm_hal_finalize   (GObject	*object);

#define GPM_HAL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_HAL, GpmHalPrivate))

struct GpmHalPrivate
{
	DBusGConnection		*connection;
	GHashTable		*watch_device_property_modified;
	GHashTable		*watch_device_condition;
	GpmProxy		*gproxy_manager;
	GpmProxy		*gproxy_power;
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

G_DEFINE_TYPE (GpmHal, gpm_hal, G_TYPE_OBJECT)

/**
 * gpm_hal_is_running:
 *
 * @hal: This class instance
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

	running = gpm_hal_device_get_string (hal, HAL_ROOT_COMPUTER, "info.udi", &udi, NULL);
	g_free (udi);
	return running;
}

/**
 * gpm_hal_device_get_bool:
 *
 * @hal: This class instance
 * @udi: The UDI of the device
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_hal_device_get_bool (GpmHal      *hal,
			 const gchar *udi,
			 const gchar *key,
			 gboolean    *value,
			 GError     **error)
{
	DBusGProxy *proxy;
	gboolean ret;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	ret = dbus_g_proxy_call (proxy, "GetPropertyBoolean", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, value,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
		*value = FALSE;
	}
	g_object_unref (G_OBJECT (proxy));
	return ret;
}

/**
 * gpm_hal_device_get_string:
 *
 * @hal: This class instance
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
			   gchar      **value,
			   GError     **error)
{
	DBusGProxy *proxy = NULL;
	gboolean ret;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	ret = dbus_g_proxy_call (proxy, "GetPropertyString", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_STRING, value,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
		*value = NULL;
	}
	g_object_unref (G_OBJECT (proxy));
	return ret;
}

/**
 * gpm_hal_device_get_int:
 *
 * @hal: This class instance
 * @udi: The UDI of the device
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_hal_device_get_int (GpmHal      *hal,
			const gchar *udi,
			const gchar *key,
			gint        *value,
			GError     **error)
{
	DBusGProxy *proxy;
	gboolean ret;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       udi,
					       HAL_DBUS_INTERFACE_DEVICE);
	ret = dbus_g_proxy_call (proxy, "GetPropertyInteger", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_INT, value,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
		*value = 0;
	}
	g_object_unref (G_OBJECT (proxy));
	return ret;
}

/**
 * gpm_hal_device_get_uint:
 *
 * HAL has no concept of a UINT, only INT
 **/
gboolean
gpm_hal_device_get_uint (GpmHal      *hal,
			 const gchar *udi,
			 const gchar *key,
			 guint       *value,
			 GError     **error)
{
	gint tvalue;
	gboolean ret;

	/* bodge */
	ret = gpm_hal_device_get_int (hal, udi, key, &tvalue, error);
	*value = (guint) tvalue;
	return ret;
}

/**
 * gpm_hal_device_find_capability:
 *
 * @hal: This class instance
 * @capability: The capability, e.g. "battery"
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_hal_device_find_capability (GpmHal      *hal,
				const gchar *capability,
				gchar     ***value,
				GError     **error)
{
	DBusGProxy *proxy = NULL;
	gboolean ret;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (capability != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					       HAL_DBUS_SERVICE,
					       HAL_DBUS_PATH_MANAGER,
					       HAL_DBUS_INTERFACE_MANAGER);
	ret = dbus_g_proxy_call (proxy, "FindDeviceByCapability", error,
				 G_TYPE_STRING, capability,
				 G_TYPE_INVALID,
				 G_TYPE_STRV, value,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
		*value = NULL;
	}
	g_object_unref (G_OBJECT (proxy));
	return ret;
}

/**
 * gpm_hal_device_has_capability:
 *
 * @hal: This class instance
 * @capability: The capability, e.g. "battery"
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
gpm_hal_device_has_capability (GpmHal      *hal,
			       const gchar *udi,
			       const gchar *capability,
			       gboolean    *has_capability,
			       GError     **error)
{
	DBusGProxy *proxy = NULL;
	gboolean ret;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);
	g_return_val_if_fail (capability != NULL, FALSE);
	g_return_val_if_fail (has_capability != NULL, FALSE);

	proxy = dbus_g_proxy_new_for_name (hal->priv->connection,
					   HAL_DBUS_SERVICE,
					   udi,
					   HAL_DBUS_INTERFACE_DEVICE);
	ret = dbus_g_proxy_call (proxy, "QueryCapability", error,
				 G_TYPE_STRING, udi,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, has_capability,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
		*has_capability = FALSE;
	}
	g_object_unref (G_OBJECT (proxy));
	return ret;
}

/**
 * gpm_hal_free_capability:
 *
 * @hal: This class instance
 * @value: The list of strings to free
 *
 * Frees value result of gpm_hal_device_find_capability. Safe to call with NULL.
 **/
void
gpm_hal_free_capability (GpmHal *hal, gchar **value)
{
	gint i;

	g_return_if_fail (GPM_IS_HAL (hal));

	if (value == NULL) {
		return;
	}
	for (i=0; value[i]; i++) {
		g_free (value[i]);
	}
	g_free (value);
}

/**
 * gpm_hal_num_devices_of_capability:
 *
 * @hal: This class instance
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
	gboolean ret;

	g_return_val_if_fail (GPM_IS_HAL (hal), 0);
	g_return_val_if_fail (capability != NULL, 0);

	ret = gpm_hal_device_find_capability (hal, capability, &names, NULL);
	if (ret == FALSE) {
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
 * @hal: This class instance
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
	gboolean ret;

	g_return_val_if_fail (GPM_IS_HAL (hal), 0);
	g_return_val_if_fail (capability != NULL, 0);
	g_return_val_if_fail (key != NULL, 0);
	g_return_val_if_fail (value != NULL, 0);

	ret = gpm_hal_device_find_capability (hal, capability, &names, NULL);
	if (ret == FALSE) {
		gpm_debug ("No devices of capability %s", capability);
		return 0;
	}
	for (i = 0; names[i]; i++) {
		gchar *type = NULL;
		gpm_hal_device_get_string (hal, names[i], key, &type, NULL);
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
				const gchar *udi,
				const gchar *key,
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
	const gchar *udi;
	const gchar *key;
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
 * Watch the specified device, so it emits device-property-modified and
 * adds to the gpm cache so we don't get asked to add it again.
 */
gboolean
gpm_hal_device_watch_propery_modified (GpmHal      *hal,
				       const gchar *udi,
				       gboolean     force)
{
	DBusGProxy *proxy;
	GError     *error = NULL;
	GType       struct_array_type, struct_type;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	/* Check we are not already monitoring this device */
	proxy = g_hash_table_lookup (hal->priv->watch_device_property_modified, udi);
	if (force == FALSE && proxy != NULL) {
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

	struct_type = dbus_g_type_get_struct ("GValueArray", 
					      G_TYPE_STRING, 
					      G_TYPE_BOOLEAN, 
					      G_TYPE_BOOLEAN, 
					      G_TYPE_INVALID);

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
watch_device_condition_cb (DBusGProxy  *proxy,
			   const gchar *condition,
			   const gchar *details,
			   GpmHal      *hal)
{
	const gchar *udi;
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
gpm_hal_device_watch_condition (GpmHal      *hal,
				const gchar *udi,
				gboolean     force)
{
	DBusGProxy *proxy;
	GError     *error = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	/* Check we are not already monitoring this device */
	proxy = g_hash_table_lookup (hal->priv->watch_device_condition, udi);
	if (force == FALSE && proxy != NULL) {
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
gpm_hal_device_remove_condition (GpmHal      *hal,
				 const gchar *udi)
{
	gpointer key, value;
	gboolean present;
	gchar *udi_key;
	DBusGProxy *proxy;

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
gpm_hal_device_remove_propery_modified (GpmHal      *hal,
				        const gchar *udi)
{
	gpointer key, value;
	gboolean present;
	gchar *udi_key;
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
 * @klass: This class instance
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
gpm_hal_device_added_cb (DBusGProxy  *proxy,
		         const gchar *udi,
		         GpmHal      *hal)
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
gpm_hal_device_removed_cb (DBusGProxy  *proxy,
		           const gchar *udi,
		           GpmHal      *hal)
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
gpm_hal_new_capability_cb (DBusGProxy  *proxy,
		           const gchar *udi,
		           const gchar *capability,
		           GpmHal      *hal)
{
	gpm_debug ("emitting new-capability : %s, %s", udi, capability);
	g_signal_emit (hal, signals [NEW_CAPABILITY], 0, udi, capability);
}

/**
 * reattach_property_modified_in_hash:
 *
 * @udi: The HAL UDI
 *
 * HashFunc so we can remove all the device-propery-modified devices
 */
static void
reattach_property_modified_in_hash (const gchar *udi,
				    gpointer     value,
				    GpmHal      *hal)
{
	/* force the new proxy as DBUS has invalidaded the old ones */
	gpm_debug ("reattach property-modified %s", udi);
	gpm_hal_device_watch_propery_modified (hal, udi, TRUE);
}

/**
 * reattach_condition_in_hash:
 *
 * @udi: The HAL UDI
 *
 * HashFunc so we can remove all the device-propery-modified devices
 */
static void
reattach_condition_in_hash (const gchar *udi,
				    gpointer     value,
				    GpmHal      *hal)
{
	/* force the new proxy as DBUS has invalidaded the old ones */
	gpm_debug ("reattach condition %s", udi);
	gpm_hal_device_watch_condition (hal, udi, TRUE);
}

/**
 * gpm_hal_proxy_connect_more:
 *
 * @hal: This class instance
 * Return value: Success
 *
 * Connect the manager proxy to HAL and register some basic callbacks
 */
static gboolean
gpm_hal_proxy_connect_more (GpmHal *hal)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	proxy = gpm_proxy_get_proxy (hal->priv->gproxy_manager);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	/* connect the org.freedesktop.Hal.Manager signals */
	dbus_g_proxy_add_signal (proxy, "DeviceAdded",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "DeviceAdded",
				     G_CALLBACK (gpm_hal_device_added_cb), hal, NULL);

	dbus_g_proxy_add_signal (proxy, "DeviceRemoved",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "DeviceRemoved",
				     G_CALLBACK (gpm_hal_device_removed_cb), hal, NULL);

	dbus_g_object_register_marshaller (gpm_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (proxy, "NewCapability",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "NewCapability",
				     G_CALLBACK (gpm_hal_new_capability_cb), hal, NULL);

	/* we need to re-register anything in:
	   - hal->priv->watch_device_condition
	   - hal->priv->watch_property_modified
	 */
	g_hash_table_foreach (hal->priv->watch_device_property_modified,
			      (GHFunc) reattach_property_modified_in_hash, hal);
	g_hash_table_foreach (hal->priv->watch_device_condition,
			      (GHFunc) reattach_condition_in_hash, hal);

	return TRUE;
}

/**
 * gpm_hal_proxy_disconnect_more:
 *
 * @hal: This class instance
 * Return value: Success
 *
 * Disconnect the manager proxy to HAL and disconnect some basic callbacks
 */
static gboolean
gpm_hal_proxy_disconnect_more (GpmHal *hal)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	proxy = gpm_proxy_get_proxy (hal->priv->gproxy_manager);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	dbus_g_proxy_disconnect_signal (proxy, "DeviceRemoved",
					G_CALLBACK (gpm_hal_device_removed_cb), hal);
	dbus_g_proxy_disconnect_signal (proxy, "NewCapability",
					G_CALLBACK (gpm_hal_new_capability_cb), hal);

	return TRUE;
}

/**
 * proxy_status_cb:
 * @proxy: The dbus raw proxy
 * @status: The status of the service, where TRUE is connected
 * @hal: This class instance
 **/
static void
proxy_status_cb (DBusGProxy *proxy,
		 gboolean    status,
		 GpmHal     *hal)
{
	g_return_if_fail (GPM_IS_HAL (hal));
	if (status == TRUE) {
		g_signal_emit (hal, signals [DAEMON_START], 0);
		gpm_hal_proxy_connect_more (hal);
	} else {
		g_signal_emit (hal, signals [DAEMON_STOP], 0);
		gpm_hal_proxy_disconnect_more (hal);
	}
}

/**
 * gpm_hal_init:
 *
 * @hal: This class instance
 **/
static void
gpm_hal_init (GpmHal *hal)
{
	GError *error = NULL;
	DBusGProxy *proxy;

	hal->priv = GPM_HAL_GET_PRIVATE (hal);

	hal->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
	}

	/* get the manager connection */
	hal->priv->gproxy_manager = gpm_proxy_new ();
	proxy = gpm_proxy_assign (hal->priv->gproxy_manager,
				  GPM_PROXY_SYSTEM,
				  HAL_DBUS_SERVICE,
				  HAL_DBUS_PATH_MANAGER,
				  HAL_DBUS_INTERFACE_MANAGER);
	if (proxy == NULL) {
		gpm_critical_error ("Either HAL or DBUS are not working!");
	}

	/* get the power connection */
	hal->priv->gproxy_power = gpm_proxy_new ();
	proxy = gpm_proxy_assign (hal->priv->gproxy_power,
				  GPM_PROXY_SYSTEM,
				  HAL_DBUS_SERVICE,
				  HAL_ROOT_COMPUTER,
				  HAL_DBUS_INTERFACE_POWER);
	if (proxy == NULL) {
		gpm_critical_error ("HAL does not support power management!");
	}

	g_signal_connect (hal->priv->gproxy_manager, "proxy-status",
			  G_CALLBACK (proxy_status_cb), hal);

	hal->priv->watch_device_property_modified = g_hash_table_new (g_str_hash, g_str_equal);
	hal->priv->watch_device_condition = g_hash_table_new (g_str_hash, g_str_equal);

	/* blindly try to connect, assuming HAL is alive */
	gpm_hal_proxy_connect_more (hal);
}

/**
 * remove_device_property_modified_in_hash:
 *
 * @udi: The HAL UDI
 *
 * HashFunc so we can remove all the device-propery-modified devices
 */
static void
remove_device_property_modified_in_hash (const gchar *udi,
					 gpointer     value,
					 GpmHal      *hal)
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
remove_device_condition_in_hash (const gchar *udi,
				 gpointer     value,
				 GpmHal      *hal)
{
	gpm_hal_device_remove_condition (hal, udi);
}

/**
 * gpm_hal_is_laptop:
 *
 * @hal: This class instance
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
	gpm_hal_device_get_string (hal, HAL_ROOT_COMPUTER, "system.formfactor", &formfactor, NULL);
	if (formfactor == NULL) {
		gpm_debug ("system.formfactor not set!");
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
 * @hal: This class instance
 * Return value: TRUE if haldaemon has power management capability
 *
 * Finds out if power management functions are running (only ACPI, PMU, APM)
 **/
gboolean
gpm_hal_has_power_management (GpmHal *hal)
{
	gchar *ptype = NULL;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	gpm_hal_device_get_string (hal, HAL_ROOT_COMPUTER, "power_management.type", &ptype, NULL);
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
 * @hal: This class instance
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
					  &can_suspend, NULL);
	if (exists == FALSE) {
		gpm_warning ("gpm_hal_can_suspend: Key can_suspend_to_ram missing");
		return FALSE;
	}
	return can_suspend;
}

/**
 * gpm_hal_can_hibernate:
 *
 * @hal: This class instance
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
					  &can_hibernate, NULL);
	if (exists == FALSE) {
		gpm_warning ("gpm_hal_can_hibernate: Key can_suspend_to_disk missing");
		return FALSE;
	}
	return can_hibernate;
}

/* we have to ignore dbus timeouts */
static gboolean
gpm_hal_filter_error (GError **error)
{
	/* short cut for speed, no error */
	if (*error == NULL) {
		return FALSE;
	}

	/* DBUS might time out, which is okay. We can remove this code
	   when the dbus glib bindings are fixed. See #332888 */
	if (g_error_matches (*error, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
		gpm_syslog ("DBUS timed out, but recovering");
		g_error_free (*error);
		*error = NULL;
		return TRUE;
	}
	gpm_warning ("Method failed\n(%s)",
		     (*error)->message);
	gpm_syslog ("%s code='%i' quark='%s'", (*error)->message,
		    (*error)->code, g_quark_to_string ((*error)->domain));
	return FALSE;
}

/**
 * gpm_hal_suspend:
 *
 * @hal: This class instance
 * @wakeup: Seconds to wakeup, currently unsupported
 * Return value: Success, true if we suspended OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Suspend ()
 **/
gboolean
gpm_hal_suspend (GpmHal *hal, guint wakeup)
{
	guint retval = 0;
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	gpm_debug ("Try to suspend...");

	proxy = gpm_proxy_get_proxy (hal->priv->gproxy_power);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "Suspend", &error,
				 G_TYPE_INT, wakeup,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &retval,
				 G_TYPE_INVALID);
	/* we might have to ignore the error */
	if (gpm_hal_filter_error (&error)) {
		return TRUE;
	}
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE || retval != 0) {
		/* abort as the DBUS method failed */
		gpm_warning ("Suspend failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * hal_pm_method_void:
 *
 * @hal: This class instance
 * @method: The method name, e.g. "Hibernate"
 * Return value: Success, true if we did OK
 *
 * Do a method on org.freedesktop.Hal.Device.SystemPowerManagement.*
 * with no arguments.
 **/
static gboolean
hal_pm_method_void (GpmHal *hal, const gchar* method)
{
	guint retval = 0;
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	g_return_val_if_fail (method != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (hal->priv->gproxy_power);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, method, &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &retval,
				 G_TYPE_INVALID);
	/* we might have to ignore the error */
	if (gpm_hal_filter_error (&error)) {
		return TRUE;
	}
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE || retval != 0) {
		/* abort as the DBUS method failed */
		gpm_warning ("%s failed!", method);
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_hal_hibernate:
 *
 * @hal: This class instance
 * Return value: Success, true if we hibernated OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Hibernate ()
 **/
gboolean
gpm_hal_hibernate (GpmHal *hal)
{
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	gpm_debug ("Try to hibernate...");
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
	gpm_debug ("Try to shutdown...");
	return hal_pm_method_void (hal, "Shutdown");
}

/**
 * gpm_hal_reboot:
 *
 * @hal: This class instance
 * Return value: Success, true if we shutdown OK
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.Reboot ()
 **/
gboolean
gpm_hal_reboot (GpmHal *hal)
{
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	gpm_debug ("Try to reboot...");
	return hal_pm_method_void (hal, "Reboot");
}

/**
 * gpm_hal_enable_power_save:
 *
 * @hal: This class instance
 * @enable: True to enable low power mode
 * Return value: Success, true if we set the mode
 *
 * Uses org.freedesktop.Hal.Device.SystemPowerManagement.SetPowerSave ()
 **/
gboolean
gpm_hal_enable_power_save (GpmHal *hal, gboolean enable)
{
	gint retval = 0;
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (hal != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	proxy = gpm_proxy_get_proxy (hal->priv->gproxy_power);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	/* abort if we are not a "qualified" laptop */
	if (gpm_hal_is_laptop (hal) == FALSE) {
		gpm_debug ("We are not a laptop, so not even trying");
		return FALSE;
	}

	gpm_debug ("Doing SetPowerSave (%i)", enable);
	ret = dbus_g_proxy_call (proxy, "SetPowerSave", &error,
				 G_TYPE_BOOLEAN, enable,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &retval,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE || retval != 0) {
		/* abort as the DBUS method failed */
		gpm_warning ("SetPowerSave failed!");
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_hal_has_suspend_error:
 *
 * @hal: This class instance
 * @enable: Return true if there was a suspend error
 * Return value: Success
 *
 * TODO: should call a method on HAL and also return the ouput of the file
 **/
gboolean
gpm_hal_has_suspend_error (GpmHal *hal, gboolean *state)
{
	g_return_val_if_fail (hal != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	*state = g_file_test ("/var/lib/hal/system-power-suspend-output", G_FILE_TEST_EXISTS);
	return TRUE;
}

/**
 * gpm_hal_has_hibernate_error:
 *
 * @hal: This class instance
 * @enable: Return true if there was a hibernate error
 * Return value: Success
 *
 * TODO: should call a method on HAL and also return the ouput of the file
 **/
gboolean
gpm_hal_has_hibernate_error (GpmHal *hal, gboolean *state)
{
	g_return_val_if_fail (hal != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);
	*state = g_file_test ("/var/lib/hal/system-power-hibernate-output", G_FILE_TEST_EXISTS);
	return TRUE;
}

/**
 * gpm_hal_clear_suspend_error:
 *
 * @hal: This class instance
 * Return value: Success
 *
 * Tells HAL to try and clear the suspend error as we appear to be okay
 **/
gboolean
gpm_hal_clear_suspend_error (GpmHal *hal, GError **error)
{
#if HAVE_HAL_NEW
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (hal != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	proxy = gpm_proxy_get_proxy (hal->priv->gproxy_power);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	gpm_debug ("Doing SuspendClearError");
	ret = dbus_g_proxy_call (proxy, "SuspendClearError", error,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	return ret;
#endif
	return TRUE;
}

/**
 * gpm_hal_clear_hibernate_error:
 *
 * @hal: This class instance
 * Return value: Success
 *
 * Tells HAL to try and clear the hibernate error as we appear to be okay
 **/
gboolean
gpm_hal_clear_hibernate_error (GpmHal *hal, GError **error)
{
#if HAVE_HAL_NEW
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (hal != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_HAL (hal), FALSE);

	proxy = gpm_proxy_get_proxy (hal->priv->gproxy_power);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	gpm_debug ("Doing HibernateClearError");
	ret = dbus_g_proxy_call (proxy, "HibernateClearError", error,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	return ret;
#endif
	return TRUE;
}

/**
 * gpm_hal_finalize:
 * @object: This class instance
 **/
static void
gpm_hal_finalize (GObject *object)
{
	GpmHal *hal;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_HAL (object));

	hal = GPM_HAL (object);
	hal->priv = GPM_HAL_GET_PRIVATE (hal);

	g_hash_table_foreach (hal->priv->watch_device_property_modified,
			      (GHFunc) remove_device_property_modified_in_hash, hal);
	g_hash_table_foreach (hal->priv->watch_device_condition,
			      (GHFunc) remove_device_condition_in_hash, hal);
	g_hash_table_destroy (hal->priv->watch_device_property_modified);
	g_hash_table_destroy (hal->priv->watch_device_condition);

	g_object_unref (hal->priv->gproxy_manager);
	g_object_unref (hal->priv->gproxy_power);

	G_OBJECT_CLASS (gpm_hal_parent_class)->finalize (object);
}

/**
 * gpm_hal_new:
 * Return value: new GpmHal instance.
 **/
GpmHal *
gpm_hal_new (void)
{
	static GpmHal *hal = NULL;
	if (hal != NULL) {
		g_object_ref (hal);
	} else {
		hal = g_object_new (GPM_TYPE_HAL, NULL);
	}
	return GPM_HAL (hal);
}
