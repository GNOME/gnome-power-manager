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
#include <glib/gi18n.h>

#include <glade/glade.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>
#include <math.h>
#include <string.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-graph-core.h"
#include "gpm-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-graph-widget.h"
#include "gpm-proxy.h"

static void     gpm_graph_class_init (GpmGraphClass *klass);
static void     gpm_graph_init       (GpmGraph      *graph);
static void     gpm_graph_finalize   (GObject	    *object);

#define GPM_GRAPH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_GRAPH, GpmGraphPrivate))
#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/PowerManager"
#define	GPM_DBUS_PATH_STATS		"/org/gnome/PowerManager/Statistics"
#define	GPM_DBUS_INTERFACE		"org.gnome.PowerManager"
#define	GPM_DBUS_INTERFACE_STATS	"org.gnome.PowerManager.Statistics"

#define ACTION_CHARGE		"charge"
#define ACTION_POWER		"power"
#define ACTION_TIME		"time"
#define ACTION_CHARGE_TEXT	_("Charge History")
#define ACTION_POWER_TEXT	_("Power History")
#define ACTION_TIME_TEXT	_("Estimated Time History")


struct GpmGraphPrivate
{
	GladeXML		*glade_xml;
	GConfClient		*gconf_client;
	GtkWidget		*graph_widget;
	GpmProxy		*gproxy;
};

enum {
	ACTION_HELP,
	ACTION_CLOSE,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmGraph, gpm_graph, G_TYPE_OBJECT)

/* The text that should appear in the action combo boxes */
#define ACTION_INTERACTIVE_TEXT		_("Ask me")
#define ACTION_SUSPEND_TEXT		_("Suspend")
#define ACTION_SHUTDOWN_TEXT		_("Shutdown")
#define ACTION_HIBERNATE_TEXT		_("Hibernate")
#define ACTION_BLANK_TEXT		_("Blank screen")
#define ACTION_NOTHING_TEXT		_("Do nothing")

#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/PowerManager"
#define	GPM_DBUS_INTERFACE		"org.gnome.PowerManager"

/**
 * gpm_graph_class_init:
 * @klass: This graph class instance
 **/
static void
gpm_graph_class_init (GpmGraphClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_graph_finalize;
	g_type_class_add_private (klass, sizeof (GpmGraphPrivate));

	signals [ACTION_HELP] =
		g_signal_new ("action-help",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmGraphClass, action_help),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [ACTION_CLOSE] =
		g_signal_new ("action-close",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmGraphClass, action_close),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * gpm_graph_help_cb:
 * @widget: The GtkWidget object
 * @graph: This graph class instance
 **/
static void
gpm_graph_help_cb (GtkWidget *widget,
		   GpmGraph  *graph)
{
	gpm_debug ("emitting action-help");
	g_signal_emit (graph, signals [ACTION_HELP], 0);
}

/**
 * gpm_graph_close_cb:
 * @widget: The GtkWidget object
 * @graph: This graph class instance
 **/
static void
gpm_graph_close_cb (GtkWidget	*widget,
		    GpmGraph	*graph)
{
	gpm_debug ("emitting action-close");
	g_signal_emit (graph, signals [ACTION_CLOSE], 0);
}

/**
 * gpm_graph_delete_event_cb:
 * @widget: The GtkWidget object
 * @event: The event type, unused.
 * @graph: This graph class instance
 **/
static gboolean
gpm_graph_delete_event_cb (GtkWidget	*widget,
			  GdkEvent	*event,
			  GpmGraph	*graph)
{
	gpm_graph_close_cb (widget, graph);
	return FALSE;
}

/**
 * gconf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
gconf_key_changed_cb (GConfClient *client,
		      guint	   cnxn_id,
		      GConfEntry  *entry,
		      gpointer	   user_data)
{
	GpmGraph *graph = GPM_GRAPH (user_data);
	gboolean  enabled;

	gpm_debug ("Key changed %s", entry->key);

	if (gconf_entry_get_value (entry) == NULL) {
		return;
	}

	if (strcmp (entry->key, GPM_PREF_AC_LOWPOWER) == 0) {
		enabled = gconf_client_get_bool (graph->priv->gconf_client,
				  		 GPM_PREF_AC_LOWPOWER, NULL);
		gpm_debug ("need to enable checkbox");
	}
}

/**
 * gpm_graph_widget_custom_handler:
 * @xml: The glade file we are reading.
 * @func_name: The function name to create the object
 *
 * Handler for libglade to provide interface with a pointer
 *
 * Return value: The custom widget.
 **/
static GtkWidget *
gpm_graph_widget_custom_handler (GladeXML *xml,
			  gchar *func_name, gchar *name,
			  gchar *string1, gchar *string2,
			  gint int1, gint int2,
			  gpointer user_data)
{
	GtkWidget *widget = NULL;
	if (strcmp ("gpm_graph_new", func_name) == 0) {
		widget = gpm_graph_widget_new ();
		return widget;
	}
	return NULL;
}

/**
 * gpm_graph_checkbox_events_cb:
 * @widget: The GtkWidget object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_graph_checkbox_events_cb (GtkWidget *widget,
			    GpmGraph  *graph)
{
	gboolean checked;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gpm_debug ("Events enable %i", checked);

	gpm_graph_widget_set_events (GPM_GRAPH_WIDGET (graph->priv->graph_widget), NULL);
}

/**
 * gpm_graph_checkbox_legend_cb:
 * @widget: The GtkWidget object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_graph_checkbox_legend_cb (GtkWidget *widget,
			    GpmGraph  *graph)
{
	gboolean checked;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gpm_debug ("Legend enable %i", checked);

	gpm_graph_widget_enable_legend (GPM_GRAPH_WIDGET (graph->priv->graph_widget), checked);
}

/**
 * gpm_graph_convert_strv_to_glist:
 *
 * @devices: The returned devices in strv format
 * Return value: A GList populated with the UDI's
 **/
static GList *
gpm_graph_convert_strv_to_glist (gchar **array)
{
	GList *list = NULL;
	guint i = 0;
	while (array && array[i]) {
		list = g_list_append (list, array[i]);
		++i;
	}
	return list;
}

static gboolean
gpm_graph_find_types (GpmGraph *graph,
		      GList   **list)
{
	GError *error = NULL;
	gboolean retval;
	gchar **strlist;
	DBusGProxy *proxy;

	proxy = gpm_proxy_get_proxy (graph->priv->gproxy);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}	

	retval = TRUE;
	if (dbus_g_proxy_call (proxy, "GetTypes", &error,
				G_TYPE_INVALID,
				G_TYPE_STRV, &strlist,
				G_TYPE_INVALID) == FALSE) {
		if (error) {
			g_debug ("%s", error->message);
			g_error_free (error);
		}
		retval = FALSE;
	}

	*list = gpm_graph_convert_strv_to_glist (strlist);

	return retval;
}

/**
 * gpm_graph_free_list_strings:
 *
 * Frees a GList of strings
 **/
static void
gpm_graph_free_list_strings (GList *list)
{
	GList *l;
	gchar *str;

	for (l=list; l != NULL; l=l->next) {
		str = l->data;
		g_free (str);
	}

	g_list_free (list);
}

static void
gpm_graph_type_combo_changed_cb (GtkWidget *widget,
				 GpmGraph  *graph)
{
	gchar *value;
	const gchar *type;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));

	if (strcmp (value, ACTION_CHARGE_TEXT) == 0) {
		type = ACTION_CHARGE;
	} else if (strcmp (value, ACTION_POWER_TEXT) == 0) {
		type = ACTION_POWER;
	} else if (strcmp (value, ACTION_TIME_TEXT) == 0) {
		type = ACTION_TIME;
	} else {
		g_assert (FALSE);
	}
	g_free (value);

	gpm_debug ("Changing graph type to %s", type);
}

static void
populate_graph_types (GpmGraph *graph, GtkWidget *widget)
{
	GList *list = NULL;
	GList *l;
	gchar *type;
	gchar *type_localized;
	gboolean ret;
	
	ret = gpm_graph_find_types (graph, &list);
	if (ret == FALSE) {
		return;
	}

	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_graph_type_combo_changed_cb),
			  graph);

	for (l=list; l != NULL; l=l->next) {
		type = l->data;
		if (strcmp (type, ACTION_CHARGE) == 0) {
			type_localized = ACTION_CHARGE_TEXT;
		} else if (strcmp (type, ACTION_POWER) == 0) {
			type_localized = ACTION_POWER_TEXT;
		} else if (strcmp (type, ACTION_TIME) == 0) {
			type_localized = ACTION_TIME_TEXT;
		} else {
			type_localized = _("Unknown");
		}
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), type_localized);
	}
	gpm_graph_free_list_strings (list);

	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
}

/**
 * gpm_graph_init:
 * @graph: This graph class instance
 **/
static void
gpm_graph_init (GpmGraph *graph)
{
	GtkWidget    *main_window;
	GtkWidget    *widget;

	graph->priv = GPM_GRAPH_GET_PRIVATE (graph);

	graph->priv->gconf_client = gconf_client_get_default ();

	glade_set_custom_handler (gpm_graph_widget_custom_handler, graph);

	graph->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (graph->priv->gproxy,
			  GPM_PROXY_SESSION,
			  GPM_DBUS_SERVICE,
			  GPM_DBUS_PATH_STATS,
			  GPM_DBUS_INTERFACE_STATS);

	gconf_client_notify_add (graph->priv->gconf_client,
				 GPM_PREF_DIR,
				 gconf_key_changed_cb,
				 graph,
				 NULL,
				 NULL);

	graph->priv->glade_xml = glade_xml_new (GPM_DATA "/gpm-graph.glade", NULL, NULL);

	main_window = glade_xml_get_widget (graph->priv->glade_xml, "window_graph");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW(main_window), GPM_STOCK_APP_ICON);

	/* Get the main window quit */
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gpm_graph_delete_event_cb), graph);

	widget = glade_xml_get_widget (graph->priv->glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_graph_close_cb), graph);

	widget = glade_xml_get_widget (graph->priv->glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_graph_help_cb), graph);

	widget = glade_xml_get_widget (graph->priv->glade_xml, "custom_graph");
	gtk_widget_set_size_request (widget, 600, 300);
	graph->priv->graph_widget = widget;
	gpm_graph_widget_set_axis_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);

	/* FIXME: There's got to be a better way than this */
	gtk_widget_hide (widget);
	gtk_widget_show (GTK_WIDGET (widget));

	widget = glade_xml_get_widget (graph->priv->glade_xml, "combobox_type");
	populate_graph_types (graph, widget);

	widget = glade_xml_get_widget (graph->priv->glade_xml, "combobox_device");
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Default");
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

	widget = glade_xml_get_widget (graph->priv->glade_xml, "checkbutton_events");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_graph_checkbox_events_cb), graph);

	widget = glade_xml_get_widget (graph->priv->glade_xml, "checkbutton_legend");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_graph_checkbox_legend_cb), graph);

	gtk_widget_show (main_window);
}

/**
 * gpm_graph_finalize:
 * @object: This graph class instance
 **/
static void
gpm_graph_finalize (GObject *object)
{
	GpmGraph *graph;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_GRAPH (object));

	graph = GPM_GRAPH (object);
	graph->priv = GPM_GRAPH_GET_PRIVATE (graph);

	g_object_unref (graph->priv->gconf_client);
	g_object_unref (graph->priv->gproxy);

	G_OBJECT_CLASS (gpm_graph_parent_class)->finalize (object);
}

/**
 * gpm_graph_new:
 * Return value: new GpmGraph instance.
 **/
GpmGraph *
gpm_graph_new (void)
{
	GpmGraph *graph;
	graph = g_object_new (GPM_TYPE_GRAPH, NULL);
	return GPM_GRAPH (graph);
}
