/***************************************************************************
 *
 * gpm-main.c : GNOME Power Manager
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 *
 * Taken in part from:
 * - lshal   (C) 2003 David Zeuthen, <david@fubar.dk>
 * - notibat (C) 2004 Benjamin Kahn, <xkahn@zoned.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

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
#if HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include <libhal.h>
#include "gpm-common.h"
#include "gpm-main.h"
#include "gpm-notification.h"
#include "gpm-dbus-common.h"
#include "gpm-dbus-server.h"

/* static */
static LibHalContext *hal_ctx;

/* no need for IPC with globals */
HasData has_data;
StateData state_data;
SetupData setup;
GPtrArray *objectData = NULL;
GPtrArray *registered = NULL;

DBusConnection *connsession = NULL;


/** Convenience function to call libnotify
 *
 *  @param  content		The content text, e.g. "Battery low"
 *  @param  value		The urgency, e.g NOTIFY_URGENCY_CRITICAL
 */
static void
use_libnotify (const char *content, const int urgency)
{
#if HAVE_LIBNOTIFY
	gint x, y;
	gboolean use_hints;
	use_hints = get_icon_position (&x, &y);
	NotifyIcon *icon = notify_icon_new_from_uri (GPM_DATA "gnome-power.png");
	GHashTable *hints = NULL;
	if (use_hints) {
		hints = notify_hints_new();
		notify_hints_set_int (hints, "x", x);
		notify_hints_set_int (hints, "y", y);
		if (urgency == NOTIFY_URGENCY_CRITICAL)
			notify_hints_set_string (hints, "sound-file", GPM_DATA "critical.wav");
		else
			notify_hints_set_string (hints, "sound-file", GPM_DATA "normal.wav");
	}
	const char *summary = NICENAME;
	NotifyHandle *n = notify_send_notification (NULL, /* replaces nothing 	*/
			   NULL,
			   urgency,
			   summary, content,
			   icon, /* no icon 			*/
			   TRUE, NOTIFY_TIMOUT,
			   hints,
			   NULL, /* no user data 		*/
			   0);   /* no actions 			*/
	notify_icon_destroy(icon);	
	if (!n)
		g_warning ("failed to send notification (%s)", content);
#else
	GtkWidget *widget;
	widget = gnome_message_box_new (content, 
                                GNOME_MESSAGE_BOX_WARNING,
                                GNOME_STOCK_BUTTON_OK, 
                                NULL);
	gtk_window_set_title (GTK_WINDOW (widget), NICENAME);
	gtk_widget_show (widget);
#endif
}

/** Convenience function.
 *  Prints errors due to wrong values expected (exposes bugs, rather than hides them)
 */
static void
dbus_error_print (DBusError *error)
{
	g_return_if_fail (error);
	if (dbus_error_is_set (error)) {
		g_warning ("DBUS:%s", error->message);
		dbus_error_free (error);
	}
}

/** Do an interactive alert
 *
 *  @param  text		the text to be used in the dialogue
 * 				e.g. "Logitech MX-1000 mouse"
 */
static void
do_interactive_alert (const gchar *text)
{
	g_return_if_fail (text);

	GtkWidget *widget = gnome_message_box_new (text, 
                                GNOME_MESSAGE_BOX_WARNING,
                                GNOME_STOCK_BUTTON_OK, 
                                NULL);
	gtk_window_set_title (GTK_WINDOW (widget), NICENAME);
	gtk_widget_show (widget);
}

/** Gets policy from gconf
 *
 *  @param  name		gconf policy name
 *  @return 			the int gconf value of the policy
 */
gint
get_policy_string (const gchar *gconfpath)
{
	g_return_val_if_fail (gconfpath, -1);
	GConfClient *client = gconf_client_get_default ();
	gchar *valuestr = gconf_client_get_string (client, gconfpath, NULL);
	gint value = convert_string_to_policy (valuestr);
	g_free (valuestr);
	return value;
}

static void
run_gconf_script (const char *path)
{
	GConfClient *client = gconf_client_get_default ();
	gchar *command = gconf_client_get_string (client, path, NULL);
	if (command) {
		g_debug ("Executing '%s'", command);
		if (setup.doAction && !g_spawn_command_line_async (command, NULL))
			g_warning ("Couldn't execute '%s'.", command);
		g_free (command);
	} else
		g_warning ("'%s' is missing!", path);
}

static gboolean
dbus_action (gint action)
{
	dbus_send_signal_int (connsession, "actionAboutToHappen", action);

	RegProgram *regprog = NULL;
	int a;
	const int maxwait = 5;

	gboolean allACK = FALSE;
	gboolean anyNACK = FALSE;

	if (registered->len == 0) {
		g_debug ("No connected clients");
		return TRUE;
	}
	g_debug ("Registered clients = %i\n", registered->len);
	
	GTimer* gt = g_timer_new ();
	do {

		g_main_context_iteration (NULL, TRUE);
		if (!g_main_context_pending (NULL))
			g_usleep (100*1000);

		allACK = TRUE;
		for (a=0;a<registered->len;a++) {
			regprog = (RegProgram *) g_ptr_array_index (registered, a);
			if (!regprog->isACK && !regprog->isNACK) {
				allACK = FALSE;
				break;
			}
			if (regprog->isACK)
				g_debug ("ACK!\n");
			if (regprog->isNACK) {
				g_debug ("NACK!\n");
				anyNACK = TRUE;
				break;
			}
		}
		if (allACK || anyNACK)
			break;
	} while (g_timer_elapsed (gt, NULL) < maxwait);

	regprog->isACK = FALSE;
	regprog->isNACK = FALSE;

	if (anyNACK) {
		GString *gs = g_string_new ("");
		gchar *actionstr = convert_dbus_enum_to_string (action);
		g_string_printf (gs, _("The program '%s' is preventing the %s "
				       "from occurring.\n\n"
				       "The explaination given is: %s"), 
				     regprog->appName->str, actionstr, regprog->reason->str);
		g_message ("%s", gs->str);
		use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
		g_string_free (gs, TRUE);
		return FALSE;
	}
	if (!allACK) {
		GString *gs = g_string_new ("");
		char *actionstr = convert_policy_to_string (action);
		g_string_printf (gs, _("The program '%s' has not returned data that "
				     "is preventing the %s from occurring."),
				     regprog->appName->str, actionstr);
		g_message ("%s", gs->str);
		use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
		g_string_free (gs, TRUE);
		return FALSE;
	}
	dbus_send_signal_int (connsession, "performingAction", action);
	return TRUE;
}

static void
pm_do_action (const gchar *action)
{
	g_debug ("action = %s\n", action);
	DBusConnection *connection;
	DBusError error;
	gboolean boolvalue = FALSE;
	dbus_error_init (&error);
	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

	if (setup.doAction && !get_bool_value_pm (connection, action, &boolvalue)) {
		GString *gs = g_string_new ("bug");
		g_string_printf (gs, _("PowerManager service is not running.\n"
				     "%s cannot perform a %s."), NICENAME, action);
		use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
		g_string_free (gs, TRUE);
	} else if (!boolvalue)
		g_warning ("%s failed", action);
}

/** For this specific hard-drive, set the spin-down timeout
 *
 *  @param  device		The device, e.g. /dev/hda
 *  @param  minutes		How many minutes to set the spin-down for
 */
static void
set_hdd_spindown_device (gchar *device, int minutes)
{
	DBusConnection *connection;
	DBusError error;
	gboolean boolvalue = FALSE;
	dbus_error_init (&error);
	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

	if (setup.doAction && !get_bool_value_pm_int_string (connection, "hdparm", &boolvalue, minutes, device)) {
		GString *gs = g_string_new ("bug");
		g_string_printf (gs, _("PowerManager service is not running.\n"
				     "%s cannot perform hard-drive timout changes."), NICENAME);
		use_libnotify (gs->str, NOTIFY_URGENCY_NORMAL);
		g_string_free (gs, TRUE);
	} else if (!boolvalue)
		g_warning ("hard-drive timout change failed");

}

/** For each hard-drive in the system, set the spin-down timeout
 *
 *  @param  minutes		How many minutes to set the spin-down for
 */
static void
set_hdd_spindown (int minutes)
{
	gint i, num_devices;
	char **device_names;
	DBusError error;

	/* find devices of type hard-disks from HAL */
	dbus_error_init (&error);
	device_names = libhal_find_device_by_capability (hal_ctx, "storage", 
					&num_devices, &error);
	dbus_error_print (&error);
	if (device_names == NULL)
		g_warning ("Couldn't obtain list of storage");
	for (i = 0; i < num_devices; i++) {
		char *udi = device_names[i];
		dbus_error_init (&error);
		gchar *type = libhal_device_get_property_string (hal_ctx, udi, "storage.drive_type", &error);
		dbus_error_print (&error);
		if (strcmp (type, "disk") == 0) {
			dbus_error_init (&error);
			gchar *device = libhal_device_get_property_string (hal_ctx, udi, "block.device", &error);
			dbus_error_print (&error);
			g_debug ("Setting device %s to sleep after %i minutes\n", device, minutes);
			set_hdd_spindown_device (device, minutes);
			libhal_free_string (device);
		}
		libhal_free_string (type);
	}
	libhal_free_string_array (device_names);
}

/** Do the action dictated by policy from gconf
 *
 *  @param  policy_number	What to do!
 */
void
action_policy_do (gint policy_number)
{
	if (policy_number == ACTION_NOTHING) {
		g_debug ("*ACTION* Doing nothing");
	} else if (policy_number == ACTION_WARNING) {
		g_warning ("*ACTION* Send warning should be done locally!");
		do_interactive_alert ("Warning (should be done locally)!");
	} else if (policy_number == ACTION_REBOOT) {
		g_debug ("*ACTION* Reboot");
		if (dbus_action (GPM_DBUS_POWEROFF))
			pm_do_action ("restart");
	} else if (policy_number == ACTION_SUSPEND) {
		g_debug ("*ACTION* Suspend");
		if (dbus_action (GPM_DBUS_SUSPEND))
			pm_do_action ("suspend");
	} else if (policy_number == ACTION_HIBERNATE) {
		g_debug ("*ACTION* Hibernate");
		if (dbus_action (GPM_DBUS_HIBERNATE))
			pm_do_action ("hibernate");
	} else if (policy_number == ACTION_SHUTDOWN) {
		g_debug ("*ACTION* Shutdown");
		if (dbus_action (GPM_DBUS_POWEROFF))
			pm_do_action ("shutdown");
	} else if (policy_number == ACTION_BATTERY_CHARGE) {
		g_debug ("*ACTION* Battery Charging");
	} else if (policy_number == ACTION_BATTERY_DISCHARGE) {
		g_debug ("*ACTION* Battery Discharging");
	} else if (policy_number == ACTION_NOW_BATTERYPOWERED) {
		g_debug ("*DBUS* Now battery powered");
		/* spin down the hard-drives */
		GConfClient *client = gconf_client_get_default ();
		gint value = gconf_client_get_int (client, 
			GCONF_ROOT "policy/Batteries/SleepHardDrive", NULL);
		set_hdd_spindown (value);
		/* set dpms_suspend to our value */
		gint displaytimout = gconf_client_get_int (client, 
			GCONF_ROOT "policy/Batteries/SleepDisplay", NULL);
		gconf_client_set_int (client, 
			"/apps/gnome-screensaver/dpms_suspend", displaytimout, NULL);
		dbus_send_signal_bool (connsession, "mainsStatusChanged", FALSE);
	} else if (policy_number == ACTION_NOW_MAINSPOWERED) {
		g_debug ("*DBUS* Now mains powered");
		/* spin down the hard-drives */
		GConfClient *client = gconf_client_get_default ();
		gint value = gconf_client_get_int (client, 
			GCONF_ROOT "policy/AC/SleepHardDrive", NULL);
		set_hdd_spindown (value);
		/* set dpms_suspend to our value */
		gint displaytimout = gconf_client_get_int (client, 
			GCONF_ROOT "policy/Batteries/SleepDisplay", NULL);
		gconf_client_set_int (client, 
			"/apps/gnome-screensaver/dpms_suspend", displaytimout, NULL);
		dbus_send_signal_bool (connsession, "mainsStatusChanged", TRUE);
	} else
		g_warning ("action_policy_do called with unknown action %i", 
			policy_number);
}

/** Compare the old and the new values, if different or force'd then updates gconf
 *
 *  @param  name		general gconf name, e.g. hasButtons
 */
static void
compare_bool_set_gconf (const gchar *gconfpath, gboolean *has_dataold, gboolean *has_datanew, gboolean force)
{
	g_return_if_fail (gconfpath);
	GConfClient *client = gconf_client_get_default ();

	if (force || *has_datanew != *has_dataold) {
		*has_dataold = *has_datanew;
		gconf_client_set_bool (client, gconfpath, *has_datanew, NULL);
		g_debug ("%s = %i", gconfpath, *has_datanew);
	}
}

/** Recalculate logic of StateData, without any DBUS, all cached internally
 * Exported DBUS interface values goes here :-)
 *  @param  coldplug		If set, send events even if they are the same
 */
static void
update_state_logic (GPtrArray *parray, gboolean coldplug)
{
	g_return_if_fail (parray);
	gint a;
	GenericObject *slotData;
	/* set up temp. state */
	StateData state_datanew;
	state_datanew.onBatteryPower = FALSE;
	state_datanew.onUPSPower = FALSE;

	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		if (slotData->powerDevice == POWER_UPS && slotData->isDischarging)
			state_datanew.onUPSPower = TRUE;
		if (slotData->powerDevice == POWER_PRIMARY_BATTERY && slotData->isDischarging)
			state_datanew.onBatteryPower = TRUE;
	}

	/* get old value */
	if (has_data.hasBatteries == TRUE) {
		/* Reverse logic as only one ac_adapter is needed to be "on mains power" */
		for (a=0;a<parray->len;a++) {
			slotData = (GenericObject *) g_ptr_array_index (parray, a);
			if (slotData->powerDevice == POWER_AC_ADAPTER && slotData->present) {
				g_debug ("onBatteryPower FALSE as ac_adapter present");
				state_datanew.onBatteryPower = FALSE;
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
			int policy = get_policy_string (GCONF_ROOT "policy/ACFail");
			/* only do notification if not coldplug */
			if (!coldplug) {
				if (policy == ACTION_WARNING)
			use_libnotify (_("AC Adapter has been removed"), NOTIFY_URGENCY_NORMAL);
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

/** Recalculate logic of HasData, without any DBUS, all cached internally
 *  All complicated convoluted logic goes here :-)
 *  @param  coldplug		If set, send events even if they are the same
 */
static void
update_has_logic (GPtrArray *parray, gboolean coldplug)
{
	g_return_if_fail (parray);

	gint a;
	GenericObject *slotData;
	/* set up temp. state */
	HasData has_datanew;
	has_datanew.hasBatteries = FALSE;
	has_datanew.hasAcAdapter = FALSE;
	has_datanew.hasButtonPower = TRUE;
	has_datanew.hasButtonSleep = TRUE;
	has_datanew.hasButtonLid = TRUE;
	has_datanew.hasUPS = FALSE;
	has_datanew.hasDisplays = TRUE;
	has_datanew.hasHardDrive = TRUE;
	has_datanew.hasLCD = FALSE;

	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		if (slotData->powerDevice == POWER_UPS)
			has_datanew.hasUPS = TRUE;
		else if (slotData->powerDevice == POWER_MOUSE)
			has_datanew.hasMouse = TRUE;
		else if (slotData->powerDevice == POWER_KEYBOARD)
			has_datanew.hasKeyboard = TRUE;
		else if (slotData->powerDevice == POWER_PRIMARY_BATTERY)
			if (slotData->present)
				has_datanew.hasBatteries = TRUE;
			else
				g_debug ("Battery missing?!?");
		else if (slotData->powerDevice == POWER_AC_ADAPTER)
			has_datanew.hasAcAdapter = TRUE;
	}
	compare_bool_set_gconf (GCONF_ROOT "general/hasUPS", 
		&has_data.hasUPS, &has_datanew.hasUPS, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasButtonPower", 
		&has_data.hasButtonPower, &has_datanew.hasButtonPower, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasButtonSleep", 
		&has_data.hasButtonSleep, &has_datanew.hasButtonSleep, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasButtonLid", 
		&has_data.hasButtonLid, &has_datanew.hasButtonLid, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasAcAdapter", 
		&has_data.hasAcAdapter, &has_datanew.hasAcAdapter, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasBatteries", 
		&has_data.hasBatteries, &has_datanew.hasBatteries, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasLCD", 
		&has_data.hasLCD, &has_datanew.hasLCD, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasHardDrive", 
		&has_data.hasHardDrive, &has_datanew.hasHardDrive, coldplug);
	compare_bool_set_gconf (GCONF_ROOT "general/hasDisplays", 
		&has_data.hasDisplays, &has_datanew.hasDisplays, coldplug);
}

/** Generic exit
 *
 */
static void
gpm_exit (void)
{
	g_debug ("Quitting!");
	gpn_icon_destroy ();
	exit (0);
}

/** Prints the objectData table with any parameters
 *
 *  @param  parray		pointer array to GenericObject
 */
void
genericobject_print (GPtrArray *parray)
{
	g_return_if_fail (parray);
	int a;
	GenericObject *slotData;
	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		g_return_if_fail (slotData);
		g_print ("[%i] udi: %s\n", a, slotData->udi);
		g_print ("     powerDevice: %i\n", slotData->powerDevice);
	}
}

/** Finds a device from the objectData table
 *
 *  @param  parray		pointer array to GenericObject
 *  @param  udi			HAL UDI
 */
static gint
find_udi_parray_index (GPtrArray *parray, const char *udi)
{
	GenericObject *slotData;
	int a;
	for (a=0;a<parray->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (parray, a);
		g_return_val_if_fail (slotData, -1);
		if (strcmp (slotData->udi, udi) == 0)
			return a;
	}
	return -1;
}

/** Finds a device from the objectData table
 *
 *  @param  parray		pointer array to GenericObject
 *  @param  udi			HAL UDI
 */
static GenericObject *
genericobject_find (GPtrArray *parray, const char *udi)
{
	int a = find_udi_parray_index (parray, udi);
	if (a != -1)
		return (GenericObject *) g_ptr_array_index (parray, a);
	return NULL;
}

/** Adds a device to the objectData table *IF DOES NOT EXIST*
 *
 *  @param  parray		pointer array to GenericObject
 *  @param  udi			HAL UDI
 *  @return			TRUE if we added to the table
 */
static GenericObject *
genericobject_add (GPtrArray *parray, const char *udi)
{
	int a = find_udi_parray_index (parray, udi);
	if (a != -1)
		return NULL;

	GenericObject *slotData = g_new (GenericObject, 1);
	strcpy (slotData->udi, udi);
	slotData->powerDevice = POWER_UNKNOWN;
	slotData->slot = a;
	g_ptr_array_add (parray, (gpointer) slotData);
	return slotData;
}

/** Adds a ac_adapter device. Also sets up properties on cached object
 *
 *  @param  udi			UDI
 */
static void
add_ac_adapter (const gchar *udi)
{
	DBusError error;
	g_return_if_fail (udi);
	GenericObject *slotData = genericobject_add (objectData, udi);
	if (slotData) {
		slotData->powerDevice = POWER_AC_ADAPTER;
		slotData->percentageCharge = 0;
		g_debug ("Device '%s' added", udi);
		/* ac_adapter batteries might be missing */
		dbus_error_init (&error);
		slotData->present = libhal_device_get_property_bool (hal_ctx,
			udi, "ac_adapter.present", &error);
		dbus_error_print (&error);
		slotData->isRechargeable = FALSE;
		slotData->isCharging = FALSE;
		slotData->isDischarging = FALSE;
		slotData->rawLastFull = 0;
		slotData->rawCharge = 0;
		slotData->isRechargeable = 0;
		slotData->percentageCharge = 0;
		slotData->minutesRemaining = 0;
	} else
		g_warning ("Device '%s' already added!", udi);
}

static void
read_battery_data (GenericObject *slotData)
{
	g_return_if_fail (slotData);
	DBusError error;

	/* initialise to known defaults */
	slotData->minutesRemaining = 0;
	slotData->rawCharge = 0;
	slotData->rawLastFull = 0;
	slotData->isRechargeable = FALSE;
	slotData->isCharging = FALSE;
	slotData->isDischarging = FALSE;

	if (!slotData->present) {
		g_debug ("Battery %s not present!", slotData->udi);
		return;
	}

	/* set cached variables up */
	dbus_error_init (&error);
	slotData->minutesRemaining = libhal_device_get_property_int (hal_ctx,
		slotData->udi, "battery.remaining_time", &error) / 60;
	dbus_error_print (&error);

	/*
	 * We need the RAW readings so we keep functions modular and 
	 * acpi/apm neutral
	 */
	dbus_error_init (&error);
	slotData->rawCharge = libhal_device_get_property_int (hal_ctx,
		slotData->udi, "battery.charge_level.current", &error);
	dbus_error_print (&error);

	/* 
	 * We need the RAW readings so we can process them later on
	 * for time remaining
	 */
	dbus_error_init (&error);
	slotData->rawLastFull = libhal_device_get_property_int (hal_ctx,
		slotData->udi, "battery.charge_level.last_full", &error);
	dbus_error_print (&error);

	/* battery might not be rechargeable, have to check */
	dbus_error_init (&error);
	slotData->isRechargeable = libhal_device_get_property_bool (hal_ctx,
		slotData->udi, "battery.is_rechargeable", &error);
	dbus_error_print (&error);
	if (slotData->isRechargeable) {
		dbus_error_init (&error);
		slotData->isCharging = libhal_device_get_property_bool (hal_ctx,
			slotData->udi, "battery.rechargeable.is_charging", &error);
		dbus_error_print (&error);
		dbus_error_init (&error);
		slotData->isDischarging = libhal_device_get_property_bool (hal_ctx,
			slotData->udi, "battery.rechargeable.is_discharging", &error);
		dbus_error_print (&error);
	}
	update_percentage_charge (slotData);
}

/** Adds a battery device, of any type. Also sets up properties on cached object
 *
 *  @param  udi			UDI
 */
static void
add_battery (const gchar *udi)
{
	g_return_if_fail (udi);
	DBusError error;
	gchar *type = NULL;

	GenericObject *slotData = genericobject_add (objectData, udi);
	if (!slotData) {
		g_warning ("Cannot add object to table!");
		return;
	}

	/* PMU/ACPI batteries might be missing */
	dbus_error_init (&error);
	slotData->present = libhal_device_get_property_bool (hal_ctx,
		udi, "battery.present", &error);
	dbus_error_print (&error);

	/* battery is refined using the .type property */
	dbus_error_init (&error);
	type = libhal_device_get_property_string (hal_ctx, udi, "battery.type", &error);
	dbus_error_print (&error);
	if (!type) {
		g_warning ("Battery %s has no type!", udi);
		return;
	}
	slotData->powerDevice = convert_haltype_to_powerdevice (type);
	libhal_free_string (type);

	gchar *device = convert_powerdevice_to_string (slotData->powerDevice);
	g_debug ("%s added", device);

	/* read in values */
	read_battery_data (slotData);
}

/** Coldplugs devices of type battery & ups at startup
 *
 */
static void
coldplug_devices (void)
{
	gint i, num_devices;
	char **device_names;
	DBusError error;

	/* devices of type battery */
	dbus_error_init (&error);
	device_names = libhal_find_device_by_capability (hal_ctx, "battery", 
					&num_devices, &error);
	dbus_error_print (&error);
	if (device_names == NULL)
		g_warning (_("Couldn't obtain list of batteries"));
	for (i = 0; i < num_devices; i++)
		add_battery (device_names[i]);
	libhal_free_string_array (device_names);

	/* devices of type ac_adapter */
	dbus_error_init (&error);
	device_names = libhal_find_device_by_capability (hal_ctx, "ac_adapter",
					&num_devices, &error);
	dbus_error_print (&error);
	if (device_names == NULL)
		g_warning (_("Couldn't obtain list of ac_adapters"));
	for (i = 0; i < num_devices; i++)
		add_ac_adapter (device_names[i]);
	libhal_free_string_array (device_names);
}

/** Removes any type of device
 *
 *  @param  udi			UDI
 */
static void
remove_devices (const char *udi)
{
	g_return_if_fail (udi);
	int a = find_udi_parray_index (objectData, udi);
	if (a == -1) {
		g_debug ("Asked to remove '%s' when not present", udi);
		return;
	}
	g_debug ("Removed '%s'", udi);
	g_ptr_array_remove_index (objectData, a);
}

/** Invoked when a device is removed from the Global Device List. Simply
 *  prints a message on stderr.
 *
 *  @param  udi			UDI
 */
static void
device_removed (LibHalContext *ctx, const char *udi)
{
	g_return_if_fail (udi);
	/*
	 * UPS's/mice/keyboards don't use battery.present
	 * they just disappear from the device tree
	 */
	remove_devices (udi);
	/* our state has changed, update */
	update_has_logic (objectData, FALSE);
	update_state_logic (objectData, FALSE);
	if (setup.isVerbose)
		genericobject_print (objectData);
	gpn_icon_update ();
}

/** Invoked when device in the Global Device List acquires a new capability.
 *  Prints the name of the capability to stderr.
 *
 *  @param  udi			UDI
 *  @param  capability          Name of capability
 */
static void
device_new_capability (LibHalContext *ctx, const char *udi, const char *capability)
{
	g_return_if_fail (udi);
	g_return_if_fail (capability);
	/*
	 * UPS's/mice/keyboards don't use battery.present
	 * they just appear in the device tree
	 */
	if (strcmp (capability, "battery") == 0) {
		add_battery (udi);
		/* our state has changed, update */
		update_has_logic (objectData, FALSE);
		update_state_logic (objectData, FALSE);
		gpn_icon_update ();
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param  udi                 Univerisal Device Id
 *  @param  key                 Key of property
 */
static void
property_modified (LibHalContext *ctx, const char *udi, const char *key,
		   dbus_bool_t is_removed, dbus_bool_t is_added)
{
	g_return_if_fail (udi);
	g_return_if_fail (key);
	DBusError error;
	GenericObject *slotData;

	/* only process modified entries, not added or removed keys */
	if (is_removed||is_added)
		return;

	/* no point continuing any further if we are never going to match ...*/
	if (strncmp (key, "battery", 7) != 0 && strncmp (key, "ac_adapter", 10) != 0)
		return;

	slotData = genericobject_find (objectData, udi);

#if 0
	/* I think this code is now obsolete, now we are checking for bugs in HAL */
	dbus_error_init (&error);
	gchar *type = libhal_device_get_property_string (hal_ctx, udi, "info.category", &error);
	dbus_error_print (&error);
	libhal_free_string (type);
		g_warning ("Cannot find UDI '%s' in the objectData", udi);
		if (powerDevice == POWER_AC_ADAPTER)) {
			add_ac_adapter (udi);
			object_table_find (udi, &slotData, objectData);
		} else if (powerDevice != POWER_BATTERY || strcmp (type, "hiddev") == 0)  {
			add_battery (udi);
			object_table_find (udi, &slotData, objectData);
		} else if (strcmp (type, "button") == 0)  {
			/* we should do nothing here - we do not cache the button state */
			return;
		} else {
			g_warning ("Unrecognised category '%s'!", type);
			libhal_free_string (type);
			return;
		}
		libhal_free_string (type);
	}
#endif

	/* if we BUG here then *HAL* has a problem where key modification is
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
	gboolean updateHas = FALSE;
	gboolean updateState = FALSE;

	g_debug ("key = '%s'", key);
	g_debug ("udi = '%s'", udi);
	if (strcmp (key, "battery.present") == 0) {
		dbus_error_init (&error);
		slotData->present = libhal_device_get_property_bool (ctx, udi, key, &error);
		dbus_error_print (&error);
		/* read in values */
		read_battery_data (slotData);
		updateHas = TRUE;
		updateState = TRUE;
	} else if (strcmp (key, "ac_adapter.present") == 0) {
		dbus_error_init (&error);
		slotData->present = libhal_device_get_property_bool (ctx, udi, key, &error);
		dbus_error_print (&error);
		updateHas = TRUE;
		updateState = TRUE;
	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		dbus_error_init (&error);
		slotData->isCharging = libhal_device_get_property_bool (ctx, udi, key, &error);
		dbus_error_print (&error);
		updateState = TRUE;
	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		dbus_error_init (&error);
		slotData->isDischarging = libhal_device_get_property_bool (ctx, udi, key, &error);
		dbus_error_print (&error);
		updateState = TRUE;
	} else if (strcmp (key, "battery.charge_level.current") == 0) {
		dbus_error_init (&error);
		slotData->rawCharge = libhal_device_get_property_int (ctx, udi, key, &error);
		dbus_error_print (&error);
	} else if (strcmp (key, "battery.remaining_time") == 0) {
		dbus_error_init (&error);
		slotData->minutesRemaining = libhal_device_get_property_int (ctx, udi, key, &error) / 60;
		dbus_error_print (&error);
	} else if (strcmp (key, "battery.charge_level.rate") == 0) {
		/* ignore */
		return;
	} else {
		g_debug ("Cannot recognise key '%s' from UDI '%s'", key, udi);
		return;
	}

	if (updateHas)
		update_has_logic (objectData, FALSE);
	if (updateState)
		update_state_logic (objectData, FALSE);

	/* find old (taking into account multi-device machines) */
	int oldCharge, newCharge;
	if (slotData->isRechargeable) {
		GenericObject slotDataVirt = {.percentageCharge = 100};
		create_virtual_of_type (&slotDataVirt, slotData->powerDevice);
		oldCharge = slotDataVirt.percentageCharge;
	} else
		oldCharge = slotData->percentageCharge;

	/* calculate the new value */
	update_percentage_charge (slotData);

	/* find new (taking into account multi-device machines) */
	if (slotData->isRechargeable) {
		GenericObject slotDataVirt = {.percentageCharge = 100}; /* multibattery */
		create_virtual_of_type (&slotDataVirt, slotData->powerDevice);
		newCharge = slotDataVirt.percentageCharge;
	} else
		newCharge = slotData->percentageCharge;

	gpn_icon_update ();

	/* do we need to notify the user we are getting low ? */
	if (oldCharge != newCharge) {
		g_debug ("percentage change %i -> %i", oldCharge, newCharge);
		if (slotData->isDischarging) {
			GConfClient *client = gconf_client_get_default ();
			gint lowThreshold = gconf_client_get_int (client, 
			GCONF_ROOT "general/lowThreshold", NULL);
			gint criticalThreshold = gconf_client_get_int (client, 
			GCONF_ROOT "general/criticalThreshold", NULL);
			/* critical warning */
			if (newCharge < criticalThreshold) {
				int policy = get_policy_string (GCONF_ROOT "policy/BatteryCritical");
				if (policy == ACTION_WARNING) {
			GString *gs = g_string_new ("");
			char *device = convert_powerdevice_to_string (slotData->powerDevice);
			GString *remaining = get_time_string (slotData);;
			g_string_printf (gs, _("The %s (%i%%) is <b>critically low</b> (%s)"), 
				device, newCharge, remaining->str);
			g_message ("%s", gs->str);
			use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
			g_string_free (gs, TRUE);
			g_string_free (remaining, TRUE);
				} else
			action_policy_do (policy);
			/* low warning */
			} else if (newCharge < lowThreshold) {
				GString *gs = g_string_new ("");
				char *device = convert_powerdevice_to_string (slotData->powerDevice);
				GString *remaining = get_time_string (slotData);;
				g_string_printf (gs, _("The %s (%i%%) is <b>low</b> (%s)"), 
			device, newCharge, remaining->str);
				g_message ("%s", gs->str);
				use_libnotify (gs->str, NOTIFY_URGENCY_CRITICAL);
				g_string_free (gs, TRUE);
				g_string_free (remaining, TRUE);
			}
		}
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param  udi                 Univerisal Device Id
 *  @param  condition_name      Name of condition
 *  @param  message             D-BUS message with parameters
 */
static void
device_condition (LibHalContext *ctx,
		  const char *udi, 
		  const char *condition_name,
		  const char *condition_details)
{
	g_return_if_fail (udi);
	DBusError error;
	gchar *type;

	if (strcmp (condition_name, "ButtonPressed") == 0) {
		dbus_error_init (&error);
		type = libhal_device_get_property_string (hal_ctx, udi, "button.type", &error);
		dbus_error_print (&error);
		g_debug ("ButtonPressed : %s", type);
		if (strcmp (type, "power") == 0) {
			int policy = get_policy_string (GCONF_ROOT "policy/ButtonPower");
			if (policy == ACTION_WARNING)
				use_libnotify (_("Power button has been pressed"), NOTIFY_URGENCY_NORMAL);
			else
				action_policy_do (policy);
		} else if (strcmp (type, "sleep") == 0) {
			int policy = get_policy_string (GCONF_ROOT "policy/ButtonSuspend");
			if (policy == ACTION_WARNING)
				use_libnotify (_("Sleep button has been pressed"), NOTIFY_URGENCY_NORMAL);
			else
				action_policy_do (policy);
		} else if (strcmp (type, "lid") == 0) {
			gboolean value;
			/* we only do a lid event when the lid is OPENED */
			dbus_error_init (&error);
			value = libhal_device_get_property_bool (ctx, udi, "button.state.value", &error);
			dbus_error_print (&error);
			if (value) {
				int policy = get_policy_string (GCONF_ROOT "policy/ButtonLid");
				if (policy == ACTION_WARNING)
			use_libnotify (_("Lid has been opened"), NOTIFY_URGENCY_NORMAL);
				else
			action_policy_do (policy);
			}
		} else
			g_warning ("Button '%s' unrecognised", type);
		libhal_free_string (type);
	}
}

/** Prints program usage.
 *
 */
static void print_usage (void)
{
	g_print ("\nusage : gnome-power-manager [options]\n");
	g_print (
		"\n"
		"    --disable        Do not perform the action, e.g. suspend\n"
		"    --has-quit       Include the quit button on the drop-down\n"
		"    --no-actions     Do not include the actions in the drop-down\n"
		"    --verbose        Show extra debugging\n"
		"    --help           Show this information and exit\n"
		"\n");
}

/** Entry point
 *
 *  @param  argc		Number of arguments given to program
 *  @param  argv		Arguments given to program
 *  @return			Return code
 */
int
main (int argc, char *argv[])
{
	gint a;
	GMainLoop *loop;
	DBusError error;

	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();

	dbus_error_init (&error);

	gconf_init (argc, argv, NULL);
	GConfClient *client = gconf_client_get_default ();
	gconf_client_add_dir (client, GCONF_ROOT_SANS_SLASH, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (client, GCONF_ROOT_SANS_SLASH, 
		callback_gconf_key_changed, NULL, NULL, NULL);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	setup.isVerbose = FALSE;
	setup.doAction = TRUE;
	setup.hasQuit = FALSE;
	setup.hasActions = TRUE;
	for (a=1; a < argc; a++) {
		if (strcmp (argv[a], "--verbose") == 0)
			setup.isVerbose = TRUE;
		else if (strcmp (argv[a], "--disable") == 0)
			setup.doAction = FALSE;
		else if (strcmp (argv[a], "--has-quit") == 0)
			setup.hasQuit = TRUE;
		else if (strcmp (argv[a], "--no-actions") == 0)
			setup.hasActions = FALSE;
		else if (strcmp (argv[a], "--help") == 0) {
			print_usage ();
			return EXIT_SUCCESS;
		}
	}

	if (!setup.isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, g_log_ignore, NULL);

	gnome_program_init ("GNOME Power Manager", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);
	GnomeClient *master = gnome_master_client ();
	GnomeClientFlags flags = gnome_client_get_flags (master);
	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		gnome_client_set_restart_style (master, GNOME_RESTART_IMMEDIATELY);
		gnome_client_flush (master);
	}
	g_signal_connect (GTK_OBJECT (master), "die", G_CALLBACK (gpm_exit), NULL);

#if HAVE_LIBNOTIFY
	/* initialise libnotify */
	if (!notify_init (NICENAME))
		g_error ("Cannot initialise libnotify!");
#endif

	g_print ("%s %s - %s\n", NICENAME, VERSION, NICEDESC);
	g_print (_("Please report bugs to richard@hughsie.com\n"));

	setup.lockdownReboot = gconf_client_get_bool (client, GCONF_ROOT "lockdown/reboot", NULL);
	setup.lockdownShutdown = gconf_client_get_bool (client, GCONF_ROOT "lockdown/shutdown", NULL);
	setup.lockdownHibernate = gconf_client_get_bool (client, GCONF_ROOT "lockdown/hibernate", NULL);
	setup.lockdownSuspend = gconf_client_get_bool (client, GCONF_ROOT "lockdown/suspend", NULL);

	loop = g_main_loop_new (NULL, FALSE);

	/* Initialise DBUS SESSION conections */
	dbus_error_init (&error);
	connsession = dbus_bus_get (DBUS_BUS_SESSION, &error);
	if (!connsession) {
		g_error ("dbus_bus_get DBUS_BUS_SESSION failed: %s", error.message);
		dbus_error_free (&error);
		return 1;
	}
	/* listening to messages from all objects as no path is specified */
	dbus_error_init (&error);
	dbus_bus_add_match (connsession,
			    "type='signal'"
			    ",interface='" GPM_DBUS_INTERFACE_SIGNAL "'", 
			    &error);

	dbus_bus_add_match (connsession,
			    "type='signal'"
			    ",interface='" DBUS_INTERFACE_DBUS "'"
			    ",sender='" DBUS_SERVICE_DBUS "'"
			    ",member='NameOwnerChanged'",
			    NULL);

	if (dbus_error_is_set (&error))
		g_error ("dbus_bus_add_match Failed. Error says: \n'%s'", error.message);
	if (!dbus_connection_add_filter (connsession, dbus_signal_filter, loop, NULL))
		g_warning ("Cannot add signal filter");
	dbus_error_init (&error);
	if (dbus_bus_name_has_owner (connsession, GPM_DBUS_SERVICE, &error)) {
		g_warning ("GNOME Power Manager is already running in this session.");
		return 0;
	}
	dbus_error_init (&error);
	dbus_bus_request_name (connsession, GPM_DBUS_SERVICE, 0, &error);
	if (dbus_error_is_set (&error))
		g_error ("dbus_bus_acquire_service() failed: '%s'", error.message);
	DBusObjectPathVTable vtable = {
		NULL,
		dbus_message_handler,
	};
	if (!dbus_connection_register_object_path (connsession, GPM_DBUS_PATH, &vtable, dbus_method_handler))
		g_warning ("Cannot register method handler");

	/* Initilise HAL DBUS connection */
	DBusConnection *connsystem = NULL;
	dbus_error_init (&error);
	connsystem = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!connsystem)
		g_error ("dbus_bus_get DBUS_BUS_SYSTEM failed: %s: %s", error.name, error.message);
	if (!(hal_ctx = libhal_ctx_new ()))
		g_error ("HAL error: libhal_ctx_new");
	if (!libhal_ctx_set_dbus_connection (hal_ctx, connsystem))
		g_error ("HAL error: libhal_ctx_set_dbus_connection: %s: %s", error.name, error.message);
	if (!libhal_ctx_init (hal_ctx, &error))
		g_error ("HAL error: libhal_ctx_init: %s: %s", error.name, error.message);
	libhal_ctx_set_device_property_modified (hal_ctx, property_modified);
	libhal_ctx_set_device_removed (hal_ctx, device_removed);
	libhal_ctx_set_device_new_capability (hal_ctx, device_new_capability);
	libhal_ctx_set_device_condition (hal_ctx, device_condition);

	/* set up SESSION and SYSTEM connections with glib loop */
	dbus_connection_set_exit_on_disconnect (connsession, FALSE);
	dbus_connection_setup_with_g_main (connsession, NULL);
	dbus_connection_set_exit_on_disconnect (connsystem, FALSE);
	dbus_connection_setup_with_g_main (connsystem, NULL);
	libhal_device_property_watch_all (hal_ctx, &error);

	objectData = g_ptr_array_new ();
	registered = g_ptr_array_new ();

	coldplug_devices ();
	if (setup.isVerbose)
		genericobject_print (objectData);

	update_has_logic (objectData, TRUE);
	update_state_logic (objectData, TRUE);

	gpn_icon_initialise ();
	gpn_icon_update ();

	g_main_loop_run (loop);

	/* free objectData */
	for (a=0;a<objectData->len;a++)
		g_free (g_ptr_array_index (objectData, a));
	g_ptr_array_free (objectData, TRUE);

	/* free registered */
	for (a=0;a<registered->len;a++)
		g_free (g_ptr_array_index (registered, a));
	g_ptr_array_free (registered, TRUE);

	/* free all HAL stuff */
	dbus_error_init (&error);
	libhal_ctx_shutdown (hal_ctx, &error);
	libhal_ctx_free (hal_ctx);

	/* free all DBUS SESSION and SYSTEM connections */
	dbus_connection_disconnect (connsession);
	dbus_connection_unref (connsession);
	dbus_connection_disconnect (connsystem);
	dbus_connection_unref (connsystem);

	gpm_exit ();
	return 0;
}
