/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libgnomeui/gnome-client.h> /* for gnome_client_request_save */

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-screensaver.h"
#include "gpm-networkmanager.h"

/* FIXME: we should abstract the HAL stuff */
#include "gpm-hal.h"

#include "gpm-debug.h"
#include "gpm-dpms.h"
#include "gpm-idle.h"
#include "gpm-info.h"
#include "gpm-graph-widget.h"
#include "gpm-power.h"
#include "gpm-feedback-widget.h"
#include "gpm-hal-monitor.h"
#include "gpm-dbus-system-monitor.h"
#include "gpm-dbus-session-monitor.h"
#include "gpm-brightness.h"
#include "gpm-tray-icon.h"
#include "gpm-inhibit.h"
#include "gpm-stock-icons.h"
#include "gpm-manager.h"

static void     gpm_manager_class_init	(GpmManagerClass *klass);
static void     gpm_manager_init	(GpmManager      *manager);
static void     gpm_manager_finalize	(GObject	 *object);

#define GPM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_MANAGER, GpmManagerPrivate))

typedef enum {
	GPM_WARNING_NONE = 0,
	GPM_WARNING_DISCHARGING = 1,
	GPM_WARNING_LOW = 2,
	GPM_WARNING_VERY_LOW = 3,
	GPM_WARNING_CRITICAL = 4,
	GPM_WARNING_ACTION = 5
} GpmWarning;

#define GPM_BUTTON_POWER		"power"
#define GPM_BUTTON_SLEEP		"sleep"
#define GPM_BUTTON_SUSPEND		"suspend"
#define GPM_BUTTON_HIBERNATE		"hibernate"
#define GPM_BUTTON_LID			"lid"
#define GPM_BUTTON_BRIGHT_UP		"brightness-up"
#define GPM_BUTTON_BRIGHT_DOWN		"brightness-down"
#define GPM_BUTTON_BRIGHT_UP_DEP	"brightnessup"	 /* Remove when we depend on HAL 0.5.8 */
#define GPM_BUTTON_BRIGHT_DOWN_DEP	"brightnessdown" /* as these are the old names */
#define GPM_BUTTON_LOCK			"lock"
#define GPM_BUTTON_BATTERY		"battery"

#define GPM_NOTIFY_TIMEOUT_LONG		20	/* seconds */
#define GPM_NOTIFY_TIMEOUT_SHORT	5	/* seconds */

struct GpmManagerPrivate
{
	GConfClient	*gconf_client;

	GpmDpms		*dpms;
	GpmIdle		*idle;
	GpmInfo		*info;
	GpmFeedback	*feedback;
	GpmPower	*power;
	GpmBrightness   *brightness;
	GpmScreensaver  *screensaver;
	GpmInhibit	*inhibit;

	guint32          ac_throttle_id;
	guint32          dpms_throttle_id;
	guint32          lid_throttle_id;

	GpmTrayIcon	*tray_icon;

	GpmWarning	 last_primary_warning;
	GpmWarning	 last_ups_warning;
	GpmWarning	 last_mouse_warning;
	GpmWarning	 last_keyboard_warning;
	GpmWarning	 last_pda_warning;

	gint		 last_primary_percentage_change;

	gboolean	 use_time_to_notify;
	gboolean	 lid_is_closed;
	gboolean	 done_notify_fully_charged;

	time_t		 last_resume_event;
	int		 suppress_policy_timeout;

	int		 low_percentage;
	int		 very_low_percentage;
	int		 critical_percentage;
	int		 action_percentage;

	int		 low_time;
	int		 very_low_time;
	int		 critical_time;
	int		 action_time;

	int		 lcd_dim_brightness;
};

enum {
	ON_AC_CHANGED,
	DPMS_MODE_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

static GConfEnumStringPair icon_policy_enum_map [] = {
	{ GPM_ICON_POLICY_ALWAYS,	"always"   },
	{ GPM_ICON_POLICY_PRESENT,	"present"  },
	{ GPM_ICON_POLICY_CHARGE,	"charge"   },
	{ GPM_ICON_POLICY_CRITICAL,     "critical" },
	{ GPM_ICON_POLICY_NEVER,	"never"    },
	{ 0, NULL }
};

G_DEFINE_TYPE (GpmManager, gpm_manager, G_TYPE_OBJECT)

/* prototypes */
static gboolean gpm_manager_suspend (GpmManager *manager, GError **error);
static gboolean gpm_manager_hibernate (GpmManager *manager, GError **error);
static gboolean gpm_manager_shutdown (GpmManager *manager, GError **error);

/**
 * gpm_manager_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_manager_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("gpm_manager_error");
	}
	return quark;
}

/**
 * gpm_manager_is_policy_timout_valid:
 * @manager: This manager class instance
 * @action: The action we want to do, e.g. "suspend"
 *
 * Checks if the difference in time between this request for an action, and
 * the last action completing is larger than the timeout set in gconf.
 *
 * Return value: TRUE if we can perform the action.
 **/
static gboolean
gpm_manager_is_policy_timout_valid (GpmManager *manager,
				    const char *action)
{
	if ((time (NULL) - manager->priv->last_resume_event) <=
	    manager->priv->suppress_policy_timeout) {
		gpm_debug ("Skipping suppressed %s", action);
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_manager_is_inhibit_valid:
 * @manager: This manager class instance
 * @action: The action we want to do, e.g. "suspend"
 *
 * Checks to see if the specific action has been inhibited by a program.
 * If so, displays a warning libnotify dialogue for the user explaining
 * the situation.
 *
 * Return value: TRUE if we can perform the action.
 **/
static gboolean
gpm_manager_is_inhibit_valid (GpmManager *manager,
			      const char *action)
{
	gboolean action_ok;
	char *title;

	action_ok = gpm_inhibit_check (manager->priv->inhibit);
	if (! action_ok) {
		title = g_strdup_printf ("Request to %s", action);
		GString *message = g_string_new ("");
		gpm_inhibit_get_message (manager->priv->inhibit, message, action);
		gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
				      title,
				      message->str,
				      GPM_NOTIFY_TIMEOUT_LONG,
				      GTK_STOCK_DIALOG_WARNING,
				      GPM_NOTIFY_URGENCY_NORMAL);
		g_string_free (message, TRUE);
		g_free (title);
	}
	return action_ok;
}

/**
 * gpm_manager_can_suspend:
 * @manager: This manager class instance
 * @can: If we can suspend
 *
 * Checks the HAL key power_management.can_suspend_to_ram and also
 * checks gconf to see if we are allowed to suspend this computer.
 **/
gboolean
gpm_manager_can_suspend (GpmManager *manager,
			 gboolean   *can,
			 GError    **error)
{
	gboolean gconf_policy;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;
	gconf_policy = gconf_client_get_bool (manager->priv->gconf_client,
					      GPM_PREF_CAN_SUSPEND, NULL);
	if ( gconf_policy && gpm_hal_can_suspend () ) {
		*can = TRUE;
	}

	return TRUE;
}

/**
 * gpm_manager_can_hibernate:
 * @manager: This manager class instance
 * @can: If we can hibernate
 *
 * Checks the HAL key power_management.can_suspend_to_disk and also
 * checks gconf to see if we are allowed to hibernate this computer.
 **/
gboolean
gpm_manager_can_hibernate (GpmManager *manager,
			   gboolean   *can,
			   GError    **error)
{
	gboolean gconf_policy;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;
	gconf_policy = gconf_client_get_bool (manager->priv->gconf_client,
					      GPM_PREF_CAN_HIBERNATE, NULL);
	if ( gconf_policy && gpm_hal_can_hibernate () ) {
		*can = TRUE;
	}

	return TRUE;
}

/**
 * gpm_manager_can_shutdown:
 * @manager: This manager class instance
 * @can: If we can shutdown
 *
 * Stub function -- TODO.
 **/
gboolean
gpm_manager_can_shutdown (GpmManager *manager,
			  gboolean   *can,
			  GError    **error)
{
	if (can) {
		*can = FALSE;
	}
	/* FIXME: check other stuff */
	if (can) {
		*can = TRUE;
	}
	return TRUE;
}

/**
 * get_stock_id:
 * @manager: This manager class instance
 * @icon_policy: The policy set from gconf
 *
 * Get the stock filename id after analysing the state of all the devices
 * attached to the computer, and applying policy from gconf.
 *
 * Return value: The icon filename, must free using g_free.
 **/
static char *
get_stock_id (GpmManager *manager,
	      int	  icon_policy)
{
	GpmPowerStatus status_primary;
	GpmPowerStatus status_ups;
	GpmPowerStatus status_mouse;
	GpmPowerStatus status_keyboard;
	gboolean on_ac;
	gboolean present;

	if (icon_policy == GPM_ICON_POLICY_NEVER) {
		gpm_debug ("The key " GPM_PREF_ICON_POLICY
			   " is set to never, so no icon will be displayed.\n"
			   "You can change this using gnome-power-preferences");
		return NULL;
	}

	/* Finds if a device was found in the cache AND that it is present */
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_KIND_PRIMARY,
						&status_primary);
	status_primary.is_present &= present;
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_KIND_UPS,
						&status_ups);
	status_ups.is_present &= present;
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_KIND_MOUSE,
						&status_mouse);
	status_mouse.is_present &= present;
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_KIND_KEYBOARD,
						&status_keyboard);
	status_keyboard.is_present &= present;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	/* we try CRITICAL: PRIMARY, UPS, MOUSE, KEYBOARD */
	gpm_debug ("Trying CRITICAL icon: primary, ups, mouse, keyboard");
	if (status_primary.is_present &&
	    status_primary.percentage_charge < manager->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_primary, GPM_POWER_KIND_PRIMARY);

	} else if (status_ups.is_present &&
		   status_ups.percentage_charge < manager->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_UPS);

	} else if (status_mouse.is_present &&
		   status_mouse.percentage_charge < manager->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_MOUSE);

	} else if (status_keyboard.is_present &&
		   status_keyboard.percentage_charge < manager->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_KEYBOARD);
	}

	if (icon_policy == GPM_ICON_POLICY_CRITICAL) {
		gpm_debug ("no devices critical, so no icon will be displayed.");
		return NULL;
	}

	/* we try (DIS)CHARGING: PRIMARY, UPS */
	gpm_debug ("Trying CHARGING icon: primary, ups");
	if (status_primary.is_present &&
	    (status_primary.is_charging || status_primary.is_discharging) ) {
		return gpm_power_get_icon_from_status (&status_primary, GPM_POWER_KIND_PRIMARY);

	} else if (status_ups.is_present &&
		   (status_ups.is_charging || status_ups.is_discharging) ) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_UPS);
	}

	/* Check if we should just show the icon all the time */
	if (icon_policy == GPM_ICON_POLICY_CHARGE) {
		gpm_debug ("no devices (dis)charging, so no icon will be displayed.");
		return NULL;
	}

	/* we try PRESENT: PRIMARY, UPS */
	gpm_debug ("Trying PRESENT icon: primary, ups");
	if (status_primary.is_present) {
		return gpm_power_get_icon_from_status (&status_primary, GPM_POWER_KIND_PRIMARY);

	} else if (status_ups.is_present) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_UPS);
	}

	/* Check if we should just fallback to the ac icon */
	if (icon_policy == GPM_ICON_POLICY_PRESENT) {
		gpm_debug ("no devices present, so no icon will be displayed.");
		return NULL;
	}

	/* we fallback to the ac_adapter icon */
	gpm_debug ("Using fallback");
	return g_strdup_printf (GPM_STOCK_AC_ADAPTER);
}

/**
 * tray_icon_update:
 * @manager: This manager class instance
 *
 * Update the tray icon and set the correct tooltip when required, or remove
 * (hide) the icon when no longer required by policy.
 **/
static void
tray_icon_update (GpmManager *manager)
{
	char *stock_id = NULL;
	char *icon_policy_str;
	int   icon_policy;

	/* do we want to display the icon */
	icon_policy_str = gconf_client_get_string (manager->priv->gconf_client, GPM_PREF_ICON_POLICY, NULL);
	icon_policy = GPM_ICON_POLICY_ALWAYS;
	gconf_string_to_enum (icon_policy_enum_map, icon_policy_str, &icon_policy);
	g_free (icon_policy_str);

	/* try to get stock image */
	stock_id = get_stock_id (manager, icon_policy);

	gpm_debug ("Going to use stock id: %s", stock_id);

	/* only create if we have a valid filename */
	if (stock_id) {
		char *tooltip = NULL;

		gpm_tray_icon_set_image_from_stock (GPM_TRAY_ICON (manager->priv->tray_icon),
						    stock_id);

		/* make sure that we are visible */
		gpm_tray_icon_show (GPM_TRAY_ICON (manager->priv->tray_icon), TRUE);

		g_free (stock_id);

		gpm_power_get_status_summary (manager->priv->power, &tooltip, NULL);

		gpm_tray_icon_set_tooltip (GPM_TRAY_ICON (manager->priv->tray_icon),
					   tooltip);
		g_free (tooltip);
	} else {
		/* remove icon */
		gpm_debug ("no icon will be displayed");

		/* make sure that we are hidden */
		gpm_tray_icon_show (GPM_TRAY_ICON (manager->priv->tray_icon), FALSE);
	}
}

/**
 * sync_dpms_policy:
 * @manager: This manager class instance
 *
 * Sync the DPMS policy with what we have set in gconf.
 **/
static void
sync_dpms_policy (GpmManager *manager)
{
	GError  *error;
	gboolean res;
	gboolean on_ac;
	guint    standby;
	guint    suspend;
	guint    off;

	error = NULL;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	if (on_ac) {
		standby = gconf_client_get_int (manager->priv->gconf_client,
						GPM_PREF_AC_SLEEP_DISPLAY,
						&error);
	} else {
		standby = gconf_client_get_int (manager->priv->gconf_client,
						GPM_PREF_BATTERY_SLEEP_DISPLAY,
						&error);
	}

	if (error) {
		gpm_warning ("Unable to get DPMS timeouts: %s", error->message);
		g_error_free (error);
		return;
	}

	/* old policy was in seconds, warn the user if too small */
	if (standby < 60) {
		gpm_warning ("standby timeout is invalid, please re-configure");
		return;
	}

	/* try to make up some reasonable numbers */
	suspend = standby;
	off     = standby * 2;

	error = NULL;
	res = gpm_dpms_set_enabled (manager->priv->dpms, TRUE, &error);
	if (error) {
		gpm_warning ("Unable to enable DPMS: %s", error->message);
		g_error_free (error);
		return;
	}

	error = NULL;
	res = gpm_dpms_set_timeouts (manager->priv->dpms, standby, suspend, off, &error);
	if (error) {
		gpm_warning ("Unable to get DPMS timeouts: %s", error->message);
		g_error_free (error);
		return;
	}
}

static void
update_ac_throttle (GpmManager *manager,
		    gboolean    on_ac)
{
	/* Throttle the screensaver when we are not on AC power so we don't
	   waste the battery */
	if (on_ac) {
		if (manager->priv->ac_throttle_id > 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->ac_throttle_id);
			manager->priv->ac_throttle_id = 0;
		}
	} else {
		manager->priv->ac_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("On battery power"));
	}
}

static void
update_dpms_throttle (GpmManager *manager,
		      GpmDpmsMode mode)
{
	/* Throttle the screensaver when DPMS is active since we can't see it anyway */
	if (mode == GPM_DPMS_MODE_ON) {
		if (manager->priv->dpms_throttle_id > 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->dpms_throttle_id);
			manager->priv->dpms_throttle_id = 0;
		}
	} else {
		manager->priv->dpms_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("Display power management activated"));
	}
}

static void
update_lid_throttle (GpmManager	*manager,
		     gboolean    lid_is_closed)
{
	/* Throttle the screensaver when the lid is close since we can't see it anyway
	   and it may overheat the laptop */
	if (! lid_is_closed) {
		if (manager->priv->lid_throttle_id > 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->lid_throttle_id);
			manager->priv->lid_throttle_id = 0;
		}
	} else {
		manager->priv->lid_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("Laptop lid is closed"));
	}
}

/**
 * change_power_policy:
 * @manager: This manager class instance
 * @on_ac: If we are on AC power
 *
 * Changes the policy if required, setting brightness, display and computer
 * timeouts.
 * We have to make sure gnome-screensaver disables screensaving, and enables
 * monitor DPMS instead when on batteries to save power.
 **/
static void
change_power_policy (GpmManager *manager,
		     gboolean	 on_ac)
{
	int	     brightness;
	int	     sleep_display;
	int	     sleep_computer;
	gboolean     power_save;
	GConfClient *client;

	client = manager->priv->gconf_client;

	if (on_ac) {
		brightness = gconf_client_get_int (client, GPM_PREF_AC_BRIGHTNESS, NULL);
		sleep_computer = gconf_client_get_int (client, GPM_PREF_AC_SLEEP_COMPUTER, NULL);
		sleep_display = gconf_client_get_int (client, GPM_PREF_AC_SLEEP_DISPLAY, NULL);
		power_save = gconf_client_get_bool (client, GPM_PREF_AC_LOWPOWER, NULL);
	} else {
		brightness = gconf_client_get_int (client, GPM_PREF_BATTERY_BRIGHTNESS, NULL);
		sleep_computer = gconf_client_get_int (client, GPM_PREF_BATTERY_SLEEP_COMPUTER, NULL);
		sleep_display = gconf_client_get_int (client, GPM_PREF_BATTERY_SLEEP_DISPLAY, NULL);
		/* todo: what about when on UPS? */
		power_save = gconf_client_get_bool (client, GPM_PREF_BATTERY_LOWPOWER, NULL);
	}

	gpm_brightness_level_dim (manager->priv->brightness, brightness);
	gpm_hal_enable_power_save (power_save);

	update_ac_throttle (manager, on_ac);

	/* set the new sleep (inactivity) value */
	gpm_idle_set_system_timeout (manager->priv->idle, sleep_computer);
	sync_dpms_policy (manager);
}

/**
 * gpm_manager_get_lock_policy:
 * @manager: This manager class instance
 * @policy: The policy gconf string.
 *
 * This function finds out if we should lock the screen when we do an
 * action. It is required as we can either use the gnome-screensaver policy
 * or the custom policy. See the yelp file for more information.
 *
 * Return value: TRUE if we should lock.
 **/
static gboolean
gpm_manager_get_lock_policy (GpmManager *manager,
			     const char *policy)
{
	gboolean do_lock;
	gboolean use_ss_setting;
	/* This allows us to over-ride the custom lock settings set in gconf
	   with a system default set in gnome-screensaver.
	   See bug #331164 for all the juicy details. :-) */
	use_ss_setting = gconf_client_get_bool (manager->priv->gconf_client,
						GPM_PREF_LOCK_USE_SCREENSAVER,
						NULL);
	if (use_ss_setting) {
		do_lock = gpm_screensaver_lock_enabled (manager->priv->screensaver);
		gpm_debug ("Using ScreenSaver settings (%i)", do_lock);
	} else {
		do_lock = gconf_client_get_bool (manager->priv->gconf_client,
						 policy, NULL);
		gpm_debug ("Using custom locking settings (%i)", do_lock);
	}
	return do_lock;
}

/**
 * gpm_manager_blank_screen:
 * @manager: This manager class instance
 *
 * Turn off the backlight of the LCD when we shut the lid, and lock
 * if required. This is required because some laptops do not turn off the
 * LCD backlight when the lid is closed.
 * See http://bugzilla.gnome.org/show_bug.cgi?id=321313
 *
 * Return value: Success.
 **/
static gboolean
gpm_manager_blank_screen (GpmManager *manager,
			  GError    **noerror)
{
	gboolean do_lock;
	gboolean ret = TRUE;

	gpm_info_event_log (manager->priv->info, GPM_GRAPH_EVENT_DPMS_OFF, NULL);

	do_lock = gpm_manager_get_lock_policy (manager,
					       GPM_PREF_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		if (!gpm_screensaver_lock (manager->priv->screensaver))
			gpm_debug ("Could not lock screen via gnome-screensaver");
	}
	GError     *error = NULL;
	gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_OFF, &error);
	if (error) {
		gpm_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}
	return ret;
}

/**
 * gpm_manager_unblank_screen:
 * @manager: This manager class instance
 *
 * Unblank the screen after we have opened the lid of the laptop
 *
 * Return value: Success.
 **/
static gboolean
gpm_manager_unblank_screen (GpmManager *manager,
			    GError    **noerror)
{
	gboolean  do_lock;
	gboolean  ret = TRUE;
	GError   *error;

	error = NULL;
	gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_ON, &error);
	if (error) {
		gpm_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	do_lock = gpm_manager_get_lock_policy (manager,
					       GPM_PREF_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		gpm_screensaver_poke (manager->priv->screensaver);
	}
	return ret;
}

/**
 * manager_explain_reason:
 * @manager: This manager class instance
 * @event: The event type, e.g. GPM_GRAPH_EVENT_DPMS_OFF
 * @pre: The action we are about to do, e.g. "Suspending computer"
 * @post: The reason we are performing the policy action, e.g. "battery critical"
 *
 * Helper function
 **/
static void
manager_explain_reason (GpmManager   *manager,
			GpmGraphEvent event,
			const char   *pre,
			const char   *post)
{
	char *message;
	if (post) {
		message = g_strdup_printf (_("%s because %s"), pre, post);
	} else {
		message = g_strdup (pre);
	}
	gpm_syslog (message);
	gpm_info_event_log (manager->priv->info, event, message);
	g_free (message);
}

/**
 * manager_policy_do:
 * @manager: This manager class instance
 * @policy: The policy that we should do, e.g. "suspend"
 * @reason: The reason we are performing the policy action, e.g. "battery critical"
 *
 * Does one of the policy actions specified in gconf.
 **/
static void
manager_policy_do (GpmManager *manager,
		   const char *policy,
		   const char *reason)
{
	char *action;

	gpm_debug ("policy: %s", policy);
	action = gconf_client_get_string (manager->priv->gconf_client, policy, NULL);

	if (! action) {
		return;
	}
	if (! gpm_manager_is_policy_timout_valid (manager, "policy event")) {
		return;
	}

	if (strcmp (action, ACTION_NOTHING) == 0) {
		manager_explain_reason (manager, GPM_GRAPH_EVENT_NOTIFICATION,
					_("Doing nothing"), reason);

	} else if (strcmp (action, ACTION_SUSPEND) == 0) {
		manager_explain_reason (manager, GPM_GRAPH_EVENT_SUSPEND,
					_("Suspending computer"), reason);
		gpm_manager_suspend (manager, NULL);

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
		manager_explain_reason (manager, GPM_GRAPH_EVENT_HIBERNATE,
					_("Hibernating computer"), reason);
		gpm_manager_hibernate (manager, NULL);

	} else if (strcmp (action, ACTION_BLANK) == 0) {
		manager_explain_reason (manager, GPM_GRAPH_EVENT_DPMS_OFF,
					_("DPMS blanking screen"), reason);
		gpm_manager_blank_screen (manager, NULL);

	} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
		manager_explain_reason (manager, GPM_GRAPH_EVENT_NOTIFICATION,
					_("Shutting down computer"), reason);
		gpm_manager_shutdown (manager, NULL);

	} else if (strcmp (action, ACTION_INTERACTIVE) == 0) {
		manager_explain_reason (manager, GPM_GRAPH_EVENT_NOTIFICATION,
					_("GNOME interactive logout"), reason);
		gnome_client_request_save (gnome_master_client (),
					   GNOME_SAVE_GLOBAL,
					   TRUE, GNOME_INTERACT_ANY, FALSE, TRUE);
	} else {
		gpm_warning ("unknown action %s", action);
	}

	g_free (action);
}

/**
 * gpm_manager_get_on_ac:
 * @manager: This manager class instance
 * @on_ac: TRUE if we are on AC power
 **/
gboolean
gpm_manager_get_on_ac (GpmManager  *manager,
			gboolean   *on_ac,
			GError    **error)
{
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	if (on_ac) {
		gpm_power_get_on_ac (manager->priv->power, on_ac, error);
	}

	return TRUE;
}

/**
 * gpm_manager_set_dpms_mode:
 * @manager: This manager class instance
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_STANDBY
 * Return value: TRUE if we could set the GPMS mode OK.
 **/
gboolean
gpm_manager_set_dpms_mode (GpmManager *manager,
			   const char *mode,
			   GError    **error)
{
	gboolean ret;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	gpm_debug ("Setting DPMS to %s", mode);

	/* just proxy this */
	ret = gpm_dpms_set_mode (manager->priv->dpms,
				 gpm_dpms_mode_from_string (mode),
				 error);

	return ret;
}

/**
 * gpm_manager_get_dpms_mode:
 * @manager: This manager class instance
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_STANDBY
 * Return value: TRUE if we could get the GPMS mode OK.
 **/
gboolean
gpm_manager_get_dpms_mode (GpmManager  *manager,
			   const char **mode,
			   GError     **error)
{
	gboolean    ret;
	GpmDpmsMode m;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	ret = gpm_dpms_get_mode (manager->priv->dpms,
				 &m,
				 error);
	gpm_debug ("Got DPMS mode result=%d mode=%d", ret, m);
	if (ret && mode) {
		*mode = gpm_dpms_mode_to_string (m);
	}

	return ret;
}

/**
 * gpm_manager_inhibit_inactive_sleep:
 * @manager: This manager class instance
 * @application: The application that sent the request, e.g. "Nautilus"
 * @reason: The reason given to inhibit, e.g. "Copying files"
 * @context: The context we are talking to
 *
 * Processes an inhibit request from an application that want to stop the
 * idle action suspend from happening.
 **/
void
gpm_manager_inhibit_inactive_sleep (GpmManager	*manager,
				    const char	*application,
				    const char	*reason,
				    DBusGMethodInvocation *context,
				    GError    **error)
{
	const char* connection = dbus_g_method_get_sender (context);
	int cookie;
	cookie = gpm_inhibit_add (manager->priv->inhibit, connection, application, reason);
	dbus_g_method_return (context, cookie);
}

/**
 * gpm_manager_allow_inactive_sleep:
 * @manager: This manager class instance
 * @cookie: The application cookie, e.g. 17534
 * @context: The context we are talking to
 *
 * Processes an allow request from an application that want to allow the
 * idle action suspend to happen.
 **/
void
gpm_manager_allow_inactive_sleep (GpmManager	 *manager,
				  int		  cookie,
				  DBusGMethodInvocation *context,
				  GError	**error)
{
	const char* connection = dbus_g_method_get_sender (context);
	gpm_inhibit_remove (manager->priv->inhibit, connection, cookie);
	dbus_g_method_return (context);
}

/**
 * gpm_manager_shutdown:
 * @manager: This manager class instance
 *
 * Shuts down the computer, saving the session if possible.
 **/
static gboolean
gpm_manager_shutdown (GpmManager *manager,
		      GError    **error)
{
	gboolean allowed;
	gboolean ret;

	gpm_manager_can_shutdown (manager, &allowed, NULL);
	if (! allowed) {
		gpm_warning ("Cannot shutdown");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot shutdown");
		return FALSE;
	}

	gnome_client_request_save (gnome_master_client (),
				   GNOME_SAVE_GLOBAL,
				   FALSE, GNOME_INTERACT_NONE, FALSE,  TRUE);

	/* FIXME: make this async? */
	gpm_hal_shutdown ();
	ret = TRUE;

	return ret;
}

/**
 * gpm_manager_hibernate:
 * @manager: This manager class instance
 *
 * We want to hibernate the computer. This function deals with the "fluff" -
 * like disconnecting the networks and locking the screen, then does the
 * hibernate using HAL, then handles all the resume actions.
 *
 * Return value: If the hibernate was successful.
 **/
static gboolean
gpm_manager_hibernate (GpmManager *manager,
			GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean do_lock;

	gpm_manager_can_hibernate (manager, &allowed, NULL);

	if (! allowed) {
		gpm_warning ("Cannot hibernate");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot hibernate");
		return FALSE;
	}

	do_lock = gpm_manager_get_lock_policy (manager,
					       GPM_PREF_LOCK_ON_HIBERNATE);
	if (do_lock) {
		gpm_screensaver_lock (manager->priv->screensaver);
	}

	gpm_networkmanager_sleep ();

	/* FIXME: make this async? */
	ret = gpm_hal_hibernate ();
	manager_explain_reason (manager, GPM_GRAPH_EVENT_RESUME,
				_("Resuming computer"), NULL);

	/* we need to refresh all the power caches */
	gpm_power_update_all (manager->priv->power);

	if (! ret) {
		char *message;
		gboolean show_notify;
		/* We only show the HAL failed notification if set in gconf */
		show_notify = gconf_client_get_bool (manager->priv->gconf_client,
						     GPM_PREF_NOTIFY_HAL_ERROR, NULL);
		if (show_notify) {
			message = g_strdup_printf (_("HAL failed to %s. "
						     "Check the <a href=\"%s\">FAQ page</a> for common problems."),
						     _("hibernate"), GPM_FAQ_URL);
			const char *title = _("Hibernate Problem");
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      title,
					      message,
					      GPM_NOTIFY_TIMEOUT_LONG,
					      GTK_STOCK_DIALOG_WARNING,
					      GPM_NOTIFY_URGENCY_LOW);
			gpm_info_event_log (manager->priv->info,
					    GPM_GRAPH_EVENT_NOTIFICATION, title);
			g_free (message);
		}
	}

	if (do_lock) {
		gpm_screensaver_poke (manager->priv->screensaver);
	}
	gpm_networkmanager_wake ();

	sync_dpms_policy (manager);

	/* save the time that we resumed */
	manager->priv->last_resume_event = time (NULL);

	return ret;
}

/**
 * gpm_manager_suspend:
 * @manager: This manager class instance
 *
 * We want to suspend the computer. This function deals with the "fluff" -
 * like disconnecting the networks and locking the screen, then does the
 * suspend using HAL, then handles all the resume actions.
 *
 * Return value: If the suspend was successful.
 **/
static gboolean
gpm_manager_suspend (GpmManager *manager,
		     GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean do_lock;
	GpmPowerStatus status;
	char *message;

	gpm_manager_can_suspend (manager, &allowed, NULL);

	if (! allowed) {
		gpm_warning ("Cannot suspend");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot suspend");
		return FALSE;
	}

	do_lock = gpm_manager_get_lock_policy (manager,
					       GPM_PREF_LOCK_ON_SUSPEND);
	if (do_lock) {
		gpm_screensaver_lock (manager->priv->screensaver);
	}

	gpm_networkmanager_sleep ();

	/* We save the current charge in mWh so we can see how much power we
	   lost or gained over the suspend cycle */
	gpm_power_get_battery_status (manager->priv->power,
				      GPM_POWER_KIND_PRIMARY,
				      &status);
	int charge_before_suspend = status.current_charge;

	/* Do the suspend */
	ret = gpm_hal_suspend (0);
	manager_explain_reason (manager, GPM_GRAPH_EVENT_RESUME,
				_("Resuming computer"), NULL);

	/* We need to refresh all the power caches */
	gpm_power_update_all (manager->priv->power);

	/* Get the difference in charge and add it to the event log */
	gpm_power_get_battery_status (manager->priv->power,
				      GPM_POWER_KIND_PRIMARY,
				      &status);
	int charge_difference = status.current_charge - charge_before_suspend;
	if (charge_difference != 0) {
		if (charge_difference > 0) {
			message = g_strdup_printf (_("Battery charged %imWh during suspend"),
						   charge_difference);
		} else {
			message = g_strdup_printf (_("Battery discharged %imWh during suspend"),
						   -charge_difference);
		}
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_NOTIFICATION,
				    message);
		g_free (message);
	}

	gboolean show_notify;
	/* We only show the HAL failed notification if set in gconf */
	show_notify = gconf_client_get_bool (manager->priv->gconf_client,
					     GPM_PREF_NOTIFY_HAL_ERROR, NULL);
	if ((!ret) && show_notify) {
		const char *title;
		message = g_strdup_printf (_("Your computer failed to %s.\n"
					     "Check the <a href=\"%s\">FAQ page</a> for common problems."),
					     _("suspend"), GPM_FAQ_URL);
		title = _("Suspend Problem");
		gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
				      title,
				      message,
				      GPM_NOTIFY_TIMEOUT_LONG,
				      GTK_STOCK_DIALOG_WARNING,
				      GPM_NOTIFY_URGENCY_LOW);
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_NOTIFICATION,
				    title);
		g_free (message);
	}

	if (do_lock) {
		gpm_screensaver_poke (manager->priv->screensaver);
	}
	gpm_networkmanager_wake ();

	sync_dpms_policy (manager);

	/* save the time that we resumed */
	manager->priv->last_resume_event = time (NULL);

	return ret;
}

/**
 * gpm_manager_suspend_dbus_method:
 * @manager: This manager class instance
 **/
gboolean
gpm_manager_suspend_dbus_method (GpmManager *manager,
				 GError    **error)
{
	/* FIXME: From where? */
	manager_explain_reason (manager, GPM_GRAPH_EVENT_SUSPEND,
				_("Suspending computer"),
				_("the DBUS method Suspend() was invoked"));
	return gpm_manager_suspend (manager, error);
}

/**
 * gpm_manager_hibernate_dbus_method:
 **/
gboolean
gpm_manager_hibernate_dbus_method (GpmManager *manager,
				   GError    **error)
{
	/* FIXME: From where? */
	manager_explain_reason (manager, GPM_GRAPH_EVENT_HIBERNATE,
				_("Hibernating computer"),
				_("the DBUS method Hibernate() was invoked"));
	return gpm_manager_hibernate (manager, error);
}

/**
 * gpm_manager_shutdown_dbus_method:
 * @manager: This manager class instance
 **/
gboolean
gpm_manager_shutdown_dbus_method (GpmManager *manager,
				  GError    **error)
{
	/* FIXME: From where? */
	manager_explain_reason (manager, GPM_GRAPH_EVENT_NOTIFICATION,
				_("Shutting down computer"),
				_("the DBUS method Shutdown() was invoked"));
	return gpm_manager_shutdown (manager, error);
}

/**
 * idle_changed_cb:
 * @idle: The idle class instance
 * @mode: The idle mode, e.g. GPM_IDLE_MODE_SESSION
 * @manager: This manager class instance
 *
 * This callback is called when gnome-screensaver detects that the idle state
 * has changed. GPM_IDLE_MODE_SESSION is when the session has become inactive,
 * and GPM_IDLE_MODE_SYSTEM is where the session has become inactive, AND the
 * session timeout has elapsed for the idle action.
 **/
static void
idle_changed_cb (GpmIdle    *idle,
		 GpmIdleMode mode,
		 GpmManager *manager)
{
	GError  *error;
	gboolean do_laptop_dim;

	/* Ignore timeout events when the lid is closed, as the DPMS is
	   already off, and we don't want to perform policy actions or re-enable
	   the screen when the user moves the mouse on systems that do not
	   support hardware blanking.
	   Details are here: https://launchpad.net/malone/bugs/22522 */
	if (manager->priv->lid_is_closed) {
		gpm_debug ("lid is closed, so we are ignoring idle state changes");
		return;
	}

	switch (mode) {
	case GPM_IDLE_MODE_NORMAL:
		gpm_debug ("Idle state changed: NORMAL");

		/* deactivate display power management */
		error = NULL;
		gpm_dpms_set_active (manager->priv->dpms, FALSE, &error);
		if (error) {
			gpm_debug ("Unable to set DPMS active: %s", error->message);
		}

		/* Should we dim the screen? */
		do_laptop_dim = gconf_client_get_bool (manager->priv->gconf_client,
							GPM_PREF_IDLE_DIM_SCREEN, NULL);
		if (do_laptop_dim) {
			/* resume to the previous brightness */
			manager_explain_reason (manager, GPM_GRAPH_EVENT_SCREEN_RESUME,
						_("Screen resume"),
						_("idle mode ended"));
			gpm_brightness_level_resume (manager->priv->brightness);
		}

		/* sync timeouts */
		sync_dpms_policy (manager);

		break;
	case GPM_IDLE_MODE_SESSION:

		gpm_debug ("Idle state changed: SESSION");

		/* Activate display power management */
		error = NULL;
		gpm_dpms_set_active (manager->priv->dpms, TRUE, &error);
		if (error) {
			gpm_debug ("Unable to set DPMS active: %s", error->message);
		}

		/* Should we resume the screen? */
		do_laptop_dim = gconf_client_get_bool (manager->priv->gconf_client,
						       GPM_PREF_IDLE_DIM_SCREEN, NULL);
		if (do_laptop_dim) {
			int dim_br;
			int current_br;
			dim_br = manager->priv->lcd_dim_brightness;
			current_br = gpm_brightness_level_get (manager->priv->brightness);
			if (current_br < dim_br) {
				/* If the current brightness is less than the dim
				 * brightness then just use the lowest brightness
				 * so that we don't *increase* in brightness on idle.
				 * See #338630 for more details */
				gpm_warning ("Current brightness is %i, dim brightness is %i.",
					     current_br, dim_br);
				dim_br = current_br;
			}
			/* Save this brightness and dim the screen, fixes #328564 */
			manager_explain_reason (manager, GPM_GRAPH_EVENT_SCREEN_DIM,
						_("Screen dim"),
						_("idle mode started"));
			gpm_brightness_level_save (manager->priv->brightness, dim_br);
		}

		/* sync timeouts */
		sync_dpms_policy (manager);

		break;
	case GPM_IDLE_MODE_SYSTEM:
		gpm_debug ("Idle state changed: SYSTEM");

		if (! gpm_manager_is_policy_timout_valid (manager, "timeout action")) {
			return;
		}
		if (! gpm_manager_is_inhibit_valid (manager, "timeout action")) {
			return;
		}
		/* can only be hibernate or suspend */
		manager_policy_do (manager, GPM_PREF_SLEEP_TYPE, _("the system state is idle"));

		break;
	default:
		g_assert_not_reached ();
		break;
	}

}

/**
 * dpms_mode_changed_cb:
 * @dpms: dpmsdesc
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @manager: This manager class instance
 *
 * What happens when the DPMS mode is changed.
 **/
static void
dpms_mode_changed_cb (GpmDpms    *dpms,
		      GpmDpmsMode mode,
		      GpmManager *manager)
{
	gpm_debug ("DPMS mode changed: %d", mode);

	update_dpms_throttle (manager, mode);

	gpm_debug ("emitting dpms-mode-changed : %s", gpm_dpms_mode_to_string (mode));
	g_signal_emit (manager,
			signals [DPMS_MODE_CHANGED],
			0,
			gpm_dpms_mode_to_string (mode));
}

/**
 * battery_button_pressed:
 * @manager: This manager class instance
 *
 * What to do when the battery button is pressed. This used to be allocated to
 * "www", but now we watch for "battery" which has to go upstream to HAL and
 * the kernel.
 **/
static void
battery_button_pressed (GpmManager *manager)
{
	char *message;

	gpm_power_get_status_summary (manager->priv->power, &message, NULL);

	gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
			      _("Power Information"),
			      message,
			      GPM_NOTIFY_TIMEOUT_LONG,
			      GTK_STOCK_DIALOG_INFO,
			      GPM_NOTIFY_URGENCY_NORMAL);
	g_free (message);
}

/**
 * power_button_pressed:
 * @manager: This manager class instance
 *
 * What to do when the power button is pressed.
 **/
static void
power_button_pressed (GpmManager   *manager)
{
	if (! gpm_manager_is_policy_timout_valid (manager, "power button press")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "power button press")) {
		return;
	}
	gpm_debug ("power button pressed");
	manager_policy_do (manager, GPM_PREF_BUTTON_POWER, _("the power button has been pressed"));
}

/**
 * suspend_button_pressed:
 * @manager: This manager class instance
 *
 * What to do when the suspend button is pressed.
 **/
static void
suspend_button_pressed (GpmManager   *manager)
{
	if (! gpm_manager_is_policy_timout_valid (manager, "suspend button press")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "suspend button press")) {
		return;
	}
	gpm_debug ("suspend button pressed");
	manager_policy_do (manager, GPM_PREF_BUTTON_SUSPEND, _("the suspend button has been pressed"));
}

/**
 * hibernate_button_pressed:
 * @manager: This manager class instance
 *
 * What to do when the hibernate button is pressed.
 **/
static void
hibernate_button_pressed (GpmManager   *manager)
{
	if (! gpm_manager_is_policy_timout_valid (manager, "hibernate button press")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "hibernate button press")) {
		return;
	}
	manager_policy_do (manager,
			   GPM_PREF_BUTTON_HIBERNATE,
			   _("the hibernate button has been pressed"));
}

/**
 * lid_button_pressed:
 * @manager: This manager class instance
 * @state: TRUE for closed
 *
 * Does actions when the lid is closed, depending on if we are on AC or
 * battery power.
 **/
static void
lid_button_pressed (GpmManager	 *manager,
		    gboolean	  state)
{
	gboolean  on_ac;

	if (manager->priv->lid_is_closed == state) {
		gpm_debug ("duplicate lid change event");
		return;
	}

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	if (state) {
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_LID_CLOSED,
				    "The laptop lid has been closed");
		gpm_debug ("lid button CLOSED");
	} else {
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_LID_OPENED,
				    "The laptop lid has been re-opened");
		gpm_debug ("lid button OPENED");
	}

	/* We keep track of the lid state so we can do the
	   lid close on battery action if the ac_adapter is removed when the laptop
	   is closed. Fixes #331655 */
	manager->priv->lid_is_closed = state;

	/* Disable or enable the fancy screensaver, as we don't want this starting
	   when the lid is shut */
	update_lid_throttle (manager, manager->priv->lid_is_closed);

	if (state) {

		if (on_ac) {
			gpm_debug ("Performing AC policy");
			manager_policy_do (manager,
					   GPM_PREF_AC_BUTTON_LID,
					   _("the lid has been closed on ac power"));
		} else {
			gpm_debug ("Performing battery policy");
			manager_policy_do (manager,
					   GPM_PREF_BATTERY_BUTTON_LID,
					   _("the lid has been closed on battery power"));
		}
	} else {
		/* we turn the lid dpms back on unconditionally */
		manager_explain_reason (manager, GPM_GRAPH_EVENT_DPMS_ON,
					_("Turning LCD panel back on"),
					_("laptop lid re-opened"));
		gpm_manager_unblank_screen (manager, NULL);
	}
}

/**
 * brightness_step_changed_cb:
 * @brightness: The power class instance
 * @percentage: The new brightness setting
 * @manager: This manager class instance
 *
 * The callback when the brightness is stepped up or stepped down
 **/
static void
brightness_step_changed_cb (GpmBrightness *brightness,
			    int		   percentage,
			    GpmManager	  *manager)
{
	gpm_debug ("Need to diplay feedback value %i", percentage);
	gpm_feedback_display_value (manager->priv->feedback, (float) percentage / 100.0f);
}

/**
 * power_button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @manager: This manager class instance
 *
 * description
 **/
static void
power_button_pressed_cb (GpmPower   *power,
			 const char *type,
			 gboolean    state,
			 GpmManager *manager)
{
	gpm_debug ("Button press event type=%s state=%d", type, state);

	/* simulate user input */
	gpm_screensaver_poke (manager->priv->screensaver);

	if (strcmp (type, GPM_BUTTON_POWER) == 0) {
		power_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_SLEEP) == 0) {
		suspend_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_SUSPEND) == 0) {
		suspend_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_HIBERNATE) == 0) {
		hibernate_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_LID) == 0) {
		lid_button_pressed (manager, state);

	} else if ((strcmp (type, GPM_BUTTON_BRIGHT_UP) == 0) ||
		   (strcmp (type, GPM_BUTTON_BRIGHT_UP_DEP) == 0)) {
		gpm_brightness_level_up (manager->priv->brightness);

	} else if ((strcmp (type, GPM_BUTTON_BRIGHT_DOWN) == 0) ||
		   (strcmp (type, GPM_BUTTON_BRIGHT_DOWN_DEP) == 0)) {
		gpm_brightness_level_down (manager->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_LOCK) == 0) {
		gpm_screensaver_lock (manager->priv->screensaver);

	} else if (strcmp (type, GPM_BUTTON_BATTERY) == 0) {
		battery_button_pressed (manager);
	}
}

/**
 * power_on_ac_changed_cb:
 * @power: The power class instance
 * @on_ac: if we are on AC power
 * @manager: This manager class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
power_on_ac_changed_cb (GpmPower   *power,
			gboolean    on_ac,
			GpmManager *manager)
{
	gpm_debug ("Setting on-ac: %d", on_ac);

	/* simulate user input, to fix #333525 */
	gpm_screensaver_poke (manager->priv->screensaver);

	/* If we are on AC power we should show warnings again */
	if (on_ac) {
		gpm_debug ("Resetting warning to NONE as on AC power");
		manager->priv->last_primary_warning = GPM_WARNING_NONE;
	}

	tray_icon_update (manager);

	if (on_ac) {
		/* for where we add back the ac_adapter before
		 * the "AC Power unplugged" message times out. */
		gpm_tray_icon_cancel_notify (GPM_TRAY_ICON (manager->priv->tray_icon));
	}
	change_power_policy (manager, on_ac);

	gpm_debug ("emitting on-ac-changed : %i", on_ac);
	g_signal_emit (manager, signals [ON_AC_CHANGED], 0, on_ac);
	if (on_ac) {
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_ON_AC,
				    "AC adapter inserted");
	} else {
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_ON_BATTERY,
				    "AC adapter removed");
	}

	/* We do the lid close on battery action if the ac_adapter is removed
	   when the laptop is closed and on battery. Fixes #331655 */
	if ( (! on_ac) && manager->priv->lid_is_closed) {
		manager_policy_do (manager,
				   GPM_PREF_BATTERY_BUTTON_LID,
				   _("the lid has been closed, and the ac adapter removed"));
	}
}

/**
 * gpm_manager_get_warning_type:
 * @manager: This manager class instance
 * @battery_status: The battery status information
 * @use_time: If we should use a per-time or per-percent policy
 *
 * This gets the possible warning state for the device according to the
 * policy, which could be per-percent, or per-time.
 *
 * Return value: A GpmWarning state, e.g. GPM_WARNING_VERY_LOW
 **/
static GpmWarning
gpm_manager_get_warning_type (GpmManager	    *manager,
			      GpmPowerStatus *status,
			      gboolean		     use_time)
{
	GpmWarning type = GPM_WARNING_NONE;

	/* this is a CSR mouse */
	if (status->design_charge == 7) {
		if (status->current_charge == 2) {
			type = GPM_WARNING_LOW;
		} else if (status->current_charge == 1) {
			type = GPM_WARNING_VERY_LOW;
		}
	} else if (use_time) {
		if (status->remaining_time <= 0) {
			type = GPM_WARNING_NONE;
		} else if (status->remaining_time <= manager->priv->action_time) {
			type = GPM_WARNING_ACTION;
		} else if (status->remaining_time <= manager->priv->critical_time) {
			type = GPM_WARNING_CRITICAL;
		} else if (status->remaining_time <= manager->priv->very_low_time) {
			type = GPM_WARNING_VERY_LOW;
		} else if (status->remaining_time <= manager->priv->low_time) {
			type = GPM_WARNING_LOW;
		}
	} else {
		if (status->percentage_charge <= 0) {
			type = GPM_WARNING_NONE;
			gpm_warning ("Your hardware is reporting a percentage "
				     "charge of %i, which is impossible. "
				     "WARNING_ACTION will *not* be reported.",
				     status->percentage_charge);
		} else if (status->percentage_charge <= manager->priv->action_percentage) {
			type = GPM_WARNING_ACTION;
		} else if (status->percentage_charge <= manager->priv->critical_percentage) {
			type = GPM_WARNING_CRITICAL;
		} else if (status->percentage_charge <= manager->priv->very_low_percentage) {
			type = GPM_WARNING_VERY_LOW;
		} else if (status->percentage_charge <= manager->priv->low_percentage) {
			type = GPM_WARNING_LOW;
		}
	}

	/* If we have no important warnings, we should test for discharging */
	if (type == GPM_WARNING_NONE) {
		if (status->is_discharging) {
			type = GPM_WARNING_DISCHARGING;
		}
	}
	return type;
}

/**
 * battery_low_get_title:
 * @warning_type: The warning type, e.g. GPM_WARNING_VERY_LOW
 * Return value: the title text according to the warning type.
 **/
static const char *
battery_low_get_title (GpmWarning warning_type)
{
	char *title = NULL;

	if (warning_type == GPM_WARNING_ACTION ||
	    warning_type == GPM_WARNING_CRITICAL) {

		title = _("Power Critically Low");

	} else if (warning_type == GPM_WARNING_VERY_LOW) {

		title = _("Power Very Low");

	} else if (warning_type == GPM_WARNING_LOW) {

		title = _("Power Low");

	} else if (warning_type == GPM_WARNING_DISCHARGING) {

		title = _("Power Information");

	}

	return title;
}

/**
 * manager_critical_action_do:
 * @manager: This manager class instance
 *
 * This is the stub function when we have waited a few seconds for the user to
 * see the message, explaining what we are about to do.
 *
 * Return value: FALSE, as we don't want to repeat this action on resume.
 **/
static gboolean
manager_critical_action_do (GpmManager *manager)
{
	manager_policy_do (manager,
			   GPM_PREF_BATTERY_CRITICAL,
			   _("battery is critically low"));
	return FALSE;
}

/**
 * battery_status_changed_primary:
 * @manager: This manager class instance
 * @battery_kind: The battery kind, e.g. GPM_POWER_KIND_PRIMARY
 * @battery_status: The battery status information
 *
 * Hander for the battery status changed event for primary devices
 * (laptop batteries). We notify of increasing power notification levels,
 * and also do the critical actions here.
 **/
static void
battery_status_changed_primary (GpmManager	      *manager,
				GpmPowerKind    battery_kind,
				GpmPowerStatus *battery_status)
{
	GpmWarning  warning_type;
	gboolean    show_notify;
	char	   *message = NULL;
	char	   *remaining = NULL;
	const char *title = NULL;
	gboolean    on_ac;
	int	    timeout;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	/* If we are charging we should show warnings again as soon as we discharge again */
	if (battery_status->is_charging) {
		gpm_debug ("Resetting warning to NONE as charging");
		manager->priv->last_primary_warning = GPM_WARNING_NONE;
	}

	/* We have to track the last percentage, as when we do the transition
	 * 99 to 100 some laptops report this as charging, some as not-charging.
	 * This is probably a race. This method should work for both cases. */
	if (manager->priv->last_primary_percentage_change == 99 &&
	    battery_status->percentage_charge == 100 &&
	    ! manager->priv->done_notify_fully_charged) {
		show_notify = gconf_client_get_bool (manager->priv->gconf_client,
						     GPM_PREF_NOTIFY_BATTCHARGED, NULL);
		if (show_notify) {
			const char *message = _("Your battery is now fully charged");
			const char *title = _("Battery Charged");
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      title,
					      message,
					      GPM_NOTIFY_TIMEOUT_SHORT,
					      GPM_STOCK_BATTERY_CHARGED,
					      GPM_NOTIFY_URGENCY_LOW);
			gpm_info_event_log (manager->priv->info,
					    GPM_GRAPH_EVENT_NOTIFICATION,
					    title);
		}
		manager->priv->done_notify_fully_charged = TRUE;
	}
	/* We only re-enable the fully charged notification when the battery
	   drops down to 95% as some batteries charge to 100% and then fluctuate
	   from ~98% to 100%. See #338281 for details */
	if (manager->priv->last_primary_percentage_change < 95) {
		manager->priv->done_notify_fully_charged = FALSE;
	}

	manager->priv->last_primary_percentage_change = battery_status->percentage_charge;

	if (! battery_status->is_discharging) {
		gpm_debug ("%s is not discharging", gpm_power_kind_to_localised_string (battery_kind));
		return;
	}

	if (on_ac) {
		gpm_debug ("Computer marked as on_ac.");
		return;
	}

	warning_type = gpm_manager_get_warning_type (manager,
						     battery_status,
						     manager->priv->use_time_to_notify);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (warning_type <= manager->priv->last_primary_warning) {
		gpm_debug ("Already notified %i", warning_type);
		return;
	}

	/* As the level is more critical than the last warning, save it */
	manager->priv->last_primary_warning = warning_type;

	/* Do different warnings for each GPM_WARNING_* */
	if (warning_type == GPM_WARNING_ACTION) {
		const char *action;
		timeout = GPM_NOTIFY_TIMEOUT_LONG;

		if (! gpm_manager_is_policy_timout_valid (manager, "critical action")) {
			return;
		}

		/* we have to do different warnings depending on the policy */
		action = gconf_client_get_string (manager->priv->gconf_client,
						  GPM_PREF_BATTERY_CRITICAL, NULL);

		/* TODO: we should probably convert to an ENUM type, and use that */
		if (strcmp (action, ACTION_NOTHING) == 0) {
			message = _("The battery is below the critical level and "
				    "this computer will <b>power-off</b> when the "
				    "battery becomes completely empty.");

		} else if (strcmp (action, ACTION_SUSPEND) == 0) {
			message = _("The battery is below the critical level and "
				    "this computer is about to suspend.<br>"
				    "<b>NOTE:</b> A small amount of power is required "
				    "to keep your computer in a suspended state.");

		} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
			message = _("The battery is below the critical level and "
				    "this computer is about to hibernate.");

		} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
			message = _("The battery is below the critical level and "
				    "this computer is about to shutdown.");
		}

		/* wait 10 seconds for user-panic */
		g_timeout_add (1000*10, (GSourceFunc) manager_critical_action_do, manager);

	} else if (warning_type == GPM_WARNING_DISCHARGING) {
		gboolean show_notify;
		show_notify = gconf_client_get_bool (manager->priv->gconf_client,
						     GPM_PREF_NOTIFY_ACADAPTER, NULL);
		if (show_notify) {
			message = g_strdup_printf (_("The AC Power has been unplugged. "
						      "The system is now using battery power."));
			timeout = GPM_NOTIFY_TIMEOUT_SHORT;
		}

	} else {
		remaining = gpm_get_timestring (battery_status->remaining_time);
		message = g_strdup_printf (_("You have approximately <b>%s</b> "
					     "of remaining battery life (%d%%). "
					     "Plug in your AC Adapter to avoid losing data."),
					   remaining, battery_status->percentage_charge);
			timeout = GPM_NOTIFY_TIMEOUT_LONG;
		g_free (remaining);
	}

	/* If we had a message, print it as a notification */
	if (message) {
		const char *icon;
		title = battery_low_get_title (warning_type);
		icon = gpm_power_get_icon_from_status (battery_status, battery_kind);
		gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
				      title, message, timeout,
				      icon, GPM_NOTIFY_URGENCY_NORMAL);
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_NOTIFICATION,
				    title);
		g_free (message);
	}
}

/**
 * battery_status_changed_ups:
 * @manager: This manager class instance
 * @battery_kind: The battery kind, e.g. GPM_POWER_KIND_UPS
 * @battery_status: The battery status information
 *
 * Hander for the battery status changed event for UPS devices.
 * At the moment we only notify, but we should put some shutdown handlers in.
 **/
static void
battery_status_changed_ups (GpmManager	   *manager,
			    GpmPowerKind    battery_kind,
			    GpmPowerStatus *battery_status)
{
	GpmWarning warning_type;
	char *message = NULL;
	char *remaining = NULL;
	const char *title = NULL;

	/* If we are charging we should show warnings again as soon as we discharge again */
	if (battery_status->is_charging) {
		manager->priv->last_ups_warning = GPM_WARNING_NONE;
	}

	if (! battery_status->is_discharging) {
		gpm_debug ("%s is not discharging", gpm_power_kind_to_localised_string(battery_kind));
		return;
	}

	warning_type = gpm_manager_get_warning_type (manager,
						     battery_status,
						     manager->priv->use_time_to_notify);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (warning_type <= manager->priv->last_ups_warning) {
		gpm_debug ("Already notified %i", warning_type);
		return;
	}

	/* As the level is more critical than the last warning, save it */
	manager->priv->last_ups_warning = warning_type;

	if (warning_type == GPM_WARNING_ACTION) {
		const char *action;

		if (! gpm_manager_is_policy_timout_valid (manager, "critical action")) {
			return;
		}

		/* we have to do different warnings depending on the policy */
		action = gconf_client_get_string (manager->priv->gconf_client,
						  GPM_PREF_UPS_CRITICAL, NULL);

		/* FIXME: we should probably convert to an ENUM type, and use that */
		if (strcmp (action, ACTION_NOTHING) == 0) {
			message = _("The UPS is below the critical level and "
				    "this computer will <b>power-off</b> when the "
				    "UPS becomes completely empty.");

		} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
			message = _("The UPS is below the critical level and "
				    "this computer is about to hibernate.");

		} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
			message = _("The UPS is below the critical level and "
				    "this computer is about to shutdown.");
		}

		/* TODO: need to add 10 second delay so we get notification */
		manager_policy_do (manager, GPM_PREF_UPS_CRITICAL,
				   _("UPS is critically low"));

	} else if (warning_type == GPM_WARNING_DISCHARGING) {
		message = g_strdup_printf (_("Your system is running on backup power!"));

	} else {
		remaining = gpm_get_timestring (battery_status->remaining_time);
		message = g_strdup_printf (_("You have approximately <b>%s</b> "
					     "of remaining UPS power (%d%%). "
					     "Restore power to your computer to "
					     "avoid losing data."),
					   remaining, battery_status->percentage_charge);
		g_free (remaining);
	}

	/* If we had a message, print it as a notification */
	if (message) {
		const char *icon;
		title = battery_low_get_title (warning_type);
		icon = gpm_power_get_icon_from_status (battery_status, battery_kind);
		gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
				      title, message, GPM_NOTIFY_TIMEOUT_LONG,
				      icon, GPM_NOTIFY_URGENCY_NORMAL);
		gpm_info_event_log (manager->priv->info,
				    GPM_GRAPH_EVENT_NOTIFICATION,
				    title);
		g_free (message);
	}
}

/**
 * battery_status_changed_misc:
 * @manager: This manager class instance
 * @battery_kind: The battery kind, e.g. GPM_POWER_KIND_MOUSE
 * @battery_status: The battery status information
 *
 * Hander for the battery status changed event for misc devices such as MOUSE
 * KEYBOARD or PDA. We only do warning notifications here as the devices
 * are not critical to the system power state.
 **/
static void
battery_status_changed_misc (GpmManager	    	   *manager,
			     GpmPowerKind    battery_kind,
			     GpmPowerStatus *battery_status)
{
	GpmWarning warning_type;
	char *message = NULL;
	const char *title = NULL;
	const char *name;
	const char *icon;

	/* mouse, keyboard and PDA have no time, just percentage */
	warning_type = gpm_manager_get_warning_type (manager, battery_status, FALSE);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE ||
	    warning_type == GPM_WARNING_DISCHARGING) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (battery_kind == GPM_POWER_KIND_MOUSE) {
		if (warning_type > manager->priv->last_mouse_warning) {
			manager->priv->last_mouse_warning = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	} else if (battery_kind == GPM_POWER_KIND_KEYBOARD) {
		if (warning_type > manager->priv->last_keyboard_warning) {
			manager->priv->last_keyboard_warning = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	} else if (battery_kind == GPM_POWER_KIND_PDA) {
		if (warning_type > manager->priv->last_pda_warning) {
			manager->priv->last_pda_warning = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	}

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning of type");
		return;
	}

	manager->priv->last_ups_warning = warning_type;
	name = gpm_power_kind_to_localised_string (battery_kind);

	title = battery_low_get_title (warning_type);

	message = g_strdup_printf (_("The %s device attached to this computer "
				     "is low in power (%d%%). "
				     "This device will soon stop functioning "
				     "if not charged."),
				   name, battery_status->percentage_charge);

	icon = gpm_power_get_icon_from_status (battery_status, battery_kind);
	gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
			      title,
			      message,
			      GPM_NOTIFY_TIMEOUT_LONG,
			      icon,
			      GPM_NOTIFY_URGENCY_NORMAL);
	gpm_info_event_log (manager->priv->info, GPM_GRAPH_EVENT_NOTIFICATION, title);

	g_free (message);
}

/**
 * power_battery_status_changed_cb:
 * @power: The power class instance
 * @battery_kind: The battery kind, e.g. GPM_POWER_KIND_MOUSE
 * @manager: This manager class instance
 *
 * This function splits up the battery status changed callback, and calls
 * different functions for each of the device types.
 **/
static void
power_battery_status_changed_cb (GpmPower		*power,
				 GpmPowerKind	 battery_kind,
				 GpmManager		*manager)
{
	GpmPowerStatus battery_status;

	tray_icon_update (manager);

	gpm_power_get_battery_status (manager->priv->power, battery_kind, &battery_status);

	if (battery_kind == GPM_POWER_KIND_PRIMARY) {

		/* PRIMARY is special as there is lots of twisted logic */
		battery_status_changed_primary (manager, battery_kind, &battery_status);

	} else if (battery_kind == GPM_POWER_KIND_UPS) {

		/* UPS is special as it's being used on desktops */
		battery_status_changed_ups (manager, battery_kind, &battery_status);

	} else if (battery_kind == GPM_POWER_KIND_MOUSE ||
		   battery_kind == GPM_POWER_KIND_KEYBOARD ||
		   battery_kind == GPM_POWER_KIND_PDA) {

		/* MOUSE, KEYBOARD, and PDA only do low power warnings */
		battery_status_changed_misc (manager, battery_kind, &battery_status);
	}
}

/**
 * gpm_manager_class_init:
 * @klass: The GpmManagerClass
 **/
static void
gpm_manager_class_init (GpmManagerClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = gpm_manager_finalize;

	signals [ON_AC_CHANGED] =
		g_signal_new ("on-ac-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmManagerClass, on_ac_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_BOOLEAN);
	signals [DPMS_MODE_CHANGED] =
		g_signal_new ("dpms-mode-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmManagerClass, dpms_mode_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GpmManagerPrivate));
}

/**
 * gconf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
gconf_key_changed_cb (GConfClient *client,
		      guint	   cnxn_id,
		      GConfEntry  *entry,
		      gpointer	   user_data)
{
	gint	    value = 0;
	gint	    brightness;
	GpmManager *manager = GPM_MANAGER (user_data);
	gboolean    on_ac;
	gboolean    enabled;
	gboolean    allowed_in_menu;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	gpm_debug ("Key changed %s", entry->key);

	if (gconf_entry_get_value (entry) == NULL) {
		return;
	}

	if (strcmp (entry->key, GPM_PREF_ICON_POLICY) == 0) {

		tray_icon_update (manager);

	} else if (strcmp (entry->key, GPM_PREF_BATTERY_SLEEP_COMPUTER) == 0) {
		/* set new suspend timeouts */
		value = gconf_client_get_int (client, entry->key, NULL);

		if (! on_ac) {
			gpm_idle_set_system_timeout (manager->priv->idle, value);
		}

	} else if (strcmp (entry->key, GPM_PREF_AC_SLEEP_COMPUTER) == 0) {

		/* set new suspend timeouts */
		value = gconf_client_get_int (client, entry->key, NULL);

		if (on_ac) {
			gpm_idle_set_system_timeout (manager->priv->idle,
						     value);
		}

	} else if (strcmp (entry->key, GPM_PREF_BATTERY_SLEEP_DISPLAY) == 0) {

		sync_dpms_policy (manager);

	} else if (strcmp (entry->key, GPM_PREF_AC_SLEEP_DISPLAY) == 0) {

		sync_dpms_policy (manager);

	} else if (strcmp (entry->key, GPM_PREF_AC_BRIGHTNESS) == 0) {

		if (on_ac) {
			brightness = gconf_client_get_int (client, GPM_PREF_AC_BRIGHTNESS, NULL);
			gpm_brightness_level_set (manager->priv->brightness, brightness);
		}

	} else if (strcmp (entry->key, GPM_PREF_BATTERY_BRIGHTNESS) == 0) {

		if (! on_ac) {
			brightness = gconf_client_get_int (client, GPM_PREF_BATTERY_BRIGHTNESS, NULL);
			gpm_brightness_level_set (manager->priv->brightness, brightness);
		}

	} else if (strcmp (entry->key, GPM_PREF_CAN_SUSPEND) == 0) {
		gpm_manager_can_suspend (manager, &enabled, NULL);
		allowed_in_menu = gconf_client_get_bool (manager->priv->gconf_client,
							 GPM_PREF_SHOW_ACTIONS_IN_MENU, NULL);
		gpm_tray_icon_enable_suspend (GPM_TRAY_ICON (manager->priv->tray_icon),
					      allowed_in_menu && enabled);

	} else if (strcmp (entry->key, GPM_PREF_CAN_HIBERNATE) == 0) {
		gpm_manager_can_hibernate (manager, &enabled, NULL);
		allowed_in_menu = gconf_client_get_bool (manager->priv->gconf_client,
							 GPM_PREF_SHOW_ACTIONS_IN_MENU, NULL);
		gpm_tray_icon_enable_hibernate (GPM_TRAY_ICON (manager->priv->tray_icon),
						allowed_in_menu && enabled);

	} else if (strcmp (entry->key, GPM_PREF_SHOW_ACTIONS_IN_MENU) == 0) {
		allowed_in_menu = gconf_client_get_bool (manager->priv->gconf_client,
							 GPM_PREF_SHOW_ACTIONS_IN_MENU, NULL);
		gpm_manager_can_suspend (manager, &enabled, NULL);
		gpm_tray_icon_enable_suspend (GPM_TRAY_ICON (manager->priv->tray_icon),
					      allowed_in_menu && enabled);
		gpm_manager_can_hibernate (manager, &enabled, NULL);
		gpm_tray_icon_enable_hibernate (GPM_TRAY_ICON (manager->priv->tray_icon),
						allowed_in_menu && enabled);

	} else if (strcmp (entry->key, GPM_PREF_POLICY_TIMEOUT) == 0) {
		manager->priv->suppress_policy_timeout =
			gconf_client_get_int (manager->priv->gconf_client,
				  	      GPM_PREF_POLICY_TIMEOUT, NULL);

	} else if (strcmp (entry->key, GPM_PREF_PANEL_DIM_BRIGHTNESS) == 0) {
		manager->priv->lcd_dim_brightness =
			gconf_client_get_int (manager->priv->gconf_client,
					      GPM_PREF_PANEL_DIM_BRIGHTNESS, NULL);

	}
}

/**
 * gpm_manager_tray_icon_hibernate:
 * @manager: This manager class instance
 * @tray: The tray object
 *
 * The icon tray hibernate callback, which only should happen if both policy and
 * the inhibit states are valid.
 **/
static void
gpm_manager_tray_icon_hibernate (GpmManager   *manager,
				 GpmTrayIcon  *tray)
{
	if (! gpm_manager_is_policy_timout_valid (manager, "hibernate signal")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "hibernate")) {
		return;
	}

	manager_explain_reason (manager,
				GPM_GRAPH_EVENT_HIBERNATE,
				_("Hibernating computer"),
				_("user clicked hibernate from tray menu"));
	gpm_manager_hibernate (manager, NULL);
}

/**
 * gpm_manager_tray_icon_suspend:
 * @manager: This manager class instance
 * @tray: The tray object
 *
 * The icon tray suspend callback, which only should happen if both policy and
 * the inhibit states are valid.
 **/
static void
gpm_manager_tray_icon_suspend (GpmManager   *manager,
			       GpmTrayIcon  *tray)
{
	if (! gpm_manager_is_policy_timout_valid (manager, "suspend signal")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "suspend")) {
		return;
	}
	manager_explain_reason (manager,
				GPM_GRAPH_EVENT_HIBERNATE,
				_("Susspending computer"),
				_("user clicked suspend from tray menu"));
	gpm_manager_suspend (manager, NULL);
}

/**
 * hal_battery_removed_cb:
 * @monitor: The monitor class
 * @udi: The HAL udi of the device that was removed
 * @manager: This manager class instance
 **/
static void
hal_battery_removed_cb (GpmHalMonitor *monitor,
			const char    *udi,
			GpmManager    *manager)
{
	gpm_debug ("Battery Removed: %s", udi);
	tray_icon_update (manager);
}

/**
 * gpm_manager_tray_icon_show_info:
 * @manager: This manager class instance
 * @tray: The tray object
 **/
static void
gpm_manager_tray_icon_show_info (GpmManager   *manager,
				 GpmTrayIcon  *tray)
{
	gpm_debug ("Received show-info signal from tray icon");
	gpm_info_show_window (manager->priv->info);
}

/**
 * gpm_manager_init:
 * @manager: This manager class instance
 **/
static void
tray_icon_destroyed (GtkObject *object, gpointer user_data)
{
	GpmManager *manager = user_data;

	manager->priv->tray_icon = gpm_tray_icon_new ();
}

static void
screensaver_connection_changed_cb (GpmScreensaver *screensaver,
				   gboolean        connected,
				   GpmManager     *manager)
{
	/* add throttlers when first connected */
	if (connected) {
		gboolean    on_ac;
		GpmDpmsMode mode;

		gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);
		gpm_dpms_get_mode (manager->priv->dpms, &mode, NULL);

		update_ac_throttle (manager, on_ac);
		update_dpms_throttle (manager, mode);
		update_lid_throttle (manager, manager->priv->lid_is_closed);
	}
}

static void
gpm_manager_init (GpmManager *manager)
{
	gboolean on_ac;
	gboolean use_time;
	gboolean check_type_cpu;
	gboolean enabled;
	gboolean allowed_in_menu;

	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);

	manager->priv->gconf_client = gconf_client_get_default ();

	manager->priv->power = gpm_power_new ();
	g_signal_connect (manager->priv->power, "button-pressed",
			  G_CALLBACK (power_button_pressed_cb), manager);
	g_signal_connect (manager->priv->power, "ac-power-changed",
			  G_CALLBACK (power_on_ac_changed_cb), manager);
	g_signal_connect (manager->priv->power, "battery-status-changed",
			  G_CALLBACK (power_battery_status_changed_cb), manager);

	manager->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (manager->priv->screensaver, "connection-changed",
			  G_CALLBACK (screensaver_connection_changed_cb), manager);

	/* FIXME: We shouldn't assume the lid is open at startup */
	manager->priv->lid_is_closed = FALSE;

	/* we need these to refresh the tooltip and icon */
	g_signal_connect (manager->priv->power, "battery-removed",
			  G_CALLBACK (hal_battery_removed_cb), manager);

	gconf_client_add_dir (manager->priv->gconf_client,
			      GPM_PREF_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);

	gconf_client_notify_add (manager->priv->gconf_client,
				 GPM_PREF_DIR,
				 gconf_key_changed_cb,
				 manager,
				 NULL,
				 NULL);

	manager->priv->brightness = gpm_brightness_new ();
	g_signal_connect (manager->priv->brightness, "brightness-step-changed",
			  G_CALLBACK (brightness_step_changed_cb), manager);

	manager->priv->feedback = gpm_feedback_new ();

	manager->priv->idle = gpm_idle_new ();
	g_signal_connect (manager->priv->idle, "changed",
			  G_CALLBACK (idle_changed_cb), manager);

	/* set up the check_type_cpu, so we can disable the CPU load check */
	check_type_cpu = gconf_client_get_bool (manager->priv->gconf_client,
						GPM_PREF_IDLE_CHECK_CPU,
						NULL);
	gpm_idle_set_check_cpu (manager->priv->idle, check_type_cpu);

	manager->priv->dpms = gpm_dpms_new ();

	/* use a class to handle the complex stuff */
	manager->priv->inhibit = gpm_inhibit_new ();

	gpm_debug ("creating new tray icon");
	manager->priv->tray_icon = gpm_tray_icon_new ();
	g_signal_connect (G_OBJECT (manager->priv->tray_icon), "destroy",
			  G_CALLBACK (tray_icon_destroyed), manager);

	gpm_debug ("initialising info infrastructure");
	manager->priv->info = gpm_info_new ();
	/* logging system needs access to the power stuff... bit of a bodge */
	gpm_info_set_power (manager->priv->info, manager->priv->power);

	/* only show the suspend and hibernate icons if we can do the action,
	   and the policy allows the actions in the menu */
	allowed_in_menu = gconf_client_get_bool (manager->priv->gconf_client,
						 GPM_PREF_SHOW_ACTIONS_IN_MENU, NULL);
	gpm_manager_can_suspend (manager, &enabled, NULL);
	gpm_tray_icon_enable_suspend (GPM_TRAY_ICON (manager->priv->tray_icon),
				      enabled && allowed_in_menu);
	gpm_manager_can_hibernate (manager, &enabled, NULL);
	gpm_tray_icon_enable_hibernate (GPM_TRAY_ICON (manager->priv->tray_icon),
				      enabled && allowed_in_menu);

	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "suspend",
				 G_CALLBACK (gpm_manager_tray_icon_suspend),
				 manager,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "hibernate",
				 G_CALLBACK (gpm_manager_tray_icon_hibernate),
				 manager,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "show-info",
				 G_CALLBACK (gpm_manager_tray_icon_show_info),
				 manager,
				 G_CONNECT_SWAPPED);

	/* coldplug so we are in the correct state at startup */
	sync_dpms_policy (manager);
	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);
	change_power_policy (manager, on_ac);

	g_signal_connect (manager->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), manager);

	/* we want all notifications */
	manager->priv->last_ups_warning = GPM_WARNING_NONE;
	manager->priv->last_mouse_warning = GPM_WARNING_NONE;
	manager->priv->last_keyboard_warning = GPM_WARNING_NONE;
	manager->priv->last_pda_warning = GPM_WARNING_NONE;
	manager->priv->last_primary_warning = GPM_WARNING_NONE;
	manager->priv->last_primary_percentage_change = 0;
	manager->priv->done_notify_fully_charged = FALSE;

	/* We don't want to be notified on coldplug if we are on battery power
	   this should fix #332322 */
	if (! on_ac) {
		manager->priv->last_primary_warning = GPM_WARNING_DISCHARGING;
	}

	/* We can disable this if the ACPI BIOS is fucked, and the
	   time_remaining is therefore inaccurate or just plain wrong. */
	use_time = gconf_client_get_bool (manager->priv->gconf_client,
					  GPM_PREF_USE_TIME_POLICY, NULL);
	manager->priv->use_time_to_notify = use_time;
	if (use_time) {
		gpm_debug ("Using per-time notification policy");
	} else {
		gpm_debug ("Using percentage notification policy");
	}

	manager->priv->suppress_policy_timeout =
		gconf_client_get_int (manager->priv->gconf_client,
				      GPM_PREF_POLICY_TIMEOUT, NULL);
	gpm_debug ("Using a supressed policy timeout of %i seconds",
		   manager->priv->suppress_policy_timeout);

	/* Pretend we just resumed when we start to let actions settle */
	manager->priv->last_resume_event = time (NULL);

	/* get percentage policy */
	manager->priv->low_percentage = gconf_client_get_int (manager->priv->gconf_client,
							      GPM_PREF_LOW_PERCENTAGE, NULL);
	manager->priv->very_low_percentage = gconf_client_get_int (manager->priv->gconf_client,
								   GPM_PREF_VERY_LOW_PERCENTAGE, NULL);
	manager->priv->critical_percentage = gconf_client_get_int (manager->priv->gconf_client,
								   GPM_PREF_CRITICAL_PERCENTAGE, NULL);
	manager->priv->action_percentage = gconf_client_get_int (manager->priv->gconf_client,
								 GPM_PREF_ACTION_PERCENTAGE, NULL);

	/* can remove when we next release, until then, assume we are morons */
	if (manager->priv->low_percentage == 0) {
		gpm_critical_error ("GConf schema installer error, "
				    "battery_low_percentage cannot be zero");
	}

	/* get time policy */
	manager->priv->low_time = gconf_client_get_int (manager->priv->gconf_client,
							GPM_PREF_LOW_TIME, NULL);
	manager->priv->very_low_time = gconf_client_get_int (manager->priv->gconf_client,
							     GPM_PREF_VERY_LOW_TIME, NULL);
	manager->priv->critical_time = gconf_client_get_int (manager->priv->gconf_client,
							     GPM_PREF_CRITICAL_TIME, NULL);
	manager->priv->action_time = gconf_client_get_int (manager->priv->gconf_client,
							   GPM_PREF_ACTION_TIME, NULL);

	/* Get dim settings */
	manager->priv->lcd_dim_brightness = gconf_client_get_int (manager->priv->gconf_client,
								  GPM_PREF_PANEL_DIM_BRIGHTNESS, NULL);
}

/**
 * gpm_manager_finalize:
 * @object: The object to finalize
 *
 * Finalise the manager, by unref'ing all the depending modules.
 **/
static void
gpm_manager_finalize (GObject *object)
{
	GpmManager *manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_MANAGER (object));

	manager = GPM_MANAGER (object);

	g_return_if_fail (manager->priv != NULL);

	g_object_unref (manager->priv->gconf_client);
	g_object_unref (manager->priv->dpms);
	g_object_unref (manager->priv->idle);
	g_object_unref (manager->priv->info);
	g_object_unref (manager->priv->power);
	g_object_unref (manager->priv->brightness);
	g_object_unref (manager->priv->tray_icon);
	g_object_unref (manager->priv->feedback);
	g_object_unref (manager->priv->inhibit);
	g_object_unref (manager->priv->screensaver);

	G_OBJECT_CLASS (gpm_manager_parent_class)->finalize (object);
}

/**
 * gpm_manager_new:
 *
 * Return value: a new GpmManager object.
 **/
GpmManager *
gpm_manager_new (void)
{
	GpmManager *manager;

	manager = g_object_new (GPM_TYPE_MANAGER, NULL);

	return GPM_MANAGER (manager);
}
