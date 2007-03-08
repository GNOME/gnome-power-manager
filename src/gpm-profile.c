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

#include <libhal-gdevice.h>
#include <libhal-gmanager.h>

#include "gpm-ac-adapter.h"
#include "gpm-array.h"
#include "gpm-common.h"
#include "gpm-load.h"
#include "gpm-debug.h"

#include "gpm-profile.h"

static void     gpm_profile_class_init (GpmProfileClass *klass);
static void     gpm_profile_init       (GpmProfile      *profile);
static void     gpm_profile_finalize   (GObject	       *object);

#define GPM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PROFILE, GpmProfilePrivate))

struct GpmProfilePrivate
{
	HalGDevice		*hal_device;
	GpmAcAdapter		*ac_adapter;
	GpmLoad			*load;
	GTimer			*timer;
	GpmArray		*array;
	gboolean		 ac_mode;
};

static gpointer gpm_profile_object = NULL;

G_DEFINE_TYPE (GpmProfile, gpm_profile, G_TYPE_OBJECT)

/**
 * gpm_profile_class_init:
 * @klass: This class instance
 **/
static void
gpm_profile_class_init (GpmProfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_profile_finalize;

	g_type_class_add_private (klass, sizeof (GpmProfilePrivate));
}

/**
 * gpm_profile_get_time:
 *
 * Gets the time remaining for the current percentage
 *
 * @profile: This class
 */
guint
gpm_profile_get_time (GpmProfile *profile, guint percentage)
{
	g_return_val_if_fail (profile != NULL, 0);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), 0);
	return 123;
}

/**
 * gpm_profile_activate_mode:
 *
 * @profile: This class
 */
GpmArray *
gpm_profile_get_data_time_percent (GpmProfile *profile)
{
	g_return_val_if_fail (profile != NULL, NULL);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), NULL);
	return profile->priv->array;
}

/**
 * gpm_profile_activate_mode:
 *
 * @profile: This class
 */
void
gpm_profile_activate_mode (GpmProfile *profile, gboolean is_discharging)
{
	g_return_if_fail (profile != NULL);
	g_return_if_fail (GPM_IS_PROFILE (profile));
	profile->priv->ac_mode = is_discharging;
}

/**
 * gpm_profile_register_percentage:
 *
 * @profile: This class
 * @percentage: new percentage value
 */
static void
gpm_profile_register_percentage (GpmProfile *profile,
				 guint	     percentage)
{
	gdouble elapsed;
	gdouble load;
	GpmAcAdapterState state;

	gpm_ac_adapter_get_state (profile->priv->ac_adapter, &state);

//	if (state == GPM_AC_ADAPTER_PRESENT) {

	load = gpm_load_get_current (profile->priv->load);

	elapsed = g_timer_elapsed (profile->priv->timer, NULL);

	/* reset timer for next time */
	g_timer_start (profile->priv->timer);

	gpm_warning ("elapsed is %f for %i at load %f", elapsed, percentage, load);

	/* recompute the discharge graph */

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
				 GpmProfile   *profile)
{
	const gchar *udi = hal_gdevice_get_udi (device);
	guint percentage;
	gpm_debug ("udi=%s, key=%s, added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	/* do not process keys that have been removed */
	if (is_removed == TRUE) {
		return;
	}

	if (strcmp (key, "battery.charge_level.percentage") == 0) {
		hal_gdevice_get_uint (device, key, &percentage, NULL);
		gpm_profile_register_percentage (profile, percentage);
	}
}

/**
 * gpm_profile_init:
 */
static void
gpm_profile_init (GpmProfile *profile)
{
	gboolean ret;
	guint i;
	const gchar *filename;

	profile->priv = GPM_PROFILE_GET_PRIVATE (profile);

	profile->priv->timer = g_timer_new ();
	profile->priv->load = gpm_load_new ();
	profile->priv->ac_adapter = gpm_ac_adapter_new ();
	profile->priv->array = gpm_array_new ();
	gpm_array_set_fixed_size (profile->priv->array, 100);

	/* read in profile from disk */
	filename = "/home/hughsie/profile-data-01.csv";
	ret = gpm_array_load_from_file (profile->priv->array, filename);

	/* if not found, then generate a new one with a low propability */
	if (ret == FALSE) {
		gpm_debug ("no data found, generating intinial (poor) data");
		for (i=0;i<100;i++) {
			gpm_array_set (profile->priv->array, i, i, i, 0);
		}
		ret = gpm_array_save_to_file (profile->priv->array, filename);
		if (ret == FALSE) {
			gpm_warning ("saving state failed. You will not get accurate time remaining calculations");
		}
	}

	/* find, and add a single device */
	profile->priv->hal_device = hal_gdevice_new ();
	hal_gdevice_set_udi (profile->priv->hal_device, "/org/freedesktop/Hal/devices/acpi_BAT1");
	hal_gdevice_watch_property_modified (profile->priv->hal_device);
	g_signal_connect (profile->priv->hal_device, "property-modified",
			  G_CALLBACK (hal_device_property_modified_cb), profile);
}

/**
 * gpm_profile_coldplug:
 *
 * @object: This profile instance
 */
static void
gpm_profile_finalize (GObject *object)
{
	GpmProfile *profile;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_PROFILE (object));

	profile = GPM_PROFILE (object);

	g_return_if_fail (profile->priv != NULL);

	g_object_unref (profile->priv->hal_device);
	g_object_unref (profile->priv->load);
	g_object_unref (profile->priv->ac_adapter);
	g_object_unref (profile->priv->array);
	g_timer_destroy (profile->priv->timer);

	G_OBJECT_CLASS (gpm_profile_parent_class)->finalize (object);
}

/**
 * gpm_profile_new:
 * Return value: new GpmProfile instance.
 **/
GpmProfile *
gpm_profile_new (void)
{
	if (gpm_profile_object != NULL) {
		g_object_ref (gpm_profile_object);
	} else {
		gpm_profile_object = g_object_new (GPM_TYPE_PROFILE, NULL);
		g_object_add_weak_pointer (gpm_profile_object, &gpm_profile_object);
	}
	return GPM_PROFILE (gpm_profile_object);
}

