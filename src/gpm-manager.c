/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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
#include <libgnomeui/gnome-client.h>

#include <libhal-gpower.h>

#include "gpm-ac-adapter.h"
#include "gpm-button.h"
#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "gpm-cpufreq.h"
#include "gpm-debug.h"
#include "gpm-dpms.h"
#include "gpm-idle.h"
#include "gpm-info.h"
#include "gpm-inhibit.h"
#include "gpm-manager.h"
#include "gpm-notify.h"
#include "gpm-powermanager.h"
#include "gpm-prefs.h"
#include "gpm-profile.h"
#include "gpm-screensaver.h"
#include "gpm-backlight.h"
#include "gpm-srv-brightness-kbd.h"
#include "gpm-srv-screensaver.h"
#include "gpm-stock-icons.h"
#include "gpm-sound.h"
#include "gpm-tray-icon.h"
#include "gpm-engine.h"

#include "dbus/gpm-dbus-control.h"
#include "dbus/gpm-dbus-statistics.h"
#include "dbus/gpm-dbus-backlight.h"
#include "dbus/gpm-dbus-inhibit.h"

static void     gpm_manager_class_init	(GpmManagerClass *klass);
static void     gpm_manager_init	(GpmManager      *manager);
static void     gpm_manager_finalize	(GObject	 *object);

#define GPM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_MANAGER, GpmManagerPrivate))
#define GPM_MANAGER_RECALL_DELAY		1000*10

struct GpmManagerPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmButton		*button;
	GpmConf			*conf;
	GpmDpms			*dpms;
	GpmIdle			*idle;
	GpmInfo			*info;
	GpmInhibit		*inhibit;
	GpmNotify		*notify;
	GpmProfile		*profile;
	GpmControl		*control;
	GpmScreensaver 		*screensaver;
	GpmSound 		*sound;
	GpmTrayIcon		*tray_icon;
	GpmEngine		*engine;
	HalGPower		*hal_power;

	/* interactive services */
	GpmBacklight		*backlight;
	GpmSrvBrightnessKbd	*srv_brightness_kbd;
	GpmCpufreq	 	*cpufreq;
	GpmSrvScreensaver 	*srv_screensaver;
};

enum {
	ON_AC_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmManager, gpm_manager, G_TYPE_OBJECT)

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
gpm_manager_is_inhibit_valid (GpmManager *manager,
			      gboolean	  user_action,
			      const char *action)
{
	gboolean valid;
	gchar *title;

	/* We have to decide on whether this is a idle action or a user keypress */
	gpm_inhibit_is_valid (manager->priv->inhibit, user_action, &valid, NULL);

	if (valid == FALSE) {
		GString *message = g_string_new ("");
		const char *msg;

		title = g_strdup_printf (_("Request to %s"), action);
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
	return valid;
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

	if (on_ac == TRUE) {
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_AC_SLEEP_COMPUTER, &sleep_computer);
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_AC_SLEEP_DISPLAY, &sleep_display);
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_AC_LOWPOWER, &power_save);
	} else {
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_BATT_SLEEP_COMPUTER, &sleep_computer);
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_BATT_SLEEP_DISPLAY, &sleep_display);
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_BATT_LOWPOWER, &power_save);
	}

	hal_gpower_enable_power_save (manager->priv->hal_power, power_save);

	/* set the new sleep (inactivity) value */
	gpm_idle_set_system_timeout (manager->priv->idle, sleep_computer);
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
gpm_manager_blank_screen (GpmManager *manager,
			  GError    **noerror)
{
	gboolean do_lock;
	gboolean ret = TRUE;
	GError  *error = NULL;

	do_lock = gpm_control_get_lock_policy (manager->priv->control,
					       GPM_CONF_LOCK_ON_BLANK_SCREEN);
	if (do_lock == TRUE) {
		if (!gpm_screensaver_lock (manager->priv->screensaver))
			gpm_debug ("Could not lock screen via gnome-screensaver");
	}
	gpm_dpms_set_mode_enum (manager->priv->dpms, GPM_DPMS_MODE_OFF, &error);
	if (error) {
		gpm_debug ("Unable to set DPMS mode: %s", error->message);
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
gpm_manager_unblank_screen (GpmManager *manager,
			    GError    **noerror)
{
	gboolean  do_lock;
	gboolean  ret = TRUE;
	GError   *error;

	error = NULL;
	gpm_dpms_set_mode_enum (manager->priv->dpms, GPM_DPMS_MODE_ON, &error);
	if (error) {
		gpm_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	do_lock = gpm_control_get_lock_policy (manager->priv->control, GPM_CONF_LOCK_ON_BLANK_SCREEN);
	if (do_lock == TRUE) {
		gpm_screensaver_poke (manager->priv->screensaver);
	}
	return ret;
}

/**
 * manager_policy_do:
 * @manager: This class instance
 * @policy: The policy that we should do, e.g. "suspend"
 * @reason: The reason we are performing the policy action, e.g. "battery critical"
 *
 * Does one of the policy actions specified in gconf.
 **/
static void
manager_policy_do (GpmManager  *manager,
		   const gchar *policy,
		   const gchar *reason)
{
	gchar *action = NULL;

	gpm_debug ("policy: %s", policy);
	gpm_conf_get_string (manager->priv->conf, policy, &action);

	if (action == NULL) {
		return;
	}
	if (gpm_control_is_policy_timout_valid (manager->priv->control) == FALSE) {
		return;
	}

	if (strcmp (action, ACTION_NOTHING) == 0) {
		gpm_info_explain_reason (manager->priv->info, GPM_EVENT_NOTIFICATION,
					_("Doing nothing"), reason);

	} else if (strcmp (action, ACTION_SUSPEND) == 0) {
		gpm_info_explain_reason (manager->priv->info, GPM_EVENT_SUSPEND,
					_("Suspending computer"), reason);
		gpm_control_suspend (manager->priv->control, NULL);

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
		gpm_info_explain_reason (manager->priv->info, GPM_EVENT_HIBERNATE,
					_("Hibernating computer"), reason);
		gpm_control_hibernate (manager->priv->control, NULL);

	} else if (strcmp (action, ACTION_BLANK) == 0) {
		gpm_manager_blank_screen (manager, NULL);

	} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
		gpm_info_explain_reason (manager->priv->info, GPM_EVENT_NOTIFICATION,
					_("Shutting down computer"), reason);
		gpm_control_shutdown (manager->priv->control, NULL);

	} else if (strcmp (action, ACTION_INTERACTIVE) == 0) {
		gpm_info_explain_reason (manager->priv->info, GPM_EVENT_NOTIFICATION,
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
 * @manager: This class instance
 * @retval: TRUE if we are on AC power
 **/
gboolean
gpm_manager_get_on_ac (GpmManager  *manager,
			gboolean   *retval,
			GError    **error)
{
	gboolean on_ac;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	if (retval == NULL) {
		return FALSE;
	}

	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);
	if (on_ac == TRUE) {
		*retval = TRUE;
	} else {
		*retval = FALSE;
	}

	return TRUE;
}

/**
 * gpm_manager_get_low_power_mode:
 * @manager: This class instance
 * @retval: TRUE if we are on low power mode
 **/
gboolean
gpm_manager_get_low_power_mode (GpmManager  *manager,
				gboolean    *retval,
				GError     **error)
{
	gboolean power_save;
	gboolean on_ac;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	if (retval == NULL) {
		return FALSE;
	}

	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);
	if (on_ac == TRUE) {
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_AC_LOWPOWER, &power_save);
	} else {
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_BATT_LOWPOWER, &power_save);
	}
	*retval = power_save;

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

	/* find if we are on AC power */
	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);

	if (on_ac == TRUE) {
		gpm_conf_get_string (manager->priv->conf, GPM_CONF_AC_SLEEP_TYPE, &action);
	} else {
		gpm_conf_get_string (manager->priv->conf, GPM_CONF_BATT_SLEEP_TYPE, &action);
	}

	if (action == NULL) {
		gpm_warning ("action NULL, gconf error");
		return;
	}

	if (strcmp (action, ACTION_NOTHING) == 0) {
		gpm_debug ("doing nothing as system idle action");

	} else if (strcmp (action, ACTION_SUSPEND) == 0) {
		gpm_info_explain_reason (manager->priv->info, GPM_EVENT_SUSPEND,
					_("Suspending computer"), _("System idle"));
		ret = gpm_control_suspend (manager->priv->control, NULL);
		if (ret == FALSE) {
			gpm_warning ("cannot suspend, so trying hibernate");
			ret = gpm_control_hibernate (manager->priv->control, NULL);
			if (ret == FALSE) {
				gpm_warning ("cannot suspend or hibernate!");
			}
		}

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
		gpm_info_explain_reason (manager->priv->info, GPM_EVENT_HIBERNATE,
					_("Hibernating computer"), _("System idle"));
		ret = gpm_control_hibernate (manager->priv->control, NULL);
		if (ret == FALSE) {
			gpm_warning ("cannot hibernate, so trying suspend");
			ret = gpm_control_suspend (manager->priv->control, NULL);
			if (ret == FALSE) {
				gpm_warning ("cannot suspend or hibernate!");
			}
		}
	}
	g_free (action);
}

/**
 * idle_changed_cb:
 * @idle: The idle class instance
 * @mode: The idle mode, e.g. GPM_IDLE_MODE_SESSION
 * @manager: This class instance
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
	gboolean laptop_using_ext_mon;

	/*
	 * If external monitor connected we shouldn't ignore idle when lid closed.
	 * Until HAL is able to detect which monitors are connected, control
	 * behavior through gconf-key.
	 * Details here: http://bugzilla.gnome.org/show_bug.cgi?id=365016
	 */
	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_LAPTOP_USES_EXT_MON, &laptop_using_ext_mon);

	/*
	 * Ignore timeout events when the lid is closed, as the DPMS is
	 * already off, and we don't want to perform policy actions or re-enable
	 * the screen when the user moves the mouse on systems that do not
	 * support hardware blanking.
	 * Details are here: https://launchpad.net/malone/bugs/22522
	 */
	if (gpm_button_is_lid_closed (manager->priv->button) == TRUE && laptop_using_ext_mon == FALSE) {
		gpm_debug ("lid is closed, so we are ignoring idle state changes");
		return;
	}

	if (mode == GPM_IDLE_MODE_NORMAL) {

		gpm_debug ("Idle state changed: NORMAL");

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		gpm_debug ("Idle state changed: SESSION");

	} else if (mode == GPM_IDLE_MODE_SYSTEM) {
		gpm_debug ("Idle state changed: SYSTEM");

		if (! gpm_control_is_policy_timout_valid (manager->priv->control)) {
			return;
		}
		if (! gpm_manager_is_inhibit_valid (manager, FALSE, "timeout action")) {
			return;
		}
		idle_do_sleep (manager);
	}
}

/**
 * battery_button_pressed:
 * @manager: This class instance
 *
 * What to do when the battery button is pressed.
 **/
static void
battery_button_pressed (GpmManager *manager)
{
	gchar *message;

	message = gpm_engine_get_summary (manager->priv->engine);

	gpm_notify_display (manager->priv->notify,
			      _("Power Information"),
			      message,
			      GPM_NOTIFY_TIMEOUT_LONG,
			      GTK_STOCK_DIALOG_INFO,
			      GPM_NOTIFY_URGENCY_NORMAL);
	g_free (message);
}

/**
 * power_button_pressed:
 * @manager: This class instance
 *
 * What to do when the power button is pressed.
 **/
static void
power_button_pressed (GpmManager *manager)
{
	if (! gpm_control_is_policy_timout_valid (manager->priv->control)) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, TRUE, "power button press")) {
		return;
	}
	gpm_debug ("power button pressed");
	manager_policy_do (manager, GPM_CONF_BUTTON_POWER, _("the power button has been pressed"));
}

/**
 * suspend_button_pressed:
 * @manager: This class instance
 *
 * What to do when the suspend button is pressed.
 **/
static void
suspend_button_pressed (GpmManager *manager)
{
	if (! gpm_control_is_policy_timout_valid (manager->priv->control)) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, TRUE, "suspend button press")) {
		return;
	}
	gpm_debug ("suspend button pressed");
	manager_policy_do (manager, GPM_CONF_BUTTON_SUSPEND, _("the suspend button has been pressed"));
}

/**
 * hibernate_button_pressed:
 * @manager: This class instance
 *
 * What to do when the hibernate button is pressed.
 **/
static void
hibernate_button_pressed (GpmManager *manager)
{
	if (! gpm_control_is_policy_timout_valid (manager->priv->control)) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, TRUE, "hibernate button press")) {
		return;
	}
	manager_policy_do (manager, GPM_CONF_BUTTON_HIBERNATE, _("the hibernate button has been pressed"));
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
lid_button_pressed (GpmManager *manager,
		    gboolean    pressed)
{
	gboolean on_ac;

	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);

	if (pressed == TRUE) {
		if (on_ac == TRUE) {
			gpm_debug ("Performing AC policy");
			manager_policy_do (manager, GPM_CONF_AC_BUTTON_LID,
					   _("the lid has been closed on ac power"));
		} else {
			gpm_debug ("Performing battery policy");
			manager_policy_do (manager, GPM_CONF_BATT_BUTTON_LID,
					   _("the lid has been closed on battery power"));
		}
	} else {
		/* we turn the lid dpms back on unconditionally */
		gpm_manager_unblank_screen (manager, NULL);
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
button_pressed_cb (GpmButton   *button,
		   const gchar *type,
		   GpmManager  *manager)
{
	gpm_debug ("Button press event type=%s", type);

	if (strcmp (type, GPM_BUTTON_POWER) == 0) {
		power_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_SLEEP) == 0) {
		suspend_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_SUSPEND) == 0) {
		suspend_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_HIBERNATE) == 0) {
		hibernate_button_pressed (manager);

	} else if (strcmp (type, GPM_BUTTON_LID_OPEN) == 0) {
		lid_button_pressed (manager, FALSE);

	} else if (strcmp (type, GPM_BUTTON_LID_CLOSED) == 0) {
		lid_button_pressed (manager, TRUE);

	} else if (strcmp (type, GPM_BUTTON_BATTERY) == 0) {
		battery_button_pressed (manager);
	}
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
ac_adapter_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean	     on_ac,
		       GpmManager   *manager)
{
	gboolean event_when_closed;

	gpm_debug ("Setting on-ac: %d", on_ac);

	gpm_manager_sync_policy_sleep (manager);

	gpm_debug ("emitting on-ac-changed : %i", on_ac);
	if (on_ac == TRUE) {
		g_signal_emit (manager, signals [ON_AC_CHANGED], 0, TRUE);
	} else {
		g_signal_emit (manager, signals [ON_AC_CHANGED], 0, FALSE);
	}

	/* We do the lid close on battery action if the ac_adapter is removed
	   when the laptop is closed and on battery. Fixes #331655 */
	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_BATT_EVENT_WHEN_CLOSED, &event_when_closed);

	/* We keep track of the lid state so we can do the
	   lid close on battery action if the ac_adapter is removed when the laptop
	   is closed. Fixes #331655 */
	if (event_when_closed == TRUE &&
	    on_ac == FALSE &&
	    gpm_button_is_lid_closed (manager->priv->button)) {
		manager_policy_do (manager, GPM_CONF_BATT_BUTTON_LID,
				   _("the lid has been closed, and the ac adapter "
				     "removed (and gconf is okay)"));
	}

	/* Don't do any events for a few seconds after we remove the
	 * ac_adapter. See #348201 for details */
	gpm_control_reset_event_time (manager->priv->control);
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
	manager_policy_do (manager,
			   GPM_CONF_BATT_CRITICAL,
			   _("battery is critically low"));
	return FALSE;
}

#if 0
/**
 * battery_status_changed_primary:
 **/
static void
battery_status_changed_primary (GpmManager     *manager,
				GpmPowerKind    battery_kind,
				GpmPowerStatus *battery_status)
{
	/* Wait until data is trusted... */
	if (gpm_engine_get_data_is_trusted (manager->priv->engine) == FALSE) {
		gpm_debug ("Data is not yet trusted.. wait..");
		return;
	}

	if (! gpm_control_is_policy_timout_valid (manager->priv->control)) {
		return;
	}
}
#endif

/**
 * gpm_manager_class_init:
 * @klass: The GpmManagerClass
 **/
static void
gpm_manager_class_init (GpmManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_manager_finalize;

	signals [ON_AC_CHANGED] =
		g_signal_new ("on-ac-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmManagerClass, on_ac_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);

	g_type_class_add_private (klass, sizeof (GpmManagerPrivate));
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmManager  *manager)
{
	if (strcmp (key, GPM_CONF_BATT_SLEEP_COMPUTER) == 0 ||
		   strcmp (key, GPM_CONF_AC_SLEEP_COMPUTER) == 0) {

		gpm_manager_sync_policy_sleep (manager);
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
gpm_manager_tray_icon_hibernate (GpmManager  *manager,
				 GpmTrayIcon *tray)
{
	if (gpm_control_is_policy_timout_valid (manager->priv->control) == FALSE) {
		return;
	}
	if (gpm_manager_is_inhibit_valid (manager, TRUE, "hibernate") == FALSE) {
		return;
	}

	gpm_info_explain_reason (manager->priv->info,
				GPM_EVENT_HIBERNATE,
				_("Hibernating computer"),
				_("user clicked hibernate from tray menu"));
	gpm_control_hibernate (manager->priv->control, NULL);
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
gpm_manager_tray_icon_suspend (GpmManager   *manager,
			       GpmTrayIcon  *tray)
{
	if (gpm_control_is_policy_timout_valid (manager->priv->control) == FALSE) {
		return;
	}
	if (gpm_manager_is_inhibit_valid (manager, TRUE, "suspend") == FALSE) {
		return;
	}
	gpm_info_explain_reason (manager->priv->info,
				GPM_EVENT_SUSPEND,
				_("Suspending computer"),
				_("user clicked suspend from tray menu"));
	gpm_control_suspend (manager->priv->control, NULL);
}

/**
 * gpm_manager_check_sleep_errors:
 * @manager: This class instance
 *
 * Checks HAL for resume failures
 **/
static void
gpm_manager_check_sleep_errors (GpmManager *manager)
{
	gboolean suspend_error;
	gboolean hibernate_error;

	hal_gpower_has_suspend_error (manager->priv->hal_power, &suspend_error);
	hal_gpower_has_hibernate_error (manager->priv->hal_power, &hibernate_error);

	if (suspend_error == TRUE) {
		gpm_notify_sleep_failed (manager->priv->notify, FALSE);
	}
	if (hibernate_error == TRUE) {
		gpm_notify_sleep_failed (manager->priv->notify, TRUE);
	}
}

/**
 * screensaver_auth_request_cb:
 * @manager: This manager class instance
 * @auth: If we are trying to authenticate
 *
 * Called when the user is trying or has authenticated
 **/
static void
screensaver_auth_request_cb (GpmScreensaver *screensaver,
			     gboolean        auth_begin,
			     GpmManager     *manager)
{
	GError *error;
	gboolean ret;
	/* only clear errors if we have finished the authentication */
	if (auth_begin == FALSE) {
		error = NULL;
		ret = hal_gpower_clear_suspend_error (manager->priv->hal_power, &error);
		if (ret == FALSE) {
			gpm_debug ("Failed to clear suspend error; %s", error->message);
			g_error_free (error);
		}
		error = NULL;
		ret = hal_gpower_clear_hibernate_error (manager->priv->hal_power, &error);
		if (ret == FALSE) {
			gpm_debug ("Failed to clear hibernate error; %s", error->message);
			g_error_free (error);
		}
	}
}

/**
 * gpm_manager_at_exit:
 *
 * Called when we are exiting. We should remove the errors if any
 * exist so we don't get them again on next boot.
 **/
static void
gpm_manager_at_exit (void)
{
	/* we can't use manager as g_atexit has no userdata */
	HalGPower *hal_power = hal_gpower_new ();
	hal_gpower_clear_suspend_error (hal_power, NULL);
	hal_gpower_clear_hibernate_error (hal_power, NULL);
	g_object_unref (hal_power);
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
gpm_engine_perhaps_recall_cb (GpmEngine      *engine,
			      GpmCellUnitKind kind,
			      gchar          *oem_vendor,
			      gchar          *website,
			      GpmManager     *manager)
{
	g_object_set_data (G_OBJECT (manager), "recall-oem-vendor", (gpointer) g_strdup (oem_vendor));
	g_object_set_data (G_OBJECT (manager), "recall-oem-website", (gpointer) g_strdup (website));
	/* delay by a few seconds so the panel can load */
	g_timeout_add (GPM_MANAGER_RECALL_DELAY, (GSourceFunc) gpm_manager_perhaps_recall, manager);
}

/**
 * gpm_engine_low_capacity_cb:
 */
static void
gpm_engine_low_capacity_cb (GpmEngine      *engine,
			    GpmCellUnitKind kind,
			    guint           capacity,
			    GpmManager     *manager)
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
gpm_engine_icon_changed_cb (GpmEngine  *engine,
			    gchar      *icon,
			    GpmManager *manager)
{
	gpm_tray_icon_set_icon (manager->priv->tray_icon, icon);
}

/**
 * gpm_engine_summary_changed_cb:
 */
static void
gpm_engine_summary_changed_cb (GpmEngine  *engine,
			       gchar      *summary,
			       GpmManager *manager)
{
	gpm_tray_icon_set_tooltip (manager->priv->tray_icon, summary);
}

/**
 * gpm_engine_low_capacity_cb:
 */
static void
gpm_engine_fully_charged_cb (GpmEngine      *engine,
			     GpmCellUnitKind kind,
			     guint           capacity,
			     GpmManager     *manager)
{
	if (kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		gpm_notify_fully_charged_primary (manager->priv->notify);
	}
}

/**
 * gpm_engine_low_capacity_cb:
 */
static void
gpm_engine_discharging_cb (GpmEngine      *engine,
			   GpmCellUnitKind kind,
			   GpmManager     *manager)
{
	if (kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		gpm_notify_discharging_primary (manager->priv->notify);
	} else if (kind == GPM_CELL_UNIT_KIND_UPS) {
		gpm_notify_discharging_ups (manager->priv->notify);
	}
}

/**
 * control_sleep_failure_cb:
 **/
static void
control_sleep_failure_cb (GpmControl      *control,
			  GpmControlAction action,
		          GpmManager      *manager)
{
	gboolean show_sleep_failed;

	/* only show this if specified in gconf */
	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_SLEEP_FAILED, &show_sleep_failed);

	/* only emit if in GConf */
	if (show_sleep_failed == TRUE) {
		if (action == GPM_CONTROL_ACTION_SUSPEND) {
			gpm_syslog ("suspend failed");
			gpm_notify_sleep_failed (manager->priv->notify, FALSE);
		} else {
			gpm_syslog ("hibernate failed");
			gpm_notify_sleep_failed (manager->priv->notify, TRUE);
		}
	}
}

/**
 * gpm_engine_charge_low_cb:
 */
static void
gpm_engine_charge_low_cb (GpmEngine      *engine,
			  GpmCellUnitKind kind,
			  GpmCellUnit    *unit,
			  GpmManager     *manager)
{
	const gchar *title = NULL;
	gchar *message = NULL;
	gchar *remaining;
	gchar *icon;

	if (kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		title = _("Laptop battery low");
		remaining = gpm_get_timestring (unit->time_discharge);
		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining battery life (%d%%)"),
					   remaining, unit->percentage);
	} else if (kind == GPM_CELL_UNIT_KIND_UPS) {
		title = _("UPS low");
		remaining = gpm_get_timestring (unit->time_discharge);
		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining UPS backup power (%d%%)"),
					   remaining, unit->percentage);
	} else if (kind == GPM_CELL_UNIT_KIND_MOUSE) {
		title = _("Mouse battery low");
		message = g_strdup_printf (_("The wireless mouse attached to this computer is low in power (%d%%)"), unit->percentage);
	} else if (kind == GPM_CELL_UNIT_KIND_KEYBOARD) {
		title = _("Keyboard battery low");
		message = g_strdup_printf (_("The wireless keyboard attached to this computer is low in power (%d%%)"), unit->percentage);
	} else if (kind == GPM_CELL_UNIT_KIND_PDA) {
		title = _("PDA battery low");
		message = g_strdup_printf (_("The PDA attached to this computer is low in power (%d%%)"), unit->percentage);
	}

	/* get correct icon */
	icon = gpm_cell_unit_get_icon (unit);
	gpm_notify_display (manager->priv->notify,
			    title, message, GPM_NOTIFY_TIMEOUT_LONG,
			    icon, GPM_NOTIFY_URGENCY_NORMAL);
	gpm_sound_event (manager->priv->sound, GPM_SOUND_POWER_LOW);
	g_free (icon);
	g_free (message);
}

/**
 * gpm_engine_charge_critical_cb:
 */
static void
gpm_engine_charge_critical_cb (GpmEngine      *engine,
			       GpmCellUnitKind kind,
			       GpmCellUnit    *unit,
			       GpmManager     *manager)
{
	const gchar *title = NULL;
	gchar *message = NULL;
	gchar *remaining;
	gchar *icon;

	if (kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		title = _("Laptop battery critically low");
		remaining = gpm_get_timestring (unit->time_discharge);
		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining battery life (%d%%). "
					     "Plug in your AC adapter to avoid losing data."),
					   remaining, unit->percentage);
		g_free (remaining);
	} else if (kind == GPM_CELL_UNIT_KIND_UPS) {
		title = _("UPS critically low");
		remaining = gpm_get_timestring (unit->time_discharge);
		message = g_strdup_printf (_("You have approximately <b>%s</b> of remaining UPS power (%d%%). "
					     "Restore AC power to your computer to avoid losing data."),
					   remaining, unit->percentage);
		g_free (remaining);
	} else if (kind == GPM_CELL_UNIT_KIND_MOUSE) {
		title = _("Mouse battery low");
		message = g_strdup_printf (_("The wireless mouse attached to this computer is very low in power (%d%%). "
					     "This device will soon stop functioning if not charged."),
					   unit->percentage);
	} else if (kind == GPM_CELL_UNIT_KIND_KEYBOARD) {
		title = _("Keyboard battery low");
		message = g_strdup_printf (_("The wireless keyboard attached to this computer is very low in power (%d%%). "
					     "This device will soon stop functioning if not charged."),
					   unit->percentage);
	} else if (kind == GPM_CELL_UNIT_KIND_PDA) {
		title = _("PDA battery low");
		message = g_strdup_printf (_("The PDA attached to this computer is very low in power (%d%%). "
					     "This device will soon stop functioning if not charged."),
					   unit->percentage);
	}

	/* get correct icon */
	icon = gpm_cell_unit_get_icon (unit);
	gpm_notify_display (manager->priv->notify,
			    title, message, GPM_NOTIFY_TIMEOUT_LONG,
			    icon, GPM_NOTIFY_URGENCY_CRITICAL);
	gpm_sound_event (manager->priv->sound, GPM_SOUND_POWER_LOW);
	g_free (icon);
	g_free (message);
}

/**
 * gpm_engine_charge_action_cb:
 */
static void
gpm_engine_charge_action_cb (GpmEngine      *engine,
			     GpmCellUnitKind kind,
			     guint           percentage,
			     GpmManager     *manager)
{
	const gchar *title = NULL;
	gchar *action;
	gchar *message = NULL;
	gchar *icon = g_strdup ("moo");

	if (kind == GPM_CELL_UNIT_KIND_PRIMARY) {
		title = _("Laptop battery critically low");

		/* we have to do different warnings depending on the policy */
		gpm_conf_get_string (manager->priv->conf, GPM_CONF_BATT_CRITICAL, &action);

		/* TODO: we should probably convert to an ENUM type, and use that */
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
if (0)		g_timeout_add (1000*10, (GSourceFunc) manager_critical_action_do, manager);

	} else if (kind == GPM_CELL_UNIT_KIND_UPS) {
		title = _("UPS critically low");

		/* we have to do different warnings depending on the policy */
		gpm_conf_get_string (manager->priv->conf, GPM_CONF_UPS_CRITICAL, &action);

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

		g_free (action);
	}

	/* not all types have actions */
	if (title == NULL) {
		return;
	}
	gpm_notify_display (manager->priv->notify,
			    title, message, GPM_NOTIFY_TIMEOUT_LONG,
			    icon, GPM_NOTIFY_URGENCY_CRITICAL);
	gpm_sound_event (manager->priv->sound, GPM_SOUND_POWER_LOW);
	g_free (icon);
	g_free (message);
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

	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	/* do some actions even when killed */
	g_atexit (gpm_manager_at_exit);

	manager->priv->conf = gpm_conf_new ();
	g_signal_connect (manager->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), manager);

	/* we use ac_adapter so we can poke the screensaver and throttle */
	manager->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (manager->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), manager);

	/* coldplug so we are in the correct state at startup */
	on_ac = gpm_ac_adapter_is_present (manager->priv->ac_adapter);

	manager->priv->button = gpm_button_new ();
	g_signal_connect (manager->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), manager);

	manager->priv->hal_power = hal_gpower_new ();
	manager->priv->sound = gpm_sound_new ();

	/* try and start an interactive service */
	manager->priv->cpufreq = gpm_cpufreq_new ();

	/* try and start an interactive service */
	manager->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (manager->priv->screensaver, "auth-request",
 			  G_CALLBACK (screensaver_auth_request_cb), manager);
	manager->priv->srv_screensaver = gpm_srv_screensaver_new ();

	/* try an start an interactive service */
	manager->priv->backlight = gpm_backlight_new ();
	if (manager->priv->backlight != NULL) {
		/* add the new brightness lcd DBUS interface */
		dbus_g_object_type_install_info (GPM_TYPE_BACKLIGHT,
						 &dbus_glib_gpm_backlight_object_info);
		dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_BACKLIGHT,
						     G_OBJECT (manager->priv->backlight));
	}

	manager->priv->srv_brightness_kbd = gpm_srv_brightness_kbd_new ();

	manager->priv->idle = gpm_idle_new ();
	g_signal_connect (manager->priv->idle, "idle-changed",
			  G_CALLBACK (idle_changed_cb), manager);

	/* set up the check_type_cpu, so we can disable the CPU load check */
	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_IDLE_CHECK_CPU, &check_type_cpu);
	gpm_idle_set_check_cpu (manager->priv->idle, check_type_cpu);

	manager->priv->dpms = gpm_dpms_new ();
	manager->priv->notify = gpm_notify_new ();

	/* use a class to handle the complex stuff */
	gpm_debug ("creating new inhibit instance");
	manager->priv->inhibit = gpm_inhibit_new ();
	if (manager->priv->inhibit != NULL) {
		/* add the new brightness lcd DBUS interface */
		dbus_g_object_type_install_info (GPM_TYPE_INHIBIT,
						 &dbus_glib_gpm_inhibit_object_info);
		dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_INHIBIT,
						     G_OBJECT (manager->priv->inhibit));
	}

	/* use the control object */
	gpm_debug ("creating new control instance");
	manager->priv->control = gpm_control_new ();
	g_signal_connect (manager->priv->control, "sleep-failure",
			  G_CALLBACK (control_sleep_failure_cb), manager);
	/* add the new brightness lcd DBUS interface */
	dbus_g_object_type_install_info (GPM_TYPE_CONTROL,
					 &dbus_glib_gpm_control_object_info);
	dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_CONTROL,
					     G_OBJECT (manager->priv->control));

	gpm_debug ("creating new tray icon");
	manager->priv->tray_icon = gpm_tray_icon_new ();
	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "suspend", G_CALLBACK (gpm_manager_tray_icon_suspend),
				 manager, G_CONNECT_SWAPPED);
	g_signal_connect_object (G_OBJECT (manager->priv->tray_icon),
				 "hibernate", G_CALLBACK (gpm_manager_tray_icon_hibernate),
				 manager, G_CONNECT_SWAPPED);

	gpm_debug ("initialising info infrastructure");
	manager->priv->info = gpm_info_new ();
	manager->priv->profile = gpm_profile_new ();

	/* do debugging self tests */
	guint time;
	gpm_debug ("Reference times");
	time = gpm_profile_get_time (manager->priv->profile, 99, TRUE);
	gpm_debug ("99-0\t%i minutes", time / 60);
	time = gpm_profile_get_time (manager->priv->profile, 50, TRUE);
	gpm_debug ("50-0\t%i minutes", time / 60);

	time = gpm_profile_get_time (manager->priv->profile, 0, FALSE);
	gpm_debug ("0-99\t%i minutes", time / 60);
	time = gpm_profile_get_time (manager->priv->profile, 50, FALSE);
	gpm_debug ("50-99\t%i minutes", time / 60);

	/* add the new statistics DBUS interface */
	dbus_g_object_type_install_info (GPM_TYPE_INFO, &dbus_glib_gpm_statistics_object_info);
	dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_STATS,
					     G_OBJECT (manager->priv->info));

	gpm_manager_sync_policy_sleep (manager);

	/* on startup, check if there are suspend errors left */
	gpm_manager_check_sleep_errors (manager);

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

	gpm_engine_start (manager->priv->engine);

	GpmEngineCollection *collection;
	collection = gpm_engine_get_collection (manager->priv->engine);
	gpm_tray_icon_set_collection_data (manager->priv->tray_icon, collection);
	gpm_info_set_collection_data (manager->priv->info, collection);
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

	/* compulsory gobjects */
	g_object_unref (manager->priv->conf);
	g_object_unref (manager->priv->hal_power);
	g_object_unref (manager->priv->sound);
	g_object_unref (manager->priv->dpms);
	g_object_unref (manager->priv->idle);
	g_object_unref (manager->priv->info);
	g_object_unref (manager->priv->engine);
	g_object_unref (manager->priv->profile);
	g_object_unref (manager->priv->tray_icon);
	g_object_unref (manager->priv->inhibit);
	g_object_unref (manager->priv->screensaver);
	g_object_unref (manager->priv->notify);
	g_object_unref (manager->priv->srv_screensaver);

	/* optional gobjects */
	if (manager->priv->button) {
		g_object_unref (manager->priv->button);
	}
	if (manager->priv->cpufreq) {
		g_object_unref (manager->priv->cpufreq);
	}
	if (manager->priv->backlight) {
		g_object_unref (manager->priv->backlight);
	}
	if (manager->priv->srv_brightness_kbd) {
		g_object_unref (manager->priv->srv_brightness_kbd);
	}

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
