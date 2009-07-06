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

#include "egg-debug.h"
#include "egg-dbus-proxy.h"

#include "hal-marshal.h"
#include "hal-device.h"
#include "hal-manager.h"

static void     hal_device_class_init (HalDeviceClass *klass);
static void     hal_device_init       (HalDevice      *device);
static void     hal_device_finalize   (GObject	     *object);

#define HAL_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), HAL_TYPE_DEVICE, HalDevicePrivate))

struct HalDevicePrivate
{
	DBusGConnection		*connection;
	EggDbusProxy		*gproxy;
	gchar			*udi;
};

/* Signals emitted from HalDevice are:
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

G_DEFINE_TYPE (HalDevice, hal_device, G_TYPE_OBJECT)

/**
 * hal_device_set_udi:
 *
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_device_set_udi (HalDevice  *device, const gchar *udi)
{
	DBusGProxy *proxy;
	DBusGConnection *connection;

	g_return_val_if_fail (HAL_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (udi != NULL, FALSE);

	if (device->priv->udi != NULL) {
		/* aready set UDI */
		return FALSE;
	}

	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
	proxy = egg_dbus_proxy_assign (device->priv->gproxy, connection,
				       HAL_DBUS_SERVICE, udi, HAL_DBUS_INTERFACE_DEVICE);
	if (DBUS_IS_G_PROXY (proxy) == FALSE) {
		g_warning ("proxy failed");
		return FALSE;
	}
	device->priv->udi = g_strdup (udi);

	return TRUE;
}

/**
 * hal_device_get_bool:
 *
 * @hal_device: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
gboolean
hal_device_get_bool (HalDevice  *device,
		      const gchar *key,
		      gboolean    *value,
		      GError     **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (HAL_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	proxy = egg_dbus_proxy_get_proxy (device->priv->gproxy);
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
 * hal_device_get_int:
 *
 * @hal_device: This class instance
 * @key: The key to query
 * @value: return value, passed by ref
 * Return value: TRUE for success, FALSE for failure
 **/
static gboolean
hal_device_get_int (HalDevice   *device,
		     const gchar  *key,
		     gint         *value,
		     GError      **error)
{
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (HAL_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (device->priv->udi != NULL, FALSE);

	proxy = egg_dbus_proxy_get_proxy (device->priv->gproxy);
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
 * hal_device_get_uint:
 *
 * HAL has no concept of a UINT, only INT
 **/
gboolean
hal_device_get_uint (HalDevice   *device,
		      const gchar  *key,
		      guint        *value,
		      GError      **error)
{
	gint tvalue;
	gboolean ret;

	/* bodge */
	ret = hal_device_get_int (device, key, &tvalue, error);
	*value = (guint) tvalue;
	return ret;
}

/**
 * hal_device_class_init:
 * @klass: This class instance
 **/
static void
hal_device_class_init (HalDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = hal_device_finalize;
	g_type_class_add_private (klass, sizeof (HalDevicePrivate));
}

/**
 * hal_device_init:
 *
 * @hal_device: This class instance
 **/
static void
hal_device_init (HalDevice *device)
{
	GError *error = NULL;

	device->priv = HAL_DEVICE_GET_PRIVATE (device);

	device->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	/* get the manager connection */
	device->priv->gproxy = egg_dbus_proxy_new ();
}

/**
 * hal_device_finalize:
 * @object: This class instance
 **/
static void
hal_device_finalize (GObject *object)
{
	HalDevice *device;
	g_return_if_fail (object != NULL);
	g_return_if_fail (HAL_IS_DEVICE (object));

	device = HAL_DEVICE (object);
	device->priv = HAL_DEVICE_GET_PRIVATE (device);

	g_object_unref (device->priv->gproxy);
	g_free (device->priv->udi);

	G_OBJECT_CLASS (hal_device_parent_class)->finalize (object);
}

/**
 * hal_device_new:
 * Return value: new HalDevice instance.
 **/
HalDevice *
hal_device_new (void)
{
	HalDevice *device = g_object_new (HAL_TYPE_DEVICE, NULL);
	return HAL_DEVICE (device);
}
