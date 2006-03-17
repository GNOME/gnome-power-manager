/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#include <glade/glade.h>
#include <libgnomeui/gnome-help.h>
#include <gtk/gtk.h>
#include <string.h>
#include "gpm-inhibit.h"
#include "gpm-debug.h"

static void     gpm_inhibit_class_init (GpmInhibitClass *klass);
static void     gpm_inhibit_init       (GpmInhibit      *inhibit);
static void     gpm_inhibit_finalize   (GObject      *object);

#define GPM_INHIBIT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INHIBIT, GpmInhibitPrivate))

typedef struct
{
	char		*application;
	char		*reason;
	char		*connection;
	int		 cookie;
} GpmInhibitData;

struct GpmInhibitPrivate
{
	GSList		*list;
};

static GObjectClass *parent_class = NULL;
G_DEFINE_TYPE (GpmInhibit, gpm_inhibit, G_TYPE_OBJECT)

/**
 * gpm_inhibit_add:
 * @connection:		Connection name, e.g. ":0.13"
 * @application:	Application name, e.g. "Nautilus"
 * @reason:		Reason for inhibiting, e.g. "Copying files"
 * 
 * Allocates a random cookie used to identify the connection, as multiple
 * inhibit requests can come from one caller sharing a dbus connection.
 * We need to refcount internally, and data is saved in the GpmInhibitData
 * struct.
 * 
 * Return value: a new random cookie.
 **/
int
gpm_inhibit_add (GpmInhibit *inhibit,
		 const char *connection,
		 const char *application,
		 const char *reason)
{
	/* handle where the application does not add required data */
	if (connection == NULL ||
	    application == NULL ||
	    reason == NULL) {
		gpm_warning ("Recieved InhibitInactiveSleep, but application "
			     "did not set the parameters correctly");
		return -1;
	}

	/* seems okay, add to list */
	GpmInhibitData *data = g_new (GpmInhibitData, 1);
	data->cookie = g_random_int_range (0, 10240);
	data->application = g_strdup (application);
	data->connection = g_strdup (connection);
	data->reason = g_strdup (reason);

	inhibit->priv->list = g_slist_append (inhibit->priv->list, 
                                              (gpointer) data);

	gpm_debug ("Recieved InhibitInactiveSleep from '%s' (%s) because '%s' saving as #%i",
		   data->application, data->connection, data->reason, data->cookie);
	return data->cookie;
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
 * gpm_inhibit_remove:
 * @connection:		Connection name
 * @application:	Application name
 * @cookie:		The cookie that we used to register
 * @forced:		If we fell off the dbus by crashing...
 * 
 * Removes a cookie and asscosiated data from the GpmInhibitData struct.
 **/
void
gpm_inhibit_remove (GpmInhibit *inhibit,
		    const char *connection,
		    int		cookie,
		    gboolean	forced)
{
	int a;
	GpmInhibitData *data;
	gboolean found = FALSE;

	if (forced) {
		/* Remove *any* connections that match the connection */
		for (a=0; a<g_slist_length (inhibit->priv->list); a++) {
			data = (GpmInhibitData *) g_slist_nth_data (inhibit->priv->list, a);
			if (strcmp (data->connection, connection) == 0) {
				gpm_debug ("Auto-revoked idle inhibit on '%s'.",
					   data->application);
				gpm_inhibit_free_data_object (data);
				inhibit->priv->list = g_slist_remove (inhibit->priv->list,
								      (gconstpointer) data);
			}
		}
		return;
	}

	/* Only remove the correct cookie */
	for (a=0; a<g_slist_length (inhibit->priv->list); a++) {
		data = (GpmInhibitData *) g_slist_nth_data (inhibit->priv->list, a);
		if (data->cookie == cookie) {
			found = TRUE;
			gpm_debug ("AllowInactiveSleep okay on '%s' as #%i",
				   connection, cookie);
			gpm_inhibit_free_data_object (data);
			inhibit->priv->list = g_slist_remove (inhibit->priv->list,
							      (gconstpointer) data);
		}
	}

	/* If not found, then the developer never used InhibitInactiveSleep, and
	   so put a warning out on the console */
	if (!found) {
		gpm_warning ("Cannot find registered program for #%i, so "
			     "cannot do AllowInactiveSleep", cookie);
	}
}

/** intialise the class */
static void
gpm_inhibit_class_init (GpmInhibitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gpm_inhibit_finalize;
	g_type_class_add_private (klass, sizeof (GpmInhibitPrivate));
}

/** intialise the object */
static void
gpm_inhibit_init (GpmInhibit *inhibit)
{
	inhibit->priv = GPM_INHIBIT_GET_PRIVATE (inhibit);
	inhibit->priv->list = NULL;
}

/** finalise the object */
static void
gpm_inhibit_finalize (GObject *object)
{
	GpmInhibit *inhibit;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_INHIBIT (object));

	inhibit = GPM_INHIBIT (object);
	inhibit->priv = GPM_INHIBIT_GET_PRIVATE (inhibit);

	/* remove items in list and free */
	int a;
	GpmInhibitData *data;
	for (a=0; a<g_slist_length (inhibit->priv->list); a++) {
		data = (GpmInhibitData *) g_slist_nth_data (inhibit->priv->list, a);
		gpm_inhibit_free_data_object (data);
	}
	g_slist_free (inhibit->priv->list);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/** create the object */
GpmInhibit *
gpm_inhibit_new (void)
{
	GpmInhibit *inhibit;
	inhibit = g_object_new (GPM_TYPE_INHIBIT, NULL);
	return GPM_INHIBIT (inhibit);
}
