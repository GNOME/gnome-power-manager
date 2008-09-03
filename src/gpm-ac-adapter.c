/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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
#include <dbus/dbus-glib.h>

#include <libhal-gdevice.h>
#include <libhal-gmanager.h>

#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-ac-adapter.h"

#define GPM_AC_ADAPTER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_AC_ADAPTER, GpmAcAdapterPrivate))

struct GpmAcAdapterPrivate
{
	gboolean		 has_hardware;
	HalGDevice		*hal_device;
};

enum {
	AC_ADAPTER_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_ac_adapter_object = NULL;

G_DEFINE_TYPE (GpmAcAdapter, gpm_ac_adapter, G_TYPE_OBJECT)

/**
 * gpm_ac_adapter_get:
 * @ac_adapter: This class instance
 *
 * Gets the current state of the AC adapter
 **/
gboolean
gpm_ac_adapter_is_present (GpmAcAdapter *ac_adapter)
{
	gboolean is_on_ac;
	GError *error;

	g_return_val_if_fail (ac_adapter != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_AC_ADAPTER (ac_adapter), FALSE);

	/* bodge for now, PC's are considered on AC power */
	if (ac_adapter->priv->has_hardware == FALSE) {
		return TRUE;
	}

	error = NULL;
	hal_gdevice_get_bool (ac_adapter->priv->hal_device,
			      "ac_adapter.present", &is_on_ac, &error);
	if (error != NULL) {
		egg_warning ("could not read ac_adapter.present");
		g_error_free (error);
	}

	if (is_on_ac) {
		return TRUE;
	}
	return FALSE;
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
hal_device_property_modified_cb (HalGDevice   *hal_device,
				 const gchar  *key,
				 gboolean      is_added,
				 gboolean      is_removed,
				 gboolean      finally,
				 GpmAcAdapter *ac_adapter)
{
	gboolean on_ac;
	if (strcmp (key, "ac_adapter.present") == 0) {
		on_ac = gpm_ac_adapter_is_present (ac_adapter);
		g_signal_emit (ac_adapter, signals [AC_ADAPTER_CHANGED], 0, on_ac);
		return;
	}
}

/**
 * gpm_ac_adapter_finalize:
 **/
static void
gpm_ac_adapter_finalize (GObject *object)
{
	GpmAcAdapter *ac_adapter;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_AC_ADAPTER (object));
	ac_adapter = GPM_AC_ADAPTER (object);
	g_return_if_fail (ac_adapter->priv != NULL);

	g_object_unref (ac_adapter->priv->hal_device);

	G_OBJECT_CLASS (gpm_ac_adapter_parent_class)->finalize (object);
}

/**
 * gpm_ac_adapter_class_init:
 **/
static void
gpm_ac_adapter_class_init (GpmAcAdapterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_ac_adapter_finalize;

	signals [AC_ADAPTER_CHANGED] =
		g_signal_new ("ac-adapter-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmAcAdapterClass, ac_adapter_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (GpmAcAdapterPrivate));
}

/**
 * gpm_ac_adapter_init:
 * @ac_adapter: This class instance
 *
 * initialises the ac_adapter class. NOTE: We expect ac_adapter objects
 * to *NOT* be removed or added during the session.
 * We only control the first ac_adapter object if there are more than one.
 **/
static void
gpm_ac_adapter_init (GpmAcAdapter *ac_adapter)
{
	gchar **device_names;
	gboolean ret;
	GError *error;
	HalGManager *hal_manager;

	ac_adapter->priv = GPM_AC_ADAPTER_GET_PRIVATE (ac_adapter);
	ac_adapter->priv->hal_device = hal_gdevice_new ();
	hal_manager = hal_gmanager_new ();

	/* save udi of lcd adapter */
	error = NULL;
	ret = hal_gmanager_find_capability (hal_manager, "ac_adapter", &device_names, &error);
	if (!ret) {
		egg_warning ("Couldn't obtain list of AC adapters: %s", error->message);
		g_error_free (error);
		return;
	}
	if (device_names[0] != NULL) {
		/* we track this by hand as machines that have no ac_adapter object must
		 * return that they are on ac power */
		ac_adapter->priv->has_hardware = TRUE;
		egg_debug ("using %s", device_names[0]);

		/* We only want first ac_adapter object (should only be one) */
		hal_gdevice_set_udi (ac_adapter->priv->hal_device, device_names[0]);
		hal_gdevice_watch_property_modified (ac_adapter->priv->hal_device);

		/* we want state changes */
		g_signal_connect (ac_adapter->priv->hal_device, "property-modified",
				  G_CALLBACK (hal_device_property_modified_cb), ac_adapter);
	} else {
		/* no ac-adapter class support */
		ac_adapter->priv->has_hardware = FALSE;
		egg_debug ("No devices of capability ac_adapter");
	}
	hal_gmanager_free_capability (device_names);
	g_object_unref (hal_manager);
}

/**
 * gpm_ac_adapter_new:
 * Return value: A new ac_adapter class instance.
 **/
GpmAcAdapter *
gpm_ac_adapter_new (void)
{
	if (gpm_ac_adapter_object != NULL) {
		g_object_ref (gpm_ac_adapter_object);
	} else {
		gpm_ac_adapter_object = g_object_new (GPM_TYPE_AC_ADAPTER, NULL);
		g_object_add_weak_pointer (gpm_ac_adapter_object, &gpm_ac_adapter_object);
	}
	return GPM_AC_ADAPTER (gpm_ac_adapter_object);
}

