/** @file	gpm-main.c
 *  @brief	GNOME Power Manager session daemon
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *  @note	Taken in part from:
 *  @note	lshal   (C) 2003 David Zeuthen, <david@fubar.dk>
 *  @note	notibat (C) 2004 Benjamin Kahn, <xkahn@zoned.net>
 *
 * This is the main daemon for g-p-m. It handles all the setup and
 * tear-down of all the dynamic arrays, mainloops and icons in g-p-m.
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
/**
 * @addtogroup	main		GNOME Power Manager (session daemon)
 * @brief			The session daemon run for each user
 *
 * @{
 */
/** @mainpage	GNOME Power Manager
 *
 *  @section	intro		Introduction
 *
 *  GNOME Power Manager is a session daemon that takes care of power management.
 *
 *  GNOME Power Manager uses information provided by HAL to display icons and 
 *  handle system and user actions in a GNOME session. Authorised users can set
 *  policy and change preferences.
 *  GNOME Power Manager acts as a policy agent on top of the Project Utopia
 *  stack, which includes the kernel, hotplug, udev, and HAL. 
 *  GNOME Power Manager listens for HAL events and responds with 
 *  user-configurable reactions.
 *  The main focus is the user interface; e.g. allowing configuration of
 *  power management from the desktop in a sane way (no need for root password,
 *  and no need to edit configuration files)
 *  Most of the backend code is actually in HAL for abstracting various power
 *  aware devices (UPS's) and frameworks (ACPI, PMU, APM etc.) - so the 
 *  desktop parts are fairly lightweight and straightforward.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <gdk/gdkx.h>
#include <gnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include "gpm-common.h"
#include "gpm-main.h"
#include "gpm-notification.h"
#include "gpm-idle.h"
#include "gpm-screensaver.h"
#include "gpm-libnotify.h"
#include "gpm-dbus-server.h"
#include "gpm-dbus-common.h"

#include "glibhal-main.h"
#include "glibhal-callback.h"
#include "glibhal-extras.h"
#include "gnome-power-glue.h"
#include "gpm-stock-icons.h"


/* no need for IPC with globals */
StateData state_data;
GPtrArray *objectData = NULL;
gboolean isVerbose;

/** Gets policy from gconf
 *
 *  @param	gconfpath	gconf policy name
 *  @return 			the int gconf value of the policy
 */
gint
get_policy_string (const gchar *gconfpath)
{
	GConfClient *client = NULL;
	gchar *valuestr = NULL;
	gint value;

	/* assertion checks */
	g_assert (gconfpath);

	client = gconf_client_get_default ();
	valuestr = gconf_client_get_string (client, gconfpath, NULL);
	if (!valuestr) {
		g_warning ("Cannot find %s, maybe a bug in the gconf schema!", gconfpath);
		return 0;
	}
	value = convert_string_to_policy (valuestr);

	g_free (valuestr);

	return value;
}

/** Callback for gconf modified keys (that we are watching).
 *
 * @param	client		A valid GConfClient
 * @param	cnxn_id		Unknown
 * @param	entry		The key that was modified
 * @param	user_data	user_data pointer. No function.
 */
static void
callback_gconf_key_changed (GConfClient *client,
	guint cnxn_id,
	GConfEntry *entry,
	gpointer user_data)
{
	gint value = 0;

	g_debug ("callback_gconf_key_changed (%s)", entry->key);

	if (gconf_entry_get_value (entry) == NULL)
		return;

	if (strcmp (entry->key, GCONF_ROOT "general/display_icon") == 0) {
		gpn_icon_update ();
	} else if (strcmp (entry->key, GCONF_ROOT "general/display_icon_full") == 0) {
		gpn_icon_update ();
	} else if (strcmp (entry->key, GCONF_ROOT "general/display_icon_others") == 0) {
		gpn_icon_update ();
	} else if (strcmp (entry->key, GCONF_ROOT "policy/battery/sleep_computer") == 0) {
		/* set new suspend timeouts */
		value = gconf_client_get_int (client, entry->key, NULL);
		if (state_data.onBatteryPower)
			gpm_idle_set_timeout (value);
	} else if (strcmp (entry->key, GCONF_ROOT "policy/ac/sleep_computer") == 0) {
		/* set new suspend timeouts */
		value = gconf_client_get_int (client, entry->key, NULL);
		if (!state_data.onBatteryPower)
			gpm_idle_set_timeout (value);
	} else if (strcmp (entry->key, GCONF_ROOT "policy/battery/sleep_display") == 0) {
		/* set new suspend timeouts */
		if (state_data.onBatteryPower) {
			value = gconf_client_get_int (client, entry->key, NULL);
			gscreensaver_set_dpms_timeout (value);
		}
	} else if (strcmp (entry->key, GCONF_ROOT "policy/ac/sleep_display") == 0) {
		/* set new suspend timeouts */
		if (!state_data.onBatteryPower) {
			value = gconf_client_get_int (client, entry->key, NULL);
			gscreensaver_set_dpms_timeout (value);
		}
	}

}

/** Do the action dictated by policy from gconf
 *
 *  @param	policy_number	The policy ENUM value
 *
 *  @todo	Add the actions to doxygen.
 */
void
action_policy_do (gint policy_number)
{
	gint value;
	GConfClient *client = gconf_client_get_default ();
	if (policy_number == ACTION_NOTHING) {
		g_debug ("*ACTION* Doing nothing");
	} else if (policy_number == ACTION_WARNING) {
		g_warning ("*ACTION* Send warning should be done locally!");
	} else if (policy_number == ACTION_REBOOT) {
		g_debug ("*ACTION* Reboot");
		run_gconf_script (GCONF_ROOT "general/cmd_reboot");
	} else if (policy_number == ACTION_SUSPEND) {
		g_debug ("*ACTION* Suspend");
		gscreensaver_lock_check ();
		hal_suspend (0);
	} else if (policy_number == ACTION_HIBERNATE) {
		g_debug ("*ACTION* Hibernate");
		gscreensaver_lock_check ();
		hal_hibernate ();
	} else if (policy_number == ACTION_SHUTDOWN) {
		g_debug ("*ACTION* Shutdown");
		run_gconf_script (GCONF_ROOT "general/cmd_shutdown");
	} else if (policy_number == ACTION_NOW_BATTERYPOWERED) {
		/*
		 * This case does 5 things:
		 *
		 * 1. Sets the brightness level
		 * 2. Sets HAL to be in LaptopMode (only if laptop)
		 * 3. Sets DPMS timeout to be our batteries policy value
		 * 4. Sets GNOME Screensaver to not run fancy screensavers
		 * 5. Sets our inactivity sleep timeout to batteries policy
		 */
		g_debug ("*DBUS* Now battery powered");
		/* set brightness and lowpower mode */
		value = gconf_client_get_int (client,
				GCONF_ROOT "policy/battery/brightness", NULL);
		hal_set_brightness_dim (value);
		hal_setlowpowermode (TRUE);
		/* set gnome screensaver dpms_suspend to our value */
		value = gconf_client_get_int (client,
				GCONF_ROOT "policy/battery/sleep_display", NULL);
		gscreensaver_set_dpms_timeout (value);
		/*
		 * make sure gnome-screensaver disables screensaving,
		 * and enables monitor shut-off instead
		 */
		gscreensaver_set_throttle (TRUE);
		/* set the new sleep (inactivity) value */
		value = gconf_client_get_int (client,
			GCONF_ROOT "policy/battery/sleep_computer", NULL);
		gpm_idle_set_timeout (value);
		/* emit siganal */
		gpm_emit_mains_changed (FALSE);
	} else if (policy_number == ACTION_NOW_MAINSPOWERED) {
		/*
		 * This case does 5 things:
		 *
		 * 1. Sets the brightness level
		 * 2. Sets HAL to not be in LaptopMode
		 * 3. Sets DPMS timeout to be our ac policy value
		 * 4. Sets GNOME Screensaver to run fancy screensavers
		 * 5. Sets our inactivity sleep timeout to ac policy
		 */
		g_debug ("*DBUS* Now mains powered");
		/* set brightness and lowpower mode */
		value = gconf_client_get_int (client,
			GCONF_ROOT "policy/ac/brightness", NULL);
		hal_set_brightness_dim (value);
		hal_setlowpowermode (TRUE);
		/* set dpms_suspend to our value */
		value = gconf_client_get_int (client,
				GCONF_ROOT "policy/ac/sleep_display", NULL);
		gscreensaver_set_dpms_timeout (value);
		/* make sure gnome-screensaver enables screensaving */
		gscreensaver_set_throttle (FALSE);
		/* set the new sleep (inactivity) value */
		value = gconf_client_get_int (client,
				GCONF_ROOT "policy/ac/sleep_computer", NULL);
		gpm_idle_set_timeout (value);
		/* emit siganal */
		gpm_emit_mains_changed (TRUE);
	} else
		g_warning ("action_policy_do called with unknown action %i",
			policy_number);
}

/** Recalculate logic of StateData, without any DBUS, all cached internally
 *
 *  @param	parray		The ObjectData array
 *  @param	coldplug	If set, send events even if they are the same
 */
static void
update_state_logic (GPtrArray *parray, gboolean coldplug)
{
	gint a;
	GenericObject *slotData = NULL;
	/* set up temp. state */
	StateData state_datanew;
	gboolean hasBatteries;
	gboolean hasAcAdapter;
	gint policy;

	/* assertion checks */
	g_assert (parray);

	state_datanew.onBatteryPower = FALSE;
	state_datanew.onUPSPower = FALSE;
	hasBatteries = FALSE;
	hasAcAdapter = FALSE;

	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		if (slotData->powerDevice == POWER_PRIMARY_BATTERY)
			if (slotData->present)
				hasBatteries = TRUE;
			else
				g_debug ("Battery '%s' missing?!?", slotData->udi);
		else if (slotData->powerDevice == POWER_AC_ADAPTER)
			hasAcAdapter = TRUE;
		if (slotData->powerDevice == POWER_UPS && slotData->isDischarging)
			state_datanew.onUPSPower = TRUE;
	}

	/* get old value */
	if (hasBatteries) {
		/* Reverse logic as only one ac_adapter is needed to be "on mains power" */
		for (a=0;a<parray->len;a++) {
			slotData = (GenericObject *) g_ptr_array_index (parray, a);
			if (slotData->powerDevice == POWER_AC_ADAPTER && !slotData->present) {
				g_debug ("onBatteryPower TRUE as ac_adapter not present");
				state_datanew.onBatteryPower = TRUE;
				break;
				}
		}
	} else {
		g_debug ("Cannot be on batteries if have none...");
		state_datanew.onBatteryPower = FALSE;
	}
	g_debug ("onBatteryPower = %i (coldplug=%i)", state_datanew.onBatteryPower, coldplug);

	if (coldplug || state_datanew.onBatteryPower != state_data.onBatteryPower) {
		state_data.onBatteryPower = state_datanew.onBatteryPower;
		if (state_data.onBatteryPower) {
			action_policy_do (ACTION_NOW_BATTERYPOWERED);
			policy = get_policy_string (GCONF_ROOT "policy/ac_fail");
			/* only do notification if not coldplug */
			if (!coldplug) {
				if (policy == ACTION_WARNING)
					libnotify_event (_("AC Power Unplugged"),
						_("The AC Power has been unplugged.  The system is now using battery power."),
						LIBNOTIFY_URGENCY_NORMAL, get_notification_icon ());
				else
					action_policy_do (policy);
				}
		} else {
			action_policy_do (ACTION_NOW_MAINSPOWERED);
		}
	}

	if (coldplug || state_datanew.onUPSPower != state_data.onUPSPower) {
		state_data.onUPSPower = state_datanew.onUPSPower;
		g_debug ("DBUS: %s = %i", "onUPSPower", state_data.onUPSPower);
	}
}

/** Generic exit
 *
 */
static void
gpm_exit (void)
{
	gint a;
	g_debug ("Quitting!");

	for (a=0;a<objectData->len;a++)
		g_free (g_ptr_array_index (objectData, a));
	g_ptr_array_free (objectData, TRUE);

	glibhal_callback_shutdown ();

	gpm_stock_icons_shutdown ();

	gpn_icon_destroy ();
	exit (0);
}

/** Adds an ac_adapter device. Also sets up properties on cached object
 *
 *  @param	udi		The HAL UDI
 *  @return			If we added a valid AC Adapter
 */
static gboolean
add_ac_adapter (const gchar *udi)
{
	GenericObject *slotData = NULL;

	/* assertion checks */
	g_assert (udi);

	slotData = genericobject_add (objectData, udi);
	if (!slotData) {
		g_warning ("Cannot add '%s' object to table!", udi);
		return FALSE;
	}

	/* register this with HAL so we get PropertyModified events */
	glibhal_watch_add_device_property_modified (udi);

	slotData->powerDevice = POWER_AC_ADAPTER;
	slotData->percentageCharge = 0;
	g_debug ("Device '%s' added", udi);
	/* ac_adapter batteries might be missing */
	hal_device_get_bool (udi, "ac_adapter.present", &slotData->present);
	slotData->isRechargeable = FALSE;
	slotData->isCharging = FALSE;
	slotData->isDischarging = FALSE;
	slotData->isRechargeable = 0;
	slotData->percentageCharge = 0;
	slotData->minutesRemaining = 0;
	return TRUE;
}

/** Adds an battery device. Also sets up properties on cached object
 *
 *  @param	slotData	The cached object
 *  @return			If battery is present
 */
static gboolean
read_battery_data (GenericObject *slotData)
{
	gint seconds_remaining;
	gboolean is_present;

	/* assertion checks */
	g_assert (slotData);

	/* initialise to known defaults */
	slotData->minutesRemaining = 0;
	slotData->percentageCharge = 0;
	slotData->isRechargeable = FALSE;
	slotData->isCharging = FALSE;
	slotData->isDischarging = FALSE;

	if (!slotData->present) {
		g_debug ("Battery %s not present!", slotData->udi);
		return FALSE;
	}

	/* battery might not be rechargeable, have to check */
	hal_device_get_bool (slotData->udi, "battery.is_rechargeable",
		&slotData->isRechargeable);
	if (slotData->isRechargeable) {
		hal_device_get_bool (slotData->udi, "battery.rechargeable.is_charging",
			&slotData->isCharging);
		hal_device_get_bool (slotData->udi, "battery.rechargeable.is_discharging",
			&slotData->isDischarging);
	}

	/* sanity check that remaining time exists (if it should) */
	is_present = hal_device_get_int (slotData->udi,
			"battery.remaining_time", &seconds_remaining);
	if (!is_present && (slotData->isDischarging || slotData->isCharging)) {
		g_warning ("GNOME Power Manager could not read your battery's remaining time.  "
				 "Please report this as a bug, providing the information to: "
				 GPMURL);
	} else if (seconds_remaining > 0) {
		/* we have to scale this to minutes */
		slotData->minutesRemaining = seconds_remaining / 60;
	}

	/* sanity check that remaining time exists (if it should) */
	is_present = hal_device_get_int (slotData->udi,
			"battery.charge_level.percentage", &slotData->percentageCharge);
	if (!is_present && (slotData->isDischarging || slotData->isCharging)) {
		g_warning ("GNOME Power Manager could not read your battery's percentage charge.  "
				 "Please report this as a bug, providing the information to: "
				 GPMURL);
	}
	return TRUE;
}

/** Adds a battery device, of any type. Also sets up properties on cached object
 *
 *  @param  udi			UDI
 *  @return			If we added a valid battery
 */
static gboolean
add_battery (const gchar *udi)
{
	gchar *type = NULL;
	gchar *device = NULL;
	GenericObject *slotData = NULL;

	/* assertion checks */
	g_assert (udi);

	slotData = genericobject_add (objectData, udi);
	if (!slotData) {
		g_debug ("Cannot add '%s' object to table!", udi);
		return FALSE;
	}

	/* PMU/ACPI batteries might be missing */
	hal_device_get_bool (udi, "battery.present", &slotData->present);

	/* battery is refined using the .type property */
	hal_device_get_string (udi, "battery.type", &type);
	if (!type) {
		g_warning ("Battery %s has no type!", udi);
		return FALSE;
	}

	/* register this with HAL so we get PropertyModified events */
	glibhal_watch_add_device_property_modified (udi);

	slotData->powerDevice = convert_haltype_to_powerdevice (type);

	g_free (type);

	device = convert_powerdevice_to_string (slotData->powerDevice);
	g_debug ("%s added", device);

	/* read in values */
	read_battery_data (slotData);
	return TRUE;
}

/** Coldplugs devices of type battery & ups at startup
 *
 *  @return			If any devices of capability battery were found.
 */
static gboolean
coldplug_batteries (void)
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
		add_battery (device_names[i]);
	hal_free_capability (device_names);
	return TRUE;
}

/** Coldplugs devices of type ac_adaptor at startup
 *
 *  @return			If any devices of capability ac_adapter were found.
 */
static gboolean
coldplug_acadapter (void)
{
	gint i;
	gchar **device_names = NULL;
	/* devices of type ac_adapter */
	hal_find_device_capability ("ac_adapter", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of ac_adapters");
		return FALSE;
	}
	for (i = 0; device_names[i]; i++)
		add_ac_adapter (device_names[i]);
	hal_free_capability (device_names);
	return TRUE;
}

/** Coldplugs devices of type ac_adaptor at startup
 *
 *  @return			If any devices of capability button were found.
 */
static gboolean
coldplug_buttons (void)
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

/** Invoked when a device is removed from the Global Device List.
 *  Removes any type of device from the objectData database and removes the
 *  watch on it's UDI.
 *
 *  @param	udi		The HAL UDI
 */
static void
hal_device_removed (const gchar *udi)
{
	int a;

	/* assertion checks */
	g_assert (udi);

	g_debug ("hal_device_removed: udi=%s", udi);
	/*
	 * UPS's/mice/keyboards don't use battery.present
	 * they just disappear from the device tree
	 */
	a = find_udi_parray_index (objectData, udi);
	if (a == -1) {
		g_debug ("Asked to remove '%s' when not present", udi);
		return;
	}
	g_debug ("Removed '%s'", udi);
	g_ptr_array_remove_index (objectData, a);
	/* remove watch */
	glibhal_watch_remove_device_property_modified (udi);
	/* our state has changed, update */
	update_state_logic (objectData, FALSE);
	gpn_icon_update ();
}

/** Invoked when device in the Global Device List acquires a new capability.
 *  Prints the name of the capability to stderr.
 *
 *  @param	udi		UDI
 *  @param	capability	Name of capability
 */
static void
hal_device_new_capability (const gchar *udi, const gchar *capability)
{
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
		add_battery (udi);
		/* our state has changed, update */
		update_state_logic (objectData, FALSE);
		gpn_icon_update ();
	}
}

/** Invoked when device in the Global Device List acquires a new capability.
 *  Prints the name of the capability to stderr.
 *
 *  @param	slotData	Data structure
 *  @param	newCharge	New charge value (%)
 *  @return			If a warning was sent
 */
static gboolean
notify_user_low_batt (GenericObject *slotData, gint newCharge)
{
	GConfClient *client = NULL;
	gint lowThreshold;
	gint criticalThreshold;
	gchar *message = NULL;
	gchar *remaining = NULL;

	/* assertion checks */
	g_assert (slotData);

	if (!slotData->isDischarging) {
		g_debug ("battery is not discharging!");
		return FALSE;
	}

	client = gconf_client_get_default ();
	lowThreshold = gconf_client_get_int (client,
		GCONF_ROOT "general/threshold_low", NULL);
	criticalThreshold = gconf_client_get_int (client,
		GCONF_ROOT "general/threshold_critical", NULL);
	g_debug ("lowThreshold = %i, criticalThreshold = %i",
		lowThreshold, criticalThreshold);

	/* critical warning */
	if (newCharge < criticalThreshold) {
		g_debug ("battery is critical!");
		gint policy = get_policy_string (GCONF_ROOT "policy/battery_critical");
		if (policy == ACTION_WARNING) {
			remaining = get_time_string (slotData);
			g_assert (remaining);
			message = g_strdup_printf (
				_("You have approximately <b>%s</b> of remaining battery life (%i%%).  Plug in your AC Adapter to avoid losing data."),
				remaining, newCharge);
			libnotify_event (_("Battery Critically Low"), message, LIBNOTIFY_URGENCY_CRITICAL,
				get_notification_icon ());
			g_free (message);
			g_free (remaining);
		} else
			action_policy_do (policy);
		return TRUE;
	}

	/* low warning */
	if (newCharge < lowThreshold) {
		g_debug ("battery is low!");
		remaining = get_time_string (slotData);
		g_assert (remaining);
		message = g_strdup_printf (
			_("You have approximately <b>%s</b> of remaining battery life (%i%%).  Plug in your AC Adapter to avoid losing data."),
			remaining, newCharge);
		libnotify_event (_("Battery Low"), message, LIBNOTIFY_URGENCY_CRITICAL,
			get_notification_icon ());
		g_free (message);
		g_free (remaining);
		return TRUE;
	}
	return FALSE;
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi		The HAL UDI
 *  @param	key		Property key
 *  @param	is_added	If the key was added
 *  @param	is_removed	If the key was removed
 */
static void
hal_device_property_modified (const gchar *udi,
	const gchar *key,
	gboolean is_added,
	gboolean is_removed)
{
	GenericObject *slotData = NULL;
	GenericObject slotDataVirt;
	gboolean updateState;
	gint oldCharge;
	gint newCharge;

	/* assertion checks */
	g_assert (udi);
	g_assert (key);

	g_debug ("hal_device_property_modified: udi=%s, key=%s, added=%i, removed=%i",
		udi, key, is_added, is_removed);

	/* only process modified entries, not added or removed keys */
	if (is_removed||is_added)
		return;

	/* no point continuing any further if we are never going to match ...*/
	if (strncmp (key, "battery", 7) != 0 && strncmp (key, "ac_adapter", 10) != 0)
		return;

	slotData = genericobject_find (objectData, udi);
	/*
	 * if we BUG here then *HAL* has a problem where key modification is
	 * done before capability is present
	 */
	if (!slotData) {
		g_warning ("slotData is NULL! udi=%s\n"
			   "This is probably a bug in HAL where we are getting "
			   "is_removed=false, is_added=false before the capability "
			   "had been added. In addon-hid-ups this is likely to happen."
			   , udi);
		return;
	}
	updateState = FALSE;

	/* find old percentageCharge (taking into account multi-device machines) */
	if (slotData->isRechargeable) {
		slotDataVirt.percentageCharge = 100;
		create_virtual_of_type (objectData, &slotDataVirt, slotData->powerDevice);
		oldCharge = slotDataVirt.percentageCharge;
	} else
		oldCharge = slotData->percentageCharge;

	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		hal_device_get_bool (udi, key, &slotData->present);
		/* read in values */
		read_battery_data (slotData);
		updateState = TRUE;
	} else if (strcmp (key, "ac_adapter.present") == 0) {
		hal_device_get_bool (udi, key, &slotData->present);
		updateState = TRUE;
	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		hal_device_get_bool (udi, key, &slotData->isCharging);
		/*
		 * invalidate the remaining time, as we need to wait for
		 * the next HAL update. This is a HAL bug I think.
		 */
		slotData->minutesRemaining = 0;
		updateState = TRUE;
	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		hal_device_get_bool (udi, key, &slotData->isDischarging);
		/* invalidate the remaining time */
		slotData->minutesRemaining = 0;
		updateState = TRUE;
	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		hal_device_get_int (udi, key, &slotData->percentageCharge);
		/* give notification @100% */
		if (slotData->percentageCharge == 100) {
			libnotify_event (_("Battery Charged"), _("Your battery is now fully charged"),
					 LIBNOTIFY_URGENCY_LOW,
					 get_notification_icon ());
		}
	} else if (strcmp (key, "battery.remaining_time") == 0) {
		gint tempval;
		hal_device_get_int (udi, key, &tempval);
		if (tempval > 0)
			slotData->minutesRemaining = tempval / 60;
	} else
		/* ignore */
		return;

	if (updateState)
		update_state_logic (objectData, FALSE);

	/* find new percentageCharge (taking into account multi-device machines) */
	if (slotData->isRechargeable) {
		slotDataVirt.percentageCharge = 100;
		create_virtual_of_type (objectData, &slotDataVirt, slotData->powerDevice);
		newCharge = slotDataVirt.percentageCharge;
	} else
		newCharge = slotData->percentageCharge;
	g_debug ("newCharge = %i, oldCharge = %i", newCharge, oldCharge);

	/* update icon */
	gpn_icon_update ();

	/* do we need to notify the user we are getting low ? */
	if (oldCharge != newCharge) {
		g_debug ("percentage change %i -> %i", oldCharge, newCharge);
		notify_user_low_batt (slotData, newCharge);
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi			Univerisal Device Id
 *  @param	condition_name		Name of condition
 *  @param	condition_details	D-BUS message with parameters
 */
static void
hal_device_condition (const gchar *udi,
	const gchar *condition_name,
	const gchar *condition_details)
{
	gchar *type = NULL;
	gint policy;
	gboolean value;

	/* assertion checks */
	g_assert (udi);
	g_assert (condition_name);
	g_assert (condition_details);

	g_debug ("hal_device_condition: udi=%s, condition_name=%s, condition_details=%s",
		udi, condition_name, condition_details);

	if (strcmp (condition_name, "ButtonPressed") == 0) {
		hal_device_get_string (udi, "button.type", &type);
		g_debug ("ButtonPressed : %s", type);
		if (strcmp (type, "power") == 0) {
			policy = get_policy_string (GCONF_ROOT "policy/button_power");
			action_policy_do (policy);
		} else if (strcmp (type, "sleep") == 0) {
			policy = get_policy_string (GCONF_ROOT "policy/button_suspend");
			action_policy_do (policy);
		} else if (strcmp (type, "lid") == 0) {
			/* we only do a lid event when the lid is OPENED */
			hal_device_get_bool (udi, "button.state.value", &value);
			if (value) {
				gint policy = get_policy_string (GCONF_ROOT "policy/button_lid");
				action_policy_do (policy);
			}
		} else
			g_warning ("Button '%s' unrecognised", type);

		g_free (type);
	}
}

/** Prints program usage.
 *
 */
static void
print_usage (void)
{
	g_print ("usage : gnome-power-manager [options]\n");
	g_print (
		"\n"
		"    --disable        Do not perform the action, e.g. suspend\n"
		"    --no-daemon      Do not daemonize.\n"
		"    --verbose        Show extra debugging\n"
		"    --version        Show the installed version and quit\n"
		"    --help           Show this information and exit\n"
		"\n");
}

/** Callback for the idle function.
 *
 *  @param	timeout		Time in minutes that computer has been idle
 */
void
idle_callback (gint timeout)
{
	gint policy;

	policy = get_policy_string (GCONF_ROOT "policy/ac_fail");

	/* can only be hibernate or suspend */
	action_policy_do (policy);
}

/** Callback for the DBUS NameOwnerChanged function.
 *
 *  @param	name		The DBUS name, e.g. org.freedesktop.Hal
 *  @param	connected	Time in minutes that computer has been idle
 */
static void
signalhandler_noc (const char *name, gboolean connected)
{
	/* ignore that don't all apply */
	if (strcmp (name, "org.freedesktop.Hal") != 0)
		return;

	if (!connected) {
		g_critical ("HAL has been disconnected!  GNOME Power Manager will now quit.");

		/* for now, quit */
		gpm_exit ();
		return;
	}
	/** @todo: handle reconnection to the HAL bus */
	g_warning ("hal re-connected\n");
}

/** Main entry point
 *
 *  @param	argc		Number of arguments given to program
 *  @param	argv		Arguments given to program
 *  @return			Return code
 */
int
main (int argc, char *argv[])
{
	gint a;
	gint value;
	GMainLoop *loop = NULL;
	GConfClient *client = NULL;
	GnomeClient *master = NULL;
	GnomeClientFlags flags;
	DBusGConnection *system_connection = NULL;
	DBusGConnection *session_connection = NULL;
	gboolean no_daemon = FALSE;

	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();
	dbus_g_object_type_install_info (gpm_object_get_type (),
		&dbus_glib_gpm_object_object_info);

	gconf_init (argc, argv, NULL);
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, GCONF_ROOT_SANS_SLASH,
		GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (client, GCONF_ROOT_SANS_SLASH,
		callback_gconf_key_changed, NULL, NULL, NULL);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	isVerbose = FALSE;
	for (a=1; a < argc; a++) {
		if (strcmp (argv[a], "--verbose") == 0)
			isVerbose = TRUE;
		else if (strcmp (argv[a], "--version") == 0) {
			g_print ("%s %s\n", NICENAME, VERSION);
			return EXIT_SUCCESS;
		}
		else if (strcmp (argv[a], "--no-daemon") == 0)
			no_daemon = TRUE;
		else if (strcmp (argv[a], "--help") == 0) {
			print_usage ();
			return EXIT_SUCCESS;
		}
	}

	/* set log level */
	if (!isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			g_log_ignore, NULL);

	/* check dbus connections, exit if not valid */
	if (!dbus_get_system_connection (&system_connection))
		exit (1);
	if (!dbus_get_session_connection (&session_connection))
		exit (1);

	/* initialise gnome */
	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);
	master = gnome_master_client ();
	flags = gnome_client_get_flags (master);
	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		/* We'll disable this as users are getting constant crashes */
		/* gnome_client_set_restart_style (master, GNOME_RESTART_IMMEDIATELY);*/
		gnome_client_flush (master);
	}
	g_signal_connect (GTK_OBJECT (master), "die", G_CALLBACK (gpm_exit), NULL);

	/* Initialise libnotify, if compiled in. */
	if (!libnotify_init (NICENAME))
		g_error ("Cannot initialise libnotify!");

	gpm_stock_icons_init();

	if (!no_daemon && daemon (0, 0))
		g_error ("Could not daemonize: %s", g_strerror (errno));

	/* register dbus service */
	if (!gpm_object_register ()) {
		g_warning ("GNOME Power Manager is already running in this session.");
		return 0;
	}

	loop = g_main_loop_new (NULL, FALSE);
	/* check HAL is running */
	if (!is_hald_running ()) {
		g_critical ("GNOME Power Manager cannot connect to HAL!");
		exit (1);
	}

	/* check we have PM capability */
	if (!hal_pm_check ()) {
		g_critical ("HAL does not have PowerManagement capability");
		exit (1);
	}

	glibhal_callback_init ();
	/* assign the callback functions */
	glibhal_method_device_removed (hal_device_removed);
	glibhal_method_device_new_capability (hal_device_new_capability);
	glibhal_method_device_property_modified (hal_device_property_modified);
	glibhal_method_device_condition (hal_device_condition);
	/* sets up NameOwnerChanged notification */
	glibhal_method_noc (signalhandler_noc);

	objectData = g_ptr_array_new ();

	/* sets up these devices in the objectData and adds watches */
	coldplug_batteries ();
	coldplug_acadapter ();
	coldplug_buttons ();

	update_state_logic (objectData, TRUE);
	gpn_icon_update ();

	/* get idle value from gconf */
	if (state_data.onBatteryPower) {
		value = gconf_client_get_int (client, 
			GCONF_ROOT "policy/battery/sleep_computer", NULL);
	} else {
		value = gconf_client_get_int (client,
			GCONF_ROOT "policy/ac/sleep_computer", NULL);
	}
	gpm_idle_set_timeout (value);
	
	/* set callback for the timout action */
	gpm_idle_set_callback (idle_callback);

	/* set up idle calculation function */
	g_timeout_add (POLL_FREQUENCY * 1000, gpm_idle_update, NULL);

	g_main_loop_run (loop);

	gpm_exit ();
	return 0;
}
/** @} */
