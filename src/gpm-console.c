/** @file	gpm-console.c
 *  @brief	Power Manager console daemon
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-16
 *
 * Console BODGE!
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
 * @addtogroup	console		gnome-power-console
 * @brief			The shitty console program
 *
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <popt.h>

#include "gpm-common.h"
#include "gpm-core.h"
#include "gpm-main.h"
#include "gpm-hal.h"
#include "gpm-sysdev.h"
#include "gpm-dbus-common.h"
#include "gpm-dbus-server.h"
#include "gpm-dbus-signal-handler.h"

#include "glibhal-main.h"
#include "glibhal-callback.h"


/* no need for IPC with globals */
gboolean isVerbose;
gboolean onAcPower;

/** Generic exit
 *
 */

static void
gpm_console_debug_log_ignore (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
}
static void
gpm_exit (void)
{
	g_debug ("Quitting!");
	glibhal_callback_shutdown ();

	gpm_dbus_remove_noc ();
#if 0
	gpm_dbus_remove_nlost ();
#endif
	/* cleanup all system devices */
	sysDevFreeAll ();
	exit (0);
}

/** When we have a device removed
 *
 *  @param	udi		The HAL UDI
 */
static void
hal_device_removed (const gchar *udi)
{
	gpm_device_removed (udi);
}

/** When we have a new device hot-plugged
 *
 *  @param	udi		UDI
 *  @param	capability	Name of capability
 */
static void
hal_device_new_capability (const gchar *udi, const gchar *capability)
{
	gpm_device_new_capability (udi, capability);
}

/** Notifies user of a low battery
 *
 *  @param	sds		Data structure
 *  @param	newCharge	New charge value (%)
 *  @return			If a warning was sent
 */
static gboolean
notify_user_low_batt (sysDevStruct *sds, gint newCharge)
{
	gint lowThreshold;
	gint criticalThreshold;

	if (!sds->isDischarging) {
		g_debug ("battery is not discharging!");
		return FALSE;
	}

	lowThreshold = 10;
	criticalThreshold = 5;
	g_debug ("lowThreshold = %i, criticalThreshold = %i",
		lowThreshold, criticalThreshold);

	/* critical warning */
	if (newCharge < criticalThreshold) {
		g_debug ("battery is critical!");
		return TRUE;
	}

	/* low warning */
	if (newCharge < lowThreshold) {
		g_debug ("battery is low!");
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
	sysDev *sd = NULL;
	sysDevStruct *sds = NULL;
	gchar *type;

	gint oldCharge;
	gint newCharge;

	g_debug ("hal_device_property_modified: udi=%s, key=%s, added=%i, removed=%i",
		udi, key, is_added, is_removed);

	/* only process modified entries, not added or removed keys */
	if (is_removed||is_added)
		return;

	if (strcmp (key, "ac_adapter.present") == 0) {
		hal_device_get_bool (udi, key, &onAcPower);
		/* update all states */
		sysDevUpdateAll ();
		return;
	}

	/* no point continuing any further if we are never going to match ...*/
	if (strncmp (key, "battery", 7) != 0)
		return;

	sds = sysDevFindAll (udi);
	/*
	 * if we BUG here then *HAL* has a problem where key modification is
	 * done before capability is present
	 */
	if (!sds) {
		g_warning ("sds is NULL! udi=%s\n"
			   "This is probably a bug in HAL where we are getting "
			   "is_removed=false, is_added=false before the capability "
			   "had been added. In addon-hid-ups this is likely to happen."
			   , udi);
		return;
	}
	
	/* get battery type so we know what to process */
	hal_device_get_string (udi, "battery.type", &type);
	if (!type) {
		g_warning ("Battery %s has no type!", udi);
		return;
	}
	DeviceType dev = hal_to_device_type (type);
	g_free (type);

	/* find old percentageCharge */
	sd = sysDevGet (dev);
	oldCharge = sd->percentageCharge;

	/* update values in the struct */
	if (strcmp (key, "battery.present") == 0) {
		hal_device_get_bool (udi, key, &sds->isPresent);
		/* read in values */
		gpm_read_battery_data (sds);
	} else if (strcmp (key, "battery.rechargeable.is_charging") == 0) {
		hal_device_get_bool (udi, key, &sds->isCharging);
		/*
		 * invalidate the remaining time, as we need to wait for
		 * the next HAL update. This is a HAL bug I think.
		 */
		sds->minutesRemaining = 0;
	} else if (strcmp (key, "battery.rechargeable.is_discharging") == 0) {
		hal_device_get_bool (udi, key, &sds->isDischarging);
		/* invalidate the remaining time */
		sds->minutesRemaining = 0;
	} else if (strcmp (key, "battery.charge_level.percentage") == 0) {
		hal_device_get_int (udi, key, &sds->percentageCharge);
		/* give notification @100% */
	} else if (strcmp (key, "battery.remaining_time") == 0) {
		gint tempval;
		hal_device_get_int (udi, key, &tempval);
		if (tempval > 0)
			sds->minutesRemaining = tempval / 60;
	} else {
		/* ignore */
		return;
	}

	/* update */
	sysDevUpdate (dev);
	sysDevDebugPrint (dev);

	/* find new percentageCharge  */
	newCharge = sd->percentageCharge;

	g_debug ("newCharge = %i, oldCharge = %i", newCharge, oldCharge);

	/* do we need to notify the user we are getting low ? */
	if (oldCharge != newCharge) {
		g_debug ("percentage change %i -> %i", oldCharge, newCharge);
		notify_user_low_batt (sds, newCharge);
	}
}

/** Invoked when a button is pressed.
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

	g_debug ("hal_device_condition: udi=%s, condition_name=%s, condition_details=%s",
		udi, condition_name, condition_details);

	if (strcmp (condition_name, "ButtonPressed") == 0) {
		hal_device_get_string (udi, "button.type", &type);
		g_debug ("ButtonPressed : %s", type);
		if (strcmp (type, "power") == 0) {
			g_warning ("power!");
		} else if (strcmp (type, "sleep") == 0) {
			g_warning ("sleep!");
		} else if (strcmp (type, "lid") == 0) {
			g_warning ("lid!");
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
		 "    --no-daemon      Do not daemonize.\n"
		 "    --verbose	       Show extra debugging\n"
		 "    --help	       Show this information and exit\n"
		 "\n");
}

/** Callback for the DBUS NameOwnerChanged function.
 *
 *  @param	name		The DBUS name, e.g. org.freedesktop.Hal
 *  @param	connected	Time in minutes that computer has been idle
 */
static void
signalhandler_noc (const char *name, const gboolean connected)
{
	g_debug ("signalhandler_noc: (%i) %s", connected, name);
	/* ignore that don't all apply */
	if (strcmp (name, "org.freedesktop.Hal") != 0)
		return;

	/** @todo: handle reconnection to the HAL bus */
	g_warning ("hal re-connected\n");
}


#if 0
/** Callback for the DBUS NameLost function.
 *
 *  @param	name		The DBUS name, e.g. org.freedesktop.Hal
 *  @param	connected	Always true.
 */
static void
signalhandler_nlost (const char *name, const gboolean connected)
{
	if (strcmp (name, "org.gnome.GnomePowerManager") != 0)
		return;
	gpm_exit ();
}
#endif

/** Main entry point
 *
 *  @param	argc		Number of arguments given to program
 *  @param	argv		Arguments given to program
 *  @return			Return code
 */
int
main (int argc, char *argv[])
{
	gint i;
	GMainLoop *loop = NULL;
	DBusGConnection *system_connection = NULL;
	gboolean no_daemon;

	struct poptOption options[] = {
		{ "no-daemon", '\0', POPT_ARG_NONE, NULL, 0,
		  N_("Do not daemonize"), NULL },
		{ "verbose", '\0', POPT_ARG_NONE, NULL, 0,
		  N_("Show extra debugging information"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	i = 0;
	options[i++].arg = &no_daemon;
	options[i++].arg = &isVerbose;

	no_daemon = FALSE;
	isVerbose = FALSE;

	/* Initialise gnome and parse command line */

	isVerbose = FALSE;
	int a;
	for (a=1; a < argc; a++) {
		 if (strcmp (argv[a], "--verbose") == 0)
			  isVerbose = TRUE;
		 else if (strcmp (argv[a], "--no-daemon") == 0)
			  no_daemon = TRUE;
		 else if (strcmp (argv[a], "--help") == 0) {
			  print_usage ();
			  return EXIT_SUCCESS;
		 }
	}
	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* set log level */
	if (!isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			gpm_console_debug_log_ignore, NULL);

	/* check dbus connections, exit if not valid */
	if (!gpm_dbus_get_system_connection (&system_connection))
		exit (1);

	/* initialise all system devices */
	sysDevInitAll ();

	if (!no_daemon && daemon (0, 0))
		g_error ("Could not daemonize: %s", g_strerror (errno));

	loop = g_main_loop_new (NULL, FALSE);
	/* check HAL is running */
	if (!is_hald_running ()) {
		g_critical ("GNOME Power Manager cannot connect to HAL!");
		return 0;
	}

	/* check we have PM capability */
	if (!gpm_hal_pm_check ()) {
		g_critical ("HAL does not have PowerManagement capability");
		return 0;
	}

	/* register dbus service */
	if (!gpm_object_register (system_connection)) {
		g_warning ("GNOME Power Manager is already running in this session.");
		return 0;
	}

	/* initialise NameOwnerChanged and NameLost */
	gpm_dbus_init_noc (system_connection, signalhandler_noc);
#if 0
	gpm_dbus_init_nlost (system_connection, signalhandler_nlost);
#endif
	glibhal_callback_init ();
	/* assign the callback functions */
	glibhal_method_device_removed (hal_device_removed);
	glibhal_method_device_new_capability (hal_device_new_capability);
	glibhal_method_device_property_modified (hal_device_property_modified);
	glibhal_method_device_condition (hal_device_condition);

	/* sets up these devices and adds watches */
	gpm_coldplug_batteries ();
	gpm_coldplug_acadapter ();
	gpm_coldplug_buttons ();

	sysDevUpdateAll ();
	sysDevDebugPrintAll ();

	g_main_loop_run (loop);

	gpm_exit ();
	return 0;
}
/** @} */
