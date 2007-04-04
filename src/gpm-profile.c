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
#include <glib/gstdio.h>

#include <libgpm.h>

#include "gpm-ac-adapter.h"
#include "gpm-array.h"
#include "gpm-dpms.h"
#include "gpm-common.h"
#include "gpm-load.h"
#include "gpm-debug.h"

#include "gpm-profile.h"

static void     gpm_profile_class_init (GpmProfileClass *klass);
static void     gpm_profile_init       (GpmProfile      *profile);
static void     gpm_profile_finalize   (GObject	       *object);

#define GPM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PROFILE, GpmProfilePrivate))

/* assume an average of 2 hour battery life */
#define GPM_PROFILE_SECONDS_PER_PERCENT		72

/* nicely smoothed, but still pretty fast */
#define GPM_PROFILE_SMOOTH_VIEW_SLEW		10
#define GPM_PROFILE_SMOOTH_SAVE_PERCENT		80

struct GpmProfilePrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmLoad			*load;
	GpmDpms			*dpms;
	GTimer			*timer;
	GpmArray		*array_data_charge;
	GpmArray		*array_data_discharge;
	GpmArray		*array_accuracy;
	GpmArray		*array_battery;
	gboolean		 discharging;
	gboolean		 lcd_on;
	gboolean		 data_valid;
	guint			 last_percentage;
	gchar			*config_id;
};

enum {
	LAST_SIGNAL
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
 * gpm_profile_get_data_file:
 *
 * Gets the time remaining for the current percentage
 *
 * @profile: This class
 */
static gchar *
gpm_profile_get_data_file (GpmProfile *profile, gboolean discharge)
{
	const gchar *suffix;

	/* check we have a profile loaded */
	if (profile->priv->config_id == NULL) {
		gpm_warning ("no config id set!");
		return NULL;
	}

	/* use home directory */
	if (discharge == TRUE) {
		suffix = "discharging.csv";
	} else {
		suffix = "charging.csv";
	}

	return g_strdup_printf ("%s/.gnome2/gnome-power-manager/profile-%s-%s",
				g_get_home_dir (), profile->priv->config_id, suffix);
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
		return value;
	}

	/* divide by number elements */
	average = (guint) ((gdouble) total / (gdouble) non_zero);
	return average;
}

/**
 * gpm_profile_compute_data_battery:
 *
 * @profile: This class
 */
static void
gpm_profile_compute_data_battery (GpmProfile *profile, gboolean discharge)
{
	guint i;
	guint average;
	GpmArray *array;
	GpmArrayPoint *point;

	/* get the correct data */
	if (discharge == TRUE) {
		array = profile->priv->array_data_discharge;
	} else {
		array = profile->priv->array_data_charge;
	}

	/* get the average not including the default */
	average = gpm_profile_array_get_nonzero_average (array, GPM_PROFILE_SECONDS_PER_PERCENT);

	/* copy the y data field into the y battery field */
	for (i=0; i<100; i++) {
		point = gpm_array_get (array, i);
		/* only set points that are not zero */
		if (point->data > 0) {
			gpm_array_set (profile->priv->array_battery, i, point->x, point->y, GPM_COLOUR_BLUE);
		} else {
			/* set zero points a different colour, and use the average */
			gpm_array_set (profile->priv->array_battery, i, point->x, average, GPM_COLOUR_DARK_BLUE);
		}
	}

	/* smooth data using moving average algorithm */
	gpm_array_compute_uwe_self (profile->priv->array_battery, GPM_PROFILE_SMOOTH_VIEW_SLEW);
}

/**
 * gpm_profile_compute_data_accuracy:
 *
 * @profile: This class
 */
static void
gpm_profile_compute_data_accuracy (GpmProfile *profile, gboolean discharge)
{
	guint i;
	GpmArray *array;
	GpmArrayPoint *point;

	/* get the correct data */
	if (discharge == TRUE) {
		array = profile->priv->array_data_discharge;
	} else {
		array = profile->priv->array_data_charge;
	}

	/* copy the data field into the accuracy y field */
	for (i=0; i<100; i++) {
		point = gpm_array_get (array, i);
		/* only set points that are not zero */
		if (point->data > 0) {
			gpm_array_set (profile->priv->array_accuracy, i, point->x, point->data, GPM_COLOUR_RED);
		} else {
			/* set zero points a different colour */
			gpm_array_set (profile->priv->array_accuracy, i, point->x, point->data, GPM_COLOUR_DARK_RED);
		}
	}

	/* smooth data using moving average algorithm */
	gpm_array_compute_uwe_self (profile->priv->array_accuracy, GPM_PROFILE_SMOOTH_VIEW_SLEW);
}

/**
 * gpm_profile_get_data_time_percent:
 *
 * @profile: This class
 */
GpmArray *
gpm_profile_get_data_time_percent (GpmProfile *profile, gboolean discharge)
{
	g_return_val_if_fail (profile != NULL, NULL);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), NULL);

	/* recompute */
	gpm_profile_compute_data_battery (profile, discharge);

	return profile->priv->array_battery;
}

/**
 * gpm_profile_get_data_accuracy_percent:
 *
 * @profile: This class
 */
GpmArray *
gpm_profile_get_data_accuracy_percent (GpmProfile *profile, gboolean discharge)
{
	g_return_val_if_fail (profile != NULL, NULL);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), NULL);

	/* recompute */
	gpm_profile_compute_data_accuracy (profile, discharge);

	return profile->priv->array_accuracy;
}

/**
 * gpm_profile_get_time:
 *
 * Gets the time remaining for the current percentage
 *
 * @profile: This class
 */
guint
gpm_profile_get_time (GpmProfile *profile, guint percentage, gboolean discharge)
{
	guint time;

	g_return_val_if_fail (profile != NULL, 0);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), 0);

	/* check we have a profile loaded */
	if (profile->priv->config_id == NULL) {
		gpm_warning ("no config id set!");
		return 0;
	}

	/* check we can give a decent reading */
	if (percentage > 99) {
		gpm_debug ("percentage = %i, correcting to 99%", percentage);
		percentage = 99;
	}

	/* recompute */
	gpm_profile_compute_data_battery (profile, discharge);

	/* compute correct area of integral */
	if (discharge == TRUE) {
		time = gpm_array_compute_integral (profile->priv->array_battery, 0, percentage);
	} else {
		time = gpm_array_compute_integral (profile->priv->array_battery, percentage, 99);
	}

	return time;
}


/**
 * gpm_profile_save_percentage:
 *
 * @profile: This class
 * @percentage: new percentage value
 */
static void
gpm_profile_save_percentage (GpmProfile *profile, guint percentage, guint data, guint accuracy)
{
	GpmArrayPoint *point;
	GpmArray *array;
	gchar *filename;

	/* get the correct data */
	if (profile->priv->discharging == TRUE) {
		array = profile->priv->array_data_discharge;
	} else {
		array = profile->priv->array_data_charge;
	}
	point = gpm_array_get (array, percentage);

	/* if we have no data, then just use the new value */
	if (point->y == 0) {
		point->y = data;
	} else {
		/* average the data so we converge to a common point */
		point->y = gpm_exponential_average (point->y, data, GPM_PROFILE_SMOOTH_SAVE_PERCENT);
	}

	/* save new accuracy (max gain is 20%, but less if the load was higher) */
	point->data += accuracy / 5;
	if (point->data > 100) {
		point->data = 100;
	}

	/* save data file when idle */
	filename = gpm_profile_get_data_file (profile, profile->priv->discharging);
	gpm_array_save_to_file (array, filename);
	g_free (filename);
}

/**
 * gpm_profile_register_percentage:
 *
 * @profile: This class
 * @percentage: new percentage value
 */
gboolean
gpm_profile_register_percentage (GpmProfile *profile,
				 guint	     percentage)
{
	gdouble elapsed;
	gdouble load;
	guint accuracy;
	guint array_percentage;

	g_return_val_if_fail (profile != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), FALSE);

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
		return FALSE;
	}

	if (accuracy < 20) {
		gpm_debug ("not accurate enough");
		return FALSE;
	}

	if (profile->priv->lcd_on == FALSE) {
		gpm_debug ("screen blanked, so not representative - ignoring");
		return FALSE;
	}

	/* If we are discharging, 99-0 is valid, but when we are charging,
	 * 1-100 is valid. Be careful how we index the arrays in this case */
	if (profile->priv->discharging == TRUE) {
		array_percentage = percentage;
	} else {
		if (percentage == 0) {
			gpm_debug ("ignoring percentage zero when charging");
			return FALSE;
		}
		array_percentage = percentage - 1;
	}

	/* save the last valid percent so we can cope with batteries that
	   stop charging at < 100% */
	profile->priv->last_percentage = array_percentage;

	/* save new data as we passed all tests */
	gpm_profile_save_percentage (profile, array_percentage, (guint) elapsed, accuracy);
	return TRUE;
}

/**
 * gpm_profile_register_charging:
 */
gboolean
gpm_profile_register_charging (GpmProfile *profile,
			       gboolean    is_charging)
{
	guint i;

	g_return_val_if_fail (profile != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), FALSE);

	if (is_charging == TRUE) {
		/* uninteresting case */
		return FALSE;
	}
	if (profile->priv->discharging == TRUE) {
		/* normal case, the ac_adapter has been removed half way
		   through charging, and we really don't care */
		return FALSE;
	}
	/* for batteries that stop charging before they
	   get to 100% we have to set the last charging
	   values to zero for the correct rates. */
	if (profile->priv->last_percentage != 100) {
		for (i=profile->priv->last_percentage; i<100; i++) {
			/* set percentage i to zero with accuracy 100 */
			gpm_profile_save_percentage (profile, i, 0, 100);
			gpm_debug ("set percentage %i to zero", i);
		}
	}
	return TRUE;
}

/**
 * ac_adaptor_changed_cb:
 * @on_ac: If we are on AC power
 *
 **/
static void
ac_adaptor_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean      on_ac,
		       GpmProfile   *profile)
{
	/* we might be halfway through a percentage change */
	profile->priv->data_valid = FALSE;

	/* check AC state */
	if (on_ac == TRUE) {
		gpm_debug ("on AC");
		profile->priv->discharging = FALSE;
	} else {
		gpm_debug ("on battery");
		profile->priv->discharging = TRUE;
	}

	/* reset timer as we have changed state */
	g_timer_start (profile->priv->timer);
}

/**
 * dpms_mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @info: This class instance
 *
 * Log when the DPMS mode is changed.
 **/
static void
dpms_mode_changed_cb (GpmDpms    *dpms,
		      GpmDpmsMode mode,
		      GpmProfile *profile)
{
	gpm_debug ("DPMS mode changed: %d", mode);

	/* We have to monitor the screen as it's one of the biggest energy
	 * users. If this goes off, then out battery usage is non-proportional
	 * to actual discharge rates when in use, and the data is bad. */
	if (mode == GPM_DPMS_MODE_ON) {
		profile->priv->lcd_on = TRUE;
	} else {
		/* any other powersaving mode is not typical */
		profile->priv->lcd_on = FALSE;
	}
}

/**
 * gpm_profile_delete_data:
 */
gboolean
gpm_profile_delete_data (GpmProfile *profile, gboolean discharge)
{
	gchar *filename;
	gint ret;

	g_return_val_if_fail (profile != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), FALSE);

	/* check we have a profile loaded */
	if (profile->priv->config_id == NULL) {
		gpm_warning ("no config id set!");
		return FALSE;
	}

	filename = gpm_profile_get_data_file (profile, discharge);
	ret = g_unlink (filename);
	if (ret != 0) {
		gpm_warning ("could not delete '%s'", filename);
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_profile_load_data:
 */
static void
gpm_profile_load_data (GpmProfile *profile, gboolean discharge)
{
	gboolean ret;
	guint i;
	gchar *filename;
	gchar *path;
	GpmArray *array;

	/* get the correct data */
	if (discharge == TRUE) {
		array = profile->priv->array_data_discharge;
	} else {
		array = profile->priv->array_data_charge;
	}

	/* read in data profile from disk */
	filename = gpm_profile_get_data_file (profile, discharge);
	gpm_debug ("loading battery data from '%s'", filename);
	ret = gpm_array_load_from_file (array, filename);

	/* if not found, then generate a new one with a low propability */
	if (ret == FALSE) {

		/* directory might not exist */
		path = g_build_filename (g_get_home_dir (), ".gnome2", "gnome-power-manager", NULL);
		g_mkdir_with_parents (path, 744);
		g_free (path);

		gpm_debug ("no data found, generating initial (poor) data");
		for (i=0;i<100;i++) {
			/* assume average battery lasts 2 hours, but we are 0% accurate */
			gpm_array_set (array, i, i, GPM_PROFILE_SECONDS_PER_PERCENT, 0);
		}

		ret = gpm_array_save_to_file (array, filename);
		if (ret == FALSE) {
			gpm_warning ("saving state failed. You will not get accurate time remaining calculations");
		}
	}
	g_free (filename);

#if 0
	/* do debugging self tests */
	guint time;
	gpm_debug ("Reference times");
	if (discharge == TRUE) {
		time = gpm_profile_get_time (profile, 99, TRUE);
		gpm_debug ("99-0\t%i minutes", time / 60);
		time = gpm_profile_get_time (profile, 50, TRUE);
		gpm_debug ("50-0\t%i minutes", time / 60);
	} else {
		time = gpm_profile_get_time (profile, 0, FALSE);
		gpm_debug ("0-99\t%i minutes", time / 60);
		time = gpm_profile_get_time (profile, 50, FALSE);
		gpm_debug ("50-99\t%i minutes", time / 60);
	}
#endif
}

/**
 * gpm_profile_set_config_id:
 *
 * @profile: This class
 * @config_id: String to represent the state of the system, used to switch
 *             multiple profiles for multibattery laptops.
 */
gboolean
gpm_profile_set_config_id (GpmProfile  *profile,
			   const gchar *config_id)
{
	gboolean reload = FALSE;
	g_return_val_if_fail (profile != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (config_id != NULL, FALSE);

	gpm_debug ("config_id = %s", config_id);
	if (profile->priv->config_id == NULL) {
		/* if new */
		profile->priv->config_id = g_strdup (config_id);
		reload = TRUE;
	} else if (strcmp (config_id, profile->priv->config_id) != 0) {
		/* if different */
		g_free (profile->priv->config_id);
		profile->priv->config_id = g_strdup (config_id);
		reload = TRUE;
	}
	if (reload == TRUE) {
		/* get initial data */
		gpm_profile_load_data (profile, TRUE);
		gpm_profile_load_data (profile, FALSE);
	}
	return TRUE;
}

/**
 * gpm_profile_get_accuracy:
 */
guint
gpm_profile_get_accuracy (GpmProfile *profile,
			  guint	      percentage)
{
	GpmArrayPoint *point;

	g_return_val_if_fail (profile != NULL, 0);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), 0);

	/* check we have a profile loaded */
	if (profile->priv->config_id == NULL) {
		gpm_warning ("no config id set!");
		return 0;
	}

	if (percentage > 99) {
		percentage = 99;
		gpm_debug ("corrected percentage...");
	}

	/* recompute */
	gpm_profile_compute_data_accuracy (profile, profile->priv->discharging);

	point = gpm_array_get (profile->priv->array_accuracy, percentage);

	return point->y;
}

/**
 * gpm_profile_init:
 */
static void
gpm_profile_init (GpmProfile *profile)
{
	gboolean on_ac;

	profile->priv = GPM_PROFILE_GET_PRIVATE (profile);

	profile->priv->timer = g_timer_new ();
	profile->priv->load = gpm_load_new ();
	profile->priv->config_id = NULL;

	profile->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (profile->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adaptor_changed_cb), profile);

	profile->priv->dpms = gpm_dpms_new ();
	g_signal_connect (profile->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), profile);


	/* used for data */
	profile->priv->array_data_charge = gpm_array_new ();
	profile->priv->array_data_discharge = gpm_array_new ();
	gpm_array_set_fixed_size (profile->priv->array_data_charge, 100);
	gpm_array_set_fixed_size (profile->priv->array_data_discharge, 100);

	/* used for presentation, hence we can share */
	profile->priv->array_accuracy = gpm_array_new ();
	profile->priv->array_battery = gpm_array_new ();
	gpm_array_set_fixed_size (profile->priv->array_battery, 100);
	gpm_array_set_fixed_size (profile->priv->array_accuracy, 100);

	/* default */
	profile->priv->lcd_on = TRUE;
	profile->priv->last_percentage = 100;

	/* we might be halfway through a percentage change */
	profile->priv->data_valid = FALSE;

	/* coldplug the AC state */
	on_ac = gpm_ac_adapter_is_present (profile->priv->ac_adapter);

	/* check AC state */
	if (on_ac == TRUE) {
		gpm_debug ("on AC");
		profile->priv->discharging = FALSE;
	} else {
		gpm_debug ("on battery");
		profile->priv->discharging = TRUE;
	}
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

	if (profile->priv->config_id != NULL) {
		g_free (profile->priv->config_id);
	}
	g_object_unref (profile->priv->load);
	g_object_unref (profile->priv->dpms);
	g_object_unref (profile->priv->ac_adapter);

	g_object_unref (profile->priv->array_accuracy);
	g_object_unref (profile->priv->array_battery);
	g_object_unref (profile->priv->array_data_charge);
	g_object_unref (profile->priv->array_data_discharge);
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

