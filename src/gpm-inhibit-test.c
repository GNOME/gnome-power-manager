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
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-ui-init.h>
#include <glade/glade.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/PowerManager"
#define	GPM_DBUS_INTERFACE		"org.gnome.PowerManager"

/* I know global stuff is bad, just imagine this is in a GObject... */
GladeXML	*glade_xml = NULL;
guint 		 appcookie = -1;
DBusGConnection *session_connection = NULL;

static guint
dbus_inhibit (DBusGConnection *session_connection,
	      const char      *appname,
	      const char      *reason)
{
	DBusGProxy *proxy;
	gboolean    res;
	guint	    cookie;
	GError     *error = NULL;

	proxy = dbus_g_proxy_new_for_name (session_connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_PATH,
					   GPM_DBUS_INTERFACE);
	res = dbus_g_proxy_call (proxy,
				 "Inhibit", &error,
				 G_TYPE_STRING, appname,
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &cookie,
				 G_TYPE_INVALID);
	if (! res) {
		cookie = -1;
		g_warning ("Inhibit method failed");
	}
	g_debug ("cookie = %u", cookie);
	g_object_unref (G_OBJECT (proxy));
	return cookie;
}

static void
dbus_uninhibit (DBusGConnection *session_connection,
		guint cookie)
{
	DBusGProxy *proxy;
	gboolean    res;
	GError     *error = NULL;

	proxy = dbus_g_proxy_new_for_name (session_connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_PATH,
					   GPM_DBUS_INTERFACE);
	res = dbus_g_proxy_call (proxy,
				 "UnInhibit",
				 &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (! res) {
		g_warning ("UnInhibit method failed");
	}
	g_object_unref (G_OBJECT (proxy));
}

static void
inhibit_inhibit_cb (GtkWidget *iwidget)
{
	GtkWidget *widget;
	const char *appname;
	const char *reason;

	/* get the application name and the reason from the entry boxes */
	widget = glade_xml_get_widget (glade_xml, "entry_app");
	appname = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = glade_xml_get_widget (glade_xml, "entry_reason");
	reason = gtk_entry_get_text (GTK_ENTRY (widget));
	
	/* try to add the inhibit */
	appcookie = dbus_inhibit (session_connection, appname, reason);
	g_debug ("adding inhibit: %u", appcookie);
}

static void
inhibit_uninhibit_cb (GtkWidget *widget)
{
	/* try to remove the inhibit */
	g_debug ("removing inhibit: %u", appcookie);
	dbus_uninhibit (session_connection, appcookie);
}

static void
inhibit_close_cb (GtkWidget *widget)
{
	/* we don't have to UnInhibit as g-p-m notices we drop off the bus... */
	exit (0);
}

int
main (int argc, char **argv)
{
	GOptionContext  *context;
 	GnomeProgram    *program;
	GMainLoop       *loop;
	GtkWidget	*widget;
	GError		*error = NULL;

	context = g_option_context_new (_("GNOME Power Preferences"));
	program = gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_GOPTION_CONTEXT, context,
			    GNOME_PARAM_HUMAN_READABLE_NAME,
			    _("Power Inhibit Test"),
			    NULL);

	/* get the DBUS session connection */
	session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	/* load the glade file, and setup the callbacks */
	glade_xml = glade_xml_new (GPM_DATA "/gpm-inhibit-test.glade", NULL, NULL);
	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (inhibit_close_cb), NULL);
	widget = glade_xml_get_widget (glade_xml, "button_inhibit");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (inhibit_inhibit_cb), NULL);
	widget = glade_xml_get_widget (glade_xml, "button_uninhibit");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (inhibit_uninhibit_cb), NULL);
	widget = glade_xml_get_widget (glade_xml, "window_inhibit");
	gtk_widget_show (widget);

	/* main loop */
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_object_unref (program);
	g_option_context_free (context);
	return 0;
}
