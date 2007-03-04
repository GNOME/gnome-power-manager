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

#include "libhal-marshal.h"
#include "libhal-gpower.h"
#include "libhal-gdevice.h"
#include "libhal-gmanager.h"
#include "../src/gpm-debug.h"
#include "../src/gpm-proxy.h"

static void     hal_gdevice_class_init (HalGDeviceClass *klass);
static void     hal_gdevice_init       (HalGDevice      *hal_gdevice);
static void     hal_gdevice_finalize   (GObject	     *object);

#define LIBHAL_GDEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), LIBHAL_TYPE_GDEVICE, HalGDevicePrivate))

struct HalGDevicePrivate
{
	DBusGConnection		*connection;
	gboolean		 use_property_modified;
	gboolean		 use_condition;
	GpmProxy		*gproxy;
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

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (HalGDevice, hal_gdevice, G_TYPE_OBJECT)

/**
 * hal_gdevice_set_udi:
 *
const gchar  * * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_gdevice_set_udi (HalGDevice *hal_gdevice,
		        const gchar  *udi)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	if (hal_gdevice->priv->udi != NULL) {
		/* aready set UDI */
		return FALSE;
	}

	proxy = gpm_proxy_assign (hal_gdevice->priv->gproxy,
				  GPM_PROXY_SYSTEM,
				  HAL_DBUS_SERVICE,
				  udi,
				  HAL_DBUS_INTERFACE_DEVICE);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy failed");
		return FALSE;
	}
	hal_gdevice->priv->udi = g_strdup (udi);

	return TRUE;
}

/**
 * hal_gdevice_get_udi:
 *
 * Return value: UDI
 **/
const gchar *
hal_gdevice_get_udi (HalGDevice *hal_gdevice)
{
	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);

	return hal_gdevice->priv->udi;
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
hal_gdevice_get_bool (HalGDevice *hal_gdevice,
			 const gchar  *key,
			 gboolean     *value,
			 GError      **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (hal_gdevice->priv->udi != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "GetPropertyBoolean", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, value,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
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
hal_gdevice_get_string (HalGDevice   *hal_gdevice,
			const gchar  *key,
			gchar       **value,
			GError      **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (hal_gdevice->priv->udi != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "GetPropertyString", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_STRING, value,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
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
hal_gdevice_get_int (HalGDevice *hal_gdevice,
			const gchar  *key,
			gint         *value,
			GError      **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (hal_gdevice->priv->udi != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "GetPropertyInteger", error,
				 G_TYPE_STRING, key,
				 G_TYPE_INVALID,
				 G_TYPE_INT, value,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
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
hal_gdevice_get_uint (HalGDevice *hal_gdevice,
			 const gchar  *key,
			 guint        *value,
			 GError      **error)
{
	gint tvalue;
	gboolean ret;

	/* bodge */
	ret = hal_gdevice_get_int (hal_gdevice, key, &tvalue, error);
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
hal_gdevice_query_capability (HalGDevice *hal_gdevice,
			         const gchar  *capability,
			         gboolean     *has_capability,
			         GError      **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);
	g_return_val_if_fail (capability != NULL, FALSE);
	g_return_val_if_fail (has_capability != NULL, FALSE);
	g_return_val_if_fail (hal_gdevice->priv->udi != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	ret = dbus_g_proxy_call (proxy, "QueryCapability", error,
				 G_TYPE_STRING, hal_gdevice->priv->udi,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, has_capability,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
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
watch_device_property_modified (DBusGProxy   *proxy,
				const gchar  *key,
				gboolean      is_added,
				gboolean      is_removed,
				gboolean      finally,
				HalGDevice *hal_gdevice)
{
	const gchar *udi = hal_gdevice->priv->udi;
	gpm_debug ("emitting property-modified : udi=%s, key=%s, "
		   "added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	g_signal_emit (hal_gdevice, signals [DEVICE_PROPERTY_MODIFIED], 0,
		       udi, key, is_added, is_removed, finally);
}

/**
 * watch_device_properties_modified_cb:
 *
 * @proxy: The org.freedesktop.Hal.Manager proxy
 * @hal_gdevice: This class instance
 *
 * Demultiplex the composite PropertyModified events here.
 */
static void
watch_device_properties_modified_cb (DBusGProxy   *proxy,
				     gint	   type,
				     GPtrArray    *properties,
				     HalGDevice *hal_gdevice)
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

		watch_device_property_modified (proxy, key, added, removed, finally, hal_gdevice);
	}
}

/**
 * hal_gdevice_watch_property_modified:
 *
 * Watch the specified device, so it emits device-property-modified and
 * adds to the gpm cache so we don't get asked to add it again.
 */
gboolean
hal_gdevice_watch_property_modified (HalGDevice *hal_gdevice)
{
	DBusGProxy *proxy;
	GType struct_array_type, struct_type;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);
	g_return_val_if_fail (hal_gdevice->priv->udi != NULL, FALSE);

	if (hal_gdevice->priv->use_property_modified == TRUE) {
		/* already watched */
		return FALSE;
	}

	hal_gdevice->priv->use_property_modified = TRUE;

	struct_type = dbus_g_type_get_struct ("GValueArray", 
					      G_TYPE_STRING, 
					      G_TYPE_BOOLEAN, 
					      G_TYPE_BOOLEAN, 
					      G_TYPE_INVALID);

	struct_array_type = dbus_g_type_get_collection ("GPtrArray", struct_type);

	dbus_g_object_register_marshaller (libhal_marshal_VOID__INT_BOXED,
					   G_TYPE_NONE, G_TYPE_INT,
					   struct_array_type, G_TYPE_INVALID);

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_add_signal (proxy, "PropertyModified",
				 G_TYPE_INT, struct_array_type, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "PropertyModified",
				     G_CALLBACK (watch_device_properties_modified_cb), hal_gdevice, NULL);
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
			   HalGDevice      *hal_gdevice)
{
	const gchar *udi;
	udi = dbus_g_proxy_get_path (proxy);

	gpm_debug ("emitting device-condition : %s, %s (%s)", udi, condition, details);
	g_signal_emit (hal_gdevice, signals [DEVICE_CONDITION], 0, udi, condition, details);
}

/**
 * hal_gdevice_watch_condition:
 *
 * Watch the specified device, so it emits a device-condition
 */
gboolean
hal_gdevice_watch_condition (HalGDevice *hal_gdevice)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);
	g_return_val_if_fail (hal_gdevice->priv->udi != NULL, FALSE);

	if (hal_gdevice->priv->use_condition == TRUE) {
		/* already watched */
		return FALSE;
	}

	hal_gdevice->priv->use_condition = TRUE;

	dbus_g_object_register_marshaller (libhal_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING,
					   G_TYPE_INVALID);

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_add_signal (proxy, "Condition",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "Condition",
				     G_CALLBACK (watch_device_condition_cb), hal_gdevice, NULL);
	return TRUE;
}

/**
 * hal_gdevice_remove_condition:
 *
 * Remove the specified device, so it does not emit device-condition signals.
 */
gboolean
hal_gdevice_remove_condition (HalGDevice *hal_gdevice)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);

	if (hal_gdevice->priv->use_condition == FALSE) {
		/* already connected */
		return FALSE;
	}

	hal_gdevice->priv->use_condition = FALSE;

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_disconnect_signal (proxy, "Condition",
					G_CALLBACK (watch_device_condition_cb), hal_gdevice);
	return TRUE;
}

/**
 * hal_gdevice_remove_property_modified:
 *
 * Remove the specified device, so it does not emit device-propery-modified.
 */
gboolean
hal_gdevice_remove_property_modified (HalGDevice *hal_gdevice)
{
	DBusGProxy *proxy;

	g_return_val_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice), FALSE);

	if (hal_gdevice->priv->use_property_modified == FALSE) {
		/* already disconnected */
		return FALSE;
	}

	hal_gdevice->priv->use_property_modified = FALSE;

	proxy = gpm_proxy_get_proxy (hal_gdevice->priv->gproxy);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		gpm_warning ("proxy NULL!!");
		return FALSE;
	}
	dbus_g_proxy_disconnect_signal (proxy, "PropertyModified",
				        G_CALLBACK (watch_device_properties_modified_cb), hal_gdevice);
	return TRUE;
}

/**
 * proxy_status_cb:
 * @proxy: The dbus raw proxy
 * @status: The status of the service, where TRUE is connected
 * @hal_manager: This class instance
 **/
static void
proxy_status_cb (DBusGProxy   *proxy,
		 gboolean      status,
		 HalGDevice *hal_gdevice)
{
	g_return_if_fail (LIBHAL_IS_GDEVICE (hal_gdevice));
	if (status == TRUE) {
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
			      libhal_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 5, G_TYPE_STRING, G_TYPE_STRING,
			      G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	signals [DEVICE_CONDITION] =
		g_signal_new ("device-condition",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGDeviceClass, device_condition),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

/**
 * hal_gdevice_init:
 *
 * @hal_gdevice: This class instance
 **/
static void
hal_gdevice_init (HalGDevice *hal_gdevice)
{
	GError *error = NULL;

	hal_gdevice->priv = LIBHAL_GDEVICE_GET_PRIVATE (hal_gdevice);

	hal_gdevice->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		gpm_warning ("%s", error->message);
		g_error_free (error);
	}

	hal_gdevice->priv->use_property_modified = FALSE;
	hal_gdevice->priv->use_condition = FALSE;

	/* get the manager connection */
	hal_gdevice->priv->gproxy = gpm_proxy_new ();
	g_signal_connect (hal_gdevice->priv->gproxy, "proxy-status",
			  G_CALLBACK (proxy_status_cb), hal_gdevice);
}

/**
 * hal_gdevice_finalize:
 * @object: This class instance
 **/
static void
hal_gdevice_finalize (GObject *object)
{
	HalGDevice *hal_gdevice;
	g_return_if_fail (object != NULL);
	g_return_if_fail (LIBHAL_IS_GDEVICE (object));

	hal_gdevice = LIBHAL_GDEVICE (object);
	hal_gdevice->priv = LIBHAL_GDEVICE_GET_PRIVATE (hal_gdevice);

	if (hal_gdevice->priv->use_property_modified == TRUE) {
		hal_gdevice_remove_property_modified (hal_gdevice);
	}
	if (hal_gdevice->priv->use_condition == TRUE) {
		hal_gdevice_remove_condition (hal_gdevice);
	}

	g_object_unref (hal_gdevice->priv->gproxy);
	g_free (hal_gdevice->priv->udi);

	G_OBJECT_CLASS (hal_gdevice_parent_class)->finalize (object);
}

/**
 * hal_gdevice_new:
 * Return value: new HalGDevice instance.
 **/
HalGDevice *
hal_gdevice_new (void)
{
	HalGDevice *hal_gdevice = g_object_new (LIBHAL_TYPE_GDEVICE, NULL);
	return LIBHAL_GDEVICE (hal_gdevice);
}
