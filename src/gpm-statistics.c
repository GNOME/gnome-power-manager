/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2011 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libupower-glib/upower.h>

#include "gpm-array-float.h"
#include "gpm-rotated-widget.h"
#include "egg-graph-widget.h"

#define GPM_SETTINGS_SCHEMA				"org.gnome.power-manager"
#define GPM_SETTINGS_INFO_HISTORY_TIME			"info-history-time"
#define GPM_SETTINGS_INFO_HISTORY_TYPE			"info-history-type"
#define GPM_SETTINGS_INFO_HISTORY_GRAPH_SMOOTH		"info-history-graph-smooth"
#define GPM_SETTINGS_INFO_HISTORY_GRAPH_POINTS		"info-history-graph-points"
#define GPM_SETTINGS_INFO_STATS_TYPE			"info-stats-type"
#define GPM_SETTINGS_INFO_STATS_GRAPH_SMOOTH		"info-stats-graph-smooth"
#define GPM_SETTINGS_INFO_STATS_GRAPH_POINTS		"info-stats-graph-points"
#define GPM_SETTINGS_INFO_PAGE_NUMBER			"info-page-number"
#define GPM_SETTINGS_INFO_LAST_DEVICE			"info-last-device"

static GtkBuilder *builder = NULL;
static GtkListStore *list_store_info = NULL;
static GtkListStore *list_store_devices = NULL;
gchar *current_device = NULL;
static const gchar *history_type;
static const gchar *stats_type;
static guint history_time;
static guint divs_x;
static GSettings *settings;
static gfloat sigma_smoothing = 0.0f;
static GtkWidget *graph_history = NULL;
static GtkWidget *graph_statistics = NULL;
static UpClient *client = NULL;
static GPtrArray *devices = NULL;

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

#define GPM_HISTORY_MINUTE_TEXT			_("30 minutes")
#define GPM_HISTORY_HOUR_TEXT			_("3 hours")
#define GPM_HISTORY_HOURS_TEXT			_("8 hours")
#define GPM_HISTORY_DAY_TEXT			_("1 day")
#define GPM_HISTORY_WEEK_TEXT			_("1 week")

#define GPM_HISTORY_MINUTE_VALUE		30*60
#define GPM_HISTORY_HOUR_VALUE			3*60*60
#define GPM_HISTORY_HOURS_VALUE			8*60*60
#define GPM_HISTORY_DAY_VALUE			24*60*60
#define GPM_HISTORY_WEEK_VALUE			7*24*60*60

#define GPM_HISTORY_MINUTE_DIVS			6  /* 5 min tick */
#define GPM_HISTORY_HOUR_DIVS			6  /* 30 min tick */
#define GPM_HISTORY_HOURS_DIVS			8  /* 1 hr tick */
#define GPM_HISTORY_DAY_DIVS			12 /* 2 hr tick */
#define GPM_HISTORY_WEEK_DIVS			7  /* 1 day tick */

/* TRANSLATORS: what we've observed about the device */
#define GPM_STATS_CHARGE_DATA_TEXT		_("Charge profile")
#define GPM_STATS_DISCHARGE_DATA_TEXT		_("Discharge profile")
/* TRANSLATORS: how accurately we can predict the time remaining of the battery */
#define GPM_STATS_CHARGE_ACCURACY_TEXT		_("Charge accuracy")
#define GPM_STATS_DISCHARGE_ACCURACY_TEXT	_("Discharge accuracy")

#define GPM_STATS_CHARGE_DATA_VALUE		"charge-data"
#define GPM_STATS_CHARGE_ACCURACY_VALUE		"charge-accuracy"
#define GPM_STATS_DISCHARGE_DATA_VALUE		"discharge-data"
#define GPM_STATS_DISCHARGE_ACCURACY_VALUE	"discharge-accuracy"

#define GPM_UP_TIME_PRECISION			5*60 /* seconds */
#define GPM_UP_TEXT_MIN_TIME			120 /* seconds */

/**
 * gpm_stats_get_device_icon_suffix:
 * @device: The UpDevice
 *
 * Return value: The character string for the filename suffix.
 **/
static const gchar *
gpm_stats_get_device_icon_suffix (UpDevice *device)
{
	gdouble percentage;
	/* get device properties */
	g_object_get (device, "percentage", &percentage, NULL);
	if (percentage < 10)
		return "caution";
	else if (percentage < 30)
		return "low";
	else if (percentage < 60)
		return "good";
	return "full";
}

static GIcon *
gpm_stats_get_device_icon (UpDevice *device, gboolean use_symbolic)
{
	const gchar *suffix_str;
	UpDeviceKind kind;
	UpDeviceState state;
	gboolean is_present;
	gdouble percentage;
	GIcon *icon = NULL;
	g_auto(GStrv) iconnames = NULL;
	g_autoptr(GString) filename = NULL;

	g_return_val_if_fail (device != NULL, NULL);

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "state", &state,
		      "percentage", &percentage,
		      "is-present", &is_present,
		      NULL);

	/* get correct icon prefix */
	filename = g_string_new (NULL);

	/* get the icon from some simple rules */
	if (kind == UP_DEVICE_KIND_LINE_POWER) {
		if (use_symbolic)
			g_string_append (filename, "ac-adapter-symbolic;");
		g_string_append (filename, "ac-adapter;");

	} else if (kind == UP_DEVICE_KIND_MONITOR) {
		if (use_symbolic)
			g_string_append (filename, "ac-adapter-symbolic;");
		g_string_append (filename, "ac-adapter;");

	} else {
		if (!is_present) {
			if (use_symbolic)
				g_string_append (filename, "battery-missing-symbolic;");
			g_string_append (filename, "battery-missing;");

		} else {
			switch (state) {
			case UP_DEVICE_STATE_EMPTY:
				if (use_symbolic)
					g_string_append (filename, "battery-empty-symbolic;");
				g_string_append (filename, "battery-empty;");
				break;
			case UP_DEVICE_STATE_FULLY_CHARGED:
				if (use_symbolic) {
					g_string_append (filename, "battery-full-charged-symbolic;");
					g_string_append (filename, "battery-full-charging-symbolic;");
				}
				g_string_append (filename, "battery-full-charged;");
				g_string_append (filename, "battery-full-charging;");
				break;
			case UP_DEVICE_STATE_CHARGING:
			case UP_DEVICE_STATE_PENDING_CHARGE:
				suffix_str = gpm_stats_get_device_icon_suffix (device);
				if (use_symbolic)
					g_string_append_printf (filename, "battery-%s-charging-symbolic;", suffix_str);
				g_string_append_printf (filename, "battery-%s-charging;", suffix_str);
				break;
			case UP_DEVICE_STATE_DISCHARGING:
			case UP_DEVICE_STATE_PENDING_DISCHARGE:
				suffix_str = gpm_stats_get_device_icon_suffix (device);
				if (use_symbolic)
					g_string_append_printf (filename, "battery-%s-symbolic;", suffix_str);
				g_string_append_printf (filename, "battery-%s;", suffix_str);
				break;
			default:
				if (use_symbolic)
					g_string_append (filename, "battery-missing-symbolic;");
				g_string_append (filename, "battery-missing;");
			}
		}
	}

	/* nothing matched */
	if (filename->len == 0) {
		g_warning ("nothing matched, falling back to default icon");
		g_string_append (filename, "dialog-warning;");
	}

	g_debug ("got filename: %s", filename->str);

	iconnames = g_strsplit (filename->str, ";", -1);
	icon = g_themed_icon_new_from_names (iconnames, -1);
	return icon;
}

static const gchar *
gpm_device_kind_to_localised_string (UpDeviceKind kind, guint number)
{
	const gchar *text = NULL;
	switch (kind) {
	case UP_DEVICE_KIND_LINE_POWER:
		/* TRANSLATORS: system power cord */
		text = ngettext ("AC adapter", "AC adapters", number);
		break;
	case UP_DEVICE_KIND_BATTERY:
		/* TRANSLATORS: laptop primary battery */
		text = ngettext ("Laptop battery", "Laptop batteries", number);
		break;
	case UP_DEVICE_KIND_UPS:
		/* TRANSLATORS: battery-backed AC power source */
		text = ngettext ("UPS", "UPSs", number);
		break;
	case UP_DEVICE_KIND_MONITOR:
		/* TRANSLATORS: a monitor is a device to measure voltage and current */
		text = ngettext ("Monitor", "Monitors", number);
		break;
	case UP_DEVICE_KIND_MOUSE:
		/* TRANSLATORS: wireless mice with internal batteries */
		text = ngettext ("Mouse", "Mice", number);
		break;
	case UP_DEVICE_KIND_KEYBOARD:
		/* TRANSLATORS: wireless keyboard with internal battery */
		text = ngettext ("Keyboard", "Keyboards", number);
		break;
	case UP_DEVICE_KIND_PDA:
		/* TRANSLATORS: portable device */
		text = ngettext ("PDA", "PDAs", number);
		break;
	case UP_DEVICE_KIND_PHONE:
		/* TRANSLATORS: cell phone (mobile...) */
		text = ngettext ("Cell phone", "Cell phones", number);
		break;
#if UP_CHECK_VERSION(0,9,5)
	case UP_DEVICE_KIND_MEDIA_PLAYER:
		/* TRANSLATORS: media player, mp3 etc */
		text = ngettext ("Media player", "Media players", number);
		break;
	case UP_DEVICE_KIND_TABLET:
		/* TRANSLATORS: tablet device */
		text = ngettext ("Tablet", "Tablets", number);
		break;
	case UP_DEVICE_KIND_COMPUTER:
		/* TRANSLATORS: tablet device */
		text = ngettext ("Computer", "Computers", number);
		break;
#endif
	default:
		g_warning ("enum unrecognised: %u", kind);
		text = up_device_kind_to_string (kind);
	}
	return text;
}

static const gchar *
gpm_device_technology_to_localised_string (UpDeviceTechnology technology_enum)
{
	const gchar *technology = NULL;
	switch (technology_enum) {
	case UP_DEVICE_TECHNOLOGY_LITHIUM_ION:
		/* TRANSLATORS: battery technology */
		technology = _("Lithium Ion");
		break;
	case UP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER:
		/* TRANSLATORS: battery technology */
		technology = _("Lithium Polymer");
		break;
	case UP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE:
		/* TRANSLATORS: battery technology */
		technology = _("Lithium Iron Phosphate");
		break;
	case UP_DEVICE_TECHNOLOGY_LEAD_ACID:
		/* TRANSLATORS: battery technology */
		technology = _("Lead acid");
		break;
	case UP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM:
		/* TRANSLATORS: battery technology */
		technology = _("Nickel Cadmium");
		break;
	case UP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE:
		/* TRANSLATORS: battery technology */
		technology = _("Nickel metal hydride");
		break;
	case UP_DEVICE_TECHNOLOGY_UNKNOWN:
		/* TRANSLATORS: battery technology */
		technology = _("Unknown technology");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return technology;
}

static const gchar *
gpm_device_state_to_localised_string (UpDeviceState state)
{
	const gchar *state_string = NULL;

	switch (state) {
	case UP_DEVICE_STATE_CHARGING:
		/* TRANSLATORS: battery state */
		state_string = _("Charging");
		break;
	case UP_DEVICE_STATE_DISCHARGING:
		/* TRANSLATORS: battery state */
		state_string = _("Discharging");
		break;
	case UP_DEVICE_STATE_EMPTY:
		/* TRANSLATORS: battery state */
		state_string = _("Empty");
		break;
	case UP_DEVICE_STATE_FULLY_CHARGED:
		/* TRANSLATORS: battery state */
		state_string = _("Charged");
		break;
	case UP_DEVICE_STATE_PENDING_CHARGE:
		/* TRANSLATORS: battery state */
		state_string = _("Waiting to charge");
		break;
	case UP_DEVICE_STATE_PENDING_DISCHARGE:
		/* TRANSLATORS: battery state */
		state_string = _("Waiting to discharge");
		break;
	case UP_DEVICE_STATE_UNKNOWN:
		/* TRANSLATORS: battery state */
		state_string = _("Unknown");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return state_string;
}

static void
gpm_stats_add_info_columns (GtkTreeView *treeview)
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

static void
gpm_stats_add_devices_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "icon-size", GTK_ICON_SIZE_LARGE, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Image"), renderer,
							   "gicon", GPM_DEVICES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Description"), renderer,
							   "markup", GPM_DEVICES_COLUMN_TEXT, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GPM_INFO_COLUMN_TEXT);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

static void
gpm_stats_add_info_data (const gchar *attr, const gchar *text)
{
	GtkTreeIter iter;
	gtk_list_store_append (list_store_info, &iter);
	gtk_list_store_set (list_store_info, &iter,
			    GPM_INFO_COLUMN_TEXT, attr,
			    GPM_INFO_COLUMN_VALUE, text, -1);
}

static GPtrArray *
gpm_stats_update_smooth_data (GPtrArray *list)
{
	guint i;
	EggGraphPoint *point;
	EggGraphPoint *point_new;
	GPtrArray *new;
	GpmArrayFloat *raw;
	GpmArrayFloat *convolved;
	GpmArrayFloat *outliers;
	GpmArrayFloat *gaussian = NULL;

	/* convert the y data to a GpmArrayFloat array */
	raw = gpm_array_float_new (list->len);
	for (i = 0; i < list->len; i++) {
		point = (EggGraphPoint *) g_ptr_array_index (list, i);
		gpm_array_float_set (raw, i, point->y);
	}

	/* remove any outliers */
	outliers = gpm_array_float_remove_outliers (raw, 3, 0.1);

	/* convolve with gaussian */
	gaussian = gpm_array_float_compute_gaussian (15, sigma_smoothing);
	convolved = gpm_array_float_convolve (outliers, gaussian);

	/* add the smoothed data back into a new array */
	new = g_ptr_array_new_with_free_func ((GDestroyNotify) egg_graph_point_free);
	for (i = 0; i < list->len; i++) {
		point = (EggGraphPoint *) g_ptr_array_index (list, i);
		point_new = g_new0 (EggGraphPoint, 1);
		point_new->color = point->color;
		point_new->x = point->x;
		point_new->y = gpm_array_float_get (convolved, i);
		g_ptr_array_add (new, point_new);
	}

	/* free data */
	gpm_array_float_free (gaussian);
	gpm_array_float_free (raw);
	gpm_array_float_free (convolved);
	gpm_array_float_free (outliers);

	return new;
}

static gchar *
gpm_stats_time_to_string (gint seconds)
{
	gfloat value = seconds;

	if (value < 0) {
		/* TRANSLATORS: this is when the stats time is not known */
		return g_strdup (_("Unknown"));
	}
	if (value < 60) {
		/* TRANSLATORS: this is a time value, usually to show on a graph */
		return g_strdup_printf (ngettext ("%.0f second", "%.0f seconds", value), value);
	}
	value /= 60.0;
	if (value < 60) {
		/* TRANSLATORS: this is a time value, usually to show on a graph */
		return g_strdup_printf (ngettext ("%.1f minute", "%.1f minutes", value), value);
	}
	value /= 60.0;
	if (value < 60) {
		/* TRANSLATORS: this is a time value, usually to show on a graph */
		return g_strdup_printf (ngettext ("%.1f hour", "%.1f hours", value), value);
	}
	value /= 24.0;
	/* TRANSLATORS: this is a time value, usually to show on a graph */
	return g_strdup_printf (ngettext ("%.1f day", "%.1f days", value), value);
}

static const gchar *
gpm_stats_bool_to_string (gboolean ret)
{
	return ret ? _("Yes") : _("No");
}

static gchar *
gpm_stats_get_printable_device_path (UpDevice *device)
{
	const gchar *object_path;
	gchar *device_path = NULL;

	/* get object path */
	object_path = up_device_get_object_path (device);
	if (object_path != NULL)
		device_path = g_filename_display_basename (object_path);

	return device_path;
}

static void
gpm_stats_update_info_page_details (UpDevice *device)
{
	gchar *text;
	guint refreshed;
	UpDeviceKind kind;
	UpDeviceState state;
	UpDeviceTechnology technology;
	gdouble percentage;
	gdouble capacity;
	gdouble energy;
	gdouble energy_empty;
	gdouble energy_full;
	gdouble energy_full_design;
	gdouble energy_rate;
	gdouble voltage;
	gboolean online;
	gboolean is_present;
	gboolean power_supply;
	gboolean is_rechargeable;
	guint64 update_time;
	gint64 time_to_full;
	gint64 time_to_empty;
	g_autofree gchar *device_path = NULL;
	g_autofree gchar *model = NULL;
	g_autofree gchar *serial = NULL;
	g_autofree gchar *vendor = NULL;

	gtk_list_store_clear (list_store_info);

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "state", &state,
		      "percentage", &percentage,
		      "online", &online,
		      "update_time", &update_time,
		      "power_supply", &power_supply,
		      "is_rechargeable", &is_rechargeable,
		      "is-present", &is_present,
		      "time-to-full", &time_to_full,
		      "time-to-empty", &time_to_empty,
		      "technology", &technology,
		      "capacity", &capacity,
		      "energy", &energy,
		      "energy-empty", &energy_empty,
		      "energy-full", &energy_full,
		      "energy-full-design", &energy_full_design,
		      "energy-rate", &energy_rate,
		      "voltage", &voltage,
		      "vendor", &vendor,
		      "serial", &serial,
		      "model", &model,
		      NULL);

	/* remove prefix */
	device_path = gpm_stats_get_printable_device_path (device);
	/* TRANSLATORS: the device ID of the current device, e.g. "battery0" */
	gpm_stats_add_info_data (_("Device"), device_path);

	gpm_stats_add_info_data (_("Type"), gpm_device_kind_to_localised_string (kind, 1));
	if (vendor != NULL && vendor[0] != '\0')
		gpm_stats_add_info_data (_("Vendor"), vendor);
	if (model != NULL && model[0] != '\0')
		gpm_stats_add_info_data (_("Model"), model);
	if (serial != NULL && serial[0] != '\0')
		gpm_stats_add_info_data (_("Serial number"), serial);

	/* TRANSLATORS: a boolean attribute that means if the device is supplying the
	 * main power for the computer. For instance, an AC adapter or laptop battery
	 * would be TRUE,  but a mobile phone or mouse taking power is FALSE */
	gpm_stats_add_info_data (_("Supply"), gpm_stats_bool_to_string (power_supply));

	refreshed = (int) (time (NULL) - update_time);
	text = g_strdup_printf (ngettext ("%u second", "%u seconds", refreshed), refreshed);

	/* TRANSLATORS: when the device was last updated with new data. It's
	* usually a few seconds when a device is discharging or charging. */
	gpm_stats_add_info_data (_("Refreshed"), text);
	g_free (text);

	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_MOUSE ||
	    kind == UP_DEVICE_KIND_KEYBOARD ||
	    kind == UP_DEVICE_KIND_UPS) {
		/* TRANSLATORS: Present is whether the device is currently attached
		 * to the computer, as some devices (e.g. laptop batteries) can
		 * be removed, but still observed as devices on the system */
		gpm_stats_add_info_data (_("Present"), gpm_stats_bool_to_string (is_present));
	}
	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_MOUSE ||
	    kind == UP_DEVICE_KIND_KEYBOARD) {
		/* TRANSLATORS: If the device can be recharged, e.g. lithium
		 * batteries rather than alkaline ones */
		gpm_stats_add_info_data (_("Rechargeable"), gpm_stats_bool_to_string (is_rechargeable));
	}
	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_MOUSE ||
	    kind == UP_DEVICE_KIND_KEYBOARD) {
		/* TRANSLATORS: The state of the device, e.g. "Changing" or "Fully charged" */
		gpm_stats_add_info_data (_("State"), gpm_device_state_to_localised_string (state));
	}
	if (kind == UP_DEVICE_KIND_BATTERY) {
		text = g_strdup_printf ("%.1f Wh", energy);
		gpm_stats_add_info_data (_("Energy"), text);
		g_free (text);
		text = g_strdup_printf ("%.1f Wh", energy_empty);
		gpm_stats_add_info_data (_("Energy when empty"), text);
		g_free (text);
		text = g_strdup_printf ("%.1f Wh", energy_full);
		gpm_stats_add_info_data (_("Energy when full"), text);
		g_free (text);
		text = g_strdup_printf ("%.1f Wh", energy_full_design);
		gpm_stats_add_info_data (_("Energy (design)"), text);
		g_free (text);
	}
	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_MONITOR) {
		text = g_strdup_printf ("%.1f W", energy_rate);
		/* TRANSLATORS: the rate of discharge for the device */
		gpm_stats_add_info_data (_("Rate"), text);
		g_free (text);
	}
	if (kind == UP_DEVICE_KIND_UPS ||
	    kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_MONITOR) {
		text = g_strdup_printf ("%.1f V", voltage);
		gpm_stats_add_info_data (_("Voltage"), text);
		g_free (text);
	}
	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_UPS) {
		if (time_to_full >= 0) {
			text = gpm_stats_time_to_string (time_to_full);
			gpm_stats_add_info_data (_("Time to full"), text);
			g_free (text);
		}
		if (time_to_empty >= 0) {
			text = gpm_stats_time_to_string (time_to_empty);
			gpm_stats_add_info_data (_("Time to empty"), text);
			g_free (text);
		}
	}
	if (kind == UP_DEVICE_KIND_BATTERY ||
	    kind == UP_DEVICE_KIND_MOUSE ||
	    kind == UP_DEVICE_KIND_KEYBOARD ||
	    kind == UP_DEVICE_KIND_UPS) {
		text = g_strdup_printf ("%.1f%%", percentage);
		/* TRANSLATORS: the amount of charge the cell contains */
		gpm_stats_add_info_data (_("Percentage"), text);
		g_free (text);
	}
	if (kind == UP_DEVICE_KIND_BATTERY) {
		text = g_strdup_printf ("%.1f%%", capacity);
		/* TRANSLATORS: the capacity of the device, which is basically a measure
		 * of how full it can get, relative to the design capacity */
		gpm_stats_add_info_data (_("Capacity"), text);
		g_free (text);
	}
	if (kind == UP_DEVICE_KIND_BATTERY) {
		/* TRANSLATORS: the type of battery, e.g. lithium or nikel metal hydroxide */
		gpm_stats_add_info_data (_("Technology"), gpm_device_technology_to_localised_string (technology));
	}
	if (kind == UP_DEVICE_KIND_LINE_POWER) {
		/* TRANSLATORS: this is when the device is plugged in, typically
		 * only shown for the ac adaptor device */
		gpm_stats_add_info_data (_("Online"), gpm_stats_bool_to_string (online));
	}
}

static void
gpm_stats_set_graph_data (GtkWidget *widget, GPtrArray *data, gboolean use_smoothed, gboolean use_points)
{
	GPtrArray *smoothed;

	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (widget));

	/* add correct data */
	if (!use_smoothed) {
		if (use_points)
			egg_graph_widget_data_add (EGG_GRAPH_WIDGET (widget), EGG_GRAPH_WIDGET_PLOT_BOTH, data);
		else
			egg_graph_widget_data_add (EGG_GRAPH_WIDGET (widget), EGG_GRAPH_WIDGET_PLOT_LINE, data);
	} else {
		smoothed = gpm_stats_update_smooth_data (data);
		if (use_points)
			egg_graph_widget_data_add (EGG_GRAPH_WIDGET (widget), EGG_GRAPH_WIDGET_PLOT_POINTS, data);
		egg_graph_widget_data_add (EGG_GRAPH_WIDGET (widget), EGG_GRAPH_WIDGET_PLOT_LINE, smoothed);
		g_ptr_array_unref (smoothed);
	}

	/* show */
	gtk_widget_show (widget);
}

static guint32
gpm_color_from_rgb (guint8 red, guint8 green, guint8 blue)
{
	guint32 color = 0;
	color += (guint32) red * 0x10000;
	color += (guint32) green * 0x100;
	color += (guint32) blue;
	return color;
}

static void
gpm_stats_update_info_page_history (UpDevice *device)
{
	GPtrArray *array;
	guint i;
	UpHistoryItem *item;
	GtkWidget *widget;
	gboolean checked;
	gboolean points;
	EggGraphPoint *point;
	GPtrArray *new;
	gint64 offset = 0;

	new = g_ptr_array_new_with_free_func ((GDestroyNotify) egg_graph_point_free);
	if (g_strcmp0 (history_type, GPM_HISTORY_CHARGE_VALUE) == 0) {
		g_object_set (graph_history,
			      "type-x", EGG_GRAPH_WIDGET_KIND_TIME,
			      "type-y", EGG_GRAPH_WIDGET_KIND_PERCENTAGE,
			      "autorange-x", FALSE,
			      "divs-x", (guint) divs_x,
			      "start-x", -(gdouble) history_time,
			      "stop-x", (gdouble) 0.f,
			      "autorange-y", FALSE,
			      "start-y", (gdouble) 0.f,
			      "stop-y", (gdouble) 100.f,
			      NULL);
	} else if (g_strcmp0 (history_type, GPM_HISTORY_RATE_VALUE) == 0) {
		g_object_set (graph_history,
			      "type-x", EGG_GRAPH_WIDGET_KIND_TIME,
			      "type-y", EGG_GRAPH_WIDGET_KIND_POWER,
			      "autorange-x", FALSE,
			      "divs-x", (guint) divs_x,
			      "start-x", -(gdouble) history_time,
			      "stop-x", (gdouble) 0.f,
			      "autorange-y", TRUE,
			      NULL);
	} else {
		g_object_set (graph_history,
			      "type-x", EGG_GRAPH_WIDGET_KIND_TIME,
			      "type-y", EGG_GRAPH_WIDGET_KIND_TIME,
			      "autorange-x", FALSE,
			      "divs-x", (guint) divs_x,
			      "start-x", -(gdouble) history_time,
			      "stop-x", (gdouble) 0.f,
			      "autorange-y", TRUE,
			      NULL);
	}

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_history_nodata"));
	array = up_device_get_history_sync (device, history_type, history_time, 150, NULL, NULL);
	if (array == NULL) {
		/* show no data label and hide graph */
		gtk_widget_hide (graph_history);
		gtk_widget_show (widget);
		goto out;
	}

	/* hide no data and show graph */
	gtk_widget_hide (widget);
	gtk_widget_show (graph_history);

	/* convert microseconds to seconds */
	offset = g_get_real_time() / 1000000;

	for (i = 0; i < array->len; i++) {
		item = (UpHistoryItem *) g_ptr_array_index (array, i);

		/* abandon this point */
		if (up_history_item_get_state (item) == UP_DEVICE_STATE_UNKNOWN)
			continue;

		point = egg_graph_point_new ();
		point->x = (gint) up_history_item_get_time (item) - offset;
		point->y = up_history_item_get_value (item);
		if (up_history_item_get_state (item) == UP_DEVICE_STATE_CHARGING)
			point->color = gpm_color_from_rgb (255, 0, 0);
		else if (up_history_item_get_state (item) == UP_DEVICE_STATE_DISCHARGING)
			point->color = gpm_color_from_rgb (0, 0, 255);
		else if (up_history_item_get_state (item) == UP_DEVICE_STATE_PENDING_CHARGE)
			point->color = gpm_color_from_rgb (200, 0, 0);
		else if (up_history_item_get_state (item) == UP_DEVICE_STATE_PENDING_DISCHARGE)
			point->color = gpm_color_from_rgb (0, 0, 200);
		else {
			if (g_strcmp0 (history_type, GPM_HISTORY_RATE_VALUE) == 0)
				point->color = gpm_color_from_rgb (255, 255, 255);
			else
				point->color = gpm_color_from_rgb (0, 255, 0);
		}
		g_ptr_array_add (new, point);
	}

	/* render */
	sigma_smoothing = 2.0;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_smooth_history"));
	checked = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_points_history"));
	points = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));

	/* present data to graph */
	gpm_stats_set_graph_data (graph_history, new, checked, points);

	g_ptr_array_unref (array);
	g_ptr_array_unref (new);
out:
	return;
}

static void
gpm_stats_update_info_page_stats (UpDevice *device)
{
	GPtrArray *array;
	guint i;
	UpStatsItem *item;
	GtkWidget *widget;
	gboolean checked;
	gboolean points;
	EggGraphPoint *point;
	GPtrArray *new;
	gboolean use_data = FALSE;
	const gchar *type = NULL;

	new = g_ptr_array_new_with_free_func ((GDestroyNotify) egg_graph_point_free);
	if (g_strcmp0 (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0) {
		type = "charging";
		use_data = TRUE;
	} else if (g_strcmp0 (stats_type, GPM_STATS_DISCHARGE_DATA_VALUE) == 0) {
		type = "discharging";
		use_data = TRUE;
	} else if (g_strcmp0 (stats_type, GPM_STATS_CHARGE_ACCURACY_VALUE) == 0) {
		type = "charging";
		use_data = FALSE;
	} else if (g_strcmp0 (stats_type, GPM_STATS_DISCHARGE_ACCURACY_VALUE) == 0) {
		type = "discharging";
		use_data = FALSE;
	} else {
		g_assert_not_reached ();
	}

	if (use_data) {
		g_object_set (graph_statistics,
			      "type-x", EGG_GRAPH_WIDGET_KIND_PERCENTAGE,
			      "type-y", EGG_GRAPH_WIDGET_KIND_FACTOR,
			      "divs-x", 10,
			      "autorange-x", TRUE,
			      "autorange-y", TRUE,
			      NULL);
	} else {
		g_object_set (graph_statistics,
			      "type-x", EGG_GRAPH_WIDGET_KIND_PERCENTAGE,
			      "type-y", EGG_GRAPH_WIDGET_KIND_PERCENTAGE,
			      "divs-x", 10,
			      "autorange-x", TRUE,
			      "autorange-y", TRUE,
			      NULL);
	}

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_stats_nodata"));
	array = up_device_get_statistics_sync (device, type, NULL, NULL);
	if (array == NULL) {
		/* show no data label and hide graph */
		gtk_widget_hide (graph_statistics);
		gtk_widget_show (widget);
		goto out;
	}

	/* hide no data and show graph */
	gtk_widget_hide (widget);
	gtk_widget_show (graph_statistics);

	for (i = 0; i < array->len; i++) {
		item = (UpStatsItem *) g_ptr_array_index (array, i);
		point = egg_graph_point_new ();
		point->x = i;
		if (use_data)
			point->y = up_stats_item_get_value (item);
		else
			point->y = up_stats_item_get_accuracy (item);
		point->color = gpm_color_from_rgb (255, 0, 0);
		g_ptr_array_add (new, point);
	}

	/* render */
	sigma_smoothing = 1.1;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_smooth_stats"));
	checked = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_points_stats"));
	points = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));

	/* present data to graph */
	gpm_stats_set_graph_data (graph_statistics, new, checked, points);

	g_ptr_array_unref (array);
	g_ptr_array_unref (new);
out:
	return;
}

static void
gpm_stats_update_info_data_page (UpDevice *device, gint page)
{
	if (page == 0)
		gpm_stats_update_info_page_details (device);
	else if (page == 1)
		gpm_stats_update_info_page_history (device);
	else if (page == 2)
		gpm_stats_update_info_page_stats (device);
}

static void
gpm_stats_update_info_data (UpDevice *device)
{
	gint page;
	GtkNotebook *notebook;
	GtkWidget *page_widget;
	gboolean has_history;
	gboolean has_statistics;

	/* get properties */
	g_object_get (device,
		      "has-history", &has_history,
		      "has-statistics", &has_statistics,
		      NULL);

	notebook = GTK_NOTEBOOK (gtk_builder_get_object (builder, "notebook1"));

	/* show info page */
	page_widget = gtk_notebook_get_nth_page (notebook, 0);
	gtk_widget_show (page_widget);

	/* hide history if no support */
	page_widget = gtk_notebook_get_nth_page (notebook, 1);
	if (has_history)
		gtk_widget_show (page_widget);
	else
		gtk_widget_hide (page_widget);

	/* hide statistics if no support */
	page_widget = gtk_notebook_get_nth_page (notebook, 2);
	if (has_statistics)
		gtk_widget_show (page_widget);
	else
		gtk_widget_hide (page_widget);

	page = gtk_notebook_get_current_page (notebook);
	gpm_stats_update_info_data_page (device, page);

	return;
}

static void
gpm_stats_set_title (GtkWindow *window, gint page_num)
{
	g_autofree gchar *title = NULL;
	const gchar * const page_titles[] = {
		/* TRANSLATORS: shown on the titlebar */
		N_("Device Information"),
		/* TRANSLATORS: shown on the titlebar */
		N_("Device History"),
		/* TRANSLATORS: shown on the titlebar */
		N_("Device Profile"),
	};

	/* TRANSLATORS: shown on the titlebar */
	title = g_strdup_printf ("%s - %s", _("Power Statistics"), _(page_titles[page_num]));
	gtk_window_set_title (window, title);
}

static void
gpm_stats_notebook_changed_cb (GtkNotebook *notebook, gpointer page, gint page_num, gpointer user_data)
{
	UpDevice *device;
	GtkWidget *widget;

	/* set the window title depending on the mode */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_stats"));
	gpm_stats_set_title (GTK_WINDOW (widget), page_num);

	/* save page in gconf */
	g_settings_set_int (settings, GPM_SETTINGS_INFO_PAGE_NUMBER, page_num);

	if (current_device == NULL)
		return;

	device = up_device_new ();
	up_device_set_object_path_sync (device, current_device, NULL, NULL);
	gpm_stats_update_info_data_page (device, page_num);
	g_object_unref (device);
}

static void
gpm_stats_button_update_ui (void)
{
	UpDevice *device;
	device = up_device_new ();
	up_device_set_object_path_sync (device, current_device, NULL, NULL);
	gpm_stats_update_info_data (device);
	g_object_unref (device);
}

static void
gpm_stats_devices_treeview_clicked_cb (GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	UpDevice *device;

	/* This will only work in single or browse selection mode! */
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_free (current_device);
		gtk_tree_model_get (model, &iter, GPM_DEVICES_COLUMN_ID, &current_device, -1);

		/* save device in gconf */
		g_settings_set_string (settings, GPM_SETTINGS_INFO_LAST_DEVICE, current_device);

		/* show transaction_id */
		g_debug ("selected row is: %s", current_device);

		/* is special device */
		if (1) {
			device = up_device_new ();
			up_device_set_object_path_sync (device, current_device, NULL, NULL);
			gpm_stats_update_info_data (device);
			g_object_unref (device);
		}

	} else {
		g_debug ("no row selected");
	}
}

static void
gpm_stats_device_changed_cb (UpDevice *device, GParamSpec *pspec, gpointer user_data)
{
	const gchar *object_path;
	object_path = up_device_get_object_path (device);
	if (object_path == NULL || current_device == NULL)
		return;
	g_debug ("changed:   %s", object_path);
	if (g_strcmp0 (current_device, object_path) == 0)
		gpm_stats_update_info_data (device);
}

static void
gpm_stats_add_device (UpDevice *device)
{
	const gchar *id;
	GtkTreeIter iter;
	const gchar *text;
	UpDeviceKind kind;
	g_autoptr(GIcon) icon = NULL;

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      NULL);
	g_ptr_array_add (devices, g_object_ref (device));
	g_signal_connect (device, "notify",
			  G_CALLBACK (gpm_stats_device_changed_cb), NULL);

	id = up_device_get_object_path (device);
	text = gpm_device_kind_to_localised_string (kind, 1);
	icon = gpm_stats_get_device_icon (device, FALSE);

	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    GPM_DEVICES_COLUMN_ID, id,
			    GPM_DEVICES_COLUMN_TEXT, text,
			    GPM_DEVICES_COLUMN_ICON, icon, -1);
}

static void
gpm_stats_device_added_cb (UpClient *_client, UpDevice *device, gpointer user_data)
{
	const gchar *object_path;
	object_path = up_device_get_object_path (device);
	g_debug ("added:     %s", object_path);
	gpm_stats_add_device (device);
}

static void
gpm_stats_device_removed_cb (UpClient *_client, const gchar *object_path, gpointer user_data)
{
	GtkTreeIter iter;
	UpDevice *device_tmp;
	gboolean ret;
	guint i;

	for (i = 0; i < devices->len; i++) {
		device_tmp = g_ptr_array_index (devices, i);
		if (g_strcmp0 (up_device_get_object_path (device_tmp), object_path) == 0) {
			g_ptr_array_remove_index_fast (devices, i);
			break;
		}
	}

	g_debug ("removed:   %s", object_path);
	if (g_strcmp0 (current_device, object_path) == 0) {
		gtk_list_store_clear (list_store_info);
	}

	/* search the list and remove the object path entry */
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store_devices), &iter);
	while (ret) {
		g_autofree gchar *id = NULL;
		gtk_tree_model_get (GTK_TREE_MODEL (list_store_devices), &iter, GPM_DEVICES_COLUMN_ID, &id, -1);
		if (g_strcmp0 (id, object_path) == 0) {
			gtk_list_store_remove (list_store_devices, &iter);
			break;
		}
		ret = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store_devices), &iter);
	};
}

static void
gpm_stats_history_type_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	g_autofree gchar *value = NULL;
	const gchar *axis_x = NULL;
	const gchar *axis_y = NULL;
	value = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));
	if (g_strcmp0 (value, GPM_HISTORY_RATE_TEXT) == 0) {
		history_type = GPM_HISTORY_RATE_VALUE;
		/* TRANSLATORS: this is the X axis on the graph */
		axis_x = _("Time elapsed");
		/* TRANSLATORS: this is the Y axis on the graph */
		axis_y = _("Power");
	} else if (g_strcmp0 (value, GPM_HISTORY_CHARGE_TEXT) == 0) {
		history_type = GPM_HISTORY_CHARGE_VALUE;
		/* TRANSLATORS: this is the X axis on the graph */
		axis_x = _("Time elapsed");
		/* TRANSLATORS: this is the Y axis on the graph for the whole battery device */
		axis_y = _("Cell charge");
	} else if (g_strcmp0 (value, GPM_HISTORY_TIME_FULL_TEXT) == 0) {
		history_type = GPM_HISTORY_TIME_FULL_VALUE;
		/* TRANSLATORS: this is the X axis on the graph */
		axis_x = _("Time elapsed");
		/* TRANSLATORS: this is the Y axis on the graph */
		axis_y = _("Predicted time");
	} else if (g_strcmp0 (value, GPM_HISTORY_TIME_EMPTY_TEXT) == 0) {
		history_type = GPM_HISTORY_TIME_EMPTY_VALUE;
		/* TRANSLATORS: this is the X axis on the graph */
		axis_x = _("Time elapsed");
		/* TRANSLATORS: this is the Y axis on the graph */
		axis_y = _("Predicted time");
	} else {
		g_assert (FALSE);
	}

	/* set axis */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_axis_history_x"));
	gtk_label_set_label (GTK_LABEL(widget), axis_x);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_axis_history_y"));
	gtk_label_set_label (GTK_LABEL(widget), axis_y);

	gpm_stats_button_update_ui ();

	/* save to gconf */
	g_settings_set_string (settings, GPM_SETTINGS_INFO_HISTORY_TYPE, history_type);
}

static void
gpm_stats_type_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	g_autofree gchar *value = NULL;
	const gchar *axis_x = NULL;
	const gchar *axis_y = NULL;
	value = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));
	if (g_strcmp0 (value, GPM_STATS_CHARGE_DATA_TEXT) == 0) {
		stats_type = GPM_STATS_CHARGE_DATA_VALUE;
		/* TRANSLATORS: this is the X axis on the graph for the whole battery device */
		axis_x = _("Cell charge");
		/* TRANSLATORS: this is the Y axis on the graph */
		axis_y = _("Correction factor");
	} else if (g_strcmp0 (value, GPM_STATS_CHARGE_ACCURACY_TEXT) == 0) {
		stats_type = GPM_STATS_CHARGE_ACCURACY_VALUE;
		/* TRANSLATORS: this is the X axis on the graph for the whole battery device */
		axis_x = _("Cell charge");
		/* TRANSLATORS: this is the Y axis on the graph */
		axis_y = _("Prediction accuracy");
	} else if (g_strcmp0 (value, GPM_STATS_DISCHARGE_DATA_TEXT) == 0) {
		stats_type = GPM_STATS_DISCHARGE_DATA_VALUE;
		/* TRANSLATORS: this is the X axis on the graph for the whole battery device */
		axis_x = _("Cell charge");
		/* TRANSLATORS: this is the Y axis on the graph */
		axis_y = _("Correction factor");
	} else if (g_strcmp0 (value, GPM_STATS_DISCHARGE_ACCURACY_TEXT) == 0) {
		stats_type = GPM_STATS_DISCHARGE_ACCURACY_VALUE;
		/* TRANSLATORS: this is the X axis on the graph for the whole battery device */
		axis_x = _("Cell charge");
		/* TRANSLATORS: this is the Y axis on the graph */
		axis_y = _("Prediction accuracy");
	} else {
		g_assert (FALSE);
	}

	/* set axis */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_axis_stats_x"));
	gtk_label_set_label (GTK_LABEL(widget), axis_x);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_axis_stats_y"));
	gtk_label_set_label (GTK_LABEL(widget), axis_y);

	gpm_stats_button_update_ui ();

	/* save to gconf */
	g_settings_set_string (settings, GPM_SETTINGS_INFO_STATS_TYPE, stats_type);
}

static void
gpm_stats_range_combo_changed (GtkWidget *widget, gpointer data)
{
	g_autofree gchar *value = NULL;
	value = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));
	if (g_strcmp0 (value, GPM_HISTORY_MINUTE_TEXT) == 0) {
		history_time = GPM_HISTORY_MINUTE_VALUE;
		divs_x = GPM_HISTORY_MINUTE_DIVS;
	} else if (g_strcmp0 (value, GPM_HISTORY_HOUR_TEXT) == 0) {
		history_time = GPM_HISTORY_HOUR_VALUE;
		divs_x = GPM_HISTORY_HOUR_DIVS;
	} else if (g_strcmp0 (value, GPM_HISTORY_HOURS_TEXT) == 0) {
		history_time = GPM_HISTORY_HOURS_VALUE;
		divs_x = GPM_HISTORY_HOURS_DIVS;
	} else if (g_strcmp0 (value, GPM_HISTORY_DAY_TEXT) == 0) {
		history_time = GPM_HISTORY_DAY_VALUE;
		divs_x = GPM_HISTORY_DAY_DIVS;
	} else if (g_strcmp0 (value, GPM_HISTORY_WEEK_TEXT) == 0) {
		history_time = GPM_HISTORY_WEEK_VALUE;
		divs_x = GPM_HISTORY_WEEK_DIVS;
	} else
		g_assert (FALSE);

	/* save to gconf */
	g_settings_set_int (settings, GPM_SETTINGS_INFO_HISTORY_TIME, history_time);

	gpm_stats_button_update_ui ();
}

static void
gpm_stats_smooth_checkbox_history_cb (GtkWidget *widget, gpointer data)
{
	gboolean checked;
	checked = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
	g_settings_set_boolean (settings, GPM_SETTINGS_INFO_HISTORY_GRAPH_SMOOTH, checked);
	gpm_stats_button_update_ui ();
}

static void
gpm_stats_smooth_checkbox_stats_cb (GtkWidget *widget, gpointer data)
{
	gboolean checked;
	checked = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
	g_settings_set_boolean (settings, GPM_SETTINGS_INFO_STATS_GRAPH_SMOOTH, checked);
	gpm_stats_button_update_ui ();
}

static void
gpm_stats_points_checkbox_history_cb (GtkWidget *widget, gpointer data)
{
	gboolean checked;
	checked = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
	g_settings_set_boolean (settings, GPM_SETTINGS_INFO_HISTORY_GRAPH_POINTS, checked);
	gpm_stats_button_update_ui ();
}

static void
gpm_stats_points_checkbox_stats_cb (GtkWidget *widget, gpointer data)
{
	gboolean checked;
	checked = gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
	g_settings_set_boolean (settings, GPM_SETTINGS_INFO_STATS_GRAPH_POINTS, checked);
	gpm_stats_button_update_ui ();
}

static gboolean
gpm_stats_highlight_device (const gchar *object_path)
{
	gboolean ret;
	gboolean found = FALSE;
	guint i;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkWidget *widget;

	/* we have to reuse the treeview data as it may be sorted */
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store_devices), &iter);
	for (i = 0; ret && !found; i++) {
		g_autofree gchar *id = NULL;
		gtk_tree_model_get (GTK_TREE_MODEL (list_store_devices), &iter,
				    GPM_DEVICES_COLUMN_ID, &id,
				    -1);
		if (g_strcmp0 (id, object_path) == 0) {
			g_autofree gchar *path_str = NULL;
			path_str = g_strdup_printf ("%u", i);
			path = gtk_tree_path_new_from_string (path_str);
			widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
			gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (widget), path, NULL, NULL, FALSE);
			gtk_tree_path_free (path);
			found = TRUE;
		}
		ret = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store_devices), &iter);
	}
	return found;
}

static int
gpm_stats_commandline_cb (GApplication *application,
			  GApplicationCommandLine *cmdline,
			  gpointer user_data)
{
	gboolean ret;
	GVariantDict *options;
	g_autofree gchar *last_device = NULL;

	/* read command line options */
	options = g_application_command_line_get_options_dict (cmdline);
	g_variant_dict_lookup (options, "device", "s", &last_device);

	/* get from GSettings if we never specified on the command line */
	if (last_device == NULL)
		last_device = g_settings_get_string (settings, GPM_SETTINGS_INFO_LAST_DEVICE);

	/* make sure the window is raised */
	g_application_activate (application);

	/* set the correct focus on the last device */
	if (last_device != NULL) {
		ret = gpm_stats_highlight_device (last_device);
		if (!ret) {
			g_warning ("failed to select");
			return FALSE;
		}
	}

	return TRUE;
}

static void
gpm_stats_activate_cb (GApplication *application,
		       gpointer user_data)
{
	GtkBox *box;
	GtkWidget *widget;
	GtkWindow *window;
	GtkTreeSelection *selection;
	GPtrArray *devices_tmp;
	UpDevice *device;
	UpDeviceKind kind;
	guint i, j;
	gint page;
	gboolean checked;
	guint retval;
	GError *error = NULL;

	window = gtk_application_get_active_window (GTK_APPLICATION (application));

	if (window != NULL) {
		gtk_window_present (window);
		return;
	}

	/* a store of UpDevices */
	devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* Ensure types */
	g_type_ensure (GPM_TYPE_ROTATED_WIDGET);

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_resource (builder,
						"/org/gnome/power-manager/gpm-statistics.ui",
						&error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
	}

	/* add history graph */
	box = GTK_BOX (gtk_builder_get_object (builder, "hbox_history"));
	graph_history = egg_graph_widget_new ();
	gtk_box_append (box, graph_history);
	gtk_widget_set_size_request (graph_history, 400, 250);
	gtk_widget_show (graph_history);

	/* add statistics graph */
	box = GTK_BOX (gtk_builder_get_object (builder, "hbox_statistics"));
	graph_statistics = egg_graph_widget_new ();
	gtk_box_append (box, graph_statistics);
	gtk_widget_set_size_request (graph_statistics, 400, 250);
	gtk_widget_show (graph_statistics);

	window = GTK_WINDOW (gtk_builder_get_object (builder, "dialog_stats"));
	gtk_window_set_application (window, GTK_APPLICATION (application));
	gtk_window_set_default_size (window, 800, 500);
	gtk_window_set_application (window, GTK_APPLICATION (application));
	gtk_window_set_default_icon_name ("org.gnome.PowerStats");

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_smooth_history"));
	checked = g_settings_get_boolean (settings, GPM_SETTINGS_INFO_HISTORY_GRAPH_SMOOTH);
	gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), checked);
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gpm_stats_smooth_checkbox_history_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_smooth_stats"));
	checked = g_settings_get_boolean (settings, GPM_SETTINGS_INFO_STATS_GRAPH_SMOOTH);
	gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), checked);
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gpm_stats_smooth_checkbox_stats_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_points_history"));
	checked = g_settings_get_boolean (settings, GPM_SETTINGS_INFO_HISTORY_GRAPH_POINTS);
	gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), checked);
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gpm_stats_points_checkbox_history_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_points_stats"));
	checked = g_settings_get_boolean (settings, GPM_SETTINGS_INFO_STATS_GRAPH_POINTS);
	gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), checked);
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gpm_stats_points_checkbox_stats_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "notebook1"));
	page = g_settings_get_int (settings, GPM_SETTINGS_INFO_PAGE_NUMBER);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), page);
	g_signal_connect (widget, "switch-page",
			  G_CALLBACK (gpm_stats_notebook_changed_cb), NULL);

	/* create list stores */
	list_store_info = gtk_list_store_new (GPM_INFO_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING);
	list_store_devices = gtk_list_store_new (GPM_DEVICES_COLUMN_LAST, G_TYPE_ICON,
						 G_TYPE_STRING, G_TYPE_STRING);

	/* create transaction_id tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_info"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_info));

	/* add columns to the tree view */
	gpm_stats_add_info_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget)); /* show */

	/* create transaction_id tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (gpm_stats_devices_treeview_clicked_cb), NULL);

	/* add columns to the tree view */
	gpm_stats_add_devices_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget)); /* show */

	history_type = g_settings_get_string (settings, GPM_SETTINGS_INFO_HISTORY_TYPE);
	history_time = g_settings_get_int (settings, GPM_SETTINGS_INFO_HISTORY_TIME);
	if (history_type == NULL)
		history_type = GPM_HISTORY_CHARGE_VALUE;

	stats_type = g_settings_get_string (settings, GPM_SETTINGS_INFO_STATS_TYPE);
	if (stats_type == NULL)
		stats_type = GPM_STATS_CHARGE_DATA_VALUE;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_history_type"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_RATE_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_CHARGE_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_TIME_FULL_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_TIME_EMPTY_TEXT);
	if (g_strcmp0 (history_type, GPM_HISTORY_RATE_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_stats_history_type_combo_changed_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_stats_type"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_STATS_CHARGE_DATA_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_STATS_CHARGE_ACCURACY_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_STATS_DISCHARGE_DATA_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_STATS_DISCHARGE_ACCURACY_TEXT);
	if (g_strcmp0 (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	else if (g_strcmp0 (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
	else if (g_strcmp0 (stats_type, GPM_STATS_CHARGE_DATA_VALUE) == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 3);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_stats_type_combo_changed_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_history_time"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_MINUTE_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_HOUR_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_HOURS_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_DAY_TEXT);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), GPM_HISTORY_WEEK_TEXT);

	if (history_time == GPM_HISTORY_MINUTE_VALUE) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		divs_x = GPM_HISTORY_MINUTE_DIVS;
	} else if (history_time == GPM_HISTORY_HOUR_VALUE) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
		divs_x = GPM_HISTORY_HOUR_DIVS;
	} else if (history_time == GPM_HISTORY_DAY_VALUE) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 3); 
		divs_x = GPM_HISTORY_DAY_DIVS;
	} else if (history_time == GPM_HISTORY_WEEK_VALUE) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 4);
		divs_x = GPM_HISTORY_WEEK_DIVS;
	} else { /* default */
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
		history_time = GPM_HISTORY_HOURS_VALUE;
		divs_x = GPM_HISTORY_HOURS_DIVS;
	}
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_stats_range_combo_changed), NULL);

	/* coldplug */
	client = up_client_new ();
	devices_tmp = up_client_get_devices2 (client);
	g_signal_connect (client, "device-added", G_CALLBACK (gpm_stats_device_added_cb), NULL);
	g_signal_connect (client, "device-removed", G_CALLBACK (gpm_stats_device_removed_cb), NULL);

	/* add devices in visually pleasing order */
	for (j=0; j<UP_DEVICE_KIND_LAST; j++) {
		for (i=0; i < devices_tmp->len; i++) {
			device = g_ptr_array_index (devices_tmp, i);
			g_object_get (device, "kind", &kind, NULL);
			if (kind == j)
				gpm_stats_add_device (device);
		}
	}

	/* set current device */
	if (devices->len > 0) {
		device = g_ptr_array_index (devices, 0);
		gpm_stats_update_info_data (device);
		current_device = g_strdup (up_device_get_object_path (device));
	}

	/* set axis */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_history_type"));
	gpm_stats_history_type_combo_changed_cb (widget, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_stats_type"));
	gpm_stats_type_combo_changed_cb (widget, NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_stats"));
	gtk_widget_show (widget);
	g_ptr_array_unref (devices_tmp);
}

int
main (int argc, char *argv[])
{
	g_autoptr(GtkApplication) application = NULL;
	int status = 0;

	const GOptionEntry options[] = {
		{ "device", '\0', 0, G_OPTION_ARG_STRING, NULL,
		  /* TRANSLATORS: show a device by default */
		  N_("Select this device at startup"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init ();

	/* get data from gconf */
	settings = g_settings_new (GPM_SETTINGS_SCHEMA);

	/* are we already activated? */
	application = gtk_application_new ("org.gnome.PowerStats",
					   G_APPLICATION_HANDLES_COMMAND_LINE);
	g_signal_connect (application, "activate",
			  G_CALLBACK (gpm_stats_activate_cb), NULL);
	g_signal_connect (application, "command-line",
			  G_CALLBACK (gpm_stats_commandline_cb), NULL);

	g_application_add_main_option_entries (G_APPLICATION (application), options);
	g_application_set_option_context_summary (G_APPLICATION (application),
	                                          /* TRANSLATORS: the program name */
	                                          _("Power Statistics"));

	/* add application specific icons to search path */
	gtk_icon_theme_add_search_path (gtk_icon_theme_get_for_display (gdk_display_get_default ()),
					DATADIR G_DIR_SEPARATOR_S
					"gnome-power-manager" G_DIR_SEPARATOR_S
					"icons");

	/* run */
	status = g_application_run (G_APPLICATION (application), argc, argv);

	if (client != NULL)
		g_object_unref (client);
	if (devices != NULL)
		g_ptr_array_unref (devices);
	g_object_unref (settings);
	return status;
}
