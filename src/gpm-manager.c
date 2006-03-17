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
#include "gpm-power.h"
#include "gpm-hal-monitor.h"
#include "gpm-brightness.h"
#include "gpm-tray-icon.h"
#include "gpm-inhibit.h"
#include "gpm-stock-icons.h"
#include "gpm-manager.h"

static void     gpm_manager_class_init (GpmManagerClass *klass);
static void     gpm_manager_init	(GpmManager      *manager);
static void     gpm_manager_finalize   (GObject	  *object);

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
#define GPM_BUTTON_BRIGHT_UP_DEP	"brightness_up"		/* Remove when we depend on HAL 0.5.8 */
#define GPM_BUTTON_BRIGHT_DOWN_DEP	"brightness_down"	/* as these are the old names */
#define GPM_BUTTON_LOCK			"lock"
/* Using www until we get a better one defined for us by the kernel */
#define GPM_BUTTON_BATTERY		"www"

#define GPM_NOTIFY_TIMEOUT_LONG		20	/* seconds */
#define GPM_NOTIFY_TIMEOUT_SHORT	5	/* seconds */

struct GpmManagerPrivate
{
	GConfClient	*gconf_client;

	GpmDpms		*dpms;
	GpmIdle		*idle;
	GpmInfo		*info;
	GpmPower	*power;
	GpmBrightness   *brightness;
	GpmInhibit	*inhibit;

	GpmTrayIcon	*tray_icon;

	GpmWarning	 last_primary_warning;
	GpmWarning	 last_ups_warning;
	GpmWarning	 last_mouse_warning;
	GpmWarning	 last_keyboard_warning;
	GpmWarning	 last_pda_warning;

	gint		 last_primary_percentage_change;

	gboolean	 use_time_to_notify;
	gboolean	 lid_is_closed;

	time_t           last_resume_event;
	int		 suppress_policy_timeout;

	const char	*reason;
};

enum {
	PROP_0
};

enum {
	ON_AC_CHANGED,
	DPMS_MODE_CHANGED,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
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

#define		BATTERY_LOW_PERCENTAGE			(10)	  /* 10 percent */
#define		BATTERY_VERY_LOW_PERCENTAGE		(5)	  /* 5 percent  */
#define		BATTERY_CRITICAL_PERCENTAGE		(2)	  /* 2 percent  */
#define		BATTERY_ACTION_PERCENTAGE		(1)	  /* 1 percent  */

#define		BATTERY_LOW_REMAINING_TIME		(20 * 60) /* 20 minutes */
#define		BATTERY_VERY_LOW_REMAINING_TIME		(10 * 60) /* 10 minutes */
#define		BATTERY_CRITICAL_REMAINING_TIME		(5 * 60)  /* 5 minutes  */
#define		BATTERY_ACTION_REMAINING_TIME		(2 * 60)  /* 2 minutes  */

#define		LAPTOP_PANEL_DIM_BRIGHTNESS		30 /* % */

#undef DISABLE_ACTIONS_FOR_TESTING
/*#define DISABLE_ACTIONS_FOR_TESTING 1*/

/* prototypes */
static gboolean gpm_manager_suspend (GpmManager *manager, GError **error);
static gboolean gpm_manager_hibernate (GpmManager *manager, GError **error);
static gboolean gpm_manager_shutdown (GpmManager *manager, GError **error);

GQuark
gpm_manager_error_quark (void)
{
	static GQuark quark = 0;

	if (!quark) {
		quark = g_quark_from_static_string ("gpm_manager_error");
	}

	return quark;
}

/* Returns if an action is valid, i.e. if the difference in time between this
   request for an action, and the last action completing is larger than the
   timeout set in gconf. This should fix lots of ACPI bugs we are having. */
static gboolean
gpm_manager_is_policy_timout_valid (GpmManager *manager)
{
	if ((time (NULL) - manager->priv->last_resume_event) <=
	    manager->priv->suppress_policy_timeout) {
		return FALSE;
	}
	return TRUE;
}

gboolean
gpm_manager_can_suspend (GpmManager *manager,
			 gboolean   *can,
			 GError    **error)
{
	gboolean gconf_policy;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;

#ifdef DISABLE_ACTIONS_FOR_TESTING
	gpm_debug ("Suspend disabled for testing");
	return TRUE;
#endif

	gconf_policy = gconf_client_get_bool (manager->priv->gconf_client,
					      GPM_PREF_CAN_SUSPEND, NULL);
	if ( gconf_policy && gpm_hal_can_suspend () ) {
		*can = TRUE;
	}

	return TRUE;
}

gboolean
gpm_manager_can_hibernate (GpmManager *manager,
			   gboolean   *can,
			   GError    **error)
{
	gboolean gconf_policy;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;

#ifdef DISABLE_ACTIONS_FOR_TESTING
	gpm_debug ("Hibernate disabled for testing");
	return TRUE;
#endif

	gconf_policy = gconf_client_get_bool (manager->priv->gconf_client,
					      GPM_PREF_CAN_HIBERNATE, NULL);
	if ( gconf_policy && gpm_hal_can_hibernate () ) {
		*can = TRUE;
	}

	return TRUE;
}

gboolean
gpm_manager_can_shutdown (GpmManager *manager,
			  gboolean   *can,
			  GError    **error)
{
	if (can) {
		*can = FALSE;
	}

#ifdef DISABLE_ACTIONS_FOR_TESTING
	gpm_debug ("Shutdown disabled for testing");
	return TRUE;
#endif

	/* FIXME: check other stuff */

	if (can) {
		*can = TRUE;
	}

	return TRUE;
}

/* Return index value dependending on percent
	00-10  = 000
	10-30  = 020
	30-50  = 040
	50-70  = 060
	70-90  = 080
	90-100 = 100
*/
static char *
get_icon_index_from_percent (gint percent)
{
	gpm_debug ("percent = %i", percent);
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

/* required, as UPS and primary icons have charged, charging and discharging icons.
   must free retval */
static char *
get_stock_id_helper (GpmPowerBatteryStatus *device_status, const char *prefix)
{
	char *index;
	char *filename = NULL;

	if (gpm_power_battery_is_charged (device_status)) {

		filename = g_strdup_printf ("%s-charged", prefix);

	} else if (device_status->is_charging) {

		index = get_icon_index_from_percent (device_status->percentage_charge);
		filename = g_strdup_printf ("%s-charging-%s", prefix, index);

	} else if (device_status->is_discharging) {

		index = get_icon_index_from_percent (device_status->percentage_charge);
		filename = g_strdup_printf ("%s-discharging-%s", prefix, index);

	} else {

		/* We have a broken battery, not sure what to display here */
		gpm_debug ("BROKEN BATTERY... Not sure what to do");
		filename = g_strdup_printf ("%s-charging-000", prefix);
	}
	gpm_debug ("filename = %s", filename);
	return filename;
}

/* must free retval */
static char *
get_stock_id (GpmManager *manager,
	      int	  icon_policy)
{
	GpmPowerBatteryStatus status_primary;
	GpmPowerBatteryStatus status_ups;
	GpmPowerBatteryStatus status_mouse;
	GpmPowerBatteryStatus status_keyboard;
	gboolean on_ac;
	gboolean present;

	gpm_debug ("Getting stock icon for tray");

	if (icon_policy == GPM_ICON_POLICY_NEVER) {
		gpm_debug ("The key " GPM_PREF_ICON_POLICY
			   " is set to never, so no icon will be displayed.\n"
			   "You can change this using gnome-power-preferences");
		return NULL;
	}

	/* Finds if a device was found in the cache AND that it is present */
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_BATTERY_KIND_PRIMARY,
						&status_primary);
	status_primary.is_present &= present;
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_BATTERY_KIND_UPS,
						&status_ups);
	status_ups.is_present &= present;
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_BATTERY_KIND_MOUSE,
						&status_mouse);
	status_mouse.is_present &= present;
	present = gpm_power_get_battery_status (manager->priv->power,
						GPM_POWER_BATTERY_KIND_KEYBOARD,
						&status_keyboard);
	status_keyboard.is_present &= present;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	/* we try CRITICAL: PRIMARY, UPS, MOUSE, KEYBOARD */
	gpm_debug ("Trying CRITICAL: primary, ups, mouse, keyboard");
	if (status_primary.is_present &&
	    status_primary.percentage_charge < BATTERY_LOW_PERCENTAGE) {
		return get_stock_id_helper (&status_primary, ICON_PREFIX_PRIMARY);

	} else if (status_ups.is_present &&
		   status_ups.percentage_charge < BATTERY_LOW_PERCENTAGE) {
		return get_stock_id_helper (&status_ups, ICON_PREFIX_UPS);

	} else if (status_mouse.is_present &&
		   status_mouse.percentage_charge < BATTERY_LOW_PERCENTAGE) {
		return g_strdup_printf (GPM_STOCK_MOUSE_LOW);

	} else if (status_keyboard.is_present &&
		   status_keyboard.percentage_charge < BATTERY_LOW_PERCENTAGE) {
		return g_strdup_printf (GPM_STOCK_KEYBOARD_LOW);
	}

	if (icon_policy == GPM_ICON_POLICY_CRITICAL) {
		gpm_debug ("no devices critical, so no icon will be displayed.");
		return NULL;
	}

	/* we try (DIS)CHARGING: PRIMARY, UPS */
	gpm_debug ("Trying CHARGING: primary, ups");
	if (status_primary.is_present &&
	    (status_primary.is_charging || status_primary.is_discharging) ) {
		return get_stock_id_helper (&status_primary, ICON_PREFIX_PRIMARY);

	} else if (status_ups.is_present &&
		   (status_ups.is_charging || status_ups.is_discharging) ) {
		return get_stock_id_helper (&status_ups, ICON_PREFIX_UPS);
	}

	/* Check if we should just show the icon all the time */
	if (icon_policy == GPM_ICON_POLICY_CHARGE) {
		gpm_debug ("no devices (dis)charging, so no icon will be displayed.");
		return NULL;
	}

	/* we try PRESENT: PRIMARY, UPS */
	gpm_debug ("Trying PRESENT: primary, ups");
	if (status_primary.is_present) {
		return get_stock_id_helper (&status_primary, ICON_PREFIX_PRIMARY);

	} else if (status_ups.is_present) {
		return get_stock_id_helper (&status_ups, ICON_PREFIX_UPS);
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
change_power_policy (GpmManager *manager,
		     gboolean	 on_ac)
{
	int	     brightness;
	int	     sleep_display;
	int	     sleep_computer;
	GConfClient *client;

	client = manager->priv->gconf_client;

	if (on_ac) {
		brightness = gconf_client_get_int (client, GPM_PREF_AC_BRIGHTNESS, NULL);
		sleep_computer = gconf_client_get_int (client, GPM_PREF_AC_SLEEP_COMPUTER, NULL);
		sleep_display = gconf_client_get_int (client, GPM_PREF_AC_SLEEP_DISPLAY, NULL);
	} else {
		brightness = gconf_client_get_int (client, GPM_PREF_BATTERY_BRIGHTNESS, NULL);
		sleep_computer = gconf_client_get_int (client, GPM_PREF_BATTERY_SLEEP_COMPUTER, NULL);
		sleep_display = gconf_client_get_int (client, GPM_PREF_BATTERY_SLEEP_DISPLAY, NULL);
	}

	gpm_brightness_level_dim (manager->priv->brightness, brightness);
	gpm_hal_enable_power_save (!on_ac);

	/*
	 * make sure gnome-screensaver disables screensaving,
	 * and enables monitor shut-off instead when on batteries
	 */
	gpm_screensaver_enable_throttle (!on_ac);

	/* set the new sleep (inactivity) value */
	gpm_idle_set_system_timeout (manager->priv->idle, sleep_computer);
	sync_dpms_policy (manager);
}

static void
maybe_notify_on_ac_changed (GpmManager *manager,
			    gboolean	on_ac)
{
	/* If no tray icon then don't clear */
	if (! manager->priv->tray_icon) {
		return;
	}

	if (on_ac) {
		/* for where we add back the ac_adapter before
		 * the "AC Power unplugged" message times out. */
		gpm_tray_icon_cancel_notify (GPM_TRAY_ICON (manager->priv->tray_icon));
	}
}

static gboolean
manager_do_we_screensave (GpmManager *manager,
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
		do_lock = gpm_screensaver_lock_enabled ();
		gpm_debug ("Using ScreenSaver settings (%i)", do_lock);
	} else {
		do_lock = gconf_client_get_bool (manager->priv->gconf_client,
						 policy, NULL);
		gpm_debug ("Using constom locking settings (%i)", do_lock);
	}
	return do_lock;
}

static gboolean
gpm_manager_blank_screen (GpmManager *manager,
			  GError    **noerror)
{
	gboolean do_lock;
	gboolean ret = TRUE;

	do_lock = manager_do_we_screensave (manager,
					    GPM_PREF_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		gpm_screensaver_lock ();
	}
	/* We give the options to enable DPMS because some laptops do
	 * not turn off the LCD backlight when the lid is closed. 
	 * See http://bugzilla.gnome.org/show_bug.cgi?id=321313 */
	GError     *error = NULL;
	gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_OFF, &error);
	if (error) {
		gpm_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}
	return ret;
}

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

	do_lock = manager_do_we_screensave (manager,
					    GPM_PREF_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		gpm_screensaver_poke ();
	}
	return ret;
}

static void
manager_policy_do (GpmManager *manager,
		   const char *policy)
{
	char     *action;

	gpm_debug ("policy: %s", policy);

	action = gconf_client_get_string (manager->priv->gconf_client, policy, NULL);

	if (! action) {
		return;
	}

	if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
		gpm_debug ("Skipping suppressed policy event");
		return;
	}

	if (strcmp (action, ACTION_NOTHING) == 0) {

		gpm_debug ("*ACTION* Doing nothing");

	} else if (strcmp (action, ACTION_SUSPEND) == 0) {

		gpm_debug ("*ACTION* Suspend");
		gpm_manager_suspend (manager, NULL);

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {

		gpm_debug ("*ACTION* Hibernate");
		gpm_manager_hibernate (manager, NULL);

	} else if (strcmp (action, ACTION_BLANK) == 0) {

		gpm_debug ("*ACTION* Blank");
		gpm_manager_blank_screen (manager, NULL);

	} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {

		gpm_debug ("*ACTION* Shutdown");
		gpm_manager_shutdown (manager, NULL);

	} else if (strcmp (action, ACTION_INTERACTIVE) == 0) {

		gpm_debug ("*ACTION* Interactive");
		/* Log out interactively */
		gnome_client_request_save (gnome_master_client (),
					   GNOME_SAVE_GLOBAL,
					   TRUE, GNOME_INTERACT_ANY, FALSE, TRUE);

	} else {
		gpm_warning ("unknown action %s", action);
	}

	g_free (action);
}

gboolean
gpm_manager_get_on_ac (GpmManager *manager,
			gboolean	  *on_ac,
			GError    **error)
{
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	if (on_ac) {
		gpm_power_get_on_ac (manager->priv->power, on_ac, error);
	}

	return TRUE;
}

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
/***************************************************************/

void
gpm_manager_inhibit_inactive_sleep (GpmManager	*manager,
				    const char	*application,
				    const char	*reason,
				    DBusGMethodInvocation *context,
				    GError    **error)
{
#if (DBUS_VERSION_MAJOR == 0) && (DBUS_VERSION_MINOR < 60)
	const char* connection = ":demo";
#else
	const char* connection = dbus_g_method_get_sender (context);
#endif
	int cookie;
	cookie = gpm_inhibit_add (manager->priv->inhibit, connection, application, reason);
	dbus_g_method_return (context, cookie);
}

void
gpm_manager_allow_inactive_sleep (GpmManager	*manager,
				  int		 cookie,
				  DBusGMethodInvocation *context,
				  GError	**error)
{
#if (DBUS_VERSION_MAJOR == 0) && (DBUS_VERSION_MINOR < 60)
	const char* connection = ":demo";
#else
	const char* connection = dbus_g_method_get_sender (context);
#endif
	gpm_inhibit_remove (manager->priv->inhibit, connection, cookie, FALSE);
	dbus_g_method_return (context);
}

/* we set the reason to be WHY. e.g. "user pressed hibernate button" */
static void
gpm_manager_set_reason (GpmManager *manager,
			const char *reason)
{
	manager->priv->reason = reason;
}

/* we set the reason to be WHAT. e.g. "Hibernate system" */
static void
gpm_manager_log_reason (GpmManager *manager,
			const char *action)
{
	gpm_syslog ("%s because %s", action, manager->priv->reason);
}

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
	gpm_manager_log_reason (manager, "Shutting down computer");
	gpm_hal_shutdown ();
	ret = TRUE;

	return ret;
}

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

	do_lock = manager_do_we_screensave (manager,
					    GPM_PREF_LOCK_ON_HIBERNATE);
	if (do_lock) {
		gpm_screensaver_lock ();
	}

	gpm_networkmanager_sleep ();

	/* FIXME: make this async? */
	gpm_manager_log_reason (manager, "Hibernating computer");
	ret = gpm_hal_hibernate ();
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
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      GPM_NOTIFY_TIMEOUT_LONG,
					      _("Hibernate Problem"),
					      NULL,
					      message);
			g_free (message);
		}
	}

	if (do_lock) {
		gpm_screensaver_poke ();
	}
	gpm_networkmanager_wake ();

	/* save the time that we resumed */
	manager->priv->last_resume_event = time (NULL);

	return ret;
}

static gboolean
gpm_manager_suspend (GpmManager *manager,
		     GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean do_lock;

	gpm_manager_can_suspend (manager, &allowed, NULL);

	if (! allowed) {
		gpm_warning ("Cannot suspend");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot suspend");
		return FALSE;
	}

	do_lock = manager_do_we_screensave (manager,
					    GPM_PREF_LOCK_ON_SUSPEND);
	if (do_lock) {
		gpm_screensaver_lock ();
	}

	gpm_networkmanager_sleep ();

	/* FIXME: make this async? */
	gpm_manager_log_reason (manager, "Suspending computer");
	ret = gpm_hal_suspend (0);
	if (! ret) {
		char *message;
		gboolean show_notify;
		/* We only show the HAL failed notification if set in gconf */
		show_notify = gconf_client_get_bool (manager->priv->gconf_client,
						     GPM_PREF_NOTIFY_HAL_ERROR, NULL);
		if (show_notify) {
			message = g_strdup_printf (_("Your computer failed to %s.\n"
						     "Check the <a href=\"%s\">FAQ page</a> for common problems."),
						     _("suspend"), GPM_FAQ_URL);
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      GPM_NOTIFY_TIMEOUT_LONG,
					      _("Suspend Problem"),
					      NULL,
					      message);
			g_free (message);
		}
	}

	if (do_lock) {
		gpm_screensaver_poke ();
	}
	gpm_networkmanager_wake ();

	/* save the time that we resumed */
	manager->priv->last_resume_event = time (NULL);

	return ret;
}

gboolean
gpm_manager_suspend_dbus_method (GpmManager *manager,
				 GError    **error)
{
	/* FIXME: From where? */
	gpm_manager_set_reason (manager, "the DBUS method Suspend() was invoked");
	return gpm_manager_suspend (manager, error);
}

gboolean
gpm_manager_hibernate_dbus_method (GpmManager *manager,
				   GError    **error)
{
	/* FIXME: From where? */
	gpm_manager_set_reason (manager, "the DBUS method Hibernate() was invoked");
	return gpm_manager_hibernate (manager, error);
}

gboolean
gpm_manager_shutdown_dbus_method (GpmManager *manager,
				  GError    **error)
{
	/* FIXME: From where? */
	gpm_manager_set_reason (manager, "the DBUS method Shutdown() was invoked");
	return gpm_manager_shutdown (manager, error);
}

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
			gpm_brightness_level_resume (manager->priv->brightness);
		}

		/* sync timeouts */
		sync_dpms_policy (manager);

		break;
	case GPM_IDLE_MODE_SESSION:

		gpm_debug ("Idle state changed: SESSION");

		/* activate display power management */
		error = NULL;
		gpm_dpms_set_active (manager->priv->dpms, TRUE, &error);
		if (error) {
			gpm_debug ("Unable to set DPMS active: %s", error->message);
		}

		/* Should we resume the screen? */
		do_laptop_dim = gconf_client_get_bool (manager->priv->gconf_client,
							GPM_PREF_IDLE_DIM_SCREEN, NULL);
		if (do_laptop_dim) {
			/* save this brightness and dim the screen, fixes #328564 */
			gpm_brightness_level_save (manager->priv->brightness,
						   LAPTOP_PANEL_DIM_BRIGHTNESS);
		}

		/* sync timeouts */
		sync_dpms_policy (manager);

		break;
	case GPM_IDLE_MODE_SYSTEM:
		gpm_debug ("Idle state changed: SYSTEM");

		if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
			gpm_debug ("Skipping suppressed timeout action");
			return;
		}
		/* can only be hibernate or suspend */
		gpm_manager_set_reason (manager, "the system state is idle");
		manager_policy_do (manager, GPM_PREF_SLEEP_TYPE);

		break;
	default:
		g_assert_not_reached ();
		break;
	}

}

static void
dpms_mode_changed_cb (GpmDpms    *dpms,
		      GpmDpmsMode mode,
		      GpmManager *manager)
{
	gpm_debug ("DPMS mode changed: %d", mode);
	if (mode == GPM_DPMS_MODE_ON) {
		gboolean on_ac;

		/* only unthrottle if on ac power */
		gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

		if (on_ac) {
			gpm_screensaver_enable_throttle (FALSE);
		}
	} else {
		gpm_screensaver_enable_throttle (TRUE);
	}

	gpm_debug ("emitting dpms-mode-changed : %s", gpm_dpms_mode_to_string (mode));
	g_signal_emit (manager,
			signals [DPMS_MODE_CHANGED],
			0,
			gpm_dpms_mode_to_string (mode));
}

static void
battery_button_pressed (GpmManager *manager)
{
	char *message;

	gpm_power_get_status_summary (manager->priv->power, &message, NULL);

	gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
			      GPM_NOTIFY_TIMEOUT_LONG,
			      _("Power Information"),
			      NULL,
			      message);
	g_free (message);
}

static void
power_button_pressed (GpmManager   *manager,
		      gboolean	    state)
{
	if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
		gpm_debug ("Skipping suppressed power button press");
		return;
	}
	gpm_debug ("power button pressed");
	gpm_manager_set_reason (manager, "the power button has been pressed");
	manager_policy_do (manager, GPM_PREF_BUTTON_POWER);
}

static void
suspend_button_pressed (GpmManager   *manager,
			gboolean      state)
{
	if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
		gpm_debug ("Skipping suppressed suspend button press");
		return;
	}
	gpm_debug ("suspend button pressed");
	gpm_manager_set_reason (manager, "the suspend button has been pressed");
	manager_policy_do (manager, GPM_PREF_BUTTON_SUSPEND);
}

static void
hibernate_button_pressed (GpmManager   *manager,
			gboolean      state)
{
	if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
		gpm_debug ("Skipping suppressed hibernate button press");
		return;
	}
	gpm_debug ("hibernate button pressed");
	gpm_manager_set_reason (manager, "the hibernate button has been pressed");
	manager_policy_do (manager, GPM_PREF_BUTTON_HIBERNATE);
}

static void
lid_button_pressed (GpmManager	 *manager,
		    gboolean	  state)
{
	gboolean  on_ac;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	gpm_debug ("lid button changed: %d", state);

	/* We keep track of the lid state so we can do the 
	   lid close on battery action if the ac_adapter is removed when the laptop
	   is closed. Fixes #331655 */
	manager->priv->lid_is_closed = state;

	if (state) {
		/* Disable the screensaver, as we don't want this starting
		   when the lid is shut */
		gpm_screensaver_inhibit_activation ("gnome-power-manager has "
						    "detected that the lid is closed");
		if (on_ac) {
			gpm_debug ("Performing AC policy");
			gpm_manager_set_reason (manager, "the lid has been closed on ac power");
			manager_policy_do (manager, GPM_PREF_AC_BUTTON_LID);
		} else {
			gpm_debug ("Performing battery policy");
			gpm_manager_set_reason (manager, "the lid has been closed on battery power");
			manager_policy_do (manager, GPM_PREF_BATTERY_BUTTON_LID);
		}
	} else {
		/* re-enable the screensaver */
		gpm_screensaver_allow_activation ();

		/* we turn the lid dpms back on unconditionally */
		gpm_manager_unblank_screen (manager, NULL);
	}
}

static void
power_button_pressed_cb (GpmPower   *power,
			 const char *type,
			 gboolean    state,
			 GpmManager *manager)
{
	gpm_debug ("Button press event type=%s state=%d", type, state);

	/* simulate user input */
	gpm_screensaver_poke ();

	if (strcmp (type, GPM_BUTTON_POWER) == 0) {
		power_button_pressed (manager, state);

	} else if (strcmp (type, GPM_BUTTON_SLEEP) == 0) {
		suspend_button_pressed (manager, state);

	} else if (strcmp (type, GPM_BUTTON_SUSPEND) == 0) {
		suspend_button_pressed (manager, state);

	} else if (strcmp (type, GPM_BUTTON_HIBERNATE) == 0) {
		hibernate_button_pressed (manager, state);

	} else if (strcmp (type, GPM_BUTTON_LID) == 0) {
		lid_button_pressed (manager, state);

	} else if ((strcmp (type, GPM_BUTTON_BRIGHT_UP) == 0) ||
		   (strcmp (type, GPM_BUTTON_BRIGHT_UP_DEP) == 0)) {
		gpm_brightness_level_up (manager->priv->brightness);

	} else if ((strcmp (type, GPM_BUTTON_BRIGHT_DOWN) == 0) ||
		   (strcmp (type, GPM_BUTTON_BRIGHT_DOWN_DEP) == 0)) {
		gpm_brightness_level_down (manager->priv->brightness);

	} else if (strcmp (type, GPM_BUTTON_LOCK) == 0) {
		gpm_screensaver_lock ();

	} else if (strcmp (type, GPM_BUTTON_BATTERY) == 0) {
		battery_button_pressed (manager);
	}
}

static void
power_on_ac_changed_cb (GpmPower   *power,
			gboolean    on_ac,
			GpmManager *manager)
{
	gpm_debug ("Setting on-ac: %d", on_ac);

	/* simulate user input, to fix #333525 */
	gpm_screensaver_poke ();

	/* If we are on AC power we should show warnings again */
	if (on_ac) {
		gpm_debug ("Resetting last_primary_warning to NONE");
		manager->priv->last_primary_warning = GPM_WARNING_NONE;
	}

	tray_icon_update (manager);

	maybe_notify_on_ac_changed (manager, on_ac);
	change_power_policy (manager, on_ac);

	gpm_debug ("emitting on-ac-changed : %i", on_ac);
	g_signal_emit (manager, signals [ON_AC_CHANGED], 0, on_ac);

	/* We do the lid close on battery action if the ac_adapter is removed
	   when the laptop is closed and on battery. Fixes #331655 */
	if (! on_ac && manager->priv->lid_is_closed) {
		gpm_debug ("Doing battery policy when lid closed and power removed");
		gpm_manager_set_reason (manager, "the lid has been closed, and the ac adapter removed");
		manager_policy_do (manager, GPM_PREF_BATTERY_BUTTON_LID);
	}
}

static GpmWarning
gpm_manager_get_warning_type (GpmPowerBatteryStatus *battery_status,
			      gboolean		     use_time)
{
	GpmWarning type = GPM_WARNING_NONE;

	/* some devices (e.g. mice) do not have time, and we have to measure using percent */
	if (use_time) {
		if (battery_status->remaining_time <= 0) {
			type = GPM_WARNING_NONE;
		} else if (battery_status->remaining_time <= BATTERY_ACTION_REMAINING_TIME) {
			type = GPM_WARNING_ACTION;
		} else if (battery_status->remaining_time <= BATTERY_CRITICAL_REMAINING_TIME) {
			type = GPM_WARNING_CRITICAL;
		} else if (battery_status->remaining_time <= BATTERY_VERY_LOW_REMAINING_TIME) {
			type = GPM_WARNING_VERY_LOW;
		} else if (battery_status->remaining_time <= BATTERY_LOW_REMAINING_TIME) {
			type = GPM_WARNING_LOW;
		}
	} else {
		if (battery_status->percentage_charge <= BATTERY_ACTION_PERCENTAGE) {
			type = GPM_WARNING_ACTION;
		} else if (battery_status->percentage_charge <= BATTERY_CRITICAL_PERCENTAGE) {
			type = GPM_WARNING_CRITICAL;
		} else if (battery_status->percentage_charge <= BATTERY_VERY_LOW_PERCENTAGE) {
			type = GPM_WARNING_VERY_LOW;
		} else if (battery_status->percentage_charge <= BATTERY_LOW_PERCENTAGE) {
			type = GPM_WARNING_LOW;
		}
	}

	/* If we have no important warnings, we should test for discharging */
	if (type == GPM_WARNING_NONE) {
		if (battery_status->is_discharging) {
			type = GPM_WARNING_DISCHARGING;
		}
	}
	return type;
}

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

static gboolean
manager_critical_action_do (GpmManager *manager)
{
	manager_policy_do (manager, GPM_PREF_BATTERY_CRITICAL);
	return FALSE;
}

/* performs critical action is required, and displays notifications */
static void
battery_status_changed_primary (GpmManager	      *manager,
				 GpmPowerBatteryKind    battery_kind,
				GpmPowerBatteryStatus *battery_status)
{
	GpmWarning  warning_type;
	gboolean    show_notify;
	char	*message = NULL;
	char	*remaining = NULL;
	const char *title = NULL;
	gboolean    on_ac;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	/* If we are charging we should show warnings again as soon as we discharge again */
	if (battery_status->is_charging) {
		gpm_debug ("Resetting last_primary_warning to NONE");
		manager->priv->last_primary_warning = GPM_WARNING_NONE;
	}

	/* We have to track the last percentage, as when we do the transition
	 * 99 to 100 some laptops report this as charging, some as not-charging.
	 * This is probably a race. This method should work for both cases. */
	if (manager->priv->last_primary_percentage_change == 99 &&
	    battery_status->percentage_charge == 100) {
		show_notify = gconf_client_get_bool (manager->priv->gconf_client,
						     GPM_PREF_NOTIFY_BATTCHARGED, NULL);

		if (show_notify) {
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      GPM_NOTIFY_TIMEOUT_SHORT,
					      _("Battery Charged"),
					      NULL,
					      _("Your battery is now fully charged"));
		}
	}

	manager->priv->last_primary_percentage_change = battery_status->percentage_charge;

	if (! battery_status->is_discharging) {
		gpm_debug ("%s is not discharging", battery_kind_to_string (battery_kind));
		return;
	}

	if (on_ac) {
		gpm_debug ("Computer marked as on_ac.");
		return;
	}

	warning_type = gpm_manager_get_warning_type (battery_status, manager->priv->use_time_to_notify);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning");
		return;
	}

	if (warning_type == GPM_WARNING_ACTION) {
		const char *warning = NULL;
		const char *action;

		if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
			gpm_debug ("Skipping suppressed critical action");
			return;
		}

		/* we have to do different warnings depending on the policy */
		action = gconf_client_get_string (manager->priv->gconf_client,
						  GPM_PREF_BATTERY_CRITICAL, NULL);

		/* FIXME: we should probably convert to an ENUM type, and use that */
		if (strcmp (action, ACTION_NOTHING) == 0) {
			warning = _("The battery is below the critical level and "
				    "this computer will <b>power-off</b> when the "
				    "battery becomes completely empty.");
		} else if (strcmp (action, ACTION_SUSPEND) == 0) {
			warning = _("The battery is below the critical level and "
				    "this computer is about to suspend.<br>"
				    "<b>NOTE:</b> A small amount of power is required "
				    "to keep your computer in a suspended state.");
		} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
			warning = _("The battery is below the critical level and "
				    "this computer is about to hibernate.");
		} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
			warning = _("The battery is below the critical level and "
				    "this computer is about to shutdown.");
		}

		if (warning) {
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      GPM_NOTIFY_TIMEOUT_LONG,
					      _("Critical action"),
					      NULL,
					      warning);
		}
		gpm_manager_set_reason (manager, "we are critically low for the primary battery");
		/* wait 10 seconds for user-panic */
		g_timeout_add (1000*10, (GSourceFunc) manager_critical_action_do, manager);
	}

	/* Always check if we already notified the user */
	if (warning_type > manager->priv->last_primary_warning) {
		int timeout;

		manager->priv->last_primary_warning = warning_type;
		remaining = gpm_get_timestring (battery_status->remaining_time);

		/* Do different warnings for each GPM_WARNING */
		if (warning_type == GPM_WARNING_DISCHARGING) {
			gboolean show_notify;
			show_notify = gconf_client_get_bool (manager->priv->gconf_client,
							     GPM_PREF_NOTIFY_ACADAPTER, NULL);
			if (show_notify) {
				message = g_strdup_printf (_("The AC Power has been unplugged. "
							      "The system is now using battery power."));
				timeout = GPM_NOTIFY_TIMEOUT_SHORT;
			}
		} else {
			message = g_strdup_printf (_("You have approximately <b>%s</b> "
						     "of remaining battery life (%d%%). "
						     "Plug in your AC Adapter to avoid losing data."),
						   remaining, battery_status->percentage_charge);
				timeout = GPM_NOTIFY_TIMEOUT_LONG;
		}
		if (message) {
			title = battery_low_get_title (warning_type);
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      timeout,
					      title,
					      NULL,
					      message);
			g_free (message);
		}
		g_free (remaining);
	} else {
		gpm_debug ("Already notified %i", warning_type);
	}
}

/* displays notifications */
static void
battery_status_changed_ups (GpmManager		   *manager,
			    GpmPowerBatteryKind	   battery_kind,
			    GpmPowerBatteryStatus *battery_status)
{
	GpmWarning warning_type;
	char *message = NULL;
	char *remaining = NULL;
	const char *title = NULL;
	const char *name;

	/* FIXME: UPS should probably do a low power event to save the system */
	/* FIXME: UPS should warn when energised "Your system is running on backup power!" */

	/* If we are charging we should show warnings again as soon as we discharge again */
	if (battery_status->is_charging) {
		manager->priv->last_ups_warning = GPM_WARNING_NONE;
	}

	if (! battery_status->is_discharging) {
		gpm_debug ("%s is not discharging", battery_kind_to_string(battery_kind));
		return;
	}

	warning_type = gpm_manager_get_warning_type (battery_status, manager->priv->use_time_to_notify);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (warning_type > manager->priv->last_ups_warning) {
		manager->priv->last_ups_warning = warning_type;
		name = battery_kind_to_string (battery_kind);

		title = battery_low_get_title (warning_type);
		remaining = gpm_get_timestring (battery_status->remaining_time);

		/* Do different warnings for each GPM_WARNING */
		if (warning_type == GPM_WARNING_DISCHARGING) {
			message = g_strdup_printf (_("Your system is running on backup power!"));

		} else {
			message = g_strdup_printf (_("You have approximately <b>%s</b> "
						     "of remaining UPS power (%d%%). "
						     "Restore power to your computer to "
						     "avoid losing data."),
						   remaining, battery_status->percentage_charge);
		}
		gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
				      GPM_NOTIFY_TIMEOUT_LONG,
				      title,
				      NULL,
				      message);

		g_free (remaining);
		g_free (message);
	}
}

/* displays notifications */
static void
battery_status_changed_misc (GpmManager	    	   *manager,
			     GpmPowerBatteryKind    battery_kind,
			     GpmPowerBatteryStatus *battery_status)
{
	GpmWarning warning_type;
	char *message = NULL;
	const char *title = NULL;
	const char *name;

	/* mouse, keyboard and PDA have no time, just percentage */
	warning_type = gpm_manager_get_warning_type (battery_status, FALSE);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE ||
	    warning_type == GPM_WARNING_DISCHARGING) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (battery_kind == GPM_POWER_BATTERY_KIND_MOUSE) {
		if (warning_type > manager->priv->last_mouse_warning) {
			manager->priv->last_mouse_warning = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	} else if (battery_kind == GPM_POWER_BATTERY_KIND_KEYBOARD) {
		if (warning_type > manager->priv->last_keyboard_warning) {
			manager->priv->last_keyboard_warning = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	} else if (battery_kind == GPM_POWER_BATTERY_KIND_PDA) {
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
	name = battery_kind_to_string (battery_kind);

	title = battery_low_get_title (warning_type);

	message = g_strdup_printf (_("The %s device attached to this computer "
				     "is low in power (%d%%). "
				     "This device will soon stop functioning "
				     "if not charged."),
				   name, battery_status->percentage_charge);

	gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
			      GPM_NOTIFY_TIMEOUT_LONG,
			      title,
			      NULL,
			      message);

	g_free (message);
}

static void
power_battery_status_changed_cb (GpmPower		*power,
				 GpmPowerBatteryKind	battery_kind,
				 GpmManager		*manager)
{
	GpmPowerBatteryStatus battery_status;

	tray_icon_update (manager);

	gpm_power_get_battery_status (manager->priv->power, battery_kind, &battery_status);

	if (battery_kind == GPM_POWER_BATTERY_KIND_PRIMARY) {

		/* PRIMARY is special as there is lots of twisted logic */
		battery_status_changed_primary (manager, battery_kind, &battery_status);

	} else if (battery_kind == GPM_POWER_BATTERY_KIND_UPS) {

		/* UPS is special as it's being used on desktops */
		battery_status_changed_ups (manager, battery_kind, &battery_status);

	} else if (battery_kind == GPM_POWER_BATTERY_KIND_MOUSE ||
		   battery_kind == GPM_POWER_BATTERY_KIND_KEYBOARD ||
		   battery_kind == GPM_POWER_BATTERY_KIND_PDA) {

		/* MOUSE, KEYBOARD, and PDA only do low power warnings */
		battery_status_changed_misc (manager, battery_kind, &battery_status);
	}
}

static void
gpm_manager_set_property (GObject	     *object,
			  guint		      prop_id,
			  const GValue	     *value,
			  GParamSpec	     *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_manager_get_property (GObject	     *object,
			  guint		      prop_id,
			  GValue	     *value,
			  GParamSpec	     *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_manager_class_init (GpmManagerClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize	   = gpm_manager_finalize;
	object_class->get_property = gpm_manager_get_property;
	object_class->set_property = gpm_manager_set_property;

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

static void
callback_gconf_key_changed (GConfClient *client,
			    guint	 cnxn_id,
			    GConfEntry	*entry,
			    gpointer	 user_data)
{
	gint	    value = 0;
	gint	    brightness;
	GpmManager *manager = GPM_MANAGER (user_data);
	gboolean    on_ac;
	gboolean enabled;

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
		gpm_tray_icon_enable_suspend (GPM_TRAY_ICON (manager->priv->tray_icon), enabled);

	} else if (strcmp (entry->key, GPM_PREF_CAN_HIBERNATE) == 0) {
		gpm_manager_can_hibernate (manager, &enabled, NULL);
		gpm_tray_icon_enable_hibernate (GPM_TRAY_ICON (manager->priv->tray_icon), enabled);

	} else if (strcmp (entry->key, GPM_PREF_POLICY_TIMEOUT) == 0) {
		manager->priv->suppress_policy_timeout =
			gconf_client_get_int (manager->priv->gconf_client,
				  	      GPM_PREF_POLICY_TIMEOUT, NULL);
	}
}

#if ACTIONS_MENU_ENABLED
static void
gpm_manager_tray_icon_hibernate (GpmManager   *manager,
				 GpmTrayIcon  *tray)
{
	if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
		gpm_debug ("Skipping suppressed hibernate signal");
		return;
	}
	gpm_debug ("Received hibernate signal from tray icon");
	gpm_manager_set_reason (manager, "user clicked hibernate from tray menu");
	gpm_manager_hibernate (manager, NULL);
}

static void
gpm_manager_tray_icon_suspend (GpmManager   *manager,
				GpmTrayIcon  *tray)
{
	if (gpm_manager_is_policy_timout_valid (manager) == FALSE) {
		gpm_debug ("Skipping suppressed suspend signal");
		return;
	}
	gpm_debug ("Received supend signal from tray icon");
	gpm_manager_set_reason (manager, "user clicked suspend from tray menu");
	gpm_manager_suspend (manager, NULL);
}
#endif

static void
hal_battery_removed_cb (GpmHalMonitor *monitor,
			const char    *udi,
			GpmManager    *manager)
{
	gpm_debug ("Battery Removed: %s", udi);

	tray_icon_update (manager);
}

static void
gpm_manager_tray_icon_show_info (GpmManager   *manager,
				  GpmTrayIcon  *tray)
{
	gpm_debug ("Received show-info signal from tray icon");
	gpm_info_show_window (manager->priv->info);
}

static void
gpm_manager_init (GpmManager *manager)
{
	gboolean on_ac;
	gboolean use_time;
	gboolean check_type_cpu;
#if ACTIONS_MENU_ENABLED
	gboolean enabled;
#endif
	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);

	manager->priv->gconf_client = gconf_client_get_default ();

	manager->priv->power = gpm_power_new ();
	g_signal_connect (manager->priv->power, "button-pressed",
			  G_CALLBACK (power_button_pressed_cb), manager);
	g_signal_connect (manager->priv->power, "ac-power-changed",
			  G_CALLBACK (power_on_ac_changed_cb), manager);
	g_signal_connect (manager->priv->power, "battery-status-changed",
			  G_CALLBACK (power_battery_status_changed_cb), manager);

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
				 callback_gconf_key_changed,
				 manager,
				 NULL,
				 NULL);

	manager->priv->brightness = gpm_brightness_new ();

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

	gpm_debug ("initialising info infrastructure");
	manager->priv->info = gpm_info_new ();
	/* logging system needs access to the power stuff... bit of a bodge */
	gpm_info_set_power (manager->priv->info, manager->priv->power);

#if ACTIONS_MENU_ENABLED
	gpm_manager_can_suspend (manager, &enabled, NULL);
	gpm_tray_icon_enable_suspend (GPM_TRAY_ICON (manager->priv->tray_icon), enabled);
	gpm_manager_can_hibernate (manager, &enabled, NULL);
	gpm_tray_icon_enable_hibernate (GPM_TRAY_ICON (manager->priv->tray_icon), enabled);

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
#endif
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

	/* We don't want to be notified on coldplug if we are on battery power
	   this should fix #332322 */
	if (! on_ac) {
		manager->priv->last_primary_warning = GPM_WARNING_DISCHARGING;
	}

	/* we set the reason to be unknown */
	gpm_manager_set_reason (manager, "of an unknown reason");

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
}

static void
gpm_manager_finalize (GObject *object)
{
	GpmManager *manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_MANAGER (object));

	manager = GPM_MANAGER (object);

	g_return_if_fail (manager->priv != NULL);

	if (manager->priv->gconf_client != NULL) {
		g_object_unref (manager->priv->gconf_client);
	}

	if (manager->priv->dpms != NULL) {
		g_object_unref (manager->priv->dpms);
	}

	if (manager->priv->idle != NULL) {
		g_object_unref (manager->priv->idle);
	}

	if (manager->priv->info != NULL) {
		g_object_unref (manager->priv->info);
	}

	if (manager->priv->power != NULL) {
		g_object_unref (manager->priv->power);
	}

	if (manager->priv->brightness != NULL) {
		g_object_unref (manager->priv->brightness);
	}

	if (manager->priv->tray_icon != NULL) {
		g_object_unref (manager->priv->tray_icon);
	}

	if (manager->priv->inhibit != NULL) {
		g_object_unref (manager->priv->inhibit);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmManager *
gpm_manager_new (void)
{
	GpmManager *manager;

	manager = g_object_new (GPM_TYPE_MANAGER, NULL);

	return GPM_MANAGER (manager);
}
