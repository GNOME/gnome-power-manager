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
#include <libgnomeui/gnome-client.h>

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

#include <libhal-gpower.h>

#include "gpm-conf.h"
#include "gpm-screensaver.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-control.h"
#include "gpm-polkit.h"
#include "gpm-dbus-monitor.h"
#include "gpm-networkmanager.h"

#define GPM_CONTROL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CONTROL, GpmControlPrivate))

typedef struct
{
	gchar			*application;
	gchar			*connection;
	guint32			 cookie;
} GpmControlData;

struct GpmControlPrivate
{
	GpmConf			*conf;
	HalGPower		*hal_power;
	GpmPolkit		*polkit;
	GSList			*list;
	GpmDbusMonitor		*dbus_monitor;
	time_t			 last_resume_event;
	guint			 suppress_policy_timeout;
};

enum {
	REQUEST,
	RESUME,
	SLEEP,
	SLEEP_FAILURE,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };
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
	if (!quark) {
		quark = g_quark_from_static_string ("gpm_control_error");
	}
	return quark;
}

/**
 * gpm_control_is_policy_timout_valid:
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
gboolean
gpm_control_is_policy_timout_valid (GpmControl  *control,
				    const gchar *action)
{
	gchar *message;
#ifdef HAVE_CHECK_FG
	gchar *argv[] = { "check-foreground-console", NULL };
	int retcode;
#endif

	if ((time (NULL) - control->priv->last_resume_event) <=
	    control->priv->suppress_policy_timeout) {
		message = g_strdup_printf ("Skipping suppressed %s", action);
		gpm_debug (message);
		g_free (message);
		return FALSE;
	}

#ifdef HAVE_CHECK_FG
	if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
				 NULL, NULL, &retcode, NULL)  || ! WIFEXITED (retcode) ) {
		/* if check-foreground-console could not be executed,
		 * assume active console */
		gpm_debug ("could not execute check-foreground-console");
		return TRUE;
	}
	gpm_debug ("check-foreground-console returned with %i", WEXITSTATUS (retcode));
	return WEXITSTATUS (retcode) == 0;
#endif
	/* no other checks failed, so return okay */
	return TRUE;
}

/**
 * gpm_control_reset_event_time:
 * @manager: This class instance
 *
 * Resets the time so we do not do any more actions until the
 * timeout has passed.
 **/
void
gpm_control_reset_event_time (GpmControl *control)
{
	control->priv->last_resume_event = time (NULL);
}

/**
 * gpm_control_cookie_compare_func
 * @a: Pointer to the data to test
 * @b: Pointer to a cookie to compare
 *
 * A GCompareFunc for comparing a cookie to a list.
 *
 * Return value: 0 if cookie matches
 **/
static gint
gpm_control_cookie_compare_func (gconstpointer a, gconstpointer b)
{
	GpmControlData *data;
	guint32		cookie;
	data = (GpmControlData*) a;
	cookie = *((guint32*) b);
	if (cookie == data->cookie)
		return 0;
	return 1;
}

/**
 * gpm_control_find_cookie:
 * @control: This control instance
 * @cookie: The cookie we are looking for
 *
 * Finds the data in the cookie list.
 *
 * Return value: The cookie data, or NULL if not found
 **/
static GpmControlData *
gpm_control_find_cookie (GpmControl *control, guint32 cookie)
{
	GpmControlData *data;
	GSList	       *ret;
	ret = g_slist_find_custom (control->priv->list, &cookie,
				   gpm_control_cookie_compare_func);
	if (! ret) {
		return NULL;
	}
	data = (GpmControlData *) ret->data;
	return data;
}

/**
 * gpm_control_generate_cookie:
 * @control: This control instance
 *
 * Returns a random cookie not already allocated.
 *
 * Return value: a new random cookie.
 **/
static guint32
gpm_control_generate_cookie (GpmControl *control)
{
	guint32		cookie;

	/* Iterate until we have a unique cookie */
	do {
		cookie = (guint32) g_random_int_range (1, G_MAXINT32);
	} while (gpm_control_find_cookie (control, cookie));
	return cookie;
}

/**
 * gpm_control_request_cookie:
 * @connection: Connection name, e.g. ":0.13"
 * @application:	Application name, e.g. "Nautilus"
 * @reason: Reason for controling, e.g. "Copying files"
 *
 * Allocates a random cookie used to identify the connection, as multiple
 * control requests can come from one caller sharing a dbus connection.
 * We need to refcount internally, and data is saved in the GpmControlData
 * struct.
 *
 * Return value: a new random cookie.
 **/
void
gpm_control_register (GpmControl  *control,
		      const gchar *application,
		      DBusGMethodInvocation *context,
		      GError	**error)
{
	const gchar *connection;
	GpmControlData *data;

	/* as we are async, we can get the sender */
	connection = dbus_g_method_get_sender (context);

	/* handle where the application does not add required data */
	if (connection == NULL ||
	    application == NULL) {
		gpm_warning ("Recieved Actions, but application "
			     "did not set the parameters correctly");
		dbus_g_method_return (context, -1);
		return;
	}

	/* seems okay, add to list */
	data = g_new (GpmControlData, 1);
	data->cookie = gpm_control_generate_cookie (control);
	data->application = g_strdup (application);
	data->connection = g_strdup (connection);

	control->priv->list = g_slist_append (control->priv->list,
					      (gpointer) data);

	gpm_debug ("Recieved Actions from '%s' (%s) saving as #%i",
		   data->application, data->connection, data->cookie);

	dbus_g_method_return (context, data->cookie);
}

/* free one element in GpmControlData struct */
static void
gpm_control_free_data_object (GpmControlData *data)
{
	g_free (data->application);
	g_free (data->connection);
	g_free (data);
}

/**
 * gpm_control_clear_cookie:
 * @application:	Application name
 * @cookie: The cookie that we used to register
 *
 * Removes a cookie and associated data from the GpmControlData struct.
 **/
gboolean
gpm_control_un_register (GpmControl  *control,
		         guint32      cookie,
		         GError     **error)
{
	GpmControlData *data;

	/* Only remove the correct cookie */
	data = gpm_control_find_cookie (control, cookie);
	if (data == NULL) {
		gpm_warning ("Cannot find registered program for #%i, so "
			     "cannot do UnActions", cookie);
		return FALSE;
	}
	gpm_debug ("UnActions okay #%i", cookie);
	gpm_control_free_data_object (data);
	control->priv->list = g_slist_remove (control->priv->list,
					      (gconstpointer) data);
	return TRUE;
}

/**
 * gpm_control_get_requests:
 *
 * Gets a list of controls.
 **/
gboolean
gpm_control_policy (GpmControl *control,
		    guint32	cookie,
		    gboolean	allowed,
		    GError    **error)
{
	gpm_warning ("Not implimented");
	return FALSE;
}

/**
 * gpm_control_remove_dbus:
 * @connection: Connection name
 * @application:	Application name
 * @cookie: The cookie that we used to register
 *
 * Checks to see if the dbus closed session is registered, in which case
 * unregister it.
 **/
static void
gpm_control_remove_dbus (GpmControl  *control,
			 const gchar *connection)
{
	int a;
	GpmControlData *data;
	/* Remove *any* connections that match the connection */
	for (a=0; a<g_slist_length (control->priv->list); a++) {
		data = (GpmControlData *) g_slist_nth_data (control->priv->list, a);
		if (strcmp (data->connection, connection) == 0) {
			gpm_debug ("Auto-revoked idle control on '%s'.",
				   data->application);
			gpm_control_free_data_object (data);
			control->priv->list = g_slist_remove (control->priv->list,
							      (gconstpointer) data);
		}
	}
	return;
}

/**
 * dbus_noc_session_cb:
 * @power: The power class instance
 * @name: The DBUS name, e.g. hal.freedesktop.org
 * @prev: The previous name, e.g. :0.13
 * @new: The new name, e.g. :0.14
 * @control: This control class instance
 *
 * The noc session DBUS callback.
 **/
static void
dbus_noc_session_cb (GpmDbusMonitor *dbus_monitor,
		     const gchar    *name,
		     const gchar    *prev,
		     const gchar    *new,
		     GpmControl	    *control)
{
	if (strlen (new) == 0) {
		gpm_control_remove_dbus (control, name);
	}
}

/**
 * gpm_control_allowed_suspend:
 * @control: This class instance
 * @can: If we can suspend
 *
 * Checks the HAL key power_management.can_suspend_to_ram and also
 * checks gconf to see if we are allowed to suspend this computer.
 **/
gboolean
gpm_control_allowed_suspend (GpmControl *control,
			     gboolean   *can,
			     GError    **error)
{
	gboolean conf_ok;
	gboolean polkit_ok = TRUE;
	gboolean hal_ok = FALSE;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;
	gpm_conf_get_bool (control->priv->conf, GPM_CONF_CAN_SUSPEND, &conf_ok);
	hal_ok = hal_gpower_can_suspend (control->priv->hal_power);
	if (control->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (control->priv->polkit, "hal-power-suspend");
	}
	if ( conf_ok && hal_ok && polkit_ok ) {
		*can = TRUE;
	}

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
gpm_control_allowed_hibernate (GpmControl *control,
			       gboolean   *can,
			       GError    **error)
{
	gboolean conf_ok;
	gboolean polkit_ok = TRUE;
	gboolean hal_ok = FALSE;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;
	gpm_conf_get_bool (control->priv->conf, GPM_CONF_CAN_HIBERNATE, &conf_ok);
	hal_ok = hal_gpower_can_hibernate (control->priv->hal_power);
	if (control->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (control->priv->polkit, "hal-power-hibernate");
	}
	if ( conf_ok && hal_ok && polkit_ok ) {
		*can = TRUE;
	}
	return TRUE;
}

/**
 * gpm_control_allowed_shutdown:
 * @control: This class instance
 * @can: If we can shutdown
 *
 **/
gboolean
gpm_control_allowed_shutdown (GpmControl *control,
			      gboolean   *can,
			      GError    **error)
{
	gboolean polkit_ok = TRUE;
	g_return_val_if_fail (can, FALSE);
	*can = FALSE;
	if (control->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (control->priv->polkit, "hal-power-shutdown");
	}
	if (polkit_ok == TRUE) {
		*can = TRUE;
	}
	return TRUE;
}

/**
 * gpm_control_allowed_reboot:
 * @control: This class instance
 * @can: If we can reboot
 *
 * Stub function -- TODO.
 **/
gboolean
gpm_control_allowed_reboot (GpmControl *control,
			    gboolean   *can,
			    GError    **error)
{
	gboolean polkit_ok = TRUE;
	g_return_val_if_fail (can, FALSE);
	*can = FALSE;
	if (control->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (control->priv->polkit, "hal-power-reboot");
	}
	if (polkit_ok == TRUE) {
		*can = TRUE;
	}
	return TRUE;
}

/**
 * gpm_control_shutdown:
 * @control: This class instance
 *
 * Shuts down the computer, saving the session if possible.
 **/
gboolean
gpm_control_shutdown (GpmControl *control,
		      GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean save_session;

	gpm_control_allowed_shutdown (control, &allowed, error);
	if (allowed == FALSE) {
		gpm_warning ("Cannot shutdown");
		g_set_error (error,
			     GPM_CONTROL_ERROR,
			     GPM_CONTROL_ERROR_GENERAL,
			     "Cannot shutdown");
		return FALSE;
	}

	gpm_conf_get_bool (control->priv->conf, GPM_CONF_SESSION_REQUEST_SAVE, &save_session);
	/* We can set g-p-m to not save the session to avoid confusing new
	   users. By default we save the session to preserve data. */
	if (save_session == TRUE) {
		gnome_client_request_save (gnome_master_client (),
					   GNOME_SAVE_GLOBAL,
					   FALSE, GNOME_INTERACT_NONE, FALSE,  TRUE);
	}
	hal_gpower_shutdown (control->priv->hal_power);
	ret = TRUE;

	return ret;
}

/**
 * gpm_control_reboot:
 * @control: This class instance
 *
 * Reboots the computer, saving the session if possible.
 **/
gboolean
gpm_control_reboot (GpmControl *control,
		    GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean save_session;

	gpm_control_allowed_reboot (control, &allowed, error);
	if (allowed == FALSE) {
		gpm_warning ("Cannot reboot");
		g_set_error (error,
			     GPM_CONTROL_ERROR,
			     GPM_CONTROL_ERROR_GENERAL,
			     "Cannot reboot");
		return FALSE;
	}

	gpm_conf_get_bool (control->priv->conf, GPM_CONF_SESSION_REQUEST_SAVE, &save_session);
	/* We can set g-p-m to not save the session to avoid confusing new
	   users. By default we save the session to preserve data. */
	if (save_session == TRUE) {
		gnome_client_request_save (gnome_master_client (),
					   GNOME_SAVE_GLOBAL,
					   FALSE, GNOME_INTERACT_NONE, FALSE,  TRUE);
	}

	hal_gpower_reboot (control->priv->hal_power);
	ret = TRUE;

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
gpm_control_get_lock_policy (GpmControl  *control,
			     const gchar *policy)
{
	gboolean do_lock;
	gboolean use_ss_setting;
	/* This allows us to over-ride the custom lock settings set in gconf
	   with a system default set in gnome-screensaver.
	   See bug #331164 for all the juicy details. :-) */
	gpm_conf_get_bool (control->priv->conf, GPM_CONF_LOCK_USE_SCREENSAVER, &use_ss_setting);
	if (use_ss_setting) {
		gpm_conf_get_bool (control->priv->conf, GS_PREF_LOCK_ENABLED, &do_lock);
		gpm_debug ("Using ScreenSaver settings (%i)", do_lock);
	} else {
		gpm_conf_get_bool (control->priv->conf, policy, &do_lock);
		gpm_debug ("Using custom locking settings (%i)", do_lock);
	}
	return do_lock;
}

gboolean
gpm_control_suspend (GpmControl *control,
		     GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean do_lock;
	gboolean nm_sleep;
	GnomeKeyringResult keyres;
	GpmScreensaver *screensaver;

	screensaver = gpm_screensaver_new ();

	gpm_control_allowed_suspend (control, &allowed, error);
	if (allowed == FALSE) {
		gpm_syslog ("cannot suspend as not allowed from policy");
		g_set_error (error,
			     GPM_CONTROL_ERROR,
			     GPM_CONTROL_ERROR_GENERAL,
			     "Cannot suspend");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_SUSPEND);
		return FALSE;
	}

	/* we should lock keyrings when sleeping #375681 */
	keyres = gnome_keyring_lock_all_sync ();
	if (keyres != GNOME_KEYRING_RESULT_OK) {
		gpm_debug ("could not lock keyring");
	}

	do_lock = gpm_control_get_lock_policy (control, GPM_CONF_LOCK_ON_SUSPEND);
	if (do_lock == TRUE) {
		gpm_screensaver_lock (screensaver);
	}

	gpm_conf_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep == TRUE) {
		gpm_networkmanager_sleep ();
	}

	/* Do the suspend */
	gpm_debug ("emitting sleep");
	g_signal_emit (control, signals [SLEEP], 0, GPM_CONTROL_ACTION_SUSPEND);
	ret = hal_gpower_suspend (control->priv->hal_power, 0);
	gpm_debug ("emitting resume");
	g_signal_emit (control, signals [RESUME], 0, GPM_CONTROL_ACTION_SUSPEND);

	if (ret == FALSE) {
		gpm_debug ("emitting sleep-failure");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_SUSPEND);
	}

	if (do_lock == TRUE) {
		gpm_screensaver_poke (screensaver);
	}

	gpm_conf_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep == TRUE) {
		gpm_networkmanager_wake ();
	}

	/* save the time that we resumed */
	gpm_control_reset_event_time (control);
	g_object_unref (screensaver);

	return ret;
}

gboolean
gpm_control_hibernate (GpmControl *control,
		       GError    **error)
{
	gboolean allowed;
	gboolean ret;
	gboolean do_lock;
        gboolean nm_sleep;
	GnomeKeyringResult keyres;
	GpmScreensaver *screensaver;

	screensaver = gpm_screensaver_new ();

	gpm_control_allowed_hibernate (control, &allowed, error);

	if (allowed == FALSE) {
		gpm_syslog ("cannot hibernate as not allowed from policy");
		g_set_error (error,
			     GPM_CONTROL_ERROR,
			     GPM_CONTROL_ERROR_GENERAL,
			     "Cannot hibernate");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_HIBERNATE);
		return FALSE;
	}

	/* we should lock keyrings when sleeping #375681 */
	keyres = gnome_keyring_lock_all_sync ();
	if (keyres != GNOME_KEYRING_RESULT_OK) {
		gpm_debug ("could not lock keyring");
	}

	do_lock = gpm_control_get_lock_policy (control, GPM_CONF_LOCK_ON_HIBERNATE);
	if (do_lock == TRUE) {
		gpm_screensaver_lock (screensaver);
	}

	gpm_conf_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep == TRUE) {
		gpm_networkmanager_sleep ();
	}

	gpm_debug ("emitting sleep");
	g_signal_emit (control, signals [SLEEP], 0, GPM_CONTROL_ACTION_HIBERNATE);
	ret = hal_gpower_hibernate (control->priv->hal_power);
	gpm_debug ("emitting resume");
	g_signal_emit (control, signals [RESUME], 0, GPM_CONTROL_ACTION_HIBERNATE);

	if (ret == FALSE) {
		gpm_debug ("emitting sleep-failure");
		g_signal_emit (control, signals [SLEEP_FAILURE], 0, GPM_CONTROL_ACTION_HIBERNATE);
	}

	if (do_lock == TRUE) {
		gpm_screensaver_poke (screensaver);
	}

	gpm_conf_get_bool (control->priv->conf, GPM_CONF_NETWORKMANAGER_SLEEP, &nm_sleep);
	if (nm_sleep == TRUE) {
		gpm_networkmanager_wake ();
	}

	/* save the time that we resumed */
	gpm_control_reset_event_time (control);
	g_object_unref (screensaver);

	return ret;
}

/**
 * gpm_control_constructor:
 **/
static GObject *
gpm_control_constructor (GType		  type,
			 guint		  n_construct_properties,
			 GObjectConstructParam *construct_properties)
{
	GpmControl      *control;
	GpmControlClass *klass;
	klass = GPM_CONTROL_CLASS (g_type_class_peek (GPM_TYPE_CONTROL));
	control = GPM_CONTROL (G_OBJECT_CLASS (gpm_control_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (control);
}

/**
 * gpm_control_finalize:
 **/
static void
gpm_control_finalize (GObject *object)
{
	GpmControl *control;
	guint a;
	GpmControlData *data;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CONTROL (object));
	control = GPM_CONTROL (object);

	g_object_unref (control->priv->conf);
	g_object_unref (control->priv->hal_power);
	if (control->priv->polkit) {
		g_object_unref (control->priv->polkit);
	}

	/* remove items in list and free */
	for (a=0; a<g_slist_length (control->priv->list); a++) {
		data = (GpmControlData *) g_slist_nth_data (control->priv->list, a);
		gpm_control_free_data_object (data);
	}
	g_slist_free (control->priv->list);

	g_object_unref (control->priv->dbus_monitor);

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
	object_class->finalize	   = gpm_control_finalize;
	object_class->constructor  = gpm_control_constructor;

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
 *
 * initialises the control class. NOTE: We expect control objects
 * to *NOT* be removed or added during the session.
 * We only control the first control object if there are more than one.
 **/
static void
gpm_control_init (GpmControl *control)
{
	control->priv = GPM_CONTROL_GET_PRIVATE (control);

	/* this will be NULL if we don't compile in support */
	control->priv->polkit = gpm_polkit_new ();
	control->priv->hal_power = hal_gpower_new ();
	control->priv->dbus_monitor = gpm_dbus_monitor_new ();
	g_signal_connect (control->priv->dbus_monitor, "noc-session",
			  G_CALLBACK (dbus_noc_session_cb), control);

	control->priv->conf = gpm_conf_new ();
	gpm_conf_get_uint (control->priv->conf, GPM_CONF_POLICY_TIMEOUT,
			   &control->priv->suppress_policy_timeout);
	gpm_debug ("Using a supressed policy timeout of %i seconds",
		   control->priv->suppress_policy_timeout);

	/* Pretend we just resumed when we start to let actions settle */
	gpm_control_reset_event_time (control);
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

