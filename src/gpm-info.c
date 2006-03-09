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
#include "gpm-info.h"
#include "gpm-debug.h"
#include "gpm-power.h"
#include "gpm-graph-widget.h"

static void     gpm_info_class_init (GpmInfoClass *klass);
static void     gpm_info_init       (GpmInfo      *info);
static void     gpm_info_finalize   (GObject      *object);

#define GPM_INFO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO, GpmInfoPrivate))

#define GPM_INFO_HARDWARE_POLL		10 /* seconds */
#define GPM_INFO_DATA_RES_X		40 /* x resolution, greater burns the cpu */
//#define DO_TESTING			TRUE

typedef struct
{
	GList		*log_data;
	GList		*graph_data;
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
};

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_MODE,
	PROP_SYSTEM_TIMEOUT
};


static GObjectClass *parent_class = NULL;
G_DEFINE_TYPE (GpmInfo, gpm_info, G_TYPE_OBJECT)

/** handler for libglade to provide interface with a pointer */
static GtkWidget *
gpm_graph_custom_handler (GladeXML *xml,
			  gchar *func_name, gchar *name,
			  gchar *string1, gchar *string2,
			  gint int1, gint int2,
			  gpointer user_data)
{
	GtkWidget *widget = NULL;
	if (strcmp ("gpm_simple_graph_new", func_name) == 0) {
		widget = gpm_simple_graph_new ();
		return widget;
	}
	return NULL;
}

/** add an x-y point to a list */
static void
gpm_info_data_point_add (GList **list, int x, int y)
{
	g_return_if_fail (list);
	GpmSimpleDataPoint *data_point;
	data_point = g_new (GpmSimpleDataPoint, 1);
	data_point->x = x;
	data_point->y = y;
	*list = g_list_append (*list, (gpointer) data_point);
}

/** normalise both axes to 0..100 */
static void
gpm_stat_calculate_percentage (GList *source, GList **destination, int num_points)
{
	g_return_if_fail (source);
	g_return_if_fail (destination);
	int count = 0;		/* what number of max_count we are at */
	int max_count;		/* how many data values do we want */
	int this_count = 0;	/* what data point we are working on */
	int average_add = 0;
	int value_y, value_x;
	int a;
	int *point;
	float percentage_step = 100.0f / (float) num_points;
	max_count = g_list_length (source) / num_points;

	for (a=0; a < g_list_length (source) - 1; a++) {
		point = (int*) g_list_nth_data (source, a);
		average_add += *point;
		if (count == max_count) {
			value_y = (float) average_add / (float) (max_count + 1);
			value_x = (int) (percentage_step * (float) this_count);
//			gpm_debug ("(%i) value = (%i, %i)", this_count, value_x, value_y);
			gpm_info_data_point_add (destination, value_x, value_y);
			count = 0;
			average_add = 0;
			this_count++;
		} else {
			count++;
		}
	}
	/* we do not calculate the average on the remainder as the sample size would
	   not be equal, and thus not representative of the final value */
}

/** help callback */
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

/* close callback */
static void
gpm_info_close_cb (GtkWidget	*widget,
		   GpmInfo	*info)
{
	gtk_widget_destroy (info->priv->main_window);
	info->priv->main_window = NULL;
}

/** free a list of x-y points */
static void
gpm_info_data_point_free (GList *list)
{
	g_return_if_fail (list);

	int a;
	GpmSimpleDataPoint *d_point;

	/* free elements */
	for (a=0; a<g_list_length (list); a++) {
		d_point = (GpmSimpleDataPoint*) g_list_nth_data (list, a);
		g_free (d_point);
	}
	g_list_free (list);
}

#if 0
/** print a list of x-y points */
static void
gpm_info_data_point_print (GList *list)
{
	g_return_if_fail (list);
	int a;
	GpmSimpleDataPoint *d_point;
	/* print graph data */
	gpm_debug ("GRAPH DATA");
	for (a=0; a<g_list_length (list); a++) {
		d_point = (GpmSimpleDataPoint*) g_list_nth_data (list, a);
		gpm_debug ("data %i = (%i, %i)", a, d_point->x, d_point->y);
	}
}
#endif

/** find the largest and smallest integer values in a list */
static void
gpm_info_log_find_range (GList *list, int *smallest, int *biggest)
{
	g_return_if_fail (list);
	int *data;
	int a;
	*smallest = 100000;
	*biggest = 0;
	for (a=0; a < g_list_length (list) - 1; a++) {
		data = (int*) g_list_nth_data (list, a);
		if (*data > *biggest) {
			*biggest = *data;
		}
		if (*data < *smallest) {
			*smallest = *data;
		}
	}
}

/** free a scalar log */
static void
gpm_info_log_free (GList *list)
{
	g_return_if_fail (list);
	int *data;
	int a;
	for (a=0; a<g_list_length (list); a++) {
		data = (int*) g_list_nth_data (list, a);
		g_free (data);
	}
	g_list_free (list);
}

/** update this graph */
static void
gpm_info_graph_update (GpmInfoGraphData *graph_data)
{
	int max_time;
	int smallest = 0;
	int biggest = 0;

	/* free existing data if exists */
	if (graph_data->graph_data) {
		gpm_info_data_point_free (graph_data->graph_data);
		graph_data->graph_data = NULL;
	}

	if (graph_data->log_data) {
		gpm_stat_calculate_percentage (graph_data->log_data,
					       &(graph_data->graph_data),
					       GPM_INFO_DATA_RES_X + 1);
		gpm_simple_graph_set_data (GPM_SIMPLE_GRAPH (graph_data->widget), graph_data->graph_data);

		/* set the x-axis to the time that we have been sampling for */
		max_time = (GPM_INFO_HARDWARE_POLL * g_list_length (graph_data->log_data)) / 60;
		if (max_time < 10) {
			max_time = 10;
		}
		gpm_simple_graph_set_stop_x (GPM_SIMPLE_GRAPH (graph_data->widget), max_time);

		/* get the biggest and smallest value of the data */
		gpm_info_log_find_range (graph_data->log_data, &smallest, &biggest);
		if (biggest < 10) {
			biggest = 10;
		}
		gpm_simple_graph_set_stop_y (GPM_SIMPLE_GRAPH (graph_data->widget), biggest);
	} else {
		gpm_debug ("no log data");
	}

	/* FIXME: There's got to be a better way than this */
	gtk_widget_hide (graph_data->widget);
	gtk_widget_show (graph_data->widget);
}

/* pahh, wrong on so many levels. This needs to be fixed */
static GtkTreeModel *
create_tree_model (GArray *array)
{
	int a;
	GtkListStore *store;
	GtkTreeIter iter;

	/* create list store */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	GpmPowerDescriptionItem *di;
	/* add data to the list store */
	for (a=0; a<array->len; a++) {
		di = &g_array_index (array, GpmPowerDescriptionItem, a);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, di->title,
				    1, di->value,
				    -1);
	}
	return GTK_TREE_MODEL (store);
}

/* pahh, wrong on so many levels. This needs to be fixed */
static void
create_tree_widget (GtkWidget *widget, GArray *array)
{
	GtkTreeModel *model;

	/* create tree model */
	model = create_tree_model (array);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), model);                             
	g_object_unref (model);

	/* add columns to the tree view */
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", 0, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 0);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Value", renderer, "text", 1, NULL);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (widget), TRUE);
}

static void
populate_device_information (GpmInfo *info)
{
	int		 number;
	GtkWidget	*widget;
	GArray		*array;

	number = gpm_power_get_num_devices_of_kind (info->priv->power,
						    GPM_POWER_BATTERY_KIND_PRIMARY);
	gpm_debug ("primary has %i", number);
	if (number > 0) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary0");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_PRIMARY, 0);
		create_tree_widget (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_primary0");
		gtk_widget_show_all (widget);
	}
	if (number > 1) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_primary1");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_PRIMARY, 1);
		create_tree_widget (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_primary1");
		gtk_widget_show_all (widget);
	}
	number = gpm_power_get_num_devices_of_kind (info->priv->power,
						    GPM_POWER_BATTERY_KIND_UPS);
	gpm_debug ("ups has %i", number);
	if (number > 0) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_ups");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_UPS, 0);
		create_tree_widget (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_ups");
		gtk_widget_show_all (widget);
	}
	number = gpm_power_get_num_devices_of_kind (info->priv->power,
						    GPM_POWER_BATTERY_KIND_MOUSE);
	gpm_debug ("mouse has %i", number);
	if (number > 0) {
		widget = glade_xml_get_widget (info->priv->glade_xml, "treeview_mouse");
		array = gpm_power_get_description_array (info->priv->power,
							 GPM_POWER_BATTERY_KIND_MOUSE, 0);
		create_tree_widget (widget, array);
		gpm_power_free_description_array (array);
		widget = glade_xml_get_widget (info->priv->glade_xml, "frame_mouse");
		gtk_widget_show_all (widget);
	}
}

/** setup the information window */
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
	gtk_window_set_icon_from_file (GTK_WINDOW (info->priv->main_window),
				       GPM_DATA "gnome-power-manager.png", NULL);

	g_signal_connect (info->priv->main_window, "delete_event",
			  G_CALLBACK (gpm_info_close_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_close_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_help_cb), info);

	widget = glade_xml_get_widget (glade_xml, "graph_percentage");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->percentage->widget = widget;
	gpm_simple_graph_set_axis_y (GPM_SIMPLE_GRAPH (widget), GPM_GRAPH_TYPE_PERCENTAGE);

	widget = glade_xml_get_widget (glade_xml, "graph_rate");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->rate->widget = widget;
	gpm_simple_graph_set_axis_y (GPM_SIMPLE_GRAPH (widget), GPM_GRAPH_TYPE_RATE);

	widget = glade_xml_get_widget (glade_xml, "graph_time");
	gtk_widget_set_size_request (widget, 600, 300);
	info->priv->time->widget = widget;
	gpm_simple_graph_set_axis_y (GPM_SIMPLE_GRAPH (widget), GPM_GRAPH_TYPE_TIME);

	populate_device_information (info);

	gpm_info_graph_update (info->priv->rate);
	gpm_info_graph_update (info->priv->percentage);
	gpm_info_graph_update (info->priv->time);

	gtk_widget_show (info->priv->main_window);
}

/** add a scalar point to the graph log */
static void
gpm_info_graph_add (GpmInfoGraphData *graph_data, int value)
{
	int *point;
	point = g_new (int, 1);
	*point = value;
#ifdef DO_TESTING
	static int auto_inc_val = 0;
	auto_inc_val++;
	if (auto_inc_val == 100) {
		auto_inc_val = 0;
	}
	*point += auto_inc_val;
#endif
	graph_data->log_data = g_list_append (graph_data->log_data,
					      (gpointer) point);
}

/** init log and graph data elements */
static void
gpm_info_graph_init (GpmInfoGraphData *graph_data)
{
	graph_data->log_data = NULL;
	graph_data->graph_data = NULL;
	graph_data->widget = NULL;
}

/** callback to get the log data every minute */
static gboolean
log_do_poll (gpointer data)
{
	GpmInfo *info = (GpmInfo*) data;

	GpmPowerBatteryStatus battery_status;
	gpm_power_get_battery_status (info->priv->power,
				      GPM_POWER_BATTERY_KIND_PRIMARY,
				      &battery_status);

	gpm_info_graph_add (info->priv->rate, battery_status.percentage_charge);
	gpm_info_graph_add (info->priv->percentage, battery_status.charge_rate / 1000);
	gpm_info_graph_add (info->priv->time, battery_status.remaining_time);

	if (info->priv->main_window) {
		gpm_info_graph_update (info->priv->rate);
		gpm_info_graph_update (info->priv->percentage);
		gpm_info_graph_update (info->priv->time);
		//gpm_info_data_point_print (info->priv->rate.graph_data);
	}
	
	/* also update the first tab */
	//populate_device_information (info);
	return TRUE;
}

/** logging system needs access to the power stuff */
void
gpm_info_set_power (GpmInfo *info, GpmPower *power)
{
	info->priv->power = power;
}

/** intialise the class */
static void
gpm_info_class_init (GpmInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gpm_info_finalize;
	g_type_class_add_private (klass, sizeof (GpmInfoPrivate));
}

/** intialise the object */
static void
gpm_info_init (GpmInfo *info)
{
	info->priv = GPM_INFO_GET_PRIVATE (info);

	info->priv->main_window = NULL;

	info->priv->rate = g_new (GpmInfoGraphData, 1);
	info->priv->percentage = g_new (GpmInfoGraphData, 1);
	info->priv->time = g_new (GpmInfoGraphData, 1);

	gpm_info_graph_init (info->priv->rate);
	gpm_info_graph_init (info->priv->percentage);
	gpm_info_graph_init (info->priv->time);

	/* set up the timer callback so we can log data every minute */
	g_timeout_add (GPM_INFO_HARDWARE_POLL * 1000, log_do_poll, info);
}

/* free the scalar and x-y elements of a graph */
static void
gpm_info_graph_free (GpmInfoGraphData *graph_data)
{
	gpm_info_log_free (graph_data->log_data);
	gpm_info_data_point_free (graph_data->graph_data);
	g_free (graph_data);
}

/** finalise the object */
static void
gpm_info_finalize (GObject *object)
{
	GpmInfo *info;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_INFO (object));

	info = GPM_INFO (object);
	info->priv = GPM_INFO_GET_PRIVATE (info);

	gpm_info_graph_free (info->priv->rate);
	gpm_info_graph_free (info->priv->percentage);
	gpm_info_graph_free (info->priv->time);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/** create the object */
GpmInfo *
gpm_info_new (void)
{
	GpmInfo *info;

	info = g_object_new (GPM_TYPE_INFO, NULL);

	return GPM_INFO (info);
}
