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
#include <libdbus-proxy.h>

#include "libhal-marshal.h"
#include "libhal-gpower.h"
#include "libhal-gdevice.h"
#include "libhal-gmanager.h"

static void     hal_gdevice_class_init (HalGDeviceClass *klass);
static void     hal_gdevice_init       (HalGDevice      *device);
static void     hal_gdevice_finalize   (GObject	     *object);

#define LIBHAL_GDEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), LIBHAL_TYPE_GDEVICE, HalGDevicePrivate))

struct HalGDevicePrivate
{
	DBusGConnection		*connection;
	gboolean		 use_property_modified;
	gboolean		 use_condition;
	DbusProxy		*gproxy;
	gchar			*udi;
};

/* Signals emitted from HalGDevice are:
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
	DEVICE_PROPERTY_MODIFIED,
	DEVICE_CONDITION,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (HalGDevice, hal_gdevice, G_TYPE_OBJECT)

/**
 * hal_gdevice_set_udi:
 *
const gchar  * * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_gdevice_set_udi (HalGDevice  *device,
		     const gchar *udi)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	if (device->priv->udi != NULL) {
		/* aready set UDI */
		return FALSE;
	}

	proxy = dbus_proxy_assign (device->priv->gproxy,
				  DBUS_PROXY_SYSTEM,
				  HAL_DBUS_SERVICE,
				  udi,
				  HAL_DBUS_INTERFACE_DEVICE);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy failed");
		return FALSE;
	}
	device->priv->udi = g_strdup (udi);

	return TRUE;
}

/**
 * hal_gdevice_get_udi:
 *
 * Return value: UDI
 **/
const gchar *
hal_gdevice_get_udi (HalGDevice *device)
{
	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), NULL);

	return device->priv->udi;
}

/**
 * hal_gdevice_get_bool:
 *
 * @hal_gdevice: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_gdevice_get_bool (HalGDevice  *device,
		      const gchar *key,
		      gboolean    *value,
		      GError     **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "GetPropertyBoolean", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, value,
				 G_TYPE_INVALID);
	if (!ret) {
		*value = FALSE;
	}
	return ret;
}

/**
 * hal_gdevice_get_string:
 *
 * @hal_gdevice: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 *
 * You must g_free () the return value.
 **/
gboolean
hal_gdevice_get_string (HalGDevice   *device,
			const gchar  *key,
			gchar       **value,
			GError      **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "GetPropertyString", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_STRING, value,
				 G_TYPE_INVALID);
	if (!ret) {
		*value = NULL;
	}
	return ret;
}

/**
 * hal_gdevice_get_int:
 *
 * @hal_gdevice: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_gdevice_get_int (HalGDevice   *device,
		     const gchar  *key,
		     gint         *value,
		     GError      **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "GetPropertyInteger", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_INT, value,
				 G_TYPE_INVALID);
	if (!ret) {
		*value = 0;
	}
	return ret;
}

/**
 * hal_gdevice_get_uint:
 *
 * HAL has no concept of a UINT, only INT
 **/
gboolean
hal_gdevice_get_uint (HalGDevice   *device,
		      const gchar  *key,
		      guint        *value,
		      GError      **error)
{
	gint tvalue;
	gboolean ret;

	/* bodge */
	ret = hal_gdevice_get_int (device, key, &tvalue, error);
	*value = (guint) tvalue;
	return ret;
}

/**
 * hal_gdevice_query_capability:
 *
 * @hal_gdevice: This class instance
 * @capability: The capability, e.g. "battery"
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_gdevice_query_capability (HalGDevice  *device,
			      const gchar *capability,
			      gboolean    *has_capability,
			      GError     **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);
	g_return_val_if_fail (capability != NULL, FALSE);
	g_return_val_if_fail (has_capability != NULL, FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "QueryCapability", error,
				 G_TYPE_STRING, device->priv->udi,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, has_capability,
				 G_TYPE_INVALID);
	if (!ret) {
		*has_capability = FALSE;
	}
	return ret;
}

/**
 * watch_device_property_modified:
 *
 * @key: Property key
 * @is_added: If the key was added
 * @is_removed: If the key was removed
 *
 * Invoked when a property of a device in the Global Device List is
 * changed, and we have we have subscribed to changes for that device.
 */
static void
watch_device_property_modified (DBusGProxy  *proxy,
				const gchar *key,
				gboolean     is_added,
				gboolean     is_removed,
				gboolean     finally,
				HalGDevice  *device)
{
	g_signal_emit (device, signals [DEVICE_PROPERTY_MODIFIED], 0,
		       key, is_added, is_removed, finally);
}

/**
 * watch_device_properties_modified_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @device: This class instance
 *
 * Demultiplex the composite PropertyModified events here.
 */
static void
watch_device_properties_modified_cb (DBusGProxy *proxy,
				     gint	 type,
				     GPtrArray  *properties,
				     HalGDevice *device)
{
	GValueArray *array;
	const gchar *udi;
	const gchar *key;
	gboolean     added;
	gboolean     removed;
	gboolean     finally = FALSE;
	guint	     i;

	udi = dbus_g_proxy_get_path (proxy);

	array = NULL;

	for (i = 0; i < properties->len; i++) {
		array = g_ptr_array_index (properties, i);
		if (array->n_values != 3) {
			g_warning ("array->n_values invalid (!3)");
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

		watch_device_property_modified (proxy, key, added, removed, finally, device);
	}
}

/**
 * hal_gdevice_watch_property_modified:
 *
 * Watch the specified device, so it emits device-property-modified and
 * adds to the gpm cache so we don't get asked to add it again.
 */
gboolean
hal_gdevice_watch_property_modified (HalGDevice *device)
{
	DBusGProxy *proxy;
	GType struct_array_type, struct_type;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	if (device->priv->use_property_modified) {
		/* already watched */
		return FALSE;
	}

	device->priv->use_property_modified = TRUE;

	struct_type = dbus_g_type_get_struct ("GValueArray",
					      G_TYPE_STRING,
					      G_TYPE_BOOLEAN,
					      G_TYPE_BOOLEAN,
					      G_TYPE_INVALID);

	struct_array_type = dbus_g_type_get_collection ("GPtrArray", struct_type);

	dbus_g_object_register_marshaller (libhal_marshal_VOID__INT_BOXED,
					   G_TYPE_NONE, G_TYPE_INT,
					   struct_array_type, G_TYPE_INVALID);

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_add_signal (proxy, "PropertyModified",
				 G_TYPE_INT, struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "PropertyModified",
				     G_CALLBACK (watch_device_properties_modified_cb), device, NULL);
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
			   HalGDevice  *device)
{
	g_signal_emit (device, signals [DEVICE_CONDITION], 0, condition, details);
}

/**
 * hal_gdevice_watch_condition:
 *
 * Watch the specified device, so it emits a device-condition
 */
gboolean
hal_gdevice_watch_condition (HalGDevice *device)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	if (device->priv->use_condition) {
		/* already watched */
		return FALSE;
	}

	device->priv->use_condition = TRUE;

	dbus_g_object_register_marshaller (libhal_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING,
					   G_TYPE_INVALID);

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_add_signal (proxy, "Condition",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "Condition",
				     G_CALLBACK (watch_device_condition_cb), device, NULL);
	return TRUE;
}

/**
 * hal_gdevice_remove_condition:
 *
 * Remove the specified device, so it does not emit device-condition signals.
 */
gboolean
hal_gdevice_remove_condition (HalGDevice *device)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);

	if (device->priv->use_condition == FALSE) {
		/* already connected */
		return FALSE;
	}

	device->priv->use_condition = FALSE;

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_disconnect_signal (proxy, "Condition",
					G_CALLBACK (watch_device_condition_cb), device);
	return TRUE;
}

/**
 * hal_gdevice_remove_property_modified:
 *
 * Remove the specified device, so it does not emit device-propery-modified.
 */
gboolean
hal_gdevice_remove_property_modified (HalGDevice *device)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);

	if (device->priv->use_property_modified == FALSE) {
		/* already disconnected */
		return FALSE;
	}

	device->priv->use_property_modified = FALSE;

	proxy = dbus_proxy_get_proxy (device->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_disconnect_signal (proxy, "PropertyModified",
				        G_CALLBACK (watch_device_properties_modified_cb), device);
	return TRUE;
}

/**
 * proxy_status_cb:
 * @proxy: The dbus raw proxy
 * @status: The status of the service, where TRUE is connected
 * @hal_manager: This class instance
 **/
static void
proxy_status_cb (DBusGProxy *proxy,
		 gboolean    status,
		 HalGDevice *device)
{
	g_return_if_fail (LIBHAL_IS_GDEVICE (device));
	if (status) {
		/* should join */
	} else {
		/* should unjoin */
	}
}

/**
 * hal_gdevice_class_init:
 * @klass: This class instance
 **/
static void
hal_gdevice_class_init (HalGDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = hal_gdevice_finalize;
	g_type_class_add_private (klass, sizeof (HalGDevicePrivate));

	signals [DEVICE_PROPERTY_MODIFIED] =
		g_signal_new ("property-modified",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGDeviceClass, device_property_modified),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	signals [DEVICE_CONDITION] =
		g_signal_new ("device-condition",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGDeviceClass, device_condition),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);
}

/**
 * hal_gdevice_init:
 *
 * @hal_gdevice: This class instance
 **/
static void
hal_gdevice_init (HalGDevice *device)
{
	GError *error = NULL;

	device->priv = LIBHAL_GDEVICE_GET_PRIVATE (device);

	device->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	device->priv->use_property_modified = FALSE;
	device->priv->use_condition = FALSE;

	/* get the manager connection */
	device->priv->gproxy = dbus_proxy_new ();
	g_signal_connect (device->priv->gproxy, "proxy-status",
			  G_CALLBACK (proxy_status_cb), device);
}

/**
 * hal_gdevice_finalize:
 * @object: This class instance
 **/
static void
hal_gdevice_finalize (GObject *object)
{
	HalGDevice *device;
	g_return_if_fail (object != NULL);
	g_return_if_fail (LIBHAL_IS_GDEVICE (object));

	device = LIBHAL_GDEVICE (object);
	device->priv = LIBHAL_GDEVICE_GET_PRIVATE (device);

	if (device->priv->use_property_modified) {
		hal_gdevice_remove_property_modified (device);
	}
	if (device->priv->use_condition) {
		hal_gdevice_remove_condition (device);
	}

	g_object_unref (device->priv->gproxy);
	g_free (device->priv->udi);

	G_OBJECT_CLASS (hal_gdevice_parent_class)->finalize (object);
}

/**
 * hal_gdevice_new:
 * Return value: new HalGDevice instance.
 **/
HalGDevice *
hal_gdevice_new (void)
{
	HalGDevice *device = g_object_new (LIBHAL_TYPE_GDEVICE, NULL);
	return LIBHAL_GDEVICE (device);
}
