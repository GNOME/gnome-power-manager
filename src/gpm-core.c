/** @file	gpm-core.c
 *  @brief	Common functions shared between g-p-m session daemons
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-16
 *
 * These are functions used in gnome-power-manager and gnome-power-console.
 * They are included here to avoid code duplication.
 */
/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/** @todo factor these out into gpm-only modules */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include "gpm-common.h"
#include "gpm-core.h"
#include "gpm-sysdev.h"
#include "gpm-hal.h"
#include "glibhal-main.h"
#include "glibhal-callback.h"

/* no need for IPC with globals */
gboolean onAcPower;

/** Coldplugs devices of type ac_adaptor at startup
 *
 *  @return			If any devices of capability ac_adapter were found.
 */
gboolean
gpm_coldplug_acadapter (void)
{
	gint i;
	gchar **device_names = NULL;

	/* devices of type ac_adapter */
	hal_find_device_capability ("ac_adapter", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of ac_adapters");
		return FALSE;
	}
	for (i = 0; device_names[i]; i++) {
		/* assume only one */
		hal_device_get_bool (device_names[i], "ac_adapter.present", &onAcPower);
		glibhal_watch_add_device_property_modified (device_names[i]);

	}
	hal_free_capability (device_names);
	return TRUE;
}

/** Coldplugs devices of type ac_adaptor at startup
 *
 *  @return			If any devices of capability button were found.
 */
gboolean
gpm_coldplug_buttons (void)
{
	gint i;
	gchar **device_names = NULL;
	/* devices of type button */
	hal_find_device_capability ("button", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of buttons");
		return FALSE;
	}
	for (i = 0; device_names[i]; i++) {
		/*
		 * We register this here, as buttons are not present
		 * in object data, and do not need to be added manually.
		*/
		glibhal_watch_add_device_condition (device_names[i]);
	}
	hal_free_capability (device_names);
	return TRUE;
}

/** Coldplugs devices of type battery & ups at startup
 *
 *  @return			If any devices of capability battery were found.
 */
gboolean
gpm_coldplug_batteries (void)
{
	gint i;
	gchar **device_names = NULL;
	/* devices of type battery */
	hal_find_device_capability ("battery", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of batteries");
		return FALSE;
	}
	for (i = 0; device_names[i]; i++)
		gpm_add_battery (device_names[i]);
	hal_free_capability (device_names);
	return TRUE;
}

/** Adds a battery device, of any type. Also sets up properties on cached object
 *
 *  @param  udi			UDI
 *  @return			If we added a valid battery
 */
gboolean
gpm_add_battery (const gchar *udi)
{
	gchar *type = NULL;
	DeviceType dev;

	g_debug ("adding %s", udi);

	/* assertion checks */
	g_assert (udi);

	sysDevStruct *sds = g_new (sysDevStruct, 1);
	strncpy (sds->udi, udi, 128);

	/* batteries might be missing */
	hal_device_get_bool (udi, "battery.present", &sds->present);

	/* battery is refined using the .type property */
	hal_device_get_string (udi, "battery.type", &type);
	if (!type) {
		g_warning ("Battery %s has no type!", udi);
		return FALSE;
	}

	/* get battery type */
	dev = hal_to_device_type (type);
	g_debug ("Adding type %s", type);
	g_free (type);
	sysDevAdd (dev, sds);

	/* register this with HAL so we get PropertyModified events */
	glibhal_watch_add_device_property_modified (udi);

	/* read in values */
	gpm_read_battery_data (sds);
	return TRUE;
}

/** Adds an battery device. Also sets up properties on cached object
 *
 *  @param	sds		The cached object
 *  @return			If battery is present
 */
gboolean
gpm_read_battery_data (sysDevStruct *sds)
{
	gint seconds_remaining;
	gboolean is_present;

	g_debug ("reading battery data");

	/* assertion checks */
	g_assert (sds);

	/* initialise to known defaults */
	sds->minutesRemaining = 0;
	sds->percentageCharge = 0;
	sds->isRechargeable = FALSE;
	sds->isCharging = FALSE;
	sds->isDischarging = FALSE;

	if (!sds->present) {
		g_debug ("Battery %s not present!", sds->udi);
		return FALSE;
	}

	/* battery might not be rechargeable, have to check */
	hal_device_get_bool (sds->udi, "battery.is_rechargeable",
			     &sds->isRechargeable);
	if (sds->isRechargeable) {
		hal_device_get_bool (sds->udi, "battery.rechargeable.is_charging",
				     &sds->isCharging);
		hal_device_get_bool (sds->udi, "battery.rechargeable.is_discharging",
				     &sds->isDischarging);
	}

	/* sanity check that remaining time exists (if it should) */
	is_present = hal_device_get_int (sds->udi,
			"battery.remaining_time", &seconds_remaining);
	if (!is_present && (sds->isDischarging || sds->isCharging)) {
		g_warning ("GNOME Power Manager could not read your battery's "
			   "remaining time. Please report this as a bug, "
			   "providing the information to: " GPMURL);
	} else if (seconds_remaining > 0) {
		/* we have to scale this to minutes */
		sds->minutesRemaining = seconds_remaining / 60;
	}

	/* sanity check that remaining time exists (if it should) */
	is_present = hal_device_get_int (sds->udi, "battery.charge_level.percentage",
					 &sds->percentageCharge);
	if (!is_present && (sds->isDischarging || sds->isCharging)) {
		g_warning ("GNOME Power Manager could not read your battery's "
			   "percentage charge. Please report this as a bug, "
			   "providing the information to: " GPMURL);
	}
	sysDevUpdate (sds->sd->type);
	return TRUE;
}

/** Invoked when a device is removed from the Global Device List.
 *  Removes any type of device from the state database and removes the
 *  watch on it's UDI.
 *
 *  @param	udi		The HAL UDI
 *  @return			If the icon should be refreshed
 */
gboolean
gpm_device_removed (const gchar *udi)
{
	sysDevStruct *sds = NULL;
	gboolean ret = FALSE;

	g_assert (udi);

	g_debug ("hal_device_removed: udi=%s", udi);
	/*
	 * UPS's/mice/keyboards don't use battery.present
	 * they just disappear from the device tree
	 */
	sysDevRemoveAll (udi);
	/* only update the correct device class */
	sds = sysDevFindAll (udi);
	if (sds) {
		sysDevUpdate (sds->sd->type);
		ret = TRUE;
	}
	/* remove watch */
	glibhal_watch_remove_device_property_modified (udi);
	return ret;
}

/** Invoked when device in the Global Device List acquires a new capability.
 *  Prints the name of the capability to stderr.
 *
 *  @param	udi		UDI
 *  @param	capability	Name of capability
 */
gboolean
gpm_device_new_capability (const gchar *udi, const gchar *capability)
{
	sysDevStruct *sds = NULL;

	/* assertion checks */
	g_assert (udi);
	g_assert (capability);
	g_debug ("hal_device_new_capability: udi=%s, capability=%s",
		 udi, capability);
	/*
	 * UPS's/mice/keyboards don't use battery.present
	 * they just appear in the device tree
	 */
	if (strcmp (capability, "battery") == 0) {
		gpm_add_battery (udi);
		/* only update the correct device class */
		sds = sysDevFindAll (udi);
		if (sds)
			sysDevUpdate (sds->sd->type);
	}
	return TRUE;
}
