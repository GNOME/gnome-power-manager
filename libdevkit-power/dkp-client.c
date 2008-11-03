/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "egg-debug.h"
#include "dkp-client.h"
#include "dkp-client-device.h"

static void	dkp_client_class_init	(DkpClientClass	*klass);
static void	dkp_client_init		(DkpClient	*client);
static void	dkp_client_finalize	(GObject	*object);

#define DKP_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_CLIENT, DkpClientPrivate))

struct DkpClientPrivate
{
	DBusGConnection		*bus;
	DBusGProxy		*proxy;
	GHashTable		*hash;
	GPtrArray		*array;
};

enum {
	DKP_CLIENT_ADDED,
	DKP_CLIENT_CHANGED,
	DKP_CLIENT_REMOVED,
	DKP_CLIENT_LAST_SIGNAL
};

static guint signals [DKP_CLIENT_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DkpClient, dkp_client, G_TYPE_OBJECT)

/**
 * dkp_client_get_device:
 **/
static DkpClientDevice *
dkp_client_get_device (DkpClient *client, const gchar *object_path)
{
	DkpClientDevice *device;
	device = g_hash_table_lookup (client->priv->hash, object_path);
	return device;
}

/**
 * dkp_client_enumerate_devices:
 **/
GPtrArray *
dkp_client_enumerate_devices (const DkpClient *client)
{
	gboolean ret;
	GError *error = NULL;
	GPtrArray *devices = NULL;
	GType g_type_array;

	g_type_array = dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH);
	ret = dbus_g_proxy_call (client->priv->proxy, "EnumerateDevices", &error,
				 G_TYPE_INVALID,
				 g_type_array, &devices,
				 G_TYPE_INVALID);
	if (!ret) {
		egg_warning ("Couldn't enumerate devices: %s", error->message);
		g_error_free (error);
	}
	return devices;
}

/**
 * dkp_client_add:
 **/
static DkpClientDevice *
dkp_client_add (DkpClient *client, const gchar *object_path)
{
	DkpClientDevice *device;

	/* create new device */
	device = dkp_client_device_new ();
	dkp_client_device_set_object_path (device, object_path);

	g_ptr_array_add (client->priv->array, device);
	g_hash_table_insert (client->priv->hash, g_strdup (object_path), device);
	return device;
}

/**
 * dkp_client_remove:
 **/
static gboolean
dkp_client_remove (DkpClient *client, DkpClientDevice *device)
{
	/* deallocate it */
	g_object_unref (device);

	g_ptr_array_remove (client->priv->array, device);
	g_hash_table_remove (client->priv->hash, device);
	return TRUE;
}

/**
 * dkp_client_added_cb:
 **/
static void
dkp_client_added_cb (DBusGProxy *proxy, const gchar *object_path, DkpClient *client)
{
	DkpClientDevice *device;

	/* create new device */
	device = dkp_client_add (client, object_path);
	g_signal_emit (client, signals [DKP_CLIENT_ADDED], 0, device);
}

/**
 * dkp_client_changed_cb:
 **/
static void
dkp_client_changed_cb (DBusGProxy *proxy, const gchar *object_path, DkpClient *client)
{
	DkpClientDevice *device;
	device = dkp_client_get_device (client, object_path);
	if (device != NULL)
		g_signal_emit (client, signals [DKP_CLIENT_CHANGED], 0, device);
}

/**
 * dkp_client_removed_cb:
 **/
static void
dkp_client_removed_cb (DBusGProxy *proxy, const gchar *object_path, DkpClient *client)
{
	DkpClientDevice *device;
	device = dkp_client_get_device (client, object_path);
	if (device != NULL)
		g_signal_emit (client, signals [DKP_CLIENT_REMOVED], 0, device);
	dkp_client_remove (client, device);
}

/**
 * dkp_client_class_init:
 * @klass: The DkpClientClass
 **/
static void
dkp_client_class_init (DkpClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dkp_client_finalize;

	signals [DKP_CLIENT_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpClientClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [DKP_CLIENT_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpClientClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [DKP_CLIENT_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpClientClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	g_type_class_add_private (klass, sizeof (DkpClientPrivate));
}

/**
 * dkp_client_init:
 * @client: This class instance
 **/
static void
dkp_client_init (DkpClient *client)
{
	GError *error = NULL;
	const gchar *object_path;
	GPtrArray *devices;
	guint i;

	client->priv = DKP_CLIENT_GET_PRIVATE (client);
	client->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	client->priv->array = g_ptr_array_new ();

	/* get on the bus */
	client->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (client->priv->bus == NULL) {
		egg_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to main interface */
	client->priv->proxy = dbus_g_proxy_new_for_name (client->priv->bus, "org.freedesktop.DeviceKit.Power",
							 "/", "org.freedesktop.DeviceKit.Power");
	if (client->priv->proxy == NULL) {
		egg_warning ("Couldn't connect to proxy");
		goto out;
	}

	dbus_g_proxy_add_signal (client->priv->proxy, "DeviceAdded", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (client->priv->proxy, "DeviceRemoved", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (client->priv->proxy, "DeviceChanged", G_TYPE_STRING, G_TYPE_INVALID);

	/* all callbacks */
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceAdded",
				     G_CALLBACK (dkp_client_added_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceRemoved",
				     G_CALLBACK (dkp_client_removed_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceChanged",
				     G_CALLBACK (dkp_client_changed_cb), client, NULL);

	/* coldplug */
	devices = dkp_client_enumerate_devices (client);
	if (devices == NULL)
		goto out;
	for (i=0; i<devices->len; i++) {
		object_path = (const gchar *) g_ptr_array_index (devices, i);
		dkp_client_add (client, object_path);
	}
out:
	return;
}

/**
 * dkp_client_finalize:
 * @object: The object to finalize
 **/
static void
dkp_client_finalize (GObject *object)
{
	DkpClient *client;
	DkpClientDevice *device;
	guint i;

	g_return_if_fail (DKP_IS_CLIENT (object));

	client = DKP_CLIENT (object);

	/* free any devices */
	for (i=0; i<client->priv->array->len; i++) {
		device = (DkpClientDevice *) g_ptr_array_index (client->priv->array, i);
		dkp_client_remove (client, device);
	}

	g_ptr_array_free (client->priv->array, TRUE);
	g_hash_table_unref (client->priv->hash);
	dbus_g_connection_unref (client->priv->bus);

	G_OBJECT_CLASS (dkp_client_parent_class)->finalize (object);
}

/**
 * dkp_client_new:
 *
 * Return value: a new DkpClient object.
 **/
DkpClient *
dkp_client_new (void)
{
	DkpClient *client;
	client = g_object_new (DKP_TYPE_CLIENT, NULL);
	return DKP_CLIENT (client);
}

