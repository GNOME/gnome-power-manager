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
#include <math.h>
#include <string.h>

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-conf.h"
#include "gpm-statistics-core.h"
#include "gpm-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-info.h"
#include "gpm-info-data.h"
#include "gpm-proxy.h"
#include "gpm-powermanager.h"

static void     gpm_statistics_class_init (GpmStatisticsClass *klass);
static void     gpm_statistics_init       (GpmStatistics      *statistics);
static void     gpm_statistics_finalize   (GObject	    *object);

#define GPM_STATISTICS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_STATISTICS, GpmStatisticsPrivate))

#define ACTION_VOLTAGE		"voltage"
#define ACTION_CHARGE		"charge"
#define ACTION_POWER		"power"
#define ACTION_TIME		"time"
#define ACTION_CHARGE_TEXT	_("Charge History")
#define ACTION_POWER_TEXT	_("Power History")
#define ACTION_VOLTAGE_TEXT	_("Voltage History")
#define ACTION_TIME_TEXT	_("Estimated Time History")

#define GPM_STATISTICS_POLL_INTERVAL	15000 /* ms */

struct GpmStatisticsPrivate
{
	GladeXML		*glade_xml;
	GtkWidget		*graph_widget;
	GpmConf			*conf;
	GpmProxy		*gproxy;
	GpmInfoData		*events;
	GpmInfoData		*data;
	const gchar		*graph_type;
};

enum {
	ACTION_HELP,
	ACTION_CLOSE,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmStatistics, gpm_statistics, G_TYPE_OBJECT)

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
 * gpm_statistics_class_init:
 * @klass: This graph class instance
 **/
static void
gpm_statistics_class_init (GpmStatisticsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_statistics_finalize;
	g_type_class_add_private (klass, sizeof (GpmStatisticsPrivate));

	signals [ACTION_HELP] =
		g_signal_new ("action-help",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmStatisticsClass, action_help),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [ACTION_CLOSE] =
		g_signal_new ("action-close",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmStatisticsClass, action_close),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * gpm_statistics_help_cb:
 * @widget: The GtkWidget object
 * @graph: This graph class instance
 **/
static void
gpm_statistics_help_cb (GtkWidget *widget,
		   GpmStatistics  *statistics)
{
	gpm_debug ("emitting action-help");
	g_signal_emit (statistics, signals [ACTION_HELP], 0);
}

/**
 * gpm_statistics_close_cb:
 * @widget: The GtkWidget object
 * @graph: This graph class instance
 **/
static void
gpm_statistics_close_cb (GtkWidget	*widget,
		    GpmStatistics	*statistics)
{
	gpm_debug ("emitting action-close");
	g_signal_emit (statistics, signals [ACTION_CLOSE], 0);
}

/**
 * gpm_statistics_delete_event_cb:
 * @widget: The GtkWidget object
 * @event: The event type, unused.
 * @graph: This graph class instance
 **/
static gboolean
gpm_statistics_delete_event_cb (GtkWidget	*widget,
			  GdkEvent	*event,
			  GpmStatistics	*statistics)
{
	gpm_statistics_close_cb (widget, statistics);
	return FALSE;
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf       *conf,
		     const gchar   *key,
		     GpmStatistics *statistics)
{
	gboolean  enabled;

	if (strcmp (key, GPM_CONF_AC_LOWPOWER) == 0) {
		gpm_conf_get_bool (statistics->priv->conf, GPM_CONF_AC_LOWPOWER, &enabled);
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

static gboolean
gpm_statistics_get_events (GpmStatistics *statistics)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;
	GValueArray *gva;
	GValue *gv;
	GPtrArray *ptrarray = NULL;
	GType g_type_ptrarray;
	gint i;
	gint x;
	gint event;
	gint time = 0;

	g_return_val_if_fail (statistics != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_STATISTICS (statistics), FALSE);

	proxy = gpm_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	g_type_ptrarray = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_INVALID));

	ret = dbus_g_proxy_call (proxy, "GetEventLog", &error,
				 G_TYPE_INT, time,
				 G_TYPE_INVALID,
				 g_type_ptrarray, &ptrarray,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetEventLog failed!");
		return FALSE;
	}

	gpm_debug ("events size=%i", ptrarray->len);

	/* clear current events */
	gpm_info_data_clear (statistics->priv->events);

	for (i=0; i< ptrarray->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (ptrarray, i);
		gv = g_value_array_get_nth (gva, 0);
		x = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 1);
		event = g_value_get_int (gv);
		g_value_unset (gv);
		gpm_info_data_add_always (statistics->priv->events, x, event, 0, NULL);
		g_value_array_free (gva);
	}
	g_ptr_array_free (ptrarray, TRUE);

	return TRUE;
}

static void
gpm_statistics_refresh_events (GpmStatistics *statistics)
{
	GList *list;

	gpm_statistics_get_events (statistics);
	list = gpm_info_data_get_list (statistics->priv->events);

	gpm_graph_widget_set_events (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), list);
}

/**
 * gpm_statistics_checkbox_events_cb:
 * @widget: The GtkWidget object
 **/
static void
gpm_statistics_checkbox_events_cb (GtkWidget     *widget,
			           GpmStatistics *statistics)
{
	gboolean checked;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gpm_debug ("Events enable %i", checked);

	/* save to gconf so we open next time with the correct setting */
	gpm_conf_set_bool (statistics->priv->conf, GPM_CONF_STAT_SHOW_EVENTS, checked);

	if (checked == FALSE) {
		/* remove the dots from the graph */
		gpm_graph_widget_enable_events (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), FALSE);
		return;
	}

	/* refresh data automatically */
	gpm_statistics_refresh_events (statistics);

	/* only enable the dots if the checkbox is checked */
	gpm_graph_widget_enable_events (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), TRUE);
}

/**
 * gpm_statistics_checkbox_legend_cb:
 * @widget: The GtkWidget object
 * @gpm_pref_key: The GConf key for this preference setting.
 **/
static void
gpm_statistics_checkbox_legend_cb (GtkWidget *widget,
			    GpmStatistics  *statistics)
{
	gboolean checked;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gpm_debug ("Legend enable %i", checked);

	/* save to gconf so we open next time with the correct setting */
	gpm_conf_set_bool (statistics->priv->conf, GPM_CONF_STAT_SHOW_LEGEND, checked);

	gpm_graph_widget_enable_legend (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), checked);
}

/**
 * gpm_statistics_convert_strv_to_glist:
 *
 * @devices: The returned devices in strv format
 * Return value: A GList populated with the UDI's
 **/
static GList *
gpm_statistics_convert_strv_to_glist (gchar **array)
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
gpm_statistics_find_types (GpmStatistics *statistics,
			   GList        **list)
{
	GError *error = NULL;
	gboolean ret;
	gchar **strlist;
	DBusGProxy *proxy;

	proxy = gpm_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "GetTypes", &error,
			         G_TYPE_INVALID,
			         G_TYPE_STRV, &strlist,
			         G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetTypes failed!");
		return FALSE;
	}

	*list = gpm_statistics_convert_strv_to_glist (strlist);

	return TRUE;
}

/**
 * gpm_statistics_free_list_strings:
 *
 * Frees a GList of strings
 **/
static void
gpm_statistics_free_list_strings (GList *list)
{
	GList *l;
	gchar *str;

	for (l=list; l != NULL; l=l->next) {
		str = l->data;
		g_free (str);
	}

	g_list_free (list);
}

static gboolean
gpm_statistics_get_data_dbus (GpmStatistics *statistics,
			 const gchar *type)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;
	GValueArray *gva;
	GValue *gv;
	GPtrArray *ptrarray = NULL;
	GType g_type_ptrarray;
	int i;
	int x, y, col;
	gint time = 0;

	g_return_val_if_fail (statistics != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_STATISTICS (statistics), FALSE);
	g_return_val_if_fail (type != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	g_type_ptrarray = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_INVALID));

	ret = dbus_g_proxy_call (proxy, "GetData", &error,
				 G_TYPE_INT, time,
				 G_TYPE_STRING, type,
				 G_TYPE_INVALID,
				 g_type_ptrarray, &ptrarray,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetData failed!");
		return FALSE;
	}

	gpm_debug ("size=%i", ptrarray->len);

	/* clear current events */
	gpm_info_data_clear (statistics->priv->data);

	for (i=0; i< ptrarray->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (ptrarray, i);
		gv = g_value_array_get_nth (gva, 0);
		x = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 1);
		y = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 2);
		col = g_value_get_int (gv);
		g_value_unset (gv);
		gpm_info_data_add_always (statistics->priv->data, x, y, col, NULL);
		g_value_array_free (gva);
	}
	g_ptr_array_free (ptrarray, TRUE);

	return TRUE;
}

static gboolean
gpm_statistics_get_axis_type_dbus (GpmStatistics          *statistics,
			      const gchar 	     *type,
			      GpmGraphWidgetAxisType *x,
			      GpmGraphWidgetAxisType *y)
{
	GError *error = NULL;
	gboolean ret;
	gchar *axis_type_x;
	gchar *axis_type_y;
	DBusGProxy *proxy;

	g_return_val_if_fail (statistics != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_STATISTICS (statistics), FALSE);
	g_return_val_if_fail (x != NULL, FALSE);
	g_return_val_if_fail (y != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}	

	ret = dbus_g_proxy_call (proxy, "GetAxisType", &error,
			         G_TYPE_STRING, type,
			         G_TYPE_INVALID,
			         G_TYPE_STRING, &axis_type_x,
			         G_TYPE_STRING, &axis_type_y,
			         G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("GetAxisType failed!");
		return FALSE;
	}

	gpm_debug ("graph type '%s' mapped to x-axis '%s'", type, axis_type_x);
	gpm_debug ("graph type '%s' mapped to y-axis '%s'", type, axis_type_y);

	/* convert the string types to enumerated values */
	*x = gpm_graph_widget_string_to_axis_type (axis_type_x);
	*y = gpm_graph_widget_string_to_axis_type (axis_type_y);

	return TRUE;
}

static void
gpm_statistics_refresh_data (GpmStatistics *statistics)
{
	GList *list = NULL;

	/* only get the data for a valid type */
	if (statistics->priv->graph_type != NULL) {
		gpm_statistics_get_data_dbus (statistics, statistics->priv->graph_type);
	}

	list = gpm_info_data_get_list (statistics->priv->data);

	gpm_graph_widget_set_data (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), list);

	gtk_widget_hide (GTK_WIDGET (statistics->priv->graph_widget));
	gtk_widget_show (GTK_WIDGET (statistics->priv->graph_widget));
}

static void
gpm_statistics_type_combo_changed_cb (GtkWidget      *widget,
				      GpmStatistics  *statistics)
{
	gchar *value;
	gchar *type;
	GpmGraphWidgetAxisType axis_x = GPM_GRAPH_WIDGET_TYPE_INVALID;
	GpmGraphWidgetAxisType axis_y = GPM_GRAPH_WIDGET_TYPE_INVALID;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (strcmp (value, ACTION_CHARGE_TEXT) == 0) {
		type = ACTION_CHARGE;
	} else if (strcmp (value, ACTION_POWER_TEXT) == 0) {
		type = ACTION_POWER;
	} else if (strcmp (value, ACTION_TIME_TEXT) == 0) {
		type = ACTION_TIME;
	} else if (strcmp (value, ACTION_VOLTAGE_TEXT) == 0) {
		type = ACTION_VOLTAGE;
	} else {
		g_assert (FALSE);
	}
	g_free (value);

	/* find out what sort of grid axis we need */
	gpm_statistics_get_axis_type_dbus (statistics, type, &axis_x, &axis_y);
	gpm_graph_widget_set_axis_x (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), axis_x);
	gpm_graph_widget_set_axis_y (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), axis_y);

	/* const, so no need to free */
	statistics->priv->graph_type = type;

	/* save in gconf so we choose the correct graph type on next startup */
	gpm_conf_set_string (statistics->priv->conf, GPM_CONF_STAT_GRAPH_TYPE, type);

	/* refresh data automatically */
	gpm_statistics_refresh_data (statistics);
}

static void
gpm_statistics_populate_graph_types (GpmStatistics *statistics,
				     GtkWidget     *widget)
{
	GList *list = NULL;
	GList *l;
	gchar *type;
	gchar *saved;
	gchar *type_localized;
	gboolean ret;
	guint count, pos;

	ret = gpm_statistics_find_types (statistics, &list);
	if (ret == FALSE) {
		return;
	}

	gpm_conf_get_string (statistics->priv->conf, GPM_CONF_STAT_GRAPH_TYPE, &saved);
	/* gconf error, bahh */
	if (saved == NULL) {
		saved = g_strdup ("power");
	}

	count = 0;
	pos = 0;
	for (l=list; l != NULL; l=l->next) {
		type = l->data;
		if (strcmp (type, ACTION_CHARGE) == 0) {
			type_localized = ACTION_CHARGE_TEXT;
		} else if (strcmp (type, ACTION_POWER) == 0) {
			type_localized = ACTION_POWER_TEXT;
		} else if (strcmp (type, ACTION_TIME) == 0) {
			type_localized = ACTION_TIME_TEXT;
		} else if (strcmp (type, ACTION_VOLTAGE) == 0) {
			type_localized = ACTION_VOLTAGE_TEXT;
		} else {
			type_localized = _("Unknown");
		}
		/* is this the same value as we have stored in gconf? */
		if (strcmp (type, saved) == 0) {
			pos = count;
		}
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), type_localized);
		count++;
	}
	gpm_statistics_free_list_strings (list);
	g_free (saved);

	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_statistics_type_combo_changed_cb),
			  statistics);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), pos);
}

static gboolean 
gpm_statistics_graph_refresh (gpointer data)
{
	GpmStatistics *statistics = GPM_STATISTICS (data);
	gpm_debug ("refreshing graph type '%s'", statistics->priv->graph_type);
	gpm_statistics_refresh_data (statistics);
	gpm_statistics_refresh_events (statistics);
	return TRUE;
}

/**
 * gpm_statistics_activate_window:
 * @statistics: This class instance
 *
 * Activates (shows) the window.
 **/
void
gpm_statistics_activate_window (GpmStatistics *statistics)
{
	GtkWidget *widget;
	widget = glade_xml_get_widget (statistics->priv->glade_xml, "window_graph");
	gtk_window_present (GTK_WINDOW (widget));
}

/**
 * gpm_statistics_init:
 * @graph: This graph class instance
 **/
static void
gpm_statistics_init (GpmStatistics *statistics)
{
	GtkWidget *main_window;
	GtkWidget *widget;
	gboolean   checked;

	statistics->priv = GPM_STATISTICS_GET_PRIVATE (statistics);

	statistics->priv->conf = gpm_conf_new ();
	g_signal_connect (statistics->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), statistics);

	glade_set_custom_handler (gpm_graph_widget_custom_handler, statistics);

	statistics->priv->gproxy = gpm_proxy_new ();
	gpm_proxy_assign (statistics->priv->gproxy,
			  GPM_PROXY_SESSION,
			  GPM_DBUS_SERVICE,
			  GPM_DBUS_PATH_STATS,
			  GPM_DBUS_INTERFACE);

	/* would happen if not using g-p-m or using an old version of g-p-m */
	if (gpm_proxy_is_connected (statistics->priv->gproxy) == FALSE) {
		gpm_critical_error ("%s\n%s", _("Could not connect to GNOME Power Manager."),
				    _("Perhaps the program is not running or you are using an old version?"));
	}

	statistics->priv->graph_type = NULL;
	statistics->priv->events = gpm_info_data_new ();
	statistics->priv->data = gpm_info_data_new ();

	statistics->priv->glade_xml = glade_xml_new (GPM_DATA "/gpm-graph.glade", NULL, NULL);

	main_window = glade_xml_get_widget (statistics->priv->glade_xml, "window_graph");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW (main_window), GPM_STOCK_APP_ICON);

	/* Get the main window quit */
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gpm_statistics_delete_event_cb), statistics);

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_statistics_close_cb), statistics);

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_statistics_help_cb), statistics);

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "custom_graph");
	gtk_widget_set_size_request (widget, 600, 300);
	statistics->priv->graph_widget = widget;
	gpm_graph_widget_set_axis_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);

	/* add the key items */
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("On AC"),
				  GPM_EVENT_ON_AC,
				  GPM_GRAPH_WIDGET_COLOUR_BLUE,
				  GPM_GRAPH_WIDGET_SHAPE_CIRCLE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("On battery"),
				  GPM_EVENT_ON_BATTERY,
				  GPM_GRAPH_WIDGET_COLOUR_DARK_BLUE,
				  GPM_GRAPH_WIDGET_SHAPE_CIRCLE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Session idle"),
				  GPM_EVENT_SESSION_IDLE,
				  GPM_GRAPH_WIDGET_COLOUR_YELLOW,
				  GPM_GRAPH_WIDGET_SHAPE_SQUARE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Session active"),
				  GPM_EVENT_SESSION_ACTIVE,
				  GPM_GRAPH_WIDGET_COLOUR_DARK_YELLOW,
				  GPM_GRAPH_WIDGET_SHAPE_SQUARE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Suspend"),
				  GPM_EVENT_SUSPEND,
				  GPM_GRAPH_WIDGET_COLOUR_RED,
				  GPM_GRAPH_WIDGET_SHAPE_DIAMOND);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Resume"),
				  GPM_EVENT_RESUME,
				  GPM_GRAPH_WIDGET_COLOUR_DARK_RED,
				  GPM_GRAPH_WIDGET_SHAPE_DIAMOND);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Hibernate"),
				  GPM_EVENT_HIBERNATE,
				  GPM_GRAPH_WIDGET_COLOUR_MAGENTA,
				  GPM_GRAPH_WIDGET_SHAPE_DIAMOND);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Lid closed"),
				  GPM_EVENT_LID_CLOSED,
				  GPM_GRAPH_WIDGET_COLOUR_GREEN,
				  GPM_GRAPH_WIDGET_SHAPE_TRIANGLE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Lid opened"),
				  GPM_EVENT_LID_OPENED,
				  GPM_GRAPH_WIDGET_COLOUR_DARK_GREEN,
				  GPM_GRAPH_WIDGET_SHAPE_TRIANGLE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("Notification"),
				  GPM_EVENT_NOTIFICATION,
				  GPM_GRAPH_WIDGET_COLOUR_GREY,
				  GPM_GRAPH_WIDGET_SHAPE_CIRCLE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("DPMS On"),
				  GPM_EVENT_DPMS_ON,
				  GPM_GRAPH_WIDGET_COLOUR_CYAN,
				  GPM_GRAPH_WIDGET_SHAPE_CIRCLE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("DPMS Standby"),
				  GPM_EVENT_DPMS_STANDBY,
				  GPM_GRAPH_WIDGET_COLOUR_CYAN,
				  GPM_GRAPH_WIDGET_SHAPE_TRIANGLE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("DPMS Suspend"),
				  GPM_EVENT_DPMS_SUSPEND,
				  GPM_GRAPH_WIDGET_COLOUR_CYAN,
				  GPM_GRAPH_WIDGET_SHAPE_SQUARE);
	gpm_graph_widget_key_add (GPM_GRAPH_WIDGET (widget),
				  _("DPMS Off"),
				  GPM_EVENT_DPMS_OFF,
				  GPM_GRAPH_WIDGET_COLOUR_CYAN,
				  GPM_GRAPH_WIDGET_SHAPE_DIAMOND);
				       

	/* FIXME: There's got to be a better way than this */
	gtk_widget_hide (GTK_WIDGET (widget));
	gtk_widget_show (GTK_WIDGET (widget));

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "combobox_type");
	gpm_statistics_populate_graph_types (statistics, widget);

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "combobox_device");
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "Default");
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "checkbutton_events");
	gpm_conf_get_bool (statistics->priv->conf, GPM_CONF_STAT_SHOW_EVENTS, &checked);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_statistics_checkbox_events_cb), statistics);
	gpm_statistics_checkbox_events_cb (widget, statistics);

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "checkbutton_legend");
	gpm_conf_get_bool (statistics->priv->conf, GPM_CONF_STAT_SHOW_LEGEND, &checked);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_statistics_checkbox_legend_cb), statistics);
	gpm_statistics_checkbox_legend_cb (widget, statistics);

	gtk_widget_show (main_window);
	g_timeout_add (GPM_STATISTICS_POLL_INTERVAL,
		       gpm_statistics_graph_refresh, statistics);
}

/**
 * gpm_statistics_finalize:
 * @object: This graph class instance
 **/
static void
gpm_statistics_finalize (GObject *object)
{
	GpmStatistics *statistics;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_STATISTICS (object));

	statistics = GPM_STATISTICS (object);
	statistics->priv = GPM_STATISTICS_GET_PRIVATE (statistics);

	g_object_unref (statistics->priv->conf);
	g_object_unref (statistics->priv->gproxy);
	g_object_unref (statistics->priv->events);
	g_object_unref (statistics->priv->data);

	G_OBJECT_CLASS (gpm_statistics_parent_class)->finalize (object);
}

/**
 * gpm_statistics_new:
 * Return value: new GpmStatistics instance.
 **/
GpmStatistics *
gpm_statistics_new (void)
{
	GpmStatistics *statistics;
	statistics = g_object_new (GPM_TYPE_STATISTICS, NULL);
	return GPM_STATISTICS (statistics);
}
