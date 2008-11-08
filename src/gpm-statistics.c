/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#include "egg-debug.h"
#include "egg-color.h"
#include "egg-string.h"
#include "egg-obj-list.h"
#include "egg-array-float.h"
#include "egg-unique.h"

#include "gpm-common.h"
#include "gpm-devicekit.h"
#include "gpm-graph-widget.h"
#include "dkp-client.h"
#include "dkp-history-obj.h"
#include "dkp-stats-obj.h"
#include "dkp-client-device.h"

static GladeXML *glade_xml = NULL;
static GtkListStore *list_store_info = NULL;
static GtkListStore *list_store_devices = NULL;
gchar *current_device = NULL;
static const gchar *history_type;
static const gchar *stats_type;
static guint history_time;
static GConfClient *gconf_client;
static EggArrayFloat *gaussian = NULL;

enum {
	GPM_INFO_COLUMN_TEXT,
	GPM_INFO_COLUMN_VALUE,
	GPM_INFO_COLUMN_LAST
};

enum {
	GPM_DEVICES_COLUMN_ICON,
	GPM_DEVICES_COLUMN_TEXT,
	GPM_DEVICES_COLUMN_ID,
	GPM_DEVICES_COLUMN_LAST
};

#define GPM_HISTORY_RATE_TEXT			_("Rate")
#define GPM_HISTORY_CHARGE_TEXT			_("Charge")
#define GPM_HISTORY_TIME_FULL_TEXT		_("Time to full")
#define GPM_HISTORY_TIME_EMPTY_TEXT		_("Time to empty")

#define GPM_HISTORY_RATE_VALUE			"rate"
#define GPM_HISTORY_CHARGE_VALUE		"charge"
#define GPM_HISTORY_TIME_FULL_VALUE		"time-full"
#define GPM_HISTORY_TIME_EMPTY_VALUE		"time-empty"

#define GPM_HISTORY_MINUTE_TEXT			_("10 minutes")
#define GPM_HISTORY_HOUR_TEXT			_("2 hours")
#define GPM_HISTORY_DAY_TEXT			_("1 day")

#define GPM_HISTORY_MINUTE_VALUE		10*60
#define GPM_HISTORY_HOUR_VALUE			2*60*60
#define GPM_HISTORY_DAY_VALUE			24*60*60

#define GPM_STATS_CHARGE_DATA_TEXT		_("Charge profile")
#define GPM_STATS_CHARGE_ACCURACY_TEXT		_("Charge accuracy")
#define GPM_STATS_DISCHARGE_DATA_TEXT		_("Discharge profile")
#define GPM_STATS_DISCHARGE_ACCURACY_TEXT	_("Discharge accuracy")

#define GPM_STATS_CHARGE_DATA_VALUE		"charge-data"
#define GPM_STATS_CHARGE_ACCURACY_VALUE		"charge-accuracy"
#define GPM_STATS_DISCHARGE_DATA_VALUE		"discharge-data"
#define GPM_STATS_DISCHARGE_ACCURACY_VALUE	"discharge-accuracy"

/**
 * gpm_button_help_cb:
 **/
static void
gpm_button_help_cb (GtkWidget *widget, gboolean data)
{
	//gpm_gnome_help ("update-log");
}

/**
 * gpm_add_info_columns:
 **/
static void
gpm_add_info_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Attribute"), renderer,
							   "markup", GPM_INFO_COLUMN_TEXT, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GPM_INFO_COLUMN_TEXT);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Value"), renderer,
							   "markup", GPM_INFO_COLUMN_VALUE, NULL);
	gtk_tree_view_append_column (treeview, column);
}

/**
 * gpm_add_devices_columns:
 **/
static void
gpm_add_devices_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Image"), renderer,
							   "icon-name", GPM_DEVICES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Description"), renderer,
							   "markup", GPM_DEVICES_COLUMN_TEXT, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GPM_INFO_COLUMN_TEXT);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * gpm_add_info_data:
 **/
static void
gpm_add_info_data (const gchar *attr, const gchar *text)
{
	GtkTreeIter iter;
	gtk_list_store_append (list_store_info, &iter);
	gtk_list_store_set (list_store_info, &iter,
			    GPM_INFO_COLUMN_TEXT, attr,
			    GPM_INFO_COLUMN_VALUE, text, -1);
}

/**
 * gpm_update_smooth_data:
 **/
static void
gpm_update_smooth_data (EggObjList *list)
{
	guint i;
	GpmPointObj *point;
	EggArrayFloat *raw;
	EggArrayFloat *convolved;

	/* convert the y data to a EggArrayFloat array */
	raw = egg_array_float_new (list->len);
	convolved = egg_array_float_new (list->len);
	for (i=0; i<list->len; i++) {
		point = (GpmPointObj *) egg_obj_list_index (list, i);
		egg_array_float_set (raw, i, point->y);
	}

	/* convolve with gaussian */
	convolved = egg_array_float_convolve (raw, gaussian);

	/* push the smoothed data back into y data */
	for (i=0; i<list->len; i++) {
		point = (GpmPointObj *) egg_obj_list_index (list, i);
		point->y = egg_array_float_get (convolved, i);
	}

	/* free data */
	egg_array_float_free (raw);
	egg_array_float_free (convolved);
}

/**
 * gpm_time_to_text:
 **/
static gchar *
gpm_time_to_text (gint seconds)
{
	gfloat value = seconds;

	if (value < 0)
		return g_strdup ("unknown");
	if (value < 60)
		return g_strdup_printf ("%.0f seconds", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f minutes", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f hours", value);
	value /= 24.0;
	return g_strdup_printf ("%.1f days", value);
}

/**
 * gpm_bool_to_text:
 **/
static const gchar *
gpm_bool_to_text (gboolean ret)
{
	return ret ? _("Yes") : _("No");
}

/**
 * gpm_update_info_page_details:
 **/
static void
gpm_update_info_page_details (const DkpClientDevice *device)
{
	const DkpObject *obj;
	struct tm *time_tm;
	time_t t;
	gchar time_buf[256];
	gchar *text;

	gtk_list_store_clear (list_store_info);

	obj = dkp_client_device_get_object (device);

	/* get a human readable time */
	t = (time_t) obj->update_time;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	gpm_add_info_data (_("Device"), dkp_client_device_get_object_path (device));
	gpm_add_info_data (_("Type"), gpm_device_type_to_localised_text (obj->type, 1));
	if (!egg_strzero (obj->vendor))
		gpm_add_info_data (_("Vendor"), obj->vendor);
	if (!egg_strzero (obj->model))
		gpm_add_info_data (_("Model"), obj->model);
	if (!egg_strzero (obj->serial))
		gpm_add_info_data (_("Serial number"), obj->serial);
	gpm_add_info_data (_("Supply"), gpm_bool_to_text (obj->power_supply));

	text = g_strdup_printf ("%d seconds", (int) (time (NULL) - obj->update_time));
	gpm_add_info_data (_("Refreshed"), text);
	g_free (text);

	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD ||
	    obj->type == DKP_DEVICE_TYPE_UPS)
		gpm_add_info_data (_("Present"), gpm_bool_to_text (obj->is_present));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD)
		gpm_add_info_data (_("Rechargeable"), gpm_bool_to_text (obj->is_rechargeable));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD)
		gpm_add_info_data (_("State"), dkp_device_state_to_text (obj->state));
	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		text = g_strdup_printf ("%.1f Wh", obj->energy);
		gpm_add_info_data (_("Energy"), text);
		g_free (text);
		text = g_strdup_printf ("%.1f Wh", obj->energy_empty);
		gpm_add_info_data (_("Energy when empty"), text);
		g_free (text);
		text = g_strdup_printf ("%.1f Wh", obj->energy_full);
		gpm_add_info_data (_("Energy when full"), text);
		g_free (text);
		text = g_strdup_printf ("%.1f Wh", obj->energy_full_design);
		gpm_add_info_data (_("Energy (design)"), text);
		g_free (text);
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MONITOR) {
		text = g_strdup_printf ("%.1f W", obj->energy_rate);
		gpm_add_info_data (_("Rate"), text);
		g_free (text);
	}
	if (obj->type == DKP_DEVICE_TYPE_UPS ||
	    obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MONITOR) {
		text = g_strdup_printf ("%.1f V", obj->voltage);
		gpm_add_info_data (_("Voltage"), text);
		g_free (text);
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_UPS) {
		if (obj->time_to_full >= 0) {
			text = gpm_time_to_text (obj->time_to_full);
			gpm_add_info_data (_("Time to full"), text);
			g_free (text);
		}
		if (obj->time_to_empty >= 0) {
			text = gpm_time_to_text (obj->time_to_empty);
			gpm_add_info_data (_("Time to empty"), text);
			g_free (text);
		}
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY ||
	    obj->type == DKP_DEVICE_TYPE_MOUSE ||
	    obj->type == DKP_DEVICE_TYPE_KEYBOARD ||
	    obj->type == DKP_DEVICE_TYPE_UPS) {
		text = g_strdup_printf ("%.1f%%", obj->percentage);
		gpm_add_info_data (_("Percentage"), text);
		g_free (text);
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY) {
		text = g_strdup_printf ("%.1f%%", obj->capacity);
		gpm_add_info_data (_("Capacity"), text);
		g_free (text);
	}
	if (obj->type == DKP_DEVICE_TYPE_BATTERY)
		gpm_add_info_data (_("Technology"), gpm_device_technology_to_localised_text (obj->technology));
	if (obj->type == DKP_DEVICE_TYPE_LINE_POWER)
		gpm_add_info_data (_("Online"), gpm_bool_to_text (obj->online));
}

/**
 * gpm_update_info_page_history:
 **/
static void
gpm_update_info_page_history (const DkpClientDevice *device)
{
	EggObjList *array;
	guint i;
	DkpHistoryObj *hobj;
	GtkWidget *widget;
	gboolean checked;
	GpmPointObj *point;
	EggObjList *new;
	gint32 offset = 0;
	GTimeVal timeval;

	new = egg_obj_list_new ();
	egg_obj_list_set_copy (new, (EggObjListCopyFunc) dkp_point_obj_copy);
	egg_obj_list_set_free (new, (EggObjListFreeFunc) dkp_point_obj_free);

	widget = glade_xml_get_widget (glade_xml, "custom_graph_history");
	gpm_graph_widget_set_type_x (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_TIME);
	if (strcmp (history_type, GPM_HISTORY_CHARGE_VALUE) == 0)
		gpm_graph_widget_set_type_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	else if (strcmp (history_type, GPM_HISTORY_RATE_VALUE) == 0)
		gpm_graph_widget_set_type_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_POWER);
	else
		gpm_graph_widget_set_type_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_TIME);

	array = dkp_client_device_get_history (device, history_type, history_time);
	if (array == NULL) {
		gtk_widget_hide (widget);
		goto out;
	}
	gtk_widget_show (widget);

	g_get_current_time (&timeval);
	offset = timeval.tv_sec;

	for (i=0; i<array->len; i++) {
		hobj = (DkpHistoryObj *) egg_obj_list_index (array, i);

		/* abandon this point */
		if (hobj->state == DKP_DEVICE_STATE_UNKNOWN)
			continue;

		point = dkp_point_obj_new ();
		point->x = (gint32) hobj->time - offset;
		point->y = hobj->value;
		if (hobj->state == DKP_DEVICE_STATE_CHARGING)
			point->color = egg_color_from_rgb (255, 0, 0);
		else if (hobj->state == DKP_DEVICE_STATE_DISCHARGING)
			point->color = egg_color_from_rgb (0, 0, 255);
		else {
			if (strcmp (history_type, GPM_HISTORY_RATE_VALUE) == 0)
				point->color = egg_color_from_rgb (255, 255, 255);
			else
				point->color = egg_color_from_rgb (0, 255, 0);
		}
		egg_obj_list_add (new, point);
		dkp_point_obj_free (point);
	}

	widget = glade_xml_get_widget (glade_xml, "checkbutton_smooth_history");
	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	/* smooth */
	if (checked)
		gpm_update_smooth_data (new);

	widget = glade_xml_get_widget (glade_xml, "custom_graph_history");
	gpm_graph_widget_data_assign (GPM_GRAPH_WIDGET (widget), new);

	g_object_unref (array);
	g_object_unref (new);
out:
	return;
}

/**
 * gpm_update_info_page_stats:
 **/
static void
gpm_update_info_page_stats (const DkpClientDevice *device)
{
	EggObjList *array;
	guint i;
	DkpStatsObj *sobj;
	GtkWidget *widget;
	gboolean checked;
	GpmPointObj *point;
	EggObjList *new;
	gboolean use_data = FALSE;
	const gchar *type = NULL;

	new = egg_obj_list_new ();
	egg_obj_list_set_copy (new, (EggObjListCopyFunc) dkp_point_obj_copy);
	egg_obj_list_set_free (new, (EggObjListFreeFunc) dkp_point_obj_free);

//	egg_debug ("history_type=%s", history_type);

	widget = glade_xml_get_widget (glade_xml, "custom_graph_stats");
	if (strcmp (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0) {
		type = "charging";
		use_data = TRUE;
	} else if (strcmp (stats_type, GPM_STATS_DISCHARGE_DATA_VALUE) == 0) {
		type = "discharging";
		use_data = TRUE;
	} else if (strcmp (stats_type, GPM_STATS_CHARGE_ACCURACY_VALUE) == 0) {
		type = "charging";
		use_data = FALSE;
	} else if (strcmp (stats_type, GPM_STATS_DISCHARGE_ACCURACY_VALUE) == 0) {
		type = "discharging";
		use_data = FALSE;
	} else {
		g_assert_not_reached ();
	}
	gpm_graph_widget_set_type_x (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);

	if (use_data)
		gpm_graph_widget_set_type_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_FACTOR);
	else
		gpm_graph_widget_set_type_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);

	array = dkp_client_device_get_statistics (device, type);
	if (array == NULL) {
		gtk_widget_hide (widget);
		goto out;
	}

	for (i=0; i<array->len; i++) {
		sobj = (DkpStatsObj *) egg_obj_list_index (array, i);
		point = dkp_point_obj_new ();
		point->x = i;
		if (use_data)
			point->y = sobj->value;
		else
			point->y = sobj->accuracy;
		point->color = egg_color_from_rgb (255, 0, 0);
		egg_obj_list_add (new, point);
		dkp_point_obj_free (point);
	}

	/* render */
	widget = glade_xml_get_widget (glade_xml, "checkbutton_smooth_stats");
	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	/* smooth */
	if (checked)
		gpm_update_smooth_data (new);

	widget = glade_xml_get_widget (glade_xml, "custom_graph_stats");
	gpm_graph_widget_data_assign (GPM_GRAPH_WIDGET (widget), new);
	gtk_widget_show (widget);

	g_object_unref (array);
	g_object_unref (new);
out:
	return;
}

/**
 * gpm_update_info_data_page:
 **/
static void
gpm_update_info_data_page (const DkpClientDevice *device, gint page)
{
	if (page == 0)
		gpm_update_info_page_details (device);
	else if (page == 1)
		gpm_update_info_page_history (device);
	else if (page == 2)
		gpm_update_info_page_stats (device);
}

/**
 * gpm_update_info_data:
 **/
static void
gpm_update_info_data (const DkpClientDevice *device)
{
	gint page;
	GtkWidget *widget;
	GtkWidget *page_widget;
	const DkpObject	*obj;

	widget = glade_xml_get_widget (glade_xml, "notebook1");
	obj = dkp_client_device_get_object (device);

	/* hide history if no support */
	page_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK(widget), 1);
	if (obj->has_history)
		gtk_widget_show (page_widget);
	else
		gtk_widget_hide (page_widget);

	/* hide statistics if no support */
	page_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK(widget), 2);
	if (obj->has_statistics)
		gtk_widget_show (page_widget);
	else
		gtk_widget_hide (page_widget);

	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (widget));
	gpm_update_info_data_page (device, page);

	return;
}

/**
 * gpm_notebook_changed_cb:
 **/
static void
gpm_notebook_changed_cb (GtkNotebook *notebook, GtkNotebookPage *page, gint page_num, gpointer user_data)
{
	DkpClientDevice *device;

	/* save page in gconf */
	gconf_client_set_int (gconf_client, GPM_CONF_INFO_PAGE_NUMBER, page_num, NULL);

	if (current_device == NULL)
		return;

	device = dkp_client_device_new ();
	dkp_client_device_set_object_path (device, current_device);
	gpm_update_info_data_page (device, page_num);
	gpm_update_info_data (device);
	g_object_unref (device);
}

/**
 * gpm_button_refresh_cb:
 **/
static void
gpm_button_refresh_cb (GtkWidget *widget, gpointer data)
{
	DkpClientDevice *device;
	device = dkp_client_device_new ();
	dkp_client_device_set_object_path (device, current_device);
	dkp_client_device_refresh (device);
	gpm_update_info_data (device);
	g_object_unref (device);
}

/**
 * gpm_button_update_ui:
 **/
static void
gpm_button_update_ui (void)
{
	DkpClientDevice *device;
	device = dkp_client_device_new ();
	dkp_client_device_set_object_path (device, current_device);
	gpm_update_info_data (device);
	g_object_unref (device);
}

/**
 * gpm_devices_treeview_clicked_cb:
 **/
static void
gpm_devices_treeview_clicked_cb (GtkTreeSelection *selection, gboolean data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* This will only work in single or browse selection mode! */
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_free (current_device);
		gtk_tree_model_get (model, &iter, GPM_DEVICES_COLUMN_ID, &current_device, -1);

		/* save device in gconf */
		gconf_client_set_string (gconf_client, GPM_CONF_INFO_LAST_DEVICE, current_device, NULL);

		/* show transaction_id */
		egg_debug ("selected row is: %s", current_device);

		DkpClientDevice *device;
		device = dkp_client_device_new ();
		dkp_client_device_set_object_path (device, current_device);
		gpm_update_info_data (device);
		g_object_unref (device);

	} else {
		egg_debug ("no row selected");
	}
}

/**
 * gpm_info_create_custom_widget:
 **/
static GtkWidget *
gpm_info_create_custom_widget (GladeXML *xml, gchar *func_name, gchar *name,
				        gchar *string1, gchar *string2,
				        gint int1, gint int2, gpointer user_data)
{
	GtkWidget *widget = NULL;
	if (strcmp ("gpm_graph_widget_new", func_name) == 0) {
		widget = gpm_graph_widget_new ();
		return widget;
	}
	egg_warning ("name unknown=%s", name);
	return NULL;
}

/**
 * gpm_gnome_activated_cb
 **/
static void
gpm_gnome_activated_cb (EggUnique *egg_unique, gpointer data)
{
	GtkWidget *widget;
	widget = glade_xml_get_widget (glade_xml, "window_dkp");
	gtk_window_present (GTK_WINDOW (widget));
}

/**
 * gpm_add_device:
 **/
static void
gpm_add_device (const DkpClientDevice *device)
{
	const gchar *id;
	GtkTreeIter iter;
	const DkpObject *obj;
	const gchar *text;
	const gchar *icon;

	obj = dkp_client_device_get_object (device);
	id = dkp_client_device_get_object_path (device);
	text = gpm_device_type_to_localised_text (obj->type, 1);
	icon = gpm_device_type_to_icon (obj->type);

	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    GPM_DEVICES_COLUMN_ID, id,
			    GPM_DEVICES_COLUMN_TEXT, text,
			    GPM_DEVICES_COLUMN_ICON, icon, -1);
}

/**
 * gpm_tool_device_added_cb:
 **/
static void
gpm_tool_device_added_cb (DkpClient *client, const DkpClientDevice *device, gpointer user_data)
{
	const gchar *object_path;
	object_path = dkp_client_device_get_object_path (device);
	egg_debug ("added:     %s", object_path);
	gpm_add_device (device);
}

/**
 * gpm_tool_device_changed_cb:
 **/
static void
gpm_tool_device_changed_cb (DkpClient *client, const DkpClientDevice *device, gpointer user_data)
{
	const gchar *object_path;
	object_path = dkp_client_device_get_object_path (device);
	if (object_path == NULL || current_device == NULL)
		return;
	egg_debug ("changed:   %s", object_path);
	if (strcmp (current_device, object_path) == 0)
		gpm_update_info_data (device);
}

/**
 * gpm_tool_device_removed_cb:
 **/
static void
gpm_tool_device_removed_cb (DkpClient *client, const DkpClientDevice *device, gpointer user_data)
{
	const gchar *object_path;
	GtkTreeIter iter;
	gchar *id = NULL;
	gboolean ret;

	object_path = dkp_client_device_get_object_path (device);
	egg_debug ("removed:   %s", object_path);
	if (strcmp (current_device, object_path) == 0) {
		gtk_list_store_clear (list_store_info);
	}

	/* search the list and remove the object path entry */
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store_devices), &iter);
	do {
		gtk_tree_model_get (GTK_TREE_MODEL (list_store_devices), &iter, GPM_DEVICES_COLUMN_ID, &id, -1);
		if (strcmp (id, object_path) == 0) {
			gtk_list_store_remove (list_store_devices, &iter);
			break;
		}
		g_free (id);
		ret = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store_devices), &iter);
	} while (ret);
}

/**
 * gpm_history_type_combo_changed_cb:
 **/
static void
gpm_history_type_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gchar *value;
	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (strcmp (value, GPM_HISTORY_RATE_TEXT) == 0)
		history_type = GPM_HISTORY_RATE_VALUE;
	else if (strcmp (value, GPM_HISTORY_CHARGE_TEXT) == 0)
		history_type = GPM_HISTORY_CHARGE_VALUE;
	else if (strcmp (value, GPM_HISTORY_TIME_FULL_TEXT) == 0)
		history_type = GPM_HISTORY_TIME_FULL_VALUE;
	else if (strcmp (value, GPM_HISTORY_TIME_EMPTY_TEXT) == 0)
		history_type = GPM_HISTORY_TIME_EMPTY_VALUE;
	else
		g_assert (FALSE);
	gpm_button_update_ui ();
	g_free (value);

	/* save to gconf */
	gconf_client_set_string (gconf_client, GPM_CONF_INFO_HISTORY_TYPE, history_type, NULL);
}

/**
 * gpm_stats_type_combo_changed_cb:
 **/
static void
gpm_stats_type_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gchar *value;
	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (strcmp (value, GPM_STATS_CHARGE_DATA_TEXT) == 0)
		stats_type = GPM_STATS_CHARGE_DATA_VALUE;
	else if (strcmp (value, GPM_STATS_CHARGE_ACCURACY_TEXT) == 0)
		stats_type = GPM_STATS_CHARGE_ACCURACY_VALUE;
	else if (strcmp (value, GPM_STATS_DISCHARGE_DATA_TEXT) == 0)
		stats_type = GPM_STATS_DISCHARGE_DATA_VALUE;
	else if (strcmp (value, GPM_STATS_DISCHARGE_ACCURACY_TEXT) == 0)
		stats_type = GPM_STATS_DISCHARGE_ACCURACY_VALUE;
	else
		g_assert (FALSE);
	gpm_button_update_ui ();
	g_free (value);

	/* save to gconf */
	gconf_client_set_string (gconf_client, GPM_CONF_INFO_STATS_TYPE, stats_type, NULL);
}

/**
 * gpm_update_range_combo_changed:
 **/
static void
gpm_update_range_combo_changed (GtkWidget *widget, gpointer data)
{
	gchar *value;
	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (strcmp (value, GPM_HISTORY_MINUTE_TEXT) == 0)
		history_time = GPM_HISTORY_MINUTE_VALUE;
	else if (strcmp (value, GPM_HISTORY_HOUR_TEXT) == 0)
		history_time = GPM_HISTORY_HOUR_VALUE;
	else if (strcmp (value, GPM_HISTORY_DAY_TEXT) == 0)
		history_time = GPM_HISTORY_DAY_VALUE;
	else
		g_assert (FALSE);

	/* save to gconf */
	gconf_client_set_int (gconf_client, GPM_CONF_INFO_HISTORY_TIME, history_time, NULL);

	gpm_button_update_ui ();
	g_free (value);
}

/**
 * gpm_smooth_checkbox_history_cb:
 * @widget: The GtkWidget object
 **/
static void
gpm_smooth_checkbox_history_cb (GtkWidget *widget, gpointer data)
{
	gboolean checked;
	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gconf_client_set_bool (gconf_client, GPM_CONF_INFO_HISTORY_GRAPH_SMOOTH, checked, NULL);
	gpm_button_update_ui ();
}

/**
 * gpm_smooth_checkbox_stats_cb:
 * @widget: The GtkWidget object
 **/
static void
gpm_smooth_checkbox_stats_cb (GtkWidget *widget, gpointer data)
{
	gboolean checked;
	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gconf_client_set_bool (gconf_client, GPM_CONF_INFO_STATS_GRAPH_SMOOTH, checked, NULL);
	gpm_button_update_ui ();
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean verbose = FALSE;
	GOptionContext *context;
	GtkWidget *widget;
	GtkTreeSelection *selection;
	EggUnique *egg_unique;
	gboolean ret;
	DkpClient *client;
	GPtrArray *devices;
	DkpClientDevice *device;
	guint i;
	gint page;
	const gchar *object_path;
	gboolean checked;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (! g_thread_supported ()) {
		g_thread_init (NULL);
	}
	dbus_g_thread_init ();
	g_type_init ();

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Software Log Viewer"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	egg_debug_init (verbose);
	gtk_init (&argc, &argv);

	/* are we already activated? */
	egg_unique = egg_unique_new ();
	ret = egg_unique_assign (egg_unique, "org.freedesktop.DeviceKit.Gnome");
	if (!ret)
		goto unique_out;
	g_signal_connect (egg_unique, "activated",
			  G_CALLBACK (gpm_gnome_activated_cb), NULL);

	/* get data from gconf */
	gconf_client = gconf_client_get_default ();

	/* create the gaussian dataset */
	gaussian = egg_array_float_compute_gaussian (35, 4.5);

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           GPM_DATA G_DIR_SEPARATOR_S "icons");

	/* use custom widgets */
	glade_set_custom_handler (gpm_info_create_custom_widget, NULL);

	glade_xml = glade_xml_new (GPM_DATA "/gpm-statistics.glade", NULL, NULL);
	widget = glade_xml_get_widget (glade_xml, "window_dkp");
	gtk_window_set_icon_name (GTK_WINDOW (widget), "gtk-help");
	gtk_widget_set_size_request (widget, 800, 500);

	/* Get the main window quit */
	g_signal_connect_swapped (widget, "delete_event", G_CALLBACK (gtk_main_quit), NULL);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect_swapped (widget, "clicked", G_CALLBACK (gtk_main_quit), NULL);
	gtk_widget_grab_default (widget);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_button_help_cb), NULL);

	widget = glade_xml_get_widget (glade_xml, "button_refresh");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_button_refresh_cb), NULL);

	widget = glade_xml_get_widget (glade_xml, "checkbutton_smooth_history");
	checked = gconf_client_get_bool (gconf_client, GPM_CONF_INFO_HISTORY_GRAPH_SMOOTH, NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_smooth_checkbox_history_cb), NULL);

	widget = glade_xml_get_widget (glade_xml, "checkbutton_smooth_stats");
	checked = gconf_client_get_bool (gconf_client, GPM_CONF_INFO_STATS_GRAPH_SMOOTH, NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_smooth_checkbox_stats_cb), NULL);

	widget = glade_xml_get_widget (glade_xml, "notebook1");
	page = gconf_client_get_int (gconf_client, GPM_CONF_INFO_PAGE_NUMBER, NULL);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), page);
	g_signal_connect (widget, "switch-page",
			  G_CALLBACK (gpm_notebook_changed_cb), NULL);

	/* create list stores */
	list_store_info = gtk_list_store_new (GPM_INFO_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING);
	list_store_devices = gtk_list_store_new (GPM_DEVICES_COLUMN_LAST, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING);

	/* create transaction_id tree view */
	widget = glade_xml_get_widget (glade_xml, "treeview_info");
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_info));

	/* add columns to the tree view */
	gpm_add_info_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget)); /* show */

	/* create transaction_id tree view */
	widget = glade_xml_get_widget (glade_xml, "treeview_devices");
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (gpm_devices_treeview_clicked_cb), NULL);

	/* add columns to the tree view */
	gpm_add_devices_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget)); /* show */

	history_type = gconf_client_get_string (gconf_client, GPM_CONF_INFO_HISTORY_TYPE, NULL);
	history_time = gconf_client_get_int (gconf_client, GPM_CONF_INFO_HISTORY_TIME, NULL);
	if (history_type == NULL)
		history_type = GPM_HISTORY_CHARGE_VALUE;
	if (history_time == 0)
		history_time = GPM_HISTORY_HOUR_VALUE;

	stats_type = gconf_client_get_string (gconf_client, GPM_CONF_INFO_STATS_TYPE, NULL);
	if (stats_type == NULL)
		stats_type = GPM_STATS_CHARGE_DATA_VALUE;

	widget = glade_xml_get_widget (glade_xml, "combobox_history_type");
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_HISTORY_RATE_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_HISTORY_CHARGE_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_HISTORY_TIME_FULL_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_HISTORY_TIME_EMPTY_TEXT);
	if (strcmp (history_type, GPM_HISTORY_RATE_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_history_type_combo_changed_cb), NULL);

	widget = glade_xml_get_widget (glade_xml, "combobox_stats_type");
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_STATS_CHARGE_DATA_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_STATS_CHARGE_ACCURACY_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_STATS_DISCHARGE_DATA_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_STATS_DISCHARGE_ACCURACY_TEXT);
	if (strcmp (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	else if (strcmp (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
	else if (strcmp (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
	else if (strcmp (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 3);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_stats_type_combo_changed_cb), NULL);

	widget = glade_xml_get_widget (glade_xml, "combobox_history_time");
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_HISTORY_MINUTE_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_HISTORY_HOUR_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), GPM_HISTORY_DAY_TEXT);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
	if (history_time == GPM_HISTORY_MINUTE_VALUE)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	else if (history_time == GPM_HISTORY_HOUR_VALUE)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_update_range_combo_changed), NULL);

	widget = glade_xml_get_widget (glade_xml, "custom_graph_history");
	gtk_widget_set_size_request (widget, 700, 400);
	gtk_widget_show (widget);
	widget = glade_xml_get_widget (glade_xml, "custom_graph_stats");
	gtk_widget_set_size_request (widget, 700, 400);
	gtk_widget_show (widget);

	client = dkp_client_new ();
	g_signal_connect (client, "added", G_CALLBACK (gpm_tool_device_added_cb), NULL);
	g_signal_connect (client, "removed", G_CALLBACK (gpm_tool_device_removed_cb), NULL);
	g_signal_connect (client, "changed", G_CALLBACK (gpm_tool_device_changed_cb), NULL);

	/* coldplug */
	devices = dkp_client_enumerate_devices (client, NULL);
	if (devices == NULL)
		goto out;
	for (i=0; i < devices->len; i++) {
		object_path = (const gchar *) g_ptr_array_index (devices, i);
		egg_debug ("Device: %s", object_path);
		device = dkp_client_device_new ();
		dkp_client_device_set_object_path (device, object_path);
		gpm_add_device (device);
		if (i == 0) {
			gpm_update_info_data (device);
			current_device = g_strdup (object_path);
		}
		g_object_unref (device);
	}

	gchar *last_device;
	last_device = gconf_client_get_string (gconf_client, GPM_CONF_INFO_LAST_DEVICE, NULL);

	/* set the correct focus on the last device */
	for (i=0; i < devices->len; i++) {
		object_path = (const gchar *) g_ptr_array_index (devices, i);
		if (last_device == NULL || object_path == NULL)
			break;
		if (strcmp (last_device, object_path) == 0) {
			GtkTreePath *path;
			gchar *path_str;
			path_str = g_strdup_printf ("%i", i);
			path = gtk_tree_path_new_from_string (path_str);
			widget = glade_xml_get_widget (glade_xml, "treeview_devices");
			gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (widget), path, NULL, NULL, FALSE);
			g_free (path_str);
			gtk_tree_path_free (path);
		}
	}

	g_ptr_array_foreach (devices, (GFunc) g_free, NULL);
	g_ptr_array_free (devices, TRUE);

	widget = glade_xml_get_widget (glade_xml, "window_dkp");
	gtk_widget_show (widget);

	gtk_main ();

out:
	egg_array_float_free (gaussian);
	g_object_unref (gconf_client);
	g_object_unref (client);
	g_object_unref (glade_xml);
	g_object_unref (list_store_info);
unique_out:
	g_object_unref (egg_unique);
	return 0;
}
