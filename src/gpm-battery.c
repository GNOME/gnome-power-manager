/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>

#include <libhal-gpower.h>
#include <libhal-gdevice.h>
#include <libhal-gdevicestore.h>
#include <libhal-gmanager.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-marshal.h"
#include "gpm-debug.h"

#include "gpm-battery.h"

static void     gpm_battery_class_init (GpmBatteryClass *klass);
static void     gpm_battery_init       (GpmBattery      *battery);
static void     gpm_battery_finalize   (GObject	       *object);

#define GPM_BATTERY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BATTERY, GpmBatteryPrivate))

struct GpmBatteryPrivate
{
	HalGManager		*hal_manager;
	HalGDevicestore		*hal_devicestore;
};

enum {
	BATTERY_MODIFIED,
	BATTERY_ADDED,
	BATTERY_REMOVED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };
static gpointer gpm_battery_object = NULL;

G_DEFINE_TYPE (GpmBattery, gpm_battery, G_TYPE_OBJECT)

/**
 * gpm_battery_class_init:
 * @klass: This class instance
 **/
static void
gpm_battery_class_init (GpmBatteryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_battery_finalize;

	g_type_class_add_private (klass, sizeof (GpmBatteryPrivate));

	signals [BATTERY_MODIFIED] =
		g_signal_new ("battery-modified",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBatteryClass, battery_modified),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_STRING_BOOLEAN,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals [BATTERY_ADDED] =
		g_signal_new ("battery-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBatteryClass, battery_added),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [BATTERY_REMOVED] =
		g_signal_new ("battery-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBatteryClass, battery_removed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * hal_device_property_modified_cb:
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
hal_device_property_modified_cb (HalGDevice   *device,
				 const gchar  *key,
				 gboolean      is_added,
				 gboolean      is_removed,
				 gboolean      finally,
				 GpmBattery   *battery)
{
	const gchar *udi = hal_gdevice_get_udi (device);
	gpm_debug ("udi=%s, key=%s, added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	/* do not process keys that have been removed */
	if (is_removed == TRUE) {
		return;
	}

	/* only match battery* values */
	if (strncmp (key, "battery", 7) == 0) {
		gpm_debug ("emitting battery-modified : %s, %s", udi, key);
		g_signal_emit (battery, signals [BATTERY_MODIFIED], 0, udi, key, finally);
	}
}

/**
 * watch_add_battery:
 *
 * @udi: The HAL UDI
 */
static gboolean
watch_add_battery (GpmBattery    *battery,
		   const gchar   *udi)
{
	HalGDevice *device;

	device = hal_gdevicestore_find_udi (battery->priv->hal_devicestore, udi);
	if (device != NULL) {
		gpm_warning ("cannot watch already watched battery '%s'", udi);
		return FALSE;
	}

	/* create a new device */
	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, udi);
	hal_gdevice_watch_property_modified (device);
	g_signal_connect (device, "property-modified",
			  G_CALLBACK (hal_device_property_modified_cb), battery);

	/* when added to the devicestore, devices are automatically unref'ed */
	hal_gdevicestore_insert (battery->priv->hal_devicestore, device);

	gpm_debug ("emitting battery-added : %s", udi);
	g_signal_emit (battery, signals [BATTERY_ADDED], 0, udi);
	return TRUE;
}

/**
 * hal_device_removed_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @battery: This battery instance
 */
static gboolean
hal_device_removed_cb (HalGManager *hal_manager,
		       const gchar  *udi,
		       GpmBattery   *battery)
{
	HalGDevice *device;

	gpm_debug ("udi=%s", udi);

	device = hal_gdevicestore_find_udi (battery->priv->hal_devicestore, udi);
	if (device == NULL) {
		gpm_warning ("cannot remove battery not in devicestore");
		return FALSE;
	}
	hal_gdevicestore_remove (battery->priv->hal_devicestore, device);

	g_signal_emit (battery, signals [BATTERY_REMOVED], 0, udi);
	return TRUE;
}

/**
 * hal_new_capability_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @capability: the capability, e.g. "battery"
 * @battery: This battery instance
 */
static void
hal_new_capability_cb (HalGManager *hal_manager,
		       const gchar   *udi,
		       const gchar   *capability,
		       GpmBattery    *battery)
{
	gpm_debug ("udi=%s, capability=%s", udi, capability);

	if (strcmp (capability, "battery") == 0) {
		watch_add_battery (battery, udi);
	}
}

/**
 * hal_device_added_cb:
 *
 * @hal: The hal instance
 * @udi: The HAL UDI
 * @battery: This battery instance
 */
static void
hal_device_added_cb (HalGManager *hal_manager,
		     const gchar   *udi,
		     GpmBattery    *battery)
{
	gboolean is_battery;
	gboolean dummy;
	HalGDevice *device;

	/* find out if the new device has capability battery
	   this might fail for CSR as the addon is weird */
	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, udi);
	hal_gdevice_query_capability (device, "battery", &is_battery, NULL);

	/* try harder */
	if (is_battery == FALSE) {
		is_battery = hal_gdevice_get_bool (device, "battery.present", &dummy, NULL);
	}

	/* if a battery, then add */
	if (is_battery == TRUE) {
		watch_add_battery (battery, udi);
	}
	g_object_unref (device);
}

/**
 * coldplug_batteries:
 *
 *  @return			If any devices of capability battery were found.
 *
 * Coldplugs devices of type battery & ups at startup
 */
static gboolean
coldplug_batteries (GpmBattery *battery)
{
	int    i;
	char **device_names = NULL;
	gboolean ret;
	GError *error;

	/* devices of type battery */
	error = NULL;
	ret = hal_gmanager_find_capability (battery->priv->hal_manager, "battery", &device_names, &error);
	if (ret == FALSE) {
		gpm_warning ("Couldn't obtain list of batteries: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	for (i = 0; device_names[i]; i++) {
		watch_add_battery (battery, device_names[i]);
	}

	hal_gmanager_free_capability (device_names);

	return TRUE;
}

/**
 * gpm_battery_coldplug:
 *
 *
 * Cold-plugs (re-adds) all the basic devices.
 */
void
gpm_battery_coldplug (GpmBattery *battery)
{
	coldplug_batteries (battery);
}

/**
 * gpm_battery_start_idle:
 */
static gboolean
gpm_battery_start_idle (GpmBattery *battery)
{
       coldplug_batteries (battery);
       return FALSE;
}

/**
 * gpm_battery_init:
 */
static void
gpm_battery_init (GpmBattery *battery)
{
	battery->priv = GPM_BATTERY_GET_PRIVATE (battery);

	battery->priv->hal_manager = hal_gmanager_new ();
	battery->priv->hal_devicestore = hal_gdevicestore_new ();

	g_signal_connect (battery->priv->hal_manager, "device-added",
			  G_CALLBACK (hal_device_added_cb), battery);
	g_signal_connect (battery->priv->hal_manager, "device-removed",
			  G_CALLBACK (hal_device_removed_cb), battery);
	g_signal_connect (battery->priv->hal_manager, "new-capability",
			  G_CALLBACK (hal_new_capability_cb), battery);

	g_idle_add ((GSourceFunc)gpm_battery_start_idle, battery);
}

/**
 * gpm_battery_coldplug:
 *
 * @object: This battery instance
 */
static void
gpm_battery_finalize (GObject *object)
{
	GpmBattery *battery;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BATTERY (object));

	battery = GPM_BATTERY (object);

	g_return_if_fail (battery->priv != NULL);

	g_object_unref (battery->priv->hal_manager);
	g_object_unref (battery->priv->hal_devicestore);

	G_OBJECT_CLASS (gpm_battery_parent_class)->finalize (object);
}

/**
 * gpm_battery_new:
 * Return value: new GpmBattery instance.
 **/
GpmBattery *
gpm_battery_new (void)
{
	if (gpm_battery_object != NULL) {
		g_object_ref (gpm_battery_object);
	} else {
		gpm_battery_object = g_object_new (GPM_TYPE_BATTERY, NULL);
		g_object_add_weak_pointer (gpm_battery_object, &gpm_battery_object);
	}
	return GPM_BATTERY (gpm_battery_object);
}

