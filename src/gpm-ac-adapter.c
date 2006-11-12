/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-hal.h"
#include "gpm-ac-adapter.h"

#define GPM_AC_ADAPTER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_AC_ADAPTER, GpmAcAdapterPrivate))

struct GpmAcAdapterPrivate
{
	gchar			*udi;
	gboolean		 has_hardware;
	GpmHal			*hal;
};

enum {
	AC_ADAPTER_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmAcAdapter, gpm_ac_adapter, G_TYPE_OBJECT)

/**
 * gpm_ac_adapter_get:
 * @ac_adapter: This class instance
 *
 * Gets the current state of the AC adapter
 **/
gboolean
gpm_ac_adapter_get_state (GpmAcAdapter *ac_adapter,
		          GpmAcAdapterState *state)
{
	gboolean is_on_ac;

	g_return_val_if_fail (ac_adapter != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_AC_ADAPTER (ac_adapter), FALSE);

	/* bodge for now, PC's are considered on AC power */
	if (ac_adapter->priv->has_hardware == FALSE) {
		*state = GPM_AC_ADAPTER_PRESENT;
		return TRUE;
	}

	gpm_hal_device_get_bool (ac_adapter->priv->hal, ac_adapter->priv->udi,
				 "ac_adapter.present", &is_on_ac);
	if (is_on_ac == TRUE) {
		*state = GPM_AC_ADAPTER_PRESENT;
	} else {
		*state = GPM_AC_ADAPTER_MISSING;
	}
	return TRUE;
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
hal_device_property_modified_cb (GpmHal       *hal,
				 const gchar  *udi,
				 const gchar  *key,
				 gboolean      is_added,
				 gboolean      is_removed,
				 gboolean      finally,
				 GpmAcAdapter *ac_adapter)
{
	GpmAcAdapterState state;

	if (strcmp (key, "ac_adapter.present") == 0) {
		gpm_ac_adapter_get_state (ac_adapter, &state);
		g_signal_emit (ac_adapter, signals [AC_ADAPTER_CHANGED], 0, state);
		return;
	}
}

/**
 * gpm_ac_adapter_constructor:
 **/
static GObject *
gpm_ac_adapter_constructor (GType		  type,
			      guint		  n_construct_properties,
			      GObjectConstructParam *construct_properties)
{
	GpmAcAdapter      *ac_adapter;
	GpmAcAdapterClass *klass;
	klass = GPM_AC_ADAPTER_CLASS (g_type_class_peek (GPM_TYPE_AC_ADAPTER));
	ac_adapter = GPM_AC_ADAPTER (G_OBJECT_CLASS (gpm_ac_adapter_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (ac_adapter);
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

	g_free (ac_adapter->priv->udi);
	g_object_unref (ac_adapter->priv->hal);

	g_return_if_fail (ac_adapter->priv != NULL);
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
	object_class->constructor  = gpm_ac_adapter_constructor;

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
	gchar **names;

	ac_adapter->priv = GPM_AC_ADAPTER_GET_PRIVATE (ac_adapter);
	ac_adapter->priv->hal = gpm_hal_new ();

	/* save udi of lcd adapter */
	gpm_hal_device_find_capability (ac_adapter->priv->hal, "ac_adapter", &names);
	if (names == NULL || names[0] == NULL) {
		/* this shouldn't happen */
		ac_adapter->priv->has_hardware = FALSE;
		gpm_warning ("No devices of capability ac_adapter");
		return;
	}

	/* we track this by hand as machines that have no ac_adapter object must
	 * return that they are on ac power */
	ac_adapter->priv->has_hardware = TRUE;

	/* We only want first ac_adapter object (should only be one) */
	ac_adapter->priv->udi = g_strdup (names[0]);
	gpm_hal_free_capability (ac_adapter->priv->hal, names);

	/* watch this device */
	gpm_hal_device_watch_propery_modified (ac_adapter->priv->hal, ac_adapter->priv->udi, FALSE);

	/* we want state changes */
	g_signal_connect (ac_adapter->priv->hal, "property-modified",
			  G_CALLBACK (hal_device_property_modified_cb), ac_adapter);
}

/**
 * gpm_ac_adapter_new:
 * Return value: A new ac_adapter class instance.
 **/
GpmAcAdapter *
gpm_ac_adapter_new (void)
{
	static GpmAcAdapter *ac_adapter = NULL;

	if (ac_adapter != NULL) {
		g_object_ref (ac_adapter);
		return ac_adapter;
	}

	ac_adapter = g_object_new (GPM_TYPE_AC_ADAPTER, NULL);
	return ac_adapter;
}
