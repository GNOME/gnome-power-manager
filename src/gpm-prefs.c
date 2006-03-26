/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include <libgnomeui/gnome-ui-init.h> /* for gnome_program_init */
#include <libgnomeui/gnome-help.h> /* for gnome_help_display */

#include "gpm-common.h"
#include "gpm-hal.h"
#include "gpm-prefs.h"
#include "gpm-debug.h"
#include "gpm-stock-icons.h"

static GConfEnumStringPair icon_policy_enum_map [] = {
	{ GPM_ICON_POLICY_ALWAYS,	"always"   },
	{ GPM_ICON_POLICY_PRESENT,	"present"  },
	{ GPM_ICON_POLICY_CHARGE,	"charge"   },
	{ GPM_ICON_POLICY_CRITICAL,	"critical" },
	{ GPM_ICON_POLICY_NEVER,	"never"    },
	{ 0, NULL }
};

/* The text that should appear in the action combo boxes */
#define ACTION_SUSPEND_TEXT		_("Suspend")
#define ACTION_SHUTDOWN_TEXT		_("Shutdown")
#define ACTION_HIBERNATE_TEXT		_("Hibernate")
#define ACTION_BLANK_TEXT		_("Blank screen")
#define ACTION_NOTHING_TEXT		_("Do nothing")

#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/PowerManager"
#define	GPM_DBUS_INTERFACE		"org.gnome.PowerManager"

/* If sleep time in a slider is set to 61 it is considered as never sleep */
const int NEVER_TIME_ON_SLIDER = 61;

static gboolean
gpm_dbus_method_bool (const char *method)
{
	DBusGConnection *connection;
	DBusGProxy      *proxy;
	GError	   *error;
	gboolean	  value;

	value = FALSE;
	error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		if (error) {
			gpm_warning ("Couldn't connect to PowerManager %s", error->message);
			g_error_free (error);
		}
		return FALSE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_PATH,
					   GPM_DBUS_INTERFACE);
	if (! dbus_g_proxy_call (proxy,
				 method,
				 &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &value,
				 G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("Couldn't connect to PowerManager %s", error->message);
			g_error_free (error);
		}
		value = FALSE;
	}

	g_object_unref (proxy);

	return value;
}

static gboolean
gpm_can_suspend (void)
{
	return gpm_dbus_method_bool ("CanSuspend");
}

static gboolean
gpm_can_hibernate (void)
{
	return gpm_dbus_method_bool ("CanHibernate");
}

static gboolean
gpm_has_batteries (void)
{
	gboolean value;
	value = gpm_hal_num_devices_of_capability_with_value ("battery",
							      "battery.type",
							      "primary");
	return value;
}

static gboolean
gpm_has_ups (void)
{
	gboolean value;
	value = gpm_hal_num_devices_of_capability_with_value ("battery",
							      "battery.type",
							      "ups");
	return value;
}

static gboolean
gpm_has_lcd (void)
{
	gboolean value;

	value = gpm_hal_num_devices_of_capability ("laptop_panel") > 0;

	return value;
}

static gboolean
gpm_has_button_lid (void)
{
	gboolean value;

	value =  gpm_hal_num_devices_of_capability_with_value ("button", "button.type", "lid") > 0;

	return value;
}

static void
gpm_prefs_help_cb (GtkWidget *widget,
		   gpointer user_data)
{
	GError *error = NULL;

	gnome_help_display_with_doc_id (NULL, "gnome-power-manager",
					"gnome-power-manager.xml", NULL, &error);
	if (error != NULL) {
		gpm_warning (error->message);
		g_error_free (error);
	}
}

static void
gpm_prefs_icon_radio_cb (GtkWidget *widget,
			 int	    icon_policy)
{
	GConfClient *client;
	const char  *str;

	client = gconf_client_get_default ();

	str = gconf_enum_to_string (icon_policy_enum_map, icon_policy);
	gconf_client_set_string (client,
				 GPM_PREF_ICON_POLICY,
				 str, NULL);

	g_object_unref (client);
}

static char *
gpm_prefs_format_brightness_cb (GtkScale *scale,
				gdouble   value)
{
	return g_strdup_printf ("%.0f%%", value);
}

static char *
gpm_prefs_format_time_cb (GtkScale *scale,
			  gdouble value)
{
	char *str;

	if ((gint) value == NEVER_TIME_ON_SLIDER) {
		str = g_strdup (_("Never"));
	} else {
		str = gpm_get_timestring (value * 60);
	}

	return str;
}

static void
gpm_prefs_sleep_slider_changed_cb (GtkRange *range,
				   char     *gpm_pref_key)
{
	int value;
	GConfClient *client;

	value = (int)gtk_range_get_value (range);

	if (value == NEVER_TIME_ON_SLIDER) {
		/* power manager interprets 0 as Never */
		value = 0;
	} else {
		/* policy is in seconds, slider is in minutes */
		value *= 60;
	}

	client = gconf_client_get_default ();
	gconf_client_set_int (client, gpm_pref_key, value, NULL);
	g_object_unref (client);
}

static GtkWidget *
gpm_prefs_setup_sleep_slider (GladeXML  *xml,
			      char	*widget_name,
			      char	*gpm_pref_key)
{
	GtkWidget *widget;
	gint value;
	GConfClient *client;
	gboolean is_writable;

	widget = glade_xml_get_widget (xml, widget_name);
	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_time_cb), NULL);

	client = gconf_client_get_default ();
	value = gconf_client_get_int (client, gpm_pref_key, NULL);
	is_writable = gconf_client_key_is_writable (client, gpm_pref_key, NULL);
	g_object_unref (client);

	gtk_widget_set_sensitive (widget, is_writable);

	if (value == 0) {
		value = NEVER_TIME_ON_SLIDER;
	} else {
		/* policy is in seconds, slider is in minutes */
		value /= 60;
	}

	gtk_range_set_value (GTK_RANGE (widget), value);

	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (gpm_prefs_sleep_slider_changed_cb),
			  gpm_pref_key);

	return widget;
}

static void
gpm_prefs_brightness_slider_changed_cb (GtkRange *range,
					char	 *gpm_pref_key)
{
	gdouble value;
	GConfClient *client;

	value = gtk_range_get_value (range);

	client = gconf_client_get_default ();
	gconf_client_set_int (client, gpm_pref_key, (gint) value, NULL);
	g_object_unref (client);
}

static GtkWidget *
gpm_prefs_setup_brightness_slider (GladeXML *xml,
				   char     *widget_name,
				   char     *gpm_pref_key)
{
	GtkWidget *widget;
	int value;
	GConfClient *client;
	gboolean is_writable;

	widget = glade_xml_get_widget (xml, widget_name);

	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_brightness_cb), NULL);

	client = gconf_client_get_default ();
	value = gconf_client_get_int (client, gpm_pref_key, NULL);
	is_writable = gconf_client_key_is_writable (client, gpm_pref_key, NULL);
	g_object_unref (client);

	gtk_widget_set_sensitive (widget, is_writable);

	gtk_range_set_value (GTK_RANGE (widget), value);

	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (gpm_prefs_brightness_slider_changed_cb),
			  gpm_pref_key);
	return widget;
}

static void
gpm_prefs_action_combo_changed_cb (GtkWidget *widget,
				   char      *gpm_pref_key)
{
	char *value;
	char *action;
	GConfClient *client;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));

	if (strcmp (value, ACTION_SUSPEND_TEXT) == 0) {
		action = ACTION_SUSPEND;
	} else if (strcmp (value, ACTION_HIBERNATE_TEXT) == 0) {
		action = ACTION_HIBERNATE;
	} else if (strcmp (value, ACTION_SHUTDOWN_TEXT) == 0) {
		action = ACTION_SHUTDOWN;
	} else if (strcmp (value, ACTION_BLANK_TEXT) == 0) {
		action = ACTION_BLANK;
	} else if (strcmp (value, ACTION_NOTHING_TEXT) == 0) {
		action = ACTION_NOTHING;
	} else {
		g_assert (FALSE);
	}

	g_free (value);

	client = gconf_client_get_default ();
	gconf_client_set_string (client, gpm_pref_key, action, NULL);
	g_object_unref (client);
}

static void
gpm_prefs_setup_action_combo (GladeXML    *xml,
			      char	  *widget_name,
			      char        *gpm_pref_key,
			      const char **actions)
{
	char *value;
	int i = 0;
	int n_added = 0;
	gboolean can_suspend;
	gboolean can_hibernate;
	gboolean is_writable;
	GConfClient *client;
	GtkWidget *widget;

	widget = glade_xml_get_widget (xml, widget_name);

	can_suspend = gpm_can_suspend ();
	can_hibernate = gpm_can_hibernate ();

	client = gconf_client_get_default ();
	value = gconf_client_get_string (client, gpm_pref_key, NULL);
	is_writable = gconf_client_key_is_writable (client, gpm_pref_key, NULL);
	g_object_unref (client);

	gtk_widget_set_sensitive (widget, is_writable);

	if (! value) {
		gpm_warning ("invalid schema, please re-install");
		value = g_strdup ("nothing");
	}

	while (actions[i] != NULL) {
		if (strcmp (actions[i], ACTION_SHUTDOWN) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_SHUTDOWN_TEXT);
			n_added++;
		} else if ((strcmp (actions[i], ACTION_SUSPEND) == 0) && !can_suspend) {
			gpm_debug ("Cannot add option, as cannot suspend.");
		} else if ((strcmp (actions[i], ACTION_HIBERNATE) == 0) && !can_hibernate) {
			gpm_debug ("Cannot add option, as cannot hibernate.");
		} else if ((strcmp (actions[i], ACTION_SUSPEND) == 0) && can_suspend) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_SUSPEND_TEXT);
			n_added++;
		} else if ((strcmp (actions[i], ACTION_HIBERNATE) == 0) && can_hibernate) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_HIBERNATE_TEXT);
			n_added++;
		} else if (strcmp (actions[i], ACTION_BLANK) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_BLANK_TEXT);
			n_added++;
		} else if (strcmp (actions[i], ACTION_NOTHING) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_NOTHING_TEXT);
			n_added++;
		} else {
			gpm_critical_error ("Unknown action read from gconf: %s",
					    actions[i]);
		}

		if (strcmp (value, actions[i]) == 0)
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added - 1);
		i++;
	}
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_action_combo_changed_cb),
			  gpm_pref_key);

	g_free (value);
}

static void
gpm_prefs_checkbox_lock_cb (GtkWidget  *widget,
			    const char *gpm_pref_key)
{
	GConfClient *client;
	gboolean checked;

	client = gconf_client_get_default ();

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	gpm_debug ("Changing %s to %i", gpm_pref_key, checked);

	gconf_client_set_bool (client,
				gpm_pref_key,
				checked, NULL);

	g_object_unref (client);
}

static GtkWidget*
gpm_prefs_setup_checkbox (GladeXML  *xml,
			  char	    *widget_name,
			  char	    *gpm_pref_key)
{

	GConfClient *client;
	gboolean checked;
	GtkWidget *widget;

	gpm_debug ("Setting up %s", gpm_pref_key);

	widget = glade_xml_get_widget (xml, widget_name);

	client = gconf_client_get_default ();

	checked = gconf_client_get_bool (client, gpm_pref_key, NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);

	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_checkbox_lock_cb),
			  (gpointer) gpm_pref_key);

	g_object_unref (client);

	return widget;
}

static void
setup_battery_sliders (GladeXML *xml)
{
	GtkWidget   *vbox_battery_brightness;
	gboolean     can_set_brightness;

	/* Sleep time on batteries */
	gpm_prefs_setup_sleep_slider (xml, "hscale_battery_computer", GPM_PREF_BATTERY_SLEEP_COMPUTER);

	/* Sleep time for display when on batteries */
	gpm_prefs_setup_sleep_slider (xml, "hscale_battery_display", GPM_PREF_BATTERY_SLEEP_DISPLAY);

	/* Display brightness when on batteries */
	gpm_prefs_setup_brightness_slider (xml, "hscale_battery_brightness", GPM_PREF_BATTERY_BRIGHTNESS);

	can_set_brightness = gpm_has_lcd ();
	if (! can_set_brightness) {
		vbox_battery_brightness = glade_xml_get_widget (xml, "vbox_battery_brightness");
		gtk_widget_hide_all (vbox_battery_brightness);
	}
}

static void
setup_ac_sliders (GladeXML *xml)
{
	GtkWidget   *vbox_ac_brightness;
	gboolean     can_set_brightness;

	/* Sleep time on AC */
	gpm_prefs_setup_sleep_slider (xml, "hscale_ac_computer", GPM_PREF_AC_SLEEP_COMPUTER);

	/* Sleep time for display on AC */
	gpm_prefs_setup_sleep_slider (xml, "hscale_ac_display", GPM_PREF_AC_SLEEP_DISPLAY);

	/* Display brightness when on AC */
	gpm_prefs_setup_brightness_slider (xml, "hscale_ac_brightness", GPM_PREF_AC_BRIGHTNESS);

	can_set_brightness = gpm_has_lcd ();
	if (! can_set_brightness) {
		vbox_ac_brightness = glade_xml_get_widget (xml, "vbox_ac_brightness");
		gtk_widget_hide_all (vbox_ac_brightness);
	}
}

static void
setup_sleep_type (GladeXML *xml)
{
	GtkWidget    *checkbutton_dim_idle;
	gboolean      can_set_brightness;
	const char   *sleep_type_actions[] = {ACTION_NOTHING, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	/* Sleep Type Combo Box */

	gpm_prefs_setup_action_combo (xml, "combobox_sleep_type",
				      GPM_PREF_SLEEP_TYPE,
				      sleep_type_actions);

	/* set up the "do we dim screen on idle checkbox */
	checkbutton_dim_idle = gpm_prefs_setup_checkbox (xml, "checkbutton_dim_idle",
				  			 GPM_PREF_IDLE_DIM_SCREEN);

	can_set_brightness = gpm_has_lcd ();
	if (! can_set_brightness) {
		gtk_widget_hide_all (checkbutton_dim_idle);
	}
}

static void
setup_ac_actions (GladeXML *xml)
{
	GtkWidget    *vbox_ac_actions;
	gboolean      has_lid_button;
	const char   *button_lid_actions[] = {ACTION_BLANK, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	gpm_prefs_setup_action_combo (xml, "combobox_ac_lid_close",
				      GPM_PREF_AC_BUTTON_LID,
				      button_lid_actions);

	has_lid_button = gpm_has_button_lid ();

	if (! has_lid_button) {
		vbox_ac_actions = glade_xml_get_widget (xml, "vbox_ac_actions");
		/* there is nothing else in this action box apart from
		   lid event, so hide the whole box */
		gtk_widget_hide_all (vbox_ac_actions);
	}
}

static void
setup_battery_actions (GladeXML *xml)
{
	GtkWidget    *label_button_lid;
	GtkWidget    *combo_button_lid;
	const char   *button_lid_actions[] = {ACTION_BLANK, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};
	const char   *battery_critical_actions[] = {ACTION_NOTHING, ACTION_SUSPEND, ACTION_HIBERNATE, ACTION_SHUTDOWN, NULL};
	gboolean      has_lid_button;

	/* Button Lid Combo Box */
	gpm_prefs_setup_action_combo (xml, "combobox_battery_lid_close",
				      GPM_PREF_BATTERY_BUTTON_LID,
				      button_lid_actions);

	has_lid_button = gpm_has_button_lid ();
	if (! has_lid_button) {
		label_button_lid = glade_xml_get_widget (xml, "label_battery_button_lid");
		combo_button_lid = glade_xml_get_widget (xml, "combobox_battery_lid_close");
		gtk_widget_hide_all (label_button_lid);
		gtk_widget_hide_all (combo_button_lid);
	}
	/* FIXME: also need lid lock */

	gpm_prefs_setup_action_combo (xml, "combobox_battery_critical",
				      GPM_PREF_BATTERY_CRITICAL,
				      battery_critical_actions);
}

static void
setup_icon_policy (GladeXML *xml)
{
	GConfClient *client;
	char	 *icon_policy_str;
	int	   icon_policy;
	GtkWidget   *radiobutton_icon_always;
	GtkWidget   *radiobutton_icon_present;
	GtkWidget   *radiobutton_icon_charge;
	GtkWidget   *radiobutton_icon_critical;
	GtkWidget   *radiobutton_icon_never;
	gboolean     is_writable;
	gboolean     has_batteries = gpm_has_batteries ();

	client = gconf_client_get_default ();

	icon_policy_str = gconf_client_get_string (client, GPM_PREF_ICON_POLICY, NULL);
	icon_policy = GPM_ICON_POLICY_ALWAYS;
	gconf_string_to_enum (icon_policy_enum_map, icon_policy_str, &icon_policy);
	g_free (icon_policy_str);

	radiobutton_icon_always = glade_xml_get_widget (xml, "radiobutton_icon_always");
	radiobutton_icon_present = glade_xml_get_widget (xml, "radiobutton_icon_present");
	radiobutton_icon_charge = glade_xml_get_widget (xml, "radiobutton_icon_charge");
	radiobutton_icon_critical = glade_xml_get_widget (xml, "radiobutton_icon_critical");
	radiobutton_icon_never = glade_xml_get_widget (xml, "radiobutton_icon_never");

	is_writable = gconf_client_key_is_writable (client, GPM_PREF_ICON_POLICY, NULL);
	gtk_widget_set_sensitive (radiobutton_icon_always, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_present, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_charge, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_critical, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_never, is_writable);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_always),
				      icon_policy == GPM_ICON_POLICY_ALWAYS);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_present),
				      icon_policy == GPM_ICON_POLICY_PRESENT);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_charge),
				      icon_policy == GPM_ICON_POLICY_CHARGE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_critical),
				      icon_policy == GPM_ICON_POLICY_CRITICAL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_never),
				      icon_policy == GPM_ICON_POLICY_NEVER);

	/* only connect the callbacks after we set the value, else the gconf
	   keys gets written to (for a split second), and the icon flickers. */
	g_signal_connect (radiobutton_icon_always, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb),
			  (gpointer)GPM_ICON_POLICY_ALWAYS);
	g_signal_connect (radiobutton_icon_present, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb),
			  (gpointer)GPM_ICON_POLICY_PRESENT);
	g_signal_connect (radiobutton_icon_charge, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb),
			  (gpointer)GPM_ICON_POLICY_CHARGE);
	g_signal_connect (radiobutton_icon_critical, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb),
			  (gpointer)GPM_ICON_POLICY_CRITICAL);
	g_signal_connect (radiobutton_icon_never, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb),
			  (gpointer)GPM_ICON_POLICY_NEVER);

	if (! has_batteries) {
		/* Hide battery radio options if we have no batteries */
		gtk_widget_hide_all (radiobutton_icon_charge);
		gtk_widget_hide_all (radiobutton_icon_critical);
	}

	g_object_unref (client);
}

static GtkWidget *
gpm_prefs_create (void)
{
	GtkWidget    *main_window;
	GtkWidget    *widget;
	GladeXML     *glade_xml;
	gboolean      present;

	glade_xml = glade_xml_new (GPM_DATA "/gpm-prefs.glade", NULL, NULL);

	main_window = glade_xml_get_widget (glade_xml, "window_preferences");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW(main_window), GPM_STOCK_APP_ICON);

	/* Get the main window quit */
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gtk_main_quit), NULL);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gtk_main_quit), NULL);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_help_cb), NULL);

	setup_icon_policy (glade_xml);
	setup_ac_actions (glade_xml);
	setup_ac_sliders (glade_xml);
	setup_battery_actions (glade_xml);
	setup_battery_sliders (glade_xml);
	setup_sleep_type (glade_xml);

	/* if no options then disable frame as it will be empty */
	present = gpm_has_batteries ();
	if (! present) {
		widget = glade_xml_get_widget (glade_xml, "gpm_notebook");
		gtk_notebook_remove_page (GTK_NOTEBOOK(widget), 1);
	}
	present = gpm_has_ups ();
	if (! present) {
		widget = glade_xml_get_widget (glade_xml, "gpm_notebook");
		gtk_notebook_remove_page (GTK_NOTEBOOK(widget), 2);
	}
	return main_window;
}

int
main (int argc, char **argv)
{
	GtkWidget *dialog;
	gboolean   verbose = FALSE;
	GOptionContext *context;
 	GnomeProgram *program;

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	context = g_option_context_new (_("GNOME Power Preferences"));

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	program = gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_GOPTION_CONTEXT, context,
			    GNOME_PARAM_HUMAN_READABLE_NAME,
			    _("Power Preferences"),
			    NULL);

	gpm_debug_init (verbose);

	dialog = gpm_prefs_create ();
	gtk_widget_show (dialog);

	gtk_main ();

	gpm_debug_shutdown ();

	g_object_unref(program);

	return 0;
}
