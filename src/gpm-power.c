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
#include <gconf/gconf-client.h>

#include "gpm-common.h"
#include "gpm-hal.h"
#include "gpm-hal-monitor.h"

#include "gpm-power.h"
#include "gpm-marshal.h"
#include "gpm-debug.h"
#include "gpm-prefs.h"

static void     gpm_power_class_init (GpmPowerClass *klass);
static void     gpm_power_init       (GpmPower      *power);
static void     gpm_power_finalize   (GObject       *object);

#define GPM_POWER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POWER, GpmPowerPrivate))

#define GPM_POWER_MIN_CHARGED_PERCENTAGE	90

struct GpmPowerPrivate
{
	gboolean		 on_ac;
	int			 exp_ave_factor;
	GHashTable		*battery_kind_cache;
	GHashTable		*battery_device_cache;
	GpmHal			*hal;
	GpmHalMonitor		*hal_monitor;
};

enum {
	BUTTON_PRESSED,
	AC_STATE_CHANGED,
	BATTERY_STATUS_CHANGED,
	BATTERY_REMOVED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_ON_AC
};

static guint	     signals [LAST_SIGNAL] = { 0, };
static gpointer      gpm_power_object = NULL;

G_DEFINE_TYPE (GpmPower, gpm_power, G_TYPE_OBJECT)

/**
 * Multiple batteries percentages are averaged and times added
 * so that a virtual device is presented to the program. This is
 * required as we show the icons and do the events as averaged
 * over all battery devices of the same type.
 */
typedef struct {
	GpmPowerKind	 battery_kind;
	GpmPowerStatus	 battery_status;
	/* List of device udis */
	GSList		*devices;
} BatteryKindCacheEntry;

/**
 * gpm_power_battery_status_set_defaults:
 * @status: A battery info structure
 **/
static void
gpm_power_battery_status_set_defaults (GpmPowerStatus *status)
{
	/* initialise to known defaults */
	status->design_charge = 0;
	status->last_full_charge = 0;
	status->current_charge = 0;
	status->charge_rate_smoothed = 0;
	status->charge_rate_raw = 0;
	status->percentage_charge = 0;
	status->remaining_time = 0;
	status->capacity = 0;
	status->is_rechargeable = FALSE;
	status->is_present = FALSE;
	status->is_charging = FALSE;
	status->is_discharging = FALSE;
}

/**
 * gpm_power_battery_is_charged:
 * @status: A battery info structure
 *
 * We have to be clever here, as there are lots of broken batteries.
 * Some batteries have a reduced charge capacity and cannot charge to
 * 100% anymore (although they *should* update thier own last_full
 * values, it appears most do not...)
 * Return value: If the battery is considered "charged".
 **/
gboolean
gpm_power_battery_is_charged (GpmPowerStatus *status)
{
	if (! status->is_charging &&
	    ! status->is_discharging &&
	    status->percentage_charge > GPM_POWER_MIN_CHARGED_PERCENTAGE) {
		return TRUE;
	}
	return FALSE;
}

/**
 * battery_device_cache_entry_update_all:
 * @entry: A device cache instance
 *
 * Updates all the information fields in a cache entry
 **/
static void
battery_device_cache_entry_update_all (GpmPower *power, GpmPowerDevice *entry)
{
	gboolean exists;
	GpmPowerStatus *status = &entry->battery_status;
	char *udi = entry->udi;
	char *battery_kind_str;

	/* invalidate last rate */
	entry->charge_rate_previous = 0;

	/* Initialize battery_status to reasonable defaults */
	gpm_power_battery_status_set_defaults (status);

	gpm_hal_device_get_string (power->priv->hal, udi, "battery.type", &battery_kind_str);

	if (!battery_kind_str) {
		gpm_warning ("cannot obtain battery type");
		return;
	}
	if (strcmp (battery_kind_str, "primary") == 0) {
		entry->battery_kind = GPM_POWER_KIND_PRIMARY;
	} else if (strcmp (battery_kind_str, "ups") == 0) {
		entry->battery_kind = GPM_POWER_KIND_UPS;
	} else if (strcmp (battery_kind_str, "keyboard") == 0) {
		entry->battery_kind = GPM_POWER_KIND_KEYBOARD;
	} else if (strcmp (battery_kind_str, "mouse") == 0) {
		entry->battery_kind = GPM_POWER_KIND_MOUSE;
	} else if (strcmp (battery_kind_str, "pda") == 0) {
		entry->battery_kind = GPM_POWER_KIND_PDA;
	} else {
		gpm_warning ("battery type %s unknown",
			   battery_kind_str);
		g_free (battery_kind_str);
		return;
	}
	g_free (battery_kind_str);

	/* batteries might be missing */
	gpm_hal_device_get_bool (power->priv->hal, udi, "battery.present", &status->is_present);
	if (! status->is_present) {
		gpm_debug ("Battery not present, so not filling up values");
		return;
	}

	gpm_hal_device_get_int (power->priv->hal, udi, "battery.charge_level.design",
				&status->design_charge);
	gpm_hal_device_get_int (power->priv->hal, udi, "battery.charge_level.last_full",
				&status->last_full_charge);
	gpm_hal_device_get_int (power->priv->hal, udi, "battery.charge_level.current",
				&status->current_charge);

	/* battery might not be rechargeable, have to check */
	gpm_hal_device_get_bool (power->priv->hal, udi, "battery.is_rechargeable",
				&status->is_rechargeable);

	if (entry->battery_kind == GPM_POWER_KIND_PRIMARY ||
	    entry->battery_kind == GPM_POWER_KIND_UPS) {
		if (status->is_rechargeable) {
			gpm_hal_device_get_bool (power->priv->hal, udi, "battery.rechargeable.is_charging",
						&status->is_charging);
			gpm_hal_device_get_bool (power->priv->hal, udi, "battery.rechargeable.is_discharging",
						&status->is_discharging);
		}
	}

	/* sanity check that charge_level.rate exists (if it should) */
	if (entry->battery_kind == GPM_POWER_KIND_PRIMARY) {
		exists = gpm_hal_device_get_int (power->priv->hal, udi, "battery.charge_level.rate",
						 &status->charge_rate_raw);
		if (!exists && (status->is_discharging || status->is_charging)) {
			gpm_warning ("could not read your battery's charge rate");
		}
		if (exists) {
			status->charge_rate_smoothed = status->charge_rate_raw;
		}
		/* FIXME: following can be removed if bug #5752 of hal on freedesktop
		   gets fixed and is part of a new release of HAL and we depend on that
		   version*/
		if (exists && status->charge_rate_raw == 0) {
			status->is_discharging = FALSE;
			status->is_charging = FALSE;
		}
	}

	/* sanity check that charge_level.percentage exists (if it should) */
	exists = gpm_hal_device_get_int (power->priv->hal, udi, "battery.charge_level.percentage",
					 &status->percentage_charge);
	if (!exists && (status->is_discharging || status->is_charging)) {
		gpm_warning ("could not read your battery's percentage charge.");
	}

	/* sanity check that remaining time exists (if it should) */
	if (entry->battery_kind == GPM_POWER_KIND_PRIMARY ||
	    entry->battery_kind == GPM_POWER_KIND_UPS) {
		exists = gpm_hal_device_get_int (power->priv->hal, udi,"battery.remaining_time",
						 &status->remaining_time);
		if (! exists && (status->is_discharging || status->is_charging)) {
			gpm_warning ("could not read your battery's remaining time");
		}
	}

	/* calculate the batteries capacity if it is primary and present */
	if (entry->battery_kind == GPM_POWER_KIND_PRIMARY && status->is_present) {
		if (status->design_charge > 0 && status->last_full_charge > 0) {
			if (status->design_charge != status->last_full_charge) {
				float capacity;
				capacity = 100.0f / (float) status->design_charge;
				status->capacity = capacity * (float) status->last_full_charge;
				if (status->capacity > 100) {
					gpm_debug ("rounding down capactity from "
						   "%i to 100", status->capacity);
					status->capacity = 100;
				}
			}
		}
	}

	/* get other stuff we might need to know */
	gpm_hal_device_get_string (power->priv->hal, udi, "info.product", &entry->product);
	gpm_hal_device_get_string (power->priv->hal, udi, "info.vendor", &entry->vendor);
	gpm_hal_device_get_string (power->priv->hal, udi, "battery.technology", &entry->technology);
	gpm_hal_device_get_string (power->priv->hal, udi, "battery.serial", &entry->serial);
	gpm_hal_device_get_string (power->priv->hal, udi, "battery.model", &entry->model);

	if (entry->battery_kind == GPM_POWER_KIND_PRIMARY) {
		/* true as not reporting, but charge_level */
		entry->unit = GPM_POWER_UNIT_MWH;
	} else if (entry->battery_kind == GPM_POWER_KIND_UPS) {
		/* is this always correct? */
		entry->unit = GPM_POWER_UNIT_PERCENT;
	} else if (entry->battery_kind == GPM_POWER_KIND_MOUSE ||
		   entry->battery_kind == GPM_POWER_KIND_KEYBOARD) {
		entry->unit = GPM_POWER_UNIT_CSR;
	}
}

/**
 * gpm_power_exp_aver:
 * @previous: The old value
 * @new: The new value
 * @factor_pc: The factor as a percentage
 *
 * We should do an exponentially weighted average so that high frequency
 * changes are smoothed. This should mean the rate (and thus the remaining time)
 * does not change drastically between updates.
 **/
static int
gpm_power_exp_aver (int previous, int new, int factor_pc)
{
	int result = 0;
	float factor = 0;
	float factor_inv = 1;
	if (previous == 0 || factor_pc == 0) {
		/* startup, or re-initialization - we have no data */
		gpm_debug ("Telling rate with no ave factor (okay once)");
		result = new;
	} else {
		factor = (float) factor_pc / 100.0f;
		factor_inv = 1.0f - factor;
		result = (int) ((factor_inv * (float) new) + (factor * (float) previous));
	}
	gpm_debug ("factor = %f, previous = %i, new=%i, result = %i",
		   factor, previous, new, result);
	return result;
}

/**
 * battery_device_cache_entry_update_key:
 * @power: This power class instance
 * @entry: A device cache instance
 * @key: the HAL key name, e.g. battery.rechargeable.is_charging
 *
 * Updates the device object with the new information given to us from HAL.
 **/
static void
battery_device_cache_entry_update_key (GpmPower		       *power,
				       GpmPowerDevice *entry,
				       const char	       *key)
{
	GpmPowerStatus *status = &entry->battery_status;
	char *udi = entry->udi;

	if (key == NULL) {
		return;
	}

	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		gpm_hal_device_get_bool (power->priv->hal, udi, key, &status->is_present);
		battery_device_cache_entry_update_all (power, entry);

	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		gpm_hal_device_get_bool (power->priv->hal, udi, key, &status->is_charging);
		status->charge_rate_smoothed = 0;

	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		gpm_hal_device_get_bool (power->priv->hal, udi, key, &status->is_discharging);
		status->charge_rate_smoothed = 0;

	} else if (strcmp (key, "battery.charge_level.design") == 0) {
		gpm_hal_device_get_int (power->priv->hal, udi, key, &status->design_charge);

	} else if (strcmp (key, "battery.charge_level.last_full") == 0) {
		gpm_hal_device_get_int (power->priv->hal, udi, key, &status->last_full_charge);

	} else if (strcmp (key, "battery.charge_level.current") == 0) {
		gpm_hal_device_get_int (power->priv->hal, udi, key, &status->current_charge);

	} else if (strcmp (key, "battery.charge_level.rate") == 0) {
		gpm_hal_device_get_int (power->priv->hal, udi, key, &status->charge_rate_raw);

		/* Do an exponentially weighted average, fixes bug #328927 */
		status->charge_rate_smoothed = gpm_power_exp_aver (entry->charge_rate_previous,
							status->charge_rate_raw,
							power->priv->exp_ave_factor);
		entry->charge_rate_previous = status->charge_rate_smoothed;

		/* FIXME: following can be removed if bug #5752 of hal on freedesktop
		   gets fixed and is part of a new release of HAL and we depend on that
		   version */
		if (status->charge_rate_raw == 0) {
			status->is_discharging = FALSE;
			status->is_charging = FALSE;
		}
	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		gpm_hal_device_get_int (power->priv->hal, udi, key, &status->percentage_charge);

	} else if (strcmp (key, "battery.remaining_time") == 0) {
		gpm_hal_device_get_int (power->priv->hal, udi, key, &status->remaining_time);

	} else {
		/* ignore */
		return;
	}
}

/**
 * battery_device_cache_entry_new_from_udi:
 * @udi: The HAL UDI for this device
 *
 * Creates a new device object and populates the values.
 * Return value: The new device cache entry.
 **/
static GpmPowerDevice *
battery_device_cache_entry_new_from_udi (GpmPower *power, const char *udi)
{
	GpmPowerDevice *entry;

	entry = g_new0 (GpmPowerDevice, 1);

	entry->udi = g_strdup (udi);

	entry->product = NULL;
	entry->vendor = NULL;
	entry->technology = NULL;
	entry->serial = NULL;
	entry->model = NULL;
	entry->unit = GPM_POWER_UNIT_UNKNOWN;

	battery_device_cache_entry_update_all (power, entry);

	return entry;
}

/**
 * battery_kind_cache_entry_new_from_battery_kind:
 * @power: This power class instance
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 **/
static BatteryKindCacheEntry *
battery_kind_cache_entry_new_from_battery_kind (GpmPowerKind battery_kind)
{
	BatteryKindCacheEntry *entry;
	entry = g_new0 (BatteryKindCacheEntry, 1);
	entry->battery_kind = battery_kind;
	return entry;
}

/**
 * battery_kind_cache_entry_free:
 * @entry: A device cache instance
 **/
static void
battery_kind_cache_entry_free (BatteryKindCacheEntry *entry)
{
	g_slist_foreach (entry->devices, (GFunc)g_free, NULL);
	g_slist_free (entry->devices);
	g_free (entry);
	entry = NULL;
}

/**
 * gpm_power_kind_to_localised_string:
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 * Return value: The localised battery kind. Do not free this string.
 **/
const char *
gpm_power_kind_to_localised_string (GpmPowerKind battery_kind)
{
	const char *str;

	if (battery_kind == GPM_POWER_KIND_PRIMARY) {
 		str = _("Laptop battery");
	} else if (battery_kind == GPM_POWER_KIND_UPS) {
 		str = _("UPS");
	} else if (battery_kind == GPM_POWER_KIND_MOUSE) {
 		str = _("Wireless mouse");
	} else if (battery_kind == GPM_POWER_KIND_KEYBOARD) {
 		str = _("Wireless keyboard");
	} else if (battery_kind == GPM_POWER_KIND_PDA) {
 		str = _("PDA");
 	} else {
 		str = _("Unknown");
	}
	return str;
}

/**
 * gpm_power_kind_to_string:
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 * Return value: The raw battery kind, e.g. primary. Do not free this string.
 **/
const char *
gpm_power_kind_to_string (GpmPowerKind battery_kind)
{
	const char *str;

	if (battery_kind == GPM_POWER_KIND_PRIMARY) {
 		str = "primary";
	} else if (battery_kind == GPM_POWER_KIND_UPS) {
 		str = _("ups");
	} else if (battery_kind == GPM_POWER_KIND_MOUSE) {
 		str = _("mouse");
	} else if (battery_kind == GPM_POWER_KIND_KEYBOARD) {
 		str = _("keyboard");
	} else if (battery_kind == GPM_POWER_KIND_PDA) {
 		str = _("pda");
 	} else {
 		str = _("unknown");
	}
	return str;
}


/**
 * battery_kind_cache_debug_print:
 * @entry: A device cache instance
 **/
static void
battery_kind_cache_debug_print (BatteryKindCacheEntry *entry)

{
	GpmPowerStatus *status = &entry->battery_status;
	gpm_debug ("Device : %s", gpm_power_kind_to_localised_string (entry->battery_kind));
	gpm_debug ("number     %i\tdesign     %i",
		   g_slist_length (entry->devices), status->design_charge);
	gpm_debug ("present    %i\tlast_full  %i",
		   status->is_present, status->last_full_charge);
	gpm_debug ("percent    %i\tcurrent    %i",
		   status->percentage_charge, status->current_charge);
	gpm_debug ("charge     %i\trate (raw) %i",
		   status->is_charging, status->charge_rate_raw);
	gpm_debug ("discharge  %i\tremaining  %i",
		   status->is_discharging, status->remaining_time);
	gpm_debug ("capacity   %i",
		   status->capacity);
}

/**
 * debug_print_type_cache_iter:
 **/
static void
debug_print_type_cache_iter (gpointer	       key,
			     BatteryKindCacheEntry *entry,
			     gpointer	      *user_data)
{
	battery_kind_cache_debug_print (entry);
}

/**
 * battery_kind_cache_debug_print_all:
 * @power: This power class instance
 **/
static void
battery_kind_cache_debug_print_all (GpmPower *power)
{
	if (power->priv->battery_kind_cache != NULL) {
		g_hash_table_foreach (power->priv->battery_kind_cache,
				      (GHFunc)debug_print_type_cache_iter,
				      NULL);
	}
}

/**
 * battery_device_cache_find:
 * @power: This power class instance
 * @udi: The HAL UDI for this device
 *
 * Finds the UDI in the device cache.
 *
 * Return value: The entry if found, or NULL if missing.
 **/
static GpmPowerDevice *
battery_device_cache_find (GpmPower   *power,
			   const char *udi)
{
	GpmPowerDevice *entry;

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

/**
 * battery_kind_cache_find:
 * @power: This power class instance
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 *
 * Finds the battery kind in the kind cache.
 *
 * Return value: The entry if found, or NULL if missing.
 **/
static BatteryKindCacheEntry *
battery_kind_cache_find (GpmPower		*power,
			 GpmPowerKind	 battery_kind)
{
	BatteryKindCacheEntry *entry;

	if (power->priv->battery_kind_cache == NULL) {
		return NULL;
	}

	entry = g_hash_table_lookup (power->priv->battery_kind_cache, &battery_kind);

	return entry;
}

/**
 * gpm_power_get_num_devices_of_kind:
 * @power: This power class instance
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 *
 * Return value: the number of devices of a specific kind.
 **/
gint
gpm_power_get_num_devices_of_kind (GpmPower		*power,
				   GpmPowerKind	 battery_kind)
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

GpmPowerDevice *
gpm_power_get_battery_device_entry (GpmPower	 *power,
				    GpmPowerKind  battery_kind,
				    gint	  device_num)
{
	const char		*udi;
	GpmPowerDevice *device;
	BatteryKindCacheEntry   *entry;

	if (! power->priv->battery_kind_cache) {
		return NULL;
	}
	entry = g_hash_table_lookup (power->priv->battery_kind_cache, &battery_kind);
	if (! entry) {
		return NULL;
	}
	if (device_num > g_slist_length (entry->devices) - 1) {
		return NULL;
	}

	/* get the udi of the battery we are interested in */
	udi = (const char *) g_slist_nth_data (entry->devices, device_num);

	/* find the udi in the device cache */
	device = battery_device_cache_find (power, udi);
	return device;
}

/**
 * gpm_power_status_for_device:
 * @power: This power class instance
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 * @device_num: The device number (e.g. 1 for the second primary battery)
 *
 * Return value: A description array.
 **/
GString *
gpm_power_status_for_device (GpmPowerDevice *device)
{
	GString		*details;
	GpmPowerStatus	*status;

	status = &device->battery_status;
	details = g_string_new ("");

	if (device->product) {
		g_string_append_printf (details, _("<b>Product:</b> %s\n"), device->product);
	}
	if (status->is_present == FALSE) {
		g_string_append (details, _("<b>Status:</b> Missing\n"));
	} else if (gpm_power_battery_is_charged (status)) {
		g_string_append (details, _("<b>Status:</b> Charged\n"));
	} else if (status->is_charging) {
		g_string_append (details, _("<b>Status:</b> Charging\n"));
	} else if (status->is_discharging) {
		g_string_append (details, _("<b>Status:</b> Discharging\n"));
	}
	if (status->percentage_charge > 0) {
		g_string_append_printf (details, _("<b>Percentage charge:</b> %i%%\n"),
					status->percentage_charge);
	}
	/* remove the last \n */
	g_string_truncate (details, details->len-1);
	return details;
}

/**
 * gpm_power_status_for_device_more:
 * @power: This power class instance
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 * @device_num: The device number (e.g. 1 for the second primary battery)
 *
 * Return value: A description array.
 **/
GString *
gpm_power_status_for_device_more (GpmPowerDevice *device)
{
	GString		*details;
	const char	*suffix;
	GpmPowerStatus	*status;

	status = &device->battery_status;
	details = g_string_new ("");

	if (device->vendor) {
		g_string_append_printf (details, _("<b>Vendor:</b> %s\n"), device->vendor);
	}
	if (device->technology) {
		const char *technology;
		if (strcasecmp (device->technology, "li-ion") == 0 ||
		    strcasecmp (device->technology, "lithium-ion") == 0 ||
		    strcasecmp (device->technology, "lion") == 0) {
			technology = _("Lithium ion");
		} else if (strcasecmp (device->technology, "pbac") == 0 ||
			   strcasecmp (device->technology, "lead-acid") == 0) {
			technology = _("Lead acid");
		} else {
			gpm_warning ("Battery type %s not translated, please report!",
				     device->technology);
			technology = device->technology;
		}
		g_string_append_printf (details, _("<b>Technology:</b> %s\n"), technology);
	}
	if (device->serial) {
		g_string_append_printf (details, _("<b>Serial number:</b> %s\n"), device->serial);
	}
	if (device->model) {
		g_string_append_printf (details, _("<b>Model:</b> %s\n"), device->model);
	}
	if (status->remaining_time > 0) {
		char *time;
		time = gpm_get_timestring (status->remaining_time);
		g_string_append_printf (details, _("<b>Remaining time:</b> %s\n"), time);
		g_free (time);
	}
	if (status->capacity > 0) {
		const char *condition;
		if (status->capacity > 99) {
			condition = _("Excellent");
		} else if (status->capacity > 90) {
			condition = _("Good");
		} else if (status->capacity > 70) {
			condition = _("Fair");
		} else {
			condition = _("Poor");
		}
		g_string_append_printf (details, _("<b>Capacity:</b> %i%% (%s)\n"),
					status->capacity, condition);
	}
	if (device->unit == GPM_POWER_UNIT_MWH) {
		suffix = "W";
		if (status->current_charge > 0) {
			g_string_append_printf (details, _("<b>Current charge:</b> %.1f %s\n"),
						status->current_charge / 1000.0f, suffix);
		}
		if (status->last_full_charge > 0 &&
		    status->design_charge != status->last_full_charge) {
			g_string_append_printf (details, _("<b>Last full charge:</b> %.1f %s\n"),
						status->last_full_charge / 1000.0f, suffix);
		}
		if (status->design_charge > 0) {
			g_string_append_printf (details, _("<b>Design charge:</b> %.1f %s\n"),
						status->design_charge / 1000.0f, suffix);
		}
		if (status->charge_rate_raw > 0) {
			g_string_append_printf (details, _("<b>Charge rate (raw):</b> %.1f %sh\n"),
						status->charge_rate_raw / 1000.0f, suffix);
		}
		if (status->charge_rate_smoothed > 0) {
			g_string_append_printf (details, _("<b>Charge rate (smoothed):</b> %.1f %sh\n"),
						status->charge_rate_smoothed / 1000.0f, suffix);
		}
	}
	if (device->unit == GPM_POWER_UNIT_CSR) {
		suffix = "/ 7";
		if (status->current_charge > 0) {
			g_string_append_printf (details, _("<b>Current charge:</b> %i %s\n"),
						status->current_charge, suffix);
		}
		if (status->design_charge > 0) {
			g_string_append_printf (details, _("<b>Design charge:</b> %i %s\n"),
						status->design_charge, suffix);
		}
	}
	/* remove the last \n */
	g_string_truncate (details, details->len-1);
	return details;
}

/**
 * gpm_power_get_index_from_percent:
 * @percent: The charge of the device
 *
 * The index value depends on the percentage charge:
 *	00-10  = 000
 *	10-30  = 020
 *	30-50  = 040
 *	50-70  = 060
 *	70-90  = 080
 *	90-100 = 100
 *
 * Return value: The character string for the filename suffix.
 **/
static char *
gpm_power_get_index_from_percent (gint percent)
{
	if (percent < 10) {
		return "000";
	} else if (percent < 30) {
		return "020";
	} else if (percent < 50) {
		return "040";
	} else if (percent < 70) {
		return "060";
	} else if (percent < 90) {
		return "080";
	}
	return "100";
}

/**
 * gpm_power_get_icon_for_all:
 * @device_status: The device status struct with the information
 * @prefix: The battery prefix, e.g. "primary" or "ups"
 *
 * Because UPS and primary icons have charged, charging and discharging icons
 * we need to abstract out the logic for the filenames.
 *
 * Return value: The complete filename, must free using g_free.
 **/
static char *
gpm_power_get_icon_for_all (GpmPowerStatus *device_status,
			    const char     *prefix)
{
	char *filename = NULL;
	char *index = NULL;

	if (! device_status->is_present) {
		/* battery missing */
		filename = g_strdup_printf ("gpm-%s-missing", prefix);

	} else if (gpm_power_battery_is_charged (device_status)) {
		filename = g_strdup_printf ("gpm-%s-charged", prefix);

	} else if (device_status->is_charging) {
		index = gpm_power_get_index_from_percent (device_status->percentage_charge);
		filename = g_strdup_printf ("gpm-%s-%s-charging", prefix, index);

	} else if (device_status->is_discharging) {
		index = gpm_power_get_index_from_percent (device_status->percentage_charge);
		filename = g_strdup_printf ("gpm-%s-%s", prefix, index);

	} else {
		/* We have a broken battery, not sure what to display here */
		gpm_debug ("Broken or missing battery...");
		filename = g_strdup_printf ("gpm-%s-missing", prefix);
	}
	return filename;
}

/**
 * gpm_power_get_icon_for_csr:
 * @device_status: The device status struct with the information
 * @prefix: The battery prefix, e.g. "mouse" or "keyboard"
 *
 * CSR has different icons to UPS and primary, work them out here.
 *
 * Return value: The complete filename, must free using g_free.
 **/
static char *
gpm_power_get_icon_for_csr (GpmPowerStatus *device_status,
			    const char     *prefix)
{
	char *filename;
	char *index;

	if (device_status->current_charge < 2) {
		index = "000";
	} else if (device_status->current_charge < 4) {
		index = "030";
	} else if (device_status->current_charge < 6) {
		index = "060";
	} else {
		index = "100";
	}
	filename = g_strdup_printf ("gpm-%s-%s", prefix, index);
	return filename;
}

/**
 * gpm_power_get_icon_from_status:
 * @device_status: The device status struct with the information
 * @kind: The battery kind, e.g. GPM_POWER_KIND_PRIMARY
 *
 * Get the correct icon for the device.
 *
 * Return value: The complete filename, must free using g_free.
 **/
char *
gpm_power_get_icon_from_status (GpmPowerStatus *device_status,
				GpmPowerKind    kind)
{
	char	   *filename = NULL;
	const char *prefix;

	/* TODO: icons need to be renamed from -battery- to -primary- */
	prefix = gpm_power_kind_to_string (kind);

	if (kind == GPM_POWER_KIND_PRIMARY ||
	    kind == GPM_POWER_KIND_UPS) {
		filename = gpm_power_get_icon_for_all (device_status, prefix);
	} else if (kind == GPM_POWER_KIND_MOUSE ||
		   kind == GPM_POWER_KIND_KEYBOARD) {
		filename = gpm_power_get_icon_for_csr (device_status, prefix);
	} else {
		/* Ummm... what to display... */
		filename = g_strdup ("gpm-ups-missing");
	}
	gpm_debug ("got filename: %s", filename);
	return filename;
}

/**
 * battery_kind_cache_update:
 * @power: This power class instance
 * @entry: A kind cache instance
 *
 * Updates the kind cache from the constituent device cache objects. This is
 * needed on multibattery laptops where the time needs to be computed over
 * two or more battereies. Some laptop batteries discharge one after the other,
 * some discharge simultanously.
 * This also does sanity checking on the values to make sure they are sane.
 **/
static void
battery_kind_cache_update (GpmPower		 *power,
			   BatteryKindCacheEntry *entry)
{
	GSList *l;
	int     num_present = 0;
	int     num_discharging = 0;
	GpmPowerStatus *type_status = &entry->battery_status;

	/* clear old values */
	gpm_power_battery_status_set_defaults (type_status);

	/* iterate thru all the devices to handle multiple batteries */
	for (l = entry->devices; l; l = l->next) {
		const char	      *udi;
		GpmPowerDevice *device;
		GpmPowerStatus	*device_status;

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
		type_status->charge_rate_smoothed += device_status->charge_rate_smoothed;
		type_status->charge_rate_raw += device_status->charge_rate_raw;
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

	/* If the primary battery is neither charging nor discharging, and
	 * the charge is low the battery is most likely broken.
	 * In this case, we'll use the ac_adaptor to determine whether it's
	 * charging or not. */
	if (entry->battery_kind == GPM_POWER_KIND_PRIMARY &&
	    (! type_status->is_charging) && (! type_status->is_discharging) &&
	    type_status->percentage_charge > 0 &&
	    type_status->percentage_charge < GPM_POWER_MIN_CHARGED_PERCENTAGE) {
		gboolean on_ac;
		on_ac = gpm_hal_is_on_ac (power->priv->hal);
		gpm_debug ("Battery is neither charging nor discharging, "
			   "using ac_adaptor value %i", on_ac);
		type_status->is_charging = on_ac;
		type_status->is_discharging = !(on_ac);
	}

	gpm_debug ("%i devices of type %s", num_present, gpm_power_kind_to_localised_string (entry->battery_kind));

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
	if (type_status->charge_rate_smoothed > 0) {
		if (type_status->is_discharging) {
			type_status->remaining_time = 3600 * ((float)type_status->current_charge /
							      (float)type_status->charge_rate_smoothed);
		} else if (type_status->is_charging) {
			type_status->remaining_time = 3600 *
				((float)(type_status->last_full_charge - type_status->current_charge) /
				(float)type_status->charge_rate_smoothed);
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
		   gpm_power_kind_to_localised_string (entry->battery_kind));
	g_signal_emit (power, signals [BATTERY_STATUS_CHANGED], 0, entry->battery_kind);
}

/**
 * battery_kind_update_cache_iter:
 **/
static void
battery_kind_update_cache_iter (const char	      *key,
				BatteryKindCacheEntry *entry,
				GpmPower	      *power)
{
	battery_kind_cache_update (power, entry);
}

/**
 * battery_kind_cache_update_all:
 * @power: This power class instance
 *
 * Updates every device of every type
 **/
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

/**
 * battery_device_cache_add_device:
 * @power: This power class instance
 * @entry: A device cache instance
 **/
static void
battery_device_cache_add_device (GpmPower		 *power,
				 GpmPowerDevice *entry)
{
	g_hash_table_insert (power->priv->battery_device_cache,
			     g_strdup (entry->udi),
			     entry);
}

/**
 * battery_device_cache_remove_device:
 * @power: This power class instance
 * @entry: A device cache instance
 **/
static void
battery_device_cache_remove_device (GpmPower		    *power,
				    GpmPowerDevice *entry)
{
	g_hash_table_remove (power->priv->battery_device_cache,
			     entry->udi);
	g_free (entry->udi);
	entry->udi = NULL;
	g_free (entry->product);
	entry->product = NULL;
	g_free (entry->vendor);
	entry->vendor = NULL;
	g_free (entry->technology);
	entry->technology = NULL;
	g_free (entry->serial);
	entry->serial = NULL;
	g_free (entry->model);
	entry->model = NULL;
}

/**
 * battery_kind_cache_add_device:
 * @power: This power class instance
 * @device_entry: A device cache instance
 *
 * Adds a device entry to the correct cache entry.
 **/
static void
battery_kind_cache_add_device (GpmPower			*power,
			       GpmPowerDevice	*device_entry)
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
	type_entry->devices = g_slist_append (type_entry->devices, device_entry->udi);

	battery_kind_cache_update (power, type_entry);
}

/**
 * battery_kind_cache_remove_device:
 * @power: This power class instance
 * @entry: A device cache instance
 *
 * Removes a device entry from a cache entry.
 **/
static void
battery_kind_cache_remove_device (GpmPower		  *power,
				  GpmPowerDevice *entry)
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

/**
 * power_get_summary_for_battery_kind:
 * @power: This power class instance
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 * @summary: The returned string summary
 *
 * Gets a summary for a specific battery kind. This is used on the tooltip
 * mainly.
 **/
static void
power_get_summary_for_battery_kind (GpmPower		*power,
			    	    GpmPowerKind  battery_kind,
			    	    GString		*summary)
{
	BatteryKindCacheEntry *entry;
	GpmPowerStatus *status;
	const char	      *type_desc = NULL;
	char		      *timestring;

	entry = battery_kind_cache_find (power, battery_kind);

	if (entry == NULL) {
		return;
	}

	status = &entry->battery_status;

	if (! status->is_present) {
		return;
	}

	type_desc = gpm_power_kind_to_localised_string (entry->battery_kind);

	/* don't display all the extra stuff for keyboards and mice */
	if (entry->battery_kind == GPM_POWER_KIND_MOUSE
	    || entry->battery_kind == GPM_POWER_KIND_KEYBOARD
	    || entry->battery_kind == GPM_POWER_KIND_PDA) {

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

			g_string_append (summary, _("fully charged"));

	} else if (status->is_discharging) {

		if (status->remaining_time > 60) {
			g_string_append_printf (summary, _("%s remaining"),
						timestring);
		} else {
			/* don't display "Unknown remaining" */
			g_string_append (summary, _("discharging"));
		}

	} else if (status->is_charging || power->priv->on_ac) {

		if (status->remaining_time > 60) {
			g_string_append_printf (summary, _("%s until charged"),
						timestring);
		} else {
			/* don't display "Unknown remaining" */
			g_string_append (summary, _("charging"));
		}

	} else {
		gpm_warning ("in an undefined state we are not charging or "
			     "discharging and the batteries are also not charged");
	}

	/* append percentage to all devices */
	g_string_append_printf (summary, " (%i%%)\n", status->percentage_charge);

	g_free (timestring);
}

/**
 * gpm_power_get_status_summary:
 * @power: This power class instance
 * @string: The returned string
 *
 * Returns the complete tooltip ready for display. Text logic is done here :-).
 **/
gboolean
gpm_power_get_status_summary (GpmPower *power,
			      char    **string,
			      GError  **error)
{
	GString *summary = NULL;
	gboolean ups_present;
	GpmPowerStatus status;

	if (! string) {
		return FALSE;
	}

	ups_present = gpm_power_get_battery_status (power,
						    GPM_POWER_KIND_UPS,
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
	power_get_summary_for_battery_kind (power, GPM_POWER_KIND_PRIMARY, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_KIND_UPS, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_KIND_MOUSE, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_KIND_KEYBOARD, summary);
	power_get_summary_for_battery_kind (power, GPM_POWER_KIND_PDA, summary);

	/* remove the last \n */
	g_string_truncate (summary, summary->len-1);

	gpm_debug ("tooltip: %s", summary->str);

	*string = g_string_free (summary, FALSE);

	return TRUE;
}

/**
 * gpm_power_get_battery_status:
 * @power: This power class instance
 * @battery_kind: The type of battery, e.g. GPM_POWER_KIND_PRIMARY
 * @battery_status: A battery info structure
 *
 * Return value: if the device was found in the cache.
 **/
gboolean
gpm_power_get_battery_status (GpmPower			 *power,
			      GpmPowerKind	  battery_kind,
			      GpmPowerStatus      *battery_status)
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

/**
 * gpm_power_set_on_ac:
 * @power: This power class instance
 **/
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

/**
 * gpm_power_get_on_ac:
 * @power: This power class instance
 **/
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

/**
 * gpm_power_set_property:
 **/
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

/**
 * gpm_power_get_property:
 **/
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

/**
 * gpm_power_class_init:
 **/
static void
gpm_power_class_init (GpmPowerClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

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

/**
 * hal_on_ac_changed_cb:
 * @monitor: The HAL monitor class instance
 * @on_ac: If we are on AC power
 * @power: This power class instance
 *
 * As the state of the AC Adapter has changed, we need to refresh our device
 * and kind caches as they have likely changed.
 **/
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

/**
 * add_battery:
 * @power: This power class instance
 * @udi: The HAL UDI for this device
 *
 * Add a battery to a device cache, and then add the device cache to the
 * correct kind cache depending on type.
 **/
static gboolean
add_battery (GpmPower   *power,
	     const char *udi)
{
	GpmPowerDevice *entry;
	GpmPowerStatus *status;

	gpm_debug ("adding %s", udi);

	g_assert (udi);

	entry = battery_device_cache_entry_new_from_udi (power, udi);

	battery_device_cache_add_device (power, entry);
	battery_kind_cache_add_device (power, entry);

	status = &entry->battery_status;
	/*
	 * We should notify the user if the battery has a low capacity,
	 * where capacity is the ratio of the last_full capacity with that of
	 * the design capacity. (#326740)
	 */
	if (entry->battery_kind == GPM_POWER_KIND_PRIMARY) {
		if (status->capacity > 0 && status->capacity < 50) {
			gpm_warning ("Your battery has a very low capacity, "
				     "meaning that it may be old or broken. "
				     "Battery life will be sub-optimal, "
				     "and the time remaining may be incorrect.");
		}
	}
	return TRUE;
}

/**
 * remove_battery:
 * @udi: The HAL UDI for this device
 * @power: This power class instance
 *
 * Removes a battery from the device and kind caches from a UDI value.
 *
 * Return value: if a battery was removed.
 **/
static gboolean
remove_battery (GpmPower   *power,
		const char *udi)
{
	GpmPowerDevice *entry;

	gpm_debug ("removing %s", udi);

	g_assert (udi);

	entry = battery_device_cache_find (power,
					   udi);
	if (entry == NULL) {
		gpm_warning ("trying to remove battery that is not present in db");
		return FALSE;
	}

	battery_kind_cache_remove_device (power, entry);
	battery_device_cache_remove_device (power, entry);

	return TRUE;
}

/**
 * hal_battery_added_cb:
 * @monitor: The HAL monitor class instance
 * @udi: The HAL UDI for this device
 * @power: This power class instance
 * Called from HAL...
 **/
static void
hal_battery_added_cb (GpmHalMonitor *monitor,
		      const char    *udi,
		      GpmPower      *power)
{
	gpm_debug ("Battery Added: %s", udi);
	add_battery (power, udi);

	battery_kind_cache_debug_print_all (power);
}

/**
 * hal_battery_removed_cb:
 * @monitor: The HAL monitor class instance
 * @udi: The HAL UDI for this device
 * @power: This power class instance
 * Called from HAL...
 **/
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

/**
 * hal_battery_property_modified_cb:
 * @monitor: The HAL monitor class instance
 * @udi: The HAL UDI for this device
 * @key: The HAL key that is modified
 * @power: This power class instance
 * Called from HAL...
 **/
static void
hal_battery_property_modified_cb (GpmHalMonitor *monitor,
				  const char    *udi,
				  const char    *key,
				  gboolean	 finally,
				  GpmPower      *power)
{
	GpmPowerDevice *device_entry;
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

	battery_device_cache_entry_update_key (power, device_entry, key);

	type_entry = battery_kind_cache_find (power, device_entry->battery_kind);

	if (type_entry == NULL) {
		gpm_warning ("battery type cache entry not found for modified device");
		return;
	}

	/* We only refresh the caches when we have all the info from a UDI */
	if (finally) {
		battery_kind_cache_update (power, type_entry);
		battery_kind_cache_debug_print (type_entry);
	}
}

/**
 * hal_button_pressed_cb:
 * @monitor: The HAL monitor class instance
 * @type: the button type, e.g. "power"
 * @power: This power class instance
 * @state: The lid state, where TRUE is open or pressed
 **/
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

/**
 * gpm_hash_remove_return:
 * FIXME: there must be a better way to do this
 **/
static gboolean
gpm_hash_remove_return (gpointer key,
			gpointer value,
			gpointer user_data)
{
	return TRUE;
}

/**
 * gpm_hash_new_kind_cache:
 * @power: This power class instance
 **/
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

/**
 * gpm_hash_free_kind_cache:
 * @power: This power class instance
 **/
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

/**
 * gpm_hash_new_device_cache:
 * @power: This power class instance
 **/
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

/**
 * gpm_hash_free_device_cache:
 * @power: This power class instance
 **/
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

/**
 * gpm_power_update_all:
 * @power: This power class instance
 *
 * We can call this anywhere to update all the device and kind caches
 **/
void
gpm_power_update_all (GpmPower *power)
{
	battery_kind_cache_update_all (power);
}

/**
 * hal_daemon_start_cb:
 * @hal: The HAL class instance
 * @power: This power class instance
 *
 * Re-create the caches as HAL has started
 **/
static void
hal_daemon_start_cb (GpmHal     *hal,
		    GpmPower   *power)
{
	gpm_hash_new_kind_cache (power);
	gpm_hash_new_device_cache (power);
	gpm_hal_monitor_coldplug (power->priv->hal_monitor);
	battery_kind_cache_update_all (power);
}

/**
 * hal_daemon_stop_cb:
 * @hal: The HAL class instance
 * @power: This power class instance
 *
 * We have to clear the caches, else the devices think they are initialised,
 * and we segfault in various places.
 **/
static void
hal_daemon_stop_cb (GpmHal     *hal,
		    GpmPower   *power)
{
	gpm_hash_free_kind_cache (power);
	gpm_hash_free_device_cache (power);
}

/**
 * gpm_power_init:
 * @power: This power class instance
 **/
static void
gpm_power_init (GpmPower *power)
{
	gboolean on_ac;

	GConfClient *client = gconf_client_get_default ();

	power->priv = GPM_POWER_GET_PRIVATE (power);

	power->priv->hal = gpm_hal_new ();
	g_signal_connect (power->priv->hal, "daemon-start",
			  G_CALLBACK (hal_daemon_start_cb), power);
	g_signal_connect (power->priv->hal, "daemon-stop",
			  G_CALLBACK (hal_daemon_stop_cb), power);

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

	power->priv->battery_kind_cache = NULL;
	power->priv->battery_device_cache = NULL;

	gpm_hash_new_kind_cache (power);
	gpm_hash_new_device_cache (power);

	on_ac = gpm_hal_is_on_ac (power->priv->hal);
	gpm_power_set_on_ac (power, on_ac, NULL);

	power->priv->exp_ave_factor = gconf_client_get_int (client,
							    GPM_PREF_RATE_EXP_AVE_FACTOR,
							    NULL);
	g_object_unref (client);
}

/**
 * gpm_power_finalize:
 **/
static void
gpm_power_finalize (GObject *object)
{
	GpmPower *power;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_POWER (object));

	power = GPM_POWER (object);

	g_return_if_fail (power->priv != NULL);

	gpm_hash_free_kind_cache (power);
	gpm_hash_free_device_cache (power);

	g_object_unref (power->priv->hal_monitor);
	g_object_unref (power->priv->hal);

	G_OBJECT_CLASS (gpm_power_parent_class)->finalize (object);
}

/**
 * gpm_power_new:
 * Return value: A new power class instance.
 **/
GpmPower *
gpm_power_new (void)
{
	if (gpm_power_object) {
		g_object_ref (gpm_power_object);
	} else {
		gpm_power_object = g_object_new (GPM_TYPE_POWER, NULL);
		g_object_add_weak_pointer (gpm_power_object,
					   (gpointer *) &gpm_power_object);
	}
	return GPM_POWER (gpm_power_object);
}
