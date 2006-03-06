/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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
#include "gpm-info.h"
#include "gpm-debug.h"
#include "gpm-power.h"

static void     gpm_info_class_init (GpmInfoClass *klass);
static void     gpm_info_init       (GpmInfo      *info);
static void     gpm_info_finalize   (GObject      *object);

#define GPM_INFO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO, GpmInfoPrivate))

#define GPM_INFO_POLL		60

struct GpmInfoPrivate
{
	GpmPower	*power;

	GList		*log_percentage_charge;

	GtkWidget	*main_window;
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

/* only temporary, will use the custom widget when finished */
typedef struct
{
	int	x;
	int	y;
} GpmDataPoint;

static void
gpm_stat_add_data_point (GList *list, int x, int y)
{
	GpmDataPoint *data_point;
	data_point = g_new (GpmDataPoint, 1);
	data_point->x = x;
	data_point->y = y;
	list = g_list_append (list, (gpointer) data_point);
}

static void
gpm_stat_calculate_percentage (GList *source, GList *destination, int num_points)
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
			gpm_debug ("(%i) value = (%i, %i)", this_count, value_x, value_y);
			gpm_stat_add_data_point (destination, value_x, value_y);
			count = 0;
			average_add = 0;
			this_count++;
		} else {
			count++;
		}
	}
	if (a > num_points && num_points != this_count) {
		value_y = (float) average_add / (float) count;
		value_x = 100;
		gpm_debug ("    value = (%i, %i)", value_x, value_y);
		gpm_stat_add_data_point (destination, value_x, value_y);
	}
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

void
gpm_info_show_window (GpmInfo *info)
{
	GtkWidget    *widget;
	GladeXML     *glade_xml;

	if (info->priv->main_window) {
		gpm_debug ("already showing info");
		return;
	}

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

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_close_cb), info);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_info_help_cb), info);

	gtk_widget_show (info->priv->main_window);

	int a;
	GpmDataPoint *d_point;

	GList *values = NULL;

	if (info->priv->log_percentage_charge) {
		gpm_stat_calculate_percentage (info->priv->log_percentage_charge, values, 11);
	}

	/* print graph data */
	gpm_debug ("GRAPH DATA");
	for (a=0; a<g_list_length (values); a++) {
		d_point = (GpmDataPoint*) g_list_nth_data (values, a);
		gpm_debug ("data %i = (%i, %i)", a, d_point->x, d_point->y);
	}

	/* free elements */
	for (a=0; a<g_list_length (values); a++) {
		d_point = (GpmDataPoint*) g_list_nth_data (values, a);
		g_free (d_point);
	}
	g_list_free (values);
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
	gpm_debug ("Graph point %i", *point);
	info->priv->log_percentage_charge = g_list_append (info->priv->log_percentage_charge,
							   (gpointer) point);
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
	g_timeout_add (GPM_INFO_POLL * 1000, log_do_poll, info);
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
	int *data;
	int a;
	for (a=0; a<g_list_length (info->priv->log_percentage_charge); a++) {
		data = (int*) g_list_nth_data (info->priv->log_percentage_charge, a);
		g_free (data);
	}
	g_list_free (info->priv->log_percentage_charge);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmInfo *
gpm_info_new (void)
{
	GpmInfo *info;

	info = g_object_new (GPM_TYPE_INFO, NULL);

	return GPM_INFO (info);
}
