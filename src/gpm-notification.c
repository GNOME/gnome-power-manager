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
#include "compiler.h"

/* shared with gpm-main.c */
StateData state_data;
static TrayData *eggtrayicon = NULL;
GPtrArray *objectData;

/** Finds the icon index value for the percentage charge
 *
 *  @param	percent		The percentage value
 *  @return			A scale 0..8
 */
static gint
get_index_from_percent (gint percent)
{
	gint num;
	/* invalid input */
	if (percent < 0)
		return 0;
	if (percent > 100)
		return 8;
	/* work out value */
	num = ((percent + 4) * 8 ) / 100;
	if (num < 0)
		return 0;
	else if (num > 8)
		return 8;
	return num;
}

/** Gets an icon (pixbuf) suitable for the object
 *
 *  @param	slotData	A cached data object
 *  @return			A valid GdkPixbuf image
 */
static GdkPixbuf *
create_icon_pixbuf (GenericObject *slotData)
{
	GdkPixbuf *pixbuf = NULL;
	GenericObject slotDataVirt;
	gint num;
	gchar *computed_name = NULL;

	/* assertion checks */
	g_assert (slotData);

	if (slotData->powerDevice == POWER_PRIMARY_BATTERY) {
		/* have to work out for all objects for multibattery setups */
		slotDataVirt.percentageCharge = 100;
		create_virtual_of_type (objectData, &slotDataVirt, slotData->powerDevice);
		num = get_index_from_percent (slotDataVirt.percentageCharge);
		computed_name = g_strdup_printf ("gnome-power-%s-%d-of-8", 
					state_data.onBatteryPower ? "bat" : "ac", num);
		if (!gpm_icon_theme_fallback (&pixbuf, computed_name, 22))
			g_error ("Could not find %s!", computed_name);
		g_debug ("computed_name = %s", computed_name);
		g_assert (pixbuf != NULL);
		/* have to be careful when using g_free */
		g_assert (computed_name);
		g_free (computed_name);
	} else if (slotData->powerDevice == POWER_UPS) {
		num = get_index_from_percent (slotData->percentageCharge);
		computed_name = g_strdup_printf ("gnome-power-ups-%d-of-8", num);
		if (!gpm_icon_theme_fallback (&pixbuf, computed_name, 22))
			g_error ("Could not find %s!", computed_name);
		g_debug ("computed_name = %s", computed_name);
		g_assert (pixbuf != NULL);
		/* have to be careful when using g_free */
		g_assert (computed_name);
		g_free (computed_name);
	} else if (slotData->powerDevice == POWER_AC_ADAPTER) {
		if (!gpm_icon_theme_fallback (&pixbuf, "gnome-dev-acadapter", 22))
			g_error ("Could not find gnome-dev-acadapter!");
	} else {
		g_error ("create_icon_pixbuf called with unknown type %i!",
			slotData->powerDevice);
	}
	/* make sure we got something */
	if (!pixbuf)
		g_error ("Failed to get pixbuf.\n"
			 "Maybe GNOME Power Manager is not installed correctly!");
	return pixbuf;
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
 *  @param	slotData	A cached data object
 *  @return			Part of the tooltip
 */
GString *
get_object_tooltip (GenericObject *slotData)
{
	GString *remaining = NULL;
	GString *tooltip = NULL;
	gchar *devicestr = NULL;
	gchar *chargestate = NULL;

	/* assertion checks */
	g_assert (slotData);

	devicestr = convert_powerdevice_to_string (slotData->powerDevice);
	if (slotData->powerDevice == POWER_PRIMARY_BATTERY ||
	    slotData->powerDevice == POWER_UPS) {
		tooltip = g_string_new ("bug?");
		remaining = get_time_string (slotData);
		chargestate = get_chargestate_string (slotData);
		if (slotData->present) {
			g_string_printf (tooltip, "%s %s (%i%%)", 
					devicestr, chargestate, slotData->percentageCharge);
			if (remaining) {
				g_string_append_printf (tooltip, "\n%s", remaining->str);
				g_string_free (remaining, TRUE);
			}
		} else {
			g_string_printf (tooltip, "%s %s", 
					devicestr, chargestate);
		}
	} else if (slotData->powerDevice == POWER_KEYBOARD ||
		   slotData->powerDevice == POWER_MOUSE) {
		tooltip = g_string_new ("bug?");
		g_string_printf (tooltip, "%s (%i%%)", 
					devicestr, slotData->percentageCharge);
	} else if (slotData->powerDevice == POWER_AC_ADAPTER) {
		tooltip = g_string_new (devicestr);
	}
	return tooltip;
}

/** Returns the tooltip for the main icon. Text logic goes here :-)
 *
 *  @return			The complete tooltip
 */
GString *
get_full_tooltip (void)
{
	GenericObject *slotData = NULL;
	GString *tooltip = NULL;
	GString *temptip = NULL;
	gint a;

	if (state_data.onBatteryPower)
		tooltip = g_string_new (_("Computer is running on battery power"));
	else if (state_data.onUPSPower)
		tooltip = g_string_new (_("Computer is running on UPS power"));
	else
		tooltip = g_string_new (_("Computer is running on AC power"));

	for (a=0;a<objectData->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (objectData, a);
		temptip = get_object_tooltip (slotData);
		if (temptip && slotData->powerDevice != POWER_AC_ADAPTER)
			g_string_append_printf (tooltip, "\n%s", temptip->str);
	}
	return tooltip;
}

/** Gets an example icon for the taskbar
 *
 *  @param	powerDevice	The power type, e.g. POWER_UPS
 *  @param	displayFull	Should we display icons for full devices?
 *  @return			cached object pointer to the applicable data type
 */
static GenericObject *
get_object_of_powertype (int powerDevice, gboolean displayFull)
{
	GenericObject *slotData = NULL;
	gint a;
	/* return value only if not full, or iconDisplayFull set true*/
	for (a=0;a<objectData->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (objectData, a);
		if (slotData->powerDevice == powerDevice && slotData->present) {
			if (displayFull || slotData->percentageCharge != 100)
				return slotData;
		}
	}
	return NULL;
}


/** Finds the best selection for the icon in the notification area
 *
 *  @return  		The pointer to the main icon, or NULL if none needed
 *
 *  @note	Our preferred choice is:
 *	 	Battery, UPS, AC_ADAPTER
 *		PDA and others should never be a main icon.
 */
GenericObject *
get_main_icon_slot (void)
{
	GenericObject *slotData = NULL;
	GConfClient *client = gconf_client_get_default ();
	gboolean showIfFull = gconf_client_get_bool (client, 
		GCONF_ROOT "general/display_icon_full", NULL);

	slotData = get_object_of_powertype (POWER_PRIMARY_BATTERY, showIfFull);
	if (slotData)
		return slotData;

	slotData = get_object_of_powertype (POWER_UPS, showIfFull);
	if (slotData)
		return slotData;

	slotData = get_object_of_powertype (POWER_AC_ADAPTER, showIfFull);
	if (slotData)
		return slotData;

	g_warning ("Cannot find preferred main device."
		   "This may be because you are running on a desktop machine.");
	return NULL;
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

	/* assertion checks */
	g_assert (menuitem);

	action = g_object_get_data ((GObject*) menuitem, "action");
	g_assert (action);

	g_debug ("action = '%s'", action);
	if (strcmp (action, "suspend") == 0) {
		action_policy_do (ACTION_SUSPEND);
	} else if (strcmp (action, "hibernate") == 0) {
		action_policy_do (ACTION_HIBERNATE);
	} else if (strcmp (action, "about") == 0) {
		callback_about_activated ();
	} else if (strcmp (action, "system") == 0) {
		/* launch info */
		run_bin_program ("gnome-power-info");
	} else if (strcmp (action, "preferences") == 0) {
		/* launch preferences */
		run_bin_program ("gnome-power-preferences");
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
 *  @param	icon		The icon filename (no .png)
 *  @param	name		The text title
 *  @param	type		The type of menu item, e.g. hibernate
 *
 *  @todo	Need to work out why there is not stock_preferences
 */
static void
menu_add_action_item (GtkWidget *menu,
	const gchar *icon,
	const gchar *name,
	const gchar *type)
{
	/* get image */
	GtkWidget *item = NULL;
	GtkWidget *image = NULL;
	GdkPixbuf *pixbuf = NULL;

	/* assertion checks */
	g_assert (menu);
	g_assert (icon);
	g_assert (name);
	g_assert (type);

	image = gtk_image_new ();
	if (!gpm_icon_theme_fallback (&pixbuf, icon, 16))
		g_error ("Cannot find menu pixmap %s", icon);
	/* set image */
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	item = gtk_image_menu_item_new_with_label (name);

	/* set action data */
	g_object_set_data ((GObject*) item, "action", (gpointer) type);
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

	/* assertion checks */
	g_assert (trayicon);
	g_assert (trayicon->popup_menu == NULL);

	trayicon->popup_menu = gtk_menu_new ();

	/* add Preferences */
	/**  @bug	stock_preferences doesn't exist! */
	menu_add_action_item (trayicon->popup_menu, "stock-properties",
			      _("Preferences"), "preferences");
	/* add About */
	menu_add_action_item (trayicon->popup_menu, "stock_about",
			      _("About"), "about");
	/* add System */
	menu_add_action_item (trayicon->popup_menu, "stock_notebook",
			      _("System"), "system");
	/* add seporator */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (trayicon->popup_menu), item);
	gtk_widget_show (item);

	/* add the actions */
	menu_add_action_item (trayicon->popup_menu, "gnome-dev-memory",
			      _("Suspend"), "suspend");
	menu_add_action_item (eggtrayicon->popup_menu, "gnome-dev-harddisk",
			      _("Hibernate"), "hibernate");
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
	g_assert (widget);
	g_assert (event);
	g_assert (traydata);

	if (!traydata || !traydata->popup_menu)
		return TRUE;
	if (event->button == 3) {
		gtk_menu_popdown (GTK_MENU (traydata->popup_menu));
		return FALSE;
	}
	return TRUE;
}

/** private click press callback
 *
 *  @param	widget		Unused
 *  @param	event		The mouse button event
 *  @param	traydata	The TrayData object in use
 *  @return			If the popup-menu is already shown
 */
static gboolean
tray_icon_press (GtkWidget *widget, GdkEventButton *event, TrayData *traydata)
{
	/* assertion checks */
	g_assert (widget);
	g_assert (event);
	g_assert (traydata);

	g_debug ("button : %i", event->button);
	if (!traydata || !(traydata->popup_menu))
		return TRUE;
	if (event->button == 3) {
		gtk_menu_popup (GTK_MENU (traydata->popup_menu), NULL, NULL,
			NULL, NULL, event->button, event->time);
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

	/* assertion checks */
	g_assert (!eggtrayicon);

	/* create new tray object */
	eggtrayicon = g_new0 (TrayData, 1);

	/* Event box */
	evbox = gtk_event_box_new ();
	eggtrayicon->evbox = evbox;
	eggtrayicon->tray_icon = egg_tray_icon_new (NICENAME);
	eggtrayicon->tray_icon_tooltip = gtk_tooltips_new ();
	eggtrayicon->popup_menu = NULL;

	/* will produce a broken image.. */
	eggtrayicon->image = gtk_image_new_from_file ("");
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
	GenericObject *slotData = NULL;
	GdkPixbuf *pixbuf = NULL;
	GString *tooltip = NULL;

	client = gconf_client_get_default ();
	/* do we want to display the icon */
	iconShow = gconf_client_get_bool (client, 
				GCONF_ROOT "general/display_icon", NULL);
	/* do we need to force an icon? */
	iconShowAlways = gconf_client_get_bool (client, 
				GCONF_ROOT "general/display_icon_others", NULL);

	/* we may return NULL (which is okay) if no pixmap is available */
	slotData = get_main_icon_slot ();

	/* calculate logic */
	use_notif_icon = (iconShow && slotData) || (iconShow && iconShowAlways);

	if (use_notif_icon) {
		if (!eggtrayicon) {
			/* create icon */
			icon_create ();
			if (!(eggtrayicon->popup_menu))
				menu_main_create (eggtrayicon);
		}
		/* get pixbuf for icon */
		if (slotData) {
			/* use object to form icon */
			pixbuf = create_icon_pixbuf (slotData);
		} else {
			/* use standard fallback (the g-p-m icon) */
			if (!gpm_icon_theme_fallback (&pixbuf, "desktop-force", 24))
				g_error ("Cannot get iconShowAlways!");
		}
		if (!pixbuf)
			g_error ("Failed to get pixbuf for icon");
		gtk_image_set_from_pixbuf (GTK_IMAGE (eggtrayicon->image), pixbuf);
		g_object_unref (pixbuf);

		tooltip = get_full_tooltip ();
		gtk_tooltips_set_tip (eggtrayicon->tray_icon_tooltip,
			GTK_WIDGET (eggtrayicon->tray_icon),
			tooltip->str, NULL);
		g_string_free (tooltip, TRUE);
	} else {
		/* remove icon */
		g_warning ("The key " GCONF_ROOT "general/display_icon"
			   "and " GCONF_ROOT "general/display_icon_others"
			   " are both set to false, so no icon will be displayed");
		if (eggtrayicon) {
			icon_destroy (eggtrayicon);
			eggtrayicon = NULL;
		}
	}
}
