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
#include <time.h>

#include "gpm-info.h"
#include "gpm-debug.h"
#include "gpm-power.h"
#include "gpm-graph-widget.h"
#include "gpm-stock-icons.h"

static void     gpm_info_class_init (GpmInfoClass *klass);
static void     gpm_info_init       (GpmInfo      *info);
static void     gpm_info_finalize   (GObject      *object);

#define GPM_INFO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO, GpmInfoPrivate))

#define GPM_INFO_DATA_POLL		5	/* seconds */
#define GPM_INFO_DATA_RES_X		80	/* x resolution, greater burns the cpu */
#define GPM_INFO_MAX_POINTS		120	/* when we should simplify data */

typedef struct
{
	GList		*data;
	GtkWidget	*widget;
} GpmInfoGraphData;

struct GpmInfoPrivate
{
	GpmPower		*power;

	GpmInfoGraphData	*rate;
	GpmInfoGraphData	*percentage;
	GpmInfoGraphData	*time;

	GladeXML		*glade_xml;
	GtkWidget		*main_window;

	time_t           	 start_time;
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
 * gpm_info_data_point_add_to_data:
 * @graph_data: The data we have for a specific graph
 * @x: The X data point
 * @y: The Y data point
 *
 * Allocates the memory and adds to the list.
 **/
static void
gpm_info_data_point_add_to_data (GpmInfoGraphData *graph_data,
				 int		   x,
				 int		   y,
				 GpmGraphColour	   colour,
				 gboolean	   point)
{
	GpmDataPoint *new_point;
	/* we have to add a new data point */
	new_point = g_slice_new (GpmDataPoint);
	new_point->x = x;
	new_point->y = y;
	new_point->colour = colour;
	new_point->info_point = point;
	graph_data->data = g_list_append (graph_data->data, (gpointer) new_point);
}

/**
 * gpm_info_data_point_add:
 * @graph_data: The data we have for a specific graph
 * @max_num: The max desired points
 * @use_time: If we should use a per-time formula
 *
 * We need to reduce the number of data points else the graph will take a long
 * time to plot accuracy we don't need at the larger scales.
 **/
static void
gpm_info_data_point_limit (GpmInfoGraphData *graph_data,
			   int		     max_num,
			   gboolean	     use_time)
{
	GpmDataPoint *point;
	GList *l;
	GList *list;

	gpm_debug ("Listing old points");
	for (l=graph_data->data; l != NULL; l=l->next) {
		point = (GpmDataPoint *) l->data;
		gpm_debug ("x=%i, y=%i", point->x, point->y);
	}

	if (use_time) {
		list = g_list_last (graph_data->data);
		point = (GpmDataPoint *) list->data;
		gpm_debug ("Last point: x=%i, y=%i", point->x, point->y);
		float div = (float) point->x / (float) max_num;
		gpm_debug ("Using a time division of %f", div);

		GList *new = NULL;
		/* Reduces the number of points to a pre-set level using a time
		 * division algorithm so we don't keep diluting the previous
		 * data with a conventional 1-in-x type algorithm. */
		float a = 0;
		gpm_debug ("Going thru points");
		for (l=graph_data->data; l != NULL; l=l->next) {
			point = (GpmDataPoint *) l->data;
			if (point->info_point) {
				/* adding info point */
//				gpm_debug ("Adding info point : x=%i, y=%i", point->x, point->y);
				new = g_list_append (new, (gpointer) point);
			} else if (point->x >= a) {
				/* adding valid point */
//				gpm_debug ("Adding valid point: x=%i, y=%i", point->x, point->y);
				new = g_list_append (new, (gpointer) point);
				a = a + div;
			} else {
				/* removing point */
//				gpm_debug ("Removing point    : x=%i, y=%i", point->x, point->y);
				g_slice_free (GpmDataPoint, point);
			}
		}
		/* freeing old list */
		g_list_free (graph_data->data);
		/* setting new data */
		graph_data->data = new;

	} else {
		/* Do a conventional 1-in-x type algorithm */
		int count = 0;
		for (l=graph_data->data; l != NULL; l=l->next) {
			point = (GpmDataPoint *) l->data;
			count++;
			if (count == 3) {
				list = l->prev;
				/* we need to free the data */
				g_slice_free (GpmDataPoint, l->data);
				graph_data->data = g_list_delete_link (graph_data->data, l);
				l = list;
				count = 0;
			}
		}
	}

	gpm_debug ("Listing new points");
	for (l=graph_data->data; l != NULL; l=l->next) {
		point = (GpmDataPoint *) l->data;
		gpm_debug ("x=%i, y=%i", point->x, point->y);
	}
}

/**
 * gpm_info_data_point_add:
 * @graph_data: The data we have for a specific graph
 * @x: The X data point
 * @y: The Y data point
 *
 * Adds an x-y point to a list. We have to save the X value as an integer, as
 * when we prune the values (when we have over 100) the X and Y values are
 * lost, and the data-points becomes non-uniform.
 **/
static void
gpm_info_data_point_add (GpmInfoGraphData *graph_data, int x, int y)
{
	int length = g_list_length (graph_data->data);
	if (length > 2) {
		GList *last = g_list_last (graph_data->data);
		GpmDataPoint *old = (GpmDataPoint *) last->data;
		if (old->y == y) {
			/* we are the same as we were before and not the first or
			   second point, just side the data time across */
			old->x = x;
		} else {
			/* we have to add a new data point */
			gpm_info_data_point_add_to_data (graph_data, x, y, 0, FALSE);
			if (y == 0) {
				/* if the rate suddenly drops we want a line
				   going down, then across, not a diagonal line.
				   Add an extra point so that we extend it horiz. */
				gpm_info_data_point_add_to_data (graph_data, x, y, 0, FALSE);
			}
		}
	} else {
		/* a new list requires a data point */
		gpm_info_data_point_add_to_data (graph_data, x, y, 0, FALSE);
	}
	gpm_debug ("Drawing %i lines", length);
	if (length > GPM_INFO_MAX_POINTS) {
		/* We have too much data, simplify by removing every 2nd link */
		gpm_debug ("Too many points (%i/%i)", length, GPM_INFO_MAX_POINTS);
		gpm_info_data_point_limit (graph_data, GPM_INFO_MAX_POINTS / 2, TRUE);
	}
}

/**
 * gpm_info_help_cb:
 * @widget: The GtkWidget object
 * @info: This info class instance
 **/
static void
gpm_info_help_cb (GtkWidget	*widget,
		  GpmInfo	*info)
{
	GError *error = NULL;

	gnome_help_display ("gnome-power-manager.xml", NULL, &error);
	if (error != NULL) {
		gpm_warning (error->message);
		g_error_free (error);
	}
}

/**
 * gpm_info_close_cb:
 * @widget: The GtkWidget object
 * @info: This info class instance
 **/
static void
gpm_info_close_cb (GtkWidget	*widget,
		   GpmInfo	*info)
{
	if (info->priv->main_window) {
		gtk_widget_destroy (info->priv->main_window);
		info->priv->main_window = NULL;
	}
}

/**
 * gpm_info_graph_update:
 * @graph_data: The data we have for a specific graph
 *
 * Update this graph
 **/
static void
gpm_info_graph_update (GpmInfoGraphData *graph_data)
{
	if (! graph_data) {
		return;
	}
	if (graph_data->data) {
		gpm_graph_set_data (GPM_GRAPH (graph_data->widget),
				    graph_data->data);
	} else {
		gpm_debug ("no log data");
	}

	/* FIXME: There's got to be a better way than this */
	gtk_widget_hide (graph_data->widget);
	gtk_widget_show (graph_data->widget);
}

/**
 * gpm_info_delete_event_cb:
 * @widget: The GtkWidget object
 * @event: The event type, unused.
 * @info: This info class instance
 **/
static gboolean
gpm_info_delete_event_cb (GtkWidget	*widget,
			  GdkEvent	*event,
			  GpmInfo	*info)
{
	gpm_info_close_cb (widget, info);
	return FALSE;
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
	GpmPowerDescriptionItem *di2;

	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	/* add data to the list store */
	gpm_debug ("Printing %i items", array->len);
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
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));                          
	g_object_unref (store);
}

/**
 * gpm_info_create_tree:
 * @widget: The GtkWidget object
 *
 * Create the tree widget, setting up the columns
 **/
static void
gpm_info_create_tree (GtkWidget *widget)
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

	if (info->priv->main_window) {
		gpm_debug ("already showing info");
		return;
	}

	glade_set_custom_handler (gpm_graph_custom_handler, info);
	glade_xml = glade_xml_new (GPM_DATA "/gpm-info.glade", NULL, NULL);
	info->priv->glade_xml = glade_xml;
	/* don't segfault on missing glade file */
	if (! glade_xml) {
		gpm_warning ("gpm-info.glade not found");
		return;
	}
	info->priv->main_window = glade_xml_get_widget (glade_xml, "window_info");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (info->priv->main_window);
	gtk_window_set_icon_name (GTK_WINDOW(info->priv->main_window), GPM_STOCK_APP_ICON);

	g_signal_connect (info->priv->main_window, "delete_event",
			  G_CALLBACK (gpm_info_delete_event_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_close_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_help_cb), info);

	widget = glade_xml_get_widget (glade_xml, "graph_percentage");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->percentage->widget = widget;
	gpm_graph_set_axis_y (GPM_GRAPH (widget), GPM_GRAPH_TYPE_PERCENTAGE);

	widget = glade_xml_get_widget (glade_xml, "graph_rate");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->rate->widget = widget;
	gpm_graph_set_axis_y (GPM_GRAPH (widget), GPM_GRAPH_TYPE_RATE);

	widget = glade_xml_get_widget (glade_xml, "graph_time");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->time->widget = widget;
	gpm_graph_set_axis_y (GPM_GRAPH (widget), GPM_GRAPH_TYPE_TIME);

	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary0");
	gpm_info_create_tree (widget);
	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary1");
	gpm_info_create_tree (widget);
	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_ups");
	gpm_info_create_tree (widget);
	widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_mouse");
	gpm_info_create_tree (widget);

	gpm_info_populate_device_information (info);

	gpm_info_graph_update (info->priv->rate);
	gpm_info_graph_update (info->priv->percentage);
	gpm_info_graph_update (info->priv->time);

	gtk_widget_show (info->priv->main_window);
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
	gpm_info_data_point_add (info->priv->percentage, value_x, battery_status.percentage_charge);
	gpm_info_data_point_add (info->priv->rate, value_x, battery_status.charge_rate_raw);
	gpm_info_data_point_add (info->priv->time, value_x, battery_status.remaining_time);

	if (info->priv->main_window) {
		gpm_info_graph_update (info->priv->rate);
		gpm_info_graph_update (info->priv->percentage);
		gpm_info_graph_update (info->priv->time);
		/* also update the first tab */
		gpm_info_populate_device_information (info);
	}
	return TRUE;
}

/**
 * gpm_info_log_do_poll:
 * @info: This info class instance
 * @event: The event type 
 *
 * Adds an interesting point to the graph.
 * TODO: Automatically make a legend.
 **/
void
gpm_info_interest_point	(GpmInfo *info, GpmGraphEvent event)
{
	GpmGraphColour colour = 0;
	int value_x;
	const char *event_desc;

	event_desc = gpm_graph_event_description (event, &colour);
	gpm_debug ("logging event %i: %s", event, event_desc);

	value_x = time (NULL) - (info->priv->start_time + GPM_INFO_DATA_POLL);
	gpm_info_data_point_add_to_data (info->priv->rate, value_x, 0, colour, TRUE);
	gpm_info_data_point_add_to_data (info->priv->time, value_x, 0, colour, TRUE);

	if (info->priv->main_window) {
		gpm_info_graph_update (info->priv->rate);
		gpm_info_graph_update (info->priv->time);
	}
}

/**  */
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

	info->priv->main_window = NULL;

	info->priv->rate = g_new (GpmInfoGraphData, 1);
	info->priv->percentage = g_new (GpmInfoGraphData, 1);
	info->priv->time = g_new (GpmInfoGraphData, 1);

	info->priv->rate->widget = NULL;
	info->priv->percentage->widget = NULL;
	info->priv->time->widget = NULL;

	info->priv->rate->data = NULL;
	info->priv->percentage->data = NULL;
	info->priv->time->data = NULL;

	/* record our start time */
	info->priv->start_time = time (NULL);

	/* set up the timer callback so we can log data */
	g_timeout_add (GPM_INFO_DATA_POLL * 1000, gpm_info_log_do_poll, info);
}

/**
 * gpm_info_free_graph_data:
 * @graph_data: The data we have for a specific graph
 *
 * Free the graph data elements, the list, and also the graph data object.
 **/
static void
gpm_info_free_graph_data (GpmInfoGraphData *graph_data)
{
	GList *l;
	for (l = graph_data->data; l != NULL; l = l->next) {
		g_slice_free (GpmDataPoint, l->data);
	}
	g_list_free (graph_data->data);
	g_free (graph_data);
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

	gpm_info_free_graph_data (info->priv->rate);
	gpm_info_free_graph_data (info->priv->percentage);
	gpm_info_free_graph_data (info->priv->time);

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
