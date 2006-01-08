/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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
#include <gconf/gconf-client.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-notification.h"
#include "gpm-screensaver.h"
#include "gpm-networkmanager.h"
#include "gpm-libnotify.h"

#include "gpm-dbus-server.h"

/* FIXME: we should abstract the HAL stuff */
#include "gpm-hal.h"

#include "gpm-idle.h"
#include "gpm-hal-monitor.h"
#include "gpm-manager.h"

static void     gpm_manager_class_init (GpmManagerClass *klass);
static void     gpm_manager_init       (GpmManager      *manager);
static void     gpm_manager_finalize   (GObject         *object);

#define GPM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_MANAGER, GpmManagerPrivate))

struct GpmManagerPrivate
{
        gboolean       on_ac;

        GConfClient   *gconf_client;

        GpmIdle       *idle;
        GpmHalMonitor *hal_monitor;
};

enum {
        PROP_0,
        PROP_ON_AC
};

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (GpmManager, gpm_manager, G_TYPE_OBJECT)

#undef DISABLE_ACTIONS_FOR_TESTING

/** Do all the action when we go from batt to ac, or ac to batt (or coldplug)
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
                     gboolean    on_ac)
{
	int          brightness;
        int          sleep_display;
        int          sleep_computer;
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
	gpm_idle_set_session_timeout (manager->priv->idle, sleep_display);
        gpm_idle_set_system_timeout (manager->priv->idle, sleep_computer);
}

static void
maybe_notify_on_ac_changed (GpmManager *manager,
                            gboolean    on_ac)
{
        gboolean show_notify;

        show_notify = gconf_client_get_bool (manager->priv->gconf_client,
                                             GPM_PREF_NOTIFY_ACADAPTER, NULL);

        if (! on_ac) {
                if (show_notify) {
                        gpm_libnotify_event (_("AC Power Unplugged"),
                                             _("The AC Power has been unplugged. "
                                               "The system is now using battery power."),
                                             LIBNOTIFY_URGENCY_NORMAL,
                                             gpm_notification_get_icon ());
                }
        } else {

                /*
                 * for where we add back the ac_adapter before
                 * the "AC Power unplugged" message times out.
                 */
                gpm_libnotify_clear ();
        }

        /* update icon */
        gpm_notification_icon_update ();
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
maybe_notify_battery_power_changed (GpmManager *manager,
                                    int         percentage,
                                    glong       minutes,
                                    gboolean    discharging,
                                    gboolean    primary)
{
        gboolean show_notify;
	gint     low_threshold;
	gint     critical_threshold;
	gchar   *message;
	gchar   *remaining;

	g_debug ("percentage = %d, minutes = %ld, discharging = %d, primary = %d",
		 percentage, minutes, discharging, primary);

        /* give notification @100% */
        if (primary && percentage >= 100) {
                show_notify = gconf_client_get_bool (gconf_client_get_default (),
                                                     GPM_PREF_NOTIFY_BATTCHARGED, NULL);
                if (show_notify)
                        gpm_libnotify_event (_("Battery Charged"),
                                             _("Your battery is now fully charged"),
                                             LIBNOTIFY_URGENCY_LOW,
                                             gpm_notification_get_icon ());
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
		gpm_libnotify_event (_("Battery Critically Low"),
				     message,
				     LIBNOTIFY_URGENCY_CRITICAL,
				     gpm_notification_get_icon ());
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

		gpm_libnotify_event (_("Battery Low"),
				     message,
				     LIBNOTIFY_URGENCY_CRITICAL,
				     gpm_notification_get_icon ());
		g_free (message);
		g_free (remaining);

		return;
	}

        /* update icon */
        gpm_notification_icon_update ();
}

void
gpm_manager_set_on_ac (GpmManager *manager,
                       gboolean    on_ac)
{
        g_return_if_fail (GS_IS_MANAGER (manager));

        if (on_ac != manager->priv->on_ac) {
                manager->priv->on_ac = on_ac;

		g_debug ("Setting on-ac: %d", on_ac);

                maybe_notify_on_ac_changed (manager, on_ac);
                gpm_emit_mains_changed (on_ac);

                change_power_policy (manager, on_ac);
        }
}

gboolean
gpm_manager_get_on_ac (GpmManager *manager)
{
        gboolean on_ac;

        g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

        on_ac = manager->priv->on_ac;

        return on_ac;
}

void
gpm_manager_shutdown (GpmManager *manager)
{
#ifndef DISABLE_ACTIONS_FOR_TESTING
        gnome_client_request_save (gnome_master_client (),
                                   GNOME_SAVE_GLOBAL,
                                   FALSE, GNOME_INTERACT_NONE, FALSE,  TRUE);
        gpm_hal_shutdown ();
#else
        g_debug ("Shutdown disabled for testing");
#endif
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
#ifndef DISABLE_ACTIONS_FOR_TESTING
	gboolean should_lock = gpm_screensaver_lock_enabled ();

	if (!gpm_hal_can_hibernate ()) {
		g_warning ("Cannot hibernate as disabled in HAL");
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

#else
        g_debug ("Hibernate disabled for testing");
#endif
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
#ifndef DISABLE_ACTIONS_FOR_TESTING
	gboolean should_lock = gpm_screensaver_lock_enabled ();

	if (! gpm_hal_can_suspend ()) {
		g_warning ("Cannot suspend as disabled in HAL");
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
#else
        g_debug ("Suspend disabled for testing");
#endif
}

/** Callback for the idle function.
 */
static void
idle_changed_cb (GpmIdle    *idle,
                 GpmIdleMode mode,
                 GpmManager *manager)
{

        switch (mode) {
	case GPM_IDLE_MODE_NORMAL:
                /* FIXME: Disable DPMS */
                g_debug ("Idle state changed: NORMAL");

		break;
	case GPM_IDLE_MODE_SESSION:
                /* FIXME: Enable DPMS */
                g_debug ("Idle state changed: SESSION");

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
hal_power_button_cb (GpmHalMonitor *monitor,
                     gboolean       state,
                     GpmManager    *manager)
{
        /* Log out interactively */
        gnome_client_request_save (gnome_master_client (),
                                   GNOME_SAVE_GLOBAL,
                                   TRUE, GNOME_INTERACT_ANY, FALSE,  TRUE);
}

static void
hal_lid_button_cb (GpmHalMonitor *monitor,
                   gboolean       state,
                   GpmManager    *manager)
{
        /*
         * We enable/disable DPMS because some laptops do
         * not turn off the LCD backlight when the lid
         * is closed. See
         * http://bugzilla.gnome.org/show_bug.cgi?id=321313
         */
        if (state) {
                /* we only do a policy event when the lid is CLOSED */
                manager_policy_do (manager, GPM_PREF_BUTTON_LID);
        }

        gpm_screensaver_enable_dpms (! state);

}

static void
hal_suspend_button_cb (GpmHalMonitor *monitor,
                       gboolean       state,
                       GpmManager    *manager)
{
        manager_policy_do (manager, GPM_PREF_BUTTON_SUSPEND);
}

static void
hal_suspend_cb (GpmHalMonitor *monitor,
                GpmManager    *manager)
{
        gpm_manager_suspend (manager);
}

static void
hal_hibernate_cb (GpmHalMonitor *monitor,
                  GpmManager    *manager)
{
        gpm_manager_hibernate (manager);
}

static void
hal_lock_cb (GpmHalMonitor *monitor,
             GpmManager    *manager)
{
        gpm_screensaver_lock ();
}

static void
hal_on_ac_changed_cb (GpmHalMonitor *monitor,
                      gboolean       on_ac,
                      GpmManager    *manager)
{
        gpm_manager_set_on_ac (manager, on_ac);
}

static void
hal_battery_power_changed_cb (GpmHalMonitor *monitor,
                              int            percentage,
                              glong          minutes,
                              gboolean       discharging,
                              gboolean       primary,
                              GpmManager    *manager)
{
        maybe_notify_battery_power_changed (manager,
                                            percentage,
                                            minutes,
                                            discharging,
                                            primary);
}

static void
gpm_manager_set_property (GObject            *object,
                          guint               prop_id,
                          const GValue       *value,
                          GParamSpec         *pspec)
{
	GpmManager *manager = GPM_MANAGER (object);

        switch (prop_id) {
        case PROP_ON_AC:
		gpm_manager_set_on_ac (manager, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gpm_manager_get_property (GObject            *object,
                          guint               prop_id,
                          GValue             *value,
                          GParamSpec         *pspec)
{
	GpmManager *manager = GPM_MANAGER (object);

        switch (prop_id) {
        case PROP_ON_AC:
		g_value_set_boolean (value, manager->priv->on_ac);
                break;
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

        object_class->finalize     = gpm_manager_finalize;
        object_class->get_property = gpm_manager_get_property;
        object_class->set_property = gpm_manager_set_property;

        g_object_class_install_property (object_class,
                                         PROP_ON_AC,
                                         g_param_spec_boolean ("on_ac",
                                                               NULL,
                                                               NULL,
                                                               TRUE,
                                                               G_PARAM_READWRITE));

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
                            guint        cnxn_id,
                            GConfEntry  *entry,
                            gpointer     user_data)
{
	gint        value = 0;
        GpmManager *manager = GPM_MANAGER (user_data);
	gboolean    on_ac;

        on_ac = manager->priv->on_ac;

	g_debug ("callback_gconf_key_changed (%s)", entry->key);

	if (gconf_entry_get_value (entry) == NULL) {
		return;
        }

	if (strcmp (entry->key, GPM_PREF_ICON_POLICY) == 0) {
		gpm_notification_icon_update ();
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
		/* set new suspend timeouts */
                value = gconf_client_get_int (client, entry->key, NULL);

		if (! on_ac) {
			/*gpm_dpms_set_timeout (manager->priv->dpms, value); */
			gpm_screensaver_set_dpms_timeout (value);
		}
	} else if (strcmp (entry->key, GPM_PREF_AC_SLEEP_DISPLAY) == 0) {
		/* set new suspend timeouts */
                value = gconf_client_get_int (client, entry->key, NULL);

		if (on_ac) {
			/*gpm_dpms_set_timeout (manager->priv->idle, value); */
			gpm_screensaver_set_dpms_timeout (value);
		}
	}
}

static void
gpm_manager_init (GpmManager *manager)
{
        manager->priv = GPM_MANAGER_GET_PRIVATE (manager);

	manager->priv->gconf_client = gconf_client_get_default ();

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

	if (! gpm_libnotify_init (NICENAME))
		g_error ("Cannot initialise libnotify!");

        manager->priv->idle = gpm_idle_new ();
        g_signal_connect (manager->priv->idle, "changed",
                          G_CALLBACK (idle_changed_cb), manager);

	manager->priv->hal_monitor = gpm_hal_monitor_new ();
        g_signal_connect (manager->priv->hal_monitor, "ac-power-changed",
                          G_CALLBACK (hal_on_ac_changed_cb), manager);
        g_signal_connect (manager->priv->hal_monitor, "battery-power-changed",
                          G_CALLBACK (hal_battery_power_changed_cb), manager);
        g_signal_connect (manager->priv->hal_monitor, "power-button",
                          G_CALLBACK (hal_power_button_cb), manager);
        g_signal_connect (manager->priv->hal_monitor, "suspend-button",
                          G_CALLBACK (hal_suspend_button_cb), manager);
        g_signal_connect (manager->priv->hal_monitor, "lid-button",
                          G_CALLBACK (hal_lid_button_cb), manager);
        g_signal_connect (manager->priv->hal_monitor, "suspend",
                          G_CALLBACK (hal_suspend_cb), manager);
        g_signal_connect (manager->priv->hal_monitor, "hibernate",
                          G_CALLBACK (hal_hibernate_cb), manager);
        g_signal_connect (manager->priv->hal_monitor, "lock",
                          G_CALLBACK (hal_lock_cb), manager);

	/* do all the actions as we have to set initial state */
	gpm_manager_set_on_ac (manager,
                               gpm_hal_monitor_get_on_ac (manager->priv->hal_monitor));

	gpm_notification_icon_update ();

}

static void
gpm_manager_finalize (GObject *object)
{
        GpmManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GS_IS_MANAGER (object));

        manager = GPM_MANAGER (object);

        g_return_if_fail (manager->priv != NULL);

        if (manager->priv->gconf_client != NULL) {
                g_object_unref (manager->priv->gconf_client);
        }

        if (manager->priv->idle != NULL) {
                g_object_unref (manager->priv->idle);
        }

        if (manager->priv->hal_monitor != NULL) {
                g_object_unref (manager->priv->hal_monitor);
        }

	gpm_notification_icon_destroy ();

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmManager *
gpm_manager_new (void)
{
        GpmManager *manager;

        manager = g_object_new (GPM_TYPE_MANAGER, NULL);

        return GPM_MANAGER (manager);
}
