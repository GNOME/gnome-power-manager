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

static void     gpm_info_class_init (GpmInfoClass *klass);
static void     gpm_info_init       (GpmInfo      *info);
static void     gpm_info_finalize   (GObject      *object);

#define GPM_INFO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO, GpmInfoPrivate))

#define GPM_INFO_HARDWARE_POLL		10	/* seconds */
#define GPM_INFO_DATA_RES_X		80	/* x resolution, greater burns the cpu */
#define GPM_INFO_MAX_POINTS		100	/* when we should simplify data */

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
gpm_info_data_point_add (GpmInfoGraphData *list_data, int x, int y)
{
	GpmDataPoint *new_point;
	int length = g_list_length (list_data->data);
	if (length > 2) {
		GList		*first = g_list_first (list_data->data);
		GpmDataPoint	*old = (GpmDataPoint *) first->data;
		if (old->y == y) {
			/* we are the same as we were before and not the first or
			   second point, just side the data time across */
			old->x = x;
		} else {
			/* we have to add a new data point */
			new_point = g_new (GpmDataPoint, 1);
			new_point->x = x;
			new_point->y = y;
			list_data->data = g_list_prepend (list_data->data, (gpointer) new_point);
		}
	} else {
		/* a new list requires a data point */
		new_point = g_new (GpmDataPoint, 1);
		new_point->x = x;
		new_point->y = y;
		list_data->data = g_list_prepend (list_data->data, (gpointer) new_point);
	}
	gpm_debug ("Drawing %i lines", length);
	if (length > GPM_INFO_MAX_POINTS) {
		/* we have too much data, simplify by remove every 3rd link */
		gpm_debug ("Too many points (%i)", length);
		GList *l;
		GList *temp;
		int count = 0;
		for (l=list_data->data; l != NULL; l=l->next) {
			new_point = (GpmDataPoint *) l->data;
			count++;
			if (count == 3) {
				temp = l->prev;
				list_data->data = g_list_delete_link (list_data->data, l);
				l = temp;
				count = 0;
			}
		}
	}
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
	if (info->priv->main_window) {
		gtk_widget_destroy (info->priv->main_window);
		info->priv->main_window = NULL;
	}
}

/** update this graph */
static void
gpm_info_graph_update (GpmInfoGraphData *graph)
{
	/* free existing data if exists */


	if (graph->data) {
		gpm_simple_graph_set_data (GPM_SIMPLE_GRAPH (graph->widget), graph->data);
	} else {
		gpm_debug ("no log data");
	}

	/* FIXME: There's got to be a better way than this */
	gtk_widget_hide (graph->widget);
	gtk_widget_show (graph->widget);
}

/** update the tree widget with new data */
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

/** create the tree widget */
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

//	g_signal_connect (info->priv->main_window, "delete_event",
//			  G_CALLBACK (gpm_info_close_cb), info);

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

/** callback to get the log data every minute */
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
	value_x = time (NULL) - (info->priv->start_time + GPM_INFO_HARDWARE_POLL);
	gpm_info_data_point_add (info->priv->percentage, value_x, battery_status.percentage_charge);
	gpm_info_data_point_add (info->priv->rate, value_x, battery_status.charge_rate);
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

	info->priv->rate->widget = NULL;
	info->priv->percentage->widget = NULL;
	info->priv->time->widget = NULL;

	info->priv->rate->data = NULL;
	info->priv->percentage->data = NULL;
	info->priv->time->data = NULL;

	/* record our start time */
	info->priv->start_time = time (NULL);

	/* set up the timer callback so we can log data */
	g_timeout_add (GPM_INFO_HARDWARE_POLL * 1000, gpm_info_log_do_poll, info);
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

	g_list_foreach (info->priv->rate->data, (GFunc) g_free, NULL);
	g_list_free (info->priv->rate->data);
	g_free (info->priv->rate);

	g_list_foreach (info->priv->percentage->data, (GFunc) g_free, NULL);
	g_list_free (info->priv->percentage->data);
	g_free (info->priv->percentage);

	g_list_foreach (info->priv->time->data, (GFunc) g_free, NULL);
	g_list_free (info->priv->time->data);
	g_free (info->priv->time);

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
