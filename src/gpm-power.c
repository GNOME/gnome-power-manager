/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2006 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "gpm-common.h"
#include "gpm-hal.h"
#include "gpm-hal-monitor.h"

#include "gpm-power.h"
#include "gpm-marshal.h"
#include "gpm-debug.h"

static void     gpm_power_class_init (GpmPowerClass *klass);
static void     gpm_power_init       (GpmPower      *power);
static void     gpm_power_finalize   (GObject       *object);

#define GPM_POWER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POWER, GpmPowerPrivate))

struct GpmPowerPrivate
{
	gboolean       on_ac;

	GHashTable    *battery_kind_cache;
	GHashTable    *battery_device_cache;

	GpmHalMonitor *hal_monitor;
};

enum {
	BUTTON_PRESSED,
	AC_STATE_CHANGED,
	BATTERY_STATUS_CHANGED,
	BATTERY_REMOVED,
	LAST_SIGNAL
};

typedef enum {
	GPM_POWER_UNIT_MWH,
	GPM_POWER_UNIT_CSR,
	GPM_POWER_UNIT_PERCENT,
	GPM_POWER_UNIT_UNKNOWN,
} GpmPowerBatteryUnit;

enum {
	PROP_0,
	PROP_ON_AC
};

static GObjectClass *parent_class = NULL;
static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmPower, gpm_power, G_TYPE_OBJECT)

/**
 * Multiple batteries percentages are averaged and times added
 * so that a virtual device is presented to the program. This is
 * required as we show the icons and do the events as averaged
 * over all battery devices of the same type.
 */
typedef struct {
	GpmPowerBatteryKind	battery_kind;
	GpmPowerBatteryStatus	battery_status;
	/* List of device udis */
	GSList  *devices;
} BatteryKindCacheEntry;

typedef struct {
	char		       *udi;
	int			charge_rate_previous;
	GpmPowerBatteryKind	battery_kind;
	GpmPowerBatteryStatus	battery_status;
	char		       *product;
	char		       *vendor;
	char		       *technology;
	GpmPowerBatteryUnit	unit;
	char		       *serial;
	char		       *model;
} BatteryDeviceCacheEntry;

/* Increasing this value will increase the damping effect
 * of the rate calculations, 0.8 seems a good default. */
#define		RATE_EXP_AVERAGE_FACTOR			(0.80f)

static void
gpm_power_battery_status_set_defaults (GpmPowerBatteryStatus *status)
{
	/* initialise to known defaults */
	status->design_charge = 0;
	status->last_full_charge = 0;
	status->current_charge = 0;
	status->charge_rate = 0;
	status->percentage_charge = 0;
	status->remaining_time = 0;
	status->capacity = 0;
	status->is_rechargeable = FALSE;
	status->is_present = FALSE;
	status->is_charging = FALSE;
	status->is_discharging = FALSE;
}

/* we have to be clever here, as there are lots of broken batteries */
gboolean
gpm_power_battery_is_charged (GpmPowerBatteryStatus *status)
{
	/* We have to do the additional check for 90% as
	   some batteries have a reduced charge capacity and cannot
	   charge to 100% anymore (although they *should* update
	   thier own last_full tags, it appears most do not...) */
	if (! status->is_charging &&
	    ! status->is_discharging &&
	    status->percentage_charge > 90) {
		return TRUE;
	}
	return FALSE;
}

static void
battery_device_cache_entry_update_all (BatteryDeviceCacheEntry *entry)
{
	gboolean exists;
	GpmPowerBatteryStatus *status = &entry->battery_status;
	char *udi = entry->udi;
	char *battery_kind_str;

	/* invalidate last rate */
	entry->charge_rate_previous = 0;

	/* Initialize battery_status to reasonable defaults */
	gpm_power_battery_status_set_defaults (status);

	gpm_hal_device_get_string (udi, "battery.type", &battery_kind_str);

	if (!battery_kind_str) {
		gpm_warning ("cannot obtain battery type");
		return;
	}
	if (strcmp (battery_kind_str, "primary") == 0) {
		entry->battery_kind = GPM_POWER_BATTERY_KIND_PRIMARY;
	} else if (strcmp (battery_kind_str, "ups") == 0) {
		entry->battery_kind = GPM_POWER_BATTERY_KIND_UPS;
	} else if (strcmp (battery_kind_str, "keyboard") == 0) {
		entry->battery_kind = GPM_POWER_BATTERY_KIND_KEYBOARD;
	} else if (strcmp (battery_kind_str, "mouse") == 0) {
		entry->battery_kind = GPM_POWER_BATTERY_KIND_MOUSE;
	} else if (strcmp (battery_kind_str, "pda") == 0) {
		entry->battery_kind = GPM_POWER_BATTERY_KIND_PDA;
	} else {
		gpm_warning ("battery type %s unknown",
			   battery_kind_str);
		g_free (battery_kind_str);
		return;
	}
	g_free (battery_kind_str);

	/* batteries might be missing */
	gpm_hal_device_get_bool (udi, "battery.present", &status->is_present);
	if (! status->is_present) {
		gpm_debug ("Battery not present, so not filling up values");
		return;
	}

	gpm_hal_device_get_int (udi, "battery.charge_level.design",
				&status->design_charge);
	gpm_hal_device_get_int (udi, "battery.charge_level.last_full",
				&status->last_full_charge);
	gpm_hal_device_get_int (udi, "battery.charge_level.current",
				&status->current_charge);

	/* battery might not be rechargeable, have to check */
	gpm_hal_device_get_bool (udi, "battery.is_rechargeable",
				&status->is_rechargeable);

	if (entry->battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY ||
	    entry->battery_kind == GPM_POWER_BATTERY_KIND_UPS) {
		if (status->is_rechargeable) {
			gpm_hal_device_get_bool (udi, "battery.rechargeable.is_charging",
						&status->is_charging);
			gpm_hal_device_get_bool (udi, "battery.rechargeable.is_discharging",
						&status->is_discharging);
		}
	}

	/* sanity check that charge_level.rate exists (if it should) */
	if (entry->battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY) {
		exists = gpm_hal_device_get_int (udi, "battery.charge_level.rate",
						 &status->charge_rate);
		if (!exists && (status->is_discharging || status->is_charging)) {
			gpm_warning ("could not read your battery's charge rate");
		}

		/* FIXME: following can be removed if bug #5752 of hal on freedesktop
		   gets fixed and is part of a new release of HAL and we depend on that
		   version*/
		if (exists && status->charge_rate == 0) {
			status->is_discharging = FALSE;
			status->is_charging = FALSE;
		}
	}


	/* sanity check that charge_level.percentage exists (if it should) */
	exists = gpm_hal_device_get_int (udi, "battery.charge_level.percentage",
					     &status->percentage_charge);
	if (!exists && (status->is_discharging || status->is_charging)) {
		gpm_warning ("could not read your battery's percentage charge.");
	}

	/* sanity check that remaining time exists (if it should) */
	if (entry->battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY ||
	    entry->battery_kind == GPM_POWER_BATTERY_KIND_UPS) {
		exists = gpm_hal_device_get_int (udi,"battery.remaining_time",
						 &status->remaining_time);
		if (! exists && (status->is_discharging || status->is_charging)) {
			gpm_warning ("could not read your battery's remaining time");
		}
	}

	/* calculate the batteries capacity if it is primary and present */
	if (entry->battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY && status->is_present) {
		if (status->design_charge > 0 && status->last_full_charge > 0) {
			if (status->design_charge != status->last_full_charge) {
				float capacity;
				capacity = status->design_charge / status->last_full_charge;
				status->capacity = capacity * 100;
			}
		}
	}

	/* get other stuff we might need to know */
	gpm_hal_device_get_string (udi, "info.product", &entry->product);
	gpm_hal_device_get_string (udi, "info.vendor", &entry->vendor);
	gpm_hal_device_get_string (udi, "battery.technology", &entry->technology);
	gpm_hal_device_get_string (udi, "battery.serial", &entry->serial);
	gpm_hal_device_get_string (udi, "battery.model", &entry->model);

	if (entry->battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY) {
		/* true as not reporting, but charge_level */
		entry->unit = GPM_POWER_UNIT_MWH;
	} else if (entry->battery_kind == GPM_POWER_BATTERY_KIND_UPS) {
		/* is this always correct? */
		entry->unit = GPM_POWER_UNIT_PERCENT;
	} else if (entry->battery_kind == GPM_POWER_BATTERY_KIND_MOUSE ||
		   entry->battery_kind == GPM_POWER_BATTERY_KIND_KEYBOARD) {
		entry->unit = GPM_POWER_UNIT_CSR;
	}
}

static void
battery_device_cache_entry_update_key (BatteryDeviceCacheEntry *entry,
				       const char	      *key)
{
	GpmPowerBatteryStatus *status = &entry->battery_status;
	char *udi = entry->udi;

	if (key == NULL) {
		return;
	}

	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		gpm_hal_device_get_bool (udi, key, &status->is_present);

		battery_device_cache_entry_update_all (entry);

	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		gpm_hal_device_get_bool (udi, key, &status->is_charging);

		 /* invalidate the remaining time, as we need to wait for
		    the next HAL update. This is a HAL bug I think. */
		status->remaining_time = 0;
	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		gpm_hal_device_get_bool (udi, key, &status->is_discharging);

		/* invalidate the remaining time */
		status->remaining_time = 0;
	} else if (strcmp (key, "battery.charge_level.design") == 0) {
		gpm_hal_device_get_int (udi, key, &status->design_charge);

	} else if (strcmp (key, "battery.charge_level.last_full") == 0) {
		gpm_hal_device_get_int (udi, key, &status->last_full_charge);

	} else if (strcmp (key, "battery.charge_level.current") == 0) {
		gpm_hal_device_get_int (udi, key, &status->current_charge);

	} else if (strcmp (key, "battery.charge_level.rate") == 0) {
		int charge_rate_new;
		gpm_hal_device_get_int (udi, key, &charge_rate_new);
		/* Do an exponentially weighted average for the rate so
		   that high frequency changes are smoothed. This should mean
		   the remaining_time does not change drastically between updates.
		   Fixes bug #328927 */
		if (entry->charge_rate_previous == 0) {
			/* startup, or re-initialization - we have no data */
			status->charge_rate = charge_rate_new;
		} else {
			status->charge_rate = ((1.0f - RATE_EXP_AVERAGE_FACTOR) * charge_rate_new) +
					       (RATE_EXP_AVERAGE_FACTOR * entry->charge_rate_previous);
		}
		entry->charge_rate_previous = charge_rate_new;

		/* FIXME: following can be removed if bug #5752 of hal on freedesktop
		   gets fixed and is part of a new release of HAL and we depend on that
		   version */
		if (status->charge_rate == 0) {
			status->is_discharging = FALSE;
			status->is_charging = FALSE;
		}
	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		gpm_hal_device_get_int (udi, key, &status->percentage_charge);

	} else if (strcmp (key, "battery.remaining_time") == 0) {
		gpm_hal_device_get_int (udi, key, &status->remaining_time);

	} else {
		/* ignore */
		return;
	}
}

static BatteryDeviceCacheEntry *
battery_device_cache_entry_new_from_udi (const char *udi)
{
	BatteryDeviceCacheEntry *entry;

	entry = g_new0 (BatteryDeviceCacheEntry, 1);

	entry->udi = g_strdup (udi);

	entry->product = NULL;
	entry->vendor = NULL;
	entry->technology = NULL;
	entry->serial = NULL;
	entry->model = NULL;
	entry->unit = GPM_POWER_UNIT_UNKNOWN;

	battery_device_cache_entry_update_all (entry);

	return entry;
}

static BatteryKindCacheEntry *
battery_kind_cache_entry_new_from_battery_kind (GpmPowerBatteryKind battery_kind)
{
	BatteryKindCacheEntry *entry;

	entry = g_new0 (BatteryKindCacheEntry, 1);

	entry->battery_kind = battery_kind;

	return entry;
}

static void
battery_kind_cache_entry_free (BatteryKindCacheEntry *entry)
{
	g_slist_foreach (entry->devices, (GFunc)g_free, NULL);
	g_slist_free (entry->devices);
	g_free (entry);
	entry = NULL;
}

const char *
battery_kind_to_string (GpmPowerBatteryKind battery_kind)
{
	const char *str;

	if (battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY) {
 		str = _("Laptop battery");
	} else if (battery_kind == GPM_POWER_BATTERY_KIND_UPS) {
 		str = _("UPS");
	} else if (battery_kind == GPM_POWER_BATTERY_KIND_MOUSE) {
 		str = _("Wireless mouse");
	} else if (battery_kind == GPM_POWER_BATTERY_KIND_KEYBOARD) {
 		str = _("Wireless keyboard");
	} else if (battery_kind == GPM_POWER_BATTERY_KIND_PDA) {
 		str = _("PDA");
 	} else {
 		str = _("Unknown");
	}
	return str;
}

static void
battery_kind_cache_debug_print (BatteryKindCacheEntry *entry)

{
	GpmPowerBatteryStatus *status = &entry->battery_status;
	gpm_debug ("Device : %s", battery_kind_to_string (entry->battery_kind));
	gpm_debug ("number     %i\tdesign     %i",
		   g_slist_length (entry->devices), status->design_charge);
	gpm_debug ("present    %i\tlast_full  %i",
		   status->is_present, status->last_full_charge);
	gpm_debug ("percent    %i\tcurrent    %i",
		   status->percentage_charge, status->current_charge);
	gpm_debug ("charge     %i\trate       %i", 
		   status->is_charging, status->charge_rate);
	gpm_debug ("discharge  %i\tremaining  %i", 
		   status->is_discharging, status->remaining_time);
	gpm_debug ("capacity   %i", 
		   status->capacity);
}

static void
debug_print_type_cache_iter (gpointer	       key,
			     BatteryKindCacheEntry *entry,
			     gpointer	      *user_data)
{
	battery_kind_cache_debug_print (entry);
}

static void
battery_kind_cache_debug_print_all (GpmPower *power)
{
	if (power->priv->battery_kind_cache != NULL) {
		g_hash_table_foreach (power->priv->battery_kind_cache,
				      (GHFunc)debug_print_type_cache_iter,
				      NULL);
	}
}

static BatteryDeviceCacheEntry *
battery_device_cache_find (GpmPower   *power,
			   const char *udi)
{
	BatteryDeviceCacheEntry *entry;

	if (! udi) {
		gpm_warning ("UDI is NULL");
		return NULL;
	}

	if (power->priv->battery_device_cache == NULL) {
		return NULL;
	}

	entry = g_hash_table_lookup (power->priv->battery_device_cache, udi);

	return entry;
}

static BatteryKindCacheEntry *
battery_kind_cache_find (GpmPower		*power,
			 GpmPowerBatteryKind	 battery_kind)
{
	BatteryKindCacheEntry *entry;

	if (power->priv->battery_kind_cache == NULL) {
		return NULL;
	}

	entry = g_hash_table_lookup (power->priv->battery_kind_cache, &battery_kind);

	return entry;
}

/** returns the number of devices of a specific kind */
gint
gpm_power_get_num_devices_of_kind (GpmPower		*power,
				   GpmPowerBatteryKind	 battery_kind)
{
	BatteryKindCacheEntry *entry;
	if (power->priv->battery_kind_cache == NULL) {
		return 0;
	}
	entry = g_hash_table_lookup (power->priv->battery_kind_cache, &battery_kind);
	if (entry == NULL) {
		return 0;
	}
	return (g_slist_length (entry->devices));
}

/** frees the custom array type */
void
gpm_power_free_description_array (GArray *array)
{
	int a;
	GpmPowerDescriptionItem *di;
	for (a=0; a<array->len; a++) {
		di = &g_array_index (array, GpmPowerDescriptionItem, a);
		g_free (di->title);
		g_free (di->value);
	}
	g_array_free (array, TRUE);
}

static const char *
get_power_unit_suffix (GpmPowerBatteryUnit unit)
{
	const char *suffix;
	if (unit == GPM_POWER_UNIT_MWH) {
		suffix = "mWh";
	} else if (unit == GPM_POWER_UNIT_PERCENT) {
		suffix = "%";
	} else if (unit == GPM_POWER_UNIT_CSR) {
		suffix = "/7";
	} else {
		suffix = "?";
	}
	return suffix;
}

/** returns in a custom array the device parameters */
GArray *
gpm_power_get_description_array (GpmPower		*power,
				 GpmPowerBatteryKind	 battery_kind,
				 gint			 device_num)
{
	const char	        *udi;
	const char		*suffix;
	BatteryDeviceCacheEntry *device;
	GpmPowerBatteryStatus	*status;
	GpmPowerDescriptionItem  di;

	GArray *array = g_array_new (FALSE, FALSE, sizeof (GpmPowerDescriptionItem));
	BatteryKindCacheEntry *entry;
	if (power->priv->battery_kind_cache == NULL) {
		return NULL;
	}
	entry = g_hash_table_lookup (power->priv->battery_kind_cache, &battery_kind);
	if (entry == NULL) {
		return NULL;
	}
	if (device_num > g_slist_length (entry->devices) - 1) {
		return NULL;
	}

	/* get the udi of the battery we are interested in */
	udi = (const char *) g_slist_nth_data (entry->devices, device_num);

	/* find the udi in the device cache */
	device = battery_device_cache_find (power, udi);
	if (device == NULL) {
		return NULL;
	}
	status = &device->battery_status;

	if (status->is_present == FALSE) {
		di.title = g_strdup (_("Status:"));
		di.value = g_strdup (_("Missing"));
		g_array_append_vals (array, &di, 1);
		return array;
	}
	if (gpm_power_battery_is_charged (status)) {
		di.title = g_strdup (_("Status:"));
		di.value = g_strdup (_("Charged"));
		g_array_append_vals (array, &di, 1);
	} else if (status->is_charging) {
		di.title = g_strdup (_("Status:"));
		di.value = g_strdup (_("Charging"));
		g_array_append_vals (array, &di, 1);
	} else if (status->is_discharging) {
		di.title = g_strdup (_("Status:"));
		di.value = g_strdup (_("Discharging"));
		g_array_append_vals (array, &di, 1);
	}
	if (device->product) {
		di.title = g_strdup (_("Product:"));
		di.value = g_strdup (device->product);
		g_array_append_vals (array, &di, 1);
	}
	if (device->vendor) {
		di.title = g_strdup (_("Vendor:"));
		di.value = g_strdup (device->vendor);
		g_array_append_vals (array, &di, 1);
	}
	if (device->technology) {
		di.title = g_strdup (_("Technology:"));
		const char *technology;
		if (g_ascii_strcasecmp (device->technology, "li-ion") == 0) {
			technology = _("Lithium ion");
		} else if (g_ascii_strcasecmp (device->technology, "pbac") == 0) {
			technology = _("Lead acid");
		} else {
			gpm_warning ("Battery type %s not translated, please report!",
				     device->technology);
			technology = device->technology;
		}
		di.value = g_strdup (technology);
		g_array_append_vals (array, &di, 1);
	}
	if (device->serial) {
		di.title = g_strdup (_("Serial number:"));
		di.value = g_strdup (device->serial);
		g_array_append_vals (array, &di, 1);
	}
	if (device->model) {
		di.title = g_strdup (_("Model:"));
		di.value = g_strdup (device->model);
		g_array_append_vals (array, &di, 1);
	}
	if (status->remaining_time > 0) {
		di.title = g_strdup (_("Remaining time:"));
		di.value = gpm_get_timestring (status->remaining_time);
		g_array_append_vals (array, &di, 1);
	}
	if (status->percentage_charge > 0) {
		di.title = g_strdup (_("Percentage charge:"));
		di.value = g_strdup_printf ("%i%%", status->percentage_charge);
		g_array_append_vals (array, &di, 1);
	}
	if (status->capacity > 0) {
		const char *condition;
		di.title = g_strdup (_("Capacity:"));
		if (status->capacity > 99) {
			condition = _("Very good condition");
		} else if (status->capacity > 90) {
			condition = _("Good condition");
		} else {
			condition = _("Poor condition");
		}
		di.value = g_strdup_printf ("%i%% (%s)", status->capacity, condition);
		g_array_append_vals (array, &di, 1);
	}
	if (device->unit != GPM_POWER_UNIT_PERCENT) {
		/* no point displaying these if we are measuring in percent */
		suffix = get_power_unit_suffix (device->unit);
		if (status->current_charge > 0) {
			di.title = g_strdup (_("Current charge:"));
			di.value = g_strdup_printf ("%i%s", status->current_charge, suffix);
			g_array_append_vals (array, &di, 1);
		}
		if (status->last_full_charge > 0 &&
		    status->design_charge != status->last_full_charge) {
			di.title = g_strdup (_("Last full charge:"));
			di.value = g_strdup_printf ("%i%s", status->last_full_charge, suffix);
			g_array_append_vals (array, &di, 1);
		}
		if (status->design_charge > 0) {
			di.title = g_strdup (_("Design charge:"));
			di.value = g_strdup_printf ("%i%s", status->design_charge, suffix);
			g_array_append_vals (array, &di, 1);
		}
		if (status->charge_rate > 0) {
			di.title = g_strdup (_("Charge rate:"));
			di.value = g_strdup_printf ("%imWh", status->charge_rate);
			g_array_append_vals (array, &di, 1);
		}
	}

	return array;
}

static void
battery_kind_cache_update (GpmPower	         *power,
			   BatteryKindCacheEntry *entry)
{
	GSList *l;
	int     num_present = 0;
	int     num_discharging = 0;
	GpmPowerBatteryStatus *type_status = &entry->battery_status;

	/* clear old values */
	gpm_power_battery_status_set_defaults (type_status);

	/* iterate thru all the devices to handle multiple batteries */
	for (l = entry->devices; l; l = l->next) {
		const char	      *udi;
		BatteryDeviceCacheEntry *device;
		GpmPowerBatteryStatus	*device_status;

		udi = (const char *)l->data;

		device = battery_device_cache_find (power, udi);
		device_status = &device->battery_status;

		if (! device_status->is_present) {
			continue;
		}

		num_present++;

		/* Only one device has to be present for the class to
		 * be present. */
		type_status->is_present = TRUE;

		if (device_status->is_charging) {
			type_status->is_charging = TRUE;
		}

		if (device_status->is_discharging) {
			type_status->is_discharging = TRUE;
			num_discharging++;
		}

		type_status->design_charge += device_status->design_charge;
		type_status->last_full_charge += device_status->last_full_charge;
		type_status->current_charge += device_status->current_charge;
		type_status->charge_rate += device_status->charge_rate;
		/* we have to sum this here, in case the device has no rate
		   data, and we can't compute it further down */
		type_status->remaining_time += device_status->remaining_time;
	}

	/* sanity check */
	if (type_status->is_discharging && type_status->is_charging) {
		gpm_warning ("Sanity check kicked in! "
			     "Multiple device object cannot be charging and "
			     "discharging simultaneously!");
		type_status->is_charging = FALSE;
	}

	gpm_debug ("%i devices of type %s", num_present, battery_kind_to_string (entry->battery_kind));

	/* Perform following calculations with floating point otherwise we might
	 * get an with batteries which have a very small charge unit and consequently
	 * a very high charge. Fixes bug #327471 */
	if (type_status->is_present) {
		int pc = 100 * ((float)type_status->current_charge /
				(float)type_status->last_full_charge);
		if (pc < 0) {
			gpm_warning ("Corrected percentage charge (%i) and set to minimum", pc);
			pc = 0;
		} else if (pc > 100) {
			gpm_warning ("Corrected percentage charge (%i) and set to maximum", pc);
			pc = 100;
		}
		type_status->percentage_charge = pc;
	}
	/* We only do the "better" remaining time algorithm if the battery has rate,
	   i.e not a UPS, which gives it's own battery.remaining_time but has no rate */
	if (type_status->charge_rate > 0) {
		if (type_status->is_discharging) {
			type_status->remaining_time = 3600 * ((float)type_status->current_charge /
							      (float)type_status->charge_rate);
		} else if (type_status->is_charging) {
			type_status->remaining_time = 3600 *
				((float)(type_status->last_full_charge - type_status->current_charge) /
				(float)type_status->charge_rate);
		}
	}
	/* Check the remaining time is under a set limit, to deal with broken
	   primary batteries. Fixes bug #328927 */
	if (type_status->remaining_time > (100 * 60 * 60)) {
		gpm_warning ("Another sanity check kicked in! "
			     "Remaining time cannot be > 100 hours!");
		type_status->remaining_time = 0;
	}

	gpm_debug ("emitting battery-status-changed : %s",
		   battery_kind_to_string (entry->battery_kind));
	g_signal_emit (power, signals [BATTERY_STATUS_CHANGED], 0, entry->battery_kind);
}

static void
battery_kind_update_cache_iter (const char	      *key,
				BatteryKindCacheEntry *entry,
				GpmPower	      *power)
{
	battery_kind_cache_update (power, entry);
}

static void
battery_kind_cache_update_all (GpmPower *power)
{
	gpm_debug ("Updating all device types");

	if (power->priv->battery_kind_cache != NULL) {
		g_hash_table_foreach (power->priv->battery_kind_cache,
				      (GHFunc)battery_kind_update_cache_iter,
				      power);
	}
}

static void
battery_device_cache_add_device (GpmPower		 *power,
				 BatteryDeviceCacheEntry *entry)
{
	g_hash_table_insert (power->priv->battery_device_cache,
			     g_strdup (entry->udi),
			     entry);
}

static void
battery_device_cache_remove_device (GpmPower		    *power,
				    BatteryDeviceCacheEntry *entry)
{
	g_hash_table_remove (power->priv->battery_device_cache,
			     entry->udi);
	g_free (entry->udi);
	g_free (entry->product);
	g_free (entry->vendor);
	g_free (entry->technology);
	g_free (entry->serial);
	g_free (entry->model);
}

static void
battery_kind_cache_add_device (GpmPower			*power,
			       BatteryDeviceCacheEntry	*device_entry)
{
	BatteryKindCacheEntry *type_entry;

	if (! device_entry->battery_status.is_present) {
		gpm_debug ("Adding missing device");
	}

	type_entry = battery_kind_cache_find (power,
					      device_entry->battery_kind);
	if (type_entry == NULL) {
		type_entry = battery_kind_cache_entry_new_from_battery_kind (device_entry->battery_kind);
		g_hash_table_insert (power->priv->battery_kind_cache,
				     &device_entry->battery_kind,
				     type_entry);
	}

	/* assume that it isn't in there already */
	type_entry->devices = g_slist_prepend (type_entry->devices, device_entry->udi);

	battery_kind_cache_update (power, type_entry);
}

static void
battery_kind_cache_remove_device (GpmPower		  *power,
				  BatteryDeviceCacheEntry *entry)
{
	BatteryKindCacheEntry *type_entry;

	type_entry = battery_kind_cache_find (power,
					      entry->battery_kind);
	if (type_entry == NULL) {
		return;
	}

	type_entry->devices = g_slist_remove_all (type_entry->devices, entry->udi);

	/* if we've removed the last device then remove from the hash */
	if (type_entry->devices == NULL) {
		g_hash_table_remove (power->priv->battery_kind_cache,
				     &entry->battery_kind);
	} else {
		battery_kind_cache_update (power, type_entry);
	}
}

static void
power_get_summary_for_battery_kind (GpmPower		*power,
			    	    GpmPowerBatteryKind  battery_kind,
			    	    GString		*summary)
{
	BatteryKindCacheEntry *entry;
	GpmPowerBatteryStatus *status;
	const char	    *type_desc = NULL;
	char		  *timestring;

	entry = battery_kind_cache_find (power, battery_kind);

	if (entry == NULL) {
		return;
	}

	status = &entry->battery_status;

	if (! status->is_present) {
		return;
	}

	type_desc = battery_kind_to_string (entry->battery_kind);

	/* don't display all the extra stuff for keyboards and mice */
	if (entry->battery_kind == GPM_POWER_BATTERY_KIND_MOUSE
	    || entry->battery_kind == GPM_POWER_BATTERY_KIND_KEYBOARD
	    || entry->battery_kind == GPM_POWER_BATTERY_KIND_PDA) {

		g_string_append_printf (summary, "%s (%i%%)\n", type_desc,
					status->percentage_charge);
		return;
	}

	timestring = gpm_get_timestring (status->remaining_time);

	/* We always display "Laptop Battery 16 minutes remaining" as we need
	   to clarify what device we are refering to. For details see :
	   http://bugzilla.gnome.org/show_bug.cgi?id=329027 */
	g_string_append_printf (summary, "%s ", type_desc);

	if (gpm_power_battery_is_charged (status)) {

			g_string_append_printf (summary, "%s", _("fully charged"));

	} else if (status->is_discharging) {

		if (status->remaining_time > 60) {
			g_string_append_printf (summary, "%s %s",
						timestring, _("remaining"));
		} else {
			/* don't display "Unknown remaining" */
			g_string_append_printf (summary, "%s", _("discharging"));
		}

	} else if (status->is_charging || power->priv->on_ac) {

		if (status->remaining_time > 60) {
			g_string_append_printf (summary, "%s %s",
						timestring, _("until charged"));
		} else {
			/* don't display "Unknown remaining" */
			g_string_append_printf (summary, "%s", _("charging"));
		}

	} else {
		gpm_warning ("in an undefined state we are not charging or "
			     "discharging and the batteries are also not charged");
	}
	
	/* append percentage to all devices */
	g_string_append_printf (summary, " (%i%%)\n", status->percentage_charge);

	g_free (timestring);
}

gboolean
gpm_power_get_status_summary (GpmPower *power,
			      char    **string,
			      GError  **error)
{
	GString *summary = NULL;
	gboolean ups_present;
	GpmPowerBatteryStatus status;

	if (! string) {
		return FALSE;
	}

	ups_present = gpm_power_get_battery_status (power,
						    GPM_POWER_BATTERY_KIND_UPS,
						    &status);

	if (ups_present && status.is_discharging) {
		/* only enable this if discharging on UPS, for details see:
		   http://bugzilla.gnome.org/show_bug.cgi?id=329027 */
		summary = g_string_new (_("Computer is running on backup power\n"));

	} else if (power->priv->on_ac) {
		summary = g_string_new (_("Computer is running on AC power\n"));

	} else {
		summary = g_string_new (_("Computer is running on battery power\n"));
	}

	/* do each device type we know about, in the correct visual order */
	power_get_summary_for_battery_kind (power, GPM_POWER_BATTERY_KIND_PRIMARY, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_BATTERY_KIND_UPS, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_BATTERY_KIND_MOUSE, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_BATTERY_KIND_KEYBOARD, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_BATTERY_KIND_PDA, summary);

	/* remove the last \n */
	g_string_truncate (summary, summary->len-1);

	gpm_debug ("tooltip: %s", summary->str);

	*string = g_string_free (summary, FALSE);

	return TRUE;
}

/* returns if the device was found in the cache */
gboolean
gpm_power_get_battery_status (GpmPower			 *power,
			      GpmPowerBatteryKind	 battery_kind,
			      GpmPowerBatteryStatus      *battery_status)
{
	BatteryKindCacheEntry *entry;

	g_return_val_if_fail (GPM_IS_POWER (power), FALSE);

	/* Make sure we at least return the defaults */
	gpm_power_battery_status_set_defaults (battery_status);

	entry = battery_kind_cache_find (power, battery_kind);
	if (entry == NULL) {
		return FALSE;
	}
	*battery_status = entry->battery_status;

	return TRUE;
}

static gboolean
gpm_power_set_on_ac (GpmPower *power,
		     gboolean  on_ac,
		     GError  **error)
{
	g_return_val_if_fail (GPM_IS_POWER (power), FALSE);

	if (on_ac != power->priv->on_ac) {
		power->priv->on_ac = on_ac;

		gpm_debug ("emitting ac-state-changed : %i", on_ac);
		g_signal_emit (power, signals [AC_STATE_CHANGED], 0, on_ac);
	}

	return TRUE;
}

gboolean
gpm_power_get_on_ac (GpmPower *power,
		     gboolean *on_ac,
		     GError  **error)
{
	g_return_val_if_fail (GPM_IS_POWER (power), FALSE);

	if (on_ac) {
		*on_ac = power->priv->on_ac;
	}

	return TRUE;
}

static void
gpm_power_set_property (GObject	     *object,
			guint	      prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
	GpmPower *power;

	power = GPM_POWER (object);

	switch (prop_id) {
	case PROP_ON_AC:
		gpm_power_set_on_ac (power, g_value_get_boolean (value), NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_power_get_property (GObject    *object,
			guint       prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
	GpmPower *power;

	power = GPM_POWER (object);

	switch (prop_id) {
	case PROP_ON_AC:
		g_value_set_boolean (value, power->priv->on_ac);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_power_class_init (GpmPowerClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize	   = gpm_power_finalize;
	object_class->get_property = gpm_power_get_property;
	object_class->set_property = gpm_power_set_property;

	g_object_class_install_property (object_class,
					 PROP_ON_AC,
					 g_param_spec_boolean ("on_ac",
							       NULL,
							       NULL,
							       TRUE,
							       G_PARAM_READWRITE));

	signals [BUTTON_PRESSED] =
		g_signal_new ("button-pressed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPowerClass, button_pressed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals [AC_STATE_CHANGED] =
		g_signal_new ("ac-power-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPowerClass, ac_state_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);
	signals [BATTERY_STATUS_CHANGED] =
		g_signal_new ("battery-status-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPowerClass, battery_status_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1, G_TYPE_INT);
	signals [BATTERY_REMOVED] =
		g_signal_new ("battery-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPowerClass, battery_removed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GpmPowerPrivate));
}

static void
hal_on_ac_changed_cb (GpmHalMonitor *monitor,
		      gboolean       on_ac,
		      GpmPower      *power)
{
	gpm_power_set_on_ac (power, on_ac, NULL);

	if (! on_ac) {
		battery_kind_cache_update_all (power);
	}
}

static gboolean
add_battery (GpmPower   *power,
	     const char *udi)
{
	BatteryDeviceCacheEntry *entry;
	GpmPowerBatteryStatus *status;

	gpm_debug ("adding %s", udi);

	g_assert (udi);

	entry = battery_device_cache_entry_new_from_udi (udi);

	battery_device_cache_add_device (power, entry);
	battery_kind_cache_add_device (power, entry);

	status = &entry->battery_status;
	/*
	 * We should notify the user if the battery has a low capacity,
	 * where capacity is the ratio of the last_full capacity with that of
	 * the design capacity. (#326740)
	 */
	if (entry->battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY) {
		if (status->capacity > 0 && status->capacity < 0.5f) {
			gpm_warning ("Your battery has a very low capacity, "
				     "meaning that it may be old or broken. "
				     "Battery life will be sub-optimal, "
				     "and the time remaining may be incorrect.");
		}
	}
	return TRUE;
}

static gboolean
remove_battery (GpmPower   *power,
		const char *udi)
{
	BatteryDeviceCacheEntry *entry;

	gpm_debug ("removing %s", udi);

	g_assert (udi);

	entry = battery_device_cache_find (power,
					   udi);
	if (entry == NULL) {
		return FALSE;
	}

	battery_kind_cache_remove_device (power, entry);
	battery_device_cache_remove_device (power, entry);

	return TRUE;
}

static void
hal_battery_added_cb (GpmHalMonitor *monitor,
		      const char    *udi,
		      GpmPower      *power)
{
	gpm_debug ("Battery Added: %s", udi);
	add_battery (power, udi);

	battery_kind_cache_debug_print_all (power);
}

static void
hal_battery_removed_cb (GpmHalMonitor *monitor,
			const char    *udi,
			GpmPower      *power)
{
	gpm_debug ("Battery Removed: %s", udi);

	remove_battery (power, udi);

	battery_kind_cache_debug_print_all (power);

	/* proxy it */
	gpm_debug ("emitting battery-removed : %s", udi);
	g_signal_emit (power, signals [BATTERY_REMOVED], 0, udi);
}

static void
hal_battery_property_modified_cb (GpmHalMonitor *monitor,
				  const char    *udi,
				  const char    *key,
				  GpmPower      *power)
{
	BatteryDeviceCacheEntry *device_entry;
	BatteryKindCacheEntry   *type_entry;

	gpm_debug ("Battery Property Modified: %s", udi);

	device_entry = battery_device_cache_find (power, udi);

	/*
	 * if we BUG here then *HAL* has a problem where key modification is
	 * done before capability is present
	 */
	if (device_entry == NULL) {
		gpm_warning ("device cache entry is NULL! udi=%s\n"
			     "This is probably a bug in HAL where we are getting "
			     "is_removed=false, is_added=false before the capability "
			     "had been added.",
			     udi);
		return;
	}

	battery_device_cache_entry_update_key (device_entry, key);

	type_entry = battery_kind_cache_find (power, device_entry->battery_kind);

	if (type_entry == NULL) {
		gpm_warning ("battery type cache entry not found for modified device");
		return;
	}

	battery_kind_cache_update (power, type_entry);

	battery_kind_cache_debug_print (type_entry);
}

static void
hal_button_pressed_cb (GpmHalMonitor *monitor,
		       const char    *type,
		       gboolean       state,
		       GpmPower      *power)
{
	/* just proxy it */
	gpm_debug ("emitting button-pressed : %s (%i)", type, state);
	g_signal_emit (power, signals [BUTTON_PRESSED], 0, type, state);
}

/* FIXME: there must be a better way to do this */
static gboolean
gpm_hash_remove_return (gpointer key,
			gpointer value,
			gpointer user_data)
{
	return TRUE;
}

static void
gpm_hash_new_kind_cache (GpmPower *power)
{
	if (power->priv->battery_kind_cache) {
		return;
	}
	gpm_debug ("creating cache");
	power->priv->battery_kind_cache = g_hash_table_new_full (g_int_hash,
							 	 g_int_equal,
							 	 NULL,
							 	 (GDestroyNotify)battery_kind_cache_entry_free);
}

static void
gpm_hash_free_kind_cache (GpmPower *power)
{
	if (! power->priv->battery_kind_cache) {
		return;
	}
	gpm_debug ("freeing cache");
	g_hash_table_foreach_remove (power->priv->battery_kind_cache,
				     gpm_hash_remove_return, NULL);
	g_hash_table_destroy (power->priv->battery_kind_cache);
	power->priv->battery_kind_cache = NULL;
}

static void
gpm_hash_new_device_cache (GpmPower *power)
{
	if (power->priv->battery_device_cache) {
		return;
	}
	gpm_debug ("creating cache");
	power->priv->battery_device_cache = g_hash_table_new_full (g_str_hash,
							   	   g_str_equal,
								   g_free,
								   NULL);
}

static void
gpm_hash_free_device_cache (GpmPower *power)
{
	if (! power->priv->battery_device_cache) {
		return;
	}
	gpm_debug ("freeing cache");
	g_hash_table_foreach_remove (power->priv->battery_device_cache,
				     gpm_hash_remove_return, NULL);
	g_hash_table_destroy (power->priv->battery_device_cache);
	power->priv->battery_device_cache = NULL;
}

static void
hal_disconnected_cb (GpmHalMonitor *monitor,
		     GpmPower      *power)
{
	/* We have to clear the caches, else the devices think they are
	   initialised, and we segfault in various places. */
	gpm_hash_free_kind_cache (power);
	gpm_hash_new_kind_cache (power);
	gpm_hash_free_device_cache (power);
	gpm_hash_new_device_cache (power);
}

static void
gpm_power_init (GpmPower *power)
{
	gboolean on_ac;

	power->priv = GPM_POWER_GET_PRIVATE (power);

	power->priv->hal_monitor = gpm_hal_monitor_new ();
	g_signal_connect (power->priv->hal_monitor, "button-pressed",
			  G_CALLBACK (hal_button_pressed_cb), power);
	g_signal_connect (power->priv->hal_monitor, "ac-power-changed",
			  G_CALLBACK (hal_on_ac_changed_cb), power);
	g_signal_connect (power->priv->hal_monitor, "battery-property-modified",
			  G_CALLBACK (hal_battery_property_modified_cb), power);
	g_signal_connect (power->priv->hal_monitor, "battery-added",
			  G_CALLBACK (hal_battery_added_cb), power);
	g_signal_connect (power->priv->hal_monitor, "battery-removed",
			  G_CALLBACK (hal_battery_removed_cb), power);
	g_signal_connect (power->priv->hal_monitor, "hal-disconnected",
			  G_CALLBACK (hal_disconnected_cb), power);

	power->priv->battery_kind_cache = NULL;
	power->priv->battery_device_cache = NULL;

	gpm_hash_new_kind_cache (power);
	gpm_hash_new_device_cache (power);

	on_ac = gpm_hal_monitor_get_on_ac (power->priv->hal_monitor);
	gpm_power_set_on_ac (power, on_ac, NULL);
}

static void
gpm_power_finalize (GObject *object)
{
	GpmPower *power;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_POWER (object));

	power = GPM_POWER (object);

	g_return_if_fail (power->priv != NULL);

	if (power->priv->hal_monitor != NULL) {
		g_object_unref (power->priv->hal_monitor);
	}

	gpm_hash_free_kind_cache (power);
	gpm_hash_free_device_cache (power);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmPower *
gpm_power_new (void)
{
	GpmPower *power;

	power = g_object_new (GPM_TYPE_POWER, NULL);

	return GPM_POWER (power);
}
