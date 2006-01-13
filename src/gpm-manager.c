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

#include <libgnomeui/gnome-client.h> /* for gnome_client_request_save */

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-screensaver.h"
#include "gpm-networkmanager.h"

/* FIXME: we should abstract the HAL stuff */
#include "gpm-hal.h"

#include "gpm-dpms.h"
#include "gpm-idle.h"
#include "gpm-power.h"
#include "gpm-tray-icon.h"
#include "gpm-manager.h"

static void     gpm_manager_class_init (GpmManagerClass *klass);
static void     gpm_manager_init       (GpmManager      *manager);
static void     gpm_manager_finalize   (GObject         *object);

static gboolean gpm_manager_setup_tray_icon (GpmManager *manager,
                                             GtkObject  *object);

#define GPM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_MANAGER, GpmManagerPrivate))

struct GpmManagerPrivate
{
	GConfClient	*gconf_client;

	GpmDpms		*dpms;
	GpmIdle		*idle;
	GpmPower	*power;

	GpmTrayIcon	*tray_icon;
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

G_DEFINE_TYPE (GpmManager, gpm_manager, G_TYPE_OBJECT)

#undef DISABLE_ACTIONS_FOR_TESTING
/*#define DISABLE_ACTIONS_FOR_TESTING 1*/

static gboolean
_gpm_manager_can_suspend (GpmManager *manager)
{
	gboolean can;

#ifdef DISABLE_ACTIONS_FOR_TESTING
	g_debug ("Suspend disabled for testing");
	return FALSE;
#endif

	/* FIXME: check other stuff */

	can = gpm_hal_can_suspend ();

	return can;
}

static gboolean
_gpm_manager_can_hibernate (GpmManager *manager)
{
	gboolean can;

#ifdef DISABLE_ACTIONS_FOR_TESTING
	g_debug ("Hibernate disabled for testing");
	return FALSE;
#endif

	/* FIXME: check other stuff */

	can = gpm_hal_can_hibernate ();

	return can;
}

static gboolean
_gpm_manager_can_shutdown (GpmManager *manager)
{
	gboolean can;

#ifdef DISABLE_ACTIONS_FOR_TESTING
	g_debug ("Shutdown disabled for testing");
	return FALSE;
#endif

	/* FIXME: check other stuff */

	can = TRUE;

	return can;
}


/** Finds the icon index value for the percentage charge
 *
 *  @param	percent		The percentage value
 *  @return			A scale 0..8
 */
static gint
get_icon_index_from_percent (gint percent)
{
	const gint NUM_INDEX = 8;
	gint	   index;

	index = ((percent + NUM_INDEX / 2) * NUM_INDEX ) / 100;
	if (index < 0)
		return 0;
	else if (index > NUM_INDEX)
		return NUM_INDEX;

	return index;
}

/** Gets an icon name for the object
 *
 *  @return			An icon name
 */
static char *
get_stock_id (GpmManager *manager,
	      char	 *icon_policy)
{
	gboolean res;
	gboolean has_primary;
	int	 index;
	int	 low_threshold;
	int      primary_percentage;
	int      percentage;
	gboolean primary_charging;
	gboolean primary_discharging;
	gboolean on_ac;

	g_return_val_if_fail (icon_policy != NULL, NULL);

	g_debug ("Getting stock icon for tray");

	if (strcmp (icon_policy, ICON_POLICY_NEVER) == 0) {
		g_debug ("The key " GPM_PREF_ICON_POLICY
			 " is set to never, so no icon will be displayed.\n"
			 "You can change this using gnome-power-preferences");
		return NULL;
	}

	/* find out when the user considers the power "low" */
	low_threshold = gconf_client_get_int (manager->priv->gconf_client, GPM_PREF_THRESHOLD_LOW, NULL);

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	has_primary = gpm_power_get_battery_percentage (manager->priv->power,
							"primary",
							&primary_percentage,
							NULL);
	if (has_primary && primary_percentage < low_threshold) {
		index = get_icon_index_from_percent (primary_percentage);

		if (on_ac) {
			return g_strdup_printf ("gnome-power-ac-%d-of-8", index);
		} else {
			return g_strdup_printf ("gnome-power-bat-%d-of-8", index);
		}
	}

	res = gpm_power_get_battery_percentage (manager->priv->power,
						"ups",
						&percentage,
						NULL);
	if (res && percentage < low_threshold) {
		index = get_icon_index_from_percent (percentage);

		return g_strdup_printf ("gnome-power-ups-%d-of-8", index);
	}

	res = gpm_power_get_battery_percentage (manager->priv->power,
						"mouse",
						&percentage,
						NULL);
	if (res && percentage < low_threshold) {
		return g_strdup_printf ("gnome-power-mouse");
	}

	res = gpm_power_get_battery_percentage (manager->priv->power,
						"keyboard",
						&percentage,
						NULL);
	if (res && percentage < low_threshold) {
		return g_strdup_printf ("gnome-power-keyboard");
	}

	/*
	 * Check if we should just show the charging / discharging icon 
	 * even when not low or critical.
	 */
	if ((strcmp (icon_policy, ICON_POLICY_CRITICAL) == 0)) {
		g_debug ("get_stock_id: no devices critical, so "
			 "no icon will be displayed.");
		return NULL;
	}

	/* Only display if charging or disharging */
	primary_charging = FALSE;
	primary_discharging = FALSE;
	if (has_primary) {
		res = gpm_power_get_battery_charging (manager->priv->power,
						      "primary",
						      &primary_charging,
						      &primary_discharging,
						      NULL);
		if (primary_charging || primary_discharging) {
			index = get_icon_index_from_percent (primary_percentage);
			if (on_ac) {
				return g_strdup_printf ("gnome-power-ac-%d-of-8", index);
			} else {
				return g_strdup_printf ("gnome-power-bat-%d-of-8", index);
			}
		}
	}

	/* Check if we should just show the icon all the time */
	if (strcmp (icon_policy, ICON_POLICY_CHARGE) == 0) {
		g_debug ("get_stock_id: no devices (dis)charging, so "
			 "no icon will be displayed.");
		return NULL;
	}

	/* Do the rest of the battery icon states */
	if (has_primary) {
		index = get_icon_index_from_percent (primary_percentage);

		if (on_ac) {
			if (!primary_charging && !primary_discharging) {
				return g_strdup ("gnome-power-ac-charged");
			} else {
				return g_strdup_printf ("gnome-power-ac-%d-of-8", index);
			}
		} else {
			return g_strdup_printf ("gnome-power-bat-%d-of-8", index);
		}
	}

	/* We fallback to the ac_adapter icon */
	return g_strdup_printf ("gnome-dev-acadapter");
}

static void
tray_icon_update (GpmManager *manager)
{
	char *stock_id = NULL;
	char *icon_policy;

	/* do we want to display the icon */
	icon_policy = gconf_client_get_string (manager->priv->gconf_client, GPM_PREF_ICON_POLICY, NULL);

	if (! icon_policy) {
		g_warning ("You have not set an icon policy! "
			   "(Please run gnome-power-preferences) -- "
			   "I'll assume you want an icon all the time...");
		icon_policy = g_strdup (ICON_POLICY_ALWAYS);
	}

	/* try to get stock image */
	stock_id = get_stock_id (manager, icon_policy);
	g_free (icon_policy);

	/* only create if we have a valid filename */
	if (stock_id) {
		char *tooltip = NULL;

		if (! manager->priv->tray_icon) {
			gpm_manager_setup_tray_icon (manager, NULL);
		}

		gpm_tray_icon_set_image_from_stock (GPM_TRAY_ICON (manager->priv->tray_icon),
						    stock_id);
		g_free (stock_id);

		gpm_power_get_status_summary (manager->priv->power, &tooltip, NULL);

		gpm_tray_icon_set_tooltip (GPM_TRAY_ICON (manager->priv->tray_icon),
					   tooltip);
		g_free (tooltip);
	} else {
		/* remove icon */
		g_debug ("no icon will be displayed");

		if (manager->priv->tray_icon) {
			/* disconnect the signal so we don't restart */
			g_signal_handlers_disconnect_by_func (manager->priv->tray_icon,
							      G_CALLBACK (gpm_manager_setup_tray_icon),
							      manager);
			gtk_widget_destroy (GTK_WIDGET (manager->priv->tray_icon));
			manager->priv->tray_icon = NULL;
		}
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

	/* convert minutes to secs */
	standby *= 60;

	if (error) {
		g_warning ("Unable to get DPMS timeouts: %s", error->message);
		g_error_free (error);
		return;
	}

	/* try to make up some reasonable numbers */
	suspend = standby;
	off     = standby * 2;

	error = NULL;
	res = gpm_dpms_set_enabled (manager->priv->dpms, TRUE, &error);
	if (error) {
		g_warning ("Unable to enable DPMS: %s", error->message);
		g_error_free (error);
		return;
	}

	error = NULL;
	res = gpm_dpms_set_timeouts (manager->priv->dpms, standby, suspend, off, &error);
	if (error) {
		g_warning ("Unable to get DPMS timeouts: %s", error->message);
		g_error_free (error);
		return;
	}
}

/** Do all the action when we go from batt to ac, or ac to batt (or percentagechanged)
 *
 *  @param	on_ac		If we are on AC power
 *
 *  @note
 *	- Sets the brightness level
 *	- Sets HAL to be in LaptopMode if !AC
 *	- Sets DPMS timeout to be our policy value
 *	- Sets GNOME Screensaver to [not] run fancy screensavers
 *	- Sets our inactivity sleep timeout to policy value
 */
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

	gpm_hal_set_brightness_dim (brightness);
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
	gboolean show_notify;

	if (manager->priv->tray_icon == NULL) {
		return;
	}

	show_notify = gconf_client_get_bool (manager->priv->gconf_client,
					     GPM_PREF_NOTIFY_ACADAPTER, NULL);

	if (! on_ac) {
		if (show_notify) {
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      5000,
					      _("AC Power Unplugged"),
					      NULL,
					     _("The AC Power has been unplugged. "
					       "The system is now using battery power."));
		}
	} else {

		/*
		 * for where we add back the ac_adapter before
		 * the "AC Power unplugged" message times out.
		 */
		gpm_tray_icon_cancel_notify (GPM_TRAY_ICON (manager->priv->tray_icon));
	}

	/* update icon */
	tray_icon_update (manager);
}

/** Do the action dictated by policy from gconf
 *
 *  @param	action	string
 *
 *  @todo	Add the actions to doxygen.
 */
static void
manager_policy_do (GpmManager *manager,
		   const char *policy)
{
	char *action;

	g_debug ("manager_policy_do: %s", policy);

	action = gconf_client_get_string (manager->priv->gconf_client, policy, NULL);

	if (! action)
		return;

	if (strcmp (action, ACTION_NOTHING) == 0) {
		g_debug ("*ACTION* Doing nothing");

	} else if (strcmp (action, ACTION_SUSPEND) == 0) {
		g_debug ("*ACTION* Suspend");

		gpm_manager_suspend (manager);

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
		g_debug ("*ACTION* Hibernate");

		gpm_manager_hibernate (manager);

	} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
		g_debug ("*ACTION* Shutdown");

		gpm_manager_shutdown (manager);

	} else {
		g_warning ("manager_policy_do called with unknown action %s", action);
	}

	g_free (action);

}

static void
maybe_notify_battery_power_changed (GpmManager         *manager,
				    const char         *kind,
				    int		        percentage,
				    glong	        minutes,
				    gboolean	        discharging,
				    gboolean	        charging,
				    gboolean	        percentagechanged)
{
	gboolean show_notify;
	gboolean primary;
	gint	 low_threshold;
	gint	 critical_threshold;
	gchar	*message;
	gchar	*remaining;

	primary = (strcmp (kind, "primary") == 0);

	g_debug ("percentage = %d, minutes = %ld, discharging = %d, "
		 "charging = %d, primary = %d, percentagechanged=%i",
		 percentage, minutes, discharging, charging, primary, percentagechanged);

	if (manager->priv->tray_icon == NULL) {
		return;
	}

	/* update icon */
	tray_icon_update (manager);

	/* give notification @100%, on percentagechanged */
	if (percentagechanged && primary && percentage >= 100) {
		show_notify = gconf_client_get_bool (manager->priv->gconf_client,
						     GPM_PREF_NOTIFY_BATTCHARGED, NULL);

		if (show_notify) {
			gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
					      5000,
					      _("Battery Charged"),
					      NULL,
					      _("Your battery is now fully charged"));
		}

		return;
	}

	if (! discharging) {
		g_debug ("battery is not discharging!");
		return;
	}

	low_threshold = gconf_client_get_int (manager->priv->gconf_client,
					      GPM_PREF_THRESHOLD_LOW, NULL);
	critical_threshold = gconf_client_get_int (manager->priv->gconf_client,
						   GPM_PREF_THRESHOLD_CRITICAL, NULL);

	g_debug ("percentage = %d, low_threshold = %i, critical_threshold = %i",
		 percentage, low_threshold, critical_threshold);

	/* less than critical, do action */
	if (percentage < critical_threshold) {
		g_debug ("battery is below critical limit!");
		manager_policy_do (manager, GPM_PREF_BATTERY_CRITICAL);

		return;
	}

	/* critical warning */
	if (percentage == critical_threshold) {
		g_debug ("battery is critical limit!");
		remaining = get_timestring_from_minutes (minutes);

		message = g_strdup_printf (_("You have approximately <b>%s</b> "
					     "of remaining battery life (%d%%). "
					     "Plug in your AC Adapter to avoid losing data."),
					   remaining, percentage);

		gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
				      5000,
				      _("Battery Critically Low"),
				      NULL,
				      message);
		g_free (message);
		g_free (remaining);

		return;
	}

	/* low warning */
	if (percentage < low_threshold) {
		g_debug ("battery is low!");
		remaining = get_timestring_from_minutes (minutes);
		g_assert (remaining);

		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining battery life (%d%%). "
					     "Plug in your AC Adapter to avoid losing data."),
					   remaining, percentage);

		gpm_tray_icon_notify (GPM_TRAY_ICON (manager->priv->tray_icon),
				      5000,
				      _("Battery Low"),
				      NULL,
				      message);

		g_free (message);
		g_free (remaining);

		return;
	}
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

	g_debug ("Setting DPMS to %s", mode);

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
	g_debug ("Got DPMS mode result=%d mode=%d", ret, m);
	if (ret && mode) {
		*mode = gpm_dpms_mode_to_string (m);
	}

	return ret;
}

void
gpm_manager_shutdown (GpmManager *manager)
{
	if (! _gpm_manager_can_shutdown (manager)) {
		g_warning ("Cannot shutdown");
		return;
	}

	gnome_client_request_save (gnome_master_client (),
				   GNOME_SAVE_GLOBAL,
				   FALSE, GNOME_INTERACT_NONE, FALSE,  TRUE);

	gpm_hal_shutdown ();
}

/** Do a hibernate with all the associated callbacks and methods.
 *
 *  @note
 *	- Locks the screen (if required)
 *	- Sets NetworkManager to sleep
 *	- Does the hibernate...
 *	- Sets NetworkManager to wake
 *	- Pokes g-s so we get the unlock screen (if required)
 */
void
gpm_manager_hibernate (GpmManager *manager)
{
	gboolean should_lock = gpm_screensaver_lock_enabled ();

	if (! _gpm_manager_can_hibernate (manager)) {
		g_warning ("Cannot hibernate");
		return;
	}

	if (should_lock)
		gpm_screensaver_lock ();

	gpm_networkmanager_sleep ();
	gpm_hal_hibernate ();
	gpm_networkmanager_wake ();

	/* Poke GNOME ScreenSaver so the dialogue is displayed */
	if (should_lock)
		gpm_screensaver_poke ();
}

/** Do a suspend with all the associated callbacks and methods.
 *
 *  @note
 *	- Locks the screen (if required)
 *	- Sets NetworkManager to sleep
 *	- Does the suspend...
 *	- Sets NetworkManager to wake
 *	- Pokes g-s so we get the unlock screen (if required)
 */
void
gpm_manager_suspend (GpmManager *manager)
{
	gboolean should_lock = gpm_screensaver_lock_enabled ();

	if (! _gpm_manager_can_suspend (manager)) {
		g_warning ("Cannot suspend");
		return;
	}

	if (should_lock)
		gpm_screensaver_lock ();

	gpm_networkmanager_sleep ();
	gpm_hal_suspend (0);
	gpm_networkmanager_wake ();

	/* Poke GNOME ScreenSaver so the dialogue is displayed */
	if (should_lock)
		gpm_screensaver_poke ();
}

/** Callback for the idle function.
 */
static void
idle_changed_cb (GpmIdle    *idle,
		 GpmIdleMode mode,
		 GpmManager *manager)
{
	GError  *error;
	gboolean res;

	switch (mode) {
	case GPM_IDLE_MODE_NORMAL:
		g_debug ("Idle state changed: NORMAL");

		/* deactivate display power management */
		error = NULL;
		res = gpm_dpms_set_active (manager->priv->dpms, FALSE, &error);
		if (error) {
			g_debug ("Unable to set DPMS active: %s", error->message);
		}
		
		sync_dpms_policy (manager);

		break;
	case GPM_IDLE_MODE_SESSION:
		
		g_debug ("Idle state changed: SESSION");

		/* activate display power management */
		error = NULL;
		res = gpm_dpms_set_active (manager->priv->dpms, TRUE, &error);
		if (error) {
			g_debug ("Unable to set DPMS active: %s", error->message);
		}

		/* sync timeouts */
		sync_dpms_policy (manager);

		break;
	case GPM_IDLE_MODE_SYSTEM:
		g_debug ("Idle state changed: SYSTEM");

		/* can only be hibernate or suspend */
		manager_policy_do (manager, GPM_PREF_BATTERY_CRITICAL);

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
	g_debug ("DPMS mode changed: %d", mode);
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

	g_signal_emit (manager,
		       signals [DPMS_MODE_CHANGED],
		       0,
		       gpm_dpms_mode_to_string (mode));
}

static void
power_button_pressed (GpmManager   *manager,
		      gboolean	    state)
{
	g_debug ("power button changed: %d", state);

	/* Log out interactively */
	gnome_client_request_save (gnome_master_client (),
				   GNOME_SAVE_GLOBAL,
				   TRUE, GNOME_INTERACT_ANY, FALSE,  TRUE);
}

static void
suspend_button_pressed (GpmManager   *manager,
			gboolean      state)
{
	manager_policy_do (manager, GPM_PREF_BUTTON_SUSPEND);
}

static void
lid_button_pressed (GpmManager	 *manager,
		    gboolean	  state)
{
	GpmDpmsMode mode;
	GError     *error;
	gboolean    res;

	g_debug ("lid button changed: %d", state);

	/*
	 * We enable/disable DPMS because some laptops do
	 * not turn off the LCD backlight when the lid
	 * is closed. See
	 * http://bugzilla.gnome.org/show_bug.cgi?id=321313
	 */
	if (state) {
		/* we only do a policy event when the lid is CLOSED */
		manager_policy_do (manager, GPM_PREF_BUTTON_LID);
		mode = GPM_DPMS_MODE_OFF;
	} else {
		mode = GPM_DPMS_MODE_ON;
	}

	error = NULL;
	res = gpm_dpms_set_mode (manager->priv->dpms, mode, &error);
	if (error) {
		g_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
	}
}

static void
power_button_pressed_cb (GpmPower   *power,
			 const char *type,
			 const char *details,
			 gboolean    state,
			 GpmManager *manager)
{
	g_debug ("Received a button press event type=%s details=%s state=%d",
		 type, details, state);

	if (strcmp (type, "power") == 0) {
		power_button_pressed (manager, state);
	} else if (strcmp (type, "sleep") == 0) {
		suspend_button_pressed (manager, state);
	} else if (strcmp (type, "lid") == 0) {
		lid_button_pressed (manager, state);
	} else if (strcmp (type, "virtual") == 0) {
		if (details == NULL) {
			return;
		}

		if (strcmp (details, "BrightnessUp") == 0) {
			gpm_hal_set_brightness_up ();
		} else if (strcmp (details, "BrightnessDown") == 0) {
			gpm_hal_set_brightness_down ();
		} else if (strcmp (details, "Suspend") == 0) {
			gpm_manager_suspend (manager);
		} else if (strcmp (details, "Hibernate") == 0) {
			gpm_manager_hibernate (manager);
		} else if (strcmp (details, "Lock") == 0) {
			gpm_screensaver_lock ();
		}
	}
}

static void
power_on_ac_changed_cb (GpmPower   *power,
			gboolean    on_ac,
			GpmManager *manager)
{
	g_debug ("Setting on-ac: %d", on_ac);

	maybe_notify_on_ac_changed (manager, on_ac);
	change_power_policy (manager, on_ac);

	g_signal_emit (manager, signals [ON_AC_CHANGED], 0, on_ac);
}

static void
power_battery_power_changed_cb (GpmPower           *power,
				const char         *kind,
				int	            percentage,
				glong	            minutes,
				gboolean            discharging,
				gboolean            charging,
				gboolean            percentagechanged,
				GpmManager         *manager)
{
	maybe_notify_battery_power_changed (manager,
					    kind,
					    percentage,
					    minutes,
					    discharging,
					    charging,
					    percentagechanged);
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

/** Callback for gconf modified keys (that we are watching).
 *
 * @param	client		A valid GConfClient
 * @param	cnxn_id		Unknown
 * @param	entry		The key that was modified
 * @param	user_data	user_data pointer. No function.
 */
static void
callback_gconf_key_changed (GConfClient *client,
			    guint	 cnxn_id,
			    GConfEntry	*entry,
			    gpointer	 user_data)
{
	gint	    value = 0;
	GpmManager *manager = GPM_MANAGER (user_data);
	gboolean    on_ac;

	gpm_power_get_on_ac (manager->priv->power, &on_ac, NULL);

	g_debug ("callback_gconf_key_changed (%s)", entry->key);

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

	}
}

static void
gpm_manager_tray_icon_hibernate (GpmManager   *manager,
				 GpmTrayIcon  *tray)
{
	g_debug ("Received hibernate signal from tray icon");
	gpm_manager_hibernate (manager);
}

static void
gpm_manager_tray_icon_suspend (GpmManager   *manager,
			       GpmTrayIcon  *tray)
{
	g_debug ("Received supend signal from tray icon");
	gpm_manager_suspend (manager);
}

static gboolean
gpm_manager_setup_tray_icon (GpmManager *manager,
			     GtkObject	*object)
{
	gboolean enabled;

	if (manager->priv->tray_icon) {
		g_debug ("caught destroy event for tray icon %p",
			 manager->priv->tray_icon);
		gtk_object_sink (GTK_OBJECT (manager->priv->tray_icon));
		manager->priv->tray_icon = NULL;
		g_debug ("finished sinking tray");
	}

	g_debug ("creating new tray icon");
	manager->priv->tray_icon = gpm_tray_icon_new ();

	enabled = _gpm_manager_can_suspend (manager);
	gpm_tray_icon_enable_suspend (GPM_TRAY_ICON (manager->priv->tray_icon), enabled);

	enabled = _gpm_manager_can_hibernate (manager);
	gpm_tray_icon_enable_hibernate (GPM_TRAY_ICON (manager->priv->tray_icon), enabled);

	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "destroy",
				 G_CALLBACK (gpm_manager_setup_tray_icon),
				 manager,
				 G_CONNECT_SWAPPED);

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

	gtk_widget_show_all (GTK_WIDGET (manager->priv->tray_icon));

	g_debug ("done creating new tray icon %p", manager->priv->tray_icon);

	return TRUE;
}

static void
gpm_manager_init (GpmManager *manager)
{
	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);

	manager->priv->gconf_client = gconf_client_get_default ();

	manager->priv->power = gpm_power_new ();
	g_signal_connect (manager->priv->power, "button-pressed",
			  G_CALLBACK (power_button_pressed_cb), manager);
	g_signal_connect (manager->priv->power, "ac-power-changed",
			  G_CALLBACK (power_on_ac_changed_cb), manager);
	g_signal_connect (manager->priv->power, "battery-power-changed",
			  G_CALLBACK (power_battery_power_changed_cb), manager);

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

	manager->priv->idle = gpm_idle_new ();
	g_signal_connect (manager->priv->idle, "changed",
			  G_CALLBACK (idle_changed_cb), manager);

	manager->priv->dpms = gpm_dpms_new ();
	sync_dpms_policy (manager);
	g_signal_connect (manager->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), manager);

	tray_icon_update (manager);
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

	if (manager->priv->power != NULL) {
		g_object_unref (manager->priv->power);
	}

	if (manager->priv->tray_icon != NULL) {
		gtk_widget_destroy (GTK_WIDGET (manager->priv->tray_icon));
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
