/** @file	gpm-main.c
 *  @brief	GNOME Power Manager session daemon
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *  @note	Taken in part from:
 *  @note	- lshal   (C) 2003 David Zeuthen, <david@fubar.dk>
 *  @note	- notibat (C) 2004 Benjamin Kahn, <xkahn@zoned.net>
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
#include <libgnome/libgnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <popt.h>

#include "gpm-common.h"
#include "gpm-core.h"
#include "gpm-main.h"
#include "gpm-notification.h"
#include "gpm-idle.h"
#include "gpm-screensaver.h"
#include "gpm-networkmanager.h"
#include "gpm-libnotify.h"
#include "gpm-dbus-server.h"
#include "gpm-dbus-common.h"

#include "glibhal-main.h"
#include "glibhal-callback.h"
#include "glibhal-extras.h"
#include "gnome-power-glue.h"
#include "gpm-stock-icons.h"
#include "gpm-sysdev.h"


/* no need for IPC with globals */
gboolean isVerbose;
gboolean onAcPower;

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

	if (strcmp (entry->key, GCONF_ROOT "general/display_icon_policy") == 0) {
		gpn_icon_update ();
	} else if (strcmp (entry->key, GCONF_ROOT "policy/battery/sleep_computer") == 0) {
		/* set new suspend timeouts */
		value = gconf_client_get_int (client, entry->key, NULL);
		if (!onAcPower)
			gpm_idle_set_timeout (value);
	} else if (strcmp (entry->key, GCONF_ROOT "policy/ac/sleep_computer") == 0) {
		/* set new suspend timeouts */
		value = gconf_client_get_int (client, entry->key, NULL);
		if (onAcPower)
			gpm_idle_set_timeout (value);
	} else if (strcmp (entry->key, GCONF_ROOT "policy/battery/sleep_display") == 0) {
		/* set new suspend timeouts */
		if (!onAcPower) {
			value = gconf_client_get_int (client, entry->key, NULL);
			gpm_screensaver_set_dpms_timeout (value);
		}
	} else if (strcmp (entry->key, GCONF_ROOT "policy/ac/sleep_display") == 0) {
		/* set new suspend timeouts */
		if (onAcPower) {
			value = gconf_client_get_int (client, entry->key, NULL);
			gpm_screensaver_set_dpms_timeout (value);
		}
	}

}

/** Do all the action when we go from batt to ac, or ac to batt (or coldplug)
 *
 *  @param	isOnAc		If we are on AC power
 *
 *  @note
 *	- Sets the brightness level
 *	- Sets HAL to be in LaptopMode if !AC
 *	- Sets DPMS timeout to be our policy value
 *	- Sets GNOME Screensaver to [not] run fancy screensavers
 *	- Sets our inactivity sleep timeout to policy value
 */
void
perform_power_policy (gboolean isOnAc)
{
	gchar *policyname = NULL;
	gchar *gconfpath = NULL;
	gint value;
	GConfClient *client = gconf_client_get_default ();

	if (isOnAc)
		policyname = "ac";
	else
		policyname = "battery";

	/* set brightness */
	gconfpath = g_strdup_printf ("%s/policy/%s/brightness",
				     GCONF_ROOT_SANS_SLASH, policyname);
	value = gconf_client_get_int (client, gconfpath, NULL);
	g_free (gconfpath);
	hal_set_brightness_dim (value);

	/* set lowpower mode */
	hal_setlowpowermode (!isOnAc);

	/* set gnome screensaver dpms_suspend to our value */
	gconfpath = g_strdup_printf ("%s/policy/%s/sleep_display",
				     GCONF_ROOT_SANS_SLASH, policyname);
	value = gconf_client_get_int (client, gconfpath, NULL);
	g_free (gconfpath);
	gpm_screensaver_set_dpms_timeout (value);

	/*
	 * make sure gnome-screensaver disables screensaving,
	 * and enables monitor shut-off instead
	 */
	gpm_screensaver_set_throttle (!isOnAc);

	/* set the new sleep (inactivity) value */
	gconfpath = g_strdup_printf ("%spolicy/%s/sleep_computer",
				     GCONF_ROOT, policyname);
	value = gconf_client_get_int (client, gconfpath, NULL);
	g_free (gconfpath);
	gpm_idle_set_timeout (value);
}


/** Do a hibernate or suspend with all the associated callbacks and methods.
 *
 *  @param	toDisk		If we hibernate, i.e. sleep to disk.
 *
 *  @note
 *	- Locks the screen (if required)
 *	- Sets NetworkManager to sleep
 *	- Does the sleep...
 *	- Sets NetworkManager to wake
 *	- Pokes g-s so we get the unlock screen (if required)
 */
void
perform_sleep_methods (gboolean toDisk)
{
	GConfClient *client = gconf_client_get_default ();
	gboolean shouldLock = gconf_client_get_bool (client, 
				GCONF_ROOT "general/require_password", NULL);
	/* only lock if we should */
	if (shouldLock)
		gpm_screensaver_lock ();

	/* Send NetworkManager to sleep */
	gpm_networkmanager_sleep ();

	/* do the sleep type */
	if (toDisk)
		hal_hibernate ();
	else
		hal_suspend (0);
	/* Bring NetworkManager back to life */
	gpm_networkmanager_wake ();

	/* Poke GNOME ScreenSaver so the dialogue is displayed */
	if (shouldLock)
		gpm_screensaver_poke ();
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
	if (policy_number == ACTION_NOTHING) {
		g_debug ("*ACTION* Doing nothing");
	} else if (policy_number == ACTION_WARNING) {
		g_warning ("*ACTION* Send warning should be done locally!");
	} else if (policy_number == ACTION_REBOOT) {
		g_debug ("*ACTION* Reboot");
		run_gconf_script (GCONF_ROOT "general/cmd_reboot");
	} else if (policy_number == ACTION_SUSPEND) {
		g_debug ("*ACTION* Suspend");
		if (!hal_pm_can_suspend ()) {
			g_warning ("Cannot suspend as disabled in HAL");
			return;
		}
		perform_sleep_methods (FALSE);
	} else if (policy_number == ACTION_HIBERNATE) {
		g_debug ("*ACTION* Hibernate");
		if (!hal_pm_can_hibernate ()) {
			g_warning ("Cannot hibernate as disabled in HAL");
			return;
		}
		perform_sleep_methods (TRUE);
	} else if (policy_number == ACTION_SHUTDOWN) {
		g_debug ("*ACTION* Shutdown");
		run_gconf_script (GCONF_ROOT "general/cmd_shutdown");
	} else if (policy_number == ACTION_NOW_BATTERYPOWERED) {
		g_debug ("*DBUS* Now battery powered");
		/* do all our powersaving stuff */
		perform_power_policy (FALSE);
		/* emit signal */
		gpm_emit_mains_changed (FALSE);
	} else if (policy_number == ACTION_NOW_MAINSPOWERED) {
		g_debug ("*DBUS* Now mains powered");
		/* do all our powersaving stuff */
		perform_power_policy (TRUE);
		/* emit siganal */
		gpm_emit_mains_changed (TRUE);
	} else
		g_warning ("action_policy_do called with unknown action %i",
			   policy_number);
}

/** Generic exit
 *
 */
static void
gpm_exit (void)
{
	g_debug ("Quitting!");
	glibhal_callback_shutdown ();
	gpm_stock_icons_shutdown ();

	/* cleanup all system devices */
	sysDevFreeAll ();

	gpn_icon_destroy ();
	exit (0);
}

/** When we have a device removed
 *
 *  @param	udi		The HAL UDI
 */
static void
hal_device_removed (const gchar *udi)
{
	if (gpm_device_removed (udi))
		gpn_icon_update ();
}

/** When we have a new device hot-plugged
 *
 *  @param	udi		UDI
 *  @param	capability	Name of capability
 */
static void
hal_device_new_capability (const gchar *udi, const gchar *capability)
{
	if (gpm_device_new_capability (udi, capability))
		gpn_icon_update ();
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
	GConfClient *client = NULL;
	gint lowThreshold;
	gint criticalThreshold;
	gchar *message = NULL;
	gchar *remaining = NULL;

	if (!sds->isDischarging) {
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
			remaining = get_time_string (sds->minutesRemaining, sds->isCharging);
			g_assert (remaining);
			message = g_strdup_printf (
				_("You have approximately <b>%s</b> of remaining battery life (%i%%). "
				  "Plug in your AC Adapter to avoid losing data."),
				remaining, newCharge);
			libnotify_event (_("Battery Critically Low"),
					 message,
					 LIBNOTIFY_URGENCY_CRITICAL,
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
		remaining = get_time_string (sds->minutesRemaining, sds->isCharging);
		g_assert (remaining);
		message = g_strdup_printf (
			_("You have approximately <b>%s</b> of remaining battery life (%i%%). "
			  "Plug in your AC Adapter to avoid losing data."),
			remaining, newCharge);
		libnotify_event (_("Battery Low"),
				 message,
				 LIBNOTIFY_URGENCY_CRITICAL,
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
		if (!onAcPower) {
			libnotify_event (_("AC Power Unplugged"),
					_("The AC Power has been unplugged. "
					"The system is now using battery power."),
					LIBNOTIFY_URGENCY_NORMAL,
					get_notification_icon ());
			action_policy_do (ACTION_NOW_BATTERYPOWERED);
		} else {
			/*
			 * for where we add back the ac_adapter before
			 * the "AC Power unplugged" message times out.
			 */
			libnotify_clear ();
			action_policy_do (ACTION_NOW_MAINSPOWERED);
		}
		/* update all states */
		sysDevUpdateAll ();
		/* update icon */
		gpn_icon_update ();
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
			   "had been added. In addon-hid-ups this is likely to happen.",
			   udi);
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
		hal_device_get_bool (udi, key, &sds->present);
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
		if (sd->type == BATT_PRIMARY && sds->percentageCharge == 100) {
			libnotify_event (_("Battery Charged"), _("Your battery is now fully charged"),
					 LIBNOTIFY_URGENCY_LOW,
					 get_notification_icon ());
		}
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
	if (isVerbose)
		sysDevPrint (dev);

	/* find new percentageCharge  */
	newCharge = sd->percentageCharge;

	g_debug ("newCharge = %i, oldCharge = %i", newCharge, oldCharge);

	/* update icon */
	gpn_icon_update ();

	/* do we need to notify the user we are getting low ? */
	if (oldCharge != newCharge) {
		g_debug ("percentage change %i -> %i", oldCharge, newCharge);
		notify_user_low_batt (sds, newCharge);
	}
}

/** Invoked when a property of a device in the Global Device List is
 *  changed, and we have we have subscribed to changes for that device.
 *
 *  @param	udi			Univerisal Device Id
 *  @param	name		Name of condition
 *  @param	details	D-BUS message with parameters
 */
static void
hal_device_condition (const gchar *udi, const gchar *name, const gchar *details)
{
	gchar *type = NULL;
	gint policy;
	gboolean value;

	/* assertion checks */
	g_assert (udi);
	g_assert (name);
	g_assert (details);

	g_debug ("hal_device_condition: udi=%s, name=%s, details=%s",
		udi, name, details);

	if (strcmp (name, "ButtonPressed") == 0) {
		hal_device_get_string (udi, "button.type", &type);
		if (!type) {
			g_warning ("You must have a button type for %s!", udi);
			return;
		}
		g_debug ("ButtonPressed : %s", type);
		if (strcmp (type, "power") == 0) {
			policy = get_policy_string (GCONF_ROOT "policy/button_power");
			action_policy_do (policy);
		} else if (strcmp (type, "sleep") == 0) {
			policy = get_policy_string (GCONF_ROOT "policy/button_suspend");
			action_policy_do (policy);
		} else if (strcmp (type, "lid") == 0) {
			hal_device_get_bool (udi, "button.state.value", &value);
			/*
			 * We enable/disable DPMS because some laptops do
			 * not turn off the LCD backlight when the lid
			 * is closed. See
			 * http://bugzilla.gnome.org/show_bug.cgi?id=321313
			 */
			if (value) {
				/* we only do a policy event when the lid is CLOSED */
				gint policy = get_policy_string (GCONF_ROOT "policy/button_lid");
				action_policy_do (policy);
				gpm_screensaver_set_dpms (FALSE);
			} else
				gpm_screensaver_set_dpms (TRUE);
		} else if (strcmp (type, "virtual") == 0) {
			if (!details) {
				g_warning ("Virtual buttons must have details for %s!", udi);
				return;
			}
			if (strcmp (details, "BrightnessUp") == 0)
				hal_set_brightness_up ();
			else if (strcmp (details, "BrightnessDown") == 0)
				hal_set_brightness_down ();
		} else {
			g_warning ("Button '%s' unrecognised", type);
		}
		g_free (type);
	}
}

/** Callback for the idle function.
 *
 *  @param	timeout		Time in minutes that computer has been idle
 */
void
idle_callback (gint timeout)
{
	gint policy;

	policy = get_policy_string (GCONF_ROOT "policy/idle"); /* @todo! */

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
	gint i;
	gint value;
	GMainLoop *loop = NULL;
	GConfClient *client = NULL;
	GnomeClient *master = NULL;
	GnomeClientFlags flags;
	DBusGConnection *system_connection = NULL;
	DBusGConnection *session_connection = NULL;
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
	gnome_program_init (argv[0], VERSION, 
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("GNOME Power Manager"),
			    NULL);
	master = gnome_master_client ();
	flags = gnome_client_get_flags (master);

	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		/* We'll disable this as users are getting constant crashes */
		/* gnome_client_set_restart_style (master, GNOME_RESTART_IMMEDIATELY);*/
		gnome_client_flush (master);
	}

	g_signal_connect (GTK_OBJECT (master), "die", G_CALLBACK (gpm_exit), NULL);

	if (!g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();
	dbus_g_object_type_install_info (gpm_object_get_type (),
		&dbus_glib_gpm_object_object_info);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, GCONF_ROOT_SANS_SLASH,
		GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (client, GCONF_ROOT_SANS_SLASH,
		callback_gconf_key_changed, NULL, NULL, NULL);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* set log level */
	if (!isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			g_log_ignore, NULL);

	/* check dbus connections, exit if not valid */
	if (!dbus_get_system_connection (&system_connection))
		exit (1);
	if (!dbus_get_session_connection (&session_connection))
		exit (1);


	/* Initialise libnotify, if compiled in. */
	if (!libnotify_init (NICENAME))
		g_error ("Cannot initialise libnotify!");

	/* initialise stock icons */
	if (!gpm_stock_icons_init())
		g_error ("Cannot continue without stock icons");

	/* initialise all system devices */
	sysDevInitAll ();

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
		return 0;
	}

	/* check we have PM capability */
	if (!hal_pm_check ()) {
		g_critical ("HAL does not have PowerManagement capability");
		return 0;
	}

	glibhal_callback_init ();
	/* assign the callback functions */
	glibhal_method_device_removed (hal_device_removed);
	glibhal_method_device_new_capability (hal_device_new_capability);
	glibhal_method_device_property_modified (hal_device_property_modified);
	glibhal_method_device_condition (hal_device_condition);
	/* sets up NameOwnerChanged notification */
	glibhal_method_noc (signalhandler_noc);

	/* sets up these devices and adds watches */
	gpm_coldplug_batteries ();
	gpm_coldplug_acadapter ();
	gpm_coldplug_buttons ();

	sysDevUpdateAll ();
	if (isVerbose)
		sysDevPrintAll ();

	g_debug ("init'ing icon");
	gpn_icon_update ();

	/* do all the actions as we have to set initial state */
	perform_power_policy (onAcPower);
	
	/* set callback for the timout action */
	gpm_idle_set_callback (idle_callback);

	/* set up idle calculation function */
	g_timeout_add (POLL_FREQUENCY * 1000, gpm_idle_update, NULL);

	g_main_loop_run (loop);

	gpm_exit ();
	return 0;
}
/** @} */
