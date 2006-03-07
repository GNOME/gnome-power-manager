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

struct GpmInfoPrivate
{
	GpmPower	*power;

	GList		*log_percentage_charge;
	GList		*graph_percentage_charge;

	GtkWidget	*main_window;
	GtkWidget	*graph_percentage;
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

static void
gpm_info_data_point_add (GList **list, int x, int y)
{
	GpmSimpleDataPoint *data_point;
	data_point = g_new (GpmSimpleDataPoint, 1);
	data_point->x = x;
	data_point->y = y;
	*list = g_list_append (*list, (gpointer) data_point);
}

/* normalise 0..100 */
static void
gpm_stat_calculate_percentage (GList *source, GList **destination, int num_points)
{
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

static void
gpm_info_close_cb (GtkWidget	*widget,
		   GpmInfo	*info)
{
	gtk_widget_destroy (info->priv->main_window);
	info->priv->main_window = NULL;
}

static void
gpm_info_data_point_free (GList **list)
{
	if (list == NULL || *list == NULL) {
		return;
	}
	int a;
	GpmSimpleDataPoint *d_point;

	/* free elements */
	for (a=0; a<g_list_length (*list); a++) {
		d_point = (GpmSimpleDataPoint*) g_list_nth_data (*list, a);
		g_free (d_point);
	}
	g_list_free (*list);
	*list = NULL;
}

static void
gpm_info_data_point_print (GList *list)
{
	int a;
	GpmSimpleDataPoint *d_point;
	/* print graph data */
	gpm_debug ("GRAPH DATA");
	for (a=0; a<g_list_length (list); a++) {
		d_point = (GpmSimpleDataPoint*) g_list_nth_data (list, a);
		gpm_debug ("data %i = (%i, %i)", a, d_point->x, d_point->y);
	}
}

static void
gpm_info_update_graph (GpmInfo *info)
{
	GtkWidget *widget = info->priv->graph_percentage;
	int max_time;

	/* free existing data if exists */
	gpm_info_data_point_free (&(info->priv->graph_percentage_charge));

	if (info->priv->log_percentage_charge) {
		gpm_stat_calculate_percentage (info->priv->log_percentage_charge,
					       &(info->priv->graph_percentage_charge),
					       GPM_INFO_DATA_RES_X + 1);
		gpm_simple_graph_set_data (GPM_SIMPLE_GRAPH (widget), info->priv->graph_percentage_charge);
		max_time = (GPM_INFO_HARDWARE_POLL * g_list_length (info->priv->log_percentage_charge)) / 60;
		if (max_time == 0) {
			max_time = 10;
		}
		gpm_simple_graph_set_stop_x (GPM_SIMPLE_GRAPH (widget), max_time);
	} else {
		gpm_debug ("no log data");
	}

	gtk_widget_set_size_request (widget, 600, 300);

	gtk_widget_hide (widget);
	gtk_widget_show (widget);
}

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
	/* don't segfault on missing glade file */
	if (! glade_xml) {
		gpm_warning ("gpm-info.glade not found");
		return;
	}
	info->priv->main_window = glade_xml_get_widget (glade_xml, "window_info");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (info->priv->main_window);
	gtk_window_set_icon_name (GTK_WINDOW (info->priv->main_window), "gnome-dev-battery");

	g_signal_connect (info->priv->main_window, "delete_event",
			  G_CALLBACK (gpm_info_close_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_close_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_help_cb), info);

	info->priv->graph_percentage = glade_xml_get_widget (glade_xml, "graph_percentage");

	gpm_info_update_graph (info);
	gpm_info_data_point_print (info->priv->graph_percentage_charge);

	gtk_widget_show (info->priv->main_window);
}

/* log data every minute */
static gboolean
log_do_poll (gpointer data)
{
	GpmInfo *info = (GpmInfo*) data;
	int *point;

	GpmPowerBatteryStatus battery_status;
	gpm_power_get_battery_status (info->priv->power,
				      GPM_POWER_BATTERY_KIND_PRIMARY,
				      &battery_status);
	point = g_new (int, 1);
	*point = battery_status.percentage_charge;

#ifdef DO_TESTING
	static int auto_inc_val = 0;
	auto_inc_val++;
	if (auto_inc_val == 100) {
		auto_inc_val = 0;
	}
	*point += auto_inc_val;
#endif

	gpm_debug ("logging percentage_charge: %i", *point);
	info->priv->log_percentage_charge = g_list_append (info->priv->log_percentage_charge,
							   (gpointer) point);
	if (info->priv->main_window) {
		gpm_info_update_graph (info);
		gpm_info_data_point_print (info->priv->graph_percentage_charge);
	}
	return TRUE;
}

/* logging system needs access to the power stuff */
void
gpm_info_set_power (GpmInfo *info, GpmPower *power)
{
	info->priv->power = power;
}

static void
gpm_info_class_init (GpmInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gpm_info_finalize;
	g_type_class_add_private (klass, sizeof (GpmInfoPrivate));
}

static void
gpm_info_init (GpmInfo *info)
{
	info->priv = GPM_INFO_GET_PRIVATE (info);

	info->priv->main_window = NULL;
	/* set up the timer callback so we can log data every minute */
	info->priv->log_percentage_charge = NULL;
	info->priv->graph_percentage_charge = NULL;
	g_timeout_add (GPM_INFO_HARDWARE_POLL * 1000, log_do_poll, info);
}

static void
gpm_info_log_free (GList **list)
{
	if (list == NULL || *list == NULL) {
		return;
	}
	int *data;
	int a;
	for (a=0; a<g_list_length (*list); a++) {
		data = (int*) g_list_nth_data (*list, a);
		g_free (data);
	}
	g_list_free (*list);
	*list = NULL;

}

static void
gpm_info_finalize (GObject *object)
{
	GpmInfo *info;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_INFO (object));

	info = GPM_INFO (object);
	info->priv = GPM_INFO_GET_PRIVATE (info);

	/* free log_percentage_charge elements */
	gpm_info_log_free (&(info->priv->log_percentage_charge));
	gpm_info_data_point_free (&(info->priv->graph_percentage_charge));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmInfo *
gpm_info_new (void)
{
	GpmInfo *info;

	info = g_object_new (GPM_TYPE_INFO, NULL);

	return GPM_INFO (info);
}
