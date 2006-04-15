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
#include <glib/gprintf.h>

#include <glade/glade.h>
#include <libgnomeui/gnome-help.h>
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>

#include "gpm-info.h"
#include "gpm-info-data.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-power.h"
#include "gpm-graph-widget.h"
#include "gpm-stock-icons.h"
#include "gpm-info-data.h"

static void     gpm_info_class_init (GpmInfoClass *klass);
static void     gpm_info_init       (GpmInfo      *info);
static void     gpm_info_finalize   (GObject      *object);

#define GPM_INFO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO, GpmInfoPrivate))

#define GPM_INFO_DATA_POLL		5	/* seconds */
#define GPM_INFO_DATA_RES_X		80	/* x resolution, greater burns the cpu */

struct GpmInfoPrivate
{
	GpmPower		*power;

	GtkWidget		*rate_widget;
	GtkWidget		*percentage_widget;
	GtkWidget		*time_widget;
	GtkWidget		*main_window;
	GtkWidget		*treeview_event_viewer;

	GpmInfoData		*events;
	GpmInfoData		*rate_data;
	GpmInfoData		*time_data;
	GpmInfoData		*percentage_data;

	GladeXML		*glade_xml;

	time_t		 start_time;
};

G_DEFINE_TYPE (GpmInfo, gpm_info, G_TYPE_OBJECT)

/**
 * gpm_graph_custom_handler:
 * @xml: The glade file we are reading.
 * @func_name: The function name to create the object
 *
 * Handler for libglade to provide interface with a pointer
 *
 * Return value: The custom widget.
 **/
static GtkWidget *
gpm_graph_custom_handler (GladeXML *xml,
			  gchar *func_name, gchar *name,
			  gchar *string1, gchar *string2,
			  gint int1, gint int2,
			  gpointer user_data)
{
	GtkWidget *widget = NULL;
	if (strcmp ("gpm_graph_new", func_name) == 0) {
		widget = gpm_graph_new ();
		return widget;
	}
	return NULL;
}

/**
 * gpm_info_graph_update:
 * @graph_data: The data we have for a specific graph
 *
 * Update this graph
 **/
static void
gpm_info_graph_update (GList *data, GtkWidget *widget, GList *events)
{
	if (! data) {
		return;
	}
	if (data) {
		gpm_graph_set_data (GPM_GRAPH (widget), data);
		gpm_graph_set_events (GPM_GRAPH (widget), events);
	} else {
		gpm_debug ("no log data");
	}

	/* FIXME: There's got to be a better way than this */
	gtk_widget_hide (widget);
	gtk_widget_show (widget);
}

/**
 * gpm_info_update_tree:
 * @widget: The GtkWidget object
 * @array: The array of GpmPowerDescriptionItem obtained about the hardware
 *
 * Update the tree widget with new data that we obtained from power.
 **/
static void
gpm_info_update_tree (GtkWidget *widget, GArray *array)
{
	int a;
	GtkListStore *store;
	GtkTreeIter   iter;
	GpmPowerDescriptionItem *di;

	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	/* add data to the list store */
	gpm_debug ("Printing %i items", array->len);
#if 0
	GpmPowerDescriptionItem *di2;
	for (a=0; a<array->len; a+=2) {
		gtk_list_store_append (store, &iter);
		di = &g_array_index (array, GpmPowerDescriptionItem, a);
		if (a+1 < array->len) {
			di2 = &g_array_index (array, GpmPowerDescriptionItem, a+1);
			gtk_list_store_set (store, &iter,
					    0, di->title,
					    1, di->value,
					    2, di2->title,
					    3, di2->value,
					    -1);
		} else {
			gtk_list_store_set (store, &iter,
					    0, di->title,
					    1, di->value,
					    -1);
		}
	}
#else
	for (a=0; a<array->len; a+=1) {
		gtk_list_store_append (store, &iter);
		di = &g_array_index (array, GpmPowerDescriptionItem, a);
		gtk_list_store_set (store, &iter,
				    0, di->title,
				    1, di->value,
				    -1);
	}
#endif
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));
	g_object_unref (store);
}

/**
 * gpm_info_create_device_info_tree:
 * @widget: The GtkWidget object
 *
 * Create the tree widget, setting up the columns
 **/
static void
gpm_info_create_device_info_tree (GtkWidget *widget)
{
	/* add columns to the tree view */
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	const int MIN_SIZE = 150;

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", 0, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 0);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_column_set_min_width (column, MIN_SIZE);

	column = gtk_tree_view_column_new_with_attributes ("Value", renderer, "text", 1, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_column_set_min_width (column, MIN_SIZE);

	column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", 2, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 2);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_set_min_width (column, MIN_SIZE);

	column = gtk_tree_view_column_new_with_attributes ("Value", renderer, "text", 3, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 3);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_column_set_min_width (column, MIN_SIZE);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (widget), TRUE);
}

/**
 * gpm_info_populate_device_information:
 * @info: This info class instance
 *
 * Populate the 4 possible treeviews, depending on the hardware we have
 * available on our system.
 **/
static void
gpm_info_populate_device_information (GpmInfo *info)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (GPM_IS_INFO (info));
	int		 number;
	GtkWidget	*widget;
	GArray		*array;

	number = gpm_power_get_num_devices_of_kind (info->priv->power,
						    GPM_POWER_BATTERY_KIND_PRIMARY);
	if (number > 0) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary0");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_PRIMARY, 0);
		gpm_info_update_tree (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_primary0");
		gtk_widget_show_all (widget);
	}
	if (number > 1) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary1");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_PRIMARY, 1);
		gpm_info_update_tree (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_primary1");
		gtk_widget_show_all (widget);
	}
	number = gpm_power_get_num_devices_of_kind (info->priv->power,
						    GPM_POWER_BATTERY_KIND_UPS);
	if (number > 0) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_ups");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_UPS, 0);
		gpm_info_update_tree (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_ups");
		gtk_widget_show_all (widget);
	}
	number = gpm_power_get_num_devices_of_kind (info->priv->power,
						    GPM_POWER_BATTERY_KIND_MOUSE);
	if (number > 0) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_mouse");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_MOUSE, 0);
		gpm_info_update_tree (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_mouse");
		gtk_widget_show_all (widget);
	}
}

/**
 * gpm_info_create_event_viewer_tree:
 * @widget: The GtkWidget object
 *
 * Create the event viewer widget, setting up the columns
 **/
static void
gpm_info_create_event_viewer_tree (GtkWidget *widget)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Time"), renderer, "text", 0, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 0);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_column_set_min_width (column, 120);

	column = gtk_tree_view_column_new_with_attributes (_("Event"), renderer, "text", 1, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_column_set_min_width (column, 200);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (widget), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), TRUE);
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (widget), FALSE);
}

/**
 * gpm_info_get_time_string:
 * @time: A time_t value
 *
 * Converts a time_t to a string description.
 * The return value must be freed using g_free().
 * Return value: The timestring, e.g. "Sat Apr 15, 15:35:40".
 **/
static char *
gpm_info_get_time_string (time_t time)
{
	char outstr[256];
	struct tm *tmp;
	tmp = localtime (&time);
	strftime (outstr, sizeof(outstr), "%a %b %d, %T", tmp);
	return g_strdup (outstr);
}

/**
 * gpm_info_update_event_tree:
 * @info: This info class instance
 *
 * Updates the event log tree widget with the data we currently have.
 **/
static void
gpm_info_update_event_tree (GpmInfo *info)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (GPM_IS_INFO (info));
	char *timestring;
	const char *descstring;
	GtkListStore *store;
	GtkTreeIter   iter;
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	GpmInfoDataPoint *new;
	GList *l;
	GList *events = gpm_info_data_get_list (info->priv->events);
	for (l=events; l != NULL; l=l->next) {
		new = (GpmInfoDataPoint *) l->data;
		timestring = gpm_info_get_time_string (info->priv->start_time + new->time);
		descstring = gpm_graph_event_description (new->value);
		gpm_debug ("event log: %s: %s", timestring, descstring);
		/* add data to the list store */
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, timestring, 1, descstring, -1);
		g_free (timestring);
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (info->priv->treeview_event_viewer),
				 GTK_TREE_MODEL (store));
	g_object_unref (store);
}

/**
 * gpm_info_graph_update_all:
 * @info: This info class instance
 *
 * Updates all the graphs together.
 **/
static void
gpm_info_graph_update_all (GpmInfo *info)
{
	GList *data = gpm_info_data_get_list (info->priv->rate_data);
	GList *events = gpm_info_data_get_list (info->priv->events);
	gpm_info_graph_update (data, info->priv->rate_widget, events);
	data = gpm_info_data_get_list (info->priv->percentage_data);
	gpm_info_graph_update (data, info->priv->percentage_widget, events);
	data = gpm_info_data_get_list (info->priv->time_data);
	gpm_info_graph_update (data, info->priv->time_widget, events);
}

/**
 * gpm_info_close_cb:
 * @widget: The GtkWidget button object
 * @info: This info class instance
 **/
static void
gpm_info_close_cb (GtkWidget *widget,
		   GpmInfo   *info)
{
	if (info->priv->main_window) {
		gtk_widget_destroy (info->priv->main_window);
		info->priv->main_window = NULL;
	}
}

/**
 * gpm_info_delete_event_cb:
 * @widget: The GtkWidget object
 * @event: The event type, unused.
 * @info: This info class instance
 **/
static gboolean
gpm_info_delete_event_cb (GtkWidget *widget,
			  GdkEvent  *event,
			  GpmInfo   *info)
{
	gpm_info_close_cb (widget, info);
	return FALSE;
}

/**
 * gpm_info_clear_cb:
 * @widget: The GtkWidget button object
 * @info: This info class instance
 *
 * Clears the data from memory, so that the event log and the reset graphs.
 **/
static void
gpm_info_clear_cb (GtkWidget *widget,
		   GpmInfo   *info)
{
	/* clear data */
	g_object_unref (info->priv->rate_data);
	g_object_unref (info->priv->percentage_data);
	g_object_unref (info->priv->time_data);
	g_object_unref (info->priv->events);

	/* set to a blank list */
	info->priv->events = gpm_info_data_new ();
	info->priv->percentage_data = gpm_info_data_new ();
	info->priv->rate_data = gpm_info_data_new ();
	info->priv->time_data = gpm_info_data_new ();

	/* update widgets */
	gpm_info_update_event_tree (info);
	gpm_info_graph_update_all (info);
}

/**
 * gpm_info_help_cb:
 * @widget: The GtkWidget button object
 * @info: This info class instance
 **/
static void
gpm_info_help_cb (GtkWidget *widget,
		  GpmInfo   *info)
{
	GError *error = NULL;

	gnome_help_display ("gnome-power-manager.xml", NULL, &error);
	if (error != NULL) {
		gpm_warning (error->message);
		g_error_free (error);
	}
}

/**
 * gpm_info_show_window:
 * @info: This info class instance
 *
 * Show the information window, setting up callbacks and initialising widgets
 * when required.
 **/
void
gpm_info_show_window (GpmInfo *info)
{
	GtkWidget    *widget;
	GladeXML     *glade_xml;

	g_return_if_fail (info != NULL);
	g_return_if_fail (GPM_IS_INFO (info));

	if (info->priv->main_window) {
		gpm_debug ("already showing info");
		return;
	}

	glade_xml = glade_xml_new (GPM_DATA "/gpm-info.glade", NULL, NULL);
	info->priv->glade_xml = glade_xml;
	/* don't segfault on missing glade file */
	if (! glade_xml) {
		gpm_critical_error ("gpm-info.glade not found");
	}
	info->priv->main_window = glade_xml_get_widget (glade_xml, "window_info");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (info->priv->main_window);
	gtk_window_set_icon_name (GTK_WINDOW(info->priv->main_window), GPM_STOCK_APP_ICON);

	g_signal_connect (info->priv->main_window, "delete_event",
			  G_CALLBACK (gpm_info_delete_event_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_clear");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_clear_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_close_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_help_cb), info);

	widget = glade_xml_get_widget (glade_xml, "graph_percentage");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->percentage_widget = widget;
	gpm_graph_set_axis_y (GPM_GRAPH (widget), GPM_GRAPH_TYPE_PERCENTAGE);

	widget = glade_xml_get_widget (glade_xml, "graph_rate");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->rate_widget = widget;
	gpm_graph_set_axis_y (GPM_GRAPH (widget), GPM_GRAPH_TYPE_RATE);
	gpm_graph_enable_legend (GPM_GRAPH (widget), TRUE);

	widget = glade_xml_get_widget (glade_xml, "graph_time");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->time_widget = widget;
	gpm_graph_set_axis_y (GPM_GRAPH (widget), GPM_GRAPH_TYPE_TIME);
	gpm_graph_enable_legend (GPM_GRAPH (widget), TRUE);

	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary0");
	gpm_info_create_device_info_tree (widget);
	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary1");
	gpm_info_create_device_info_tree (widget);
	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_ups");
	gpm_info_create_device_info_tree (widget);
	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_mouse");
	gpm_info_create_device_info_tree (widget);

	/* set up the event viewer tree-view */
	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_event_log");
	info->priv->treeview_event_viewer = widget;
	gpm_info_create_event_viewer_tree (widget);

	gpm_info_populate_device_information (info);
	gpm_info_graph_update_all (info);
	gpm_info_update_event_tree (info);

	gtk_widget_show (info->priv->treeview_event_viewer);
	gtk_widget_show (info->priv->rate_widget);
	gtk_widget_show (info->priv->time_widget);
	gtk_widget_show (info->priv->percentage_widget);
	gtk_widget_show (info->priv->main_window);
}

/**
 * gpm_info_event_log
 * @info: This info class instance
 * @event: The event description, e.g. "Application started"
 *
 * Adds an point to the event log
 **/
void
gpm_info_event_log (GpmInfo *info, GpmGraphEvent event)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (GPM_IS_INFO (info));
	gpm_debug ("Adding %i to the event log", event);

	gpm_info_data_add_always (info->priv->events,
				  time (NULL) - info->priv->start_time,
				  event,
				  gpm_graph_event_colour (event));
	if (info->priv->main_window) {
		/* do this only if the main window is loaded */
		gpm_info_update_event_tree (info);
	}
}

/**
 * gpm_info_log_do_poll:
 * @data: gpointer to this info class instance
 *
 * This is the callback to get the log data every timeout period, where we have
 * to add points to the database and also update the graphs.
 **/
static gboolean
gpm_info_log_do_poll (gpointer data)
{
	GpmInfo *info = (GpmInfo*) data;

	int value_x;

	GpmPowerBatteryStatus battery_status;
	gpm_power_get_battery_status (info->priv->power,
				      GPM_POWER_BATTERY_KIND_PRIMARY,
				      &battery_status);

	/* work out seconds elapsed */
	value_x = time (NULL) - (info->priv->start_time + GPM_INFO_DATA_POLL);
	gpm_info_data_add (info->priv->percentage_data,
			   value_x,
			   battery_status.percentage_charge, 0);
	gpm_info_data_add (info->priv->rate_data,
			   value_x,
			   battery_status.charge_rate_raw, 0);
	gpm_info_data_add (info->priv->time_data,
			   value_x,
			   battery_status.remaining_time, 0);

	if (info->priv->main_window) {
		gpm_info_graph_update_all (info);
		/* also update the first tab */
		gpm_info_populate_device_information (info);
	}
	return TRUE;
}

/**
 * gpm_info_set_power:
 * @info: This info class instance
 * @power: The power class instance
 *
 * The logging system needs access to the power stuff for the device
 * information and also the values to log for the graphs.
 **/
void
gpm_info_set_power (GpmInfo *info, GpmPower *power)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (GPM_IS_INFO (info));
	info->priv->power = power;
}

/**
 * gpm_info_class_init:
 * @klass: This info class instance
 **/
static void
gpm_info_class_init (GpmInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_info_finalize;
	g_type_class_add_private (klass, sizeof (GpmInfoPrivate));
}

/**
 * gpm_info_init:
 * @info: This info class instance
 **/
static void
gpm_info_init (GpmInfo *info)
{
	info->priv = GPM_INFO_GET_PRIVATE (info);

	/* record our start time */
	info->priv->start_time = time (NULL);

	/* set up the timer callback so we can log data */
	g_timeout_add (GPM_INFO_DATA_POLL * 1000, gpm_info_log_do_poll, info);

	/* set to a blank list */
	info->priv->events = gpm_info_data_new ();
	info->priv->percentage_data = gpm_info_data_new ();
	info->priv->rate_data = gpm_info_data_new ();
	info->priv->time_data = gpm_info_data_new ();

	glade_set_custom_handler (gpm_graph_custom_handler, info);
}

/**
 * gpm_info_finalize:
 * @object: This info class instance
 **/
static void
gpm_info_finalize (GObject *object)
{
	GpmInfo *info;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_INFO (object));

	info = GPM_INFO (object);
	info->priv = GPM_INFO_GET_PRIVATE (info);

	g_object_unref (info->priv->rate_data);
	g_object_unref (info->priv->percentage_data);
	g_object_unref (info->priv->time_data);
	g_object_unref (info->priv->events);

	G_OBJECT_CLASS (gpm_info_parent_class)->finalize (object);
}

/**
 * gpm_info_new:
 * Return value: new GpmInfo instance.
 **/
GpmInfo *
gpm_info_new (void)
{
	GpmInfo *info;
	info = g_object_new (GPM_TYPE_INFO, NULL);
	return GPM_INFO (info);
}
