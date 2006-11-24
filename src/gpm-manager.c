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
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-client.h>
#include <gnome-keyring-1/gnome-keyring.h>
#include "gpm-ac-adapter.h"
#include "gpm-battery.h"
#include "gpm-button.h"
#include "gpm-conf.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-dpms.h"
#include "gpm-hal.h"
#include "gpm-idle.h"
#include "gpm-info.h"
#include "gpm-inhibit.h"
#include "gpm-interface-statistics.h"
#include "gpm-networkmanager.h"
#include "gpm-manager.h"
#include "gpm-notify.h"
#include "gpm-power.h"
#include "gpm-policy.h"
#include "gpm-prefs.h"
#include "gpm-screensaver.h"
#include "gpm-srv-brightness-lcd.h"
#include "gpm-srv-brightness-kbd.h"
#include "gpm-srv-cpufreq.h"
#include "gpm-srv-dpms.h"
#include "gpm-srv-screensaver.h"
#include "gpm-stock-icons.h"
#include "gpm-sound.h"
#include "gpm-tray-icon.h"
#include "gpm-warning.h"

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
	GpmHal			*hal;
	GpmIdle			*idle;
	GpmInfo			*info;
	GpmInhibit		*inhibit;
	GpmNotify		*notify;
	GpmPower		*power;
	GpmPolicy		*policy;
	GpmScreensaver 		*screensaver;
	GpmSound 		*sound;
	GpmTrayIcon		*tray_icon;
	GpmWarning		*warning;

	/* interactive services */
	GpmSrvBrightnessLcd	*srv_brightness_lcd;
	GpmSrvBrightnessKbd	*srv_brightness_kbd;
	GpmSrvCpuFreq	 	*srv_cpufreq;
	GpmSrvDpms	 	*srv_dpms;
	GpmSrvScreensaver 	*srv_screensaver;

	GpmWarningState		 last_primary;
	GpmWarningState		 last_ups;
	GpmWarningState		 last_mouse;
	GpmWarningState		 last_keyboard;
	GpmWarningState		 last_pda;

	gboolean		 done_notify_fully_charged;

	time_t			 last_resume_event;
	guint			 suppress_policy_timeout;
};

enum {
	ON_AC_CHANGED,
	DPMS_MODE_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

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
 * @manager: This class instance
 * @action: The action we want to do, e.g. "suspend"
 *
 * Checks if the difference in time between this request for an action, and
 * the last action completing is larger than the timeout set in gconf.
 *
 * Return value: TRUE if we can perform the action.
 **/
static gboolean
gpm_manager_is_policy_timout_valid (GpmManager  *manager,
				    const gchar *action)
{
	gchar *message;
	if ((time (NULL) - manager->priv->last_resume_event) <=
	    manager->priv->suppress_policy_timeout) {
		message = g_strdup_printf ("Skipping suppressed %s", action);
		gpm_info_event_log (manager->priv->info,
				    GPM_EVENT_NOTIFICATION, message);
		g_free (message);
		return FALSE;
	}
	return TRUE;
}

/**
 * gpm_manager_reset_event_time:
 * @manager: This class instance
 *
 * Resets the time so we do not do any more actions until the
 * timeout has passed.
 **/
static void
gpm_manager_reset_event_time (GpmManager *manager)
{
	manager->priv->last_resume_event = time (NULL);
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
			      const char *action)
{
	gboolean action_ok;
	gchar *title;

	action_ok = gpm_inhibit_check (manager->priv->inhibit);
	if (! action_ok) {
		GString *message = g_string_new ("");

		title = g_strdup_printf (_("Request to %s"), action);
		gpm_inhibit_get_message (manager->priv->inhibit, message, action);
		gpm_notify_display (manager->priv->notify,
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
 * gpm_manager_allowed_suspend:
 * @manager: This class instance
 * @can: If we can suspend
 *
 * Checks the HAL key power_management.can_suspend_to_ram and also
 * checks gconf to see if we are allowed to suspend this computer.
 **/
gboolean
gpm_manager_allowed_suspend (GpmManager *manager,
			     gboolean   *can,
			     GError    **error)
{
	g_return_val_if_fail (can, FALSE);
	gpm_policy_allowed_suspend (manager->priv->policy, can);
	return TRUE;
}

/**
 * gpm_manager_allowed_hibernate:
 * @manager: This class instance
 * @can: If we can hibernate
 *
 * Checks the HAL key power_management.can_suspend_to_disk and also
 * checks gconf to see if we are allowed to hibernate this computer.
 **/
gboolean
gpm_manager_allowed_hibernate (GpmManager *manager,
			       gboolean   *can,
			       GError    **error)
{
	g_return_val_if_fail (can, FALSE);
	gpm_policy_allowed_hibernate (manager->priv->policy, can);
	return TRUE;
}

/**
 * gpm_manager_allowed_shutdown:
 * @manager: This class instance
 * @can: If we can shutdown
 *
 **/
gboolean
gpm_manager_allowed_shutdown (GpmManager *manager,
			      gboolean   *can,
			      GError    **error)
{
	g_return_val_if_fail (can, FALSE);
	gpm_policy_allowed_shutdown (manager->priv->policy, can);
	return TRUE;
}

/**
 * gpm_manager_allowed_reboot:
 * @manager: This class instance
 * @can: If we can reboot
 *
 * Stub function -- TODO.
 **/
gboolean
gpm_manager_allowed_reboot (GpmManager *manager,
			    gboolean   *can,
			    GError    **error)
{
	g_return_val_if_fail (can, FALSE);
	gpm_policy_allowed_reboot (manager->priv->policy, can);
	return TRUE;
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

	gpm_hal_enable_power_save (manager->priv->hal, power_save);

	/* set the new sleep (inactivity) value */
	gpm_idle_set_system_timeout (manager->priv->idle, sleep_computer);
}

/**
 * gpm_manager_get_lock_policy:
 * @manager: This class instance
 * @policy: The policy gconf string.
 *
 * This function finds out if we should lock the screen when we do an
 * action. It is required as we can either use the gnome-screensaver policy
 * or the custom policy. See the yelp file for more information.
 *
 * Return value: TRUE if we should lock.
 **/
static gboolean
gpm_manager_get_lock_policy (GpmManager  *manager,
			     const gchar *policy)
{
	gboolean do_lock;
	gboolean use_ss_setting;
	/* This allows us to over-ride the custom lock settings set in gconf
	   with a system default set in gnome-screensaver.
	   See bug #331164 for all the juicy details. :-) */
	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_LOCK_USE_SCREENSAVER, &use_ss_setting);
	if (use_ss_setting) {
		do_lock = gpm_screensaver_lock_enabled (manager->priv->screensaver);
		gpm_debug ("Using ScreenSaver settings (%i)", do_lock);
	} else {
		gpm_conf_get_bool (manager->priv->conf, policy, &do_lock);
		gpm_debug ("Using custom locking settings (%i)", do_lock);
	}
	return do_lock;
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

	do_lock = gpm_manager_get_lock_policy (manager,
					       GPM_CONF_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		if (!gpm_screensaver_lock (manager->priv->screensaver))
			gpm_debug ("Could not lock screen via gnome-screensaver");
	}
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
	gpm_dpms_set_mode (manager->priv->dpms, GPM_DPMS_MODE_ON, &error);
	if (error) {
		gpm_debug ("Unable to set DPMS mode: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	}

	do_lock = gpm_manager_get_lock_policy (manager, GPM_CONF_LOCK_ON_BLANK_SCREEN);
	if (do_lock) {
		gpm_screensaver_poke (manager->priv->screensaver);
	}
	return ret;
}

/**
 * manager_explain_reason:
 * @manager: This class instance
 * @event: The event type, e.g. GPM_EVENT_DPMS_OFF
 * @pre: The action we are about to do, e.g. "Suspending computer"
 * @post: The reason we are performing the policy action, e.g. "battery critical"
 *
 * Helper function
 **/
static void
manager_explain_reason (GpmManager   *manager,
			GpmGraphWidgetEvent event,
			const gchar  *pre,
			const gchar  *post)
{
	gchar *message;
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
	if (gpm_manager_is_policy_timout_valid (manager, "policy event") == FALSE) {
		return;
	}

	if (strcmp (action, ACTION_NOTHING) == 0) {
		manager_explain_reason (manager, GPM_EVENT_NOTIFICATION,
					_("Doing nothing"), reason);

	} else if (strcmp (action, ACTION_SUSPEND) == 0) {
		manager_explain_reason (manager, GPM_EVENT_SUSPEND,
					_("Suspending computer"), reason);
		gpm_manager_suspend (manager, NULL);

	} else if (strcmp (action, ACTION_HIBERNATE) == 0) {
		manager_explain_reason (manager, GPM_EVENT_HIBERNATE,
					_("Hibernating computer"), reason);
		gpm_manager_hibernate (manager, NULL);

	} else if (strcmp (action, ACTION_BLANK) == 0) {
		gpm_manager_blank_screen (manager, NULL);

	} else if (strcmp (action, ACTION_SHUTDOWN) == 0) {
		manager_explain_reason (manager, GPM_EVENT_NOTIFICATION,
					_("Shutting down computer"), reason);
		gpm_manager_shutdown (manager, NULL);

	} else if (strcmp (action, ACTION_INTERACTIVE) == 0) {
		manager_explain_reason (manager, GPM_EVENT_NOTIFICATION,
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
 * gpm_manager_set_dpms_mode:
 * @manager: This class instance
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_STANDBY
 * Return value: TRUE if we could set the DPMS mode OK.
 **/
gboolean
gpm_manager_set_dpms_mode (GpmManager  *manager,
			   const gchar *mode,
			   GError     **error)
{
	gboolean ret;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	gpm_debug ("Setting DPMS to %s", mode);

	/* just proxy this */
	ret = gpm_dpms_set_mode (manager->priv->dpms,
				 gpm_dpms_mode_from_string (mode), error);

	return ret;
}

/**
 * gpm_manager_get_dpms_mode:
 * @manager: This class instance
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_STANDBY
 * Return value: TRUE if we could get the GPMS mode OK.
 **/
gboolean
gpm_manager_get_dpms_mode (GpmManager   *manager,
			   const gchar **mode,
			   GError      **error)
{
	gboolean ret;
	GpmDpmsMode m;

	g_return_val_if_fail (GPM_IS_MANAGER (manager), FALSE);

	ret = gpm_dpms_get_mode (manager->priv->dpms, &m, error);
	gpm_debug ("Got DPMS mode result=%d mode=%d", ret, m);
	if (ret && mode) {
		*mode = gpm_dpms_mode_to_string (m);
	}

	return ret;
}

/**
 * gpm_manager_inhibit:
 * @manager: This class instance
 * @application: The application that sent the request, e.g. "Nautilus"
 * @reason: The reason given to inhibit, e.g. "Copying files"
 * @context: The context we are talking to
 *
 * Processes an inhibit request from an application that want to stop the
 * idle action suspend from happening.
 **/
void
gpm_manager_inhibit (GpmManager	 *manager,
		     const gchar *application,
		     const gchar *reason,
		     DBusGMethodInvocation *context,
		     GError     **error)
{
	guint32 cookie;
	const gchar *connection = dbus_g_method_get_sender (context);
	cookie = gpm_inhibit_add (manager->priv->inhibit, connection, application, reason);
	dbus_g_method_return (context, cookie);
}

/**
 * gpm_manager_uninhibit:
 * @manager: This class instance
 * @cookie: The application cookie, e.g. 17534
 * @context: The context we are talking to
 *
 * Processes an allow request from an application that want to allow the
 * idle action suspend to happen.
 **/
void
gpm_manager_uninhibit (GpmManager	 *manager,
		       guint32		  cookie,
		       DBusGMethodInvocation *context,
		       GError		**error)
{
	const gchar *connection = dbus_g_method_get_sender (context);
	gpm_inhibit_remove (manager->priv->inhibit, connection, cookie);
	dbus_g_method_return (context);
}

/**
 * gpm_manager_shutdown:
 * @manager: This class instance
 *
 * Shuts down the computer, saving the session if possible.
 **/
static gboolean
gpm_manager_shutdown (GpmManager *manager,
		      GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean save_session;

	gpm_policy_allowed_shutdown (manager->priv->policy, &allowed);
	if (! allowed) {
		gpm_warning ("Cannot shutdown");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot shutdown");
		return FALSE;
	}

	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_SESSION_REQUEST_SAVE, &save_session);
	/* We can set g-p-m to not save the session to avoid confusing new
	   users. By default we save the session to preserve data. */
	if (save_session == TRUE) {
		gnome_client_request_save (gnome_master_client (),
					   GNOME_SAVE_GLOBAL,
					   FALSE, GNOME_INTERACT_NONE, FALSE,  TRUE);
	}
	gpm_hal_shutdown (manager->priv->hal);
	ret = TRUE;

	return ret;
}

/**
 * gpm_manager_reboot:
 * @manager: This class instance
 *
 * Reboots the computer, saving the session if possible.
 **/
static gboolean
gpm_manager_reboot (GpmManager *manager,
		    GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean save_session;

	gpm_policy_allowed_reboot (manager->priv->policy, &allowed);
	if (! allowed) {
		gpm_warning ("Cannot reboot");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot reboot");
		return FALSE;
	}

	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_SESSION_REQUEST_SAVE, &save_session);
	/* We can set g-p-m to not save the session to avoid confusing new
	   users. By default we save the session to preserve data. */
	if (save_session) {
		gnome_client_request_save (gnome_master_client (),
					   GNOME_SAVE_GLOBAL,
					   FALSE, GNOME_INTERACT_NONE, FALSE,  TRUE);
	}

	gpm_hal_reboot (manager->priv->hal);
	ret = TRUE;

	return ret;
}

/**
 * gpm_manager_hibernate:
 * @manager: This class instance
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
        gboolean nm_sleep;
	GnomeKeyringResult keyres;

	gpm_policy_allowed_hibernate (manager->priv->policy, &allowed);

	if (! allowed) {
		gpm_syslog ("cannot hibernate as not allowed from policy");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot hibernate");
		gpm_sound_event (manager->priv->sound, GPM_SOUND_SUSPEND_FAILURE);
		return FALSE;
	}

	/* we should lock keyrings when sleeping #375681 */
	keyres = gnome_keyring_lock_all_sync ();
	if (keyres != GNOME_KEYRING_RESULT_OK) {
		gpm_debug ("could not lock keyring");
	}

	do_lock = gpm_manager_get_lock_policy (manager,
					       GPM_CONF_LOCK_ON_HIBERNATE);
	if (do_lock) {
		gpm_screensaver_lock (manager->priv->screensaver);
	}

	gpm_conf_get_bool (manager->priv->conf,  GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep) {
		gpm_networkmanager_sleep ();
	}

	ret = gpm_hal_hibernate (manager->priv->hal);
	manager_explain_reason (manager, GPM_EVENT_RESUME,
				_("Resuming computer"), NULL);

	/* we need to refresh all the power caches */
	gpm_power_update_all (manager->priv->power);

	if (! ret) {
		gchar *message;
		gboolean show_notify;

		gpm_sound_event (manager->priv->sound, GPM_SOUND_SUSPEND_FAILURE);

		/* We only show the HAL failed notification if set in gconf */
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_HAL_ERROR, &show_notify);
		if (show_notify) {
			const gchar *title = _("Hibernate Problem");
			message = g_strdup_printf (_("HAL failed to %s. "
						     "Check the help file for common problems."),
						     _("hibernate"));
			gpm_syslog ("hibernate failed");
			gpm_notify_display (manager->priv->notify,
					      title,
					      message,
					      GPM_NOTIFY_TIMEOUT_LONG,
					      GTK_STOCK_DIALOG_WARNING,
					      GPM_NOTIFY_URGENCY_LOW);
			gpm_info_event_log (manager->priv->info,
					    GPM_EVENT_NOTIFICATION, title);
			g_free (message);
		}
	}

	if (do_lock) {
		gpm_screensaver_poke (manager->priv->screensaver);
	}

	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep) {
		gpm_networkmanager_wake ();
	}

	gpm_srv_dpms_sync_policy (manager->priv->srv_dpms);

	/* save the time that we resumed */
	gpm_manager_reset_event_time (manager);

	return ret;
}

/**
 * gpm_manager_suspend:
 * @manager: This class instance
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
	gboolean nm_sleep;
	GnomeKeyringResult keyres;

	gpm_policy_allowed_suspend (manager->priv->policy, &allowed);

	if (! allowed) {
		gpm_syslog ("cannot suspend as not allowed from policy");
		g_set_error (error,
			     GPM_MANAGER_ERROR,
			     GPM_MANAGER_ERROR_GENERAL,
			     "Cannot suspend");
		gpm_sound_event (manager->priv->sound, GPM_SOUND_SUSPEND_FAILURE);
		return FALSE;
	}

	/* we should lock keyrings when sleeping #375681 */
	keyres = gnome_keyring_lock_all_sync ();
	if (keyres != GNOME_KEYRING_RESULT_OK) {
		gpm_debug ("could not lock keyring");
	}

	do_lock = gpm_manager_get_lock_policy (manager,
					       GPM_CONF_LOCK_ON_SUSPEND);
	if (do_lock) {
		gpm_screensaver_lock (manager->priv->screensaver);
	}

	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep) {
		gpm_networkmanager_sleep ();
	}

	/* Do the suspend */
	ret = gpm_hal_suspend (manager->priv->hal, 0);
	manager_explain_reason (manager, GPM_EVENT_RESUME,
				_("Resuming computer"), NULL);

	/* We need to refresh all the power caches */
	gpm_power_update_all (manager->priv->power);

	if (! ret) {
		gboolean show_notify;

		gpm_sound_event (manager->priv->sound, GPM_SOUND_SUSPEND_FAILURE);

		/* We only show the HAL failed notification if set in gconf */
		gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NOTIFY_HAL_ERROR, &show_notify);
		if (show_notify) {
			const gchar *title;
			gchar *message;
			message = g_strdup_printf (_("Your computer failed to %s.\n"
						     "Check the help file for common problems."),
						     _("suspend"));
			title = _("Suspend Problem");
			gpm_syslog ("suspend failed");
			gpm_notify_display (manager->priv->notify,
					      title,
					      message,
					      GPM_NOTIFY_TIMEOUT_LONG,
					      GTK_STOCK_DIALOG_WARNING,
					      GPM_NOTIFY_URGENCY_LOW);
			gpm_info_event_log (manager->priv->info,
					    GPM_EVENT_NOTIFICATION,
					    title);
			g_free (message);
			gpm_sound_event (manager->priv->sound, GPM_SOUND_SUSPEND_FAILURE);
		}
	}

	if (do_lock) {
		gpm_screensaver_poke (manager->priv->screensaver);
	}

	gpm_conf_get_bool (manager->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep) {
		gpm_networkmanager_wake ();
	}

	gpm_srv_dpms_sync_policy (manager->priv->srv_dpms);

	/* save the time that we resumed */
	gpm_manager_reset_event_time (manager);

	return ret;
}

/**
 * gpm_manager_suspend_dbus_method:
 * @manager: This class instance
 **/
gboolean
gpm_manager_suspend_dbus_method (GpmManager *manager,
				 GError    **error)
{
	/* FIXME: From where? */
	manager_explain_reason (manager, GPM_EVENT_SUSPEND,
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
	manager_explain_reason (manager, GPM_EVENT_HIBERNATE,
				_("Hibernating computer"),
				_("the DBUS method Hibernate() was invoked"));
	return gpm_manager_hibernate (manager, error);
}

/**
 * gpm_manager_shutdown_dbus_method:
 * @manager: This class instance
 **/
gboolean
gpm_manager_shutdown_dbus_method (GpmManager *manager,
				  GError    **error)
{
	/* FIXME: From where? */
	manager_explain_reason (manager, GPM_EVENT_NOTIFICATION,
				_("Shutting down computer"),
				_("the DBUS method Shutdown() was invoked"));
	return gpm_manager_shutdown (manager, error);
}

/**
 * gpm_manager_reboot_dbus_method:
 * @manager: This class instance
 **/
gboolean
gpm_manager_reboot_dbus_method (GpmManager *manager,
				GError    **error)
{
	/* FIXME: From where? */
	manager_explain_reason (manager, GPM_EVENT_NOTIFICATION,
				_("Rebooting computer"),
				_("the DBUS method Reboot() was invoked"));
	return gpm_manager_reboot (manager, error);
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
	GpmAcAdapterState state;
	gboolean laptop_using_ext_mon;

	/* find if we are on AC power */
	gpm_ac_adapter_get_state (manager->priv->ac_adapter, &state);

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
	if (button_is_lid_closed (manager->priv->button) == TRUE && laptop_using_ext_mon == FALSE) {
		gpm_debug ("lid is closed, so we are ignoring idle state changes");
		return;
	}

	if (mode == GPM_IDLE_MODE_NORMAL) {

		gpm_debug ("Idle state changed: NORMAL");

	} else if (mode == GPM_IDLE_MODE_SESSION) {

		gpm_debug ("Idle state changed: SESSION");

	} else if (mode == GPM_IDLE_MODE_SYSTEM) {
		gpm_debug ("Idle state changed: SYSTEM");

		if (! gpm_manager_is_policy_timout_valid (manager, "timeout action")) {
			return;
		}
		if (! gpm_manager_is_inhibit_valid (manager, "timeout action")) {
			return;
		}
		/* can only be hibernate, suspend or nothing */
		if (state == GPM_AC_ADAPTER_PRESENT) {
			manager_policy_do (manager, GPM_CONF_AC_SLEEP_TYPE, _("the system state is idle"));
		} else {
			manager_policy_do (manager, GPM_CONF_BATTERY_SLEEP_TYPE, _("the system state is idle"));
		}
	}
}

/**
 * dpms_mode_changed_cb:
 * @mode: The DPMS mode, e.g. GPM_DPMS_MODE_OFF
 * @manager: This class instance
 *
 * What happens when the DPMS mode is changed.
 **/
static void
dpms_mode_changed_cb (GpmDpms    *dpms,
		      GpmDpmsMode mode,
		      GpmManager *manager)
{
	gpm_debug ("DPMS mode changed: %d", mode);

	gpm_debug ("emitting dpms-mode-changed : %s", gpm_dpms_mode_to_string (mode));
	g_signal_emit (manager,
			signals [DPMS_MODE_CHANGED],
			0,
			gpm_dpms_mode_to_string (mode));
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
	if (! gpm_manager_is_policy_timout_valid (manager, "power button press")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "power button press")) {
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
	if (! gpm_manager_is_policy_timout_valid (manager, "suspend button press")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "suspend button press")) {
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
	if (! gpm_manager_is_policy_timout_valid (manager, "hibernate button press")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "hibernate button press")) {
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
	    button_is_lid_closed (manager->priv->button)) {
		manager_policy_do (manager,
				   GPM_CONF_BATTERY_BUTTON_LID,
				   _("the lid has been closed, and the ac adapter "
				     "removed (and gconf is okay)"));
	}

	/* Don't do any events for a few seconds after we remove the
	 * ac_adapter. See #348201 for details */
	gpm_manager_reset_event_time (manager);
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

		if (! gpm_manager_is_policy_timout_valid (manager, "critical action")) {
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
		remaining = gpm_get_timestring (battery_status->remaining_time);
		message = g_strdup_printf (_("You have approximately <b>%s</b> "
					     "of remaining battery life (%d%%). "
					     "Plug in your AC Adapter to avoid losing data."),
					   remaining, battery_status->percentage_charge);
		timeout = GPM_NOTIFY_TIMEOUT_LONG;
		g_free (remaining);
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

		if (! gpm_manager_is_policy_timout_valid (manager, "critical action")) {
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

	} else if (strcmp (key, GPM_CONF_POLICY_TIMEOUT) == 0) {
		 gpm_conf_get_uint (manager->priv->conf, GPM_CONF_POLICY_TIMEOUT,
		 		    &manager->priv->suppress_policy_timeout);
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
	if (! gpm_manager_is_policy_timout_valid (manager, "hibernate signal")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "hibernate")) {
		return;
	}

	manager_explain_reason (manager,
				GPM_EVENT_HIBERNATE,
				_("Hibernating computer"),
				_("user clicked hibernate from tray menu"));
	gpm_manager_hibernate (manager, NULL);
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
	if (! gpm_manager_is_policy_timout_valid (manager, "suspend signal")) {
		return;
	}
	if (! gpm_manager_is_inhibit_valid (manager, "suspend")) {
		return;
	}
	manager_explain_reason (manager,
				GPM_EVENT_SUSPEND,
				_("Suspending computer"),
				_("user clicked suspend from tray menu"));
	gpm_manager_suspend (manager, NULL);
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

	gpm_hal_has_suspend_error (manager->priv->hal, &suspend_error);
	gpm_hal_has_hibernate_error (manager->priv->hal, &hibernate_error);

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
	/* only clear errors if we have finished the authentication */
	if (auth_begin == FALSE) {
		gpm_hal_clear_suspend_error (manager->priv->hal);
		gpm_hal_clear_hibernate_error (manager->priv->hal);
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
	GpmHal *hal = gpm_hal_new ();
	gpm_hal_clear_suspend_error (hal);
	gpm_hal_clear_hibernate_error (hal);
	g_object_unref (hal);
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

	manager->priv->hal = gpm_hal_new ();
	manager->priv->warning = gpm_warning_new ();
	manager->priv->sound = gpm_sound_new ();

	/* try and start an interactive service */
	manager->priv->srv_cpufreq = gpm_srv_cpufreq_new ();

	/* try and start an interactive service */
	manager->priv->srv_dpms = gpm_srv_dpms_new ();

	/* try and start an interactive service */
	manager->priv->screensaver = gpm_screensaver_new ();
	g_signal_connect (manager->priv->screensaver, "auth-request",
 			  G_CALLBACK (screensaver_auth_request_cb), manager);
	manager->priv->srv_screensaver = gpm_srv_screensaver_new ();

	/* try an start an interactive service */
	manager->priv->srv_brightness_lcd = gpm_srv_brightness_lcd_new ();
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
	manager->priv->inhibit = gpm_inhibit_new ();

	/* use the policy object */
	manager->priv->policy = gpm_policy_new ();

	gpm_debug ("creating new tray icon");
	manager->priv->tray_icon = gpm_tray_icon_new ();

	gpm_debug ("initialising info infrastructure");
	manager->priv->info = gpm_info_new ();

	/* add the new statistics DBUS interface */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	dbus_g_object_type_install_info (GPM_TYPE_INFO, &dbus_glib_gpm_statistics_object_info);
	dbus_g_connection_register_g_object (connection, "/org/gnome/PowerManager/Statistics",
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

	g_signal_connect (manager->priv->dpms, "mode-changed",
			  G_CALLBACK (dpms_mode_changed_cb), manager);

	gpm_conf_get_uint (manager->priv->conf, GPM_CONF_POLICY_TIMEOUT,
			   &manager->priv->suppress_policy_timeout);
	gpm_debug ("Using a supressed policy timeout of %i seconds",
		   manager->priv->suppress_policy_timeout);

	/* Pretend we just resumed when we start to let actions settle */
	gpm_manager_reset_event_time (manager);

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
	g_object_unref (manager->priv->hal);
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
	if (manager->priv->srv_cpufreq) {
		g_object_unref (manager->priv->srv_cpufreq);
	}
	if (manager->priv->srv_dpms) {
		g_object_unref (manager->priv->srv_dpms);
	}
	if (manager->priv->srv_brightness_lcd) {
		g_object_unref (manager->priv->srv_brightness_lcd);
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
