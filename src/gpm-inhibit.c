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

#include "config.h"

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib/gi18n.h>
#include <string.h>
#include <libdbus-monitor-session.h>

#include "gpm-inhibit.h"
#include "gpm-debug.h"
#include "gpm-conf.h"

static void     gpm_inhibit_class_init (GpmInhibitClass *klass);
static void     gpm_inhibit_init       (GpmInhibit      *inhibit);
static void     gpm_inhibit_finalize   (GObject		*object);

#define GPM_INHIBIT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INHIBIT, GpmInhibitPrivate))

typedef struct
{
	gchar			*application;
	gchar			*reason;
	gchar			*connection;
	guint32			 cookie;
} GpmInhibitData;

struct GpmInhibitPrivate
{
	GSList			*list;
	DbusMonitorSession	*dbus_monitor;
	GpmConf			*conf;
	gboolean		 ignore_inhibits;
};

enum {
	HAS_INHIBIT_CHANGED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmInhibit, gpm_inhibit, G_TYPE_OBJECT)

/**
 * gpm_inhibit_cookie_compare_func
 * @a: Pointer to the data to test
 * @b: Pointer to a cookie to compare
 *
 * A GCompareFunc for comparing a cookie to a list.
 *
 * Return value: 0 if cookie matches
 **/
static gint
gpm_inhibit_cookie_compare_func (gconstpointer a, gconstpointer b)
{
	GpmInhibitData *data;
	guint32		cookie;
	data = (GpmInhibitData*) a;
	cookie = *((guint32*) b);
	if (cookie == data->cookie)
		return 0;
	return 1;
}

/**
 * gpm_inhibit_find_cookie:
 * @inhibit: This inhibit instance
 * @cookie: The cookie we are looking for
 *
 * Finds the data in the cookie list.
 *
 * Return value: The cookie data, or NULL if not found
 **/
static GpmInhibitData *
gpm_inhibit_find_cookie (GpmInhibit *inhibit, guint32 cookie)
{
	GpmInhibitData *data;
	GSList	       *ret;
	/* search list */
	ret = g_slist_find_custom (inhibit->priv->list, &cookie,
				   gpm_inhibit_cookie_compare_func);
	if (ret == NULL) {
		return NULL;
	}
	data = (GpmInhibitData *) ret->data;
	return data;
}

/**
 * gpm_inhibit_generate_cookie:
 * @inhibit: This inhibit instance
 *
 * Returns a random cookie not already allocated.
 *
 * Return value: a new random cookie.
 **/
static guint32
gpm_inhibit_generate_cookie (GpmInhibit *inhibit)
{
	guint32		cookie;

	/* Iterate until we have a unique cookie */
	do {
		cookie = (guint32) g_random_int_range (1, G_MAXINT32);
	} while (gpm_inhibit_find_cookie (inhibit, cookie));
	return cookie;
}

/**
 * gpm_inhibit_inhibit:
 * @connection: Connection name, e.g. ":0.13"
 * @application:	Application name, e.g. "Nautilus"
 * @reason: Reason for inhibiting, e.g. "Copying files"
 *
 * Allocates a random cookie used to identify the connection, as multiple
 * inhibit requests can come from one caller sharing a dbus connection.
 * We need to refcount internally, and data is saved in the GpmInhibitData
 * struct.
 *
 * Return value: a new random cookie.
 **/
void
gpm_inhibit_inhibit (GpmInhibit  *inhibit,
		          const gchar *application,
		          const gchar *reason,
		          DBusGMethodInvocation *context,
		          GError     **error)
{
	const gchar *connection;
	GpmInhibitData *data;

	/* as we are async, we can get the sender */
	connection = dbus_g_method_get_sender (context);

	/* handle where the application does not add required data */
	if (connection == NULL ||
	    application == NULL ||
	    reason == NULL) {
		gpm_warning ("Recieved Inhibit, but application "
			     "did not set the parameters correctly");
		dbus_g_method_return (context, -1);
		return;
	}

	/* seems okay, add to list */
	data = g_new (GpmInhibitData, 1);
	data->cookie = gpm_inhibit_generate_cookie (inhibit);
	data->application = g_strdup (application);
	data->connection = g_strdup (connection);
	data->reason = g_strdup (reason);

	inhibit->priv->list = g_slist_append (inhibit->priv->list, (gpointer) data);

	gpm_debug ("Recieved Inhibit from '%s' (%s) because '%s' saving as #%i",
		   data->application, data->connection, data->reason, data->cookie);

	/* only emit event on the first one */
	if (g_slist_length (inhibit->priv->list) == 1) {
		g_signal_emit (inhibit, signals [HAS_INHIBIT_CHANGED], 0, TRUE);
	}

	dbus_g_method_return (context, data->cookie);
}

/* free one element in GpmInhibitData struct */
static void
gpm_inhibit_free_data_object (GpmInhibitData *data)
{
	g_free (data->application);
	g_free (data->reason);
	g_free (data->connection);
	g_free (data);
}

/**
 * gpm_inhibit_un_inhibit:
 * @application:	Application name
 * @cookie: The cookie that we used to register
 *
 * Removes a cookie and associated data from the GpmInhibitData struct.
 **/
gboolean
gpm_inhibit_un_inhibit (GpmInhibit  *inhibit,
		        guint32      cookie,
		        GError     **error)
{
	GpmInhibitData *data;

	/* Only remove the correct cookie */
	data = gpm_inhibit_find_cookie (inhibit, cookie);
	if (data == NULL) {
		gpm_warning ("Cannot find registered program for #%i, so "
			     "cannot do UnInhibit", cookie);
		return FALSE;
	}
	gpm_debug ("UnInhibit okay #%i", cookie);
	gpm_inhibit_free_data_object (data);

	/* remove it from the list */
	inhibit->priv->list = g_slist_remove (inhibit->priv->list, (gconstpointer) data);

	/* only emit event on the last one */
	if (g_slist_length (inhibit->priv->list) == 0) {
		g_signal_emit (inhibit, signals [HAS_INHIBIT_CHANGED], 0, FALSE);
	}

	return TRUE;
}

/**
 * gpm_inhibit_get_requests:
 *
 * Gets a list of inhibits.
 **/
gboolean
gpm_inhibit_get_requests (GpmInhibit *inhibit,
			  gchar	   ***requests,
			  GError    **error)
{
	gpm_warning ("Not implimented");
	return FALSE;
}

/**
 * gpm_inhibit_remove_dbus:
 * @connection: Connection name
 * @application:	Application name
 * @cookie: The cookie that we used to register
 *
 * Checks to see if the dbus closed session is registered, in which case
 * unregister it.
 **/
static void
gpm_inhibit_remove_dbus (GpmInhibit  *inhibit,
			 const gchar *connection)
{
	int a;
	GpmInhibitData *data;
	/* Remove *any* connections that match the connection */
	for (a=0; a<g_slist_length (inhibit->priv->list); a++) {
		data = (GpmInhibitData *) g_slist_nth_data (inhibit->priv->list, a);
		if (strcmp (data->connection, connection) == 0) {
			gpm_debug ("Auto-revoked idle inhibit on '%s'.",
				   data->application);
			gpm_inhibit_free_data_object (data);
			/* remove it from the list */
			inhibit->priv->list = g_slist_remove (inhibit->priv->list, (gconstpointer) data);
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
 * @inhibit: This inhibit class instance
 *
 * The noc session DBUS callback.
 **/
static void
dbus_noc_session_cb (DbusMonitorSession *dbus_monitor,
		     const gchar    *name,
		     const gchar    *prev,
		     const gchar    *new,
		     GpmInhibit	    *inhibit)
{
	if (strlen (new) == 0) {
		gpm_inhibit_remove_dbus (inhibit, name);
	}
}

/**
 * gpm_inhibit_has_inhibit
 *
 * Checks to see if we are being stopped from performing an action.
 *
 * TRUE if the action is OK, i.e. we have *not* been inhibited.
 **/
gboolean
gpm_inhibit_has_inhibit (GpmInhibit *inhibit,
		         gboolean   *has_inihibit,
		         GError    **error)
{
	guint length;

	length = g_slist_length (inhibit->priv->list);

	if (inhibit->priv->ignore_inhibits == TRUE) {
		gpm_debug ("Inhibit ignored through gconf policy!");
		*has_inihibit = FALSE;
	}

	/* An inhibit can stop the action */
	if (length == 0) {
		gpm_debug ("Valid as no inhibitors");
		*has_inihibit = FALSE;
	} else {
		/* we have at least one application blocking the action */
		gpm_debug ("We have %i valid inhibitors", length);
		*has_inihibit = TRUE;
	}

	/* we always return successful for DBUS */
	return TRUE;
}

/**
 * gpm_inhibit_get_message:
 *
 * @message:	Description string, e.g. "Nautilus because 'copying files'"
 * @action:	Action we wanted to do, e.g. "suspend"
 *
 * Returns a localised message text describing what application has inhibited
 * the action, and why.
 *
 **/
void
gpm_inhibit_get_message (GpmInhibit  *inhibit,
			 GString     *message,
			 const gchar *action)
{
	guint a;
	GpmInhibitData *data;

	if (g_slist_length (inhibit->priv->list) == 1) {
		data = (GpmInhibitData *) g_slist_nth_data (inhibit->priv->list, 0);
		g_string_append_printf (message, "<b>%s</b> ",
					data->application);
		g_string_append_printf (message, _("has stopped the %s from "
						   "taking place : "),
					action);
		g_string_append_printf (message, "<i>%s</i>.",
					data->reason);
	} else {
		g_string_append_printf (message, _("Multiple applications have stopped the "
					"%s from taking place."), action);
		for (a=0; a<g_slist_length (inhibit->priv->list); a++) {
			data = (GpmInhibitData *) g_slist_nth_data (inhibit->priv->list, a);
			g_string_append_printf (message,
						"\n<b>%s</b> : <i>%s</i>",
						data->application, data->reason);
		}
	}
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf       *conf,
		     const gchar   *key,
		     GpmInhibit    *inhibit)
{
	if (strcmp (key, GPM_CONF_IGNORE_INHIBITS) == 0) {
		gpm_conf_get_bool (inhibit->priv->conf, GPM_CONF_IGNORE_INHIBITS,
				   &inhibit->priv->ignore_inhibits);
	}
}

/** intialise the class */
static void
gpm_inhibit_class_init (GpmInhibitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_inhibit_finalize;

	signals [HAS_INHIBIT_CHANGED] =
		g_signal_new ("has-inhibit-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmInhibitClass, has_inhibit_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	g_type_class_add_private (klass, sizeof (GpmInhibitPrivate));
}

/** intialise the object */
static void
gpm_inhibit_init (GpmInhibit *inhibit)
{
	inhibit->priv = GPM_INHIBIT_GET_PRIVATE (inhibit);
	inhibit->priv->list = NULL;
	inhibit->priv->dbus_monitor = dbus_monitor_session_new ();
	g_signal_connect (inhibit->priv->dbus_monitor, "name-owner-changed",
			  G_CALLBACK (dbus_noc_session_cb), inhibit);

	inhibit->priv->conf = gpm_conf_new ();
	g_signal_connect (inhibit->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), inhibit);

	/* Do we ignore inhibit requests? */
	gpm_conf_get_bool (inhibit->priv->conf, GPM_CONF_IGNORE_INHIBITS, &inhibit->priv->ignore_inhibits);
}

/** finalise the object */
static void
gpm_inhibit_finalize (GObject *object)
{
	GpmInhibit *inhibit;
	guint a;
	GpmInhibitData *data;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_INHIBIT (object));

	inhibit = GPM_INHIBIT (object);
	inhibit->priv = GPM_INHIBIT_GET_PRIVATE (inhibit);

	for (a=0; a<g_slist_length (inhibit->priv->list); a++) {
		data = (GpmInhibitData *) g_slist_nth_data (inhibit->priv->list, a);
		gpm_inhibit_free_data_object (data);
	}
	g_slist_free (inhibit->priv->list);

	g_object_unref (inhibit->priv->conf);
	g_object_unref (inhibit->priv->dbus_monitor);
	G_OBJECT_CLASS (gpm_inhibit_parent_class)->finalize (object);
}

/** create the object */
GpmInhibit *
gpm_inhibit_new (void)
{
	GpmInhibit *inhibit;
	inhibit = g_object_new (GPM_TYPE_INHIBIT, NULL);
	return GPM_INHIBIT (inhibit);
}
