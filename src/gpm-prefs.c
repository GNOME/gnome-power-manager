/***************************************************************************
 *
 * gpm-prefs.c : GNOME Power Preferences
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 *
 * Taken in part from:
 * - GNOME Tutorial, example 1
 * http://www.gnome.org/~newren/tutorials/developing-with-gnome/html/apd.html
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#if HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include "gpm-common.h"
#include "gpm-prefs.h"
#include "gpm-main.h"

static GladeXML *all_pref_widgets;
static gboolean isVerbose;
static HasData hasData;
gboolean displayIcon = TRUE;
gboolean displayIconFull = TRUE;

/** Convenience function to call libnotify
 *
 *  @param  content		The content text, e.g. "Battery low"
 *  @param  value		The urgency, e.g NOTIFY_URGENCY_CRITICAL
 */
static void
use_libnotify (const char *content, const int urgency)
{
#if HAVE_LIBNOTIFY
	NotifyIcon *icon = notify_icon_new_from_uri (GPM_DATA "gnome-power.png");
	const char *summary = NICENAME;
	NotifyHandle *n = notify_send_notification (NULL, /* replaces nothing 	*/
			   NULL,
			   urgency,
			   summary, content,
			   icon, /* no icon 			*/
			   TRUE, NOTIFY_TIMOUT,
			   NULL,
			   NULL, /* no user data 		*/
			   0);   /* no actions 			*/
	notify_icon_destroy(icon);	
	if (!n)
		g_warning ("failed to send notification (%s)", content);
#else
	GtkWidget *widget;
	widget = gnome_message_box_new (content, 
                                GNOME_MESSAGE_BOX_WARNING,
                                GNOME_STOCK_BUTTON_OK, 
                                NULL);
	gtk_window_set_title (GTK_WINDOW (widget), NICENAME);
	gtk_widget_show (widget);
#endif
}

/** Sets/Hides GTK visibility
 *
 *  @param  widgetname		the libglade widget name
 *  @param  set			should widget be visible?
 */
static void
gtk_set_visibility (const char *widgetname, gboolean set)
{
	g_return_if_fail (widgetname);
	GtkWidget *widget;
	widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	if (!widget) {
		g_warning ("gtk_set_visibility: widget '%s' not found", widgetname);
		return;
	}

	if (set)
		gtk_widget_show_all (widget);
	else
		gtk_widget_hide_all (widget);
}

/** Sets/Clears GTK Checkbox
 *
 *  @param  widgetname		the libglade widget name
 *  @param  set			should check be ticked?
 */
static void
gtk_set_check (const char *widgetname, gboolean set)
{
	g_return_if_fail (widgetname);
	GtkWidget *widget;
	widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	if (!widget) {
		g_warning ("widget '%s' failed to load, aborting", widgetname);
		return;
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), set);
}

/** Shows/hides/renames controls based on hasData, i.e. what hardware is in the system.
 *
 */
static void
recalc (void)
{
	GtkWidget *widget;
	/* checkboxes */
	gtk_set_check ("checkbutton_display_icon", displayIcon);
	gtk_set_check ("checkbutton_display_icon_full", displayIconFull);

	/* frame labels */
	if (hasData.hasBatteries) {
		widget = glade_xml_get_widget (all_pref_widgets, "label_frame_ac");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Running on AC adapter</b>");
		widget = glade_xml_get_widget (all_pref_widgets, "label_frame_batteries");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Running on batteries</b>");
	} else {
		widget = glade_xml_get_widget (all_pref_widgets, "label_frame_ac");
		gtk_label_set_markup (GTK_LABEL (widget), "<b>Configuration</b>");
	}

	/* top frame */
	gtk_set_visibility ("frame_batteries", hasData.hasBatteries);
	gtk_set_visibility ("combobox_battery_critical", hasData.hasBatteries);
	gtk_set_visibility ("label_battery_critical_action", hasData.hasBatteries);
	gtk_set_visibility ("label_battery_critical", hasData.hasBatteries);
	gtk_set_visibility ("label_battery_low", hasData.hasBatteries);
	gtk_set_visibility ("hscale_battery_low", hasData.hasBatteries);
	gtk_set_visibility ("hscale_battery_critical", hasData.hasBatteries);
	/* assumes only battery options are in this frame */
	gtk_set_visibility ("frame_other_options", hasData.hasBatteries);

	/* options */
	gtk_set_visibility ("combobox_button_lid", hasData.hasButtonLid);
	gtk_set_visibility ("label_button_lid", hasData.hasButtonLid);

	gtk_set_visibility ("combobox_button_power", hasData.hasButtonPower);
	gtk_set_visibility ("label_button_power", hasData.hasButtonPower);

	gtk_set_visibility ("combobox_button_suspend", hasData.hasButtonSleep);
	gtk_set_visibility ("label_button_suspend", hasData.hasButtonSleep);

	gtk_set_visibility ("combobox_ac_fail", hasData.hasAcAdapter);
	gtk_set_visibility ("label_ac_fail", hasData.hasAcAdapter);

	gtk_set_visibility ("combobox_ups_critical", hasData.hasUPS);
	gtk_set_visibility ("label_ups_critical", hasData.hasUPS);

	/* variables */
	gtk_set_visibility ("hscale_ac_brightness", hasData.hasLCD);
	gtk_set_visibility ("label_ac_brightness", hasData.hasLCD);
	gtk_set_visibility ("hscale_batteries_brightness", hasData.hasLCD);
	gtk_set_visibility ("label_batteries_brightness", hasData.hasLCD);

	gtk_set_visibility ("hscale_ac_display", hasData.hasDisplays);
	gtk_set_visibility ("label_ac_display", hasData.hasDisplays);
	gtk_set_visibility ("hscale_batteries_display", hasData.hasDisplays & hasData.hasBatteries);
	gtk_set_visibility ("label_batteries_display", hasData.hasDisplays & hasData.hasBatteries);

	gtk_set_visibility ("hscale_ac_hdd", hasData.hasHardDrive);
	gtk_set_visibility ("label_ac_hdd", hasData.hasHardDrive);
	gtk_set_visibility ("hscale_batteries_hdd", hasData.hasHardDrive);
	gtk_set_visibility ("label_batteries_hdd", hasData.hasHardDrive);
}

/** Perform the interactive action when a bool gconf key has been changed
 *
 *  @param  key			full gconf key path
 */
static void
gconf_key_action (const char *key)
{
	g_return_if_fail (key);
	gboolean value;
	GConfClient *client = gconf_client_get_default ();

	value = gconf_client_get_bool (client, key, NULL);

	if (strcmp (key, GCONF_ROOT "general/hasUPS") == 0)
		hasData.hasUPS = value;
	else if (strcmp (key, GCONF_ROOT "general/hasAcAdapter") == 0)
		hasData.hasAcAdapter = value;
	else if (strcmp (key, GCONF_ROOT "general/hasBatteries") == 0)
		hasData.hasBatteries = value;
	else if (strcmp (key, GCONF_ROOT "general/hasButtonPower") == 0)
		hasData.hasButtonPower = value;
	else if (strcmp (key, GCONF_ROOT "general/hasButtonSleep") == 0)
		hasData.hasButtonSleep = value;
	else if (strcmp (key, GCONF_ROOT "general/hasButtonLid") == 0)
		hasData.hasButtonLid = value;
	else if (strcmp (key, GCONF_ROOT "general/hasHardDrive") == 0)
		hasData.hasHardDrive = value;
	else if (strcmp (key, GCONF_ROOT "general/hasLCD") == 0)
		hasData.hasLCD = value;
	else if (strcmp (key, GCONF_ROOT "general/displayIcon") == 0)
		displayIcon = value;
	else if (strcmp (key, GCONF_ROOT "general/displayIconFull") == 0)
		displayIconFull = value;
	/* data is not got from HAL, but from gnome-screensaver */
	else if (strcmp (key, "/apps/gnome-screensaver/dpms_enabled") == 0)
		hasData.hasDisplays = value;
	else {
		g_warning ("Urecognised key [%s]", key);
		return;
	}
	recalc ();
}

/** Callback for gconf_key_changed
 *
 */
static void
callback_gconf_key_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	g_return_if_fail (client);
	if (gconf_entry_get_value (entry) == NULL)
		return;

	if (gconf_entry_get_value (entry)->type == GCONF_VALUE_BOOL)
		gconf_key_action (entry->key);
}

/** Callback for combo_changed
 *
 */
static void
callback_combo_changed (GtkWidget *widget, gpointer user_data)
{
	g_return_if_fail (widget);
	GConfClient *client = gconf_client_get_default ();
	gint value = gtk_combo_box_get_active(GTK_COMBO_BOX (widget));
	char *policypath = g_object_get_data ((GObject*) widget, "policypath");
	g_return_if_fail (policypath);

	g_debug ("[%s] = (%i)", policypath, value);

	gchar *policyoption = convert_policy_to_string (value);
	gconf_client_set_string (client, policypath, policyoption, NULL);
}

/** Callback for hscale_changed
 *
 */
static void
callback_hscale_changed (GtkWidget *widget, gpointer user_data)
{
	g_return_if_fail (widget);

	gint value = (int) gtk_range_get_value (GTK_RANGE (widget));
/* 
 * Code for divisions of 10 seconds, unfinished
 *
	int v2 = (int (value / 10)) * 10;
	gtk_range_set_value (GTK_RANGE (widget), v2);
	if (v2 != value)
		return;
*/

/* if this is hscale for battery_low, then set upper range of hscale for 
 * battery_critical maximum to value
 * (This stops criticalThreshold > lowThreshold)
 */
	if (strcmp (gtk_widget_get_name (widget), "hscale_battery_low") == 0) {
		GtkWidget *widget2;
		widget2 = glade_xml_get_widget (all_pref_widgets, "hscale_battery_critical");
		gtk_range_set_range (GTK_RANGE (widget2), 0, value);
	}

	GConfClient *client = gconf_client_get_default ();
	char *policypath = g_object_get_data ((GObject*) widget, "policypath");
	g_return_if_fail (policypath);
	g_debug ("[%s] = (%i)", policypath, value);
	gconf_client_set_int (client, policypath, value, NULL);
}

/** Callback for button_help
 *
 */
static void
callback_help (GtkWidget *widget, gpointer user_data)
{
	/* for now, show website */
	gnome_url_show ("http://gnome-power.sourceforge.net/", NULL);
}

/** Callback for check_changed
 *
 */
static void
callback_check_changed (GtkWidget *widget, gpointer user_data)
{
	g_return_if_fail (widget);
	GConfClient *client = gconf_client_get_default ();
	gboolean value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	char *policypath = g_object_get_data ((GObject*) widget, "policypath");
	g_return_if_fail (policypath);
	/*int policytype = (int) g_object_get_data ((GObject*) widget, "policytype");*/

	g_debug ("[%s] = (%i)", policypath, value);
	gconf_client_set_bool (client, policypath, value, NULL);
}

/** Prints program usage.
 *
 */
static void
print_usage (void)
{
	g_print ("\nusage : gnome-power-preferences [--verbose] [--help]\n");
	g_print (
		"\n"
		"        --verbose               Show extra debugging\n"
		"        --help                  Show this information and exit\n"
		"\n");
}

/** simple callback just appending a "%" to the reading
 *
 */
static gchar*
format_value_callback_percent (GtkScale *scale, gdouble value)
{
	return g_strdup_printf ("%i%%", (int) value);
}

/** simple callback that converts minutes into pretty text
 *
 */
static gchar*
format_value_callback_time (GtkScale *scale, gdouble value)
{
	char unitstring[32];
	GString *strvalue;
	if (value == 0)
		return g_strdup_printf ("Never");

	strvalue = get_timestring_from_minutes (value);
	strcpy (unitstring, strvalue->str);
	g_string_free (strvalue, TRUE);
	return g_strdup_printf ("%s", unitstring);
}

/** Sets the comboboxes up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the GConf policy path, 
 *				e.g. "policy/AC/Brightness"
 *  @param  policytype		the policy ptye, e.g. POLICY_PERCENT
 */
static void
combo_setup_action (const char *widgetname, const char *policypath, int policytype)
{
	g_return_if_fail (widgetname);
	GConfClient *client = gconf_client_get_default ();

	GtkWidget *widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	g_return_if_fail (widget);
	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);
	g_object_set_data ((GObject*) widget, "policytype", (gpointer) policytype);

	if (policytype == POLICY_CHOICE) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Do nothing"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Send warning"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Suspend"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Hibernate"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Shutdown"));
	} else if (policytype == POLICY_NONE) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Quick sleep"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Hibernate"));
	}

	gchar *policyoption = gconf_client_get_string (client, policypath, NULL);

	if (!policyoption) {
		g_warning ("gconf_client_get_string for widget '%s' failed (policy='%s')!!", widgetname, policypath);
		return;
	}

	gint value = convert_string_to_policy (policyoption);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), value);
	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (callback_combo_changed), NULL);
}

/** Sets the hscales up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the GConf policy path, 
 *				e.g. "policy/AC/Brightness"
 *  @param  policytype		the policy ptye, e.g. POLICY_PERCENT
 */
static void
hscale_setup_action (const char *widgetname, const char *policypath, int policytype)
{
	g_return_if_fail (widgetname);
	g_return_if_fail (policypath);
	GConfClient *client = gconf_client_get_default ();
	GtkWidget *widget = glade_xml_get_widget (all_pref_widgets, widgetname);

	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);
	g_object_set_data ((GObject*) widget, "policytype", (gpointer) policytype);

	gint value = gconf_client_get_int (client, policypath, NULL);
	g_debug ("'%s' -> [%s] = (%i)", widgetname, policypath, value);

	if (policytype == POLICY_PERCENT)
		g_signal_connect (G_OBJECT (widget), "format-value", G_CALLBACK (format_value_callback_percent), NULL);
	else
		g_signal_connect (G_OBJECT (widget), "format-value", G_CALLBACK (format_value_callback_time), NULL);
	gtk_range_set_value (GTK_RANGE (widget), (int) value);
	g_signal_connect (G_OBJECT (widget), "value-changed", G_CALLBACK (callback_hscale_changed), NULL);
}

/** Sets the checkboxes up to the gconf value, and sets up callbacks.
 *
 *  @param  widgetname		the libglade widget name
 *  @param  policypath		the GConf policy path, 
 *				e.g. "policy/AC/Brightness"
 */
static void
checkbox_setup_action (const char *widgetname, const char *policypath)
{
	g_return_if_fail (widgetname);
	g_return_if_fail (policypath);
	GConfClient *client = gconf_client_get_default ();
	GtkWidget *widget = glade_xml_get_widget (all_pref_widgets, widgetname);
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (callback_check_changed), NULL);
	g_object_set_data ((GObject*) widget, "policypath", (gpointer) policypath);

	gboolean value = gconf_client_get_bool (client, policypath, NULL);
	g_debug ("'%s' -> [%s] = (%i)", widgetname, policypath, value);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

/** Main program
 *
 */
int
main (int argc, char **argv)
{
	gtk_init (&argc, &argv);
	gconf_init (argc, argv, NULL);
	GtkWidget *widget = NULL;
	int a;

	isVerbose = FALSE;
	for (a=1; a < argc; a++) {
		if (strcmp (argv[a], "--verbose") == 0)
			isVerbose = TRUE;
		else if (strcmp (argv[a], "--help") == 0) {
			print_usage ();
			return EXIT_SUCCESS;
		}
	}

	if (!isVerbose)
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, g_log_ignore, NULL);

	/* load the interface */
	all_pref_widgets = glade_xml_new (GPM_DATA "preferences.glade", NULL, NULL);
	if (!all_pref_widgets)
		g_error ("glade file failed to load, aborting");

	/* Get the GconfClient, tell it we want to monitor /apps/gnome-power */
	GConfClient *client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/apps/gnome-power", GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (client, "/apps/gnome-power", callback_gconf_key_changed, widget, NULL, NULL);
	gconf_client_notify_add (client, "/apps/gnome-screensaver", callback_gconf_key_changed, widget, NULL, NULL);

	/* Get the main_window quit */
	widget = glade_xml_get_widget (all_pref_widgets, "window_preferences");
	if (!widget)
		g_error ("Main window failed to load, aborting");
	g_signal_connect (G_OBJECT (widget), "delete_event", G_CALLBACK (gtk_main_quit), NULL);

#if HAVE_LIBNOTIFY
	/* initialise libnotify */
	if (!notify_init (NICENAME))
		g_error ("Cannot initialise libnotify!");
#endif

	/* Get the help and quit buttons */
	widget = glade_xml_get_widget (all_pref_widgets, "button_close");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gtk_main_quit), NULL);
	widget = glade_xml_get_widget (all_pref_widgets, "button_help");
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (callback_help), NULL);

	/* get values from gconf */
	gconf_key_action (GCONF_ROOT "general/displayIcon");
	gconf_key_action (GCONF_ROOT "general/displayIconFull");

	/* disable these until the backend code is in place */
	gtk_set_visibility ("combobox_double_click", FALSE);
	gtk_set_visibility ("label_double_click", FALSE);

	/* checkboxes */
	checkbox_setup_action ("checkbutton_display_icon",
		GCONF_ROOT "general/displayIcon");
	checkbox_setup_action ("checkbutton_display_icon_full",
		GCONF_ROOT "general/displayIconFull");

	/* comboboxes */
	combo_setup_action ("combobox_button_power",
		GCONF_ROOT "policy/ButtonPower", POLICY_CHOICE);
	combo_setup_action ("combobox_button_suspend",
		GCONF_ROOT "policy/ButtonSuspend", POLICY_CHOICE);
	combo_setup_action ("combobox_button_lid",
		GCONF_ROOT "policy/ButtonLid", POLICY_CHOICE);
	combo_setup_action ("combobox_ac_fail",
		GCONF_ROOT "policy/ACFail", POLICY_CHOICE);
	combo_setup_action ("combobox_battery_critical",
		GCONF_ROOT "policy/BatteryCritical", POLICY_CHOICE);
	combo_setup_action ("combobox_ups_critical",
		GCONF_ROOT "policy/UPSCritical", POLICY_CHOICE);
	combo_setup_action ("combobox_sleep_type",
		GCONF_ROOT "policy/SleepType", POLICY_NONE);

	/* sliders */
	hscale_setup_action ("hscale_ac_computer", 
		GCONF_ROOT "policy/AC/SleepComputer", POLICY_TIME);
	hscale_setup_action ("hscale_ac_hdd", 
		GCONF_ROOT "policy/AC/SleepHardDrive", POLICY_TIME);
	hscale_setup_action ("hscale_ac_display", 
		GCONF_ROOT "policy/AC/SleepDisplay", POLICY_TIME);
	hscale_setup_action ("hscale_ac_brightness", 
		GCONF_ROOT "policy/AC/Brightness", POLICY_PERCENT);
	hscale_setup_action ("hscale_batteries_computer", 
		GCONF_ROOT "policy/Batteries/SleepComputer", POLICY_TIME);
	hscale_setup_action ("hscale_batteries_hdd", 
		GCONF_ROOT "policy/Batteries/SleepHardDrive", POLICY_TIME);
	hscale_setup_action ("hscale_batteries_display", 
		GCONF_ROOT "policy/Batteries/SleepDisplay", POLICY_TIME);
	hscale_setup_action ("hscale_batteries_brightness", 
		GCONF_ROOT "policy/Batteries/Brightness", POLICY_PERCENT);
	hscale_setup_action ("hscale_battery_low", 
		GCONF_ROOT "general/lowThreshold", POLICY_PERCENT);
	hscale_setup_action ("hscale_battery_critical", 
		GCONF_ROOT "general/criticalThreshold", POLICY_PERCENT);

	/* set up upper limit for battery_critical */
	widget = glade_xml_get_widget (all_pref_widgets, "hscale_battery_low");
	gint value = (int) gtk_range_get_value (GTK_RANGE (widget));
	widget = glade_xml_get_widget (all_pref_widgets, "hscale_battery_critical");
	gtk_range_set_range (GTK_RANGE (widget), 0, value);

	hasData.hasDisplays = gconf_client_get_bool (client, "/apps/gnome-screensaver/dpms_enabled", NULL);
	gtk_set_visibility ("hscale_ac_display", hasData.hasDisplays);
	gtk_set_visibility ("label_ac_display", hasData.hasDisplays);
	gtk_set_visibility ("hscale_batteries_display", hasData.hasDisplays & hasData.hasBatteries);
	gtk_set_visibility ("label_batteries_display", hasData.hasDisplays & hasData.hasBatteries);
	if (!hasData.hasDisplays) {
		use_libnotify ("You have not got DPMS support enabled in gnome-screensaver. You cannot cannot change the screen shutdown time using this program.", NOTIFY_URGENCY_NORMAL);
		gtk_set_visibility ("button_gnome_screensave", FALSE);
	}

	gconf_key_action (GCONF_ROOT "general/hasHardDrive");
	gconf_key_action (GCONF_ROOT "general/hasBatteries");
	gconf_key_action (GCONF_ROOT "general/hasAcAdapter");
	gconf_key_action (GCONF_ROOT "general/hasUPS");
	gconf_key_action (GCONF_ROOT "general/hasLCD");
	gconf_key_action (GCONF_ROOT "general/hasDisplays");
	gconf_key_action (GCONF_ROOT "general/hasUPS");
	gconf_key_action (GCONF_ROOT "general/hasButtonLid");
	gconf_key_action (GCONF_ROOT "general/hasButtonSleep");
	gconf_key_action (GCONF_ROOT "general/hasButtonPower");

	gtk_main ();
	return 0;
}
