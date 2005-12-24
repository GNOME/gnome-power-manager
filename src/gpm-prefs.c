/*
 * Copyright (C) 2005 Richard Hughes <hughsient@gmail.com>
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <math.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <popt.h>
#include "gpm-prefs.h"
#include "gpm-hal.h"
#include "gpm-common.h"
#include "gpm-screensaver.h"
#include "gpm-dbus-client.h"
#include "gpm-dbus-common.h"

/* The text that should appear in the action combo boxes */
#define ACTION_SUSPEND_TEXT		_("Suspend")
#define ACTION_SHUTDOWN_TEXT		_("Shutdown")
#define ACTION_HIBERNATE_TEXT		_("Hibernate")
#define ACTION_NOTHING_TEXT		_("Do nothing")

/* If sleep time in a slider is set to 61 it is considered as never sleep */
const int NEVER_TIME_ON_SLIDER = 61;

static void
gpm_prefs_debug_log_ignore (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
}

gint
get_battery_time_for_percentage (gint value)
{
	/**	@bug	This is inherently buggy. Multibattery laptops break. */
	gchar **device_names = NULL;
	gchar *udi = NULL;
	gint i;
	gint percentage;
	gint time;
	gboolean discharging;
	gint ret = 0;

	gpm_hal_find_device_capability ("battery", &device_names);
	if (!device_names) {
		g_debug ("Couldn't obtain list of batteries");
		return 0;
	}
	for (i = 0; device_names[i]; i++) {
		/* assume only one */
		gchar *type;
		gpm_hal_device_get_string (device_names[i], "battery.type", &type);
		if (type && strcmp (type, "primary") == 0) {
			udi = device_names[i];
			break;
		}
		g_free (type);
	}

	/* no battery found */
	if (!udi)
		return 0;
	g_debug ("Using battery %s for estimate.", udi);
	/*
	 * if no device then cannot compute and also rate information
	 * is useless when charging
	 */
	gpm_hal_device_get_bool (udi, "battery.rechargeable.is_discharging", &discharging);
	gpm_hal_device_get_int (udi, "battery.charge_level.percentage", &percentage);
	gpm_hal_device_get_int (udi, "battery.remaining_time", &time);
	if (discharging && time > 0 && percentage > 0) {
		time = time / 60;
		ret = value * ((double) time / (double) percentage);
	}

	gpm_hal_free_capability (device_names);
	return ret;
}

static void
set_estimated_label_widget (GtkWidget *widget, gint value)
{
	gchar *timestring;
	gchar *estimated;

	if (value > 1) {
		timestring = get_timestring_from_minutes (value);
		estimated = g_strdup_printf ("<i>Estimated %s</i>", timestring);
		gtk_widget_show_all (GTK_WIDGET (widget));
		gtk_label_set_markup (GTK_LABEL (widget), estimated);
		g_free (timestring);
		g_free (estimated);
	} else {
		/* hide if no valid number */
		gtk_widget_hide_all (GTK_WIDGET (widget));
	}
}

static void
gpm_prefs_help_cb (GtkWidget *widget, gpointer user_data)
{
	/* for now, show website */
	gnome_url_show (GPMURL, NULL);
}

static void
gpm_prefs_icon_radio_cb (GtkWidget *widget, gchar *icon_visibility)
{
	gconf_client_set_string (gconf_client_get_default (), GPM_PREF_ICON_POLICY,
				 icon_visibility, NULL);
}

static void
gpm_prefs_check_requirepw_cb (GtkWidget *widget, gpointer data)
{
	gconf_client_set_bool (gconf_client_get_default (), GPM_PREF_REQUIRE_PASSWORD,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)), NULL);
}

static gchar*
gpm_prefs_format_percentage_cb (GtkScale *scale, gdouble value)
{
	int steps;
	int *psteps = NULL;
	psteps = g_object_get_data ((GObject*) GTK_WIDGET (scale), "lcdsteps");
	if (!psteps)
		return g_strdup_printf ("%i%%", (gint) value);

	steps = GPOINTER_TO_INT (psteps);
	return g_strdup_printf ("%i%%", (gint) value * 100 / (steps - 1));
}

static gchar*
gpm_prefs_format_time_cb (GtkScale *scale, gdouble value)
{
	if ((gint) value == NEVER_TIME_ON_SLIDER)
		return g_strdup (_("Never"));

	return get_timestring_from_minutes (value);
}

static void
gpm_prefs_sleep_slider_changed_cb (GtkRange *range, gchar* gpm_pref_key)
{
	gint value;
	
	value = (gint)gtk_range_get_value (range);
	if (value == NEVER_TIME_ON_SLIDER) {
		value = 0; /* gnome power manager interprets 0 as Never */
	}

	gconf_client_set_int (gconf_client_get_default (), gpm_pref_key, value, NULL);
}

static GtkWidget*
gpm_prefs_setup_sleep_slider (GladeXML *dialog, gchar *widget_name, gchar *gpm_pref_key)
{
	GtkWidget *widget;
	gint value;

	widget = glade_xml_get_widget (dialog, widget_name);
	g_signal_connect (G_OBJECT (widget), "format-value", 
			  G_CALLBACK (gpm_prefs_format_time_cb), NULL);
	g_signal_connect (G_OBJECT (widget), "value-changed", 
			  G_CALLBACK (gpm_prefs_sleep_slider_changed_cb), gpm_pref_key);
	value = gconf_client_get_int (gconf_client_get_default (), gpm_pref_key, NULL);
	if (value == 0)
		value = NEVER_TIME_ON_SLIDER;
	gtk_range_set_value (GTK_RANGE (widget), value);

	return widget;
}

static void
gpm_prefs_brightness_slider_changed_cb (GtkRange *range, gchar* gpm_pref_key)
{
	gdouble value;
	gboolean is_on_ac;
	
	value = gtk_range_get_value (range);
	gconf_client_set_int (gconf_client_get_default (), gpm_pref_key, (gint) value, NULL);

	/* Change the brightness in real-time */
	is_on_ac = gpm_hal_is_on_ac ();
	if ((is_on_ac && strcmp (gpm_pref_key, GPM_PREF_AC_BRIGHTNESS) == 0) ||
	    (!is_on_ac && strcmp (gpm_pref_key, GPM_PREF_BATTERY_BRIGHTNESS) == 0)) {
		gpm_hal_set_brightness (value);
	}
}

static GtkWidget*
gpm_prefs_setup_brightness_slider (GladeXML *dialog, gchar *widget_name, gchar *gpm_pref_key)
{
	GtkWidget *widget;
	gint steps;
	gint value;

	widget = glade_xml_get_widget (dialog, widget_name);

	g_signal_connect (G_OBJECT (widget), "format-value", 
			  G_CALLBACK (gpm_prefs_format_percentage_cb), NULL);

	/* set the value before the changed cb, else the brightness is set */
	value = gconf_client_get_int (gconf_client_get_default (), gpm_pref_key, NULL);
	gtk_range_set_value (GTK_RANGE (widget), value);

	g_signal_connect (G_OBJECT (widget), "value-changed", 
			  G_CALLBACK (gpm_prefs_brightness_slider_changed_cb), gpm_pref_key);

	if (gpm_hal_get_brightness_steps (&steps)) {
		g_object_set_data ((GObject*) GTK_WIDGET (widget), "lcdsteps", GINT_TO_POINTER (steps));
		gtk_range_set_range (GTK_RANGE (widget), 0, steps - 1);
	}
	return widget;
}

static void
gpm_prefs_action_combo_changed_cb (GtkWidget *widget, gchar* gpm_pref_key)
{
	gchar *value;
	gchar *action;

	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));

	if (strcmp (value, ACTION_SUSPEND_TEXT) == 0) {
		action = ACTION_SUSPEND;
	} else if (strcmp (value, ACTION_HIBERNATE_TEXT) == 0) {
		action = ACTION_HIBERNATE;	
	} else if (strcmp (value, ACTION_SHUTDOWN_TEXT) == 0) {
		action = ACTION_SHUTDOWN;	
	} else if (strcmp (value, ACTION_NOTHING_TEXT) == 0) {
		action = ACTION_NOTHING;	
	} else {
		g_assert (FALSE);
	}
	
	g_free (value);
	gconf_client_set_string (gconf_client_get_default (), gpm_pref_key, action, NULL);
}

static GtkWidget*
gpm_prefs_setup_action_combo (GladeXML *dialog, gchar *widget_name, gchar *gpm_pref_key, const gchar **actions)
{
	GtkWidget *widget;
	gchar *value;
	gint i = 0;
	gint n_added = 0;
	gboolean can_suspend = gpm_hal_can_suspend ();
	gboolean can_hibernate = gpm_hal_can_hibernate ();

	widget = glade_xml_get_widget (dialog, widget_name);
	value = gconf_client_get_string (gconf_client_get_default (), gpm_pref_key, NULL);

	while (actions[i] != NULL) { 	
		if (strcmp (actions[i], ACTION_SHUTDOWN) == 0) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_SHUTDOWN_TEXT);
			n_added++;
		} else if ((strcmp (actions[i], ACTION_SUSPEND) == 0) && !can_suspend) {
			g_debug ("Cannot add option, as cannot suspend.");
		} else if ((strcmp (actions[i], ACTION_HIBERNATE) == 0) && !can_hibernate) {
			g_debug ("Cannot add option, as cannot hibernate.");
		} else if ((strcmp (actions[i], ACTION_SUSPEND) == 0) && can_suspend) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_SUSPEND_TEXT);
			n_added++;
		} else if ((strcmp (actions[i], ACTION_HIBERNATE) == 0) && can_hibernate) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
						   ACTION_HIBERNATE_TEXT);
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
	return widget;
}

static void
gpm_prefs_battery_low_slider_changed_cb (GtkWidget *widget, GladeXML *dialog)
{
	gdouble value;

	value = gtk_range_get_value (GTK_RANGE (widget));
	gconf_client_set_int (gconf_client_get_default (), GPM_PREF_THRESHOLD_LOW, (gint) value, NULL);

	gtk_range_set_range (GTK_RANGE (glade_xml_get_widget (dialog, "hscale_battery_critical")),
		       	     0, value);

	set_estimated_label_widget (glade_xml_get_widget (dialog, "label_battery_low_estimate"),
				    get_battery_time_for_percentage (value));
}

static void
gpm_prefs_battery_critical_slider_changed_cb (GtkWidget *widget, GladeXML *dialog)
{
	gdouble value;

	value = gtk_range_get_value (GTK_RANGE (widget));
	gconf_client_set_int (gconf_client_get_default (), GPM_PREF_THRESHOLD_CRITICAL, (gint) value, NULL);

	set_estimated_label_widget (glade_xml_get_widget (dialog, "label_battery_critical_estimate"),
				    get_battery_time_for_percentage (value));
}

static void
gpm_prefs_init ()
{
	GtkWidget *widget, *main_window;
	GladeXML *dialog;
	GtkSizeGroup *size_group;
	GConfClient *client;
	gint value;

	client = gconf_client_get_default ();
	dialog = glade_xml_new (GPM_DATA "/gpm-prefs.glade", NULL, NULL);

	main_window = glade_xml_get_widget (dialog, "window_preferences");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);

	gtk_window_set_icon_name (GTK_WINDOW (main_window), "gnome-dev-battery");

	/* Get the main window quit */
	g_signal_connect (G_OBJECT (main_window), "delete_event",
			  G_CALLBACK (gtk_main_quit), NULL);

	widget = glade_xml_get_widget (dialog, "button_close");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gtk_main_quit), NULL);

	widget = glade_xml_get_widget (dialog, "button_help");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gpm_prefs_help_cb), NULL);

	/************************************************************************/
	/* Sleep Tab 								*/
	/************************************************************************/

	/* AC icon */
	widget = glade_xml_get_widget (dialog, "image_side_acadapter");
	gtk_image_set_from_icon_name (GTK_IMAGE(widget), "gnome-fs-socket", GTK_ICON_SIZE_DIALOG);
	
	/* Sleep time on AC */
	gpm_prefs_setup_sleep_slider (dialog, "hscale_ac_computer", GPM_PREF_AC_SLEEP_COMPUTER);

	/* Sleep time for display on AC */	
	GtkWidget *label_ac_display, *slider_ac_display;

	label_ac_display = glade_xml_get_widget (dialog, "label_ac_display");
	slider_ac_display = gpm_prefs_setup_sleep_slider (dialog, "hscale_ac_display", GPM_PREF_AC_SLEEP_DISPLAY);

	/* Display brightness when on AC */
	GtkWidget *label_ac_brightness, *slider_ac_brightness;

	label_ac_brightness = glade_xml_get_widget (dialog, "label_ac_brightness");
	slider_ac_brightness = gpm_prefs_setup_brightness_slider (dialog, "hscale_ac_brightness",
								  GPM_PREF_AC_BRIGHTNESS);

	/* Battery icon */
	widget = glade_xml_get_widget (dialog, "image_side_battery");
	gtk_image_set_from_icon_name (GTK_IMAGE(widget), "gnome-dev-battery", GTK_ICON_SIZE_DIALOG);
	
	/* Sleep time on batteries */
	gpm_prefs_setup_sleep_slider (dialog, "hscale_batteries_computer", GPM_PREF_BATTERY_SLEEP_COMPUTER);

	/* Sleep time for display when on batteries */
	GtkWidget *label_batteries_display, *slider_batteries_display;

	label_batteries_display = glade_xml_get_widget (dialog, "label_batteries_display");
	slider_batteries_display = gpm_prefs_setup_sleep_slider (dialog, "hscale_batteries_display",
								 GPM_PREF_BATTERY_SLEEP_DISPLAY);

	/* Display brightness when on batteries */
	GtkWidget *label_batteries_brightness, *slider_batteries_brightness;

	label_batteries_brightness = glade_xml_get_widget (dialog, "label_batteries_brightness");
	slider_batteries_brightness = gpm_prefs_setup_brightness_slider (dialog, "hscale_batteries_brightness",
									 GPM_PREF_BATTERY_BRIGHTNESS);

	/************************************************************************/
	/* Options Tab 								*/
	/************************************************************************/

	/* Sleep Type Combo Box */
	GtkWidget *label_sleep_type, *combo_sleep_type;
	const gchar *sleep_type_actions[] = {ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	label_sleep_type = glade_xml_get_widget (dialog, "label_sleep_type");
	combo_sleep_type = gpm_prefs_setup_action_combo (dialog, "combobox_sleep_type",
							 GPM_PREF_SLEEP_TYPE, sleep_type_actions);

	/* Require password Check Button */
	widget = glade_xml_get_widget (dialog, "checkbutton_require_password");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (gpm_prefs_check_requirepw_cb), NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      gconf_client_get_bool (client, GPM_PREF_REQUIRE_PASSWORD, NULL));

	/* Button Suspend Combo Box */
	GtkWidget *label_button_suspend, *combo_button_suspend;
	const gchar *button_suspend_actions[] = {ACTION_NOTHING, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	label_button_suspend = glade_xml_get_widget (dialog, "label_button_suspend");
	combo_button_suspend = gpm_prefs_setup_action_combo (dialog, "combobox_button_suspend",
				      			     GPM_PREF_BUTTON_SUSPEND, button_suspend_actions);
	/* Button Lid Combo Box */
	GtkWidget *label_button_lid, *combo_button_lid;
	const gchar *button_lid_actions[] = {ACTION_NOTHING, ACTION_SUSPEND, ACTION_HIBERNATE, NULL};

	label_button_lid = glade_xml_get_widget (dialog, "label_button_lid");
	combo_button_lid = gpm_prefs_setup_action_combo (dialog, "combobox_button_lid",
				      			 GPM_PREF_BUTTON_LID, button_lid_actions);

	/* Battery critical Combo Box */
	GtkWidget *label_battery_critical, *combo_battery_critical;
	const gchar *battery_critical_actions[] = {ACTION_NOTHING, ACTION_SUSPEND, ACTION_HIBERNATE,
					     	   ACTION_SHUTDOWN, NULL};

	label_battery_critical = glade_xml_get_widget (dialog, "label_battery_critical_action");
	combo_battery_critical = gpm_prefs_setup_action_combo (dialog, "combobox_battery_critical",
				      			 GPM_PREF_BATTERY_CRITICAL, battery_critical_actions);

	/* Make sure that all comboboxes get the same size by adding their
	 * labels to a GtkSizeGroup
	 */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, label_sleep_type);
	gtk_size_group_add_widget (size_group, label_button_suspend);
	gtk_size_group_add_widget (size_group, label_button_lid);
	gtk_size_group_add_widget (size_group, label_battery_critical);
	g_object_unref (G_OBJECT (size_group));

	/************************************************************************/
	/* Advanced Tab 							*/
	/************************************************************************/

	/* Radio buttons icon policy */
	gchar* icon_policy = gconf_client_get_string (client, GPM_PREF_ICON_POLICY, NULL);

	widget = glade_xml_get_widget (dialog, "radiobutton_icon_always");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), ICON_POLICY_ALWAYS);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      strcmp (icon_policy, ICON_POLICY_ALWAYS) == 0);

	widget = glade_xml_get_widget (dialog, "radiobutton_icon_charge");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), ICON_POLICY_CHARGE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      strcmp (icon_policy, ICON_POLICY_CHARGE) == 0);

	widget = glade_xml_get_widget (dialog, "radiobutton_icon_critical");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), ICON_POLICY_CRITICAL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      strcmp (icon_policy, ICON_POLICY_CRITICAL) == 0);

	widget = glade_xml_get_widget (dialog, "radiobutton_icon_never");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), ICON_POLICY_NEVER);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      strcmp (icon_policy, ICON_POLICY_NEVER) == 0);
	g_free (icon_policy);

	/* Threshold low sliders */
	GtkWidget *scale_battery_low;

	scale_battery_low = glade_xml_get_widget (dialog, "hscale_battery_low");
	gtk_range_set_range (GTK_RANGE (scale_battery_low), 5, 25);
	g_signal_connect (G_OBJECT (scale_battery_low), "format-value", 
			  G_CALLBACK (gpm_prefs_format_percentage_cb), NULL);
	g_signal_connect (G_OBJECT (scale_battery_low), "value-changed", 
			  G_CALLBACK (gpm_prefs_battery_low_slider_changed_cb), dialog);
	value = gconf_client_get_int (gconf_client_get_default (), GPM_PREF_THRESHOLD_LOW, NULL);
	gtk_range_set_value (GTK_RANGE (scale_battery_low), value);

	/* Threshold critical slider */
	GtkWidget *scale_battery_critical;

	scale_battery_critical = glade_xml_get_widget (dialog, "hscale_battery_critical");
	gtk_range_set_range (GTK_RANGE (scale_battery_critical), 0, value);
	g_signal_connect (G_OBJECT (scale_battery_critical), "format-value", 
			  G_CALLBACK (gpm_prefs_format_percentage_cb), NULL);	
	g_signal_connect (G_OBJECT (scale_battery_critical), "value-changed", 
			  G_CALLBACK (gpm_prefs_battery_critical_slider_changed_cb), dialog);
	value = gconf_client_get_int (gconf_client_get_default (), GPM_PREF_THRESHOLD_CRITICAL, NULL);
	gtk_range_set_value (GTK_RANGE (scale_battery_critical), value);
	/* set estimated label in case the value is the same */
	set_estimated_label_widget (glade_xml_get_widget (dialog, "label_battery_critical_estimate"),
				    get_battery_time_for_percentage (value));

	gboolean has_batteries = gpm_hal_num_devices_of_capability ("battery") > 0;
	if (!has_batteries) {
		widget = glade_xml_get_widget (dialog, "label_frame_ac");
		gtk_label_set_markup (GTK_LABEL (widget), _("<b>Configuration</b>"));

		/* Sleep tab */
		widget = glade_xml_get_widget (dialog, "frame_batteries");
		gtk_widget_hide_all (widget);
		/* Hide battery options in options tab */
		gtk_widget_hide_all (label_battery_critical);
		gtk_widget_hide_all (combo_battery_critical);
		/* Hide battery options in advanced tab */
		widget = glade_xml_get_widget (dialog, "frame_other_options");
		gtk_widget_hide_all (widget);
	}

	gboolean has_suspend_button = gpm_hal_num_devices_of_capability_with_value  ("button", "button.type", "sleep") > 0;
	if (!has_suspend_button) {
		gtk_widget_hide_all (label_button_suspend);
		gtk_widget_hide_all (combo_button_suspend);
	}

	gboolean has_lid_button =  gpm_hal_num_devices_of_capability_with_value ("button", "button.type", "lid") > 0;
	if (!has_lid_button) {
		gtk_widget_hide_all (label_button_lid);
		gtk_widget_hide_all (combo_button_lid);
	}

	/* if no options then disable frame as it will be empty */
	if (!has_batteries && !has_suspend_button && !has_lid_button) {
		widget = glade_xml_get_widget (dialog, "frame_actions");
		gtk_widget_hide_all (widget);
	}

	gboolean can_display_sleep;
	if (gpm_screensaver_is_running ()) {
		can_display_sleep = gconf_client_get_bool (client, GS_PREF_DPMS_ENABLED, NULL);
	} else {
		can_display_sleep = FALSE;
	}
	if (!can_display_sleep) {
		gtk_widget_hide_all (label_ac_display);
		gtk_widget_hide_all (slider_ac_display);
		gtk_widget_hide_all (label_batteries_display);
		gtk_widget_hide_all (slider_batteries_display);
	}

	gboolean can_set_brightness = gpm_hal_num_devices_of_capability ("laptop_panel") > 0;
	if (!can_set_brightness) {
		gtk_widget_hide_all (label_ac_brightness);
		gtk_widget_hide_all (slider_ac_brightness);
		gtk_widget_hide_all (label_batteries_brightness);
		gtk_widget_hide_all (slider_batteries_brightness);
	}

	/* Now we are ready to show the main window */
	gtk_widget_show (main_window);
}

int
main (int argc, char **argv)
{
	gint i;
	gboolean verbose = FALSE;
	GConfClient *client;

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
			    _("GNOME Power Preferences"),
			    NULL);

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	if (!verbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, gpm_prefs_debug_log_ignore, NULL);

	client = gconf_client_get_default ();

	/* check if we have GNOME Screensaver, but have disabled dpms */
	if (gpm_screensaver_is_running ()) {
		if (!gconf_client_get_bool (client, GS_PREF_DPMS_ENABLED, NULL)) {
			g_warning ("You have not got DPMS support enabled"
				   "in gnome-screensaver.\n"
				   "GNOME Power Manager will enable it now.");
			gconf_client_set_bool (client, GS_PREF_DPMS_ENABLED, TRUE, NULL);
		}
	}

	gpm_prefs_init ();

	gtk_main ();
	return 0;
}
