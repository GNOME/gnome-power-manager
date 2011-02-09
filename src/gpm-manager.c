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
#include <canberra-gtk.h>
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
#include "gpm-tray-icon.h"
#include "gpm-engine.h"
#include "gpm-upower.h"
#include "gpm-disks.h"

static const gchar *power_manager_introspection = ""
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<node name=\"/org/gnome/PowerManager\">"
  "<interface name=\"org.gnome.PowerManager\">"
    "<property name='Icon' type='s' access='read'>"
    "</property>"
    "<property name='Tooltip' type='s' access='read'>"
    "</property>"
    "<signal name=\"Changed\">"
    "</signal>"
    "<method name=\"GetPrimaryDevice\">"
      "<arg name=\"device\" type=\"(susdut)\" direction=\"out\" />"
    "</method>"
    "<method name=\"GetDevices\">"
      "<arg name=\"devices\" type=\"a(susdut)\" direction=\"out\" />"
    "</method>"
  "</interface>"
"</node>";

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
	GpmDisks		*disks;
	GpmDpms			*dpms;
	GpmIdle			*idle;
	GpmControl		*control;
	GpmScreensaver		*screensaver;
	GpmTrayIcon		*tray_icon;
	GpmEngine		*engine;
	GpmBacklight		*backlight;
	EggConsoleKit		*console;
	guint32			 screensaver_ac_throttle_id;
	guint32			 screensaver_dpms_throttle_id;
	guint32			 screensaver_lid_throttle_id;
	guint32                  critical_alert_timeout_id;
	ca_proplist             *critical_alert_loop_props;
	UpClient		*client;
	gboolean		 on_battery;
	gboolean		 just_resumed;
	GtkStatusIcon		*status_icon;
	NotifyNotification	*notification_general;
	NotifyNotification	*notification_warning_low;
	NotifyNotification	*notification_discharging;
	NotifyNotification	*notification_fully_charged;
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
 * gpm_manager_play_loop_timeout_cb:
 **/
static gboolean
gpm_manager_play_loop_timeout_cb (GpmManager *manager)
{
	ca_context *context;
	context = ca_gtk_context_get_for_screen (gdk_screen_get_default ());
	ca_context_play_full (context, 0,
			      manager->priv->critical_alert_loop_props,
			      NULL,
			      NULL);
	return TRUE;
}

/**
 * gpm_manager_play_loop_stop:
 **/
static gboolean
gpm_manager_play_loop_stop (GpmManager *manager)
{
	if (manager->priv->critical_alert_timeout_id == 0) {
		g_warning ("no sound loop present to stop");
		return FALSE;
	}

	g_source_remove (manager->priv->critical_alert_timeout_id);
	ca_proplist_destroy (manager->priv->critical_alert_loop_props);

	manager->priv->critical_alert_loop_props = NULL;
	manager->priv->critical_alert_timeout_id = 0;

	return TRUE;
}

/**
 * gpm_manager_play_loop_start:
 **/
static gboolean
gpm_manager_play_loop_start (GpmManager *manager, GpmManagerSound action, gboolean force, guint timeout)
{
	const gchar *id = NULL;
	const gchar *desc = NULL;
	gboolean ret;
	gint retval;
	ca_context *context;

	ret = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_ENABLE_SOUND);
	if (!ret && !force) {
		g_debug ("ignoring sound due to policy");
		return FALSE;
	}

	if (timeout == 0) {
		g_warning ("received invalid timeout");
		return FALSE;
	}

	/* if a sound loop is already running, stop the existing loop */
	if (manager->priv->critical_alert_timeout_id != 0) {
		g_warning ("was instructed to play a sound loop with one already playing");
		gpm_manager_play_loop_stop (manager);
	}

	if (action == GPM_MANAGER_SOUND_BATTERY_LOW) {
		id = "battery-low";
		/* TRANSLATORS: this is the sound description */
		desc = _("Battery is very low");
	}

	/* no match */
	if (id == NULL) {
		g_warning ("no sound match for %i", action);
		return FALSE;
	}

	ca_proplist_create (&(manager->priv->critical_alert_loop_props));
	ca_proplist_sets (manager->priv->critical_alert_loop_props,
			  CA_PROP_EVENT_ID, id);
	ca_proplist_sets (manager->priv->critical_alert_loop_props,
			  CA_PROP_EVENT_DESCRIPTION, desc);

	manager->priv->critical_alert_timeout_id = g_timeout_add_seconds (timeout,
									  (GSourceFunc) gpm_manager_play_loop_timeout_cb,
									  manager);
	g_source_set_name_by_id (manager->priv->critical_alert_timeout_id, "[GpmManager] play-loop");

	/* play the sound, using sounds from the naming spec */
	context = ca_gtk_context_get_for_screen (gdk_screen_get_default ());
	retval = ca_context_play (context, 0,
				  CA_PROP_EVENT_ID, id,
				  CA_PROP_EVENT_DESCRIPTION, desc, NULL);
	if (retval < 0)
		g_warning ("failed to play %s: %s", id, ca_strerror (retval));
	return TRUE;
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
	gint retval;
	ca_context *context;

	ret = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_ENABLE_SOUND);
	if (!ret && !force) {
		g_debug ("ignoring sound due to policy");
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
		g_warning ("no match");
		return FALSE;
	}

	/* play the sound, using sounds from the naming spec */
	context = ca_gtk_context_get_for_screen (gdk_screen_get_default ());
	retval = ca_context_play (context, 0,
				  CA_PROP_EVENT_ID, id,
				  CA_PROP_EVENT_DESCRIPTION, desc, NULL);
	if (retval < 0)
		g_warning ("failed to play %s: %s", id, ca_strerror (retval));
	return TRUE;
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
 * gpm_manager_notify_close:
 **/
static gboolean
gpm_manager_notify_close (GpmManager *manager, NotifyNotification *notification)
{
	gboolean ret = FALSE;
	GError *error = NULL;

	/* exists? */
	if (notification == NULL)
		goto out;

	/* try to close */
	ret = notify_notification_close (notification, &error);
	if (!ret) {
		g_warning ("failed to close notification: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * gpm_manager_notification_closed_cb:
 **/
static void
gpm_manager_notification_closed_cb (NotifyNotification *notification, NotifyNotification **notification_class)
{
	g_debug ("caught notification closed signal %p", notification);
	/* the object is already unreffed in _close_signal_handler */
	*notification_class = NULL;
}

/**
 * gpm_manager_get_icon_name:
 **/
static const gchar *
gpm_manager_get_icon_name (GIcon *icon)
{
	const gchar* const *icon_names;
	const gchar *icon_name = NULL;

	/* no icon */
	if (icon == NULL)
		goto out;

	/* just use the first icon */
	icon_names = g_themed_icon_get_names (G_THEMED_ICON (icon));
	if (icon_names != NULL)
		icon_name = icon_names[0];
out:
	return icon_name;
}

/**
 * gpm_manager_notify:
 **/
static gboolean
gpm_manager_notify (GpmManager *manager, NotifyNotification **notification_class,
		    const gchar *title, const gchar *message,
		    guint timeout, const gchar *icon_name, NotifyUrgency urgency)
{
	gboolean ret;
	GError *error = NULL;
	NotifyNotification *notification;

	/* close any existing notification of this class */
	gpm_manager_notify_close (manager, *notification_class);

	/* create a new notification */
	notification = notify_notification_new (title, message, icon_name);
	notify_notification_set_timeout (notification, timeout);
	notify_notification_set_urgency (notification, urgency);
	g_signal_connect (notification, "closed", G_CALLBACK (gpm_manager_notification_closed_cb), notification_class);
	g_debug ("notification %p: %s : %s", notification, title, message);

	/* non-urgent notifications are transient */
	if (urgency != NOTIFY_URGENCY_CRITICAL) {
		notify_notification_set_hint (notification,
					      "transient",
					      g_variant_new_boolean (TRUE));
	}

	/* try to show */
	ret = notify_notification_show (notification, &error);
	if (!ret) {
		g_warning ("failed to show notification: %s", error->message);
		g_error_free (error);
		g_object_unref (notification);
		goto out;
	}

	/* save this local instance as the class instance */
	g_object_add_weak_pointer (G_OBJECT (notification), (gpointer) &notification);
	*notification_class = notification;
out:
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

	g_debug ("sleep failed");
	gpm_manager_play (manager, GPM_MANAGER_SOUND_SUSPEND_ERROR, TRUE);

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
	const gchar *description;
	const gchar *policy_key;
	GpmActionPolicy policy;

	if (pressed)
		gpm_manager_play (manager, GPM_MANAGER_SOUND_LID_CLOSE, FALSE);
	else
		gpm_manager_play (manager, GPM_MANAGER_SOUND_LID_OPEN, FALSE);

	/* we turn the lid dpms back on unconditionally */
	if (pressed == FALSE) {
		gpm_manager_unblank_screen (manager, NULL);
		return;
	}

	/* we have different settings depending on AC state */
	if (!manager->priv->on_battery) {
		policy_key = GSD_SETTINGS_BUTTON_LID_AC;
		description = "Lid closed on AC power";
	} else {
		policy_key = GSD_SETTINGS_BUTTON_LID_BATT;
		description = "Lid closed on battery power";
	}

	/* check that on systems that would meld when the lid is closed
	 * and not asleep we set a better policy option */
	policy = g_settings_get_enum (manager->priv->settings_gsd,
				      policy_key);
	if (policy != GPM_ACTION_POLICY_SUSPEND &&
	    policy != GPM_ACTION_POLICY_HIBERNATE) {
#if UP_CHECK_VERSION(0,9,9)
		if (up_client_get_lid_force_sleep (manager->priv->up_client)) {
			g_warning ("to prevent damage, %s is now forced to 'suspend'",
				   policy_key);
			g_settings_set_enum (manager->priv->settings_gsd,
					     policy_key,
					     GPM_ACTION_POLICY_SUSPEND);
		}
#else
		g_warning ("Laptop may melt if lid is closed. "
			   "Update UPower and rebuild to find out!");
#endif
	}

#if UP_CHECK_VERSION(0,9,8)
	/* are we docked? */
	if (up_client_get_is_docked (manager->priv->client)) {
		g_debug ("ignoring lid closed action because we are docked");
		return;
	}
#endif

	/* do action */
	gpm_manager_perform_policy (manager,
				    policy_key,
				    description);
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
	gchar *message;
	g_debug ("Button press event type=%s", type);

	/* ConsoleKit says we are not on active console */
	if (!gpm_manager_is_active (manager)) {
		g_debug ("ignoring as not on active console");
		return;
	}

	if (g_strcmp0 (type, GPM_BUTTON_POWER) == 0) {
		gpm_manager_perform_policy (manager, GSD_SETTINGS_BUTTON_POWER, "The power button has been pressed.");
	} else if (g_strcmp0 (type, GPM_BUTTON_SLEEP) == 0) {
		gpm_manager_perform_policy (manager, GSD_SETTINGS_BUTTON_SUSPEND, "The suspend button has been pressed.");
	} else if (g_strcmp0 (type, GPM_BUTTON_SUSPEND) == 0) {
		gpm_manager_perform_policy (manager, GSD_SETTINGS_BUTTON_SUSPEND, "The suspend button has been pressed.");
	} else if (g_strcmp0 (type, GPM_BUTTON_HIBERNATE) == 0) {
		gpm_manager_perform_policy (manager, GSD_SETTINGS_BUTTON_HIBERNATE, "The hibernate button has been pressed.");
	} else if (g_strcmp0 (type, GPM_BUTTON_LID_OPEN) == 0) {
		gpm_manager_lid_button_pressed (manager, FALSE);
	} else if (g_strcmp0 (type, GPM_BUTTON_LID_CLOSED) == 0) {
		gpm_manager_lid_button_pressed (manager, TRUE);
	} else if (g_strcmp0 (type, GPM_BUTTON_BATTERY) == 0) {
		message = gpm_engine_get_summary (manager->priv->engine);
		gpm_manager_notify (manager, &manager->priv->notification_general,
				    _("Power Information"),
				    message,
				    GPM_MANAGER_NOTIFY_TIMEOUT_LONG,
				    GTK_STOCK_DIALOG_INFO,
				    NOTIFY_URGENCY_NORMAL);
		g_free (message);
	}

	/* really belongs in gnome-screensaver */
	if (g_strcmp0 (type, GPM_BUTTON_LOCK) == 0)
		gpm_screensaver_lock (manager->priv->screensaver);

	/* disable or enable the fancy screensaver, as we don't want
	 * this starting when the lid is shut */
	if (g_strcmp0 (type, GPM_BUTTON_LID_CLOSED) == 0)
		gpm_manager_update_lid_throttle (manager, TRUE);
	else if (g_strcmp0 (type, GPM_BUTTON_LID_OPEN) == 0)
		gpm_manager_update_lid_throttle (manager, FALSE);
}

/**
 * gpm_manager_get_spindown_timeout:
 **/
static gint
gpm_manager_get_spindown_timeout (GpmManager *manager)
{
	gboolean enabled;
	gint timeout;

	/* get policy */
	if (!manager->priv->on_battery) {
		enabled = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_SPINDOWN_ENABLE_AC);
		timeout = g_settings_get_int (manager->priv->settings, GPM_SETTINGS_SPINDOWN_TIMEOUT_AC);
	} else {
		enabled = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_SPINDOWN_ENABLE_BATT);
		timeout = g_settings_get_int (manager->priv->settings, GPM_SETTINGS_SPINDOWN_TIMEOUT_BATT);
	}
	if (!enabled)
		timeout = 0;
	return timeout;
}

/**
 * gpm_manager_client_changed_cb:
 **/
static void
gpm_manager_client_changed_cb (UpClient *client, GpmManager *manager)
{
	gint timeout;
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

	/* close any discharging notifications */
	if (!on_battery) {
		g_debug ("clearing notify due ac being present");
		gpm_manager_notify_close (manager, manager->priv->notification_warning_low);
		gpm_manager_notify_close (manager, manager->priv->notification_discharging);
	}

	/* if we are playing a critical charge sound loop, stop it */
	if (!on_battery && manager->priv->critical_alert_timeout_id) {
		g_debug ("stopping alert loop due to ac being present");
		gpm_manager_play_loop_stop (manager);
	}

	/* save in local cache */
	manager->priv->on_battery = on_battery;

	/* ConsoleKit says we are not on active console */
	if (!gpm_manager_is_active (manager)) {
		g_debug ("ignoring as not on active console");
		return;
	}

	g_debug ("on_battery: %d", on_battery);

	/* set disk spindown threshold */
	timeout = gpm_manager_get_spindown_timeout (manager);
	gpm_disks_set_spindown_timeout (manager->priv->disks, timeout);

	gpm_manager_sync_policy_sleep (manager);

	gpm_manager_update_ac_throttle (manager);

	/* simulate user input, but only when the lid is open */
	if (!lid_is_closed)
		gpm_screensaver_poke (manager->priv->screensaver);

	if (!on_battery)
		gpm_manager_play (manager, GPM_MANAGER_SOUND_POWER_PLUG, FALSE);
	else
		gpm_manager_play (manager, GPM_MANAGER_SOUND_POWER_UNPLUG, FALSE);

	/* We keep track of the lid state so we can do the
	 * lid close on battery action if the ac adapter is removed when the laptop
	 * is closed */
	if (on_battery && lid_is_closed) {
		gpm_manager_perform_policy (manager, GSD_SETTINGS_BUTTON_LID_BATT,
					    "The lid has been closed, and the ac adapter removed.");
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
	/* stop playing the alert as it's too late to do anything now */
	if (manager->priv->critical_alert_timeout_id)
		gpm_manager_play_loop_stop (manager);

	gpm_manager_perform_policy (manager, GSD_SETTINGS_ACTION_CRITICAL_BATT, "Battery is critically low.");
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
 * gpm_manager_engine_perhaps_recall_cb:
 */
static void
gpm_manager_engine_perhaps_recall_cb (GpmEngine *engine, UpDevice *device, gchar *oem_vendor, gchar *website, GpmManager *manager)
{
	gboolean ret;
	guint timer_id;

	/* don't show when running under GDM */
	if (g_getenv ("RUNNING_UNDER_GDM") != NULL) {
		g_debug ("running under gdm, so no notification");
		return;
	}

	/* already shown, and dismissed */
	ret = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_NOTIFY_PERHAPS_RECALL);
	if (!ret) {
		g_debug ("GConf prevents notification: %s", GPM_SETTINGS_NOTIFY_PERHAPS_RECALL);
		return;
	}

	g_object_set_data_full (G_OBJECT (manager), "recall-oem-vendor", (gpointer) g_strdup (oem_vendor), (GDestroyNotify) g_free);
	g_object_set_data_full (G_OBJECT (manager), "recall-oem-website", (gpointer) g_strdup (website), (GDestroyNotify) g_free);

	/* delay by a few seconds so the panel can load */
	timer_id = g_timeout_add_seconds (GPM_MANAGER_RECALL_DELAY,
					  (GSourceFunc) gpm_manager_perhaps_recall_delay_cb, manager);
	g_source_set_name_by_id (timer_id, "[GpmManager] perhaps-recall");
}

/**
 * gpm_manager_engine_icon_changed_cb:
 */
static void
gpm_manager_engine_icon_changed_cb (GpmEngine  *engine, GIcon *icon, GpmManager *manager)
{
	gpm_tray_icon_set_icon (manager->priv->tray_icon, icon);
}

/**
 * gpm_manager_engine_summary_changed_cb:
 */
static void
gpm_manager_engine_summary_changed_cb (GpmEngine *engine, gchar *summary, GpmManager *manager)
{
	gpm_tray_icon_set_tooltip (manager->priv->tray_icon, summary);
}

/**
 * gpm_manager_engine_low_capacity_cb:
 */
static void
gpm_manager_engine_low_capacity_cb (GpmEngine *engine, UpDevice *device, GpmManager *manager)
{
	gchar *message = NULL;
	const gchar *title;
	gdouble capacity;

	/* don't show when running under GDM */
	if (g_getenv ("RUNNING_UNDER_GDM") != NULL) {
		g_debug ("running under gdm, so no notification");
		goto out;
	}

	/* get device properties */
	g_object_get (device,
		      "capacity", &capacity,
		      NULL);

	/* We should notify the user if the battery has a low capacity,
	 * where capacity is the ratio of the last_full capacity with that of
	 * the design capacity. (#326740) */

	/* TRANSLATORS: battery is old or broken */
	title = _("Battery may be broken");

	/* TRANSLATORS: notify the user that that battery is broken as the capacity is very low */
	message = g_strdup_printf (_("Battery has a very low capacity (%1.1f%%), "
				     "which means that it may be old or broken."), capacity);
	gpm_manager_notify (manager, &manager->priv->notification_general, title, message, GPM_MANAGER_NOTIFY_TIMEOUT_SHORT,
			    GTK_STOCK_DIALOG_INFO, NOTIFY_URGENCY_LOW);
out:
	g_free (message);
}

/**
 * gpm_manager_engine_fully_charged_cb:
 */
static void
gpm_manager_engine_fully_charged_cb (GpmEngine *engine, UpDevice *device, GpmManager *manager)
{
	UpDeviceKind kind;
	gchar *native_path = NULL;
	gboolean ret;
	guint plural = 1;
	const gchar *title;

	/* only action this if specified in the setings */
	ret = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_NOTIFY_FULLY_CHARGED);
	if (!ret) {
		g_debug ("no notification");
		goto out;
	}

	/* don't show when running under GDM */
	if (g_getenv ("RUNNING_UNDER_GDM") != NULL) {
		g_debug ("running under gdm, so no notification");
		goto out;
	}

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "native-path", &native_path,
		      NULL);

	if (kind == UP_DEVICE_KIND_BATTERY) {
		/* is this a dummy composite device, which is plural? */
		if (g_str_has_prefix (native_path, "dummy"))
			plural = 2;

		/* hide the discharging notification */
		gpm_manager_notify_close (manager, manager->priv->notification_warning_low);
		gpm_manager_notify_close (manager, manager->priv->notification_discharging);

		/* TRANSLATORS: show the charged notification */
		title = ngettext ("Battery Charged", "Batteries Charged", plural);
		gpm_manager_notify (manager, &manager->priv->notification_fully_charged,
				    title, NULL, GPM_MANAGER_NOTIFY_TIMEOUT_SHORT,
				    GTK_STOCK_DIALOG_INFO, NOTIFY_URGENCY_LOW);
	}
out:
	g_free (native_path);
}

/**
 * gpm_manager_engine_discharging_cb:
 */
static void
gpm_manager_engine_discharging_cb (GpmEngine *engine, UpDevice *device, GpmManager *manager)
{
	UpDeviceKind kind;
	gboolean ret;
	const gchar *title;
	const gchar *message;
	gdouble percentage;
	gint64 time_to_empty;
	gchar *remaining_text = NULL;
	GIcon *icon = NULL;
	const gchar *device_desc;

	/* only action this if specified in the settings */
	ret = g_settings_get_boolean (manager->priv->settings, GPM_SETTINGS_NOTIFY_DISCHARGING);
	if (!ret) {
		g_debug ("no notification");
		goto out;
	}

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "percentage", &percentage,
		      "time-to-empty", &time_to_empty,
		      NULL);

	/* only show text if there is a valid time */
	if (time_to_empty > 0)
		remaining_text = gpm_get_timestring (time_to_empty);
	device_desc = gpm_device_to_localised_string (device);

	if (kind == UP_DEVICE_KIND_BATTERY) {
		/* TRANSLATORS: laptop battery is now discharging */
		title = _("Battery Discharging");

		if (remaining_text != NULL) {
			/* TRANSLATORS: tell the user how much time they have got */
			message = g_strdup_printf (_("%s of battery power remaining (%.0f%%)"), remaining_text, percentage);
		} else {
			message = g_strdup_printf ("%s (%.0f%%)", device_desc, percentage);
		}
	} else if (kind == UP_DEVICE_KIND_UPS) {
		/* TRANSLATORS: UPS is now discharging */
		title = _("UPS Discharging");

		if (remaining_text != NULL) {
			/* TRANSLATORS: tell the user how much time they have got */
			message = g_strdup_printf (_("%s of UPS backup power remaining (%.0f%%)"), remaining_text, percentage);
		} else {
			message = g_strdup (gpm_device_to_localised_string (device));
			message = g_strdup_printf ("%s (%.0f%%)", device_desc, percentage);
		}
	} else {
		/* nothing else of interest */
		goto out;
	}

	icon = gpm_upower_get_device_icon (device, TRUE);
	/* show the notification */
	gpm_manager_notify (manager, &manager->priv->notification_discharging, title, message, GPM_MANAGER_NOTIFY_TIMEOUT_LONG,
			    gpm_manager_get_icon_name (icon), NOTIFY_URGENCY_NORMAL);
out:
	if (icon != NULL)
		g_object_unref (icon);
	g_free (remaining_text);
	return;
}

/**
 * gpm_manager_engine_just_laptop_battery:
 */
static gboolean
gpm_manager_engine_just_laptop_battery (GpmManager *manager)
{
	UpDevice *device;
	UpDeviceKind kind;
	GPtrArray *array;
	gboolean ret = TRUE;
	guint i;

	/* find if there are any other device types that mean we have to
	 * be more specific in our wording */
	array = gpm_engine_get_devices (manager->priv->engine);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		g_object_get (device, "kind", &kind, NULL);
		if (kind != UP_DEVICE_KIND_BATTERY) {
			ret = FALSE;
			break;
		}
	}
	g_ptr_array_unref (array);
	return ret;
}

/**
 * gpm_manager_engine_charge_low_cb:
 */
static void
gpm_manager_engine_charge_low_cb (GpmEngine *engine, UpDevice *device, GpmManager *manager)
{
	const gchar *title = NULL;
	gchar *message = NULL;
	gchar *remaining_text;
	GIcon *icon = NULL;
	UpDeviceKind kind;
	gdouble percentage;
	gint64 time_to_empty;
	gboolean ret;

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "percentage", &percentage,
		      "time-to-empty", &time_to_empty,
		      NULL);

	/* do we do the notification */
	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_UPS) {
		ret = g_settings_get_boolean (manager->priv->settings,
					      GPM_SETTINGS_NOTIFY_LOW_POWER_SYSTEM);
	} else {
		ret = g_settings_get_boolean (manager->priv->settings,
					      GPM_SETTINGS_NOTIFY_LOW_POWER_DEVICE);
	}
	if (!ret) {
		g_debug ("ignoring notication for type %s", up_device_kind_to_string (kind));
		goto out;
	}

	/* check to see if the batteries have not noticed we are on AC */
	if (kind == UP_DEVICE_KIND_BATTERY) {
		if (!manager->priv->on_battery) {
			g_warning ("ignoring critically low message as we are not on battery power");
			goto out;
		}
	}

	if (kind == UP_DEVICE_KIND_BATTERY) {

		/* if the user has no other batteries, drop the "Laptop" wording */
		ret = gpm_manager_engine_just_laptop_battery (manager);
		if (ret) {
			/* TRANSLATORS: laptop battery low, and we only have one battery */
			title = _("Battery low");
		} else {
			/* TRANSLATORS: laptop battery low, and we have more than one kind of battery */
			title = _("Laptop battery low");
		}

		remaining_text = gpm_get_timestring (time_to_empty);

		/* TRANSLATORS: tell the user how much time they have got */
		message = g_strdup_printf (_("Approximately <b>%s</b> remaining (%.0f%%)"), remaining_text, percentage);

	} else if (kind == UP_DEVICE_KIND_UPS) {
		/* TRANSLATORS: UPS is starting to get a little low */
		title = _("UPS low");
		remaining_text = gpm_get_timestring (time_to_empty);

		/* TRANSLATORS: tell the user how much time they have got */
		message = g_strdup_printf (_("Approximately <b>%s</b> of remaining UPS backup power (%.0f%%)"),
					   remaining_text, percentage);
	} else if (kind == UP_DEVICE_KIND_MOUSE) {
		/* TRANSLATORS: mouse is getting a little low */
		title = _("Mouse battery low");

		/* TRANSLATORS: tell user more details */
		message = g_strdup_printf (_("Wireless mouse is low in power (%.0f%%)"), percentage);

	} else if (kind == UP_DEVICE_KIND_KEYBOARD) {
		/* TRANSLATORS: keyboard is getting a little low */
		title = _("Keyboard battery low");

		/* TRANSLATORS: tell user more details */
		message = g_strdup_printf (_("Wireless keyboard is low in power (%.0f%%)"), percentage);

	} else if (kind == UP_DEVICE_KIND_PDA) {
		/* TRANSLATORS: PDA is getting a little low */
		title = _("PDA battery low");

		/* TRANSLATORS: tell user more details */
		message = g_strdup_printf (_("PDA is low in power (%.0f%%)"), percentage);

	} else if (kind == UP_DEVICE_KIND_PHONE) {
		/* TRANSLATORS: cell phone (mobile) is getting a little low */
		title = _("Cell phone battery low");

		/* TRANSLATORS: tell user more details */
		message = g_strdup_printf (_("Cell phone is low in power (%.0f%%)"), percentage);

#if UP_CHECK_VERSION(0,9,5)
	} else if (kind == UP_DEVICE_KIND_MEDIA_PLAYER) {
		/* TRANSLATORS: media player, e.g. mp3 is getting a little low */
		title = _("Media player battery low");

		/* TRANSLATORS: tell user more details */
		message = g_strdup_printf (_("Media player is low in power (%.0f%%)"), percentage);

	} else if (kind == UP_DEVICE_KIND_TABLET) {
		/* TRANSLATORS: graphics tablet, e.g. wacom is getting a little low */
		title = _("Tablet battery low");

		/* TRANSLATORS: tell user more details */
		message = g_strdup_printf (_("Tablet is low in power (%.0f%%)"), percentage);

	} else if (kind == UP_DEVICE_KIND_COMPUTER) {
		/* TRANSLATORS: computer, e.g. ipad is getting a little low */
		title = _("Attached computer battery low");

		/* TRANSLATORS: tell user more details */
		message = g_strdup_printf (_("Attached computer is low in power (%.0f%%)"), percentage);
#endif
	}

	/* get correct icon */
	icon = gpm_upower_get_device_icon (device, TRUE);
	gpm_manager_notify (manager, &manager->priv->notification_warning_low, title, message,
			    GPM_MANAGER_NOTIFY_TIMEOUT_LONG, gpm_manager_get_icon_name (icon), NOTIFY_URGENCY_NORMAL);
	gpm_manager_play (manager, GPM_MANAGER_SOUND_BATTERY_CAUTION, TRUE);
out:
	if (icon != NULL)
		g_object_unref (icon);
	g_free (message);
}

/**
 * gpm_manager_engine_charge_critical_cb:
 */
static void
gpm_manager_engine_charge_critical_cb (GpmEngine *engine, UpDevice *device, GpmManager *manager)
{
	const gchar *title = NULL;
	gchar *message = NULL;
	GIcon *icon = NULL;
	UpDeviceKind kind;
	gdouble percentage;
	gint64 time_to_empty;
	GpmActionPolicy policy;
	gboolean ret;

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "percentage", &percentage,
		      "time-to-empty", &time_to_empty,
		      NULL);

	/* check to see if the batteries have not noticed we are on AC */
	if (kind == UP_DEVICE_KIND_BATTERY) {
		if (!manager->priv->on_battery) {
			g_warning ("ignoring critically low message as we are not on battery power");
			goto out;
		}
	}

	/* do we do the notification */
	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_UPS) {
		/* this is not configurable */
		ret = TRUE;
	} else {
		ret = g_settings_get_boolean (manager->priv->settings,
					      GPM_SETTINGS_NOTIFY_LOW_POWER_DEVICE);
	}
	if (!ret) {
		g_debug ("ignoring notication for type %s", up_device_kind_to_string (kind));
		goto out;
	}

	if (kind == UP_DEVICE_KIND_BATTERY) {

		/* if the user has no other batteries, drop the "Laptop" wording */
		ret = gpm_manager_engine_just_laptop_battery (manager);
		if (ret) {
			/* TRANSLATORS: laptop battery critically low, and only have one kind of battery */
			title = _("Battery critically low");
		} else {
			/* TRANSLATORS: laptop battery critically low, and we have more than one type of battery */
			title = _("Laptop battery critically low");
		}

		/* we have to do different warnings depending on the policy */
		policy = g_settings_get_enum (manager->priv->settings_gsd, GSD_SETTINGS_ACTION_CRITICAL_BATT);

		/* use different text for different actions */
		if (policy == GPM_ACTION_POLICY_NOTHING) {
			/* TRANSLATORS: tell the use to insert the plug, as we're not going to do anything */
			message = g_strdup (_("Plug in your AC adapter to avoid losing data."));

		} else if (policy == GPM_ACTION_POLICY_SUSPEND) {
			/* TRANSLATORS: give the user a ultimatum */
			message = g_strdup_printf (_("Computer will suspend very soon unless it is plugged in."));

		} else if (policy == GPM_ACTION_POLICY_HIBERNATE) {
			/* TRANSLATORS: give the user a ultimatum */
			message = g_strdup_printf (_("Computer will hibernate very soon unless it is plugged in."));

		} else if (policy == GPM_ACTION_POLICY_SHUTDOWN) {
			/* TRANSLATORS: give the user a ultimatum */
			message = g_strdup_printf (_("Computer will shutdown very soon unless it is plugged in."));
		}

	} else if (kind == UP_DEVICE_KIND_UPS) {
		gchar *remaining_text;

		/* TRANSLATORS: the UPS is very low */
		title = _("UPS critically low");
		remaining_text = gpm_get_timestring (time_to_empty);

		/* TRANSLATORS: give the user a ultimatum */
		message = g_strdup_printf (_("Approximately <b>%s</b> of remaining UPS power (%.0f%%). "
					     "Restore AC power to your computer to avoid losing data."),
					   remaining_text, percentage);
		g_free (remaining_text);
	} else if (kind == UP_DEVICE_KIND_MOUSE) {
		/* TRANSLATORS: the mouse battery is very low */
		title = _("Mouse battery low");

		/* TRANSLATORS: the device is just going to stop working */
		message = g_strdup_printf (_("Wireless mouse is very low in power (%.0f%%). "
					     "This device will soon stop functioning if not charged."),
					   percentage);
	} else if (kind == UP_DEVICE_KIND_KEYBOARD) {
		/* TRANSLATORS: the keyboard battery is very low */
		title = _("Keyboard battery low");

		/* TRANSLATORS: the device is just going to stop working */
		message = g_strdup_printf (_("Wireless keyboard is very low in power (%.0f%%). "
					     "This device will soon stop functioning if not charged."),
					   percentage);
	} else if (kind == UP_DEVICE_KIND_PDA) {

		/* TRANSLATORS: the PDA battery is very low */
		title = _("PDA battery low");

		/* TRANSLATORS: the device is just going to stop working */
		message = g_strdup_printf (_("PDA is very low in power (%.0f%%). "
					     "This device will soon stop functioning if not charged."),
					   percentage);

	} else if (kind == UP_DEVICE_KIND_PHONE) {

		/* TRANSLATORS: the cell battery is very low */
		title = _("Cell phone battery low");

		/* TRANSLATORS: the device is just going to stop working */
		message = g_strdup_printf (_("Cell phone is very low in power (%.0f%%). "
					     "This device will soon stop functioning if not charged."),
					   percentage);

#if UP_CHECK_VERSION(0,9,5)
	} else if (kind == UP_DEVICE_KIND_MEDIA_PLAYER) {

		/* TRANSLATORS: the cell battery is very low */
		title = _("Cell phone battery low");

		/* TRANSLATORS: the device is just going to stop working */
		message = g_strdup_printf (_("Media player is very low in power (%.0f%%). "
					     "This device will soon stop functioning if not charged."),
					   percentage);
	} else if (kind == UP_DEVICE_KIND_TABLET) {

		/* TRANSLATORS: the cell battery is very low */
		title = _("Tablet battery low");

		/* TRANSLATORS: the device is just going to stop working */
		message = g_strdup_printf (_("Tablet is very low in power (%.0f%%). "
					     "This device will soon stop functioning if not charged."),
					   percentage);
	} else if (kind == UP_DEVICE_KIND_COMPUTER) {

		/* TRANSLATORS: the cell battery is very low */
		title = _("Attached computer battery low");

		/* TRANSLATORS: the device is just going to stop working */
		message = g_strdup_printf (_("Attached computer is very low in power (%.0f%%). "
					     "The device will soon shutdown if not charged."),
					   percentage);
#endif
	}

	/* get correct icon */
	icon = gpm_upower_get_device_icon (device, TRUE);
	gpm_manager_notify (manager, &manager->priv->notification_warning_low, title, message,
			    GPM_MANAGER_NOTIFY_TIMEOUT_NEVER, gpm_manager_get_icon_name (icon), NOTIFY_URGENCY_CRITICAL);

	switch (kind) {

	case UP_DEVICE_KIND_BATTERY:
	case UP_DEVICE_KIND_UPS:
		g_debug ("critical charge level reached, starting sound loop");
		gpm_manager_play_loop_start (manager,
					     GPM_MANAGER_SOUND_BATTERY_LOW,
					     TRUE,
					     GPM_MANAGER_CRITICAL_ALERT_TIMEOUT);
		break;

	default:
		gpm_manager_play (manager, GPM_MANAGER_SOUND_BATTERY_LOW, TRUE);
	}
out:
	if (icon != NULL)
		g_object_unref (icon);
	g_free (message);
}

/**
 * gpm_manager_engine_charge_action_cb:
 */
static void
gpm_manager_engine_charge_action_cb (GpmEngine *engine, UpDevice *device, GpmManager *manager)
{
	const gchar *title = NULL;
	gchar *message = NULL;
	GIcon *icon = NULL;
	UpDeviceKind kind;
	GpmActionPolicy policy;
	guint timer_id;

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      NULL);

	/* check to see if the batteries have not noticed we are on AC */
	if (kind == UP_DEVICE_KIND_BATTERY) {
		if (!manager->priv->on_battery) {
			g_warning ("ignoring critically low message as we are not on battery power");
			goto out;
		}
	}

	if (kind == UP_DEVICE_KIND_BATTERY) {

		/* TRANSLATORS: laptop battery is really, really, low */
		title = _("Laptop battery critically low");

		/* we have to do different warnings depending on the policy */
		policy = g_settings_get_enum (manager->priv->settings_gsd, GSD_SETTINGS_ACTION_CRITICAL_BATT);

		/* use different text for different actions */
		if (policy == GPM_ACTION_POLICY_NOTHING) {
			/* TRANSLATORS: computer will shutdown without saving data */
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer will <b>power-off</b> when the "
					      "battery becomes completely empty."));

		} else if (policy == GPM_ACTION_POLICY_SUSPEND) {
			/* TRANSLATORS: computer will suspend */
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer is about to suspend.<br>"
					      "<b>NOTE:</b> A small amount of power is required "
					      "to keep your computer in a suspended state."));

		} else if (policy == GPM_ACTION_POLICY_HIBERNATE) {
			/* TRANSLATORS: computer will hibernate */
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer is about to hibernate."));

		} else if (policy == GPM_ACTION_POLICY_SHUTDOWN) {
			/* TRANSLATORS: computer will just shutdown */
			message = g_strdup (_("The battery is below the critical level and "
					      "this computer is about to shutdown."));
		}

		/* wait 20 seconds for user-panic */
		timer_id = g_timeout_add_seconds (20, (GSourceFunc) manager_critical_action_do, manager);
		g_source_set_name_by_id (timer_id, "[GpmManager] battery critical-action");

	} else if (kind == UP_DEVICE_KIND_UPS) {
		/* TRANSLATORS: UPS is really, really, low */
		title = _("UPS critically low");

		/* we have to do different warnings depending on the policy */
		policy = g_settings_get_enum (manager->priv->settings_gsd, GSD_SETTINGS_ACTION_CRITICAL_BATT);

		/* use different text for different actions */
		if (policy == GPM_ACTION_POLICY_NOTHING) {
			/* TRANSLATORS: computer will shutdown without saving data */
			message = g_strdup (_("UPS is below the critical level and "
					      "this computer will <b>power-off</b> when the "
					      "UPS becomes completely empty."));

		} else if (policy == GPM_ACTION_POLICY_HIBERNATE) {
			/* TRANSLATORS: computer will hibernate */
			message = g_strdup (_("UPS is below the critical level and "
					      "this computer is about to hibernate."));

		} else if (policy == GPM_ACTION_POLICY_SHUTDOWN) {
			/* TRANSLATORS: computer will just shutdown */
			message = g_strdup (_("UPS is below the critical level and "
					      "this computer is about to shutdown."));
		}

		/* wait 20 seconds for user-panic */
		timer_id = g_timeout_add_seconds (20, (GSourceFunc) manager_critical_action_do, manager);
		g_source_set_name_by_id (timer_id, "[GpmManager] ups critical-action");
	}

	/* not all types have actions */
	if (title == NULL)
		return;

	/* get correct icon */
	icon = gpm_upower_get_device_icon (device, TRUE);
	gpm_manager_notify (manager, &manager->priv->notification_warning_low,
			    title, message, GPM_MANAGER_NOTIFY_TIMEOUT_NEVER,
			    gpm_manager_get_icon_name (icon), NOTIFY_URGENCY_CRITICAL);
	gpm_manager_play (manager, GPM_MANAGER_SOUND_BATTERY_LOW, TRUE);
out:
	if (icon != NULL)
		g_object_unref (icon);
	g_free (message);
}

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

	if (manager->priv->notification_general != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_general);
	if (manager->priv->notification_warning_low != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_warning_low);
	if (manager->priv->notification_discharging != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_discharging);
	if (manager->priv->notification_fully_charged != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_fully_charged);

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
 * gpm_manager_device_to_variant_blob:
 **/
static GVariant *
gpm_manager_device_to_variant_blob (UpDevice *device)
{
	UpDeviceKind kind;
	UpDeviceState state;
	gdouble percentage;
	guint64 time_state = 0;
	guint64 time_empty, time_full;
	GVariant *value;
	GIcon *icon;
	gchar *device_icon;
	const gchar *object_path;

	icon = gpm_upower_get_device_icon (device, FALSE);
	device_icon = g_icon_to_string (icon);
	g_object_get (device,
		      "kind", &kind,
		      "percentage", &percentage,
		      "state", &state,
		      "time-to-empty", &time_empty,
		      "time-to-full", &time_full,
		      NULL);

	/* only return time for these simple states */
	if (state == UP_DEVICE_STATE_DISCHARGING)
		time_state = time_empty;
	else if (state == UP_DEVICE_STATE_CHARGING)
		time_state = time_full;

	/* get an object path, even for the composite device */
	object_path = up_device_get_object_path (device);
	if (object_path == NULL)
		object_path = "/org/gnome/PowerManager";

	/* format complex object */
	value = g_variant_new ("(susdut)",
			       object_path,
			       kind,
			       device_icon,
			       percentage,
			       state,
			       time_state);
	g_free (device_icon);
	return value;
}

/**
 * gpm_manager_dbus_method_call:
 **/
static void
gpm_manager_dbus_method_call (GDBusConnection *connection,
			      const gchar *sender, const gchar *object_path,
			      const gchar *interface_name, const gchar *method_name,
			      GVariant *parameters,
			      GDBusMethodInvocation *invocation,
			      gpointer user_data)
{
	GpmManager *manager = GPM_MANAGER (user_data);
	UpDevice *device;
	GVariant *value = NULL;
	GVariant *tuple = NULL;
	GPtrArray *array = NULL;
	guint i;
	GVariantBuilder *builder;

	/* return object */
	if (g_strcmp0 (method_name, "GetPrimaryDevice") == 0) {

		/* get the virtual device */
		device = gpm_engine_get_primary_device (manager->priv->engine);
		if (device == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.gnome.PowerManager.Failed",
								    "There is no primary device to reflect system state (don't show any UI)");
			goto out;
		}

		/* return the value */
		value = gpm_manager_device_to_variant_blob (device);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return array */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		/* create builder */
		builder = g_variant_builder_new (G_VARIANT_TYPE("a(susdut)"));

		/* add each tuple to the array */
		array = gpm_engine_get_devices (manager->priv->engine);
		for (i=0; i<array->len; i++) {
			device = g_ptr_array_index (array, i);
			value = gpm_manager_device_to_variant_blob (device);
			g_variant_builder_add_value (builder, value);
		}

		/* return the value */
		value = g_variant_builder_end (builder);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		g_variant_builder_unref (builder);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * gpm_manager_dbus_property_get:
 **/
static GVariant *
gpm_manager_dbus_property_get (GDBusConnection *connection,
			       const gchar *sender, const gchar *object_path,
			       const gchar *interface_name, const gchar *property_name,
			       GError **error, gpointer user_data)
{
	GpmManager *manager = GPM_MANAGER (user_data);
	gchar *tooltip = NULL;
	GIcon *icon = NULL;
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "Icon") == 0) {
		icon = gpm_engine_get_icon (manager->priv->engine);
		if (icon != NULL)
			tooltip = g_icon_to_string (icon);
		retval = g_variant_new_string (tooltip != NULL ? tooltip : "");
		goto out;
	}
	if (g_strcmp0 (property_name, "Tooltip") == 0) {
		tooltip = gpm_engine_get_summary (manager->priv->engine);
		retval = g_variant_new_string (tooltip != NULL ? tooltip : "");
		goto out;
	}
out:
	if (icon != NULL)
		g_object_unref (icon);
	g_free (tooltip);
	return retval;
}

/**
 * gpm_manager_dbus_property_set:
 **/
static gboolean
gpm_manager_dbus_property_set (GDBusConnection *connection,
			       const gchar *sender, const gchar *object_path,
			       const gchar *interface_name, const gchar *property_name,
			       GVariant *value,
			       GError **invocation, gpointer user_data)
{
	/* GpmManager *manager = GPM_MANAGER (user_data); */
	/* do nothing, no properties defined (yet) */
	return FALSE;
}

/**
 * gpm_manager_bus_acquired_cb:
 **/
static void
gpm_manager_bus_acquired_cb (GDBusConnection *connection,
			    const gchar *name, gpointer user_data)
{
	GDBusNodeInfo *node_info;
	GDBusInterfaceInfo *interface_info;
	GpmManager *manager = GPM_MANAGER (user_data);
	GDBusInterfaceVTable interface_vtable = {
			gpm_manager_dbus_method_call,
			gpm_manager_dbus_property_get,
			gpm_manager_dbus_property_set
	};

	node_info = g_dbus_node_info_new_for_xml (power_manager_introspection, NULL);
	interface_info = g_dbus_node_info_lookup_interface (node_info, GPM_DBUS_INTERFACE);

	manager->priv->bus_connection = (GDBusConnection*) g_object_ref ((GObject*) connection);
	manager->priv->bus_object_id = g_dbus_connection_register_object (connection,
			GPM_DBUS_PATH,
			interface_info,
			&interface_vtable,
			manager,
			NULL,
			NULL);
	g_dbus_node_info_unref (node_info);

	if (manager->priv->backlight != NULL) {
		gpm_backlight_register_dbus (manager->priv->backlight, connection);
	}
}

/**
 * gpm_manager_engine_devices_changed_cb:
 **/
static void
gpm_manager_engine_devices_changed_cb (GpmEngine *engine, GpmManager *manager)
{
	/* emit for the shell */
	if (manager->priv->bus_connection != NULL) {
		g_dbus_connection_emit_signal (manager->priv->bus_connection,
					       NULL, GPM_DBUS_PATH, GPM_DBUS_INTERFACE,
					       "Changed", NULL, NULL);
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
}

/**
 * gpm_manager_init:
 * @manager: This class instance
 **/
static void
gpm_manager_init (GpmManager *manager)
{
	gint timeout;

	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);

	/* init to unthrottled */
	manager->priv->screensaver_ac_throttle_id = 0;
	manager->priv->screensaver_dpms_throttle_id = 0;
	manager->priv->screensaver_lid_throttle_id = 0;

	manager->priv->critical_alert_timeout_id = 0;
	manager->priv->critical_alert_loop_props = NULL;

	/* init to not just_resumed */
	manager->priv->just_resumed = FALSE;

	/* don't apply policy when not active, so listen to ConsoleKit */
	manager->priv->console = egg_console_kit_new ();

	manager->priv->notification_general = NULL;
	manager->priv->notification_warning_low = NULL;
	manager->priv->notification_discharging = NULL;
	manager->priv->notification_fully_charged = NULL;
	manager->priv->disks = gpm_disks_new ();
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

	g_debug ("creating new tray icon");
	manager->priv->tray_icon = gpm_tray_icon_new ();

	/* keep a reference for the notifications */
	manager->priv->status_icon = gpm_tray_icon_get_status_icon (manager->priv->tray_icon);

	gpm_manager_sync_policy_sleep (manager);

	manager->priv->engine = gpm_engine_new ();
	g_signal_connect (manager->priv->engine, "perhaps-recall",
			  G_CALLBACK (gpm_manager_engine_perhaps_recall_cb), manager);
	g_signal_connect (manager->priv->engine, "low-capacity",
			  G_CALLBACK (gpm_manager_engine_low_capacity_cb), manager);
	g_signal_connect (manager->priv->engine, "icon-changed",
			  G_CALLBACK (gpm_manager_engine_icon_changed_cb), manager);
	g_signal_connect (manager->priv->engine, "summary-changed",
			  G_CALLBACK (gpm_manager_engine_summary_changed_cb), manager);
	g_signal_connect (manager->priv->engine, "fully-charged",
			  G_CALLBACK (gpm_manager_engine_fully_charged_cb), manager);
	g_signal_connect (manager->priv->engine, "discharging",
			  G_CALLBACK (gpm_manager_engine_discharging_cb), manager);
	g_signal_connect (manager->priv->engine, "charge-low",
			  G_CALLBACK (gpm_manager_engine_charge_low_cb), manager);
	g_signal_connect (manager->priv->engine, "charge-critical",
			  G_CALLBACK (gpm_manager_engine_charge_critical_cb), manager);
	g_signal_connect (manager->priv->engine, "charge-action",
			  G_CALLBACK (gpm_manager_engine_charge_action_cb), manager);
	g_signal_connect (manager->priv->engine, "devices-changed",
			  G_CALLBACK (gpm_manager_engine_devices_changed_cb), manager);

	/* set disk spindown threshold */
	timeout = gpm_manager_get_spindown_timeout (manager);
	gpm_disks_set_spindown_timeout (manager->priv->disks, timeout);

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

	/* close any notifications (also unrefs them) */
	if (manager->priv->notification_general != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_general);
	if (manager->priv->notification_warning_low != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_warning_low);
	if (manager->priv->notification_discharging != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_discharging);
	if (manager->priv->notification_fully_charged != NULL)
		gpm_manager_notify_close (manager, manager->priv->notification_fully_charged);
	if (manager->priv->critical_alert_timeout_id != 0)
		g_source_remove (manager->priv->critical_alert_timeout_id);

	g_object_unref (manager->priv->settings);
	g_object_unref (manager->priv->settings_gsd);
	g_object_unref (manager->priv->disks);
	g_object_unref (manager->priv->dpms);
	g_object_unref (manager->priv->idle);
	g_object_unref (manager->priv->engine);
	g_object_unref (manager->priv->tray_icon);
	g_object_unref (manager->priv->screensaver);
	g_object_unref (manager->priv->control);
	g_object_unref (manager->priv->button);
	g_object_unref (manager->priv->backlight);
	g_object_unref (manager->priv->console);
	g_object_unref (manager->priv->client);
	g_object_unref (manager->priv->status_icon);

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
