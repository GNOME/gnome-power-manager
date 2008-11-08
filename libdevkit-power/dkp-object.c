/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib.h>
#include <glib-object.h>
#include <string.h>

#include "egg-debug.h"
#include "egg-string.h"

#include "dkp-enum.h"
#include "dkp-object.h"

/**
 * dkp_object_clear_internal:
 **/
static void
dkp_object_clear_internal (DkpObject *obj)
{
	obj->type = DKP_DEVICE_TYPE_UNKNOWN;
	obj->update_time = 0;
	obj->energy = 0;
	obj->energy_full = 0;
	obj->energy_full_design = 0;
	obj->energy_rate = 0;
	obj->voltage = 0;
	obj->percentage = 0;
	obj->capacity = 0;
	obj->time_to_empty = 0;
	obj->time_to_full = 0;
	obj->state = DKP_DEVICE_STATE_UNKNOWN;
	obj->technology = DKP_DEVICE_TECHNOLGY_UNKNOWN;
	obj->vendor = NULL;
	obj->model = NULL;
	obj->serial = NULL;
	obj->native_path = NULL;
	obj->online = FALSE;
	obj->is_present = FALSE;
	obj->power_supply = FALSE;
	obj->is_rechargeable = FALSE;
	obj->has_history = FALSE;
	obj->has_statistics = FALSE;
}

/**
 * dkp_object_collect_props:
 **/
static void
dkp_object_collect_props (const char *key, const GValue *value, DkpObject *obj)
{
	gboolean handled = TRUE;

	if (egg_strequal (key, "native-path"))
		obj->native_path = g_strdup (g_value_get_string (value));
	else if (egg_strequal (key, "vendor"))
		obj->vendor = g_strdup (g_value_get_string (value));
	else if (egg_strequal (key, "model"))
		obj->model = g_strdup (g_value_get_string (value));
	else if (egg_strequal (key, "serial"))
		obj->serial = g_strdup (g_value_get_string (value));
	else if (egg_strequal (key, "update-time"))
		obj->update_time = g_value_get_uint64 (value);
	else if (egg_strequal (key, "type"))
		obj->type = dkp_device_type_from_text (g_value_get_string (value));
	else if (egg_strequal (key, "online"))
		obj->online = g_value_get_boolean (value);
	else if (egg_strequal (key, "has-history"))
		obj->has_history = g_value_get_boolean (value);
	else if (egg_strequal (key, "has-statistics"))
		obj->has_statistics = g_value_get_boolean (value);
	else if (egg_strequal (key, "energy"))
		obj->energy = g_value_get_double (value);
	else if (egg_strequal (key, "energy-empty"))
		obj->energy_empty = g_value_get_double (value);
	else if (egg_strequal (key, "energy-full"))
		obj->energy_full = g_value_get_double (value);
	else if (egg_strequal (key, "energy-full-design"))
		obj->energy_full_design = g_value_get_double (value);
	else if (egg_strequal (key, "energy-rate"))
		obj->energy_rate = g_value_get_double (value);
	else if (egg_strequal (key, "voltage"))
		obj->voltage = g_value_get_double (value);
	else if (egg_strequal (key, "time-to-full"))
		obj->time_to_full = g_value_get_int64 (value);
	else if (egg_strequal (key, "time-to-empty"))
		obj->time_to_empty = g_value_get_int64 (value);
	else if (egg_strequal (key, "percentage"))
		obj->percentage = g_value_get_double (value);
	else if (egg_strequal (key, "technology"))
		obj->technology = dkp_device_technology_from_text (g_value_get_string (value));
	else if (egg_strequal (key, "is-present"))
		obj->is_present = g_value_get_boolean (value);
	else if (egg_strequal (key, "is-rechargeable"))
		obj->is_rechargeable = g_value_get_boolean (value);
	else if (egg_strequal (key, "power-supply"))
		obj->power_supply = g_value_get_boolean (value);
	else if (egg_strequal (key, "capacity"))
		obj->capacity = g_value_get_double (value);
	else if (egg_strequal (key, "state"))
		obj->state = dkp_device_state_from_text (g_value_get_string (value));
	else
		handled = FALSE;

	if (!handled)
		egg_warning ("unhandled property '%s'", key);
}

/**
 * dkp_object_set_from_map:
 **/
gboolean
dkp_object_set_from_map	(DkpObject *obj, GHashTable *hash_table)
{
	g_hash_table_foreach (hash_table, (GHFunc) dkp_object_collect_props, obj);
	return TRUE;
}

/**
 * dkp_object_copy:
 **/
DkpObject *
dkp_object_copy (const DkpObject *cobj)
{
	DkpObject *obj;
	obj = g_new0 (DkpObject, 1);

	obj->type = cobj->type;
	obj->update_time = cobj->update_time;
	obj->energy = cobj->energy;
	obj->energy_full = cobj->energy_full;
	obj->energy_full_design = cobj->energy_full_design;
	obj->energy_rate = cobj->energy_rate;
	obj->voltage = cobj->voltage;
	obj->percentage = cobj->percentage;
	obj->capacity = cobj->capacity;
	obj->time_to_empty = cobj->time_to_empty;
	obj->time_to_full = cobj->time_to_full;
	obj->state = cobj->state;
	obj->technology = cobj->technology;
	obj->vendor = g_strdup (cobj->vendor);
	obj->model = g_strdup (cobj->model);
	obj->serial = g_strdup (cobj->serial);
	obj->native_path = g_strdup (cobj->native_path);
	obj->online = cobj->online;
	obj->is_present = cobj->is_present;
	obj->power_supply = cobj->power_supply;
	obj->is_rechargeable = cobj->is_rechargeable;
	obj->has_history = cobj->has_history;
	obj->has_statistics = cobj->has_statistics;

	return obj;
}

/**
 * dkp_object_equal:
 **/
gboolean
dkp_object_equal (const DkpObject *obj1, const DkpObject *obj2)
{
	if (obj1->type == obj2->type &&
	    obj1->update_time == obj2->update_time &&
	    obj1->energy == obj2->energy &&
	    obj1->energy_full == obj2->energy_full &&
	    obj1->energy_full_design == obj2->energy_full_design &&
	    obj1->voltage == obj2->voltage &&
	    obj1->energy_rate == obj2->energy_rate &&
	    obj1->percentage == obj2->percentage &&
	    obj1->has_history == obj2->has_history &&
	    obj1->has_statistics == obj2->has_statistics &&
	    obj1->capacity == obj2->capacity &&
	    obj1->time_to_empty == obj2->time_to_empty &&
	    obj1->time_to_full == obj2->time_to_full &&
	    obj1->state == obj2->state &&
	    obj1->technology == obj2->technology &&
	    egg_strequal (obj1->vendor, obj2->vendor) &&
	    egg_strequal (obj1->model, obj2->model) &&
	    egg_strequal (obj1->serial, obj2->serial) &&
	    egg_strequal (obj1->native_path, obj2->native_path) &&
	    obj1->online == obj2->online &&
	    obj1->is_present == obj2->is_present &&
	    obj1->power_supply == obj2->power_supply &&
	    obj1->is_rechargeable == obj2->is_rechargeable)
		return TRUE;
	return FALSE;
}

/**
 * dkp_object_time_to_text:
 **/
static gchar *
dkp_object_time_to_text (gint seconds)
{
	gfloat value = seconds;

	if (value < 0)
		return g_strdup ("unknown");
	if (value < 60)
		return g_strdup_printf ("%.0f seconds", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f minutes", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f hours", value);
	value /= 24.0;
	return g_strdup_printf ("%.1f days", value);
}

/**
 * dkp_object_bool_to_text:
 **/
static const gchar *
dkp_object_bool_to_text (gboolean ret)
{
	return ret ? "yes" : "no";
}

/**
 * dkp_object_print:
 **/
gboolean
dkp_object_print (const DkpObject *obj)
{
	gboolean ret = TRUE;
	struct tm *time_tm;
	time_t t;
	gchar time_buf[256];
	gchar *time_str;

	/* get a human readable time */
	t = (time_t) obj->update_time;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	g_print ("  native-path:          %s\n", obj->native_path);
	if (!egg_strzero (obj->vendor))
		g_print ("  vendor:               %s\n", obj->vendor);
	if (!egg_strzero (obj->model))
		g_print ("  model:                %s\n", obj->model);
	if (!egg_strzero (obj->serial))
		g_print ("  serial:               %s\n", obj->serial);
	g_print ("  power supply:         %s\n", dkp_object_bool_to_text (obj->power_supply));
	g_print ("  updated:              %s (%d seconds ago)\n", time_buf, (int) (time (NULL) - obj->update_time));
	g_print ("  has history:          %s\n", dkp_object_bool_to_text (obj->has_history));
	g_print ("  has statistics:       %s\n", dkp_object_bool_to_text (obj->has_statistics));
	g_print ("  %s\n", dkp_device_type_to_text (obj->type));

	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD ||
	    obj->type == DKP_DEVICE_TYPE_UPS)
		g_print ("    present:             %s\n", dkp_object_bool_to_text (obj->is_present));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD)
		g_print ("    rechargeable:        %s\n", dkp_object_bool_to_text (obj->is_rechargeable));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD)
		g_print ("    state:               %s\n", dkp_device_state_to_text (obj->state));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		g_print ("    energy:              %g Wh\n", obj->energy);
		g_print ("    energy-empty:        %g Wh\n", obj->energy_empty);
		g_print ("    energy-full:         %g Wh\n", obj->energy_full);
		g_print ("    energy-full-design:  %g Wh\n", obj->energy_full_design);
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MONITOR)
		g_print ("    energy-rate:         %g W\n", obj->energy_rate);
	if (obj->type == DKP_DEVICE_TYPE_UPS ||
	    obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MONITOR)
		g_print ("    voltage:             %g V\n", obj->voltage);
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_UPS) {
		if (obj->time_to_full >= 0) {
			time_str = dkp_object_time_to_text (obj->time_to_full);
			g_print ("    time to full:        %s\n", time_str);
			g_free (time_str);
		}
		if (obj->time_to_empty >= 0) {
			time_str = dkp_object_time_to_text (obj->time_to_empty);
			g_print ("    time to empty:       %s\n", time_str);
			g_free (time_str);
		}
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD ||
	    obj->type == DKP_DEVICE_TYPE_UPS)
		g_print ("    percentage:          %g%%\n", obj->percentage);
	if (obj->type == DKP_DEVICE_TYPE_BATTERY)
		g_print ("    capacity:            %g%%\n", obj->capacity);
	if (obj->type == DKP_DEVICE_TYPE_BATTERY)
		g_print ("    technology:          %s\n", dkp_device_technology_to_text (obj->technology));
	if (obj->type == DKP_DEVICE_TYPE_LINE_POWER)
		g_print ("    online:             %s\n", dkp_object_bool_to_text (obj->online));

	return ret;
}

/**
 * dkp_object_diff:
 **/
gboolean
dkp_object_diff (const DkpObject *old, const DkpObject *obj)
{
	gchar *time_str;
	gchar *time_str_old;

	g_print ("  native-path:          %s\n", obj->native_path);
	if (!egg_strequal (obj->vendor, old->vendor))
		g_print ("  vendor:               %s -> %s\n", old->vendor, obj->vendor);
	if (!egg_strequal (obj->model, old->model))
		g_print ("  model:                %s -> %s\n", old->model, obj->model);
	if (!egg_strequal (obj->serial, old->serial))
		g_print ("  serial:               %s -> %s\n", old->serial, obj->serial);
	if (obj->has_history != old->has_history)
		g_print ("  has history:          %s -> %s\n",
			 dkp_object_bool_to_text (old->has_history),
			 dkp_object_bool_to_text (obj->has_history));
	if (obj->has_statistics != old->has_statistics)
		g_print ("  has statistics:       %s -> %s\n",
			 dkp_object_bool_to_text (old->has_statistics),
			 dkp_object_bool_to_text (obj->has_statistics));

	g_print ("  %s\n", dkp_device_type_to_text (obj->type));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD)
		if (old->is_present != obj->is_present)
			g_print ("    present:             %s -> %s\n",
				 dkp_object_bool_to_text (old->is_present),
				 dkp_object_bool_to_text (obj->is_present));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY)
		if (old->is_rechargeable != obj->is_rechargeable)
			g_print ("    rechargeable:        %s -> %s\n",
				 dkp_object_bool_to_text (old->is_rechargeable),
				 dkp_object_bool_to_text (obj->is_rechargeable));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD ||
	    obj->type == DKP_DEVICE_TYPE_UPS)
		if (old->state != obj->state)
			g_print ("    state:               %s -> %s\n",
				 dkp_device_state_to_text (old->state),
				 dkp_device_state_to_text (obj->state));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		if (old->energy != obj->energy)
			g_print ("    energy:              %g -> %g Wh\n",
				 old->energy,
				 obj->energy);
		if (old->energy_empty != obj->energy_empty)
			g_print ("    energy-empty:        %g -> %g Wh\n",
				 old->energy_empty,
				 obj->energy_empty);
		if (old->energy_full != obj->energy_full)
			g_print ("    energy-full:         %g -> %g Wh\n",
				 old->energy_full,
				 obj->energy_full);
		if (old->energy_full_design != obj->energy_full_design)
			g_print ("    energy-full-design:  %g -> %g Wh\n",
				 old->energy_full_design,
				 obj->energy_full_design);
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MONITOR)
		if (old->energy_rate != obj->energy_rate)
			g_print ("    energy-rate:         %g -> %g W\n",
				 old->energy_rate, obj->energy_rate);
	if (obj->type == DKP_DEVICE_TYPE_UPS ||
	    obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MONITOR)
		if (old->voltage != obj->voltage)
			g_print ("    voltage:             %g -> %g V\n",
				 old->voltage, obj->voltage);
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_UPS) {
		if (old->time_to_full != obj->time_to_full) {
			time_str_old = dkp_object_time_to_text (old->time_to_full);
			time_str = dkp_object_time_to_text (obj->time_to_full);
			g_print ("    time to full:        %s -> %s\n", time_str_old, time_str);
			g_free (time_str_old);
			g_free (time_str);
		}
		if (old->time_to_empty != obj->time_to_empty) {
			time_str_old = dkp_object_time_to_text (old->time_to_empty);
			time_str = dkp_object_time_to_text (obj->time_to_empty);
			g_print ("    time to empty:       %s -> %s\n", time_str_old, time_str);
			g_free (time_str_old);
			g_free (time_str);
		}
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_UPS ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD)
		if (old->percentage != obj->percentage)
			g_print ("    percentage:          %g%% -> %g%%\n",
				 old->percentage, obj->percentage);
	if (obj->type == DKP_DEVICE_TYPE_BATTERY)
		if (old->capacity != obj->capacity)
			g_print ("    capacity:            %g%% -> %g%%\n",
				 old->capacity, obj->capacity);
	if (obj->type == DKP_DEVICE_TYPE_BATTERY)
		if (old->technology != obj->technology)
			g_print ("    technology:          %s -> %s\n",
				 dkp_device_technology_to_text (old->technology),
				 dkp_device_technology_to_text (obj->technology));
	if (obj->type == DKP_DEVICE_TYPE_LINE_POWER)
		if (old->online != obj->online)
			g_print ("    online:             %s -> %s\n",
				 dkp_object_bool_to_text (old->online),
				 dkp_object_bool_to_text (obj->online));
	return TRUE;
}

/**
 * dkp_object_new:
 **/
DkpObject *
dkp_object_new (void)
{
	DkpObject *obj;
	obj = g_new0 (DkpObject, 1);
	dkp_object_clear_internal (obj);
	return obj;
}

/**
 * dkp_object_free_internal:
 **/
static gboolean
dkp_object_free_internal (DkpObject *obj)
{
	g_free (obj->vendor);
	g_free (obj->model);
	g_free (obj->serial);
	g_free (obj->native_path);
	return TRUE;
}

/**
 * dkp_object_free:
 **/
gboolean
dkp_object_free (DkpObject *obj)
{
	if (obj == NULL)
		return FALSE;
	dkp_object_free_internal (obj);
	g_free (obj);
	return TRUE;
}

/**
 * dkp_object_clear:
 **/
gboolean
dkp_object_clear (DkpObject *obj)
{
	if (obj == NULL)
		return FALSE;
	dkp_object_free_internal (obj);
	dkp_object_clear_internal (obj);
	return TRUE;
}

/**
 * dkp_object_get_id:
 **/
gchar *
dkp_object_get_id (DkpObject *obj)
{
	GString *string;
	gchar *id = NULL;

	/* line power */
	if (obj->type == DKP_DEVICE_TYPE_LINE_POWER) {
		goto out;

	/* batteries */
	} else if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		/* we don't have an ID if we are not present */
		if (!obj->is_present)
			goto out;

		string = g_string_new ("");

		/* in an ideal world, model-capacity-serial */
		if (obj->model != NULL && strlen (obj->model) > 2) {
			g_string_append (string, obj->model);
			g_string_append_c (string, '-');
		}
		if (obj->energy_full_design > 0) {
			g_string_append_printf (string, "%i", (guint) obj->energy_full_design);
			g_string_append_c (string, '-');
		}
		if (obj->serial != NULL && strlen (obj->serial) > 2) {
			g_string_append (string, obj->serial);
			g_string_append_c (string, '-');
		}

		/* make sure we are sane */
		if (string->len == 0) {
			/* just use something generic */
			g_string_append (string, "generic_id");
		} else {
			/* remove trailing '-' */
			g_string_set_size (string, string->len - 1);
		}

		/* the id may have invalid chars that need to be replaced */
		id = g_string_free (string, FALSE);

	} else {
		/* generic fallback */
		id = g_strdup_printf ("%s-%s-%s", obj->vendor, obj->model, obj->serial);
	}

	g_strdelimit (id, "\\\t\"?' /", '_');

out:
	return id;
}

