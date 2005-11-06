/** @file	gpm-notification.c
 *  @brief	GNOME Power Notification
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module provides panel functions for g-p-m, and is closely linked
 * to gpm-main.c
 */
/*
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

#include <gconf/gconf-client.h>
#include <gdk/gdk.h>
#if defined(HAVE_LIBNOTIFY)
#include <libnotify/notify.h>
#endif

#include "gpm-common.h"
#include "gpm-main.h"
#include "gpm-notification.h"
#include "gpm-libnotify.h"
#include "gpm-stock-icons.h"
#include "compiler.h"
#include "gpm-sysdev.h"

/* shared with gpm-main.c */
static TrayData *eggtrayicon = NULL;
gboolean onAcPower;

/** Finds the icon index value for the percentage charge
 *
 *  @param	percent		The percentage value
 *  @return			A scale 0..8
 */
static gint
get_index_from_percent (gint percent)
{
	const gint NUM_INDEX = 8;
	gint index;

	index = ((percent + NUM_INDEX/2) * NUM_INDEX ) / 100;
	if (index < 0)
		return 0;
	else if (index > NUM_INDEX)
		return NUM_INDEX;
	return index;
}

/** Gets an icon name for the object
 *
 *  @return			An icon name
 */
static gchar *
get_stock_id (void)
{
	gint index;
	gchar *stock_id = NULL;
	sysDev *sd = NULL;
	GConfClient *client = NULL;
	gint lowThreshold;
	gboolean displayFull;

	/* find out when the user considers the power "low" */
	client = gconf_client_get_default ();
	lowThreshold = gconf_client_get_int (client,
				GCONF_ROOT "general/threshold_low", NULL);

	/* list in order of priority */
	sd = sysDevGet (BATT_PRIMARY);
	if (sd->numberDevices > 0 && sd->percentageCharge < lowThreshold) {
		index = get_index_from_percent (sd->percentageCharge);
		if (onAcPower)
			return g_strdup_printf ("gnome-power-ac-%d-of-8", index);
		return g_strdup_printf ("gnome-power-bat-%d-of-8", index);
	}
	sd = sysDevGet (BATT_UPS);
	if (sd->numberDevices > 0 && sd->percentageCharge < lowThreshold) {
		index = get_index_from_percent (sd->percentageCharge);
		return g_strdup_printf ("gnome-power-ups-%d-of-8", index);
	}
	sd = sysDevGet (BATT_MOUSE);
	if (sd->numberDevices > 0 && sd->percentageCharge < lowThreshold)
		return g_strdup_printf ("gnome-power-mouse");
	sd = sysDevGet (BATT_KEYBOARD);
	if (sd->numberDevices > 0 && sd->percentageCharge < lowThreshold)
		return g_strdup_printf ("gnome-power-keyboard");

	/*
	 * Check if we should just show the charging / discharging icon 
	 * even when not low or critical.
	 */
	client = gconf_client_get_default ();
	displayFull = gconf_client_get_bool (client,
				GCONF_ROOT "general/display_icon_full", NULL);
	if (!displayFull)
		return NULL;

	/* Only display if not at 100% */
	sd = sysDevGet (BATT_PRIMARY);
	if (sd->percentageCharge == 100)
		return NULL;

	/* Do the rest of the battery icon states */
	if (sd->numberDevices > 0) {
		index = get_index_from_percent (sd->percentageCharge);
		if (onAcPower)
			return g_strdup_printf ("gnome-power-ac-%d-of-8", index);
		return g_strdup_printf ("gnome-power-bat-%d-of-8", index);
	}
	return NULL;
}

/** Frees resources and hides notification area icon
 *
 *  @param	eggtrayicon	A valid TrayIcon
 */
void
icon_destroy (TrayData *eggtrayicon)
{
	/* assertion checks */
	g_assert (eggtrayicon);

	g_debug ("icon_destroy");
	if (eggtrayicon->popup_menu)
		g_free (eggtrayicon->popup_menu);
	if (eggtrayicon->tray_icon_tooltip)
		g_free (eggtrayicon->tray_icon_tooltip);
	gtk_widget_hide_all (GTK_WIDGET (eggtrayicon->tray_icon));
	g_free (eggtrayicon);
	eggtrayicon = NULL;
}

/** Frees icon, wrapper function
 *
 */
void
gpn_icon_destroy (void)
{
	if (eggtrayicon)
		icon_destroy (eggtrayicon);
	eggtrayicon = NULL;
}

/** Gets the tooltip for a specific device object
 *
 *  @param	type		The device type
 *  @param	sds		The device struct
 *  @return			Part of the tooltip
 */
GString *
get_tooltip_system_struct (DeviceType type, sysDevStruct *sds)
{
	GString *tooltip = NULL;
	gchar *devicestr = NULL;
	gchar *chargestate = NULL;

	/* do not display for not present devices */
	if (!sds->present)
		return NULL;

	tooltip = g_string_new ("");
	devicestr = sysDevToString (type);

	/* don't display all the extra stuff for keyboards and mice */
	if (type == BATT_MOUSE ||
	    type == BATT_KEYBOARD ||
	    type == BATT_PDA) {
		g_string_printf (tooltip, "%s (%i%%)",
				 devicestr, sds->percentageCharge);
		return tooltip;
	}

	/* work out chargestate */
	if (sds->isCharging)
		chargestate = _("charging");
	else if (sds->isDischarging)
		chargestate = _("discharging");
	else if (!sds->isCharging &&
		 !sds->isDischarging)
		chargestate = _("charged");

	g_string_printf (tooltip, "%s %s (%i%%)",
			 devicestr, chargestate, sds->percentageCharge);
	/*
	 * only display time remaining if minutesRemaining > 2
	 * and percentageCharge < 99 to cope with some broken
	 * batteries.
	 */
	if (sds->minutesRemaining > 2 && sds->percentageCharge < 99) {
		gchar *timestring;
		timestring = get_timestring_from_minutes (sds->minutesRemaining);
		if (timestring) {
			if (sds->isCharging)
				g_string_append_printf (tooltip, "\n%s %s",
					timestring, _("until charged"));
			else
				g_string_append_printf (tooltip, "\n%s %s",
					timestring, _("until empty"));
		g_free (timestring);
		}
	}

	return tooltip;
}
/** Gets the tooltip for a specific device object
 *
 *  @param	type		The device type
 *  @param	sd		The system device
 *  @return			Part of the tooltip
 */
GString *
get_tooltips_system_device (DeviceType type, sysDev *sd)
{
	//list each in this group, and call get_tooltip_system_struct for each one
	int a;
	sysDevStruct *sds;
	GString *temptipdevice = NULL;
	GString *tooltip = NULL;
	tooltip = g_string_new ("");

	for (a=0; a < sd->devices->len; a++) {
		sds = (sysDevStruct *) g_ptr_array_index (sd->devices, a);
		temptipdevice = get_tooltip_system_struct (type, sds);
		g_string_append_printf (tooltip, "%s\n", temptipdevice->str);
		g_string_free (temptipdevice, TRUE);
	}
	return tooltip;
}

/** Returns the tooltip for icon type
 *
 *  @return			The complete tooltip
 */
GString *
get_tooltips_system_device_type (GString *tooltip, DeviceType type)
{
	sysDev *sd;
	GString *temptip = NULL;
	sd = sysDevGet (type);
	if (sd->numberDevices > 0) {
		temptip = get_tooltips_system_device (type, sd);
		g_string_append (tooltip, temptip->str);
		g_string_free (temptip, TRUE);
	}
}

/** Returns the tooltip for the main icon. Text logic goes here :-)
 *
 *  @return			The complete tooltip
 */
GString *
get_full_tooltip (void)
{
	GString *tooltip = NULL;
	gint a;

	if (!onAcPower)
		tooltip = g_string_new (_("Computer is running on battery power\n"));
	else
		tooltip = g_string_new (_("Computer is running on AC power\n"));

	/* do each device type we have  */
	get_tooltips_system_device_type (tooltip, BATT_PRIMARY);
	get_tooltips_system_device_type (tooltip, BATT_UPS);
	get_tooltips_system_device_type (tooltip, BATT_MOUSE);
	get_tooltips_system_device_type (tooltip, BATT_KEYBOARD);
	get_tooltips_system_device_type (tooltip, BATT_PDA);

	/* remove the last \n */
	g_string_truncate (tooltip, tooltip->len-1);
	return tooltip;
}

/** Callback for "about" box
 *
 */
static void
callback_about_activated (void)
{
	const gchar *authors[] = {
		"Richard Hughes <richard@hughsie.com>",
		NULL};
	const gchar *documenters[] = {
		"Richard Hughes <richard@hughsie.com>",
		NULL};
	const gchar *artists[] = {
		"Diana Fong <dfong@redhat.com>",
		NULL};

	GtkWidget *about = gtk_about_dialog_new ();
	GdkPixbuf *logo = gdk_pixbuf_new_from_file (GPM_DATA "gnome-power.png", NULL);
	gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (about), "GNOME Power Manager");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (about), VERSION);
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (about),
		"\xc2\xa9 2005 Richard Hughes <richard@hughsie.com>");
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (about),
		"Power Manager for GNOME Desktop");
	gtk_about_dialog_set_license (GTK_ABOUT_DIALOG (about), GPLV2);
	gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (about), GPMURL);
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (about), authors);
	gtk_about_dialog_set_artists (GTK_ABOUT_DIALOG (about), artists);
	gtk_about_dialog_set_documenters (GTK_ABOUT_DIALOG (about), documenters);
	gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (about),
		GPMTRANSLATORS);
	gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (about), logo);
	gtk_widget_show (about);
	g_object_unref (logo);
}

/** Callback for actions boxes
 *
 *  @param	menuitem	The part of the menu that was clicked
 *  @param	user_data	Unused
 */
static void
callback_actions_activated (GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *action = NULL;

	action = g_object_get_data ((GObject*) menuitem, "action");
	g_assert (action);

	g_debug ("action = '%s'", action);
	if (strcmp (action, "suspend") == 0) {
		action_policy_do (ACTION_SUSPEND);
	} else if (strcmp (action, "hibernate") == 0) {
		action_policy_do (ACTION_HIBERNATE);
	} else if (strcmp (action, "about") == 0) {
		callback_about_activated ();
	} else if (strcmp (action, "info") == 0) {
		run_bin_program ("gnome-power-info");
	} else if (strcmp (action, "preferences") == 0) {
		run_bin_program ("gnome-power-preferences");
	} else if (strcmp (action, "help") == 0) {
		/* for now, show website */
		gnome_url_show (GPMURL, NULL);
	} else
		g_warning ("No handler for '%s'", action);
}

/** Returns the GtkWidget that is the notification icon
 *
 *  @return			Success, return FALSE when no icon present
 */
GtkWidget *
get_notification_icon (void)
{
	/* no asserts required, as we are allowed to be called when no icon */
	if (!eggtrayicon)
		return NULL;
	return GTK_WIDGET (eggtrayicon->image);
}

/** Function to set callbacks, and to get icons.
 *
 *  @param	menu		The menu
 *  @param	icon_name	The stock id of the icon
 *  @param	menu_label	The text title
 *  @param	action		The action to perform, e.g. hibernate
 */
static void
menu_add_action_item (GtkWidget *menu,
	const gchar *icon_name,
	const gchar *menu_label,
	const gchar *action)
{
	GtkWidget *item;
	GtkWidget *image;

	/* assertion checks */
	g_assert (menu);
	g_assert (icon_name);
	g_assert (menu_label);
	g_assert (action);

	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic (menu_label);

	/* set action data */
	g_object_set_data ((GObject*) item, "action", (gpointer) action);
	gtk_image_menu_item_set_image ((GtkImageMenuItem*) item, GTK_WIDGET (image));

	/* connect to the callback */
	g_signal_connect (G_OBJECT (item), "activate",
		G_CALLBACK (callback_actions_activated), (gpointer) menu);

	/* append to the menu, and show */
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

/** Creates right-click panel menu
 *
 *  @param	trayicon	A valid TrayIcon
 *  @return			Success
 */
static gboolean
menu_main_create (TrayData *trayicon)
{
	GtkWidget *item = NULL;

	g_assert (trayicon);
	g_assert (trayicon->popup_menu == NULL);

	trayicon->popup_menu = gtk_menu_new ();

	menu_add_action_item (trayicon->popup_menu, "gnome-dev-memory",
			      _("_Suspend"), "suspend");
	menu_add_action_item (eggtrayicon->popup_menu, "gnome-dev-harddisk",
			      _("Hi_bernate"), "hibernate");

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (trayicon->popup_menu), item);
	menu_add_action_item (trayicon->popup_menu, GTK_STOCK_DIALOG_INFO,
			      _("Po_wer Info"), "info");

	menu_add_action_item (trayicon->popup_menu, GTK_STOCK_PREFERENCES,
			      _("_Preferences"), "preferences");
	menu_add_action_item (trayicon->popup_menu, GTK_STOCK_HELP,
			      _("_Help"), "help");
	menu_add_action_item (trayicon->popup_menu, GTK_STOCK_ABOUT,
			      _("_About"), "about");
	gtk_widget_show (item);

	return TRUE;
}

/** private click release callback
 *
 *  @param	widget		Unused
 *  @param	event		The mouse button event
 *  @param	traydata	The TrayData object in use
 *  @return			If the popup-menu is already shown
 */
static gboolean
tray_icon_release (GtkWidget *widget, GdkEventButton *event, TrayData *traydata)
{
	/* assertion checks */
	g_assert (traydata);

	if (!traydata || !traydata->popup_menu)
		return TRUE;
	if (event->button == 3) {
		gtk_menu_popdown (GTK_MENU (traydata->popup_menu));
		return FALSE;
	}
	return TRUE;
}

/** private callback to position the popup menu of the tray icon
 *
 *  @param	menu		The menu to position		
 *  @param	x		The x coordinate of where to put the menu
 *  @param	y		The y coordinate of where to put the menu
 *  @param	push_in		Always set to true. We always want to see the complete menu
 *  @param	user_data	In this case the tray icon widget
 */
static void
tray_popup_position_menu (GtkMenu *menu,
			  int *x,
			  int *y,
			  gboolean *push_in,
			  gpointer user_data)
{
	GtkWidget *widget;
	GtkRequisition requisition;
	gint menu_xpos;
	gint menu_ypos;

	widget = GTK_WIDGET (user_data);

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

	menu_xpos += widget->allocation.x;
	menu_ypos += widget->allocation.y;

	if (menu_ypos > gdk_screen_get_height (gtk_widget_get_screen (widget)) / 2)
		menu_ypos -= (requisition.height + 1);
	else
		menu_ypos += widget->allocation.height + 1;

	*x = menu_xpos;
	*y = menu_ypos;
	*push_in = TRUE;
}

/** private click press callback
 *
 *  @param	widget		The widget on which was clicked
 *  @param	event		The mouse button event
 *  @param	traydata	The TrayData object in use
 *  @return			If the popup-menu is already shown
 */
static gboolean
tray_icon_press (GtkWidget *widget, GdkEventButton *event, TrayData *traydata)
{
	/* assertion checks */
	g_assert (traydata);

	if (!traydata || !(traydata->popup_menu))
		return TRUE;

	if (event->type == GDK_2BUTTON_PRESS)
	{
		run_bin_program ("gnome-power-preferences");
		return TRUE;
	}
	else if (event->button == 3) {
		gtk_menu_popup (GTK_MENU (traydata->popup_menu), NULL, NULL,
			tray_popup_position_menu, widget, event->button, event->time);
		return TRUE;
	}
	return FALSE;
}

/** Creates icon in the notification area
 *
 */
void
icon_create (void)
{
	GtkWidget *evbox = NULL;

	g_assert (!eggtrayicon);

	/* create new tray object */
	eggtrayicon = g_new0 (TrayData, 1);

	/* Event box */
	evbox = gtk_event_box_new ();
	eggtrayicon->evbox = evbox;
	eggtrayicon->tray_icon = egg_tray_icon_new (NICENAME);
	eggtrayicon->tray_icon_tooltip = gtk_tooltips_new ();
	eggtrayicon->popup_menu = NULL;

	eggtrayicon->image = gtk_image_new_from_stock (GPM_STOCK_AC_ADAPTER, GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_signal_connect (G_OBJECT (evbox), "button_press_event",
			  G_CALLBACK (tray_icon_press), (gpointer) eggtrayicon);
	g_signal_connect (G_OBJECT (evbox), "button_release_event",
			  G_CALLBACK (tray_icon_release), (gpointer) eggtrayicon);

	gtk_container_add (GTK_CONTAINER (evbox), eggtrayicon->image);
	gtk_container_add (GTK_CONTAINER (eggtrayicon->tray_icon), evbox);

	gtk_widget_show_all (GTK_WIDGET (eggtrayicon->tray_icon));
}

/** Update icon by showing it, hiding it, or modifying it, as applicable
 *
 */
void
gpn_icon_update (void)
{
	GConfClient *client = NULL;
	gboolean iconShow;
	gboolean iconShowAlways;
	gboolean use_notif_icon;
	GString *tooltip = NULL;
	gchar* stock_id;

	client = gconf_client_get_default ();
	/* do we want to display the icon */
	iconShow = gconf_client_get_bool (client,
				GCONF_ROOT "general/display_icon", NULL);

	stock_id = get_stock_id ();

	if (iconShow && stock_id) {
		if (!eggtrayicon) {
			/* create icon */
			icon_create ();
			if (!(eggtrayicon->popup_menu))
				menu_main_create (eggtrayicon);
		}

		gtk_image_set_from_stock (GTK_IMAGE (eggtrayicon->image),
			stock_id, GTK_ICON_SIZE_LARGE_TOOLBAR);
		g_free (stock_id);

		tooltip = get_full_tooltip ();
		gtk_tooltips_set_tip (eggtrayicon->tray_icon_tooltip,
			GTK_WIDGET (eggtrayicon->tray_icon),
			tooltip->str, NULL);
		g_string_free (tooltip, TRUE);
	} else {
		/* remove icon */
		g_debug ("The key " GCONF_ROOT "general/display_icon"
			   "and " GCONF_ROOT "general/display_icon_full "
			   " are both set to false, so no icon will be displayed");
		if (eggtrayicon) {
			icon_destroy (eggtrayicon);
			eggtrayicon = NULL;
		}
	}
}
