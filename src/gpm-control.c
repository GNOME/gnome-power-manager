/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gnome-keyring.h>
#include <gconf/gconf-client.h>
#include <devkit-power-gobject/devicekit-power.h>

#include "egg-debug.h"
#include "egg-console-kit.h"

#include "gpm-screensaver.h"
#include "gpm-common.h"
#include "gpm-control.h"
#include "gpm-networkmanager.h"

#define GPM_CONTROL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CONTROL, GpmControlPrivate))

struct GpmControlPrivate
{
	GConfClient		*conf;
	DkpClient		*client;
};

enum {
	REQUEST,
	RESUME,
	SLEEP,
	SLEEP_FAILURE,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_control_object = NULL;

G_DEFINE_TYPE (GpmControl, gpm_control, G_TYPE_OBJECT)

/**
 * gpm_control_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_control_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gpm_control_error");
	return quark;
}

/**
 * gpm_control_check_foreground_console:
 * @manager: This class instance
 * @action: The action we want to do, e.g. "suspend"
 *
 * Checks if the difference in time between this request for an action, and
 * the last action completing is larger than the timeout set in gconf.
 *
 * Also check for the foreground console if we specified
 * --enable-checkfg on the command line. This is only needed on Debian.
 *
 * Return value: TRUE if we can perform the action.
 **/
static gboolean
gpm_control_check_foreground_console (GpmControl *control)
{
#ifdef HAVE_CHECK_FG
	gchar *argv[] = { "check-foreground-console", NULL };
	int retcode;

	if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
				 NULL, NULL, &retcode, NULL)  || ! WIFEXITED (retcode)) {
		/* if check-foreground-console could not be executed,
		 * assume active console */
		egg_debug ("could not execute check-foreground-console");
		return TRUE;
	}
	egg_debug ("check-foreground-console returned with %i", WEXITSTATUS (retcode));
	return WEXITSTATUS (retcode) == 0;
#endif
	/* no other checks failed, so return okay */
	return TRUE;
}

/**
 * gpm_control_allowed_suspend:
 * @control: This class instance
 * @can: If we can suspend
 *
 * checks gconf to see if we are allowed to suspend this computer.
 **/
gboolean
gpm_control_allowed_suspend (GpmControl *control, gboolean *can, GError **error)
{
	gboolean conf_ok;
	gboolean hardware_ok;
	gboolean fg;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;

	/* get values from DkpClient */
	g_object_get (control->priv->client,
		      "can-suspend", &hardware_ok,
		      NULL);

	conf_ok = gconf_client_get_bool (control->priv->conf, GPM_CONF_CAN_SUSPEND, NULL);
	g_object_get (control->priv->client,
		      "can-suspend", &hardware_ok,
		      NULL);
	fg = gpm_control_check_foreground_console (control);
	if (conf_ok && hardware_ok && fg)
		*can = TRUE;
	egg_debug ("conf=%i, fg=%i, can=%i", conf_ok, fg, *can);
	return TRUE;
}

/**
 * gpm_control_allowed_hibernate:
 * @control: This class instance
 * @can: If we can hibernate
 *
 * Checks the HAL key power_management.can_suspend_to_disk and also
 * checks gconf to see if we are allowed to hibernate this computer.
 **/
gboolean
gpm_control_allowed_hibernate (GpmControl *control, gboolean *can, GError **error)
{
	gboolean conf_ok;
	gboolean hardware_ok;
	gboolean fg;
	g_return_val_if_fail (can, FALSE);

	/* get values from DkpClient */
	g_object_get (control->priv->client,
		      "can-hibernate", &hardware_ok,
		      NULL);

	*can = FALSE;
	conf_ok = gconf_client_get_bool (control->priv->conf, GPM_CONF_CAN_HIBERNATE, NULL);
	fg = gpm_control_check_foreground_console (control);
	g_object_get (control->priv->client,
		      "can-hibernate", &hardware_ok,
		      NULL);
	if (conf_ok && hardware_ok && fg)
		*can = TRUE;
	egg_debug ("conf=%i, fg=%i, can=%i", conf_ok, fg, *can);
	return TRUE;
}

/**
 * gpm_control_shutdown:
 * @control: This class instance
 *
 * Shuts down the computer
 **/
gboolean
gpm_control_shutdown (GpmControl *control, GError **error)
{
	gboolean ret;
	EggConsoleKit *console;
	console = egg_console_kit_new ();
	ret = egg_console_kit_stop (console, error);
	g_object_unref (console);
	return ret;
}

/**
 * gpm_control_get_lock_policy:
 * @control: This class instance
 * @policy: The policy gconf string.
 *
 * This function finds out if we should lock the screen when we do an
 * action. It is required as we can either use the gnome-screensaver policy
 * or the custom policy. See the yelp file for more information.
 *
 * Return value: TRUE if we should lock.
 **/
gboolean
gpm_control_get_lock_policy (GpmControl *control, const gchar *policy)
{
	gboolean do_lock;
	gboolean use_ss_setting;
	/* This allows us to over-ride the custom lock settings set in gconf
	   with a system default set in gnome-screensaver.
	   See bug #331164 for all the juicy details. :-) */
	use_ss_setting = gconf_client_get_bool (control->priv->conf, GPM_CONF_LOCK_USE_SCREENSAVER, NULL);
	if (use_ss_setting) {
		do_lock = gconf_client_get_bool (control->priv->conf, GS_PREF_LOCK_ENABLED, NULL);
		egg_debug ("Using ScreenSaver settings (%i)", do_lock);
	} else {
		do_lock = gconf_client_get_bool (control->priv->conf, policy, NULL);
		egg_debug ("Using custom locking settings (%i)", do_lock);
	}
	return do_lock;
}

/**
 * gpm_control_suspend:
 **/
gboolean
gpm_control_suspend (GpmControl *control, GError **error)
{
	gboolean allowed;
	gboolean ret = FALSE;
	gboolean do_lock;
	gboolean nm_sleep;
	gboolean lock_gnome_keyring;
	GnomeKeyringResult keyres;
	GpmScreensaver *screensaver;
	guint32 throttle_cookie = 0;

	screensaver = gpm_screensaver_new ();

	gpm_control_allowed_suspend (control, &allowed, error);
	if (allowed == FALSE) {
		egg_debug ("cannot suspend as not allowed from policy");
		g_set_error (error,
			     GPM_CONTROL_ERROR,
			     GPM_CONTROL_ERROR_GENERAL,
			     "Cannot suspend as not allowed from policy");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_SUSPEND);
		return FALSE;
	}

	/* we should perhaps lock keyrings when sleeping #375681 */
	lock_gnome_keyring = gconf_client_get_bool (control->priv->conf, GPM_CONF_LOCK_GNOME_KEYRING_SUSPEND, NULL);
	if (lock_gnome_keyring) {
		keyres = gnome_keyring_lock_all_sync ();
		if (keyres != GNOME_KEYRING_RESULT_OK)
			egg_warning ("could not lock keyring");
	}

	do_lock = gpm_control_get_lock_policy (control, GPM_CONF_LOCK_ON_SUSPEND);
	if (do_lock) {
		throttle_cookie = gpm_screensaver_add_throttle (screensaver, "suspend");
		gpm_screensaver_lock (screensaver);
	}

	nm_sleep = gconf_client_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, NULL);
	if (nm_sleep)
		gpm_networkmanager_sleep ();

	/* Do the suspend */
	egg_debug ("emitting sleep");
	g_signal_emit (control, signals [SLEEP], 0, GPM_CONTROL_ACTION_SUSPEND);

	ret = dkp_client_suspend (control->priv->client, error);

	egg_debug ("emitting resume");
	g_signal_emit (control, signals [RESUME], 0, GPM_CONTROL_ACTION_SUSPEND);

	if (!ret) {
		egg_debug ("emitting sleep-failure");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_SUSPEND);
	}

	if (do_lock) {
		gpm_screensaver_poke (screensaver);
		if (throttle_cookie)
			gpm_screensaver_remove_throttle (screensaver, throttle_cookie);
	}

	nm_sleep = gconf_client_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, NULL);
	if (nm_sleep)
		gpm_networkmanager_wake ();

	g_object_unref (screensaver);

	return ret;
}

/**
 * gpm_control_hibernate:
 **/
gboolean
gpm_control_hibernate (GpmControl *control, GError **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean do_lock;
	gboolean nm_sleep;
	gboolean lock_gnome_keyring;
	GnomeKeyringResult keyres;
	GpmScreensaver *screensaver;
	guint32 throttle_cookie = 0;

	screensaver = gpm_screensaver_new ();

	gpm_control_allowed_hibernate (control, &allowed, error);

	if (allowed == FALSE) {
		egg_debug ("cannot hibernate as not allowed from policy");
		g_set_error (error,
			     GPM_CONTROL_ERROR,
			     GPM_CONTROL_ERROR_GENERAL,
			     "Cannot hibernate");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_HIBERNATE);
		return FALSE;
	}

	/* we should perhaps lock keyrings when sleeping #375681 */
	lock_gnome_keyring = gconf_client_get_bool (control->priv->conf, GPM_CONF_LOCK_GNOME_KEYRING_HIBERNATE, NULL);
	if (lock_gnome_keyring) {
		keyres = gnome_keyring_lock_all_sync ();
		if (keyres != GNOME_KEYRING_RESULT_OK) {
			egg_warning ("could not lock keyring");
		}
	}

	do_lock = gpm_control_get_lock_policy (control, GPM_CONF_LOCK_ON_HIBERNATE);
	if (do_lock) {
		throttle_cookie = gpm_screensaver_add_throttle (screensaver, "hibernate");
		gpm_screensaver_lock (screensaver);
	}

	nm_sleep = gconf_client_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, NULL);
	if (nm_sleep)
		gpm_networkmanager_sleep ();

	egg_debug ("emitting sleep");
	g_signal_emit (control, signals [SLEEP], 0, GPM_CONTROL_ACTION_HIBERNATE);

	ret = dkp_client_hibernate (control->priv->client, error);

	egg_debug ("emitting resume");
	g_signal_emit (control, signals [RESUME], 0, GPM_CONTROL_ACTION_HIBERNATE);

	if (!ret) {
		egg_debug ("emitting sleep-failure");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_HIBERNATE);
	}

	if (do_lock) {
		gpm_screensaver_poke (screensaver);
		if (throttle_cookie)
			gpm_screensaver_remove_throttle (screensaver, throttle_cookie);
	}

	nm_sleep = gconf_client_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, NULL);
	if (nm_sleep)
		gpm_networkmanager_wake ();

	g_object_unref (screensaver);

	return ret;
}

/**
 * gpm_control_finalize:
 **/
static void
gpm_control_finalize (GObject *object)
{
	GpmControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CONTROL (object));
	control = GPM_CONTROL (object);

	g_object_unref (control->priv->conf);
	g_object_unref (control->priv->client);

	g_return_if_fail (control->priv != NULL);
	G_OBJECT_CLASS (gpm_control_parent_class)->finalize (object);
}

/**
 * gpm_control_class_init:
 **/
static void
gpm_control_class_init (GpmControlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_control_finalize;

	signals [RESUME] =
		g_signal_new ("resume",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmControlClass, resume),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals [SLEEP] =
		g_signal_new ("sleep",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmControlClass, sleep),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals [SLEEP_FAILURE] =
		g_signal_new ("sleep-failure",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmControlClass, sleep_failure),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals [REQUEST] =
		g_signal_new ("request",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmControlClass, request),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GpmControlPrivate));
}

/**
 * gpm_control_init:
 * @control: This control class instance
 **/
static void
gpm_control_init (GpmControl *control)
{
	control->priv = GPM_CONTROL_GET_PRIVATE (control);

	control->priv->client = dkp_client_new ();
	control->priv->conf = gconf_client_get_default ();
}

/**
 * gpm_control_new:
 * Return value: A new control class instance.
 **/
GpmControl *
gpm_control_new (void)
{
	if (gpm_control_object != NULL) {
		g_object_ref (gpm_control_object);
	} else {
		gpm_control_object = g_object_new (GPM_TYPE_CONTROL, NULL);
		g_object_add_weak_pointer (gpm_control_object, &gpm_control_object);
	}
	return GPM_CONTROL (gpm_control_object);
}

