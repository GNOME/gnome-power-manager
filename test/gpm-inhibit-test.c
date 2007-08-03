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
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#define	GPM_DBUS_SERVICE		"org.freedesktop.PowerManagement"
#define	GPM_DBUS_INHIBIT_PATH		"/org/freedesktop/PowerManagement/Inhibit"
#define	GPM_DBUS_INHIBIT_INTERFACE	"org.freedesktop.PowerManagement.Inhibit"

/* imagine this in a GObject private struct... */
guint appcookie = -1;

/** cookie is returned as an unsigned integer */
static guint
dbus_inhibit_gpm (const gchar *appname,
		  const gchar *reason)
{
	gboolean         res;
	guint	         cookie;
	GError          *error = NULL;
	DBusGProxy      *proxy = NULL;
	DBusGConnection *session_connection = NULL;

	/* get the DBUS session connection */
	session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error != NULL) {
		g_warning ("DBUS cannot connect : %s", error->message);
		g_error_free (error);
		return -1;
	}

	/* get the proxy with g-p-m */
	proxy = dbus_g_proxy_new_for_name (session_connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_INHIBIT_PATH,
					   GPM_DBUS_INHIBIT_INTERFACE);
	if (proxy == NULL) {
		g_warning ("Could not get DBUS proxy: %s", GPM_DBUS_SERVICE);
		return -1;
	}

	res = dbus_g_proxy_call (proxy,
				 "Inhibit", &error,
				 G_TYPE_STRING, appname,
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &cookie,
				 G_TYPE_INVALID);

	/* check the return value */
	if (! res) {
		cookie = -1;
		g_warning ("Inhibit method failed");
	}

	/* check the error value */
	if (error != NULL) {
		g_warning ("Inhibit problem : %s", error->message);
		g_error_free (error);
		cookie = -1;
	}

	g_debug ("cookie = %u", cookie);
	g_object_unref (G_OBJECT (proxy));
	return cookie;
}

static void
dbus_uninhibit_gpm (guint cookie)
{
	gboolean         res;
	GError          *error = NULL;
	DBusGProxy      *proxy = NULL;
	DBusGConnection *session_connection = NULL;

	/* cookies have to be positive as unsigned */
	if (cookie < 0) {
		g_warning ("Invalid cookie");
		return;
	}

	/* get the DBUS session connection */
	session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		g_warning ("DBUS cannot connect : %s", error->message);
		g_error_free (error);
		return;
	}

	/* get the proxy with g-p-m */
	proxy = dbus_g_proxy_new_for_name (session_connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_INHIBIT_PATH,
					   GPM_DBUS_INHIBIT_INTERFACE);
	if (proxy == NULL) {
		g_warning ("Could not get DBUS proxy: %s", GPM_DBUS_SERVICE);
		return;
	}

	res = dbus_g_proxy_call (proxy,
				 "UnInhibit",
				 &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);

	/* check the return value */
	if (! res) {
		g_warning ("UnInhibit method failed");
	}

	/* check the error value */
	if (error != NULL) {
		g_warning ("Inhibit problem : %s", error->message);
		g_error_free (error);
		cookie = -1;
	}
	g_object_unref (G_OBJECT (proxy));
}

static void
widget_inhibit_cb (GtkWidget *iwidget, GladeXML *glade_xml)
{
	GtkWidget *widget;
	const gchar *appname;
	const gchar *reason;

	/* get the application name and the reason from the entry boxes */
	widget = glade_xml_get_widget (glade_xml, "entry_app");
	appname = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = glade_xml_get_widget (glade_xml, "entry_reason");
	reason = gtk_entry_get_text (GTK_ENTRY (widget));
	
	/* try to add the inhibit */
	appcookie = dbus_inhibit_gpm (appname, reason);
	g_debug ("adding inhibit: %u", appcookie);
}

static void
widget_uninhibit_cb (GtkWidget *widget, GladeXML *glade_xml)
{
	/* try to remove the inhibit */
	g_debug ("removing inhibit: %u", appcookie);
	dbus_uninhibit_gpm (appcookie);
}

static void
window_close_cb (GtkWidget *widget)
{
	/* we don't have to UnInhibit as g-p-m notices we exit... */
	exit (0);
}

int
main (int argc, char **argv)
{
	GOptionContext  *context;
	GMainLoop       *loop = NULL;
	GtkWidget	*widget = NULL;
	GladeXML	*glade_xml = NULL;

	context = g_option_context_new (_("GNOME Power Preferences"));
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_parse (context, &argc, &argv, NULL);
	gtk_init (&argc, &argv);

	/* load the glade file, and setup the callbacks */
	glade_xml = glade_xml_new (GPM_DATA "/gpm-inhibit-test.glade", NULL, NULL);
	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (window_close_cb), glade_xml);
	widget = glade_xml_get_widget (glade_xml, "button_inhibit");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (widget_inhibit_cb), glade_xml);
	widget = glade_xml_get_widget (glade_xml, "button_uninhibit");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (widget_uninhibit_cb), glade_xml);
	widget = glade_xml_get_widget (glade_xml, "window_inhibit");
	g_signal_connect (widget, "destroy",
			  G_CALLBACK (window_close_cb), glade_xml);
	gtk_widget_show (widget);

	/* main loop */
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_option_context_free (context);
	return 0;
}
