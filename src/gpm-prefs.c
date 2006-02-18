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

#include <popt.h>

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

static GConfEnumStringPair icon_policy_enum_map [] = {
       { GPM_ICON_POLICY_ALWAYS,       "always"   },
       { GPM_ICON_POLICY_CHARGE,       "charge"   },
       { GPM_ICON_POLICY_CRITICAL,     "critical" },
       { GPM_ICON_POLICY_NEVER,        "never"    },
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
#define	GPM_DBUS_INTERFACE	        "org.gnome.PowerManager"

/* If sleep time in a slider is set to 61 it is considered as never sleep */
const int NEVER_TIME_ON_SLIDER = 61;


static gboolean
gpm_dbus_method_bool (const char *method)
{
	DBusGConnection *connection;
	DBusGProxy      *proxy;
	GError          *error;
	gboolean         value;

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

	value = gpm_hal_num_devices_of_capability ("battery") > 0;

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
gpm_has_button_sleep (void)
{
	gboolean value;

	value = gpm_hal_num_devices_of_capability_with_value  ("button", "button.type", "sleep") > 0;

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
			 int        icon_policy)
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
				gdouble value)
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
				   char *gpm_pref_key)
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
gpm_prefs_setup_sleep_slider (GladeXML *dialog,
			      char *widget_name,
			      char *gpm_pref_key)
{
	GtkWidget *widget;
	gint value;
	GConfClient *client;

	widget = glade_xml_get_widget (dialog, widget_name);
	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_time_cb), NULL);

	client = gconf_client_get_default ();
	value = gconf_client_get_int (client, gpm_pref_key, NULL);
	g_object_unref (client);

	if (value == 0) {
		value = NEVER_TIME_ON_SLIDER;
	} else {
		/* policy is in seconds, slider is in minutes */
		value /= 60;
	}

	gtk_range_set_value (GTK_RANGE (widget), value);

	/* don't connect the callback until we have set the slider */
	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (gpm_prefs_sleep_slider_changed_cb), gpm_pref_key);

	return widget;
}

static void
gpm_prefs_brightness_slider_changed_cb (GtkRange *range,
					char *gpm_pref_key)
{
	gdouble value;
	GConfClient *client;

	value = gtk_range_get_value (range);

	client = gconf_client_get_default ();
	gconf_client_set_int (client, gpm_pref_key, (gint) value, NULL);
	g_object_unref (client);
}

static GtkWidget *
gpm_prefs_setup_brightness_slider (GladeXML *dialog,
				   char *widget_name,
				   char *gpm_pref_key)
{
	GtkWidget *widget;
	int value;
	GConfClient *client;

	widget = glade_xml_get_widget (dialog, widget_name);

	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_brightness_cb), NULL);

	client = gconf_client_get_default ();
	value = gconf_client_get_int (client, gpm_pref_key, NULL);
	g_object_unref (client);

	gtk_range_set_value (GTK_RANGE (widget), value);

	/* don't connect the callback until we have set the slider */
	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (gpm_prefs_brightness_slider_changed_cb), gpm_pref_key);

	return widget;
}

static void
gpm_prefs_action_combo_changed_cb (GtkWidget *widget,
				   char *gpm_pref_key)
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
gpm_prefs_setup_action_combo (GtkWidget *widget,
			      char *gpm_pref_key,
			      const char **actions)
{
	char *value;
	int i = 0;
	int n_added = 0;
	gboolean can_suspend;
	gboolean can_hibernate;
	GConfClient *client;

	can_suspend = gpm_can_suspend ();
	can_hibernate = gpm_can_hibernate ();

	client = gconf_client_get_default ();
	value = gconf_client_get_string (client, gpm_pref_key, NULL);
	g_object_unref (client);

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
			g_error ("Unknown action : %s", actions[i]);
		}

		if (strcmp (value, actions[i]) == 0)
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), n_added - 1);
		i++;
	}
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_action_combo_changed_cb), gpm_pref_key);

	g_free (value);
}

static void
setup_battery_sliders (GladeXML *xml, gboolean has_batteries)
{
	GtkWidget   *label_batteries_display;
	GtkWidget   *slider_batteries_display;
	GtkWidget   *label_batteries_brightness;
	GtkWidget   *slider_batteries_brightness;
	gboolean     can_set_brightness;

	if (! has_batteries) {
		/* no point */
		return;
	}

	/* Sleep time on batteries */
	gpm_prefs_setup_sleep_slider (xml, "hscale_batteries_computer", GPM_PREF_BATTERY_SLEEP_COMPUTER);

	/* Sleep time for display when on batteries */

	label_batteries_display = glade_xml_get_widget (xml, "label_batteries_display");
	slider_batteries_display = gpm_prefs_setup_sleep_slider (xml, "hscale_batteries_display",
								 GPM_PREF_BATTERY_SLEEP_DISPLAY);

	/* Display brightness when on batteries */

	label_batteries_brightness = glade_xml_get_widget (xml, "label_batteries_brightness");
	slider_batteries_brightness = gpm_prefs_setup_brightness_slider (xml, "hscale_batteries_brightness",
									 GPM_PREF_BATTERY_BRIGHTNESS);
	can_set_brightness = gpm_has_lcd ();
	if (! can_set_brightness) {
		gtk_widget_hide_all (label_batteries_brightness);
		gtk_widget_hide_all (slider_batteries_brightness);
	}

}

static void
setup_ac_sliders (GladeXML *xml, gboolean has_batteries)
{
	GtkWidget   *widget;
	GtkWidget   *label_ac_brightness;
	GtkWidget   *slider_ac_brightness;
	gboolean     can_set_brightness;

	/* Sleep time on AC */
	gpm_prefs_setup_sleep_slider (xml, "hscale_ac_computer", GPM_PREF_AC_SLEEP_COMPUTER);

	/* Sleep time for display on AC */
	gpm_prefs_setup_sleep_slider (xml, "hscale_ac_display", GPM_PREF_AC_SLEEP_DISPLAY);

	/* Display brightness when on AC */
	label_ac_brightness = glade_xml_get_widget (xml, "label_ac_brightness");
	slider_ac_brightness = gpm_prefs_setup_brightness_slider (xml, "hscale_ac_brightness",
								  GPM_PREF_AC_BRIGHTNESS);

	can_set_brightness = gpm_has_lcd ();
	if (! can_set_brightness) {
		gtk_widget_hide_all (label_ac_brightness);
		gtk_widget_hide_all (slider_ac_brightness);
	}

	/* when we have no batteries, we just want the title to be "Configuration" */
	if (! has_batteries) {
		char *str;
		widget = glade_xml_get_widget (xml, "label_frame_ac");
		str = g_strdup_printf ("<b>%s</b>", _("Configuration"));
		gtk_label_set_markup (GTK_LABEL (widget), str);
		g_free (str);
	}
}

static void
setup_power_buttons (GladeXML *xml, gboolean has_suspend_button)
{
	GtkWidget    *label_button_suspend;
	GtkWidget    *combo_button_suspend;
	GtkWidget    *frame_options_actions;
	const char   *button_suspend_actions[] = {ACTION_NOTHING, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	/* Button Suspend Combo Box */
	label_button_suspend = glade_xml_get_widget (xml, "label_button_suspend");
	combo_button_suspend = glade_xml_get_widget (xml, "combobox_button_suspend");
	frame_options_actions = glade_xml_get_widget (xml, "frame_options_actions");

	if (has_suspend_button) {
		gpm_prefs_setup_action_combo (combo_button_suspend,
					      GPM_PREF_BUTTON_SUSPEND,
					      button_suspend_actions);
	} else {
		gtk_widget_hide_all (label_button_suspend);
		gtk_widget_hide_all (combo_button_suspend);
		/* as the suspend button is the only think in the
		   action frame, remove if empty */
		gtk_widget_hide_all (frame_options_actions);
	}
}

static void
setup_sleep_type (GladeXML *xml)
{
	GtkWidget    *label_sleep_type;
	GtkWidget    *combo_sleep_type;
	const char   *sleep_type_actions[] = {ACTION_NOTHING, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	/* Sleep Type Combo Box */
	label_sleep_type = glade_xml_get_widget (xml, "label_sleep_type");
	combo_sleep_type = glade_xml_get_widget (xml, "combobox_sleep_type");

	gpm_prefs_setup_action_combo (combo_sleep_type,
				      GPM_PREF_SLEEP_TYPE,
				      sleep_type_actions);
}

static void
setup_ac_actions (GladeXML *xml)
{
	GtkWidget    *label_button_lid;
	GtkWidget    *combo_button_lid;
	GtkWidget    *vbox_ac_actions;
	gboolean      has_lid_button;
	const char   *button_lid_actions[] = {ACTION_BLANK, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	label_button_lid = glade_xml_get_widget (xml, "label_ac_button_lid");
	combo_button_lid = glade_xml_get_widget (xml, "combobox_ac_lid_close");
	vbox_ac_actions = glade_xml_get_widget (xml, "vbox_ac_actions");

	has_lid_button = gpm_has_button_lid ();

	if (has_lid_button) {
		gpm_prefs_setup_action_combo (combo_button_lid,
					      GPM_PREF_AC_BUTTON_LID,
					      button_lid_actions);
	} else {
		gtk_widget_hide_all (label_button_lid);
		gtk_widget_hide_all (combo_button_lid);
		/* there is nothing else in this action box apart from
		   lid event, so hide the whole box */
		gtk_widget_hide_all (vbox_ac_actions);
	}
}

static void
setup_battery_actions (GladeXML *xml, gboolean has_batteries)
{
	GtkWidget    *label_button_lid;
	GtkWidget    *combo_button_lid;
	GtkWidget    *label_battery_critical;
	GtkWidget    *combo_battery_critical;
	const char   *button_lid_actions[] = {ACTION_BLANK, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};
	const char   *battery_critical_actions[] = {ACTION_NOTHING, ACTION_HIBERNATE, ACTION_SHUTDOWN, NULL};
	gboolean      has_lid_button;

	if (! has_batteries) {
		/* no point */
		return;
	}

	/* Button Lid Combo Box */
	label_button_lid = glade_xml_get_widget (xml, "label_battery_button_lid");
	combo_button_lid = glade_xml_get_widget (xml, "combobox_battery_lid_close");

	has_lid_button = gpm_has_button_lid ();

	if (has_lid_button) {
		gpm_prefs_setup_action_combo (combo_button_lid,
					      GPM_PREF_BATTERY_BUTTON_LID,
					      button_lid_actions);
	} else {
		gtk_widget_hide_all (label_button_lid);
		gtk_widget_hide_all (combo_button_lid);
	}
	/* FIXME: also need lid lock */

	label_battery_critical = glade_xml_get_widget (xml, "label_battery_critical_action");
	combo_battery_critical = glade_xml_get_widget (xml, "combobox_battery_critical");

	gpm_prefs_setup_action_combo (combo_battery_critical,
				      GPM_PREF_BATTERY_CRITICAL,
				      battery_critical_actions);
}

static void
setup_icon_policy (GladeXML *xml, gboolean has_batteries)
{
	GConfClient *client;
	char        *icon_policy_str;
	int          icon_policy;
	GtkWidget   *radiobutton_icon_always;
	GtkWidget   *radiobutton_icon_charge;
	GtkWidget   *radiobutton_icon_critical;
	GtkWidget   *radiobutton_icon_never;

	client = gconf_client_get_default ();

	icon_policy_str = gconf_client_get_string (client, GPM_PREF_ICON_POLICY, NULL);
	icon_policy = GPM_ICON_POLICY_ALWAYS;
	gconf_string_to_enum (icon_policy_enum_map, icon_policy_str, &icon_policy);
	g_free (icon_policy_str);

	radiobutton_icon_always = glade_xml_get_widget (xml, "radiobutton_icon_always");
	radiobutton_icon_charge = glade_xml_get_widget (xml, "radiobutton_icon_charge");
	radiobutton_icon_critical = glade_xml_get_widget (xml, "radiobutton_icon_critical");
	radiobutton_icon_never = glade_xml_get_widget (xml, "radiobutton_icon_never");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_always),
				      icon_policy == GPM_ICON_POLICY_ALWAYS);
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
	GtkWidget *main_window;
	GtkWidget *widget;
	GladeXML *glade_xml;

	glade_xml = glade_xml_new (GPM_DATA "/gpm-prefs.glade", NULL, NULL);

	main_window = glade_xml_get_widget (glade_xml, "window_preferences");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW (main_window), "gnome-dev-battery");

	/* Get the main window quit */
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gtk_main_quit), NULL);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gtk_main_quit), NULL);

	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_help_cb), NULL);


	gboolean      has_batteries;
	gboolean      has_suspend_button;

	has_suspend_button = gpm_has_button_sleep ();
	has_batteries = gpm_has_batteries ();

	GtkWidget    *label_sleep_type;
	GtkWidget    *label_button_suspend;
	GtkSizeGroup *size_group;

	setup_icon_policy (glade_xml, has_batteries);
	setup_ac_actions (glade_xml);
	setup_ac_sliders (glade_xml, has_batteries);
	setup_battery_actions (glade_xml, has_batteries);
	setup_battery_sliders (glade_xml, has_batteries);
	setup_sleep_type (glade_xml);
	setup_power_buttons (glade_xml, has_suspend_button);

	/* Make sure that all comboboxes get the same size by adding their
	 * labels to a GtkSizeGroup
	 */
	label_sleep_type = glade_xml_get_widget (glade_xml, "label_sleep_type");
	label_button_suspend = glade_xml_get_widget (glade_xml, "label_button_suspend");

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, label_sleep_type);
	gtk_size_group_add_widget (size_group, label_button_suspend);
	g_object_unref (G_OBJECT (size_group));

	/* if no options then disable frame as it will be empty */
	if (! has_batteries) {
		widget = glade_xml_get_widget (glade_xml, "gpm_notebook");
		gtk_notebook_remove_page (GTK_NOTEBOOK(widget), 1);
	}
	return main_window;
}

int
main (int argc, char **argv)
{
	GtkWidget *dialog;
	gboolean   verbose = FALSE;
	gint       i;

	struct poptOption options[] = {
		{ "verbose", '\0', POPT_ARG_NONE, NULL, 0,
		  N_("Show extra debugging information"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	i = 0;
	options[i++].arg = &verbose;
	verbose = FALSE;

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PARAM_HUMAN_READABLE_NAME,
			    _("Power Preferences"),
			    NULL);

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gpm_debug_init (verbose, FALSE);

	dialog = gpm_prefs_create ();
	gtk_widget_show (dialog);

	gtk_main ();

	gpm_debug_shutdown ();

	return 0;
}
