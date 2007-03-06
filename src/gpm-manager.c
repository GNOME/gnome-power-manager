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
#include "gpm-battery.h"
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
#include "gpm-power.h"
#include "gpm-powermanager.h"
#include "gpm-prefs.h"
#include "gpm-screensaver.h"
#include "gpm-srv-backlight.h"
#include "gpm-srv-brightness-kbd.h"
#include "gpm-srv-screensaver.h"
#include "gpm-stock-icons.h"
#include "gpm-sound.h"
#include "gpm-tray-icon.h"
#include "gpm-warning.h"

#include "dbus/gpm-dbus-control.h"
#include "dbus/gpm-dbus-statistics.h"
#include "dbus/gpm-dbus-backlight.h"
#include "dbus/gpm-dbus-ui.h"
#include "dbus/gpm-dbus-inhibit.h"

static void     gpm_manager_class_init	(GpmManagerClass *klass);
static void     gpm_manager_init	(GpmManager      *manager);
static void     gpm_manager_finalize	(GObject	 *object);

#define GPM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_MANAGER, GpmManagerPrivate))

#define GPM_NOTIFY_TIMEOUT_LONG		20	/* seconds */
#define GPM_NOTIFY_TIMEOUT_SHORT	5	/* seconds */

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
	GpmPower		*power;
	GpmControl		*control;
	GpmScreensaver 		*screensaver;
	GpmSound 		*sound;
	GpmTrayIcon		*tray_icon;
	GpmWarning		*warning;

	HalGPower		*hal_power;

	/* interactive services */
	GpmSrvBacklight		*srv_backlight;
	GpmSrvBrightnessKbd	*srv_brightness_kbd;
	GpmCpufreq	 	*cpufreq;
	GpmSrvScreensaver 	*srv_screensaver;

	GpmWarningState		 last_primary;
	GpmWarningState		 last_ups;
	GpmWarningState		 last_mouse;
	GpmWarningState		 last_keyboard;
	GpmWarningState		 last_pda;

	gboolean		 done_notify_fully_charged;
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
 * gpm_manager_allowed_suspend:
 *
 * Proxy this to maintain API compatability.
 **/
gboolean
gpm_manager_allowed_suspend (GpmManager *manager,
			     gboolean   *can,
			     GError    **error)
{
	return gpm_control_allowed_suspend (manager->priv->control, can, error);
}

/**
 * gpm_manager_allowed_hibernate:
 *
 * Proxy this to maintain API compatability.
 **/
gboolean
gpm_manager_allowed_hibernate (GpmManager *manager,
			       gboolean   *can,
			       GError    **error)
{
	return gpm_control_allowed_hibernate (manager->priv->control, can, error);
}

/**
 * gpm_manager_allowed_shutdown:
 *
 * Proxy this to maintain API compatability.
 **/
gboolean
gpm_manager_allowed_shutdown (GpmManager *manager,
			      gboolean   *can,
			      GError    **error)
{
	return gpm_control_allowed_shutdown (manager->priv->control, can, error);
}

/**
 * gpm_manager_allowed_reboot:
 *
 * Proxy this to maintain API compatability.
 **/
gboolean
gpm_manager_allowed_reboot (GpmManager *manager,
			    gboolean   *can,
			    GError    **error)
{
	return gpm_control_allowed_reboot (manager->priv->control, can, error);
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
	guint	     sleep_display;
	guint	     sleep_computer;
	GpmAcAdapterState state;
	gboolean     power_save;

	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_AC_SLEEP_COMPUTER, &sleep_computer);
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_AC_SLEEP_DISPLAY, &sleep_display);
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_AC_LOWPOWER, &power_save);
	} else {
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_BATTERY_SLEEP_COMPUTER, &sleep_computer);
		gpm_conf_get_uint (manager->priv->conf, GPM_CONF_BATTERY_SLEEP_DISPLAY, &sleep_display);
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_BATTERY_LOWPOWER, &power_save);
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
	if (gpm_control_is_policy_timout_valid (manager->priv->control, "policy event") == FALSE) {
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
	GpmAcAdapterState state;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	if (retval == NULL) {
		return FALSE;
	}

	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);
	if (state == GPM_AC_ADAPTER_PRESENT) {
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
	GpmAcAdapterState state;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	if (retval == NULL) {
		return FALSE;
	}

	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);
	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_AC_LOWPOWER, &power_save);
	} else {
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_BATTERY_LOWPOWER, &power_save);
	}
	*retval = power_save;

	return TRUE;
}

/**
 * gpm_manager_inhibit:
 *
 * Proxy this while we support the old API.
 **/
void
gpm_manager_inhibit (GpmManager	 *manager,
		     const gchar *application,
		     const gchar *reason,
		     DBusGMethodInvocation *context,
		     GError     **error)
{
	gpm_inhibit_inhibit_auto (manager->priv->inhibit, application, reason, context, error);
}

/**
 * gpm_manager_uninhibit:
 *
 * Proxy this while we support the old API.
 **/
void
gpm_manager_uninhibit (GpmManager	 *manager,
		       guint32		  cookie,
		       DBusGMethodInvocation *context,
		       GError		**error)
{
	gpm_inhibit_un_inhibit (manager->priv->inhibit, cookie, error);
	dbus_g_method_return (context);
}

/**
 * gpm_manager_suspend_dbus_method:
 *
 * Proxy this while we support the old API.
 **/
gboolean
gpm_manager_suspend_dbus_method (GpmManager *manager,
				 GError    **error)
{
	/* FIXME: From where? */
	gpm_info_explain_reason (manager->priv->info, GPM_EVENT_SUSPEND,
				_("Suspending computer"),
				_("the DBUS method Suspend() was invoked"));
	return gpm_control_suspend (manager->priv->control, error);
}

/**
 * gpm_manager_hibernate_dbus_method:
 *
 * Proxy this while we support the old API.
 **/
gboolean
gpm_manager_hibernate_dbus_method (GpmManager *manager,
				   GError    **error)
{
	/* FIXME: From where? */
	gpm_info_explain_reason (manager->priv->info, GPM_EVENT_HIBERNATE,
				_("Hibernating computer"),
				_("the DBUS method Hibernate() was invoked"));
	return gpm_control_hibernate (manager->priv->control, error);
}

/**
 * gpm_manager_shutdown_dbus_method:
 *
 * Proxy this while we support the old API.
 **/
gboolean
gpm_manager_shutdown_dbus_method (GpmManager *manager,
				  GError    **error)
{
	/* FIXME: From where? */
	gpm_info_explain_reason (manager->priv->info, GPM_EVENT_NOTIFICATION,
				_("Shutting down computer"),
				_("the DBUS method Shutdown() was invoked"));
	return gpm_control_shutdown (manager->priv->control, error);
}

/**
 * gpm_manager_reboot_dbus_method:
 *
 * Proxy this while we support the old API.
 **/
gboolean
gpm_manager_reboot_dbus_method (GpmManager *manager,
				GError    **error)
{
	/* FIXME: From where? */
	gpm_info_explain_reason (manager->priv->info, GPM_EVENT_NOTIFICATION,
				_("Rebooting computer"),
				_("the DBUS method Reboot() was invoked"));
	return gpm_control_reboot (manager->priv->control, error);
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
	GpmAcAdapterState state;
	gchar *action = NULL;
	gboolean ret;

	/* find if we are on AC power */
	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_conf_get_string (manager->priv->conf, GPM_CONF_AC_SLEEP_TYPE, &action);
	} else {
		gpm_conf_get_string (manager->priv->conf, GPM_CONF_BATTERY_SLEEP_TYPE, &action);
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

		if (! gpm_control_is_policy_timout_valid (manager->priv->control, "timeout action")) {
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
	char *message;

	gpm_power_get_status_summary (manager->priv->power, &message, NULL);

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
	if (! gpm_control_is_policy_timout_valid (manager->priv->control, "power button press")) {
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
	if (! gpm_control_is_policy_timout_valid (manager->priv->control, "suspend button press")) {
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
	if (! gpm_control_is_policy_timout_valid (manager->priv->control, "hibernate button press")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, TRUE, "hibernate button press")) {
		return;
	}
	manager_policy_do (manager,
			   GPM_CONF_BUTTON_HIBERNATE,
			   _("the hibernate button has been pressed"));
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
	GpmAcAdapterState state;

	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);

	if (pressed) {
		if (state == GPM_AC_ADAPTER_PRESENT) {
			gpm_debug ("Performing AC policy");
			manager_policy_do (manager,
					   GPM_CONF_AC_BUTTON_LID,
					   _("the lid has been closed on ac power"));
		} else {
			gpm_debug ("Performing battery policy");
			manager_policy_do (manager,
					   GPM_CONF_BATTERY_BUTTON_LID,
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
ac_adapter_changed_cb (GpmAcAdapter     *ac_adapter,
		       GpmAcAdapterState state,
		       GpmManager       *manager)
{
	gboolean event_when_closed;

	gpm_debug ("Setting on-ac: %d", state);

	/* If we are on AC power we should show warnings again */
	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_debug ("Resetting warning to NONE as on AC power");
		manager->priv->last_primary = GPM_WARNING_NONE;
	}

	gpm_manager_sync_policy_sleep (manager);

	gpm_debug ("emitting on-ac-changed : %i", state);
	if (state == GPM_AC_ADAPTER_PRESENT) {
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
	    state == GPM_AC_ADAPTER_MISSING &&
	    gpm_button_is_lid_closed (manager->priv->button)) {
		manager_policy_do (manager,
				   GPM_CONF_BATTERY_BUTTON_LID,
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
			   GPM_CONF_BATTERY_CRITICAL,
			   _("battery is critically low"));
	return FALSE;
}

/**
 * battery_status_changed_primary:
 * @manager: This class instance
 * @battery_kind: The battery kind, e.g. GPM_POWER_KIND_PRIMARY
 * @battery_status: The battery status information
 *
 * Hander for the battery status changed event for primary devices
 * (laptop batteries). We notify of increasing power notification levels,
 * and also do the critical actions here.
 **/
static void
battery_status_changed_primary (GpmManager     *manager,
				GpmPowerKind    battery_kind,
				GpmPowerStatus *battery_status)
{
	GpmWarningState warning_type;
	gboolean show_notify;
	gchar *message = NULL;
	gchar *remaining = NULL;
	const gchar *title = NULL;
	gint timeout = 0;
	GpmAcAdapterState state;

	/* Wait until data is trusted... */
	if (gpm_power_get_data_is_trusted (manager->priv->power) == FALSE) {
		gpm_debug ("Data is not yet trusted.. wait..");
		return;
	}

	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);

	/* If we are charging we should show warnings again as soon as we discharge again */
	if (battery_status->is_charging) {
		gpm_debug ("Resetting warning to NONE as charging");
		manager->priv->last_primary = GPM_WARNING_NONE;
	}

	/* We use the hardware charged state instead of the old 99%->100%
	 * percentage charge method, as the icon must disappear when this
	 * notification is shown (if we set to the display policy "charging")
	 * and also the fact that some ACPI batteries don't make it to 100% */
	if (manager->priv->done_notify_fully_charged == FALSE &&
	    gpm_power_battery_is_charged (battery_status)) {

		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_BATTCHARGED, &show_notify);
		if (show_notify) {
			message = _("Your battery is now fully charged");
			title = _("Battery Charged");
			gpm_notify_display (manager->priv->notify,
					      title,
					      message,
					      GPM_NOTIFY_TIMEOUT_SHORT,
					      GPM_STOCK_BATTERY_CHARGED,
					      GPM_NOTIFY_URGENCY_LOW);
			gpm_info_event_log (manager->priv->info,
					    GPM_EVENT_NOTIFICATION,
					    title);
		}
		manager->priv->done_notify_fully_charged = TRUE;
	}

	/* We only re-enable the fully charged notification when the battery
	   drops down to 95% as some batteries charge to 100% and then fluctuate
	   from ~98% to 100%. See #338281 for details */
	if (battery_status->percentage_charge < 95 &&
	    battery_status->percentage_charge > 0 &&
	    gpm_power_battery_is_charged (battery_status) == FALSE) {
		manager->priv->done_notify_fully_charged = FALSE;
	}

	if (! battery_status->is_discharging) {
		gpm_debug ("%s is not discharging", gpm_power_kind_to_localised_string (battery_kind));
		return;
	}

	if (state == GPM_AC_ADAPTER_PRESENT) {
		gpm_debug ("Computer marked as on_ac.");
		return;
	}

	warning_type = gpm_warning_get_state (manager->priv->warning,
					     battery_status, GPM_WARNING_AUTO);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (warning_type <= manager->priv->last_primary) {
		gpm_debug ("Already notified %i", warning_type);
		return;
	}

	/* As the level is more critical than the last warning, save it */
	manager->priv->last_primary = warning_type;

	/* Do different warnings for each GPM_WARNING_* */
	if (warning_type == GPM_WARNING_ACTION) {
		gchar *action;
		timeout = GPM_NOTIFY_TIMEOUT_LONG;

		if (! gpm_control_is_policy_timout_valid (manager->priv->control, "critical action")) {
			return;
		}

		/* we have to do different warnings depending on the policy */
		gpm_conf_get_string (manager->priv->conf, GPM_CONF_BATTERY_CRITICAL, &action);

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
		g_timeout_add (1000*10, (GSourceFunc) manager_critical_action_do, manager);

	} else if (warning_type == GPM_WARNING_DISCHARGING) {

		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_ACADAPTER, &show_notify);
		if (show_notify) {
			message = g_strdup (_("The AC Power has been unplugged. "
					      "The system is now using battery power."));
			timeout = GPM_NOTIFY_TIMEOUT_SHORT;
		}

	} else {

		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_LOW_POWER, &show_notify);
		if (show_notify) {
			remaining = gpm_get_timestring (battery_status->remaining_time);
			message = g_strdup_printf (_("You have approximately <b>%s</b> "
						     "of remaining battery life (%d%%). "
						     "Plug in your AC Adapter to avoid losing data."),
						   remaining, battery_status->percentage_charge);
			timeout = GPM_NOTIFY_TIMEOUT_LONG;
			g_free (remaining);
		}
		gpm_sound_event (manager->priv->sound, GPM_SOUND_POWER_LOW);
	}

	/* If we had a message, print it as a notification */
	if (message) {
		gchar *icon;
		title = gpm_warning_get_title (warning_type);
		icon = gpm_power_get_icon_from_status (battery_status, battery_kind);
		gpm_notify_display (manager->priv->notify,
				      title, message, timeout,
				      icon, GPM_NOTIFY_URGENCY_NORMAL);
		g_free (icon);
		gpm_info_event_log (manager->priv->info,
				    GPM_EVENT_NOTIFICATION,
				    title);
		g_free (message);
	}
}

/**
 * battery_status_changed_ups:
 * @manager: This class instance
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
	GpmWarningState warning_type;
	gchar *message = NULL;
	gchar *remaining = NULL;
	const gchar *title = NULL;

	/* If we are charging we should show warnings again as soon as we discharge again */
	if (battery_status->is_charging) {
		manager->priv->last_ups = GPM_WARNING_NONE;
	}

	if (! battery_status->is_discharging) {
		gpm_debug ("%s is not discharging", gpm_power_kind_to_localised_string(battery_kind));
		return;
	}

	warning_type = gpm_warning_get_state (manager->priv->warning,
					     battery_status, GPM_WARNING_AUTO);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (warning_type <= manager->priv->last_ups) {
		gpm_debug ("Already notified %i", warning_type);
		return;
	}

	/* As the level is more critical than the last warning, save it */
	manager->priv->last_ups = warning_type;

	if (warning_type == GPM_WARNING_ACTION) {
		char *action;

		if (! gpm_control_is_policy_timout_valid (manager->priv->control, "critical action")) {
			return;
		}

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
		/* TODO: need to add 10 second delay so we get notification */
		manager_policy_do (manager, GPM_CONF_UPS_CRITICAL,
				   _("UPS is critically low"));
		gpm_sound_event (manager->priv->sound, GPM_SOUND_POWER_LOW);

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
		gpm_sound_event (manager->priv->sound, GPM_SOUND_POWER_LOW);
	}

	/* If we had a message, print it as a notification */
	if (message) {
		gchar *icon;
		title = gpm_warning_get_title (warning_type);
		icon = gpm_power_get_icon_from_status (battery_status, battery_kind);
		gpm_notify_display (manager->priv->notify,
				      title, message, GPM_NOTIFY_TIMEOUT_LONG,
				      icon, GPM_NOTIFY_URGENCY_NORMAL);
		gpm_info_event_log (manager->priv->info,
				    GPM_EVENT_NOTIFICATION,
				    title);
		g_free (icon);
		g_free (message);
	}
}

/**
 * battery_status_changed_misc:
 * @manager: This class instance
 * @battery_kind: The battery kind, e.g. GPM_POWER_KIND_MOUSE
 * @battery_status: The battery status information
 *
 * Hander for the battery status changed event for misc devices such as MOUSE
 * KEYBOARD or PDA. We only do warning notifications here as the devices
 * are not critical to the system power state.
 **/
static void
battery_status_changed_misc (GpmManager	    *manager,
			     GpmPowerKind    battery_kind,
			     GpmPowerStatus *battery_status)
{
	GpmWarningState warning_type;
	gchar *message = NULL;
	const gchar *title = NULL;
	const gchar *name;
	gchar *icon;

	/* mouse, keyboard and PDA have no time, just percentage */
	warning_type = gpm_warning_get_state (manager->priv->warning, battery_status,
					      GPM_WARNING_PERCENTAGE);

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE ||
	    warning_type == GPM_WARNING_DISCHARGING) {
		gpm_debug ("No warning");
		return;
	}

	/* Always check if we already notified the user */
	if (battery_kind == GPM_POWER_KIND_MOUSE) {
		if (warning_type > manager->priv->last_mouse) {
			manager->priv->last_mouse = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	} else if (battery_kind == GPM_POWER_KIND_KEYBOARD) {
		if (warning_type > manager->priv->last_keyboard) {
			manager->priv->last_keyboard = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	} else if (battery_kind == GPM_POWER_KIND_PDA) {
		if (warning_type > manager->priv->last_pda) {
			manager->priv->last_pda = warning_type;
		} else {
			warning_type = GPM_WARNING_NONE;
		}
	}

	/* no point continuing, we are not going to match */
	if (warning_type == GPM_WARNING_NONE) {
		gpm_debug ("No warning of type");
		return;
	}

	manager->priv->last_ups = warning_type;
	name = gpm_power_kind_to_localised_string (battery_kind);

	title = gpm_warning_get_title (warning_type);

	message = g_strdup_printf (_("The %s device attached to this computer "
				     "is low in power (%d%%). "
				     "This device will soon stop functioning "
				     "if not charged."),
				   name, battery_status->percentage_charge);

	icon = gpm_power_get_icon_from_status (battery_status, battery_kind);
	gpm_notify_display (manager->priv->notify,
			      title, message, GPM_NOTIFY_TIMEOUT_LONG,
			      icon, GPM_NOTIFY_URGENCY_NORMAL);

	gpm_sound_event (manager->priv->sound, GPM_SOUND_POWER_LOW);
	gpm_info_event_log (manager->priv->info, GPM_EVENT_NOTIFICATION, title);

	g_free (icon);
	g_free (message);
}

/**
 * power_battery_status_changed_cb:
 * @power: The power class instance
 * @battery_kind: The battery kind, e.g. GPM_POWER_KIND_MOUSE
 * @manager: This class instance
 *
 * This function splits up the battery status changed callback, and calls
 * different functions for each of the device types.
 **/
static void
power_battery_status_changed_cb (GpmPower    *power,
				 GpmPowerKind battery_kind,
				 GpmManager  *manager)
{
	GpmPowerStatus battery_status;

	gpm_tray_icon_sync (manager->priv->tray_icon);

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
	if (strcmp (key, GPM_CONF_BATTERY_SLEEP_COMPUTER) == 0 ||
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
gpm_manager_tray_icon_hibernate (GpmManager   *manager,
				 GpmTrayIcon  *tray)
{
	if (! gpm_control_is_policy_timout_valid (manager->priv->control, "hibernate signal")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, TRUE, "hibernate")) {
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
	if (! gpm_control_is_policy_timout_valid (manager->priv->control, "suspend signal")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, TRUE, "suspend")) {
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
	const gchar *error_title = NULL;
	const gchar *error_body = NULL;
	gchar *error_msg;

	hal_gpower_has_suspend_error (manager->priv->hal_power, &suspend_error);
	hal_gpower_has_hibernate_error (manager->priv->hal_power, &hibernate_error);

	if (suspend_error == TRUE) {
		error_title = _("Suspend failure");
		error_body = _("Your computer did not appear to resume correctly from suspend.");
	}
	if (hibernate_error == TRUE) {
		error_title = _("Hibernate failure");
		error_body = _("Your computer did not appear to resume correctly from hibernate.");
	}

	if (suspend_error == TRUE || hibernate_error == TRUE) {
		error_msg = g_strdup_printf ("%s\n%s\n%s", error_body,
					     _("This may be a driver or hardware problem."),
					     _("Check the GNOME Power Manager manual for common problems."));
		gpm_notify_display (manager->priv->notify,
				      error_title,
				      error_msg,
				      GPM_NOTIFY_TIMEOUT_LONG,
				      GTK_STOCK_DIALOG_WARNING,
				      GPM_NOTIFY_URGENCY_NORMAL);
		g_free (error_msg);
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
 * gpm_manager_init:
 * @manager: This class instance
 **/
static void
gpm_manager_init (GpmManager *manager)
{
	gboolean check_type_cpu;
	DBusGConnection *connection;
	GError *error = NULL;
	GpmAcAdapterState state;

	manager->priv = GPM_MANAGER_GET_PRIVATE (manager);
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	/* we want all notifications */
	manager->priv->last_ups = GPM_WARNING_NONE;
	manager->priv->last_mouse = GPM_WARNING_NONE;
	manager->priv->last_keyboard = GPM_WARNING_NONE;
	manager->priv->last_pda = GPM_WARNING_NONE;
	manager->priv->last_primary = GPM_WARNING_NONE;

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
	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);
	manager->priv->done_notify_fully_charged = FALSE;

	/* Don't notify on startup if we are on battery power */
	if (state == GPM_AC_ADAPTER_MISSING) {
		manager->priv->last_primary = GPM_WARNING_DISCHARGING;
	}

	/* Don't notify at startup if we are fully charged on AC */
	if (state == GPM_AC_ADAPTER_PRESENT) {
		manager->priv->done_notify_fully_charged = TRUE;
	}

	manager->priv->power = gpm_power_new ();
	g_signal_connect (manager->priv->power, "battery-status-changed",
			  G_CALLBACK (power_battery_status_changed_cb), manager);

	manager->priv->button = gpm_button_new ();
	g_signal_connect (manager->priv->button, "button-pressed",
			  G_CALLBACK (button_pressed_cb), manager);

	manager->priv->hal_power = hal_gpower_new ();
	manager->priv->warning = gpm_warning_new ();
	manager->priv->sound = gpm_sound_new ();

	/* try and start an interactive service */
	manager->priv->cpufreq = gpm_cpufreq_new ();

	/* try and start an interactive service */
	manager->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (manager->priv->screensaver, "auth-request",
 			  G_CALLBACK (screensaver_auth_request_cb), manager);
	manager->priv->srv_screensaver = gpm_srv_screensaver_new ();

	/* try an start an interactive service */
	manager->priv->srv_backlight = gpm_srv_backlight_new ();
	if (manager->priv->srv_backlight != NULL) {
		/* add the new brightness lcd DBUS interface */
		dbus_g_object_type_install_info (GPM_TYPE_SRV_BACKLIGHT,
						 &dbus_glib_gpm_backlight_object_info);
		dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_BACKLIGHT,
						     G_OBJECT (manager->priv->srv_backlight));
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
	if (manager->priv->control != NULL) {
		/* add the new brightness lcd DBUS interface */
		dbus_g_object_type_install_info (GPM_TYPE_CONTROL,
						 &dbus_glib_gpm_control_object_info);
		dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_CONTROL,
						     G_OBJECT (manager->priv->control));
	}

	gpm_debug ("creating new tray icon");
	manager->priv->tray_icon = gpm_tray_icon_new ();
	if (manager->priv->tray_icon != NULL) {
		dbus_g_object_type_install_info (GPM_TYPE_TRAY_ICON,
						 &dbus_glib_gpm_ui_object_info);
		dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_UI,
						     G_OBJECT (manager->priv->tray_icon));
	}

	gpm_debug ("initialising info infrastructure");
	manager->priv->info = gpm_info_new ();

	/* add the new statistics DBUS interface */
	dbus_g_object_type_install_info (GPM_TYPE_INFO, &dbus_glib_gpm_statistics_object_info);
	dbus_g_connection_register_g_object (connection, GPM_DBUS_PATH_STATS,
					     G_OBJECT (manager->priv->info));

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

	gpm_manager_sync_policy_sleep (manager);
	gpm_tray_icon_sync (manager->priv->tray_icon);

	/* on startup, check if there are suspend errors left */
	gpm_manager_check_sleep_errors (manager);
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
	g_object_unref (manager->priv->warning);
	g_object_unref (manager->priv->dpms);
	g_object_unref (manager->priv->idle);
	g_object_unref (manager->priv->info);
	g_object_unref (manager->priv->power);
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
	if (manager->priv->srv_backlight) {
		g_object_unref (manager->priv->srv_backlight);
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
