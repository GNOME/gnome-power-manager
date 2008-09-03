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
#include "egg-debug.h"
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

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmInhibit, gpm_inhibit, G_TYPE_OBJECT)

/**
 * gpm_inhibit_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_inhibit_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("gpm_inhibit_error");
	}
	return quark;
}

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
		egg_warning ("Recieved Inhibit, but application "
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

	egg_debug ("Recieved Inhibit from '%s' (%s) because '%s' saving as #%i",
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
		*error = g_error_new (gpm_inhibit_error_quark (),
				      GPM_INHIBIT_ERROR_GENERAL,
				      "Cannot find registered program for #%i, so "
				      "cannot do UnInhibit!", cookie);
		return FALSE;
	}
	egg_debug ("UnInhibit okay #%i", cookie);
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
	egg_warning ("Not implimented");
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
			egg_debug ("Auto-revoked idle inhibit on '%s'.",
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

	if (inhibit->priv->ignore_inhibits) {
		egg_debug ("Inhibit ignored through gconf policy!");
		*has_inihibit = FALSE;
	}

	/* An inhibit can stop the action */
	if (length == 0) {
		egg_debug ("Valid as no inhibitors");
		*has_inihibit = FALSE;
	} else {
		/* we have at least one application blocking the action */
		egg_debug ("We have %i valid inhibitors", length);
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
		gchar *boldstr = g_strdup_printf ("<b>%s</b>", data->application);
		gchar *italicstr = g_strdup_printf ("<b>%s</b>", data->reason);
		
		if (strcmp (action, "suspend") == 0) {
			/*Translators: first %s is an application name, second %s is the reason*/
			g_string_append_printf (message, _("%s has stopped the suspend from taking place: %s."),
				boldstr, italicstr);

		} else if (strcmp (action, "hibernate") == 0) {
			/*Translators: first %s is an application name, second %s is the reason*/
			g_string_append_printf (message, _("%s has stopped the hibernate from taking place: %s."),
					boldstr, italicstr); 

		} else if (strcmp (action, "policy action") == 0) {
			/*Translators: first %s is an application name, second %s is the reason*/
			g_string_append_printf (message, _("%s has stopped the policy action from taking place: %s."),
					boldstr, italicstr); 

		} else if (strcmp (action, "reboot") == 0) {
			/*Translators: first %s is an application name, second %s is the reason*/
			g_string_append_printf (message, _("%s has stopped the reboot from taking place: %s."),
					boldstr, italicstr); 

		} else if (strcmp (action, "shutdown") == 0) {
			/*Translators: first %s is an application name, second %s is the reason*/
			g_string_append_printf (message, _("%s has stopped the shutdown from taking place: %s."),
				boldstr, italicstr); 

		} else if (strcmp (action, "timeout action") == 0) {
			/*Translators: first %s is an application name, second %s is the reason*/
			g_string_append_printf (message, _("%s has stopped the timeout action from taking place: %s."),
				boldstr, italicstr); 
		}

		g_free (boldstr);
		g_free (italicstr);

	} else {
		if (strcmp (action, "suspend") == 0) {
			g_string_append (message, _("Multiple applications have stopped the suspend from taking place."));

		} else if (strcmp (action, "hibernate") == 0) {
			g_string_append (message, _("Multiple applications have stopped the hibernate from taking place."));

		} else if (strcmp (action, "policy action") == 0) {
			g_string_append (message, _("Multiple applications have stopped the policy action from taking place."));

		} else if (strcmp (action, "reboot") == 0) {
			g_string_append (message, _("Multiple applications have stopped the reboot from taking place.")); 

		} else if (strcmp (action, "shutdown") == 0) {
			g_string_append (message, _("Multiple applications have stopped the shutdown from taking place."));

		} else if (strcmp (action, "timeout action") == 0) {
			g_string_append (message, _("Multiple applications have stopped the suspend from taking place."));
		}
		
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

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef GPM_BUILD_TESTS
#include "gpm-self-test.h"
#include <libdbus-proxy.h>
#include "gpm-common.h"

/** cookie is returned as an unsigned integer */
static gboolean
inhibit (DbusProxy       *gproxy,
	 const gchar     *appname,
	 const gchar     *reason,
	 guint           *cookie)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (cookie != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "Inhibit", &error,
				 G_TYPE_STRING, appname,
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
		*cookie = 0;
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		g_warning ("Inhibit failed!");
	}

	return ret;
}

static gboolean
uninhibit (DbusProxy *gproxy,
	   guint      cookie)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	proxy = dbus_proxy_get_proxy (gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "UnInhibit", &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

static gboolean
has_inhibit (DbusProxy *gproxy,
			      gboolean        *has_inhibit)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	proxy = dbus_proxy_get_proxy (gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "HasInhibit", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, has_inhibit,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		g_warning ("HasInhibit failed!");
	}

	return ret;
}

void
gpm_st_inhibit (GpmSelfTest *test)
{
	gboolean ret;
	gboolean valid;
	guint cookie1 = 0;
	guint cookie2 = 0;
	DbusProxy *gproxy;

	if (gpm_st_start (test, "GpmInhibit") == FALSE) {
		return;
	}

	gproxy = dbus_proxy_new ();
	dbus_proxy_assign (gproxy, DBUS_PROXY_SESSION,
				   GPM_DBUS_SERVICE,
				   GPM_DBUS_PATH_INHIBIT,
				   GPM_DBUS_INTERFACE_INHIBIT);

	if (gproxy == NULL) {
		g_warning ("Unable to get connection to power manager");
		return;
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are not inhibited");
	ret = has_inhibit (gproxy, &valid);
	if (!ret) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid) {
		gpm_st_failed (test, "Already inhibited");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "clear an invalid cookie");
	ret = uninhibit (gproxy, 123456);
	if (!ret) {
		gpm_st_success (test, "invalid cookie failed as expected");
	} else {
		gpm_st_failed (test, "should have rejected invalid cookie");
	}

	/************************************************************/
	gpm_st_title (test, "get auto cookie 1");
	ret = inhibit (gproxy,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie1);
	if (!ret) {
		gpm_st_failed (test, "Unable to inhibit");
	} else if (cookie1 == 0) {
		gpm_st_failed (test, "Cookie invalid (cookie: %u)", cookie1);
	} else {
		gpm_st_success (test, "cookie: %u", cookie1);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are auto inhibited");
	ret = has_inhibit (gproxy, &valid);
	if (!ret) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "get cookie 2");
	ret = inhibit (gproxy,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie2);
	if (!ret) {
		gpm_st_failed (test, "Unable to inhibit");
	} else if (cookie2 == 0) {
		gpm_st_failed (test, "Cookie invalid (cookie: %u)", cookie2);
	} else {
		gpm_st_success (test, "cookie: %u", cookie2);
	}

	/************************************************************/
	gpm_st_title (test, "clear cookie 1");
	ret = uninhibit (gproxy, cookie1);
	if (!ret) {
		gpm_st_failed (test, "cookie failed to clear");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are still inhibited");
	ret = has_inhibit (gproxy, &valid);
	if (!ret) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "clear cookie 2");
	ret = uninhibit (gproxy, cookie2);
	if (!ret) {
		gpm_st_failed (test, "cookie failed to clear");
	} else {
		gpm_st_success (test, NULL);
	}

	g_object_unref (gproxy);

	gpm_st_end (test);
}

#endif

