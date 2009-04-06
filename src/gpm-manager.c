/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2008 Richard Hughes <richard@hughsie.com>
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
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gconf/gconf-client.h>
#include <canberra-gtk.h>
#include <dkp-device.h>

#include "egg-debug.h"
#include "egg-console-kit.h"

#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "gpm-dpms.h"
#include "gpm-idle.h"
#include "gpm-inhibit.h"
#include "gpm-manager.h"
#include "gpm-notify.h"
#include "gpm-prefs.h"
#include "gpm-screensaver.h"
#include "gpm-backlight.h"
#include "gpm-brightness-kbd.h"
#include "gpm-screensaver.h"
#include "gpm-session.h"
#include "gpm-stock-icons.h"
#include "gpm-prefs-server.h"
#include "gpm-tray-icon.h"
#include "gpm-engine.h"
#include "gpm-devicekit.h"
#include "gpm-feedback-widget.h"

#include "org.freedesktop.PowerManagement.Inhibit.h"
#include "org.freedesktop.PowerManagement.Backlight.h"

static void     gpm_manager_finalize	(GObject	 *object);

#define GPM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_MANAGER, GpmManagerPrivate))
#define GPM_MANAGER_RECALL_DELAY		10

struct GpmManagerPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmButton		*button;
	GConfClient		*conf;
	GpmDpms			*dpms;
	GpmIdle			*idle;
	GpmPrefsServer		*prefs_server;
	GpmInhibit		*inhibit;
	GpmNotify		*notify;
	GpmControl		*control;
	GpmScreensaver 		*screensaver;
	GpmTrayIcon		*tray_icon;
	GpmEngine		*engine;
	gboolean		 low_power;
	GpmBrightnessKbd	*brightness_kbd;
	GpmFeedback		*feedback_kbd;
	GpmBacklight		*backlight;
	EggConsoleKit		*console;
	guint32         	 screensaver_ac_throttle_id;
	guint32         	 screensaver_dpms_throttle_id;
	guint32         	 screensaver_lid_throttle_id;
};

enum {
	ON_BATTERY_CHANGED,
	LOW_BATTERY_CHANGED,
	POWER_SAVE_STATUS_CHANGED,
	LAST_SIGNAL
};

typedef enum {
	GPM_MANAGER_SOUND_POWER_PLUG,
	GPM_MANAGER_SOUND_POWER_UNPLUG,
	GPM_MANAGER_SOUND_LID_OPEN,
	GPM_MANAGER_SOUND_LID_CLOSE,
	GPM_MANAGER_SOUND_BATTERY_CAUTION,
	GPM_MANAGER_SOUND_BATTERY_LOW,
	GPM_MANAGER_SOUND_BATTERY_FULL,
	GPM_MANAGER_SOUND_SUSPEND_START,
	GPM_MANAGER_SOUND_SUSPEND_RESUME,
	GPM_MANAGER_SOUND_SUSPEND_ERROR,
	GPM_MANAGER_SOUND_LAST
} GpmManagerSound;

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmManager, gpm_manager, G_TYPE_OBJECT)

/**
 * gpm_manager_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_manager_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gpm_manager_error");
	return quark;
}

/**
 * gpm_manager_error_get_type:
 **/
#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }
GType
gpm_manager_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] =
		{
			ENUM_ENTRY (GPM_MANAGER_ERROR_DENIED, "PermissionDenied"),
			ENUM_ENTRY (GPM_MANAGER_ERROR_NO_HW, "NoHardwareSupport"),
			{ 0, 0, 0 }
		};
		etype = g_enum_register_static ("GpmManagerError", values);
	}
	return etype;
}

/**
 * gpm_manager_play:
 **/
static gboolean
gpm_manager_play (GpmManager *manager, GpmManagerSound action, gboolean force)
{
	const gchar *id = NULL;
	const gchar *desc = NULL;
	gboolean ret;

	ret = gconf_client_get_bool (manager->priv->conf, GPM_CONF_UI_ENABLE_BEEPING, NULL);
	if (!ret && !force) {
		egg_debug ("ignoring sound due to policy");
		return FALSE;
	}

	if (action == GPM_MANAGER_SOUND_POWER_PLUG) {
		id = "power-plug";
		/* TRANSLATORS: this is the sound description */
		desc = _("Power plugged in");
	} else if (action == GPM_MANAGER_SOUND_POWER_UNPLUG) {
		id = "power-unplug";
		/* TRANSLATORS: this is the sound description */
		desc = _("Power unplugged");
	} else if (action == GPM_MANAGER_SOUND_LID_OPEN) {
		id = "lid-open";
		/* TRANSLATORS: this is the sound description */
		desc = _("Lid has opened");
	} else if (action == GPM_MANAGER_SOUND_LID_CLOSE) {
		id = "lid-close";
		/* TRANSLATORS: this is the sound description */
		desc = _("Lid has closed");
	} else if (action == GPM_MANAGER_SOUND_BATTERY_CAUTION) {
		id = "battery-caution";
		/* TRANSLATORS: this is the sound description */
		desc = _("Battery is low");
	} else if (action == GPM_MANAGER_SOUND_BATTERY_LOW) {
		id = "battery-low";
		/* TRANSLATORS: this is the sound description */
		desc = _("Battery is very low");
	} else if (action == GPM_MANAGER_SOUND_BATTERY_FULL) {
		id = "battery-full";
		/* TRANSLATORS: this is the sound description */
		desc = _("Battery is full");
	} else if (action == GPM_MANAGER_SOUND_SUSPEND_START) {
		id = "suspend-start";
		/* TRANSLATORS: this is the sound description */
		desc = _("Suspend started");
	} else if (action == GPM_MANAGER_SOUND_SUSPEND_RESUME) {
		id = "suspend-resume";
		/* TRANSLATORS: this is the sound description */
		desc = _("Resumed");
	} else if (action == GPM_MANAGER_SOUND_SUSPEND_ERROR) {
		id = "suspend-error";
		/* TRANSLATORS: this is the sound description */
		desc = _("Suspend failed");
	}

	/* no match */
	if (id == NULL) {
		egg_warning ("no match");
		return FALSE;
	}

	/* play the sound, using sounds from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, id,
			 CA_PROP_EVENT_DESCRIPTION, desc, NULL);
	return TRUE;
}

/**
 * gpm_manager_is_inhibit_valid:
 * @manager: This class instance
 * @action: The action we want to do, e.g. "suspend"
 *
 * Checks to see if the specific action has been inhibited by a program.
 * If so, displays a warning libnotify dialogue for the user explaining
 * the situation.
 *
 * Return value: TRUE if we can perform the action.
 **/
static gboolean
gpm_manager_is_inhibit_valid (GpmManager *manager, gboolean user_action, const char *action)
{
	gboolean has_inhibit;
	gchar *title = NULL;

	/* We have to decide on whether this is a idle action or a user keypress */
	gpm_inhibit_has_inhibit (manager->priv->inhibit, &has_inhibit, NULL);

	if (has_inhibit) {
		GString *message = g_string_new ("");
		const char *msg;

		/*Compose message for each possible action*/
		if (strcmp (action, "suspend") == 0)
				title = g_strdup (_("Request to suspend")); 
		else if (strcmp (action, "hibernate") == 0)
				title = g_strdup (_("Request to hibernate")); 
		else if (strcmp (action, "policy action") == 0)
				title = g_strdup (_("Request to do policy action")); 
		else if (strcmp (action, "timeout action") == 0)
				title = g_strdup (_("Request to do timeout action"));
		
		gpm_inhibit_get_message (manager->priv->inhibit, message, action);
		gpm_notify_display (manager->priv->notify,
				      title,
				      message->str,
				      GPM_NOTIFY_TIMEOUT_LONG,
				      GTK_STOCK_DIALOG_WARNING,
				      GPM_NOTIFY_URGENCY_NORMAL);
		/* I want this translated */
		msg = _("Perform action anyway");
		g_string_free (message, TRUE);
		g_free (title);
	}
	return !has_inhibit;
}

/**
 * gpm_manager_sync_policy_sleep:
 * @manager: This class instance
 * @on_ac: If we are on AC power
 *
 * Changes the policy if required, setting brightness, display and computer
 * timeouts.
 * We have to make sure gnome-screensaver disables screensaving, and enables
 * monitor DPMS instead when on batteries to save power.
 **/
static void
gpm_manager_sync_policy_sleep (GpmManager *manager)
{
	guint sleep_display;
	guint sleep_computer;
	gboolean on_ac;
	gboolean power_save;

	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);

	if (on_ac) {
		sleep_computer = gconf_client_get_int (manager->priv->conf, GPM_CONF_TIMEOUT_SLEEP_COMPUTER_AC, NULL);
		sleep_display = gconf_client_get_int (manager->priv->conf, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_AC, NULL);
		power_save = gconf_client_get_bool (manager->priv->conf, GPM_CONF_LOWPOWER_AC, NULL);
	} else {
		sleep_computer = gconf_client_get_int (manager->priv->conf, GPM_CONF_TIMEOUT_SLEEP_COMPUTER_BATT, NULL);
		sleep_display = gconf_client_get_int (manager->priv->conf, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_BATT, NULL);
		power_save = gconf_client_get_bool (manager->priv->conf, GPM_CONF_LOWPOWER_BATT, NULL);
	}

	/* set the new sleep (inactivity) value */
	gpm_idle_set_timeout_blank (manager->priv->idle, sleep_display);
	gpm_idle_set_timeout_sleep (manager->priv->idle, sleep_computer);
}

/**
 * gpm_manager_blank_screen:
 * @manager: This class instance
 *
 * Turn off the backlight of the LCD when we shut the lid, and lock
 * if required. This is required because some laptops do not turn off the
 * LCD backlight when the lid is closed.
 * See http://bugzilla.gnome.org/show_bug.cgi?id=321313
 *
 * Return value: Success.
 **/
static gboolean
gpm_manager_blank_screen (GpmManager *manager, GError **noerror)
{
	gboolean do_lock;
	gboolean ret = TRUE;
	GError *error = NULL;

	do_lock = gpm_control_get_lock_policy (manager->priv->control,
					       GPM_CONF_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		if (!gpm_screensaver_lock (manager->priv->screensaver))
			egg_debug ("Could not lock screen via gnome-screensaver");
	}
	gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_OFF, &error);
	if (error) {
		egg_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}
	return ret;
}

/**
 * gpm_manager_unblank_screen:
 * @manager: This class instance
 *
 * Unblank the screen after we have opened the lid of the laptop
 *
 * Return value: Success.
 **/
static gboolean
gpm_manager_unblank_screen (GpmManager *manager, GError **noerror)
{
	gboolean do_lock;
	gboolean ret = TRUE;
	GError *error = NULL;

	gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_ON, &error);
	if (error) {
		egg_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	do_lock = gpm_control_get_lock_policy (manager->priv->control, GPM_CONF_LOCK_ON_BLANK_SCREEN);
	if (do_lock)
		gpm_screensaver_poke (manager->priv->screensaver);
	return ret;
}

/**
 * gpm_manager_action_suspend:
 **/
static gboolean
gpm_manager_action_suspend (GpmManager *manager, const gchar *reason)
{
	gboolean allowed;
	GError *error = NULL;

	/* check if the admin has disabled */
	allowed = gconf_client_get_bool (manager->priv->conf, GPM_CONF_CAN_SUSPEND, NULL);
	if (allowed == FALSE) {
		/* error msg as disabled in gconf */
		gpm_notify_display (manager->priv->notify,
				    _("Action disallowed"),
				    _("Suspend support has been disabled. Contact your administrator for more details."),
				    GPM_NOTIFY_TIMEOUT_SHORT,
				    GPM_STOCK_APP_ICON,
				    GPM_NOTIFY_URGENCY_NORMAL);
		return FALSE;
	}

	/* check to see if we are inhibited */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "suspend") == FALSE)
		return FALSE;

	egg_debug ("suspending, reason: %s", reason);
	gpm_control_suspend (manager->priv->control, &error);
	gpm_button_reset_time (manager->priv->button);
	if (error != NULL)
		g_error_free (error);
	return TRUE;
}

/**
 * gpm_manager_action_hibernate:
 **/
static gboolean
gpm_manager_action_hibernate (GpmManager *manager, const gchar *reason)
{
	gboolean allowed;
	GError *error = NULL;

	/* check if the admin has disabled */
	allowed = gconf_client_get_bool (manager->priv->conf, GPM_CONF_CAN_HIBERNATE, NULL);
	if (allowed == FALSE) {
		/* error msg as disabled in gconf */
		gpm_notify_display (manager->priv->notify,
				    _("Action disallowed"),
				    _("Hibernate support has been disabled. Contact your administrator for more details."),
				    GPM_NOTIFY_TIMEOUT_SHORT,
				    GPM_STOCK_APP_ICON,
				    GPM_NOTIFY_URGENCY_NORMAL);
		return FALSE;
	}

	/* check to see if we are inhibited */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "hibernate") == FALSE)
		return FALSE;

	egg_debug ("hibernating, reason: %s", reason);
	gpm_control_hibernate (manager->priv->control, &error);
	gpm_button_reset_time (manager->priv->button);
	if (error != NULL)
		g_error_free (error);
	return TRUE;
}

/**
 * manager_policy_do:
 * @manager: This class instance
 * @policy: The policy that we should do, e.g. "suspend"
 * @reason: The reason we are performing the policy action, e.g. "battery critical"
 *
 * Does one of the policy actions specified in gconf.
 **/
static gboolean
manager_policy_do (GpmManager  *manager, const gchar *policy, const gchar *reason)
{
	gchar *action = NULL;

	/* are we inhibited? */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "policy action") == FALSE)
		return FALSE;

	egg_debug ("policy: %s", policy);
	action = gconf_client_get_string (manager->priv->conf, policy, NULL);
	if (action == NULL)
		return FALSE;

	if (strcmp (action, ACTION_NOTHING) == 0) {
		egg_debug ("doing nothing, reason: %s", reason);
	} else if (strcmp (action, ACTION_SUSPEND) == 0) {
		gpm_manager_action_suspend (manager, reason);

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
		gpm_manager_action_hibernate (manager, reason);

	} else if (strcmp (action, ACTION_BLANK) == 0) {
		gpm_manager_blank_screen (manager, NULL);

	} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
		egg_debug ("shutting down, reason: %s", reason);
		gpm_control_shutdown (manager->priv->control, NULL);

	} else if (strcmp (action, ACTION_INTERACTIVE) == 0) {
		GpmSession *session;
		egg_debug ("logout, reason: %s", reason);
		session = gpm_session_new ();
		gpm_session_logout (session);
		g_object_unref (session);
	} else {
		egg_warning ("unknown action %s", action);
	}

	g_free (action);
	return TRUE;
}

/**
 * gpm_manager_suspend:
 *
 * Attempt to suspend the system.
 **/
gboolean
gpm_manager_suspend (GpmManager *manager, GError **error)
{
	gboolean allowed;

	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	/* check if the admin has disabled */
	allowed = gconf_client_get_bool (manager->priv->conf, GPM_CONF_CAN_SUSPEND, NULL);
	if (allowed == FALSE) {
		g_set_error (error, GPM_MANAGER_ERROR, GPM_MANAGER_ERROR_DENIED, "Suspend denied by gconf policy");
		return FALSE;
	}

	/* check to see if we are inhibited */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "hibernate") == FALSE)
		return FALSE;

	return gpm_control_suspend (manager->priv->control, error);
}

/**
 * gpm_manager_hibernate:
 *
 * Attempt to hibernate the system.
 **/
gboolean
gpm_manager_hibernate (GpmManager *manager, GError **error)
{
	gboolean allowed;

	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	/* check if the admin has disabled */
	allowed = gconf_client_get_bool (manager->priv->conf, GPM_CONF_CAN_HIBERNATE, NULL);
	if (allowed == FALSE) {
		g_set_error (error, GPM_MANAGER_ERROR, GPM_MANAGER_ERROR_DENIED, "Hibernate denied by gconf policy");
		return FALSE;
	}

	/* check to see if we are inhibited */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "hibernate") == FALSE)
		return FALSE;

	return gpm_control_hibernate (manager->priv->control, error);
}

/**
 * gpm_manager_can_suspend:
 *
 * If the current session user is able to suspend.
 **/
gboolean
gpm_manager_can_suspend (GpmManager *manager, gboolean *can_suspend, GError **error)
{
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);
	return gpm_control_allowed_suspend (manager->priv->control, can_suspend, error);
}

/**
 * gpm_manager_can_hibernate:
 *
 * If the current session user is able to hibernate.
 **/
gboolean
gpm_manager_can_hibernate (GpmManager *manager, gboolean *can_hibernate, GError **error)
{
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);
	return gpm_control_allowed_hibernate (manager->priv->control, can_hibernate, error);
}

/**
 * gpm_manager_get_preferences_options:
 **/
gboolean
gpm_manager_get_preferences_options (GpmManager *manager, gint *capability, GError **error)
{
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);
	return gpm_prefs_server_get_capability (manager->priv->prefs_server, capability);
}

/**
 * gpm_manager_get_power_save_status:
 *
 * Returns true if low power mode has been set.
 * This may be set on AC or battery power, both, or neither depending on
 * the users policy setting.
 * This setting may also change with the battery level changing.
 * Programs should respect this value for the session.
 **/
gboolean
gpm_manager_get_power_save_status (GpmManager *manager, gboolean *low_power, GError **error)
{
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);
	*low_power = manager->priv->low_power;
	return TRUE;
}

/**
 * gpm_manager_get_on_battery:
 *
 * Returns the system AC state, i.e. if we are not running on battery
 * power.
 * Note: This method may still return false on AC using a desktop system
 * if the computer is using backup power from a monitored UPS.
 **/
gboolean
gpm_manager_get_on_battery (GpmManager *manager, gboolean *on_battery, GError **error)
{
	gboolean on_ac;
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);
	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);
	*on_battery = !on_ac;
	return TRUE;
}

/**
 * gpm_manager_get_low_battery:
 **/
gboolean
gpm_manager_get_low_battery (GpmManager *manager, gboolean *low_battery, GError **error)
{
	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	/* TODO */
	*low_battery = FALSE;
	return TRUE;
}

/**
 * idle_do_sleep:
 * @manager: This class instance
 *
 * This callback is called when we want to sleep. Use the users
 * preference from gconf, but change it if we can't do the action.
 **/
static void
idle_do_sleep (GpmManager *manager)
{
	gboolean on_ac;
	gchar *action = NULL;
	gboolean ret;
	GError *error = NULL;

	/* find if we are on AC power */
	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);

	if (on_ac)
		action = gconf_client_get_string (manager->priv->conf, GPM_CONF_ACTIONS_SLEEP_TYPE_AC, NULL);
	else
		action = gconf_client_get_string (manager->priv->conf, GPM_CONF_ACTIONS_SLEEP_TYPE_BATT, NULL);

	if (action == NULL) {
		egg_warning ("action NULL, gconf error");
		return;
	}

	if (strcmp (action, ACTION_NOTHING) == 0) {
		egg_debug ("doing nothing as system idle action");

	} else if (strcmp (action, ACTION_SUSPEND) == 0) {
		egg_debug ("suspending, reason: System idle");
		ret = gpm_control_suspend (manager->priv->control, &error);
		if (!ret) {
			egg_warning ("cannot suspend (error: %s), so trying hibernate", error->message);
			g_error_free (error);
			error = NULL;
			ret = gpm_control_hibernate (manager->priv->control, &error);
			if (!ret) {
				egg_warning ("cannot suspend or hibernate: %s", error->message);
				g_error_free (error);
			}
		}

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
		egg_debug ("hibernating, reason: System idle");
		ret = gpm_control_hibernate (manager->priv->control, &error);
		if (!ret) {
			egg_warning ("cannot hibernate (error: %s), so trying suspend", error->message);
			g_error_free (error);
			error = NULL;
			ret = gpm_control_suspend (manager->priv->control, &error);
			if (!ret) {
				egg_warning ("cannot suspend or hibernate: %s", error->message);
				g_error_free (error);
			}
		}
	}
	g_free (action);
}

/**
 * idle_changed_cb:
 * @idle: The idle class instance
 * @mode: The idle mode, e.g. GPM_IDLE_MODE_BLANK
 * @manager: This class instance
 *
 * This callback is called when the idle class detects that the idle state
 * has changed. GPM_IDLE_MODE_BLANK is when the session has become inactive,
 * and GPM_IDLE_MODE_SLEEP is where the session has become inactive, AND the
 * session timeout has elapsed for the idle action.
 **/
static void
idle_changed_cb (GpmIdle *idle, GpmIdleMode mode, GpmManager *manager)
{
	/* ConsoleKit says we are not on active console */
	if (!egg_console_kit_is_active (manager->priv->console)) {
		egg_debug ("ignoring as not on active console");
		return;
	}

	/* Ignore timeout events when the lid is closed, as the DPMS is
	 * already off, and we don't want to perform policy actions or re-enable
	 * the screen when the user moves the mouse on systems that do not
	 * support hardware blanking.
	 * Details are here: https://launchpad.net/malone/bugs/22522 */
	if (gpm_button_is_lid_closed (manager->priv->button)) {
		egg_debug ("lid is closed, so we are ignoring idle state changes");
		return;
	}

	if (mode == GPM_IDLE_MODE_NORMAL) {
		egg_debug ("Idle state changed: NORMAL");
		gpm_brightness_kbd_undim (manager->priv->brightness_kbd);
	} else if (mode == GPM_IDLE_MODE_BLANK) {
		egg_debug ("Idle state changed: BLANK");
		gpm_brightness_kbd_dim (manager->priv->brightness_kbd);
	} else if (mode == GPM_IDLE_MODE_SLEEP) {
		egg_debug ("Idle state changed: SLEEP");
		if (gpm_manager_is_inhibit_valid (manager, FALSE, "timeout action") == FALSE)
			return;
		idle_do_sleep (manager);
	}
}

/**
 * lid_button_pressed:
 * @manager: This class instance
 * @state: TRUE for closed
 *
 * Does actions when the lid is closed, depending on if we are on AC or
 * battery power.
 **/
static void
lid_button_pressed (GpmManager *manager, gboolean pressed)
{
	gboolean on_ac;
	gboolean has_inhibit;
	gboolean do_policy;
	gchar *action;

	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);

	if (pressed)
		gpm_manager_play (manager, GPM_MANAGER_SOUND_LID_CLOSE, FALSE);
	else
		gpm_manager_play (manager, GPM_MANAGER_SOUND_LID_OPEN, FALSE);

	if (pressed == FALSE) {
		/* we turn the lid dpms back on unconditionally */
		gpm_manager_unblank_screen (manager, NULL);
		return;
	}

	if (on_ac) {
		egg_debug ("Performing AC policy");
		manager_policy_do (manager, GPM_CONF_BUTTON_LID_AC,
				   _("The lid has been closed on ac power."));
		return;
	}

	/* default */
	do_policy = TRUE;

	/* are we inhibited? */
	gpm_inhibit_has_inhibit (manager->priv->inhibit, &has_inhibit, NULL);

	/* do not do lid close action if suspend (or hibernate) */
	if (has_inhibit) {
		/* get the policy action for battery */
		action = gconf_client_get_string (manager->priv->conf, GPM_CONF_BUTTON_LID_BATT, NULL);

		/* if we are trying to suspend or hibernate then don't do action */
		if ((strcmp (action, ACTION_SUSPEND) == 0) ||
		    (strcmp (action, ACTION_HIBERNATE) == 0))
			do_policy = FALSE;
		g_free (action);
	}

	if (do_policy == FALSE) {
		egg_debug ("Not doing lid policy action as inhibited as set to sleep");
		return;
	}

	egg_debug ("Performing battery policy");
	manager_policy_do (manager, GPM_CONF_BUTTON_LID_BATT,
			   _("The lid has been closed on battery power."));
}

static void
update_dpms_throttle (GpmManager *manager)
{
	GpmDpmsMode mode;
	gpm_dpms_get_mode (manager->priv->dpms, &mode, NULL);

	/* Throttle the manager when DPMS is active since we can't see it anyway */
	if (mode == GPM_DPMS_MODE_ON) {
		if (manager->priv->screensaver_dpms_throttle_id != 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_dpms_throttle_id);
			manager->priv->screensaver_dpms_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (manager->priv->screensaver_dpms_throttle_id != 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_dpms_throttle_id);
		}
		manager->priv->screensaver_dpms_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("Display DPMS activated"));
	}
}

static void
update_ac_throttle (GpmManager *manager, gboolean on_ac)
{
	/* Throttle the manager when we are not on AC power so we don't
	   waste the battery */
	if (on_ac) {
		if (manager->priv->screensaver_ac_throttle_id != 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_ac_throttle_id);
			manager->priv->screensaver_ac_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (manager->priv->screensaver_ac_throttle_id != 0)
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_ac_throttle_id);
		manager->priv->screensaver_ac_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("On battery power"));
	}
}

static void
update_lid_throttle (GpmManager *manager, gboolean lid_is_closed)
{
	/* Throttle the screensaver when the lid is close since we can't see it anyway
	   and it may overheat the laptop */
	if (lid_is_closed == FALSE) {
		if (manager->priv->screensaver_lid_throttle_id != 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_lid_throttle_id);
			manager->priv->screensaver_lid_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (manager->priv->screensaver_lid_throttle_id != 0)
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_lid_throttle_id);
		manager->priv->screensaver_lid_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("Laptop lid is closed"));
	}
}

/**
 * button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @manager: This class instance
 **/
static void
button_pressed_cb (GpmButton *button, const gchar *type, GpmManager *manager)
{
	gchar *message;
	egg_debug ("Button press event type=%s", type);

	/* ConsoleKit says we are not on active console */
	if (!egg_console_kit_is_active (manager->priv->console)) {
		egg_debug ("ignoring as not on active console");
		return;
	}

	if (strcmp (type, GPM_BUTTON_POWER) == 0)
		manager_policy_do (manager, GPM_CONF_BUTTON_POWER, _("The power button has been pressed."));
	else if (strcmp (type, GPM_BUTTON_SLEEP) == 0)
		manager_policy_do (manager, GPM_CONF_BUTTON_SUSPEND, _("The suspend button has been pressed."));
	else if (strcmp (type, GPM_BUTTON_SUSPEND) == 0)
		manager_policy_do (manager, GPM_CONF_BUTTON_SUSPEND, _("The suspend button has been pressed."));
	else if (strcmp (type, GPM_BUTTON_HIBERNATE) == 0)
		manager_policy_do (manager, GPM_CONF_BUTTON_HIBERNATE, _("The hibernate button has been pressed."));
	else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0)
		lid_button_pressed (manager, FALSE);
	else if (strcmp (type, GPM_BUTTON_LID_CLOSED) == 0)
		lid_button_pressed (manager, TRUE);
	else if (strcmp (type, GPM_BUTTON_BATTERY) == 0) {
		message = gpm_engine_get_summary (manager->priv->engine);
		gpm_notify_display (manager->priv->notify,
				      _("Power Information"),
				      message,
				      GPM_NOTIFY_TIMEOUT_LONG,
				      GTK_STOCK_DIALOG_INFO,
				      GPM_NOTIFY_URGENCY_NORMAL);
		g_free (message);
	}

	/* really belongs in gnome-manager */
	if (strcmp (type, GPM_BUTTON_LOCK) == 0)
		gpm_screensaver_lock (manager->priv->screensaver);
	/* Disable or enable the fancy manager, as we don't want
	 * this starting when the lid is shut */
	if (strcmp (type, GPM_BUTTON_LID_CLOSED) == 0)
		update_lid_throttle (manager, TRUE);
	else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0)
		update_lid_throttle (manager, FALSE);

	if ((strcmp (type, GPM_BUTTON_KBD_BRIGHT_UP) == 0))
		gpm_brightness_kbd_up (manager->priv->brightness_kbd);
	else if ((strcmp (type, GPM_BUTTON_KBD_BRIGHT_DOWN) == 0))
		gpm_brightness_kbd_down (manager->priv->brightness_kbd);
	else if (strcmp (type, GPM_BUTTON_KBD_BRIGHT_TOGGLE) == 0)
		gpm_brightness_kbd_toggle (manager->priv->brightness_kbd);
}

/**
 * power_on_ac_changed_cb:
 * @power: The power class instance
 * @on_ac: if we are on AC power
 * @manager: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter *ac_adapter, gboolean on_ac, GpmManager *manager)
{
	gboolean event_when_closed;
	gboolean power_save;
	guint brightness;

	/* ConsoleKit says we are not on active console */
	if (!egg_console_kit_is_active (manager->priv->console)) {
		egg_debug ("ignoring as not on active console");
		return;
	}

	egg_debug ("Setting on-ac: %d", on_ac);

	if (on_ac)
		brightness = gconf_client_get_int (manager->priv->conf, GPM_CONF_KEYBOARD_BRIGHTNESS_AC, NULL);
	else
		brightness = gconf_client_get_int (manager->priv->conf, GPM_CONF_KEYBOARD_BRIGHTNESS_BATT, NULL);
	gpm_brightness_kbd_set_std (manager->priv->brightness_kbd, brightness);

	gpm_manager_sync_policy_sleep (manager);

	update_ac_throttle (manager, on_ac);

	/* simulate user input, but only when the lid is open */
	if (gpm_button_is_lid_closed (manager->priv->button) == FALSE)
		gpm_screensaver_poke (manager->priv->screensaver);

	if (on_ac)
		gpm_manager_play (manager, GPM_MANAGER_SOUND_POWER_PLUG, FALSE);
	else
		gpm_manager_play (manager, GPM_MANAGER_SOUND_POWER_UNPLUG, FALSE);

	egg_debug ("emitting on-ac-changed : %i", on_ac);
	g_signal_emit (manager, signals [ON_BATTERY_CHANGED], 0, !on_ac);

	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);
	if (on_ac)
		power_save = gconf_client_get_bool (manager->priv->conf, GPM_CONF_LOWPOWER_AC, NULL);
	else
		power_save = gconf_client_get_bool (manager->priv->conf, GPM_CONF_LOWPOWER_BATT, NULL);
	if (manager->priv->low_power != power_save)
		g_signal_emit (manager, signals [POWER_SAVE_STATUS_CHANGED], 0, power_save);
	manager->priv->low_power = power_save;

	/* We do the lid close on battery action if the ac_adapter is removed
	   when the laptop is closed and on battery. Fixes #331655 */
	event_when_closed = gconf_client_get_bool (manager->priv->conf, GPM_CONF_ACTIONS_SLEEP_WHEN_CLOSED, NULL);

	/* We keep track of the lid state so we can do the
	   lid close on battery action if the ac_adapter is removed when the laptop
	   is closed. Fixes #331655 */
	if (event_when_closed && on_ac == FALSE && gpm_button_is_lid_closed (manager->priv->button)) {
		manager_policy_do (manager, GPM_CONF_BUTTON_LID_BATT,
				   _("The lid has been closed, and the ac adapter "
				     "removed (and gconf is okay)."));
	}
}

/**
 * manager_critical_action_do:
 * @manager: This class instance
 *
 * This is the stub function when we have waited a few seconds for the user to
 * see the message, explaining what we are about to do.
 *
 * Return value: FALSE, as we don't want to repeat this action on resume.
 **/
static gboolean
manager_critical_action_do (GpmManager *manager)
{
	manager_policy_do (manager, GPM_CONF_ACTIONS_CRITICAL_BATT, _("Battery is critically low."));
	return FALSE;
}

/**
 * gpm_manager_class_init:
 * @klass: The GpmManagerClass
 **/
static void
gpm_manager_class_init (GpmManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_manager_finalize;

	signals [ON_BATTERY_CHANGED] =
		g_signal_new ("on-battery-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmManagerClass, on_battery_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals [LOW_BATTERY_CHANGED] =
		g_signal_new ("low-battery-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmManagerClass, low_battery_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals [POWER_SAVE_STATUS_CHANGED] =
		g_signal_new ("power-save-status-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmManagerClass, power_save_status_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	g_type_class_add_private (klass, sizeof (GpmManagerPrivate));
}

/**
 * gpm_conf_gconf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
gpm_conf_gconf_key_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, GpmManager *manager)
{
	GConfValue *value;
	gint brightness;
	gboolean on_ac;

	value = gconf_entry_get_value (entry);
	if (value == NULL)
		return;

	if (strcmp (entry->key, GPM_CONF_TIMEOUT_SLEEP_COMPUTER_BATT) == 0 ||
	    strcmp (entry->key, GPM_CONF_TIMEOUT_SLEEP_COMPUTER_AC) == 0 ||
	    strcmp (entry->key, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_BATT) == 0 ||
	    strcmp (entry->key, GPM_CONF_TIMEOUT_SLEEP_DISPLAY_AC) == 0)
		gpm_manager_sync_policy_sleep (manager);

	/* set keyboard brightness */
	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);
	if (strcmp (entry->key, GPM_CONF_KEYBOARD_BRIGHTNESS_AC) == 0) {

		brightness = gconf_value_get_int (value);
		if (on_ac)
			gpm_brightness_kbd_set_std (manager->priv->brightness_kbd, brightness);

	} else if (strcmp (entry->key, GPM_CONF_KEYBOARD_BRIGHTNESS_BATT) == 0) {

		brightness = gconf_client_get_int (manager->priv->conf, GPM_CONF_KEYBOARD_BRIGHTNESS_AC, NULL);
		if (on_ac == FALSE)
			gpm_brightness_kbd_set_std (manager->priv->brightness_kbd, brightness);

	}
}

/**
 * gpm_manager_tray_icon_hibernate:
 * @manager: This class instance
 * @tray: The tray object
 *
 * The icon tray hibernate callback, which only should happen if both policy and
 * the inhibit states are valid.
 **/
static void
gpm_manager_tray_icon_hibernate (GpmManager *manager, GpmTrayIcon *tray)
{
	gpm_manager_action_hibernate (manager, _("User clicked on tray"));
}

/**
 * gpm_manager_tray_icon_suspend:
 * @manager: This class instance
 * @tray: The tray object
 *
 * The icon tray suspend callback, which only should happen if both policy and
 * the inhibit states are valid.
 **/
static void
gpm_manager_tray_icon_suspend (GpmManager *manager, GpmTrayIcon *tray)
{
	gpm_manager_action_suspend (manager, _("User clicked on tray"));
}

/**
 * screensaver_auth_request_cb:
 * @manager: This manager class instance
 * @auth: If we are trying to authenticate
 *
 * Called when the user is trying or has authenticated
 **/
static void
screensaver_auth_request_cb (GpmScreensaver *screensaver, gboolean auth_begin, GpmManager *manager)
{
	GError *error = NULL;

	if (auth_begin) {
		/* TODO: This may be a bid of a bodge, as we will have multiple
			 resume requests -- maybe this need a logic cleanup */
		gpm_brightness_kbd_undim (manager->priv->brightness_kbd);

		/* We turn on the monitor unconditionally, as we may be using
		 * a smartcard to authenticate and DPMS might still be on.
		 * See #350291 for more details */
		gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_ON, &error);
		if (error != NULL) {
			egg_warning ("Failed to turn on DPMS: %s", error->message);
			g_error_free (error);
			error = NULL;
		}
	}
}

/**
 * gpm_manager_perhaps_recall:
 */
static gboolean
gpm_manager_perhaps_recall (GpmManager *manager)
{
	gchar *oem_vendor;
	gchar *oem_website;
	oem_vendor = (gchar *) g_object_get_data (G_OBJECT (manager), "recall-oem-vendor");
	oem_website = (gchar *) g_object_get_data (G_OBJECT (manager), "recall-oem-website");
	gpm_notify_perhaps_recall (manager->priv->notify, oem_vendor, oem_website);
	g_free (oem_vendor);
	g_free (oem_website);
	return FALSE;
}

/**
 * gpm_engine_perhaps_recall_cb:
 */
static void
gpm_engine_perhaps_recall_cb (GpmEngine *engine, DkpDeviceType type, gchar *oem_vendor, gchar *website, GpmManager *manager)
{
	g_object_set_data (G_OBJECT (manager), "recall-oem-vendor", (gpointer) g_strdup (oem_vendor));
	g_object_set_data (G_OBJECT (manager), "recall-oem-website", (gpointer) g_strdup (website));
	/* delay by a few seconds so the panel can load */
	g_timeout_add_seconds (GPM_MANAGER_RECALL_DELAY, (GSourceFunc) gpm_manager_perhaps_recall, manager);
}

/**
 * gpm_engine_low_capacity_cb:
 */
static void
gpm_engine_low_capacity_cb (GpmEngine *engine, DkpDeviceType type, guint capacity, GpmManager *manager)
{
	/* We should notify the user if the battery has a low capacity,
	 * where capacity is the ratio of the last_full capacity with that of
	 * the design capacity. (#326740) */
	gpm_notify_low_capacity (manager->priv->notify, capacity);
}

/**
 * gpm_engine_icon_changed_cb:
 */
static void
gpm_engine_icon_changed_cb (GpmEngine  *engine, gchar *icon, GpmManager *manager)
{
	gpm_tray_icon_set_icon (manager->priv->tray_icon, icon);
}

/**
 * gpm_engine_summary_changed_cb:
 */
static void
gpm_engine_summary_changed_cb (GpmEngine *engine, gchar *summary, GpmManager *manager)
{
	gpm_tray_icon_set_tooltip (manager->priv->tray_icon, summary);
}

/**
 * gpm_engine_low_capacity_cb:
 */
static void
gpm_engine_fully_charged_cb (GpmEngine *engine, DkpDeviceType type, GpmManager *manager)
{
	if (type == DKP_DEVICE_TYPE_BATTERY)
		gpm_notify_fully_charged_primary (manager->priv->notify);
}

/**
 * gpm_engine_low_capacity_cb:
 */
static void
gpm_engine_discharging_cb (GpmEngine *engine, DkpDeviceType type, GpmManager *manager)
{
	if (type == DKP_DEVICE_TYPE_BATTERY)
		gpm_notify_discharging_primary (manager->priv->notify);
	else if (type == DKP_DEVICE_TYPE_UPS)
		gpm_notify_discharging_ups (manager->priv->notify);
}

/**
 * control_sleep_failure_cb:
 **/
static void
control_sleep_failure_cb (GpmControl *control, GpmControlAction action, GpmManager *manager)
{
	gboolean show_sleep_failed;

	/* only show this if specified in gconf */
	show_sleep_failed = gconf_client_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_SLEEP_FAILED, NULL);

	egg_debug ("sleep failed");
	gpm_manager_play (manager, GPM_MANAGER_SOUND_SUSPEND_ERROR, TRUE);

	/* only emit if in GConf */
	if (show_sleep_failed) {
		if (action == GPM_CONTROL_ACTION_SUSPEND) {
			egg_debug ("suspend failed");
			gpm_notify_sleep_failed (manager->priv->notify, FALSE);
		} else {
			egg_debug ("hibernate failed");
			gpm_notify_sleep_failed (manager->priv->notify, TRUE);
		}
	}
}

/**
 * gpm_engine_charge_low_cb:
 */
static void
gpm_engine_charge_low_cb (GpmEngine *engine, DkpObject *obj, GpmManager *manager)
{
	const gchar *title = NULL;
	gchar *message = NULL;
	gchar *remaining_text;
	gchar *icon;

	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		title = _("Laptop battery low");
		remaining_text = gpm_get_timestring (obj->time_to_empty);
		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining battery life (%.1f%%)"),
					   remaining_text, obj->percentage);
	} else if (obj->type == DKP_DEVICE_TYPE_UPS) {
		title = _("UPS low");
		remaining_text = gpm_get_timestring (obj->time_to_empty);
		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining UPS backup power (%.1f%%)"),
					   remaining_text, obj->percentage);
	} else if (obj->type == DKP_DEVICE_TYPE_MOUSE) {
		title = _("Mouse battery low");
		message = g_strdup_printf (_("The wireless mouse attached to this computer is low in power (%.1f%%)"), obj->percentage);

	} else if (obj->type == DKP_DEVICE_TYPE_KEYBOARD) {
		title = _("Keyboard battery low");
		message = g_strdup_printf (_("The wireless keyboard attached to this computer is low in power (%.1f%%)"), obj->percentage);

	} else if (obj->type == DKP_DEVICE_TYPE_PDA) {
		title = _("PDA battery low");
		message = g_strdup_printf (_("The PDA attached to this computer is low in power (%.1f%%)"), obj->percentage);

	} else if (obj->type == DKP_DEVICE_TYPE_PHONE) {
		title = _("Cell phone battery low");
		message = g_strdup_printf (_("The cell phone attached to this computer is low in power (%.1f%%)"), obj->percentage);
	}

	/* get correct icon */
	icon = gpm_devicekit_get_object_icon (obj);
	gpm_notify_display (manager->priv->notify, title, message, GPM_NOTIFY_TIMEOUT_LONG, icon, GPM_NOTIFY_URGENCY_NORMAL);
	gpm_manager_play (manager, GPM_MANAGER_SOUND_BATTERY_LOW, TRUE);
	g_free (icon);
	g_free (message);
}

/**
 * gpm_manager_get_time_until_action_text:
 */
static gchar *
gpm_manager_get_time_until_action_text (GpmManager *manager)
{
	guint time_s = 0;
	return gpm_get_timestring (time_s);
}

/**
 * gpm_engine_charge_critical_cb:
 */
static void
gpm_engine_charge_critical_cb (GpmEngine *engine, DkpObject *obj, GpmManager *manager)
{
	const gchar *title = NULL;
	gchar *message = NULL;
	gchar *action_text = NULL;
	gchar *remaining_text;
	gchar *action;
	gchar *icon;
	gchar *time_text;

	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		title = _("Laptop battery critically low");
		remaining_text = gpm_get_timestring (obj->time_to_empty);
		time_text = gpm_manager_get_time_until_action_text (manager);

		/* we have to do different warnings depending on the policy */
		action = gconf_client_get_string (manager->priv->conf, GPM_CONF_ACTIONS_CRITICAL_BATT, NULL);
		if (action == NULL) {
			egg_warning ("schema invalid!");
			action = g_strdup (ACTION_NOTHING);
		}

		/* use different text for different actions */
		if (strcmp (action, ACTION_NOTHING) == 0)
			action_text = g_strdup (_("Plug in your AC adapter to avoid losing data."));
		else if (strcmp (action, ACTION_SUSPEND) == 0)
			action_text = g_strdup_printf (_("This computer will suspend in %s if the AC is not connected."), time_text);
		else if (strcmp (action, ACTION_HIBERNATE) == 0)
			action_text = g_strdup_printf (_("This computer will hibernate in %s if the AC is not connected."), time_text);
		else if (strcmp (action, ACTION_SHUTDOWN) == 0)
			action_text = g_strdup_printf (_("This computer will shutdown in %s if the AC is not connected."), time_text);

		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining battery life (%.1f%%). %s"),
					   remaining_text, obj->percentage, action_text);

		g_free (action);
		g_free (action_text);
		g_free (remaining_text);
		g_free (time_text);
	} else if (obj->type == DKP_DEVICE_TYPE_UPS) {
		title = _("UPS critically low");
		remaining_text = gpm_get_timestring (obj->time_to_empty);
		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining UPS power (%.1f%%). "
					     "Restore AC power to your computer to avoid losing data."),
					   remaining_text, obj->percentage);
		g_free (remaining_text);
	} else if (obj->type == DKP_DEVICE_TYPE_MOUSE) {
		title = _("Mouse battery low");
		message = g_strdup_printf (_("The wireless mouse attached to this computer is very low in power (%.1f%%). "
					     "This device will soon stop functioning if not charged."),
					   obj->percentage);
	} else if (obj->type == DKP_DEVICE_TYPE_KEYBOARD) {
		title = _("Keyboard battery low");
		message = g_strdup_printf (_("The wireless keyboard attached to this computer is very low in power (%.1f%%). "
					     "This device will soon stop functioning if not charged."),
					   obj->percentage);
	} else if (obj->type == DKP_DEVICE_TYPE_PDA) {
		title = _("PDA battery low");
		message = g_strdup_printf (_("The PDA attached to this computer is very low in power (%.1f%%). "
					     "This device will soon stop functioning if not charged."),
					   obj->percentage);
	} else if (obj->type == DKP_DEVICE_TYPE_PHONE) {
		title = _("Cell phone battery low");
		message = g_strdup_printf (_("Your cell phone is very low in power (%.1f%%). "
					     "This device will soon stop functioning if not charged."),
					   obj->percentage);
	}

	/* get correct icon */
	icon = gpm_devicekit_get_object_icon (obj);
	gpm_notify_display (manager->priv->notify, title, message, GPM_NOTIFY_TIMEOUT_LONG, icon, GPM_NOTIFY_URGENCY_CRITICAL);
	gpm_manager_play (manager, GPM_MANAGER_SOUND_BATTERY_LOW, TRUE);
	g_free (icon);
	g_free (message);
}

/**
 * gpm_engine_charge_action_cb:
 */
static void
gpm_engine_charge_action_cb (GpmEngine *engine, DkpObject *obj, GpmManager *manager)
{
	const gchar *title = NULL;
	gchar *action;
	gchar *message = NULL;
	gchar *icon;

	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		title = _("Laptop battery critically low");

		/* we have to do different warnings depending on the policy */
		action = gconf_client_get_string (manager->priv->conf, GPM_CONF_ACTIONS_CRITICAL_BATT, NULL);

		/* use different text for different actions */
		if (strcmp (action, ACTION_NOTHING) == 0) {
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer will <b>power-off</b> when the "
					      "battery becomes completely empty."));

		} else if (strcmp (action, ACTION_SUSPEND) == 0) {
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer is about to suspend.<br>"
					      "<b>NOTE:</b> A small amount of power is required "
					      "to keep your computer in a suspended state."));

		} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer is about to hibernate."));

		} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer is about to shutdown."));
		}

		g_free (action);

		/* wait 10 seconds for user-panic */
		g_timeout_add_seconds (10, (GSourceFunc) manager_critical_action_do, manager);

	} else if (obj->type == DKP_DEVICE_TYPE_UPS) {
		title = _("UPS critically low");

		/* we have to do different warnings depending on the policy */
		action = gconf_client_get_string (manager->priv->conf, GPM_CONF_ACTIONS_CRITICAL_UPS, NULL);

		/* use different text for different actions */
		if (strcmp (action, ACTION_NOTHING) == 0) {
			message = g_strdup (_("The UPS is below the critical level and "
				              "this computer will <b>power-off</b> when the "
				              "UPS becomes completely empty."));

		} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
			message = g_strdup (_("The UPS is below the critical level and "
				              "this computer is about to hibernate."));

		} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
			message = g_strdup (_("The UPS is below the critical level and "
				              "this computer is about to shutdown."));
		}

		g_free (action);
	}

	/* not all types have actions */
	if (title == NULL)
		return;

	/* get correct icon */
	icon = gpm_devicekit_get_object_icon (obj);
	gpm_notify_display (manager->priv->notify,
			    title, message, GPM_NOTIFY_TIMEOUT_LONG,
			    icon, GPM_NOTIFY_URGENCY_CRITICAL);
	gpm_manager_play (manager, GPM_MANAGER_SOUND_BATTERY_LOW, TRUE);
	g_free (icon);
	g_free (message);
}

/**
 * has_inhibit_changed_cb:
 **/
static void
has_inhibit_changed_cb (GpmInhibit *inhibit, gboolean has_inhibit, GpmManager *manager)
{
	gboolean is_laptop = FALSE;
	gboolean show_inhibit_lid;
	gchar *action = NULL;

	/* we don't care about uninhibits */
	if (has_inhibit == FALSE)
		return;

	/* only show this if specified in gconf */
	show_inhibit_lid = gconf_client_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_INHIBIT_LID, NULL);

	/* we've already shown the UI and been clicked */
	if (show_inhibit_lid == FALSE)
		return;

/* FIXME: detect if laptop */

	/* we don't warn for desktops, as they do not have a lid... */
	if (is_laptop == FALSE)
		return;

	/* get the policy action for battery */
	action = gconf_client_get_string (manager->priv->conf, GPM_CONF_BUTTON_LID_BATT, NULL);
	if (action == NULL)
		return;

	/* if the policy on lid close is sleep then show a warning */
	if ((strcmp (action, ACTION_SUSPEND) == 0) ||
	    (strcmp (action, ACTION_HIBERNATE) == 0))
		gpm_notify_inhibit_lid (manager->priv->notify);

	g_free (action);
}

/**
 * brightness_kbd_changed_cb:
 * @brightness: The GpmBrightnessKbd class instance
 * @percentage: The new percentage brightness
 * @brightness: This class instance
 *
 * This callback is called when the brightness value changes.
 **/
static void
brightness_kbd_changed_cb (GpmBrightnessKbd *brightness, gint percentage, GpmManager *manager)
{
	egg_debug ("Need to display backlight feedback value %i", percentage);
	gpm_feedback_display_value (manager->priv->feedback_kbd, (float) percentage / 100.0f);
}

/**
 * dpms_mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @info: This class instance
 *
 * Log when the DPMS mode is changed.
 **/
static void
dpms_mode_changed_cb (GpmDpms *dpms, GpmDpmsMode mode, GpmManager *manager)
{
	egg_debug ("DPMS mode changed: %d", mode);

	if (mode == GPM_DPMS_MODE_ON)
		egg_debug ("dpms on");
	else if (mode == GPM_DPMS_MODE_STANDBY)
		egg_debug ("dpms standby");
	else if (mode == GPM_DPMS_MODE_SUSPEND)
		egg_debug ("suspend");
	else if (mode == GPM_DPMS_MODE_OFF)
		egg_debug ("dpms off");

	update_dpms_throttle (manager);
}

/**
 * gpm_manager_console_kit_active_changed_cb:
 **/
static void
gpm_manager_console_kit_active_changed_cb (EggConsoleKit *console, gboolean active, GpmManager *manager)
{
	egg_debug ("console now %s", active ? "active" : "inactive");
	/* FIXME: do we need to do policy actions when we switch? */
}

/**
 * gpm_manager_init:
 * @manager: This class instance
 **/
static void
gpm_manager_init (GpmManager *manager)
{
	gboolean check_type_cpu;
	DBusGConnection *connection;
	GError *error = NULL;
	gboolean on_ac;
	guint version;

	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	/* init to unthrottled */
	manager->priv->screensaver_ac_throttle_id = 0;
	manager->priv->screensaver_dpms_throttle_id = 0;
	manager->priv->screensaver_lid_throttle_id = 0;

	/* don't apply policy when not active */
	manager->priv->console = egg_console_kit_new ();
	g_signal_connect (manager->priv->console, "active-changed",
			  G_CALLBACK (gpm_manager_console_kit_active_changed_cb), manager);

	/* this is a singleton, so we keep a master copy open here */
	manager->priv->prefs_server = gpm_prefs_server_new ();

	manager->priv->notify = gpm_notify_new ();
	manager->priv->conf = gconf_client_get_default ();

	/* watch gnome-power-manager keys */
	gconf_client_add_dir (manager->priv->conf, GPM_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (manager->priv->conf, GPM_CONF_DIR,
				 (GConfClientNotifyFunc) gpm_conf_gconf_key_changed_cb,
				 manager, NULL, NULL);

	/* check to see if the user has installed the schema properly */
	version = gconf_client_get_int (manager->priv->conf, GPM_CONF_SCHEMA_VERSION, NULL);
	if (version != GPM_CONF_SCHEMA_ID) {
		gpm_notify_display (manager->priv->notify,
				    _("Install problem!"),
				    _("The configuration defaults for GNOME Power Manager have not been installed correctly.\n"
				      "Please contact your computer administrator."),
				    GPM_NOTIFY_TIMEOUT_LONG,
				    GTK_STOCK_DIALOG_WARNING,
				    GPM_NOTIFY_URGENCY_NORMAL);
		egg_error ("no gconf schema installed!");
	}

	/* we use ac_adapter so we can poke the screensaver and throttle */
	manager->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (manager->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), manager);

	/* coldplug so we are in the correct state at startup */
	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);
	if (on_ac)
		manager->priv->low_power = gconf_client_get_bool (manager->priv->conf, GPM_CONF_LOWPOWER_AC, NULL);
	else
		manager->priv->low_power = gconf_client_get_bool (manager->priv->conf, GPM_CONF_LOWPOWER_BATT, NULL);

	manager->priv->button = gpm_button_new ();
	g_signal_connect (manager->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), manager);

	/* try and start an interactive service */
	manager->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (manager->priv->screensaver, "auth-request",
 			  G_CALLBACK (screensaver_auth_request_cb), manager);

	/* try an start an interactive service */
	manager->priv->backlight = gpm_backlight_new ();
	if (manager->priv->backlight != NULL) {
		/* add the new brightness lcd DBUS interface */
		dbus_g_object_type_install_info (GPM_TYPE_BACKLIGHT,
						 &dbus_glib_gpm_backlight_object_info);
		dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_BACKLIGHT,
						     G_OBJECT (manager->priv->backlight));
	}

	/* use a visual widget */
	manager->priv->feedback_kbd = gpm_feedback_new ();
	gpm_feedback_set_icon_name (manager->priv->feedback_kbd, GPM_STOCK_BRIGHTNESS_KBD);

	manager->priv->brightness_kbd = gpm_brightness_kbd_new ();
	g_signal_connect (manager->priv->brightness_kbd, "brightness-changed",
			  G_CALLBACK (brightness_kbd_changed_cb), manager);

	manager->priv->idle = gpm_idle_new ();
	g_signal_connect (manager->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), manager);

	/* set up the check_type_cpu, so we can disable the CPU load check */
	check_type_cpu = gconf_client_get_bool (manager->priv->conf, GPM_CONF_IDLE_CHECK_CPU, NULL);
	gpm_idle_set_check_cpu (manager->priv->idle, check_type_cpu);

	manager->priv->dpms = gpm_dpms_new ();
	g_signal_connect (manager->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), manager);

	/* use a class to handle the complex stuff */
	egg_debug ("creating new inhibit instance");
	manager->priv->inhibit = gpm_inhibit_new ();
	g_signal_connect (manager->priv->inhibit, "has-inhibit-changed",
			  G_CALLBACK (has_inhibit_changed_cb), manager);
	/* add the interface */
	dbus_g_object_type_install_info (GPM_TYPE_INHIBIT, &dbus_glib_gpm_inhibit_object_info);
	dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_INHIBIT,
					     G_OBJECT (manager->priv->inhibit));

	/* use the control object */
	egg_debug ("creating new control instance");
	manager->priv->control = gpm_control_new ();
	g_signal_connect (manager->priv->control, "sleep-failure",
			  G_CALLBACK (control_sleep_failure_cb), manager);

	egg_debug ("creating new tray icon");
	manager->priv->tray_icon = gpm_tray_icon_new ();
	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "suspend", G_CALLBACK (gpm_manager_tray_icon_suspend),
				 manager, G_CONNECT_SWAPPED);
	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "hibernate", G_CALLBACK (gpm_manager_tray_icon_hibernate),
				 manager, G_CONNECT_SWAPPED);

	gpm_manager_sync_policy_sleep (manager);

	manager->priv->engine = gpm_engine_new ();
	g_signal_connect (manager->priv->engine, "perhaps-recall",
			  G_CALLBACK (gpm_engine_perhaps_recall_cb), manager);
	g_signal_connect (manager->priv->engine, "low-capacity",
			  G_CALLBACK (gpm_engine_low_capacity_cb), manager);
	g_signal_connect (manager->priv->engine, "icon-changed",
			  G_CALLBACK (gpm_engine_icon_changed_cb), manager);
	g_signal_connect (manager->priv->engine, "summary-changed",
			  G_CALLBACK (gpm_engine_summary_changed_cb), manager);
	g_signal_connect (manager->priv->engine, "fully-charged",
			  G_CALLBACK (gpm_engine_fully_charged_cb), manager);
	g_signal_connect (manager->priv->engine, "discharging",
			  G_CALLBACK (gpm_engine_discharging_cb), manager);
	g_signal_connect (manager->priv->engine, "charge-low",
			  G_CALLBACK (gpm_engine_charge_low_cb), manager);
	g_signal_connect (manager->priv->engine, "charge-critical",
			  G_CALLBACK (gpm_engine_charge_critical_cb), manager);
	g_signal_connect (manager->priv->engine, "charge-action",
			  G_CALLBACK (gpm_engine_charge_action_cb), manager);

	/* update ac throttle */
	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);
	update_ac_throttle (manager, on_ac);
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

	g_object_unref (manager->priv->conf);
	g_object_unref (manager->priv->dpms);
	g_object_unref (manager->priv->idle);
	g_object_unref (manager->priv->engine);
	g_object_unref (manager->priv->tray_icon);
	g_object_unref (manager->priv->inhibit);
	g_object_unref (manager->priv->screensaver);
	g_object_unref (manager->priv->notify);
	g_object_unref (manager->priv->prefs_server);
	g_object_unref (manager->priv->control);
	g_object_unref (manager->priv->feedback_kbd);
	g_object_unref (manager->priv->button);
	g_object_unref (manager->priv->brightness_kbd);
	g_object_unref (manager->priv->backlight);
	g_object_unref (manager->priv->console);

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
