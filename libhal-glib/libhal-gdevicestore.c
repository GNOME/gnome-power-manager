/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#include "libhal-marshal.h"
#include "libhal-gdevice.h"
#include "libhal-gdevicestore.h"
#include "../src/gpm-debug.h"

static void     hal_gdevicestore_class_init (HalGDevicestoreClass *klass);
static void     hal_gdevicestore_init       (HalGDevicestore      *devicestore);
static void     hal_gdevicestore_finalize   (GObject	          *object);

#define LIBHAL_GDEVICESTORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), LIBHAL_TYPE_GDEVICESTORE, HalGDevicestorePrivate))

struct HalGDevicestorePrivate
{
	GPtrArray		*array;		/* the device array */
};

enum {
	DEVICE_REMOVED,				/* is not expected to work yet */
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (HalGDevicestore, hal_gdevicestore, G_TYPE_OBJECT)

/**
 * hal_devicestore_index:
 *
 * Returns -1 if not found
 *
 * @devicestore: This store instance
 * @device: The device
 */
static gint
hal_gdevicestore_index (HalGDevicestore *devicestore, HalGDevice *device)
{
	HalGDevice *d;
	gint i;
	guint length;
	const gchar *udi;

	g_return_val_if_fail (LIBHAL_IS_GDEVICESTORE (devicestore), FALSE);
	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);

	length = devicestore->priv->array->len;
	udi = hal_gdevice_get_udi (device);
	gpm_debug ("is %s present in %p", udi, devicestore);

	/* trivial check, is instance the same (FAST) */
	for (i=0;i<length;i++) {
		d = (HalGDevice *) g_ptr_array_index (devicestore->priv->array, i);
		if (d == device) {
			gpm_debug ("Found %s with trivial check", udi);
			return i;
		}
	}

	/* non trivial check, is udi the same (SLOW) */
	for (i=0;i<length;i++) {
		d = (HalGDevice *) g_ptr_array_index (devicestore->priv->array, i);
		if (strcmp (hal_gdevice_get_udi (d), udi) == 0) {
			gpm_debug ("Found %s with udi check", udi);
			return i;
		}
	}

	gpm_debug ("Did not find %s", udi);
	return -1;
}

/**
 * hal_devicestore_present:
 *
 * @devicestore: This store instance
 * @device: The device
 */
gboolean
hal_gdevicestore_present (HalGDevicestore *devicestore, HalGDevice *device)
{
	g_return_val_if_fail (LIBHAL_IS_GDEVICESTORE (devicestore), FALSE);
	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);

	/* if we have an index, we have the device */
	if (hal_gdevicestore_index (devicestore, device) != -1) {
		return TRUE;
	}
	return FALSE;
}

/**
 * hal_devicestore_insert:
 *
 * @devicestore: This store instance
 * @device: The device
 */
gboolean
hal_gdevicestore_insert (HalGDevicestore *devicestore, HalGDevice *device)
{
	g_return_val_if_fail (LIBHAL_IS_GDEVICESTORE (devicestore), FALSE);
	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);

	if (hal_gdevicestore_present (devicestore, device) == TRUE) {
		gpm_warning ("already present, cannot add");
		return FALSE;
	}

	g_ptr_array_add (devicestore->priv->array, (gpointer) device);
	return TRUE;
}

/**
 * hal_devicestore_remove:
 *
 * @devicestore: This store instance
 * @device: The device
 */
gboolean
hal_gdevicestore_remove (HalGDevicestore *devicestore, HalGDevice *device)
{
	gint index;

	g_return_val_if_fail (LIBHAL_IS_GDEVICESTORE (devicestore), FALSE);
	g_return_val_if_fail (LIBHAL_IS_GDEVICE (device), FALSE);

	index = hal_gdevicestore_index (devicestore, device);
	if (index == -1) {
		gpm_warning ("device not present, cannot remove");
		return FALSE;
	}

	g_ptr_array_remove_index (devicestore->priv->array, index);

	return TRUE;
}

/**
 * hal_devicestore_print:
 *
 * @devicestore: This store instance
 */
gboolean
hal_gdevicestore_print (HalGDevicestore *devicestore)
{
	HalGDevice *d;
	guint i;
	guint length;

	g_return_val_if_fail (LIBHAL_IS_GDEVICESTORE (devicestore), FALSE);

	length = devicestore->priv->array->len;
	gpm_debug ("Printing device list in %p", devicestore);
	for (i=0;i<length;i++) {
		d = (HalGDevice *) g_ptr_array_index (devicestore->priv->array, i);
		gpm_debug ("%i: %s", i, hal_gdevice_get_udi (d));
	}	

	return TRUE;
}

/**
 * hal_gdevicestore_class_init:
 * @klass: This class instance
 **/
static void
hal_gdevicestore_class_init (HalGDevicestoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = hal_gdevicestore_finalize;
	g_type_class_add_private (klass, sizeof (HalGDevicestorePrivate));

	signals [DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HalGDevicestoreClass, device_removed),
			      NULL,
			      NULL,
			      libhal_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_STRING);
}

/**
 * hal_gdevicestore_init:
 *
 * @hal_gdevicestore: This class instance
 **/
static void
hal_gdevicestore_init (HalGDevicestore *devicestore)
{
	devicestore->priv = LIBHAL_GDEVICESTORE_GET_PRIVATE (devicestore);

	devicestore->priv->array = g_ptr_array_new ();
}

/**
 * hal_gdevicestore_finalize:
 * @object: This class instance
 **/
static void
hal_gdevicestore_finalize (GObject *object)
{
	HalGDevicestore *devicestore;
	HalGDevice *d;
	gint i;
	guint length;

	g_return_if_fail (object != NULL);
	g_return_if_fail (LIBHAL_IS_GDEVICESTORE (object));

	devicestore = LIBHAL_GDEVICESTORE (object);
	devicestore->priv = LIBHAL_GDEVICESTORE_GET_PRIVATE (devicestore);

	length = devicestore->priv->array->len;

	/* unref all */
	for (i=0;i<length;i++) {
		d = (HalGDevice *) g_ptr_array_index (devicestore->priv->array, i);
		g_object_unref (d);
	}	

	G_OBJECT_CLASS (hal_gdevicestore_parent_class)->finalize (object);
}

/**
 * hal_gdevicestore_new:
 * Return value: new HalGDevicestore instance.
 **/
HalGDevicestore *
hal_gdevicestore_new (void)
{
	HalGDevicestore *devicestore = g_object_new (LIBHAL_TYPE_GDEVICESTORE, NULL);
	return LIBHAL_GDEVICESTORE (devicestore);
}

