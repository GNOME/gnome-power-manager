/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
 *
 * Authors:
 *          William Jon McCann <mccann@jhu.edu>
 *          Richard Hughes <richard@hughsie.com>
 *
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

static void     gpm_power_class_init (GpmPowerClass *klass);
static void     gpm_power_init       (GpmPower      *power);
static void     gpm_power_finalize   (GObject       *object);

#define GPM_POWER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POWER, GpmPowerPrivate))

struct GpmPowerPrivate
{
	gboolean       on_ac;

	GHashTable    *kind_cache;
	GHashTable    *device_cache;

	GpmHalMonitor *hal_monitor;
};

enum {
	BUTTON_PRESSED,
	AC_STATE_CHANGED,
	BATTERY_POWER_CHANGED,
	LAST_SIGNAL
};

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
	char    *kind;

	int      percentage_charge;
	int      minutes_remaining;
	gboolean is_charging;
	gboolean is_discharging;
	gboolean is_present;

	/* List of device udis */
	GSList  *devices;

} BatteryKindCacheEntry;

typedef struct {
	char    *udi;
	char    *kind;

	int      percentage_charge;
	int      minutes_remaining;
	gboolean is_rechargeable;
	gboolean is_present;
	gboolean is_charging;
	gboolean is_discharging;
} BatteryDeviceCacheEntry;

static void
battery_device_cache_entry_update_all (BatteryDeviceCacheEntry *entry)
{
	gboolean is_present;
	int      seconds_remaining;

	/* batteries might be missing */
	gpm_hal_device_get_bool (entry->udi, "battery.present", &entry->is_present);

	gpm_hal_device_get_string (entry->udi, "battery.type", &entry->kind);

	/* initialise to known defaults */
	entry->minutes_remaining = 0;
	entry->percentage_charge = 0;
	entry->is_rechargeable = FALSE;
	entry->is_charging = FALSE;
	entry->is_discharging = FALSE;

	/* battery might not be rechargeable, have to check */
	gpm_hal_device_get_bool (entry->udi,
				 "battery.is_rechargeable",
				 &entry->is_rechargeable);
	if (entry->is_rechargeable) {
		gpm_hal_device_get_bool (entry->udi,
					 "battery.rechargeable.is_charging",
					 &entry->is_charging);
		gpm_hal_device_get_bool (entry->udi,
					 "battery.rechargeable.is_discharging",
					 &entry->is_discharging);
	}

	/* sanity check that remaining time exists (if it should) */
	is_present = gpm_hal_device_get_int (entry->udi,
					     "battery.remaining_time",
					     &seconds_remaining);
	if (! is_present && (entry->is_discharging || entry->is_charging)) {
		g_warning ("could not read your battery's remaining time");
	} else if (seconds_remaining > 0) {
		entry->minutes_remaining = seconds_remaining / 60;
	}

	/* sanity check that remaining time exists (if it should) */
	is_present = gpm_hal_device_get_int (entry->udi,
					     "battery.charge_level.percentage",
					     &entry->percentage_charge);
	if (!is_present && (entry->is_discharging || entry->is_charging)) {
		g_warning ("could not read your battery's percentage charge.");
	}
}

static void
battery_device_cache_entry_update_key (BatteryDeviceCacheEntry *entry,
				       const char              *key)
{
	if (key == NULL) {
		return;
	}

	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		gpm_hal_device_get_bool (entry->udi, key, &entry->is_present);

		battery_device_cache_entry_update_all (entry);

	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		gpm_hal_device_get_bool (entry->udi, key, &entry->is_charging);

		/*
		 * invalidate the remaining time, as we need to wait for
		 * the next HAL update. This is a HAL bug I think.
		 */
		entry->minutes_remaining = 0;
	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		gpm_hal_device_get_bool (entry->udi, key, &entry->is_discharging);

		/* invalidate the remaining time */
		entry->minutes_remaining = 0;
	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		gpm_hal_device_get_int (entry->udi, key, &entry->percentage_charge);

	} else if (strcmp (key, "battery.remaining_time") == 0) {
		int tempval;

		gpm_hal_device_get_int (entry->udi, key, &tempval);
		if (tempval > 0)
			entry->minutes_remaining = tempval / 60;
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
	battery_device_cache_entry_update_all (entry);

	return entry;
}

static void
battery_device_cache_entry_free (BatteryDeviceCacheEntry *entry)
{
	g_free (entry->udi);
	g_free (entry->kind);
	g_free (entry);
	entry = NULL;
}

static BatteryKindCacheEntry *
battery_kind_cache_entry_new_from_kind (const char *kind)
{
	BatteryKindCacheEntry *entry;

	entry = g_new0 (BatteryKindCacheEntry, 1);

	entry->kind = g_strdup (kind);

	return entry;
}

static void
battery_kind_cache_entry_free (BatteryKindCacheEntry *entry)
{
	g_free (entry->kind);
	g_slist_foreach (entry->devices, (GFunc)g_free, NULL);
	g_slist_free (entry->devices);
	g_free (entry);
	entry = NULL;
}

static const char *
kind_for_display (const char *kind)
{
	const char *str;

	if (kind == NULL) {
		return _("Unknown");
	}

	if (strcmp (kind, "primary") == 0) {
		str = _("Laptop battery");
	} else if (strcmp (kind, "ups") == 0) {
		str = _("UPS");
	} else if (strcmp (kind, "mouse") == 0) {
		str = _("Wireless mouse");
	} else if (strcmp (kind, "keyboard") == 0) {
		str = _("Wireless keyboard");
	} else if (strcmp (kind, "pda") == 0) {
		str = _("PDA");
	} else {
		str = _("Unknown");
	}

	return str;
}

/** Prints the system device object of a specified type
 *
 *  @param	type		The device type
 */
static void
battery_kind_cache_debug_print (GpmPower              *power,
				const char            *kind,
				BatteryKindCacheEntry *entry)

{
	g_debug ("Printing %s device parameters:", kind_for_display (kind));

	g_debug ("percentage_charge = %i", entry->percentage_charge);
	g_debug ("number_devices    = %i", g_slist_length (entry->devices));

	if (strcmp (kind, "mouse") != 0
	    && strcmp (kind, "keyboard") != 0) {
		g_debug ("is_present        = %i", entry->is_present);
		g_debug ("minutes_remaining = %i", entry->minutes_remaining);
		g_debug ("is_charging       = %i", entry->is_charging);
		g_debug ("is_discharging    = %i", entry->is_discharging);
	}
}

static void
debug_print_kind_cache_iter (const char            *key,
			     BatteryKindCacheEntry *entry,
			     GpmPower              *power)
{
	battery_kind_cache_debug_print (power, key, entry);
}

static void
battery_kind_cache_debug_print_all (GpmPower *power)
{
	if (power->priv->kind_cache != NULL) {
		g_hash_table_foreach (power->priv->kind_cache,
				      (GHFunc)debug_print_kind_cache_iter,
				      power);
	}
}

static BatteryDeviceCacheEntry *
battery_device_cache_find (GpmPower   *power,
			   const char *udi)
{
	BatteryDeviceCacheEntry *entry;

	if (! udi) {
		g_warning ("UDI is NULL");
		return NULL;
	}

	if (power->priv->device_cache == NULL) {
		return NULL;
	}

	entry = g_hash_table_lookup (power->priv->device_cache, udi);

	return entry;
}

static BatteryKindCacheEntry *
battery_kind_cache_find (GpmPower   *power,
			 const char *kind)
{
	BatteryKindCacheEntry *entry;

	if (! kind) {
		g_warning ("Kind is NULL");
		return NULL;
	}

	if (power->priv->kind_cache == NULL) {
		return NULL;
	}

	entry = g_hash_table_lookup (power->priv->kind_cache, kind);

	return entry;
}

static void
battery_kind_cache_update (GpmPower              *power,
			   BatteryKindCacheEntry *entry)
{
	GSList *l;
	int     num_present = 0;
	int     num_discharging = 0;
	int     old_charge;
	int     new_charge;

	old_charge = entry->percentage_charge;

	/* clear old values */
	entry->minutes_remaining = 0;
	entry->percentage_charge = 0;
	entry->is_charging = FALSE;
	entry->is_discharging = FALSE;
	entry->is_present = FALSE;


	/* Count the number present */
	for (l = entry->devices; l; l = l->next) {
		const char              *udi;
		BatteryDeviceCacheEntry *device;

		udi = (const char *)l->data;

		device = battery_device_cache_find (power, udi);

		if (! device->is_present) {
			continue;
		}

		num_present++;

		/* Only one device has to be present for the class to
		 * be present. */
		entry->is_present = TRUE;

		if (device->is_charging) {
			entry->is_charging = TRUE;
		}

		if (device->is_discharging) {
			entry->is_discharging = TRUE;
			num_discharging++;
		}
	}

	/* sanity check */
	if (entry->is_discharging && entry->is_charging) {
		g_warning ("battery_kind_cache_update: Sanity check kicked in! "
			   "Multiple device object cannot be charging and "
			   "discharging simultaneously!");
		entry->is_charging = FALSE;
	}

	/* no point working out average if no devices */
	if (num_present == 0) {
		g_debug ("no devices of type %s", kind_for_display (entry->kind));
		/* send a signal, as devices have disappeared */
		g_signal_emit (power,
			       signals [BATTERY_POWER_CHANGED], 0,
			       entry->kind,
			       0,
			       0,
			       FALSE,
			       FALSE,
			       TRUE);
		return;
	}

	g_debug ("%i devices of type %s", num_present, entry->kind);

	/* iterate thru all the devices (multiple battery scenario) */
	for (l = entry->devices; l; l = l->next) {
		const char              *udi;
		BatteryDeviceCacheEntry *device;

		udi = (const char *)l->data;

		device = battery_device_cache_find (power, udi);

		if (device->is_present) {
			/* for now, just add */
			entry->minutes_remaining += device->minutes_remaining;
			/* for now, just average */
			entry->percentage_charge += (device->percentage_charge / num_present);
		}
	}

	/*
	 * if we are discharging, and the number or batteries
	 * discharging != the number present, then we have a case where the
	 * batteries are discharging one at a time (i.e. not simultanously)
	 * and we have to factor this into the time remaining calculations.
	 * This should effect:
	 *   https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=169158
	 */
	if (entry->is_discharging && num_discharging != num_present) {
		g_warning ("doubling minutes_remaining as sequential");
		/* for now, just double the result */
		entry->minutes_remaining *= num_present;
	}


	/* find new percentage_charge  */
	new_charge = entry->percentage_charge;

	g_debug ("new_charge = %i, old_charge = %i", new_charge, old_charge);

	gboolean percentagechanged = FALSE;
	/* only do some actions when the value changes */
	if (old_charge != new_charge)
		percentagechanged = TRUE;
	/*
	 * old_charge is initialised to zero, and we don't want to
	 * send a signal for the percentagechanged sequence
	 */
	if (old_charge == 0)
		percentagechanged = FALSE;

	if (percentagechanged)
		g_debug ("percentage change %i -> %i", old_charge, new_charge);

	/* always send a signal, as we needto setup the icon */
	g_signal_emit (power,
		       signals [BATTERY_POWER_CHANGED], 0,
		       entry->kind,
		       entry->percentage_charge,
		       entry->minutes_remaining,
		       entry->is_discharging,
		       entry->is_charging,
		       percentagechanged);
}

static void
update_kind_cache_iter (const char            *key,
			BatteryKindCacheEntry *entry,
			GpmPower              *power)
{
	battery_kind_cache_update (power, entry);
}

static void
battery_kind_cache_update_all (GpmPower *power)
{
	g_debug ("Updating all device types");

	if (power->priv->kind_cache != NULL) {
		g_hash_table_foreach (power->priv->kind_cache,
				      (GHFunc)update_kind_cache_iter,
				      power);
	}
}

static void
battery_device_cache_add_device (GpmPower                *power,
				 BatteryDeviceCacheEntry *entry)
{
	g_hash_table_insert (power->priv->device_cache,
			     g_strdup (entry->udi),
			     entry);
}

static void
battery_device_cache_remove_device (GpmPower                *power,
				    BatteryDeviceCacheEntry *entry)
{
	g_hash_table_remove (power->priv->device_cache,
			     entry->udi);
}

static void
battery_kind_cache_add_device (GpmPower                *power,
			       BatteryDeviceCacheEntry *entry)
{
	BatteryKindCacheEntry *kind_entry;

	if (! entry->is_present) {
		g_warning ("Adding missing device, may bug");
	}

	kind_entry = battery_kind_cache_find (power,
					      entry->kind);
	if (kind_entry == NULL) {
		kind_entry = battery_kind_cache_entry_new_from_kind (entry->kind);
		g_hash_table_insert (power->priv->kind_cache,
				     g_strdup (entry->kind),
				     kind_entry);
	}

	/* assume that it isn't in there already */
	kind_entry->devices = g_slist_prepend (kind_entry->devices, entry->udi);

	battery_kind_cache_update (power, kind_entry);
}

static void
battery_kind_cache_remove_device (GpmPower                *power,
				  BatteryDeviceCacheEntry *entry)
{
	BatteryKindCacheEntry *kind_entry;

	kind_entry = battery_kind_cache_find (power,
					      entry->kind);
	if (kind_entry == NULL) {
		return;
	}

	kind_entry->devices = g_slist_remove_all (kind_entry->devices, entry->udi);

	/* if we've removed the last device then remove from the hash */
	if (kind_entry->devices == NULL) {
		g_hash_table_remove (power->priv->kind_cache,
				     entry->kind);
	} else {
		battery_kind_cache_update (power, kind_entry);
	}
}

static void
power_get_summary_for_udi (GpmPower   *power,
			   const char *udi,
			   GString    *summary)
{
	BatteryDeviceCacheEntry *entry;
	const char              *kind_desc = NULL;
	const char              *chargestate = NULL;

	entry = battery_device_cache_find (power, udi);
	if (entry == NULL) {
		return;
	}

	if (! entry->is_present) {
		return;
	}

	kind_desc = kind_for_display (entry->kind);

	/* don't display all the extra stuff for keyboards and mice */
	if (strcmp (entry->kind, "mouse") == 0
	    || strcmp (entry->kind, "keyboard") == 0
	    || strcmp (entry->kind, "pda") == 0) {

		g_string_append_printf (summary,
					"%s (%i%%)\n",
					kind_desc,
					entry->percentage_charge);
		return;
	}

	/* work out chargestate */
	if (entry->is_charging)
		chargestate = _("charging");
	else if (entry->is_discharging)
		chargestate = _("discharging");
	else if (! entry->is_charging && !entry->is_discharging)
		chargestate = _("charged");

	g_string_append_printf (summary,
				"%s %s (%i%%)",
				kind_desc,
				chargestate,
				entry->percentage_charge);

	/*
	 * only display time remaining if minutes_remaining > 2
	 * and percentage_charge < 99 to cope with some broken
	 * batteries.
	 */
	if (entry->minutes_remaining > 2 && entry->percentage_charge < 99) {
		char *timestring;

		timestring = get_timestring_from_minutes (entry->minutes_remaining);

		if (timestring) {
			if (entry->is_charging) {
				g_string_append_printf (summary,
							"\n%s %s",
							timestring,
							_("until charged"));
			} else {
				g_string_append_printf (summary,
							"\n%s %s",
							timestring,
							_("until empty"));
			}
			g_free (timestring);
		}
	}
	g_string_append (summary, "\n");
}

static void
power_get_summary_for_kind (GpmPower   *power,
			    const char *kind,
			    GString    *summary)
{
	GSList                *l;
	BatteryKindCacheEntry *entry;

	entry = battery_kind_cache_find (power, kind);

	if (entry == NULL) {
		return;
	}

	if (! entry->is_present) {
		return;
	}

	for (l = entry->devices; l; l = l->next) {
		const char *udi = (const char *)l->data;
		power_get_summary_for_udi (power, udi, summary);
	}
}

gboolean
gpm_power_get_status_summary (GpmPower *power,
			      char    **string,
			      GError  **error)
{
	GString *summary = NULL;
	char    *list [] = { "primary", "ups", "mouse", "keyboard", "pda", NULL };
	int      i;

	if (! string) {
		return FALSE;
	}

	if (power->priv->on_ac) {
		summary = g_string_new (_("Computer is running on AC power\n"));
	} else {
		summary = g_string_new (_("Computer is running on battery power\n"));
	}

	/* do each device type we know about */
	/* FIXME: maybe don't hard code these ? */
	for (i = 0; list [i] != NULL; i++) {
		power_get_summary_for_kind (power, list [i], summary);
	}

	/* remove the last \n */
	g_string_truncate (summary, summary->len-1);

	g_debug ("tooltip: %s", summary->str);

	*string = g_string_free (summary, FALSE);

	return TRUE;
}

gboolean
gpm_power_get_battery_percentage (GpmPower   *power,
				  const char *kind,
				  int        *percentage,
				  GError    **error)
{
	BatteryKindCacheEntry *entry;

	if (percentage) {
		*percentage = 0;
	}

	g_return_val_if_fail (GPM_IS_POWER (power), FALSE);
	g_return_val_if_fail (kind != NULL, FALSE);

	entry = battery_kind_cache_find (power, kind);
	if (entry == NULL) {
		return FALSE;
	}
	/*
	 * Batteries can exist in the system without being "present"
	 * as the battery bay remains in HAL when they are removed.
	 */
	if (! entry->is_present) {
		return FALSE;
	}

	if (percentage) {
		*percentage = entry->percentage_charge;
	}

	return TRUE;
}

gboolean
gpm_power_get_battery_minutes (GpmPower   *power,
			       const char *kind,
			       gint64     *minutes,
			       GError    **error)
{
	BatteryKindCacheEntry *entry;

	if (minutes) {
		*minutes = 0;
	}

	g_return_val_if_fail (GPM_IS_POWER (power), FALSE);
	g_return_val_if_fail (kind != NULL, FALSE);

	entry = battery_kind_cache_find (power, kind);
	if (entry == NULL) {
		return FALSE;
	}

	if (minutes) {
		*minutes = entry->minutes_remaining;
	}

	return TRUE;
}

gboolean
gpm_power_get_battery_charging (GpmPower   *power,
				const char *kind,
				gboolean   *charging,
				gboolean   *discharging,
				GError    **error)
{
	BatteryKindCacheEntry *entry;

	if (charging) {
		*charging = 0;
	}
	if (discharging) {
		*discharging = 0;
	}

	g_return_val_if_fail (GPM_IS_POWER (power), FALSE);
	g_return_val_if_fail (kind != NULL, FALSE);

	entry = battery_kind_cache_find (power, kind);
	if (entry == NULL) {
		return FALSE;
	}

	if (charging) {
		*charging = entry->is_charging;
	}
	if (discharging) {
		*discharging = entry->is_discharging;
	}

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

		g_debug ("Setting on-ac: %d", on_ac);

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
			      gpm_marshal_VOID__STRING_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals [AC_STATE_CHANGED] =
		g_signal_new ("ac-power-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPowerClass, ac_state_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_BOOLEAN);
	signals [BATTERY_POWER_CHANGED] =
		g_signal_new ("battery-power-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmPowerClass, battery_power_changed),
			      NULL,
			      NULL,
			      gpm_marshal_VOID__INT_LONG_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 6, G_TYPE_INT, G_TYPE_LONG,
			      G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

	g_type_class_add_private (klass, sizeof (GpmPowerPrivate));
}

static void
hal_on_ac_changed_cb (GpmHalMonitor *monitor,
		      gboolean       on_ac,
		      GpmPower      *power)
{
	gpm_power_set_on_ac (power, on_ac, NULL);

	if (! on_ac) {
		/* update all states */
		battery_kind_cache_update_all (power);
	}
}

static gboolean
add_battery (GpmPower   *power,
	     const char *udi)
{
	BatteryDeviceCacheEntry *entry;

	g_debug ("adding %s", udi);

	g_assert (udi);

	entry = battery_device_cache_entry_new_from_udi (udi);

	battery_device_cache_add_device (power, entry);
	battery_kind_cache_add_device (power, entry);

	return TRUE;
}

static gboolean
remove_battery (GpmPower   *power,
		const char *udi)
{
	BatteryDeviceCacheEntry *entry;

	g_debug ("removing %s", udi);

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
		      const char    *reserved,
		      GpmPower      *power)
{
	g_debug ("Battery Added: %s", udi);
	add_battery (power, udi);

	battery_kind_cache_debug_print_all (power);
}

static void
hal_battery_removed_cb (GpmHalMonitor *monitor,
			const char    *udi,
			GpmPower      *power)
{
	g_debug ("Battery Removed: %s", udi);

	remove_battery (power, udi);

	battery_kind_cache_debug_print_all (power);
}

static void
hal_battery_property_modified_cb (GpmHalMonitor *monitor,
				  const char    *udi,
				  const char    *key,
				  GpmPower      *power)
{
	BatteryDeviceCacheEntry *entry;
	BatteryKindCacheEntry   *kind_entry;

	g_debug ("Battery Property Modified: %s", udi);

	entry = battery_device_cache_find (power, udi);

	/*
	 * if we BUG here then *HAL* has a problem where key modification is
	 * done before capability is present
	 */
	if (entry == NULL) {
		g_warning ("device cache entry is NULL! udi=%s\n"
			   "This is probably a bug in HAL where we are getting "
			   "is_removed=false, is_added=false before the capability "
			   "had been added. In addon-hid-ups this is likely to happen.",
			   udi);
		return;
	}

	battery_device_cache_entry_update_key (entry, key);

	/* find old percentage_charge */
	kind_entry = battery_kind_cache_find (power, entry->kind);

	if (kind_entry == NULL) {
		g_warning ("kind cache entry not found for modified device");
		return;
	}

	battery_kind_cache_update (power, kind_entry);

	battery_kind_cache_debug_print (power, entry->kind, kind_entry);
}

static void
hal_button_pressed_cb (GpmHalMonitor *monitor,
		       const char    *type,
		       const char    *details,
		       gboolean       state,
		       GpmPower      *power)
{
	/* just proxy it */
	g_signal_emit (power, signals [BUTTON_PRESSED], 0, type, details, state);
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

	power->priv->kind_cache = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 (GDestroyNotify)battery_kind_cache_entry_free);

	power->priv->device_cache = g_hash_table_new_full (g_str_hash,
							   g_str_equal,
							   g_free,
							   (GDestroyNotify)battery_device_cache_entry_free);

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

	if (power->priv->kind_cache != NULL) {
		g_hash_table_destroy (power->priv->kind_cache);
	}

	if (power->priv->device_cache != NULL) {
		g_hash_table_destroy (power->priv->device_cache);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmPower *
gpm_power_new (void)
{
	GpmPower *power;

	power = g_object_new (GPM_TYPE_POWER, NULL);

	return GPM_POWER (power);
}
