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

#include <glib.h>
#include <glib/gi18n.h>

#include <glade/glade.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <math.h>
#include <string.h>

#include "gpm-array.h"
#include "gpm-array-float.h"
#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-conf.h"
#include "gpm-statistics-core.h"
#include "egg-debug.h"
#include "gpm-stock-icons.h"
#include "gpm-info.h"
#include <libdbus-proxy.h>

static void     gpm_statistics_class_init (GpmStatisticsClass *klass);
static void     gpm_statistics_init       (GpmStatistics      *statistics);
static void     gpm_statistics_finalize   (GObject	    *object);

static void	gpm_statistics_refresh_data (GpmStatistics *statistics);

#define GPM_STATISTICS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_STATISTICS, GpmStatisticsPrivate))

#define ACTION_VOLTAGE				"voltage"
#define ACTION_CHARGE				"charge"
#define ACTION_POWER				"power"
#define ACTION_TIME				"time"
#define ACTION_PROFILE_CHARGE_TIME		"profile-charge-time"
#define ACTION_PROFILE_CHARGE_ACCURACY		"profile-charge-accuracy"
#define ACTION_PROFILE_DISCHARGE_TIME		"profile-discharge-time"
#define ACTION_PROFILE_DISCHARGE_ACCURACY	"profile-discharge-accuracy"
#define ACTION_CHARGE_TEXT			_("Charge history")
#define ACTION_POWER_TEXT			_("Power history")
#define ACTION_VOLTAGE_TEXT			_("Voltage history")
#define ACTION_TIME_TEXT			_("Estimated time history")
#define ACTION_PROFILE_CHARGE_TIME_TEXT		_("Charge time profile")
#define ACTION_PROFILE_DISCHARGE_TIME_TEXT	_("Discharge time profile")
#define ACTION_PROFILE_CHARGE_ACCURACY_TEXT	_("Charge time accuracy profile")
#define ACTION_PROFILE_DISCHARGE_ACCURACY_TEXT	_("Discharge time accuracy profile")

#define GPM_STATISTICS_POLL_INTERVAL	15000 /* ms */

struct GpmStatisticsPrivate
{
	GladeXML		*glade_xml;
	GtkWidget		*graph_widget;
	GpmConf			*conf;
	DbusProxy		*gproxy;
	GpmArray		*events;
	GpmArray		*data;
	const gchar		*graph_type;
	gchar			*axis_desc_x;
	gchar			*axis_desc_y;
};

enum {
	ACTION_HELP,
	ACTION_CLOSE,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmStatistics, gpm_statistics, G_TYPE_OBJECT)

/* The text that should appear in the action combo boxes */
#define ACTION_INTERACTIVE_TEXT		_("Ask me")
#define ACTION_SUSPEND_TEXT		_("Suspend")
#define ACTION_SHUTDOWN_TEXT		_("Shutdown")
#define ACTION_HIBERNATE_TEXT		_("Hibernate")
#define ACTION_BLANK_TEXT		_("Blank screen")
#define ACTION_NOTHING_TEXT		_("Do nothing")

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
	egg_debug ("emitting action-help");
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
	egg_debug ("emitting action-close");
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

	if (strcmp (key, GPM_CONF_LOWPOWER_AC) == 0) {
		gpm_conf_get_bool (statistics->priv->conf, GPM_CONF_LOWPOWER_AC, &enabled);
		egg_debug ("need to enable checkbox");
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

	g_return_val_if_fail (statistics != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_STATISTICS (statistics), FALSE);

	proxy = dbus_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	g_type_ptrarray = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_INVALID));

	ret = dbus_g_proxy_call (proxy, "GetEventLog", &error,
				 G_TYPE_INVALID,
				 g_type_ptrarray, &ptrarray,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("GetEventLog failed!");
		return FALSE;
	}

	egg_debug ("events size=%i", ptrarray->len);

	/* clear current events */
	gpm_array_clear (statistics->priv->events);

	for (i=0; i< ptrarray->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (ptrarray, i);
		gv = g_value_array_get_nth (gva, 0);
		x = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 1);
		event = g_value_get_int (gv);
		g_value_unset (gv);
		gpm_array_append (statistics->priv->events, x, event, 0);
		g_value_array_free (gva);
	}
	g_ptr_array_free (ptrarray, TRUE);

	return TRUE;
}

static void
gpm_statistics_refresh_events (GpmStatistics *statistics)
{
	gpm_statistics_get_events (statistics);
	gpm_graph_widget_events_add (GPM_GRAPH_WIDGET (statistics->priv->graph_widget),
				     statistics->priv->events);
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
	egg_debug ("Events enable %i", checked);

	/* save to gconf so we open next time with the correct setting */
	gpm_conf_set_bool (statistics->priv->conf, GPM_CONF_STATS_SHOW_EVENTS, checked);

	if (checked == FALSE) {
		/* remove the dots from the graph */
		gpm_graph_widget_enable_events (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), FALSE);
		/* disable legend  */
		gpm_graph_widget_enable_legend (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), FALSE);
		return;
	}

	/* refresh events automatically */
	gpm_statistics_refresh_events (statistics);

	/* only enable the dots if the checkbox is checked */
	gpm_graph_widget_enable_events (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), TRUE);

	/* enable legend  */
	gpm_graph_widget_enable_legend (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), TRUE);
}

/**
 * gpm_statistics_refresh_axis_labels:
 **/
static void
gpm_statistics_refresh_axis_labels (GpmStatistics *statistics)
{
	gboolean show;
	GtkWidget *widget1;
	GtkWidget *widget2;

	/* save to gconf so we open next time with the correct setting */
	gpm_conf_get_bool (statistics->priv->conf, GPM_CONF_STATS_SHOW_AXIS_LABELS, &show);

	widget1 = glade_xml_get_widget (statistics->priv->glade_xml, "label_x_axis");
	widget2 = glade_xml_get_widget (statistics->priv->glade_xml, "label_y_axis");

	if (show == FALSE) {
		gtk_widget_hide (widget1);
		gtk_widget_hide (widget2);
	} else {
		gtk_label_set_text (GTK_LABEL (widget1), statistics->priv->axis_desc_x);
		gtk_label_set_text (GTK_LABEL (widget2), statistics->priv->axis_desc_y);
		gtk_widget_show (widget1);
		gtk_widget_show (widget2);
	}
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

	proxy = dbus_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetDataTypes", &error,
			         G_TYPE_INVALID,
			         G_TYPE_STRV, &strlist,
			         G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("GetTypes failed!");
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

	g_return_val_if_fail (statistics != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_STATISTICS (statistics), FALSE);
	g_return_val_if_fail (type != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	g_type_ptrarray = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_INVALID));

	ret = dbus_g_proxy_call (proxy, "GetData", &error,
				 G_TYPE_STRING, type,
				 G_TYPE_INVALID,
				 g_type_ptrarray, &ptrarray,
				 G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("GetData failed!");
		return FALSE;
	}

	egg_debug ("size=%i", ptrarray->len);

	/* clear current events */
	gpm_array_clear (statistics->priv->data);

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
		gpm_array_append (statistics->priv->data, x, y, col);
		g_value_array_free (gva);
	}
	g_ptr_array_free (ptrarray, TRUE);

	return TRUE;
}

static gboolean
gpm_statistics_get_parameters_dbus (GpmStatistics *statistics,
			            const gchar   *type)
{
	GError *error = NULL;
	gboolean ret;
	GpmGraphWidgetAxisType axis_type_x;
	GpmGraphWidgetAxisType axis_type_y;
	gchar *axis_type_x_text;
	gchar *axis_type_y_text;
	DBusGProxy *proxy;
	GPtrArray *ptr_data_array = NULL;
	GPtrArray *ptr_event_array = NULL;
	GType g_type_ptr_data_array;
	GType g_type_ptr_event_array;

	g_return_val_if_fail (statistics != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_STATISTICS (statistics), FALSE);

	g_type_ptr_data_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_INT,
						G_TYPE_STRING,
						G_TYPE_INVALID));
	g_type_ptr_event_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_INT,
						G_TYPE_STRING,
						G_TYPE_INVALID));

	proxy = dbus_proxy_get_proxy (statistics->priv->gproxy);
	if (proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetParameters", &error,
			         G_TYPE_STRING, type,
			         G_TYPE_INVALID,
			         G_TYPE_STRING, &axis_type_x_text,
			         G_TYPE_STRING, &axis_type_y_text,
			         G_TYPE_STRING, &statistics->priv->axis_desc_x,
			         G_TYPE_STRING, &statistics->priv->axis_desc_y,
				 g_type_ptr_data_array, &ptr_data_array,
				 g_type_ptr_event_array, &ptr_event_array,
			         G_TYPE_INVALID);
	if (error) {
		egg_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		egg_warning ("GetAxisTypes failed!");
		return FALSE;
	}

	guint i;
	gint id;
	gint colour;
	gint shape;
	GValueArray *gva;
	GValue *gv;
	const gchar *desc;

	/* clear the data key */
	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (statistics->priv->graph_widget));

	for (i=0; i< ptr_data_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (ptr_data_array, i);
		gv = g_value_array_get_nth (gva, 0);
		id = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 1);
		desc = g_value_get_string (gv);
		/* add to the data key */
		gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), id, desc);
		g_value_unset (gv);
		g_value_array_free (gva);
	}
	g_ptr_array_free (ptr_data_array, TRUE);

	/* clear the events key */
	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (statistics->priv->graph_widget));

	/* process events key */
	for (i=0; i< ptr_event_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (ptr_event_array, i);
		gv = g_value_array_get_nth (gva, 0);
		id = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 1);
		colour = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 2);
		shape = g_value_get_int (gv);
		g_value_unset (gv);
		gv = g_value_array_get_nth (gva, 3);
		desc = g_value_get_string (gv);
		/* add to the data key */
		gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), id, colour, shape, desc);
		g_value_unset (gv);
		g_value_array_free (gva);
	}
	g_ptr_array_free (ptr_event_array, TRUE);

	egg_debug ("graph type '%s' mapped to x-axis '%s'", type, axis_type_x_text);
	egg_debug ("graph type '%s' mapped to y-axis '%s'", type, axis_type_y_text);

	/* convert the string types to enumerated values */
	axis_type_x = gpm_graph_widget_string_to_axis_type (axis_type_x_text);
	axis_type_y = gpm_graph_widget_string_to_axis_type (axis_type_y_text);

	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), axis_type_x);
	gpm_graph_widget_set_axis_type_y (GPM_GRAPH_WIDGET (statistics->priv->graph_widget), axis_type_y);

	return TRUE;
}

static void
gpm_statistics_refresh_data (GpmStatistics *statistics)
{
	gboolean smooth;

	/* only get the data for a valid type */
	if (statistics->priv->graph_type != NULL) {
		gpm_statistics_get_data_dbus (statistics, statistics->priv->graph_type);
	}

	gpm_conf_get_bool (statistics->priv->conf, GPM_CONF_STATS_SMOOTH_DATA, &smooth);
	if (smooth) {
		GArray *arrayfloat;
		GArray *kernel;
		GArray *result;
		egg_debug ("smoothing data, slooooooow....");

		arrayfloat = gpm_array_float_new (gpm_array_get_size (statistics->priv->data));
		gpm_array_float_from_array_y (arrayfloat, statistics->priv->data);
		kernel = gpm_array_float_compute_gaussian (35, 4.5);
		result = gpm_array_float_convolve (arrayfloat, kernel);
		gpm_array_float_to_array_y (result, statistics->priv->data);

		gpm_array_float_free (kernel);
		gpm_array_float_free (arrayfloat);
		gpm_array_float_free (result);
	}

	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (statistics->priv->graph_widget));
	gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (statistics->priv->graph_widget),
				   statistics->priv->data);

	gtk_widget_hide (GTK_WIDGET (statistics->priv->graph_widget));
	gtk_widget_show (GTK_WIDGET (statistics->priv->graph_widget));
}

static void
gpm_statistics_type_combo_changed_cb (GtkWidget      *widget,
				      GpmStatistics  *statistics)
{
	gchar *value;
	gchar *type = NULL;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (value == NULL) {
		egg_debug ("no graph types available");
		return;
	}
	if (strcmp (value, ACTION_CHARGE_TEXT) == 0) {
		type = ACTION_CHARGE;
	} else if (strcmp (value, ACTION_POWER_TEXT) == 0) {
		type = ACTION_POWER;
	} else if (strcmp (value, ACTION_TIME_TEXT) == 0) {
		type = ACTION_TIME;
	} else if (strcmp (value, ACTION_VOLTAGE_TEXT) == 0) {
		type = ACTION_VOLTAGE;
	} else if (strcmp (value, ACTION_PROFILE_CHARGE_TIME_TEXT) == 0) {
		type = ACTION_PROFILE_CHARGE_TIME;
	} else if (strcmp (value, ACTION_PROFILE_CHARGE_ACCURACY_TEXT) == 0) {
		type = ACTION_PROFILE_CHARGE_ACCURACY;
	} else if (strcmp (value, ACTION_PROFILE_DISCHARGE_TIME_TEXT) == 0) {
		type = ACTION_PROFILE_DISCHARGE_TIME;
	} else if (strcmp (value, ACTION_PROFILE_DISCHARGE_ACCURACY_TEXT) == 0) {
		type = ACTION_PROFILE_DISCHARGE_ACCURACY;
	} else {
		g_error ("value '%s' unknown", value);
	}
	g_free (value);

	/* find out what sort of grid axis we need */
	gpm_statistics_get_parameters_dbus (statistics, type);

	/* const, so no need to free */
	statistics->priv->graph_type = type;

	/* save in gconf so we choose the correct graph type on next startup */
	gpm_conf_set_string (statistics->priv->conf, GPM_CONF_STATS_GRAPH_TYPE, type);

	/* refresh data automatically */
	gpm_statistics_refresh_data (statistics);

	/* refresh the axis text */
	gpm_statistics_refresh_axis_labels (statistics);
}

static void
gpm_statistics_populate_graph_types (GpmStatistics *statistics,
				     GtkWidget     *widget)
{
	GList *list = NULL;
	GList *l;
	gchar *type;
	gchar *saved;
	const gchar *type_localized;
	gboolean ret;
	guint count, pos;

	ret = gpm_statistics_find_types (statistics, &list);
	if (!ret) {
		return;
	}

	gpm_conf_get_string (statistics->priv->conf, GPM_CONF_STATS_GRAPH_TYPE, &saved);
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
		} else if (strcmp (type, ACTION_PROFILE_CHARGE_TIME) == 0) {
			type_localized = ACTION_PROFILE_CHARGE_TIME_TEXT;
		} else if (strcmp (type, ACTION_PROFILE_CHARGE_ACCURACY) == 0) {
			type_localized = ACTION_PROFILE_CHARGE_ACCURACY_TEXT;
		} else if (strcmp (type, ACTION_PROFILE_DISCHARGE_TIME) == 0) {
			type_localized = ACTION_PROFILE_DISCHARGE_TIME_TEXT;
		} else if (strcmp (type, ACTION_PROFILE_DISCHARGE_ACCURACY) == 0) {
			type_localized = ACTION_PROFILE_DISCHARGE_ACCURACY_TEXT;
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

	egg_debug ("refreshing graph type '%s'", statistics->priv->graph_type);
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

	g_return_if_fail (GPM_IS_STATISTICS (statistics));

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

	statistics->priv->gproxy = dbus_proxy_new ();
	dbus_proxy_assign (statistics->priv->gproxy,
			  DBUS_PROXY_SESSION,
			  GPM_DBUS_SERVICE,
			  GPM_DBUS_PATH_STATS,
			  GPM_DBUS_INTERFACE_STATS);

	/* would happen if not using g-p-m or using an old version of g-p-m */
	if (dbus_proxy_is_connected (statistics->priv->gproxy) == FALSE) {
		egg_error (_("Could not connect to GNOME Power Manager."));
	}

	statistics->priv->graph_type = NULL;
	statistics->priv->events = gpm_array_new ();
	statistics->priv->data = gpm_array_new ();

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
	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_TIME);
	gpm_graph_widget_set_axis_type_y (GPM_GRAPH_WIDGET (widget), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);

	/* FIXME: There's got to be a better way than this */
	gtk_widget_hide (GTK_WIDGET (widget));
	gtk_widget_show (GTK_WIDGET (widget));

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "combobox_type");
	gpm_statistics_populate_graph_types (statistics, widget);

	widget = glade_xml_get_widget (statistics->priv->glade_xml, "checkbutton_events");
	gpm_conf_get_bool (statistics->priv->conf, GPM_CONF_STATS_SHOW_EVENTS, &checked);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_statistics_checkbox_events_cb), statistics);
	gpm_statistics_checkbox_events_cb (widget, statistics);

	/* refresh the axis */
	gpm_statistics_refresh_axis_labels (statistics);

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
