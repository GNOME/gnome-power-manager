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
#include "egg-obj-list.h"

#include "dkp-device.h"
#include "dkp-object.h"
#include "dkp-stats-obj.h"
#include "dkp-history-obj.h"

static void	dkp_device_class_init	(DkpDeviceClass	*klass);
static void	dkp_device_init		(DkpDevice	*device);
static void	dkp_device_finalize	(GObject		*object);

#define DKP_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_DEVICE, DkpDevicePrivate))

struct DkpDevicePrivate
{
	gchar			*object_path;
	DkpObject		*obj;
	DBusGConnection		*bus;
	DBusGProxy		*proxy_device;
	DBusGProxy		*proxy_props;
};

enum {
	DKP_DEVICE_CHANGED,
	DKP_DEVICE_LAST_SIGNAL
};

static guint signals [DKP_DEVICE_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DkpDevice, dkp_device, G_TYPE_OBJECT)

/**
 * dkp_device_get_device_properties:
 **/
static GHashTable *
dkp_device_get_device_properties (DkpDevice *device)
{
	gboolean ret;
	GError *error = NULL;
	GHashTable *hash_table = NULL;

	ret = dbus_g_proxy_call (device->priv->proxy_props, "GetAll", &error,
				 G_TYPE_STRING, "org.freedesktop.DeviceKit.Power.Device",
				 G_TYPE_INVALID,
				 dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				 &hash_table,
				 G_TYPE_INVALID);
	if (!ret) {
		egg_debug ("Couldn't call GetAll() to get properties for %s: %s", device->priv->object_path, error->message);
		g_error_free (error);
		goto out;
	}
out:
	return hash_table;
}

/**
 * dkp_device_refresh_internal:
 **/
static gboolean
dkp_device_refresh_internal (DkpDevice *device)
{
	GHashTable *hash;

	/* get all the properties */
	hash = dkp_device_get_device_properties (device);
	if (hash == NULL) {
		egg_warning ("Cannot get device properties for %s", device->priv->object_path);
		return FALSE;
	}
	dkp_object_set_from_map (device->priv->obj, hash);
	g_hash_table_unref (hash);
	return TRUE;
}

/**
 * dkp_device_changed_cb:
 **/
static void
dkp_device_changed_cb (DBusGProxy *proxy, DkpDevice *device)
{
	g_return_if_fail (DKP_IS_DEVICE (device));
	dkp_device_refresh_internal (device);
	g_signal_emit (device, signals [DKP_DEVICE_CHANGED], 0, device->priv->obj);
}

/**
 * dkp_device_set_object_path:
 **/
gboolean
dkp_device_set_object_path (DkpDevice *device, const gchar *object_path)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	DBusGProxy *proxy_device;
	DBusGProxy *proxy_props;

	g_return_val_if_fail (DKP_IS_DEVICE (device), FALSE);

	if (device->priv->object_path != NULL)
		return FALSE;
	if (object_path == NULL)
		return FALSE;

	/* connect to the bus */
	device->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (device->priv->bus == NULL) {
		egg_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to the correct path for properties */
	proxy_props = dbus_g_proxy_new_for_name (device->priv->bus, "org.freedesktop.DeviceKit.Power",
						 object_path, "org.freedesktop.DBus.Properties");
	if (proxy_props == NULL) {
		egg_warning ("Couldn't connect to proxy");
		goto out;
	}

	/* connect to the correct path for all the other methods */
	proxy_device = dbus_g_proxy_new_for_name (device->priv->bus, "org.freedesktop.DeviceKit.Power",
						  object_path, "org.freedesktop.DeviceKit.Power.Device");
	if (proxy_device == NULL) {
		egg_warning ("Couldn't connect to proxy");
		goto out;
	}

	/* listen to Changed */
	dbus_g_proxy_add_signal (proxy_device, "Changed", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy_device, "Changed",
				     G_CALLBACK (dkp_device_changed_cb), device, NULL);

	/* yay */
	egg_debug ("using object_path: %s", object_path);
	device->priv->proxy_device = proxy_device;
	device->priv->proxy_props = proxy_props;
	device->priv->object_path = g_strdup (object_path);

	/* coldplug */
	ret = dkp_device_refresh_internal (device);
	if (!ret)
		egg_warning ("cannot refresh");
out:
	return ret;
}

/**
 * dkp_device_get_object_path:
 **/
const gchar *
dkp_device_get_object_path (const DkpDevice *device)
{
	g_return_val_if_fail (DKP_IS_DEVICE (device), NULL);
	return device->priv->object_path;
}

/**
 * dkp_device_get_object:
 **/
const DkpObject *
dkp_device_get_object (const DkpDevice *device)
{
	g_return_val_if_fail (DKP_IS_DEVICE (device), NULL);
	return device->priv->obj;
}

/**
 * dkp_device_print_history:
 **/
static gboolean
dkp_device_print_history (const DkpDevice *device, const gchar *type)
{
	guint i;
	EggObjList *array;
	DkpHistoryObj *obj;
	gboolean ret = FALSE;

	/* get a fair chunk of data */
	array = dkp_device_get_history (device, type, 120, 10);
	if (array == NULL)
		goto out;

	/* pretty print */
	g_print ("  History (%s):\n", type);
	for (i=0; i<array->len; i++) {
		obj = (DkpHistoryObj *) egg_obj_list_index (array, i);
		g_print ("    %i\t%.3f\t%s\n", obj->time, obj->value, dkp_device_state_to_text (obj->state));
	}
	g_object_unref (array);
	ret = TRUE;
out:
	return ret;
}

/**
 * dkp_device_print:
 **/
gboolean
dkp_device_print (const DkpDevice *device)
{
	g_return_val_if_fail (DKP_IS_DEVICE (device), FALSE);

	/* print to screen */
	dkp_object_print (device->priv->obj);

	/* if we can, get history */
	dkp_device_print_history (device, "charge");
	dkp_device_print_history (device, "rate");

	return TRUE;
}

/**
 * dkp_device_refresh:
 **/
gboolean
dkp_device_refresh (DkpDevice *device)
{
	GError *error = NULL;
	gboolean ret;

	g_return_val_if_fail (DKP_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy_device != NULL, FALSE);

	/* just refresh the device */
	ret = dbus_g_proxy_call (device->priv->proxy_device, "Refresh", &error,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ret) {
		egg_debug ("Refresh() on %s failed: %s", device->priv->object_path, error->message);
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * dkp_device_get_history:
 *
 * Returns an array of %DkpHistoryObj's
 **/
EggObjList *
dkp_device_get_history (const DkpDevice *device, const gchar *type, guint timespec, guint resolution)
{
	GError *error = NULL;
	GType g_type_gvalue_array;
	GPtrArray *gvalue_ptr_array = NULL;
	GValueArray *gva;
	GValue *gv;
	guint i;
	DkpHistoryObj *obj;
	EggObjList *array = NULL;
	gboolean ret;

	g_return_val_if_fail (DKP_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy_device != NULL, FALSE);

	g_type_gvalue_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_UINT,
						G_TYPE_DOUBLE,
						G_TYPE_STRING,
						G_TYPE_INVALID));

	/* get compound data */
	ret = dbus_g_proxy_call (device->priv->proxy_device, "GetHistory", &error,
				 G_TYPE_STRING, type,
				 G_TYPE_UINT, timespec,
				 G_TYPE_UINT, resolution,
				 G_TYPE_INVALID,
				 g_type_gvalue_array, &gvalue_ptr_array,
				 G_TYPE_INVALID);
	if (!ret) {
		egg_debug ("GetHistory(%s,%i) on %s failed: %s", type, timespec,
			   device->priv->object_path, error->message);
		g_error_free (error);
		goto out;
	}

	/* no data */
	if (gvalue_ptr_array->len == 0)
		goto out;

	/* convert */
	array = egg_obj_list_new ();
	egg_obj_list_set_copy (array, (EggObjListCopyFunc) dkp_history_obj_copy);
	egg_obj_list_set_free (array, (EggObjListFreeFunc) dkp_history_obj_free);

	for (i=0; i<gvalue_ptr_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (gvalue_ptr_array, i);
		obj = dkp_history_obj_new ();
		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		obj->time = g_value_get_uint (gv);
		g_value_unset (gv);
		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		obj->value = g_value_get_double (gv);
		g_value_unset (gv);
		/* 2 */
		gv = g_value_array_get_nth (gva, 2);
		obj->state = dkp_device_state_from_text (g_value_get_string (gv));
		g_value_unset (gv);
		egg_obj_list_add (array, obj);
		dkp_history_obj_free (obj);
		g_value_array_free (gva);
	}

out:
	if (gvalue_ptr_array != NULL)
		g_ptr_array_free (gvalue_ptr_array, TRUE);
	return array;
}

/**
 * dkp_device_get_statistics:
 *
 * Returns an array of %DkpStatsObj's
 **/
EggObjList *
dkp_device_get_statistics (const DkpDevice *device, const gchar *type)
{
	GError *error = NULL;
	GType g_type_gvalue_array;
	GPtrArray *gvalue_ptr_array = NULL;
	GValueArray *gva;
	GValue *gv;
	guint i;
	DkpStatsObj *obj;
	EggObjList *array = NULL;
	gboolean ret;

	g_return_val_if_fail (DKP_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy_device != NULL, FALSE);

	g_type_gvalue_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_DOUBLE,
						G_TYPE_DOUBLE,
						G_TYPE_INVALID));

	/* get compound data */
	ret = dbus_g_proxy_call (device->priv->proxy_device, "GetStatistics", &error,
				 G_TYPE_STRING, type,
				 G_TYPE_INVALID,
				 g_type_gvalue_array, &gvalue_ptr_array,
				 G_TYPE_INVALID);
	if (!ret) {
		egg_debug ("GetStatistics(%s) on %s failed: %s", type,
			   device->priv->object_path, error->message);
		g_error_free (error);
		goto out;
	}

	/* no data */
	if (gvalue_ptr_array->len == 0)
		goto out;

	/* convert */
	array = egg_obj_list_new ();
	egg_obj_list_set_copy (array, (EggObjListCopyFunc) dkp_stats_obj_copy);
	egg_obj_list_set_free (array, (EggObjListFreeFunc) dkp_stats_obj_free);
	egg_obj_list_set_to_string (array, (EggObjListToStringFunc) dkp_stats_obj_to_string);
	egg_obj_list_set_from_string (array, (EggObjListFromStringFunc) dkp_stats_obj_from_string);

	for (i=0; i<gvalue_ptr_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (gvalue_ptr_array, i);
		obj = dkp_stats_obj_new ();
		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		obj->value = g_value_get_double (gv);
		g_value_unset (gv);
		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		obj->accuracy = g_value_get_double (gv);
		g_value_unset (gv);
		/* 2 */
		egg_obj_list_add (array, obj);
		dkp_stats_obj_free (obj);
		g_value_array_free (gva);
	}
out:
	if (gvalue_ptr_array != NULL)
		g_ptr_array_free (gvalue_ptr_array, TRUE);
	return array;
}

/**
 * dkp_device_class_init:
 * @klass: The DkpDeviceClass
 **/
static void
dkp_device_class_init (DkpDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dkp_device_finalize;

	/**
	 * PkClient::changed:
	 * @device: the #DkpDevice instance that emitted the signal
	 * @obj: the #DkpObject that has changed
	 *
	 * The ::changed signal is emitted when the device data has changed.
	 **/
	signals [DKP_DEVICE_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpDeviceClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	g_type_class_add_private (klass, sizeof (DkpDevicePrivate));
}

/**
 * dkp_device_init:
 * @device: This class instance
 **/
static void
dkp_device_init (DkpDevice *device)
{
	device->priv = DKP_DEVICE_GET_PRIVATE (device);
	device->priv->object_path = NULL;
	device->priv->proxy_device = NULL;
	device->priv->proxy_props = NULL;
	device->priv->obj = dkp_object_new ();
}

/**
 * dkp_device_finalize:
 * @object: The object to finalize
 **/
static void
dkp_device_finalize (GObject *object)
{
	DkpDevice *device;

	g_return_if_fail (DKP_IS_DEVICE (object));

	device = DKP_DEVICE (object);

	g_free (device->priv->object_path);
	dkp_object_free (device->priv->obj);
	if (device->priv->proxy_device != NULL)
		g_object_unref (device->priv->proxy_device);
	if (device->priv->proxy_props != NULL)
		g_object_unref (device->priv->proxy_props);

	G_OBJECT_CLASS (dkp_device_parent_class)->finalize (object);
}

/**
 * dkp_device_new:
 *
 * Return value: a new DkpDevice object.
 **/
DkpDevice *
dkp_device_new (void)
{
	DkpDevice *device;
	device = g_object_new (DKP_TYPE_DEVICE, NULL);
	return DKP_DEVICE (device);
}

