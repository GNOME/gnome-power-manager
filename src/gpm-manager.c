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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include <libupower-glib/upower.h>
#include <libnotify/notify.h>

#include "egg-console-kit.h"

#include "gpm-button.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "gpm-dpms.h"
#include "gpm-idle.h"
#include "gpm-manager.h"
#include "gpm-screensaver.h"
#include "gpm-backlight.h"
#include "gpm-stock-icons.h"
#include "gpm-upower.h"

static void     gpm_manager_finalize	(GObject	 *object);

#define GPM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_MANAGER, GpmManagerPrivate))
#define GPM_MANAGER_RECALL_DELAY		30 /* seconds */
#define GPM_MANAGER_NOTIFY_TIMEOUT_NEVER	0 /* ms */
#define GPM_MANAGER_NOTIFY_TIMEOUT_SHORT	10 * 1000 /* ms */
#define GPM_MANAGER_NOTIFY_TIMEOUT_LONG		30 * 1000 /* ms */

#define GPM_MANAGER_CRITICAL_ALERT_TIMEOUT      5 /* seconds */

struct GpmManagerPrivate
{
	GpmButton		*button;
	GSettings		*settings;
	GSettings		*settings_gsd;
	GpmDpms			*dpms;
	GpmIdle			*idle;
	GpmControl		*control;
	GpmScreensaver		*screensaver;
	GpmBacklight		*backlight;
	EggConsoleKit		*console;
	guint32			 screensaver_ac_throttle_id;
	guint32			 screensaver_dpms_throttle_id;
	guint32			 screensaver_lid_throttle_id;
	UpClient		*client;
	gboolean		 on_battery;
	gboolean		 just_resumed;
	GDBusConnection		*bus_connection;
	guint 			 bus_owner_id;
	guint			 bus_object_id;
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
 * gpm_manager_is_inhibit_valid:
 * @manager: This class instance
 * @action: The action we want to do, e.g. "suspend"
 *
 * Checks to see if the specific action has been inhibited by a program.
 *
 * Return value: TRUE if we can perform the action.
 **/
static gboolean
gpm_manager_is_inhibit_valid (GpmManager *manager, gboolean user_action, const char *action)
{
	return TRUE;
}

/**
 * gpm_manager_sync_policy_sleep:
 * @manager: This class instance
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

	if (!manager->priv->on_battery) {
		sleep_computer = g_settings_get_int (manager->priv->settings_gsd, GSD_SETTINGS_SLEEP_COMPUTER_AC);
		/* hack around new gsettings key */
		if (!g_settings_get_boolean (manager->priv->settings_gsd, GSD_SETTINGS_SLEEP_COMPUTER_AC_EN))
			sleep_computer = 0;
		sleep_display = g_settings_get_int (manager->priv->settings_gsd, GSD_SETTINGS_SLEEP_DISPLAY_AC);
	} else {
		sleep_computer = g_settings_get_int (manager->priv->settings_gsd, GSD_SETTINGS_SLEEP_COMPUTER_BATT);
		/* hack around new gsettings key */
		if (!g_settings_get_boolean (manager->priv->settings_gsd, GSD_SETTINGS_SLEEP_COMPUTER_BATT_EN))
			sleep_computer = 0;
		sleep_display = g_settings_get_int (manager->priv->settings_gsd, GSD_SETTINGS_SLEEP_DISPLAY_BATT);
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
					       GPM_SETTINGS_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		if (!gpm_screensaver_lock (manager->priv->screensaver))
			g_debug ("Could not lock screen via gnome-screensaver");
	}
	gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_OFF, &error);
	if (error) {
		g_debug ("Unable to set DPMS mode: %s", error->message);
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
		g_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	do_lock = gpm_control_get_lock_policy (manager->priv->control, GPM_SETTINGS_LOCK_ON_BLANK_SCREEN);
	if (do_lock)
		gpm_screensaver_poke (manager->priv->screensaver);
	return ret;
}

/**
 * gpm_manager_sleep_failure_response_cb:
 **/
static void
gpm_manager_sleep_failure_response_cb (GtkDialog *dialog, gint response_id, GpmManager *manager)
{
	GdkScreen *screen;
	GtkWidget *dialog_error;
	GError *error = NULL;
	gboolean ret;
	gchar *uri = NULL;

	/* user clicked the help button */
	if (response_id == GTK_RESPONSE_HELP) {
		uri = g_settings_get_string (manager->priv->settings, GPM_SETTINGS_NOTIFY_SLEEP_FAILED_URI);
		screen = gdk_screen_get_default();
		ret = gtk_show_uri (screen, uri, gtk_get_current_event_time (), &error);
		if (!ret) {
			dialog_error = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
							       "Failed to show uri %s", error->message);
			gtk_dialog_run (GTK_DIALOG (dialog_error));
			g_error_free (error);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_free (uri);
}

/**
 * gpm_manager_sleep_failure:
 **/
static void
gpm_manager_sleep_failure (GpmManager *manager, gboolean is_suspend, const gchar *detail)
{
	gboolean show_sleep_failed;
	GString *string = NULL;
	const gchar *title;
	gchar *uri = NULL;
	const gchar *icon;
	GtkWidget *dialog;

	/* only show this if specified in settings */
	show_sleep_failed = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_NOTIFY_SLEEP_FAILED);

	/* only emit if in GConf */
	if (!show_sleep_failed)
		goto out;

	/* TRANSLATORS: window title: there was a problem putting the machine to sleep */
	string = g_string_new ("");
	if (is_suspend) {
		/* TRANSLATORS: message text */
		g_string_append (string, _("Computer failed to suspend."));
		/* TRANSLATORS: title text */
		title = _("Failed to suspend");
		icon = GPM_STOCK_SUSPEND;
	} else {
		/* TRANSLATORS: message text */
		g_string_append (string, _("Computer failed to hibernate."));
		/* TRANSLATORS: title text */
		title = _("Failed to hibernate");
		icon = GPM_STOCK_HIBERNATE;
	}

	/* TRANSLATORS: message text */
	g_string_append_printf (string, "\n\n%s %s", _("Failure was reported as:"), detail);

	/* show modal dialog */
	dialog = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
						     "<span size='larger'><b>%s</b></span>", title);
	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	gtk_window_set_icon_name (GTK_WINDOW(dialog), icon);

	/* show a button? */
	uri = g_settings_get_string (manager->priv->settings, GPM_SETTINGS_NOTIFY_SLEEP_FAILED_URI);
	if (uri != NULL && uri[0] != '\0') {
		/* TRANSLATORS: button text, visit the suspend help website */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Visit help page"), GTK_RESPONSE_HELP);
	}

	/* wait async for close */
	gtk_widget_show (dialog);
	g_signal_connect (dialog, "response", G_CALLBACK (gpm_manager_sleep_failure_response_cb), manager);
out:
	g_free (uri);
	g_string_free (string, TRUE);
}

/**
 * gpm_manager_action_suspend:
 **/
static gboolean
gpm_manager_action_suspend (GpmManager *manager, const gchar *reason)
{
	gboolean ret;
	GError *error = NULL;

	/* check to see if we are inhibited */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "suspend") == FALSE)
		return FALSE;

	g_debug ("suspending, reason: %s", reason);
	ret = gpm_control_suspend (manager->priv->control, &error);
	if (!ret) {
		gpm_manager_sleep_failure (manager, TRUE, error->message);
		g_error_free (error);
	}
	gpm_button_reset_time (manager->priv->button);
	return TRUE;
}

/**
 * gpm_manager_action_hibernate:
 **/
static gboolean
gpm_manager_action_hibernate (GpmManager *manager, const gchar *reason)
{
	gboolean ret;
	GError *error = NULL;

	/* check to see if we are inhibited */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "hibernate") == FALSE)
		return FALSE;

	g_debug ("hibernating, reason: %s", reason);
	ret = gpm_control_hibernate (manager->priv->control, &error);
	if (!ret) {
		gpm_manager_sleep_failure (manager, TRUE, error->message);
		g_error_free (error);
	}
	gpm_button_reset_time (manager->priv->button);
	return TRUE;
}

/**
 * gpm_manager_logout:
 **/
static gboolean
gpm_manager_logout (GpmManager *manager)
{
	gboolean ret = FALSE;
	GVariant *retval = NULL;
	GError *error = NULL;
	GDBusProxy *proxy = NULL;
	GDBusConnection *connection;

	/* connect to gnome-session */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		g_warning ("Failed to connect to the session: %s", error->message);
		g_error_free (error);
		goto out;
	}
	proxy = g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
			G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
			NULL,
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager",
			"org.gnome.SessionManager",
			NULL, &error);
	if (proxy == NULL) {
		g_warning ("Failed to shutdown session: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* ask to shut it down */
	retval = g_dbus_proxy_call_sync (proxy,
					 "Shutdown",
					 NULL, G_DBUS_CALL_FLAGS_NONE,
					 -1, NULL, &error);
	if (retval == NULL) {
		g_debug ("Failed to shutdown session: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (retval != NULL)
		g_variant_unref (retval);
	if (proxy != NULL)
		g_object_unref (proxy);
	return ret;
}

/**
 * gpm_manager_perform_policy:
 * @manager: This class instance
 * @policy: The policy that we should do, e.g. "suspend"
 * @reason: The reason we are performing the policy action, e.g. "battery critical"
 *
 * Does one of the policy actions specified in the settings.
 **/
static gboolean
gpm_manager_perform_policy (GpmManager  *manager, const gchar *policy_key, const gchar *reason)
{
	GpmActionPolicy policy;

	/* are we inhibited? */
	if (gpm_manager_is_inhibit_valid (manager, FALSE, "policy action") == FALSE)
		return FALSE;

	policy = g_settings_get_enum (manager->priv->settings_gsd, policy_key);
	g_debug ("action: %s set to %i (%s)", policy_key, policy, reason);
	if (policy == GPM_ACTION_POLICY_NOTHING) {
		g_debug ("doing nothing, reason: %s", reason);
	} else if (policy == GPM_ACTION_POLICY_SUSPEND) {
		gpm_manager_action_suspend (manager, reason);

	} else if (policy == GPM_ACTION_POLICY_HIBERNATE) {
		gpm_manager_action_hibernate (manager, reason);

	} else if (policy == GPM_ACTION_POLICY_BLANK) {
		gpm_manager_blank_screen (manager, NULL);

	} else if (policy == GPM_ACTION_POLICY_SHUTDOWN) {
		g_debug ("shutting down, reason: %s", reason);
		gpm_control_shutdown (manager->priv->control, NULL);

	} else if (policy == GPM_ACTION_POLICY_INTERACTIVE) {
		g_debug ("logout, reason: %s", reason);
		gpm_manager_logout (manager);
	} else {
		g_warning ("unknown action %i", policy);
	}

	return TRUE;
}

/**
 * gpm_manager_idle_do_sleep:
 * @manager: This class instance
 *
 * This callback is called when we want to sleep. Use the users
 * preference from the settings, but change it if we can't do the action.
 **/
static void
gpm_manager_idle_do_sleep (GpmManager *manager)
{
	gboolean ret;
	GError *error = NULL;
	GpmActionPolicy policy;

	if (!manager->priv->on_battery)
		policy = g_settings_get_enum (manager->priv->settings_gsd, GSD_SETTINGS_ACTION_SLEEP_TYPE_AC);
	else
		policy = g_settings_get_enum (manager->priv->settings_gsd, GSD_SETTINGS_ACTION_SLEEP_TYPE_BATT);

	if (policy == GPM_ACTION_POLICY_NOTHING) {
		g_debug ("doing nothing as system idle action");

	} else if (policy == GPM_ACTION_POLICY_SUSPEND) {
		g_debug ("suspending, reason: System idle");
		ret = gpm_control_suspend (manager->priv->control, &error);
		if (!ret) {
			g_warning ("cannot suspend (error: %s), so trying hibernate", error->message);
			g_error_free (error);
			error = NULL;
			ret = gpm_control_hibernate (manager->priv->control, &error);
			if (!ret) {
				g_warning ("cannot suspend or hibernate: %s", error->message);
				g_error_free (error);
			}
		}

	} else if (policy == GPM_ACTION_POLICY_HIBERNATE) {
		g_debug ("hibernating, reason: System idle");
		ret = gpm_control_hibernate (manager->priv->control, &error);
		if (!ret) {
			g_warning ("cannot hibernate (error: %s), so trying suspend", error->message);
			g_error_free (error);
			error = NULL;
			ret = gpm_control_suspend (manager->priv->control, &error);
			if (!ret) {
				g_warning ("cannot suspend or hibernate: %s", error->message);
				g_error_free (error);
			}
		}
	}
}

/**
 * gpm_manager_is_active:
 **/
static gboolean
gpm_manager_is_active (GpmManager *manager)
{
	gboolean ret;
	gboolean is_active = TRUE;
	GError *error = NULL;

	/* if we fail, assume we are on active console */
	ret = egg_console_kit_is_active (manager->priv->console, &is_active, &error);
	if (!ret) {
		g_warning ("failed to get active status: %s", error->message);
		g_error_free (error);
	}
	return is_active;
}

/**
 * gpm_manager_idle_changed_cb:
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
gpm_manager_idle_changed_cb (GpmIdle *idle, GpmIdleMode mode, GpmManager *manager)
{
	/* ConsoleKit says we are not on active console */
	if (!gpm_manager_is_active (manager)) {
		g_debug ("ignoring as not on active console");
		return;
	}

	/* Ignore back-to-NORMAL events when the lid is closed, as the DPMS is
	 * already off, and we don't want to re-enable the screen when the user
	 * moves the mouse on systems that do not support hardware blanking. */
	if (gpm_button_is_lid_closed (manager->priv->button) &&
	    mode == GPM_IDLE_MODE_NORMAL) {
		g_debug ("lid is closed, so we are ignoring ->NORMAL state changes");
		return;
	}

	if (mode == GPM_IDLE_MODE_SLEEP) {
		g_debug ("Idle state changed: SLEEP");
		if (gpm_manager_is_inhibit_valid (manager, FALSE, "timeout action") == FALSE)
			return;
		gpm_manager_idle_do_sleep (manager);
	}
}

/**
 * gpm_manager_lid_button_pressed:
 * @manager: This class instance
 * @state: TRUE for closed
 *
 * Does actions when the lid is closed, depending on if we are on AC or
 * battery power.
 **/
static void
gpm_manager_lid_button_pressed (GpmManager *manager, gboolean pressed)
{
	/* we turn the lid dpms back on unconditionally */
	if (pressed == FALSE) {
		gpm_manager_unblank_screen (manager, NULL);
		return;
	}
}

static void
gpm_manager_update_dpms_throttle (GpmManager *manager)
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
		/* TRANSLATORS: this is the gnome-screensaver throttle */
		manager->priv->screensaver_dpms_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("Display DPMS activated"));
	}
}

static void
gpm_manager_update_ac_throttle (GpmManager *manager)
{
	/* Throttle the manager when we are not on AC power so we don't
	   waste the battery */
	if (!manager->priv->on_battery) {
		if (manager->priv->screensaver_ac_throttle_id != 0) {
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_ac_throttle_id);
			manager->priv->screensaver_ac_throttle_id = 0;
		}
	} else {
		/* if throttle already exists then remove */
		if (manager->priv->screensaver_ac_throttle_id != 0)
			gpm_screensaver_remove_throttle (manager->priv->screensaver, manager->priv->screensaver_ac_throttle_id);
		/* TRANSLATORS: this is the gnome-screensaver throttle */
		manager->priv->screensaver_ac_throttle_id = gpm_screensaver_add_throttle (manager->priv->screensaver, _("On battery power"));
	}
}

static void
gpm_manager_update_lid_throttle (GpmManager *manager, gboolean lid_is_closed)
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
 * gpm_manager_button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, e.g. "power"
 * @state: The state, where TRUE is depressed or closed
 * @manager: This class instance
 **/
static void
gpm_manager_button_pressed_cb (GpmButton *button, const gchar *type, GpmManager *manager)
{
	g_debug ("Button press event type=%s", type);

	/* ConsoleKit says we are not on active console */
	if (!gpm_manager_is_active (manager)) {
		g_debug ("ignoring as not on active console");
		return;
	}

	if (g_strcmp0 (type, GPM_BUTTON_LID_OPEN) == 0) {
		gpm_manager_lid_button_pressed (manager, FALSE);
	} else if (g_strcmp0 (type, GPM_BUTTON_LID_CLOSED) == 0) {
		gpm_manager_lid_button_pressed (manager, TRUE);
	}

	/* disable or enable the fancy screensaver, as we don't want
	 * this starting when the lid is shut */
	if (g_strcmp0 (type, GPM_BUTTON_LID_CLOSED) == 0)
		gpm_manager_update_lid_throttle (manager, TRUE);
	else if (g_strcmp0 (type, GPM_BUTTON_LID_OPEN) == 0)
		gpm_manager_update_lid_throttle (manager, FALSE);
}

/**
 * gpm_manager_client_changed_cb:
 **/
static void
gpm_manager_client_changed_cb (UpClient *client, GpmManager *manager)
{
	gboolean on_battery;
	gboolean lid_is_closed;

	/* get the client state */
	g_object_get (client,
		      "on-battery", &on_battery,
		      "lid-is-closed", &lid_is_closed,
		      NULL);
	if (on_battery == manager->priv->on_battery) {
		g_debug ("same state as before, ignoring");
		return;
	}

	/* save in local cache */
	manager->priv->on_battery = on_battery;

	/* ConsoleKit says we are not on active console */
	if (!gpm_manager_is_active (manager)) {
		g_debug ("ignoring as not on active console");
		return;
	}

	g_debug ("on_battery: %d", on_battery);

	gpm_manager_sync_policy_sleep (manager);

	gpm_manager_update_ac_throttle (manager);

	/* simulate user input, but only when the lid is open */
	if (!lid_is_closed)
		gpm_screensaver_poke (manager->priv->screensaver);

	/* We keep track of the lid state so we can do the
	 * lid close on battery action if the ac adapter is removed when the laptop
	 * is closed */
	if (on_battery && lid_is_closed) {
		gpm_manager_perform_policy (manager, GSD_SETTINGS_BUTTON_LID_BATT,
					    "The lid has been closed, and the ac adapter removed.");
	}
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
	g_type_class_add_private (klass, sizeof (GpmManagerPrivate));
}

/**
 * gpm_manager_settings_changed_cb:
 *
 * We might have to do things when the keys change; do them here.
 **/
static void
gpm_manager_settings_changed_cb (GSettings *settings, const gchar *key, GpmManager *manager)
{
	if (g_strcmp0 (key, GSD_SETTINGS_SLEEP_COMPUTER_BATT) == 0 ||
	    g_strcmp0 (key, GSD_SETTINGS_SLEEP_COMPUTER_AC) == 0 ||
	    g_strcmp0 (key, GSD_SETTINGS_SLEEP_COMPUTER_BATT_EN) == 0 ||
	    g_strcmp0 (key, GSD_SETTINGS_SLEEP_COMPUTER_AC_EN) == 0 ||
	    g_strcmp0 (key, GSD_SETTINGS_SLEEP_DISPLAY_BATT) == 0 ||
	    g_strcmp0 (key, GSD_SETTINGS_SLEEP_DISPLAY_AC) == 0)
		gpm_manager_sync_policy_sleep (manager);
}

#if 0
/**
 * gpm_manager_screensaver_auth_request_cb:
 * @manager: This manager class instance
 * @auth: If we are trying to authenticate
 *
 * Called when the user is trying or has authenticated
 **/
static void
gpm_manager_screensaver_auth_request_cb (GpmScreensaver *screensaver, gboolean auth_begin, GpmManager *manager)
{
	GError *error = NULL;

	if (auth_begin) {
		/* We turn on the monitor unconditionally, as we may be using
		 * a smartcard to authenticate and DPMS might still be on.
		 * See #350291 for more details */
		gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_ON, &error);
		if (error != NULL) {
			g_warning ("Failed to turn on DPMS: %s", error->message);
			g_error_free (error);
			error = NULL;
		}
	}
}
#endif

#if 0
/**
 * gpm_manager_perhaps_recall_response_cb:
 */
static void
gpm_manager_perhaps_recall_response_cb (GtkDialog *dialog, gint response_id, GpmManager *manager)
{
	GdkScreen *screen;
	GtkWidget *dialog_error;
	GError *error = NULL;
	gboolean ret;
	const gchar *website;

	/* don't show this again */
	if (response_id == GTK_RESPONSE_CANCEL) {
		g_settings_set_boolean (manager->priv->settings, GPM_SETTINGS_NOTIFY_PERHAPS_RECALL, FALSE);
		goto out;
	}

	/* visit recall website */
	if (response_id == GTK_RESPONSE_OK) {
		screen = gdk_screen_get_default();
		website = (const gchar *) g_object_get_data (G_OBJECT (manager), "recall-oem-website");
		ret = gtk_show_uri (screen, website, gtk_get_current_event_time (), &error);
		if (!ret) {
			dialog_error = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
							       "Failed to show url %s", error->message);
			gtk_dialog_run (GTK_DIALOG (dialog_error));
			g_error_free (error);
		}
		goto out;
	}
out:
	gtk_widget_destroy (GTK_WIDGET (dialog));
	return;
}

/**
 * gpm_manager_perhaps_recall_delay_cb:
 */
static gboolean
gpm_manager_perhaps_recall_delay_cb (GpmManager *manager)
{
	const gchar *oem_vendor;
	gchar *title = NULL;
	gchar *message = NULL;
	GtkWidget *dialog;

	oem_vendor = (const gchar *) g_object_get_data (G_OBJECT (manager), "recall-oem-vendor");

	/* TRANSLATORS: the battery may be recalled by it's vendor */
	title = g_strdup_printf ("%s: %s", GPM_NAME, _("Battery may be recalled"));
	message = g_strdup_printf (_("A battery in your computer may have been "
				     "recalled by %s and you may be at risk.\n\n"
				     "For more information visit the battery recall website."), oem_vendor);
	dialog = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
						     "<span size='larger'><b>%s</b></span>", title);

	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), "%s", message);

	/* TRANSLATORS: button text, visit the manufacturers recall website */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Visit recall website"), GTK_RESPONSE_OK);

	/* TRANSLATORS: button text, do not show this bubble again */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Do not show me this again"), GTK_RESPONSE_CANCEL);

	/* wait async for response */
	gtk_widget_show (dialog);
	g_signal_connect (dialog, "response", G_CALLBACK (gpm_manager_perhaps_recall_response_cb), manager);

	g_free (title);
	g_free (message);

	/* never repeat */
	return FALSE;
}

/**
 * gpm_engine_check_recall:
 **/
static gboolean
gpm_engine_check_recall (GpmEngine *engine, UpDevice *device)
{
	UpDeviceKind kind;
	gboolean recall_notice = FALSE;
	gchar *recall_vendor = NULL;
	gchar *recall_url = NULL;

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "recall-notice", &recall_notice,
		      "recall-vendor", &recall_vendor,
		      "recall-url", &recall_url,
		      NULL);

	/* not battery */
	if (kind != UP_DEVICE_KIND_BATTERY)
		goto out;

	/* no recall data */
	if (!recall_notice)
		goto out;

	/* emit signal for manager */
	g_debug ("** EMIT: perhaps-recall");
out:
	g_free (recall_vendor);
	g_free (recall_url);
	return recall_notice;
}

#endif

/**
 * gpm_manager_dpms_mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @info: This class instance
 *
 * Log when the DPMS mode is changed.
 **/
static void
gpm_manager_dpms_mode_changed_cb (GpmDpms *dpms, GpmDpmsMode mode, GpmManager *manager)
{
	g_debug ("DPMS mode changed: %d", mode);

	if (mode == GPM_DPMS_MODE_ON)
		g_debug ("dpms on");
	else if (mode == GPM_DPMS_MODE_STANDBY)
		g_debug ("dpms standby");
	else if (mode == GPM_DPMS_MODE_SUSPEND)
		g_debug ("suspend");
	else if (mode == GPM_DPMS_MODE_OFF)
		g_debug ("dpms off");

	gpm_manager_update_dpms_throttle (manager);
}

/**
 * gpm_manager_reset_just_resumed_cb:
 **/
static gboolean
gpm_manager_reset_just_resumed_cb (gpointer user_data)
{
	GpmManager *manager = GPM_MANAGER (user_data);
#if 0
	if (manager->priv->notification_warning_low != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_warning_low);
	if (manager->priv->notification_discharging != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_discharging);
#endif

	manager->priv->just_resumed = FALSE;
	return FALSE;
}

/**
 * gpm_manager_control_resume_cb:
 **/
static void
gpm_manager_control_resume_cb (GpmControl *control, GpmControlAction action, GpmManager *manager)
{
	guint timer_id;
	manager->priv->just_resumed = TRUE;
	timer_id = g_timeout_add_seconds (1, gpm_manager_reset_just_resumed_cb, manager);
	g_source_set_name_by_id (timer_id, "[GpmManager] just-resumed");
}

/**
 * gpm_manager_bus_acquired_cb:
 **/
static void
gpm_manager_bus_acquired_cb (GDBusConnection *connection,
			    const gchar *name, gpointer user_data)
{
	GpmManager *manager = GPM_MANAGER (user_data);

	if (manager->priv->backlight != NULL) {
		gpm_backlight_register_dbus (manager->priv->backlight, connection);
	}
}

/**
 * gpm_manager_name_lost_cb:
 **/
static void
gpm_manager_name_lost_cb (GDBusConnection *connection,
			  const gchar *name,
			  gpointer user_data)
{
	g_warning ("name lost %s", name);
	exit (1);
}

/**
 * gpm_manager_init:
 * @manager: This class instance
 **/
static void
gpm_manager_init (GpmManager *manager)
{
	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);

	/* init to unthrottled */
	manager->priv->screensaver_ac_throttle_id = 0;
	manager->priv->screensaver_dpms_throttle_id = 0;
	manager->priv->screensaver_lid_throttle_id = 0;

	/* init to not just_resumed */
	manager->priv->just_resumed = FALSE;

	/* don't apply policy when not active, so listen to ConsoleKit */
	manager->priv->console = egg_console_kit_new ();

	manager->priv->settings = g_settings_new (GPM_SETTINGS_SCHEMA);
	g_signal_connect (manager->priv->settings, "changed",
			  G_CALLBACK (gpm_manager_settings_changed_cb), manager);
	manager->priv->settings_gsd = g_settings_new (GSD_SETTINGS_SCHEMA);
	g_signal_connect (manager->priv->settings_gsd, "changed",
			  G_CALLBACK (gpm_manager_settings_changed_cb), manager);
	manager->priv->client = up_client_new ();
	g_signal_connect (manager->priv->client, "changed",
			  G_CALLBACK (gpm_manager_client_changed_cb), manager);

	/* use libnotify */
	notify_init (GPM_NAME);

	/* coldplug so we are in the correct state at startup */
	g_object_get (manager->priv->client,
		      "on-battery", &manager->priv->on_battery,
		      NULL);

	manager->priv->button = gpm_button_new ();
	g_signal_connect (manager->priv->button, "button-pressed",
			  G_CALLBACK (gpm_manager_button_pressed_cb), manager);

	/* try and start an interactive service */
	manager->priv->screensaver = gpm_screensaver_new ();

	/* try an start an interactive service */
	manager->priv->backlight = gpm_backlight_new ();

	manager->priv->idle = gpm_idle_new ();
	g_signal_connect (manager->priv->idle, "idle-changed",
			  G_CALLBACK (gpm_manager_idle_changed_cb), manager);

	manager->priv->dpms = gpm_dpms_new ();
	g_signal_connect (manager->priv->dpms, "mode-changed",
			  G_CALLBACK (gpm_manager_dpms_mode_changed_cb), manager);

	/* use the control object */
	g_debug ("creating new control instance");
	manager->priv->control = gpm_control_new ();
	g_signal_connect (manager->priv->control, "resume",
			  G_CALLBACK (gpm_manager_control_resume_cb), manager);

	gpm_manager_sync_policy_sleep (manager);

	/* update ac throttle */
	gpm_manager_update_ac_throttle (manager);

	/* finally, register on the bus, exporting the objects */
	manager->priv->bus_object_id = -1;
	manager->priv->bus_owner_id =
		g_bus_own_name (G_BUS_TYPE_SESSION,
				GPM_DBUS_SERVICE,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				gpm_manager_bus_acquired_cb,
				NULL,
				gpm_manager_name_lost_cb,
				g_object_ref (manager),
				g_object_unref);
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

	g_object_unref (manager->priv->settings);
	g_object_unref (manager->priv->settings_gsd);
	g_object_unref (manager->priv->dpms);
	g_object_unref (manager->priv->idle);
	g_object_unref (manager->priv->screensaver);
	g_object_unref (manager->priv->control);
	g_object_unref (manager->priv->button);
	g_object_unref (manager->priv->backlight);
	g_object_unref (manager->priv->console);
	g_object_unref (manager->priv->client);

	g_dbus_connection_unregister_object (manager->priv->bus_connection, manager->priv->bus_object_id);
	g_object_unref (manager->priv->bus_connection);
	g_bus_unown_name (manager->priv->bus_owner_id);

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
