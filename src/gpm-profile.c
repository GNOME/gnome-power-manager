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
#include <libdbus-proxy.h>

#include "egg-color.h"
#include "gpm-common.h"
#include "gpm-ac-adapter.h"
#include "gpm-array.h"
#include "gpm-array-float.h"
#include "gpm-dpms.h"
#include "gpm-load.h"
#include "egg-debug.h"
#include "gpm-control.h"

#include "gpm-profile.h"

static void     gpm_profile_class_init (GpmProfileClass *klass);
static void     gpm_profile_init       (GpmProfile      *profile);
static void     gpm_profile_finalize   (GObject	       *object);

#define GPM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PROFILE, GpmProfilePrivate))

/* nicely smoothed, but still pretty fast */
#define GPM_PROFILE_SMOOTH_VIEW_SLEW		10
#define GPM_PROFILE_SMOOTH_SAVE_PERCENT		80

struct GpmProfilePrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmLoad			*load;
	GpmDpms			*dpms;
	GpmControl		*control;
	GTimer			*timer;
	GpmArray		*present_array_accuracy;
	GpmArray		*present_array_data;

	GArray			*float_data_charge;
	GArray			*float_data_discharge;
	GArray			*float_accuracy_charge;
	GArray			*float_accuracy_discharge;

	gboolean		 discharging;
	gboolean		 lcd_on;
	gboolean		 data_valid;
	gboolean		 use_guessing;
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
		egg_warning ("no config id set!");
		return NULL;
	}

	/* use home directory */
	if (discharge) {
		suffix = "discharging.csv";
	} else {
		suffix = "charging.csv";
	}

	return g_strdup_printf ("%s/.gnome2/gnome-power-manager/profile-%s-%s",
				g_get_home_dir (), profile->priv->config_id, suffix);
}

/**
 * gpm_profile_print:
 *
 * @profile: This class
 */
void
gpm_profile_print (GpmProfile *profile)
{
	guint i;
	GArray *array_data;
	GArray *array_accuracy;
	gfloat value_data;
	gfloat value_accuracy;

	g_return_if_fail (profile != NULL);
	g_return_if_fail (GPM_IS_PROFILE (profile));

	/* get the correct data */
	if (profile->priv->discharging) {
		array_data = profile->priv->float_data_discharge;
		array_accuracy = profile->priv->float_accuracy_discharge;
	} else {
		array_data = profile->priv->float_data_charge;
		array_accuracy = profile->priv->float_accuracy_charge;
	}

	/* copy the data field into the accuracy y field */
	for (i=0; i<100; i++) {
		value_data = gpm_array_float_get (array_data, i);
		value_accuracy = gpm_array_float_get (array_accuracy, i);
		g_print ("[%i], %f (%f)\n", i, value_data, value_accuracy);
	}
}

/**
 * gpm_profile_get_data_time_percent:
 *
 * @profile: This class
 */
GpmArray *
gpm_profile_get_data_time_percent (GpmProfile *profile, gboolean discharge)
{
	guint i;
	GArray *array_data;
	GArray *array_accuracy;
	gfloat value_data;
	gfloat value_accuracy;

	g_return_val_if_fail (profile != NULL, NULL);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), NULL);

	/* get the correct data */
	if (discharge) {
		array_data = profile->priv->float_data_discharge;
		array_accuracy = profile->priv->float_accuracy_discharge;
	} else {
		array_data = profile->priv->float_data_charge;
		array_accuracy = profile->priv->float_accuracy_charge;
	}

	/* copy the data field into the accuracy y field */
	for (i=0; i<100; i++) {
		value_data = gpm_array_float_get (array_data, i);
		value_accuracy = gpm_array_float_get (array_accuracy, i);
		/* only set points that are not zero */
		if (value_data == 0) {
			gpm_array_set (profile->priv->present_array_data, i, i, value_data, EGG_COLOR_DARK_GREY);
		} else if (value_accuracy == 0) {
			gpm_array_set (profile->priv->present_array_data, i, i, value_data, EGG_COLOR_BLUE);
		} else if (value_data > 0) {
			gpm_array_set (profile->priv->present_array_data, i, i, value_data, EGG_COLOR_RED);
		}
	}

	return profile->priv->present_array_data;
}

/**
 * gpm_profile_get_accuracy_average:
 *
 * @profile: This class
 *
 * Gets the average value of the accuracy, i.e. whether this discharge profile
 * is to be trusted. We have the situation where if a user repeatedly charges
 * from 30% to 100% and then from 100% to 30% then g-p-m assumes that <30%
 * is impossible and uses the 30% as the lowest point of the integral.
 * Working out the average gives us a good ballpart for the system.
 * Generally, a 40%+ average is good enough for a policy decision.
 * (use GPM_PROFILE_GOOD_TRUST for value)
 */
gfloat
gpm_profile_get_accuracy_average (GpmProfile *profile, gboolean discharge)
{
	GArray *array;

	/* get the correct data */
	if (discharge) {
		array = profile->priv->float_accuracy_discharge;
	} else {
		array = profile->priv->float_accuracy_charge;
	}

	return gpm_array_float_get_average (array);
}

/**
 * gpm_profile_get_data_accuracy_percent:
 *
 * @profile: This class
 */
GpmArray *
gpm_profile_get_data_accuracy_percent (GpmProfile *profile, gboolean discharge)
{
	guint i;
	GArray *array;
	gfloat value;
	guint32 colour_active;
	guint32 colour_nonactive;

	g_return_val_if_fail (profile != NULL, NULL);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), NULL);

	if (discharge) {
		colour_active = EGG_COLOR_BLUE;
		colour_nonactive = EGG_COLOR_DARK_BLUE;
	} else {
		colour_active = EGG_COLOR_RED;
		colour_nonactive = EGG_COLOR_DARK_RED;
	}

	/* get the correct data */
	if (discharge) {
		array = profile->priv->float_accuracy_discharge;
	} else {
		array = profile->priv->float_accuracy_charge;
	}

	/* copy the data field into the accuracy y field */
	for (i=0; i<100; i++) {
		value = gpm_array_float_get (array, i);
		/* only set points that are not zero */
		if (value > 0) {
			gpm_array_set (profile->priv->present_array_accuracy, i, i, value, colour_active);
		} else {
			/* set zero points a different colour */
			gpm_array_set (profile->priv->present_array_accuracy, i, i, value, colour_nonactive);
		}
	}

	return profile->priv->present_array_accuracy;
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
		egg_warning ("no config id set!");
		return 0;
	}

	/* check we can give a decent reading */
	if (percentage > 99) {
		egg_debug ("percentage = %i, correcting to 99%%", percentage);
		percentage = 99;
	}

	/* compute correct area of integral */
	if (discharge) {
		time = gpm_array_float_compute_integral (profile->priv->float_data_discharge, 0, percentage);
	} else {
		time = gpm_array_float_compute_integral (profile->priv->float_data_charge, percentage, 99);
	}

	return time;
}

/**
 * gpm_profile_get_nonzero_accuracy_percent:
 *
 * @profile: This class
 */
static float
gpm_profile_get_nonzero_accuracy_percent (GArray *array_data, GArray *array_accuracy)
{
	/* find the average "non-zero accuracy" data */
	guint i;
	guint length;
	gfloat data;
	gfloat accuracy;
	guint length_average = 0;
	gfloat average = 0;

	length = array_data->len;
	for (i=0; i<length; i++) {
		data = g_array_index (array_data, gfloat, i);
		accuracy = g_array_index (array_accuracy, gfloat, i);
		if (accuracy > 0) {
			average += data;
			length_average++;
		}
	}
	egg_debug ("average=%f, length_average=%i", average, length_average);
	if (average == 0 || length_average == 0) {
		/* no data with accuracy */
		egg_warning ("no data");
		return 0;
	}
	return average / (gfloat) length_average;
}

/**
 * gpm_profile_set_zero_accuracy_average:
 *
 * @profile: This class
 */
static void
gpm_profile_set_zero_accuracy_average (GArray *array_data,
				       GArray *array_accuracy,
				       gfloat  average,
				       guint   start,
				       guint   stop)
{
	/* set the zero accuracy points to the average value over a range */
	guint i;
	guint length;
	gfloat accuracy;
	guint width = 0;

	length = array_data->len;
	if (stop > length) {
		stop = length;
	}
	for (i=start; i<stop; i++) {
		accuracy = g_array_index (array_accuracy, gfloat, i);
		if (accuracy == 0) {
			/* we only do this if the width is not just one reading
			   as some batteries map 64 states into percent */
			if (width > 0) {
				g_array_index (array_data, gfloat, i) = average;
			}
			width++;
		} else {
			width = 0;
		}
	}
}

/**
 * gpm_profile_set_average_no_accuracy:
 *
 * @profile: This class
 */
static void
gpm_profile_set_average_no_accuracy (GpmProfile *profile)
{
	GArray *array_data;
	GArray *array_accuracy;
	gfloat average;

	/* get the correct data */
	if (profile->priv->discharging) {
		array_data = profile->priv->float_data_discharge;
		array_accuracy = profile->priv->float_accuracy_discharge;
	} else {
		array_data = profile->priv->float_data_charge;
		array_accuracy = profile->priv->float_accuracy_charge;
	}

	/* find the data average of the non zero accuracy points */
	average = gpm_profile_get_nonzero_accuracy_percent (array_data, array_accuracy);
	egg_debug ("estimated average = %f", average);

	/* set the zero accuracy data to the average */
	gpm_profile_set_zero_accuracy_average (array_data, array_accuracy, average, 5, 95);
}

/**
 * gpm_profile_save_percentage:
 *
 * @profile: This class
 * @percentage: new percentage value
 * @time: seconds elapsed
 * @measurement_accuracy: the percentage accuracy, where 100% is the
 *			  best we can get...
 */
static void
gpm_profile_save_percentage (GpmProfile *profile,
			     guint       percentage,
			     guint       time,
			     guint       measurement_accuracy)
{
	gchar *filename;
	GpmArray *array;
	GArray *array_data;
	GArray *array_accuracy;
	gfloat data;
	gfloat accuracy;

	/* get the correct data */
	if (profile->priv->discharging) {
		array_data = profile->priv->float_data_discharge;
		array_accuracy = profile->priv->float_accuracy_discharge;
	} else {
		array_data = profile->priv->float_data_charge;
		array_accuracy = profile->priv->float_accuracy_charge;
	}

	data = gpm_array_float_get (array_data, percentage);
	accuracy = gpm_array_float_get (array_accuracy, percentage);

	/* if we have no accuracy, then just use the new value */
	if (accuracy == 0) {
		data = time;
	} else {
		/* average the data so we converge to a common point */
		data = gpm_exponential_average (data, time, GPM_PROFILE_SMOOTH_SAVE_PERCENT);
	}

	/* save new data */
	gpm_array_float_set (array_data, percentage, data);

	/* save new accuracy (max gain is 20%, but less if the load was higher) */
	accuracy += measurement_accuracy / 5;
	if (accuracy > 100) {
		accuracy = 100;
	}
	gpm_array_float_set (array_accuracy, percentage, accuracy);

	/* guess missing data between 5 and 95 */
	if (profile->priv->use_guessing) {
		gpm_profile_set_average_no_accuracy (profile);
	}

	/* create a temp 2D array so we can save the CSV */
	array = gpm_array_new ();
	gpm_array_set_fixed_size (array, 100);

	/* we ignore x */
	gpm_array_float_to_array_y (array_data, array);
	gpm_array_float_to_array_z (array_accuracy, array);

	/* save data file (when idle...) */
	filename = gpm_profile_get_data_file (profile, profile->priv->discharging);
	gpm_array_save_to_file (array, filename);
	g_free (filename);
	g_object_unref (array);
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

	egg_debug ("elapsed is %f for %i at load %f (accuracy:%i)", elapsed, percentage, load, accuracy);

	/* don't process the first point, we maybe in between percentage points */
	if (profile->priv->data_valid == FALSE) {
		egg_debug ("data is not valid, will process next");
		profile->priv->data_valid = TRUE;
		return FALSE;
	}

	if (accuracy < 20) {
		egg_debug ("not accurate enough");
		return FALSE;
	}

	if (profile->priv->lcd_on == FALSE) {
		egg_debug ("screen blanked, so not representative - ignoring");
		return FALSE;
	}

	/* If we are discharging, 99-0 is valid, but when we are charging,
	 * 1-100 is valid. Be careful how we index the arrays in this case */
	if (profile->priv->discharging) {
		array_percentage = percentage;
	} else {
		if (percentage == 0) {
			egg_debug ("ignoring percentage zero when charging");
			return FALSE;
		}
		array_percentage = percentage - 1;
	}

	/* save the last valid percent so we can cope with batteries that
	   stop charging at < 100% */
	profile->priv->last_percentage = array_percentage;

	/* save new data as we passed all tests */
	if (array_percentage < 100) {
		gpm_profile_save_percentage (profile, array_percentage, (guint) elapsed, accuracy);
	}
	return TRUE;
}

/**
 * gpm_profile_test_force_discharging:
 */
void
gpm_profile_test_force_discharging (GpmProfile *profile,
				    gboolean    is_discharging)
{
	profile->priv->discharging = is_discharging;
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

	if (is_charging) {
		/* uninteresting case */
		return FALSE;
	}
	if (profile->priv->discharging) {
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
			egg_debug ("set percentage %i to zero", i);
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
	if (on_ac) {
		egg_debug ("on AC");
		profile->priv->discharging = FALSE;
	} else {
		egg_debug ("on battery");
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
	egg_debug ("DPMS mode changed: %d", mode);

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
	gchar *filename = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail (profile != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), FALSE);

	/* check we have a profile loaded */
	if (profile->priv->config_id == NULL) {
		egg_warning ("no config id set!");
		goto out;
	}

	/* create filename and check is exists */
	filename = gpm_profile_get_data_file (profile, discharge);
	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE) {
		/* seems insane, but we need this for the test suite */
		ret = TRUE;
		goto out;
	}

	/* try to delete file */
	if (g_unlink (filename) != 0) {
		egg_warning ("could not delete '%s'", filename);
		goto out;
	}
	ret = TRUE;
out:
	g_free (filename);
	return ret;
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
	GArray *array_data;
	GArray *array_accuracy;

	/* create a temp 2D array so we can load the CSV */
	array = gpm_array_new ();
	gpm_array_set_fixed_size (array, 100);

	/* get the correct data */
	if (discharge) {
		array_data = profile->priv->float_data_discharge;
		array_accuracy = profile->priv->float_accuracy_discharge;
	} else {
		array_data = profile->priv->float_data_charge;
		array_accuracy = profile->priv->float_accuracy_charge;
	}

	/* read in data profile from disk */
	filename = gpm_profile_get_data_file (profile, discharge);
	egg_debug ("loading battery data from '%s'", filename);
	ret = gpm_array_load_from_file (array, filename);

	/* if not found, then generate a new one with a low propability */
	if (!ret) {

		/* directory might not exist */
		path = g_build_filename (g_get_home_dir (), ".gnome2", "gnome-power-manager", NULL);
		g_mkdir_with_parents (path, 0770);
		g_free (path);

		egg_debug ("no data found, generating initial (poor) data");
		for (i=0;i<100;i++) {
			/* just set data to zero as we have _no_ idea */
			gpm_array_set (array, i, i, 0, 0);
		}

		ret = gpm_array_save_to_file (array, filename);
		if (!ret) {
			egg_warning ("saving state failed. You will not get accurate time remaining calculations");
		}
	}

	/* we ignore x as it's just a dummy counter */
	gpm_array_float_from_array_y (array_data, array);
	gpm_array_float_from_array_z (array_accuracy, array);

	g_object_unref (array);
	g_free (filename);
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
	guint time;

	g_return_val_if_fail (profile != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (config_id != NULL, FALSE);

	egg_debug ("config_id = %s", config_id);
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
	if (reload) {
		/* get initial data */
		gpm_profile_load_data (profile, TRUE);
		gpm_profile_load_data (profile, FALSE);
	}

	/* do debugging self tests only if verbose */
	if (egg_debug_enabled ()) {
		egg_debug ("Reference times");
		time = gpm_profile_get_time (profile, 99, TRUE);
		egg_debug ("99-0\t%i minutes", time / 60);
		time = gpm_profile_get_time (profile, 50, TRUE);
		egg_debug ("50-0\t%i minutes", time / 60);

		time = gpm_profile_get_time (profile, 0, FALSE);
		egg_debug ("0-99\t%i minutes", time / 60);
		time = gpm_profile_get_time (profile, 50, FALSE);
		egg_debug ("50-99\t%i minutes", time / 60);
	}

	return TRUE;
}

/**
 * gpm_profile_use_guessing:
 */
gboolean
gpm_profile_use_guessing (GpmProfile *profile,
			  gboolean    use_guessing)
{
	profile->priv->use_guessing = use_guessing;
	return TRUE;
}

/**
 * gpm_profile_get_accuracy:
 */
guint
gpm_profile_get_accuracy (GpmProfile *profile,
			  guint	      percentage)
{
	GArray *array;

	g_return_val_if_fail (profile != NULL, 0);
	g_return_val_if_fail (GPM_IS_PROFILE (profile), 0);

	/* check we have a profile loaded */
	if (profile->priv->config_id == NULL) {
		egg_warning ("no config id set!");
		return 0;
	}

	if (percentage > 99) {
		percentage = 99;
		egg_debug ("corrected percentage...");
	}

	/* get the correct data */
	if (profile->priv->discharging) {
		array = profile->priv->float_accuracy_discharge;
	} else {
		array = profile->priv->float_accuracy_charge;
	}

	return gpm_array_float_get (array, percentage);
}

/**
 * control_sleep_action_cb:
 **/
static void
control_sleep_action_cb (GpmControl      *control,
			 GpmControlAction action,
		         GpmProfile      *profile)
{
	/* the charge might have changed when suspended or hibernated */
	profile->priv->data_valid = FALSE;
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
	profile->priv->use_guessing = TRUE;

	profile->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (profile->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adaptor_changed_cb), profile);

	profile->priv->dpms = gpm_dpms_new ();
	g_signal_connect (profile->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), profile);

	/* used for data */
	profile->priv->float_data_charge = gpm_array_float_new (100);
	profile->priv->float_data_discharge = gpm_array_float_new (100);
	profile->priv->float_accuracy_charge = gpm_array_float_new (100);
	profile->priv->float_accuracy_discharge = gpm_array_float_new (100);

	/* used for presentation, hence we can share */
	profile->priv->present_array_accuracy = gpm_array_new ();
	profile->priv->present_array_data = gpm_array_new ();
	gpm_array_set_fixed_size (profile->priv->present_array_data, 100);
	gpm_array_set_fixed_size (profile->priv->present_array_accuracy, 100);

	/* use the control object so we don't save new percentage points
	 * after suspend or hibernate */
	egg_debug ("creating new control instance");
	profile->priv->control = gpm_control_new ();
	g_signal_connect (profile->priv->control, "sleep",
			  G_CALLBACK (control_sleep_action_cb), profile);
	g_signal_connect (profile->priv->control, "resume",
			  G_CALLBACK (control_sleep_action_cb), profile);
	g_signal_connect (profile->priv->control, "sleep-failure",
			  G_CALLBACK (control_sleep_action_cb), profile);

	/* default */
	profile->priv->lcd_on = TRUE;
	profile->priv->last_percentage = 100;

	/* we might be halfway through a percentage change */
	profile->priv->data_valid = FALSE;

	/* coldplug the AC state */
	on_ac = gpm_ac_adapter_is_present (profile->priv->ac_adapter);

	/* check AC state */
	if (on_ac) {
		egg_debug ("on AC");
		profile->priv->discharging = FALSE;
	} else {
		egg_debug ("on battery");
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
	g_object_unref (profile->priv->control);

	gpm_array_float_free (profile->priv->float_data_charge);
	gpm_array_float_free (profile->priv->float_data_discharge);
	gpm_array_float_free (profile->priv->float_accuracy_charge);
	gpm_array_float_free (profile->priv->float_accuracy_discharge);

	g_object_unref (profile->priv->present_array_accuracy);
	g_object_unref (profile->priv->present_array_data);
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

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef GPM_BUILD_TESTS
#include "gpm-self-test.h"

static void
reset_profile (GpmProfile *profile)
{
	gpm_profile_delete_data (profile, TRUE);
	gpm_profile_delete_data (profile, FALSE);
	gpm_profile_load_data (profile, TRUE);
	gpm_profile_load_data (profile, FALSE);
}

void
gpm_st_profile (GpmSelfTest *test)
{
	GpmProfile *profile;
	gboolean ret;
	gchar *filename;
	gint i;
	guint value;
	gfloat fvalue;

	if (gpm_st_start (test, "GpmProfile") == FALSE) {
		return;
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null profile");
	profile = gpm_profile_new ();
	if (profile != NULL) {
		gpm_st_success (test, "got GpmProfile");
	} else {
		gpm_st_failed (test, "could not get GpmProfile");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a zero accuracy when no-id");
	value = gpm_profile_get_accuracy (profile, 50);
	if (value == 0) {
		gpm_st_success (test, "got zero");
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "set config id");
	ret = gpm_profile_set_config_id (profile, "test123");
	if (ret) {
		gpm_st_success (test, "set type");
	} else {
		gpm_st_failed (test, "could not set type");
	}


	/************************************************************
	 **              UTILITY FUNCTIONS                         **
	 ************************************************************/

	/************************************************************/
	gpm_st_title (test, "get correct charging filename");
	filename = gpm_profile_get_data_file (profile, FALSE);
	if (strstr (filename, "/.gnome2/gnome-power-manager/profile-test123-charging.csv") != NULL) {
		gpm_st_success (test, "got correct filename");
	} else {
		gpm_st_failed (test, "got incorrect filename: %s", filename);
	}
	g_free (filename);

	/************************************************************/
	gpm_st_title (test, "get correct discharging filename");
	filename = gpm_profile_get_data_file (profile, TRUE);
	if (strstr (filename, "/.gnome2/gnome-power-manager/profile-test123-discharging.csv") != NULL) {
		gpm_st_success (test, "got correct filename");
	} else {
		gpm_st_failed (test, "got incorrect filename: %s", filename);
	}
	g_free (filename);

	/* clear old profile */
	reset_profile (profile);

	/************************************************************/
	gpm_st_title (test, "make sure we get a zero accuracy with a new dataset");
	value = gpm_profile_get_accuracy (profile, 50);
	if (value == 0) {
		gpm_st_success (test, "got zero");
	} else {
		gpm_st_failed (test, "got %i (not zero!)", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a zero time with a new dataset");
	value = gpm_profile_get_time (profile, 50, TRUE);
	if (value == 0) {
		gpm_st_success (test, "got zero");
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	/************************************************************
	 **                TYPICAL DISCHARGE                       **
	 ************************************************************/

	/************************************************************/
	gpm_st_title (test, "force discharging");
	gpm_profile_test_force_discharging (profile, TRUE);
	gpm_st_success (test, NULL);

	/************************************************************/
	gpm_st_title (test, "ignore first point");
	ret = gpm_profile_register_percentage (profile, 99);
	if (!ret) {
		gpm_st_success (test, "ignored first");
	} else {
		gpm_st_failed (test, "ignored second");
	}

	/************************************************************/
	gpm_st_title (test, "make up discharging dataset 98-0 (perfect accuracy)");
	for (i=98; i>=0; i--) {
		gpm_profile_save_percentage (profile, i, 120, 100);
	}
	gpm_st_success (test, "put dataset");

	/************************************************************/
	gpm_st_title (test, "make sure we get a correct accuracy when a complete dataset");
	value = gpm_profile_get_accuracy (profile, 50);
	if (value == 20) {
		gpm_st_success (test, "got correct average %i", value);
	} else {
		gpm_st_failed (test, "got incorrect average %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a correct discharge time when set");
	value = gpm_profile_get_time (profile, 50, TRUE);
	if (value == 6120) {
		gpm_st_success (test, "got correct time %i", value);
	} else {
		gpm_st_failed (test, "got incorrect time %i", value);
	}

	/************************************************************
	 **                  NO PROFILE TEST                       **
	 ************************************************************/

	/* clear old profile */
	reset_profile (profile);

	/************************************************************/
	gpm_st_title (test, "single point of accuracy with no guessing (new profile)");
	gpm_profile_use_guessing (profile, FALSE);
	gpm_profile_save_percentage (profile, 45, 120, 100);
	gpm_st_success (test, "put single point");

	/************************************************************/
	gpm_st_title (test, "make sure we get a correct accuracy when a single point dataset");
	value = gpm_profile_get_accuracy (profile, 45);
	if (value == 20) {
		gpm_st_success (test, "got correct average %i", value);
	} else {
		gpm_st_failed (test, "got incorrect average %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a correct time when set");
	value = gpm_profile_get_time (profile, 50, TRUE);
	if (value == 120) {
		gpm_st_success (test, "got correct time %i", value);
	} else {
		gpm_st_failed (test, "got incorrect time %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non-zero average accuracy when some data present");
	fvalue = gpm_profile_get_accuracy_average (profile, TRUE);
	if (fvalue < 0.21 && fvalue > 0.19) {
		gpm_st_success (test, "got non zero %f", fvalue);
	} else {
		gpm_st_failed (test, "got %f", fvalue);
	}

	/************************************************************
	 **                  NO PROFILE TEST 2                     **
	 ************************************************************/

	/* clear old profile */
	reset_profile (profile);

	/************************************************************/
	gpm_st_title (test, "single point of accuracy with guessing (new profile)");
	gpm_profile_use_guessing (profile, TRUE);
	gpm_profile_save_percentage (profile, 45, 120, 100);
	gpm_st_success (test, "put single point");

	/************************************************************/
	gpm_st_title (test, "make sure we get a correct accuracy when a single point dataset");
	value = gpm_profile_get_accuracy (profile, 45);
	if (value == 20) {
		gpm_st_success (test, "got correct average %i", value);
	} else {
		gpm_st_failed (test, "got incorrect average %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure the point after the guessed point isn't guessed");
	value = gpm_profile_get_accuracy (profile, 46);
	if (value == 0) {
		gpm_st_success (test, "didn't try to guess point after set", value);
	} else {
		gpm_st_failed (test, "got accuracy average %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a correct time when set");
	value = gpm_profile_get_time (profile, 50, TRUE);
	if (value == 5280) {
		gpm_st_success (test, "got correct time %i", value);
	} else {
		gpm_st_failed (test, "got incorrect time!!! %i", value);
	}

	/************************************************************/
	gpm_profile_delete_data (profile, FALSE);
	gpm_profile_delete_data (profile, TRUE);

	g_object_unref (profile);

	gpm_st_end (test);
}

#endif

