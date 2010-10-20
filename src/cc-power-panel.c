/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libupower-glib/upower.h>

#include "egg-debug.h"
#include "egg-console-kit.h"

#include "gpm-common.h"
#include "gpm-brightness.h"

#include "cc-power-panel.h"

struct _CcPowerPanelPrivate {
	UpClient		*client;
	GtkBuilder		*builder;
	gboolean		 has_batteries;
	gboolean		 has_lcd;
	gboolean		 has_ups;
	gboolean		 has_button_lid;
	gboolean		 has_button_suspend;
	gboolean		 can_shutdown;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	GSettings		*settings;
	EggConsoleKit		*console;
};

G_DEFINE_DYNAMIC_TYPE (CcPowerPanel, cc_power_panel, CC_TYPE_PANEL)

static void cc_power_panel_finalize (GObject *object);

#define CC_POWER_PREFS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_POWER_PANEL, CcPowerPanelPrivate))

/**
 * cc_power_panel_help_cb:
 **/
static void
cc_power_panel_help_cb (GtkWidget *widget, CcPowerPanel *panel)
{
	gpm_help_display ("preferences");
}

/**
 * cc_power_panel_format_percentage_cb:
 **/
static gchar *
cc_power_panel_format_percentage_cb (GtkScale *scale, gdouble value)
{
	return g_strdup_printf ("%.0f%%", value * 100.0f);
}

/**
 * cc_power_panel_action_combo_changed_cb:
 **/
static void
cc_power_panel_action_combo_changed_cb (GtkWidget *widget, CcPowerPanel *panel)
{
	GpmActionPolicy policy;
	const GpmActionPolicy *actions;
	const gchar *gpm_pref_key;
	guint active;

	actions = (const GpmActionPolicy *) g_object_get_data (G_OBJECT (widget), "actions");
	gpm_pref_key = (const gchar *) g_object_get_data (G_OBJECT (widget), "settings_key");

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	policy = actions[active];
	g_settings_set_enum (panel->priv->settings, gpm_pref_key, policy);
}

/**
 * cc_power_panel_action_time_changed_cb:
 **/
static void
cc_power_panel_action_time_changed_cb (GtkWidget *widget, CcPowerPanel *panel)
{
	guint value;
	const GArray *values;
	const gchar *gpm_pref_key;
	guint active;

	values = (const GArray *) g_object_get_data (G_OBJECT (widget), "values");
	gpm_pref_key = (const gchar *) g_object_get_data (G_OBJECT (widget), "settings_key");

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	value = g_array_index (values, gint, active);

	g_debug ("Changing %s to %i", gpm_pref_key, value);
	g_settings_set_int (panel->priv->settings, gpm_pref_key, value);
}

/**
 * cc_power_panel_set_combo_simple_text:
 **/
static void
cc_power_panel_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *cell;
	GtkListStore *store;

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
					"text", 0,
					NULL);
}

/**
 * cc_power_panel_actions_destroy_cb:
 **/
static void
cc_power_panel_actions_destroy_cb (GpmActionPolicy *array)
{
	g_free (array);
}

/**
 * cc_power_panel_setup_action_combo:
 **/
static void
cc_power_panel_setup_action_combo (CcPowerPanel *panel, const gchar *widget_name,
			      const gchar *gpm_pref_key, const GpmActionPolicy *actions)
{
	gint i;
	gboolean is_writable;
	GtkWidget *widget;
	GpmActionPolicy policy;
	GpmActionPolicy	value;
	GPtrArray *array;
	GpmActionPolicy *actions_added;

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, widget_name));
	cc_power_panel_set_combo_simple_text (widget);

	value = g_settings_get_enum (panel->priv->settings, gpm_pref_key);
	is_writable = g_settings_is_writable (panel->priv->settings, gpm_pref_key);

	gtk_widget_set_sensitive (widget, is_writable);

	array = g_ptr_array_new ();
	g_object_set_data (G_OBJECT (widget), "settings_key", (gpointer) gpm_pref_key);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_power_panel_action_combo_changed_cb), panel);

	for (i=0; actions[i] != -1; i++) {
		policy = actions[i];
		if (policy == GPM_ACTION_POLICY_SHUTDOWN && !panel->priv->can_shutdown) {
			g_debug ("Cannot add option, as cannot shutdown.");
		} else if (policy == GPM_ACTION_POLICY_SHUTDOWN && panel->priv->can_shutdown) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("Shutdown"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_SUSPEND && !panel->priv->can_suspend) {
			g_debug ("Cannot add option, as cannot suspend.");
		} else if (policy == GPM_ACTION_POLICY_HIBERNATE && !panel->priv->can_hibernate) {
			g_debug ("Cannot add option, as cannot hibernate.");
		} else if (policy == GPM_ACTION_POLICY_SUSPEND && panel->priv->can_suspend) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("Suspend"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_HIBERNATE && panel->priv->can_hibernate) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("Hibernate"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_BLANK) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("Blank screen"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_INTERACTIVE) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("Ask me"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_NOTHING) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("Do nothing"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else {
			g_warning ("Unknown action read from settings: %i", policy);
		}
	}

	/* save as array _only_ the actions we could add */
	actions_added = g_new0 (GpmActionPolicy, array->len+1);
	for (i=0; i<array->len; i++)
		actions_added[i] = GPOINTER_TO_INT (g_ptr_array_index (array, i));
	actions_added[i] = -1;

	g_object_set_data_full (G_OBJECT (widget), "actions", (gpointer) actions_added, (GDestroyNotify) cc_power_panel_actions_destroy_cb);

	/* set what we have in GConf */
	for (i=0; actions_added[i] != -1; i++) {
		policy = actions_added[i];
		if (value == policy)
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
	}

	g_ptr_array_unref (array);
}

/**
 * cc_power_panel_delete_array:
 **/
static void
cc_power_panel_delete_array (GArray *array)
{
	g_array_free (array, TRUE);
}

/**
 * cc_power_panel_setup_time_combo:
 **/
static void
cc_power_panel_setup_time_combo (CcPowerPanel *panel, const gchar *widget_name,
			    const gchar *gpm_pref_key, const gint *values_in)
{
	guint value;
	gchar *text;
	guint i;
	gint values_in_len;
	gint loop_value;
	gboolean is_writable;
	gboolean found_value = FALSE;
	gint found_index = -1;
	GtkWidget *widget;
	GArray *values;

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, widget_name));
	cc_power_panel_set_combo_simple_text (widget);

	value = g_settings_get_int (panel->priv->settings, gpm_pref_key);
	is_writable = g_settings_is_writable (panel->priv->settings, gpm_pref_key);
	gtk_widget_set_sensitive (widget, is_writable);

	/* Search values to see if current value is already listed.  If not,
	   we will add it ourselves. */
	for (i=0; values_in[i] != -1; i++) {
		if (value == values_in[i])
			found_value = TRUE;
		if (found_index == -1 && (values_in[i] == 0 || value < values_in[i]))
			found_index = i;
	}
	values_in_len = i + 1;
	if (value == 0) /* 'Never' should always be last */
		found_index = values_in_len - 1;

	/* Build array of values */
	values = g_array_sized_new (FALSE, FALSE, sizeof (gint),
	                            values_in_len + (found_value ? 0 : 1));
	g_array_append_vals (values, values_in, values_in_len);
	if (!found_value)
		g_array_insert_val (values, found_index, value);

	g_object_set_data (G_OBJECT (widget), "settings_key", (gpointer) gpm_pref_key);
	g_object_set_data_full (G_OBJECT (widget), "values", (gpointer) values,
	                        (GDestroyNotify) cc_power_panel_delete_array);

	/* add each time */
	for (i=0; g_array_index (values, gint, i) != -1; i++) {

		loop_value = g_array_index (values, gint, i);
 
		/* get translation for number of seconds */
		if (loop_value != 0) {
			text = gpm_get_timestring (loop_value);
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), text);
			g_free (text);
		} else {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("Never"));
		}

		/* matches, so set default */
		if (value == loop_value)
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
	}

	/* connect after set */
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_power_panel_action_time_changed_cb), panel);
}

/**
 * cc_power_panel_setup_ac:
 **/
static void
cc_power_panel_setup_ac (CcPowerPanel *panel)
{
	GtkWidget *widget;
	const GpmActionPolicy button_lid_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_BLANK,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};

	static const gint computer_times[] =
		{10*60,
		 30*60,
		 1*60*60,
		 2*60*60,
		 0, /* never */
		 -1};
	static const gint display_times[] =
		{1*60,
		 5*60,
		 10*60,
		 30*60,
		 1*60*60,
		 0, /* never */
		 -1};

	cc_power_panel_setup_time_combo (panel, "combobox_ac_computer",
				    GPM_SETTINGS_SLEEP_COMPUTER_AC,
				    computer_times);
	cc_power_panel_setup_time_combo (panel, "combobox_ac_display",
				    GPM_SETTINGS_SLEEP_DISPLAY_AC,
				    display_times);

	cc_power_panel_setup_action_combo (panel, "combobox_ac_lid",
				      GPM_SETTINGS_BUTTON_LID_AC,
				      button_lid_actions);

	/* setup brightness slider */
//	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_ac_brightness"));
//	g_settings_bind (panel->priv->settings, GPM_SETTINGS_BRIGHTNESS_AC,
//			 widget, "fill-level",
//			 G_SETTINGS_BIND_DEFAULT);
if(0)	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (cc_power_panel_format_percentage_cb), NULL);

	/* set up the checkboxes */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_ac_display_dim"));
	g_settings_bind (panel->priv->settings, GPM_SETTINGS_IDLE_DIM_AC,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_ac_spindown"));
	g_settings_bind (panel->priv->settings, GPM_SETTINGS_SPINDOWN_ENABLE_AC,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);

	if (panel->priv->has_button_lid == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hbox_ac_lid"));
		gtk_widget_hide_all (widget);
	}
	if (panel->priv->has_lcd == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hbox_ac_brightness"));
		gtk_widget_hide_all (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_ac_display_dim"));
		gtk_widget_hide_all (widget);
	}
}

/**
 * cc_power_panel_setup_battery:
 **/
static void
cc_power_panel_setup_battery (CcPowerPanel *panel)
{
	GtkWidget *widget;
	GtkNotebook *notebook;
	gint page;

	const GpmActionPolicy button_lid_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_BLANK,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};
	const GpmActionPolicy battery_critical_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};

	static const gint computer_times[] =
		{10*60,
		 30*60,
		 1*60*60,
		 2*60*60,
		 0, /* never */
		 -1};
	static const gint display_times[] =
		{1*60,
		 5*60,
		 10*60,
		 30*60,
		 1*60*60,
		 0, /* never */
		 -1};

	cc_power_panel_setup_time_combo (panel, "combobox_battery_computer",
				    GPM_SETTINGS_SLEEP_COMPUTER_BATT,
				    computer_times);
	cc_power_panel_setup_time_combo (panel, "combobox_battery_display",
				    GPM_SETTINGS_SLEEP_DISPLAY_BATT,
				    display_times);

	if (panel->priv->has_batteries == FALSE) {
		notebook = GTK_NOTEBOOK (gtk_builder_get_object (panel->priv->builder, "notebook_preferences"));
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "vbox_battery"));
		page = gtk_notebook_page_num (notebook, GTK_WIDGET (widget));
		gtk_notebook_remove_page (notebook, page);
		return;
	}

	cc_power_panel_setup_action_combo (panel, "combobox_battery_lid",
				      GPM_SETTINGS_BUTTON_LID_BATT,
				      button_lid_actions);
	cc_power_panel_setup_action_combo (panel, "combobox_battery_critical",
				      GPM_SETTINGS_ACTION_CRITICAL_BATT,
				      battery_critical_actions);

	/* set up the checkboxes */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_battery_display_reduce"));
	g_settings_bind (panel->priv->settings, GPM_SETTINGS_BACKLIGHT_BATTERY_REDUCE,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_battery_display_dim"));
	g_settings_bind (panel->priv->settings, GPM_SETTINGS_IDLE_DIM_BATT,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_battery_spindown"));
	g_settings_bind (panel->priv->settings, GPM_SETTINGS_SPINDOWN_ENABLE_BATT,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);

	if (panel->priv->has_button_lid == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hbox_battery_lid"));
		gtk_widget_hide_all (widget);
	}
	if (panel->priv->has_lcd == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_battery_display_dim"));
		gtk_widget_hide_all (widget);
	}
}

/**
 * cc_power_panel_setup_ups:
 **/
static void
cc_power_panel_setup_ups (CcPowerPanel *panel)
{
	GtkWidget *widget;
	GtkNotebook *notebook;
	gint page;

	const GpmActionPolicy ups_low_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};

	static const gint computer_times[] =
		{10*60,
		 30*60,
		 1*60*60,
		 2*60*60,
		 0, /* never */
		 -1};
	static const gint display_times[] =
		{1*60,
		 5*60,
		 10*60,
		 30*60,
		 1*60*60,
		 0, /* never */
		 -1};

	cc_power_panel_setup_time_combo (panel, "combobox_ups_computer",
				    GPM_SETTINGS_SLEEP_COMPUTER_UPS,
				    computer_times);
	cc_power_panel_setup_time_combo (panel, "combobox_ups_display",
				    GPM_SETTINGS_SLEEP_DISPLAY_UPS,
				    display_times);

	if (panel->priv->has_ups == FALSE) {
		notebook = GTK_NOTEBOOK (gtk_builder_get_object (panel->priv->builder, "notebook_preferences"));
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "vbox_ups"));
		page = gtk_notebook_page_num (notebook, GTK_WIDGET (widget));
		gtk_notebook_remove_page (notebook, page);
		return;
	}

	cc_power_panel_setup_action_combo (panel, "combobox_ups_low",
				      GPM_SETTINGS_ACTION_LOW_UPS,
				      ups_low_actions);
	cc_power_panel_setup_action_combo (panel, "combobox_ups_critical",
				      GPM_SETTINGS_ACTION_CRITICAL_UPS,
				      ups_low_actions);
}

/**
 * cc_power_panel_setup_general:
 **/
static void
cc_power_panel_setup_general (CcPowerPanel *panel)
{
	GtkWidget *widget;
	const GpmActionPolicy power_button_actions[] =
				{GPM_ACTION_POLICY_INTERACTIVE,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};
	const GpmActionPolicy suspend_button_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 -1};

	cc_power_panel_setup_action_combo (panel, "combobox_general_power",
				      GPM_SETTINGS_BUTTON_POWER,
				      power_button_actions);
	cc_power_panel_setup_action_combo (panel, "combobox_general_suspend",
				      GPM_SETTINGS_BUTTON_SUSPEND,
				      suspend_button_actions);

	if (panel->priv->has_button_suspend == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hbox_general_suspend"));
		gtk_widget_hide_all (widget);
	}
}

#ifdef HAVE_GCONF_DEFAULTS
/**
 * cc_power_panel_set_defaults_cb:
 **/
static void
cc_power_panel_set_defaults_cb (GtkWidget *widget, CcPowerPanel *panel)
{
	DBusGProxy *proxy;
	DBusGConnection *connection;
	GError *error = NULL;
	const gchar *keys[5] = {
		"/apps/gnome-power-manager/actions",
		"/apps/gnome-power-manager/ui",
		"/apps/gnome-power-manager/buttons",
		"/apps/gnome-power-manager/backlight",
		"/apps/gnome-power-manager/timeout"
	};

	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		g_warning ("failed to get system bus connection: %s", error->message);
		g_error_free (error);
		return;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.gnome.GConf.Defaults",
					   "/",
					   "org.gnome.GConf.Defaults");
	if (proxy == NULL) {
		g_warning ("Cannot connect to defaults mechanism");
		return;
	}

	dbus_g_proxy_call (proxy, "SetSystem", &error,
			   G_TYPE_STRV, keys,
			   G_TYPE_STRV, NULL,
			   G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref (proxy);
}
#endif

static void
cc_power_panel_class_init (CcPowerPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (CcPowerPanelPrivate));
	object_class->finalize = cc_power_panel_finalize;
}

static void
cc_power_panel_class_finalize (CcPowerPanelClass *klass)
{
}

static void
cc_power_panel_finalize (GObject *object)
{
	CcPowerPanel *panel = CC_POWER_PANEL (object);

	g_object_unref (panel->priv->settings);
	g_object_unref (panel->priv->client);
	g_object_unref (panel->priv->console);

	G_OBJECT_CLASS (cc_power_panel_parent_class)->finalize (object);
}

static void
cc_power_panel_init (CcPowerPanel *panel)
{
	GtkWidget *main_window;
	GtkWidget *widget;
	guint retval;
	GError *error = NULL;
	GPtrArray *devices = NULL;
	UpDevice *device;
	UpDeviceKind kind;
	GpmBrightness *brightness;
	gboolean ret;
	guint i;

	panel->priv = CC_POWER_PREFS_GET_PRIVATE (panel);

	panel->priv->client = up_client_new ();
	panel->priv->console = egg_console_kit_new ();
	panel->priv->settings = g_settings_new (GPM_SETTINGS_SCHEMA);

	/* are we allowed to shutdown? */
	panel->priv->can_shutdown = TRUE;
	egg_console_kit_can_stop (panel->priv->console, &panel->priv->can_shutdown, NULL);

	/* get values from UpClient */
	panel->priv->can_suspend = up_client_get_can_suspend (panel->priv->client);
	panel->priv->can_hibernate = up_client_get_can_hibernate (panel->priv->client);
#if UP_CHECK_VERSION(0,9,2)
	panel->priv->has_button_lid = up_client_get_lid_is_present (panel->priv->client);
#else
	g_object_get (panel->priv->client,
		      "lid-is-present", &panel->priv->has_button_lid,
		      NULL);
#endif
	panel->priv->has_button_suspend = TRUE;

	/* find if we have brightness hardware */
	brightness = gpm_brightness_new ();
	panel->priv->has_lcd = gpm_brightness_has_hw (brightness);
	g_object_unref (brightness);

	/* get device list */
	ret = up_client_enumerate_devices_sync (panel->priv->client, NULL, &error);
	if (!ret) {
		g_warning ("failed to get device list: %s", error->message);
		g_error_free (error);
	}

	devices = up_client_get_devices (panel->priv->client);
	for (i=0; i<devices->len; i++) {
		device = g_ptr_array_index (devices, i);
		//kind = up_device_get_kind (device);
		g_object_get (device,
			      "kind", &kind,
			      NULL);
		if (kind == UP_DEVICE_KIND_BATTERY)
			panel->priv->has_batteries = TRUE;
		if (kind == UP_DEVICE_KIND_UPS)
			panel->priv->has_ups = TRUE;
	}
	g_ptr_array_unref (devices);

	panel->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (panel->priv->builder, GPM_DATA "/gpm-prefs.ui", &error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_power_panel_help_cb), panel);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_defaults"));
#ifdef HAVE_GCONF_DEFAULTS
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_power_panel_set_defaults_cb), panel);
#else
	gtk_widget_hide (widget);
#endif

	cc_power_panel_setup_ac (panel);
	cc_power_panel_setup_battery (panel);
	cc_power_panel_setup_ups (panel);
	cc_power_panel_setup_general (panel);

out:
	main_window = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_preferences"));
//	gtk_widget_show (main_window);
	widget = gtk_dialog_get_content_area (GTK_DIALOG (main_window));
	gtk_widget_unparent (widget);

	gtk_container_add (GTK_CONTAINER (panel), widget);
}

void
cc_power_panel_register (GIOModule *module)
{
	cc_power_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_POWER_PANEL,
					"power", 0);
}

/* GIO extension stuff */
void
g_io_module_load (GIOModule *module)
{
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* register the panel */
	cc_power_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}
