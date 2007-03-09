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

/* assume an average of 2 hour battery life */
#define SECONDS_PER_PERCENT 72

struct GpmProfilePrivate
{
	HalGDevice		*hal_device;
	GpmAcAdapter		*ac_adapter;
	GpmLoad			*load;
	GTimer			*timer;
	GpmArray		*array_data;
	GpmArray		*array_accuracy;
	GpmArray		*array_battery;
	gboolean		 is_discharging;
	gboolean		 data_valid;
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
 * gpm_profile_provide_data:
 *
 * Provide the profiler (this class!) with data.
 * WILL ONLY BE USED WHEN HOOKED UP TO GPM-POWER
 *
 * @profile: This class
 */
gboolean
gpm_profile_provide_data (GpmProfile *profile, guint percentage)
{
	g_return_val_if_fail (profile != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), FALSE);
	return TRUE;
}

/**
 * gpm_profile_get_data_file:
 *
 * Gets the time remaining for the current percentage
 *
 * @profile: This class
 */
static gchar *
gpm_profile_get_data_file (GpmProfile *profile, const gchar *prefix)
{
	const gchar *suffix;

	/* use home directory */
	if (profile->priv->is_discharging == TRUE) {
		suffix = "discharging.csv";
	} else {
		suffix = "charging.csv";
	}

	return g_strdup_printf ("%s/.gnome2/gnome-power-manager/%s-%s", g_get_home_dir (), prefix, suffix);
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
 * gpm_profile_array_get_nonzero_average:
 * @array: This class instance
 *
 * Gets the average y value, but only counting the elements not equal to a constant.
 **/
guint
gpm_profile_array_get_nonzero_average (GpmArray *array, guint value)
{
	GpmArrayPoint *point;
	guint i;
	guint length;
	guint total;
	guint average;
	guint non_zero;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* sum all the y values that are not zero */
	total = 0;
	non_zero = 0;
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		if (point->y != value) {
			non_zero++;
			total += point->y;
		}
	}

	/* empty array */
	if (non_zero == 0) {
		return 0;
	}

	/* divide by number elements */
	average = (guint) ((gdouble) total / (gdouble) non_zero);
	return average;
}

/**
 * gpm_profile_get_data_time_percent:
 *
 * @profile: This class
 */
GpmArray *
gpm_profile_get_data_time_percent (GpmProfile *profile)
{
	guint i;
	guint average;
	GpmArrayPoint *point;

	g_return_val_if_fail (profile != NULL, NULL);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), NULL);

	/* get the average not including the default */
	average = gpm_profile_array_get_nonzero_average (profile->priv->array_data, SECONDS_PER_PERCENT);

	/* copy the y data field into the y battery field */
	for (i=0; i<100; i++) {
		point = gpm_array_get (profile->priv->array_data, i);
		/* only set points that are not zero */
		if (point->data > 0) {
			gpm_array_set (profile->priv->array_battery, i, point->x, point->y, 4);
		} else {
			/* set zero points a different colour, and use the average */
			gpm_array_set (profile->priv->array_battery, i, point->x, average, 11);
		}
	}

	return profile->priv->array_battery;
}

/**
 * gpm_profile_get_data_accuracy_percent:
 *
 * @profile: This class
 */
GpmArray *
gpm_profile_get_data_accuracy_percent (GpmProfile *profile)
{
	guint i;
	GpmArrayPoint *point;

	g_return_val_if_fail (profile != NULL, NULL);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), NULL);

	/* copy the data field into the accuracy y field */
	for (i=0; i<100; i++) {
		point = gpm_array_get (profile->priv->array_data, i);
		/* only set points that are not zero */
		if (point->data > 0) {
			gpm_array_set (profile->priv->array_accuracy, i, point->x, point->data, 3);
		} else {
			/* set zero points a different colour */
			gpm_array_set (profile->priv->array_accuracy, i, point->x, point->data, 10);
		}
	}

	return profile->priv->array_accuracy;
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
	profile->priv->is_discharging = is_discharging;
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
	guint accuracy;
	GpmAcAdapterState state;
	gchar *filename;

	gpm_ac_adapter_get_state (profile->priv->ac_adapter, &state);

/*
	if (state != GPM_AC_ADAPTER_PRESENT && data_valid == FALSE) {
		gpm_debug ("ignoring as we are monitoring charging");
		return;
	}
	if (state == GPM_AC_ADAPTER_PRESENT && data_valid == TRUE) {
		gpm_debug ("ignoring as we are monitoring discharging");
		return;
	}
*/
	/* turn the load into a nice scaled percentage */
	load = gpm_load_get_current (profile->priv->load);
	if (load > 0.01) {
		accuracy = (guint) (100.0f / load);
		if (accuracy > 100) {
			accuracy = 100;
		}
	} else {
		accuracy = 100;
	}

	elapsed = g_timer_elapsed (profile->priv->timer, NULL);

	/* reset timer for next time */
	g_timer_start (profile->priv->timer);

	gpm_debug ("elapsed is %f for %i at load %f (accuracy:%i)", elapsed, percentage, load, accuracy);

	/* don't process the first point, we maybe in between percentage points */
	if (profile->priv->data_valid == FALSE) {
		gpm_debug ("data is not valid, will process next");
		profile->priv->data_valid = TRUE;
		return;
	}

	if (accuracy < 10) {
		gpm_debug ("not accurate enough");
		return;
	}

//	if (dpms_state == OFF) {
//		gpm_debug ("screen blanked, so not representative");
//		return;
//	}

	/* need to do averaging */
	gpm_array_set (profile->priv->array_data, percentage, percentage, (guint) elapsed, accuracy);

	/* recompute the discharge graph */
	
	/* save data file when idle */
	filename = gpm_profile_get_data_file (profile, "profile-battery");
	gpm_array_save_to_file (profile->priv->array_data, filename);
	g_free (filename);
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
 * ac_adaptor_changed_cb:
 * @on_ac: If we are on AC power
 *
 **/
static void
ac_adaptor_changed_cb (GpmAcAdapter *ac_adapter,
		       GpmAcAdapterState state,
		       GpmProfile *profile)
{
	/* we might be halfway through a percentage change */
	profile->priv->data_valid = FALSE;

	/* reset timer as we have changed state */
	g_timer_start (profile->priv->timer);
}

/**
 * gpm_profile_init:
 */
static void
gpm_profile_init (GpmProfile *profile)
{
	gboolean ret;
	guint i;
	gchar *filename;
	gchar *path;

	profile->priv = GPM_PROFILE_GET_PRIVATE (profile);

	profile->priv->timer = g_timer_new ();
	profile->priv->load = gpm_load_new ();
	profile->priv->ac_adapter = gpm_ac_adapter_new ();
	profile->priv->array_data = gpm_array_new ();
	profile->priv->array_accuracy = gpm_array_new ();
	profile->priv->array_battery = gpm_array_new ();
	gpm_array_set_fixed_size (profile->priv->array_data, 100);
	gpm_array_set_fixed_size (profile->priv->array_accuracy, 100);
	gpm_array_set_fixed_size (profile->priv->array_battery, 100);

	/* default */
	profile->priv->is_discharging = TRUE;

	/* read in data profile from disk */
	filename = gpm_profile_get_data_file (profile, "profile-battery");
	gpm_debug ("loading battery data from '%s'", filename);
	ret = gpm_array_load_from_file (profile->priv->array_data, filename);

	/* if not found, then generate a new one with a low propability */
	if (ret == FALSE) {
		/* directory might not exist */
		path = g_build_filename (g_get_home_dir (), ".gnome2", "gnome-power-manager", NULL);
		g_mkdir_with_parents (path, 744);
		g_free (path);

		gpm_debug ("no data found, generating initial (poor) data");
		for (i=0;i<100;i++) {
			/* assume average battery lasts 2 hours, but we are 0% accurate */
			gpm_array_set (profile->priv->array_data, i, i, SECONDS_PER_PERCENT, 0);
		}

		ret = gpm_array_save_to_file (profile->priv->array_data, filename);
		if (ret == FALSE) {
			gpm_warning ("saving state failed. You will not get accurate time remaining calculations");
		}
	}
	g_free (filename);

	/* we might be halfway through a percentage change */
	profile->priv->data_valid = FALSE;

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
	g_signal_connect (profile->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adaptor_changed_cb), profile);

	g_object_unref (profile->priv->array_accuracy);
	g_object_unref (profile->priv->array_battery);
	g_object_unref (profile->priv->array_data);
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

