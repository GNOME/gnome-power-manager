/***************************************************************************
 *
 * gpm-notification.c : GNOME Power Notification 
 *         (Panel functions for GNOME Power Manager)
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gconf/gconf-client.h>
#include <gdk/gdk.h>
#include "gpm-common.h"
#include "gpm-main.h"
#include "gpm-notification.h"

/* shared with gpm.c */
HasData has_data;
StateData state_data;
SetupData setup;

static IconData icon_main, icon_low;
GPtrArray *objectData;

/** Calculate the color of the charge level bar
 *
 *  @param  level		the chargeLevel of the object, 0..100
 *  @return			the RGB colour
 */
static guint32
level_bar_get_color (guint level)
{
	guint8 red, green;
	gfloat c;

	red = green = 204; /* 0xCC */
	if (level < 50) {
		c = ((gfloat) level) / 50.0;
		green = (guint8) (gfloat) (204.0 * c);
	}
	if (level > 50) {
		c = (100.0 - (gfloat) level) / 50.0;
		red = (guint8) (gfloat) (204.0 * c);
	}
	return (0xff + red * 0x01000000 + green * 0x00010000);
}

static GdkPixbuf *
gtk_icon_theme_fallback (const char *name, int size)
{
	GdkPixbuf *pixbuf = NULL;
	GError *err = NULL;
	g_debug ("name = '%s'\n", name);
	if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), name)) {
		GtkIconInfo *iinfo = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (), name, size, GTK_ICON_LOOKUP_USE_BUILTIN);
		pixbuf = gtk_icon_info_load_icon (iinfo, &err);
		gtk_icon_info_free (iinfo);
	} else {
		/* 
		 * We cannot find this specific themed GNOME icon so use builtin
		 * fallbacks. This makes GPM more portible between distros
		 */
		GString *fallback = g_string_new ("error?");
		g_string_printf (fallback, "%s%s.png", GPM_DATA, name);
		pixbuf = gdk_pixbuf_new_from_file (fallback->str, &err);
	}
	return pixbuf;
}

#if 0
static gint
get_percentagecharge_batteries (void)
{
	GenericObject *slotData;
	gint a;
	gint batteryCount = 0;
	gint batteryCharge[5]; /* not going to get more than 5 batteries */

	for (a=0;a<objectData->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (objectData, a);
		if (slotData->powerDevice == POWER_PRIMARY_BATTERY && slotData->present) {
			batteryCharge[batteryCount] = slotData->percentageCharge;
			batteryCount++;
		}
	}
	/* no batteries */
	if (batteryCount == 0)
		return 0;

	/* short cut */
	if (batteryCount == 1)
		return batteryCharge[0];

	/* work out average */
	gint totalCharge = 0;
	for (a=0;a<batteryCount;a++) {
		totalCharge += batteryCharge[a];
	}
	
	return totalCharge / batteryCount;
}
#endif

/** Returns a virtual device that takes into account having more than one device
 *  that needs to be averaged. Currently we are calculating:
 *  percentageCharge and minutesRemaining only.
 *
 *  @param  slotDataReturn	the object returned. Must not be NULL
 *  @param  powerDevice		the object to be returned. Usually POWER_PRIMARY_BATTERY
 */
static void
create_virtual_of_type (GenericObject *slotDataReturn, gint powerDevice)
{
	g_assert (slotDataReturn);

	GenericObject *slotData;
	gint a;
	gint objectCount = 0;
	GenericObject *slotDataTemp[5]; /* not going to get more than 5 objects */

	for (a=0;a<objectData->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (objectData, a);
		if (slotData->powerDevice == powerDevice && slotData->present) {
			slotDataTemp[objectCount] = slotData;
			objectCount++;
		}
	}
	/* no objects */
	if (objectCount == 0) {
		g_warning ("create_virtual_of_type couldn't find device");
		slotDataReturn = NULL;
		return;
	}

	/* short cut */
	if (objectCount == 1) {
		slotDataReturn->percentageCharge = slotDataTemp[0]->percentageCharge;
		slotDataReturn->minutesRemaining = slotDataTemp[0]->minutesRemaining;
		return;
	}

	/* work out average */
	gint percentageCharge = 0;
	gint minutesRemaining = 0;
	for (a=0;a<objectCount;a++) {
		percentageCharge += slotDataTemp[a]->percentageCharge;
		minutesRemaining += slotDataTemp[a]->minutesRemaining;
	}
	slotDataReturn->percentageCharge = percentageCharge / objectCount;
	slotDataReturn->minutesRemaining = minutesRemaining / objectCount;

}

static GdkPixbuf *
create_icon_pixbuf (GenericObject *object)
{
	g_assert (object);
	GdkPixbuf *pixbuf = NULL;
	GError *err = NULL;
	/** TODO: make this set from gconf */
	const gboolean alwaysUseGenerated = FALSE;

	if (!alwaysUseGenerated && object->powerDevice == POWER_PRIMARY_BATTERY) {
		int num;
		gchar *computed_name;

		/* have to work out for all objects for multibattery setups */
		GenericObject slotDataVirt = {.percentageCharge = 100};
		create_virtual_of_type (&slotDataVirt, object->powerDevice);

		num = ((slotDataVirt.percentageCharge + 4) * 8 ) / 100;
		if (num < 0) num = 0;
		else if (num > 8) num = 8;
		computed_name = g_strdup_printf ("gnome-power-system%s-%d-of-8", 
						 state_data.onBatteryPower ? "" : "-ac", num);
		pixbuf = gtk_icon_theme_fallback (computed_name, 24);
		g_assert (pixbuf != NULL);
		g_free (computed_name);
	} else {
		gchar *name = convert_powerdevice_to_gnomeicon (object->powerDevice);
		g_assert (name);
		pixbuf = gtk_icon_theme_fallback (name, 24);
		g_assert (pixbuf);

		/* merge with AC emblem if needed */
		if (object->isCharging) {
			GdkPixbuf *emblem = gdk_pixbuf_new_from_file (GPM_DATA "emblem-ac.png", &err);
			g_assert (emblem);
			gdk_pixbuf_composite (emblem, pixbuf, 0, 0, 24, 24, 0, 0, 1.0, 1.0, GDK_INTERP_BILINEAR, 0xFF);
			g_object_unref (emblem);
		}
		
		/* merge with level bar if needed */
		if (object->isRechargeable) {
			GdkPixbuf *emblem = gdk_pixbuf_new_from_file (GPM_DATA "emblem-bar.png", &err);
			g_assert (emblem);
			gdk_pixbuf_composite (emblem, pixbuf, 0, 0, 24, 24, 0, 0, 1.0, 1.0, GDK_INTERP_BILINEAR, 0xFF);
			g_object_unref (emblem);

			/* have to work out for all objects for multibattery setups */
			GenericObject slotDataVirt = {.percentageCharge = 100};
			create_virtual_of_type (&slotDataVirt, object->powerDevice);

			gfloat c = ((gfloat) slotDataVirt.percentageCharge) / 100.0;
			guint h = (guint) (19.0 * c);
			if (h < 1) h = 1;
			if (h > 19) h = 19;

			GdkPixbuf *bar = gdk_pixbuf_new_subpixbuf (pixbuf, 20, 22 - h, 3, h);
			gdk_pixbuf_fill (bar, level_bar_get_color (slotDataVirt.percentageCharge));
			g_object_unref (bar);
		}
	}

	return pixbuf;
}

/** Frees resources and hides notification area icon
 *
 *  @param  td			Address of the icon
 *  @param  tooltip		The new tooltip
 */
void
icon_destroy (TrayData **td)
{
	g_assert (*td);
	g_debug ("icon_destroy");
	if ((*td)->popup_menu)
		g_free ((*td)->popup_menu);
	gtk_widget_hide_all (GTK_WIDGET ((*td)->tray_icon));
	g_free (*td);
	*td = NULL;
}

/* wrapper function */
void
gpn_icon_initialise ()
{
	IconData *icon;
	GConfClient *client = gconf_client_get_default ();

	icon = &icon_main;
	free_icon_structure (icon);
	icon->show = gconf_client_get_bool (client, GCONF_ROOT "general/displayIcon", NULL);
	icon->showIfFull = gconf_client_get_bool (client, GCONF_ROOT "general/displayIconFull", NULL);
}

/* wrapper function */
void
gpn_icon_destroy ()
{
	IconData *icon;
	icon = &icon_main;
	if (icon->td)
		icon_destroy (&(icon->td));
	free_icon_structure (icon);
}

/* wrapper function */
void
gpn_icon_update ()
{
	update_icon (&icon_main);
}

/* wrapper function */
void
callback_gconf_key_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	g_assert (client);
	g_assert (entry);
	g_debug ("callback_gconf_key_changed (%s)", entry->key);

	if (gconf_entry_get_value (entry) == NULL)
		return;

	if (strcmp (entry->key, GCONF_ROOT "general/displayIcon") == 0) {
		icon_main.show = gconf_client_get_bool (client, entry->key, NULL);
		gpn_icon_update ();
	} else if (strcmp (entry->key, GCONF_ROOT "general/displayIconFull") == 0) {
		icon_main.showIfFull = gconf_client_get_bool (client, entry->key, NULL);
		gpn_icon_update ();
	} else if (strcmp (entry->key, GCONF_ROOT "general/lowThreshold") == 0) {
		icon_low.displayOptions = gconf_client_get_int (client, entry->key, NULL);
		gpn_icon_update ();
	}
}

/** Gets the timestring from a slot object
 *
 *  @param  slotData		the GenericObject reference
 *  @return			the timestring, e.g. "13 minutes until charged"
 */
static GString *
get_time_string (GenericObject *slotData)
{
	g_assert (slotData);
	g_debug ("get_time_string");
	GString* timestring = NULL;
	timestring = get_timestring_from_minutes (slotData->minutesRemaining);
	if (!timestring)
		return NULL;
	if (slotData->isCharging)
		timestring = g_string_append (timestring, _(" until charged"));
	else
		timestring = g_string_append (timestring, _(" remaining"));

	return timestring;
}

GString *
get_tooltip_state (void)
{
	GString *tooltip;
	if (state_data.onBatteryPower)
		tooltip = g_string_new (_("Computer is running on battery power"));
	else if (state_data.onUPSPower)
		tooltip = g_string_new (_("Computer is running on UPS power"));
	else
		tooltip = g_string_new (_("Computer is running on AC power"));
	return tooltip;
}

GString *
get_object_tooltip (GenericObject *slotData)
{
	GString *tooltip = NULL;
	gchar *devicestr = convert_powerdevice_to_string (slotData->powerDevice);
	if (slotData->powerDevice == POWER_PRIMARY_BATTERY ||
	    slotData->powerDevice == POWER_UPS) {
		tooltip = g_string_new ("bug?");
		GString *remaining = get_time_string (slotData);
		gchar *chargestate = get_chargestate_string (slotData);
		g_string_printf (tooltip, "%s %s (%i%%)", 
					devicestr, chargestate, slotData->percentageCharge);
		if (remaining) {
			g_string_append_printf (tooltip, "\n%s", remaining->str);
			g_string_free (remaining, TRUE);
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
 */
GString *
get_main_tooltip (IconData *tdicon)
{
	g_assert (tdicon);
	g_assert (tdicon->slotData);
	GenericObject *slotData = NULL;
	GString *tooltip = NULL;
	gint a;

	tooltip = get_tooltip_state ();

	for (a=0;a<objectData->len;a++) {
		slotData = (GenericObject *) g_ptr_array_index (objectData, a);
		GString* temp = get_object_tooltip (slotData);
		if (temp && slotData->powerDevice != POWER_AC_ADAPTER)
			g_string_append_printf (tooltip, "\n%s", temp->str);
	}
	return tooltip;
}

static GenericObject *
get_object_of_powertype (int powerDevice, gboolean displayFull)
{
	GenericObject *slotData;
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


/** Finds the best selection for the "main" icon in the notification area
 *
 *  @return  		The pointer to the main icon, or NULL if none needed
 */
GenericObject *
get_main_icon_slot (void)
{
	GenericObject *slotData;
	/*
	 * Our preferred choice is:
	 * Battery, UPS, AC_ADAPTER
	 * PDA and others should never be a main icon.
	 */

	slotData = get_object_of_powertype (POWER_PRIMARY_BATTERY, icon_main.showIfFull);
	if (slotData)
		return slotData;

	slotData = get_object_of_powertype (POWER_UPS, icon_main.showIfFull);
	if (slotData)
		return slotData;

	slotData = get_object_of_powertype (POWER_AC_ADAPTER, icon_main.showIfFull);
	if (slotData)
		return slotData;

	g_warning ("Cannot find preferred main device");
	return NULL;
}

/** Frees icon structure
 *
 *  @param  tdicon		The icon pointer that needs to be freed
 */
void
free_icon_structure (IconData *tdicon)
{
	g_assert (tdicon);
	g_debug ("free_icon_structure");
	if (tdicon->tooltip)
		g_string_free (tdicon->tooltip, TRUE);
	tdicon->tooltip = NULL;
	tdicon->td = NULL;
}

/** Callback for actions boxes
 *
 */
static void
callback_actions_activated (GtkMenuItem *menuitem, gpointer user_data)
{
	char *action = g_object_get_data ((GObject*) menuitem, "action");
	g_debug ("action = '%s'", action);
	if (strcmp (action, "shutdown") == 0)
		action_policy_do (ACTION_SHUTDOWN);
	else if (strcmp (action, "reboot") == 0)
		action_policy_do (ACTION_REBOOT);
	else if (strcmp (action, "suspend") == 0)
		action_policy_do (ACTION_SUSPEND);
	else if (strcmp (action, "hibernate") == 0)
		action_policy_do (ACTION_HIBERNATE);
	else
		g_warning ("No handler for '%s'", action);
}

/** Callback for "about" box
 *
 */
static void
callback_about_activated (GtkMenuItem *menuitem, gpointer user_data)
{
	g_debug ("callback_about_activated");
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors[] = { "Richard Hughes <richard@hughsie.com>", NULL };
	const gchar *documenters[] = { NULL };

	if (about)
	{
		gdk_window_raise (about->window);
		gdk_window_show (about->window);
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (GPM_DATA "battery-48.png", NULL);
	about = gnome_about_new(NICENAME, VERSION,
			"Copyright \xc2\xa9 2005 Richard Hughes",
			_(NICEDESC),
			(const char **)authors,
			(const char **)documenters,
			NULL,
			pixbuf);

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);

	g_signal_connect (G_OBJECT (about), "destroy", 
			  G_CALLBACK (gtk_widget_destroyed), &about);
	g_object_add_weak_pointer (G_OBJECT (about), (void**)&(about));
	gtk_widget_show(about);
}

/** Callback for quit
 *
 */
static void
callback_quit_activated (GtkMenuItem *menuitem, gpointer user_date)
{
	g_debug ("callback_quit_activated");
	GnomeClient *master;
	GnomeClientFlags flags;
	master = gnome_master_client ();
	flags = gnome_client_get_flags (master);
	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		gnome_client_set_restart_style (master,	GNOME_RESTART_NEVER);
		gnome_client_flush (master);
	}
	gpn_icon_destroy ();
	exit (0);
}

/** Callback for preferences
 *
 */
static void
callback_prefs_activated (GtkMenuItem *menuitem, gpointer user_data)
{
	g_debug ("callback_prefs_activated");
	gboolean retval;
	gchar *path;

	path = g_strconcat (BINDIR, "/", "gnome-power-preferences", NULL);
	retval = g_spawn_command_line_async (path, NULL);
	if (retval == FALSE)
		g_warning ("Couldn't execute command: %s", path);
	g_free (path);
}

static void
menu_add_separator_item (GtkWidget *menu)
{
	GtkWidget *item;
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static void
menu_add_action_item (GtkWidget *menu, const char *icon, const char *name, char *type)
{
	/* get image */
	GtkWidget *image = gtk_image_new ();
	GdkPixbuf *pixbuf = gtk_icon_theme_fallback (icon, 16);
	g_assert (pixbuf);
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

	GtkWidget *item = gtk_image_menu_item_new_with_label (name);
	if (type)
		g_object_set_data ((GObject*) item, "action", (gpointer) type);
	gtk_image_menu_item_set_image ((GtkImageMenuItem*) item, GTK_WIDGET (image));
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (callback_actions_activated), (gpointer) menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

/** Creates panel menu
 *
 *  @return			MenuData object
 */
static void
menu_main_create (TrayData *td)
{
	g_assert (td);
	g_assert (td->popup_menu == NULL);
	g_debug ("menu_main_create");
	GtkWidget *item;
	td->popup_menu = gtk_menu_new ();

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES, NULL);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (callback_prefs_activated), (gpointer) td->popup_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (td->popup_menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_from_stock (GNOME_STOCK_ABOUT, NULL);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (callback_about_activated), (gpointer) td->popup_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (td->popup_menu), item);
	gtk_widget_show (item);

	if (0) {
		item = gtk_image_menu_item_new_with_label ("LCD Brightness");
		GtkWidget *image = gtk_image_new ();
		GdkPixbuf *pixbuf = gtk_icon_theme_fallback ("brightness-100", 16);
		gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
		gtk_image_menu_item_set_image ((GtkImageMenuItem*) item, GTK_WIDGET (image));
		gtk_menu_shell_append (GTK_MENU_SHELL (td->popup_menu), item);
		gtk_widget_show(item);

		GtkWidget *submenu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM(item), submenu);

		menu_add_action_item (submenu, "brightness-000", "0%", "brightness000");
		menu_add_action_item (submenu, "brightness-025", "25%", "brightness025");
		menu_add_action_item (submenu, "brightness-050", "50%", "brightness050");
		menu_add_action_item (submenu, "brightness-075", "75%", "brightness075");
		menu_add_action_item (submenu, "brightness-100", "100%", "brightness100");
	}

	if (setup.hasActions) {
		menu_add_separator_item (td->popup_menu);
		if (!setup.lockdownReboot)
			menu_add_action_item (td->popup_menu, "gnome-reboot",
					      _("Reboot"), "reboot");
		if (!setup.lockdownShutdown)
			menu_add_action_item (td->popup_menu, "gnome-shutdown",
					      _("Shutdown"), "shutdown");
		if (!setup.lockdownSuspend)
			menu_add_action_item (td->popup_menu, "gnome-dev-memory",
					      _("Suspend"), "suspend");
		if (!setup.lockdownHibernate)
			menu_add_action_item (td->popup_menu, "gnome-dev-harddisk",
					      _("Hibernate"), "hibernate");
	}
	if (setup.hasQuit) {
		menu_add_separator_item (td->popup_menu);
		item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
		g_signal_connect (G_OBJECT (item), "activate", 
				  G_CALLBACK(callback_quit_activated), (gpointer) td->popup_menu);
		gtk_widget_show(item);
		gtk_menu_shell_append (GTK_MENU_SHELL (td->popup_menu), item);
	}
}

/** private click release callback
 *
 */
static gboolean
tray_icon_release (GtkWidget *widget, GdkEventButton *event, TrayData *td)
{
	if (!td || !td->popup_menu)
		return TRUE;
	if (event->button == 3) {
		gtk_menu_popdown (GTK_MENU (td->popup_menu));
		return FALSE;
	}
	return TRUE;
}

/** private click press callback
 *
 */
static gboolean
tray_icon_press (GtkWidget *widget, GdkEventButton *event, TrayData *td)
{
	g_debug ("button : %i", event->button);
	if (!td || !td->popup_menu)
		return TRUE;
	if (event->button == 3) {
		gtk_menu_popup (GTK_MENU (td->popup_menu), NULL, NULL, NULL, 
			NULL, event->button, event->time);
		return TRUE;
	}
	return FALSE;
}

/** Creates icon in the notification area
 *
 *  @param  td			Address of the icon
 *  @param  filename		The filename of the icon
 */
void
icon_create (TrayData **td)
{
	g_assert (*td == NULL);
	g_debug ("icon_create");
	GtkWidget *evbox;

	/* create new tray object */
	*td = g_new0 (TrayData, 1);

	/* Event box */
	evbox = gtk_event_box_new ();
	(*td)->evbox = evbox;
	(*td)->tray_icon = egg_tray_icon_new (NICENAME);
	(*td)->tray_icon_tooltip = gtk_tooltips_new ();
	(*td)->popup_menu = NULL;

#if 0
	/* image */
	gchar *fullpath = g_strconcat (GPM_DATA, filename, NULL);
	(*td)->image = gtk_image_new_from_file (fullpath);
	g_free (fullpath);
#endif
	/* will produce a broken image.. */
	(*td)->image = gtk_image_new_from_file ("");
	g_signal_connect (G_OBJECT (evbox), "button_press_event", 
			  G_CALLBACK (tray_icon_press), (gpointer) *td);
	g_signal_connect (G_OBJECT (evbox), "button_release_event", 
			  G_CALLBACK (tray_icon_release), (gpointer) *td);

	gtk_container_add (GTK_CONTAINER (evbox), (*td)->image);
	gtk_container_add (GTK_CONTAINER ((*td)->tray_icon), evbox);

	gtk_widget_show_all (GTK_WIDGET ((*td)->tray_icon));
}

/** Update icon by showing it, hiding it, or modifying it, as applicable
 *
 *  @param  tdicon		The icon pointer that needs to be updated
 */
void
update_icon (IconData *tdicon)
{
	g_assert (tdicon);
	g_debug ("update_icon");
	if (tdicon->tooltip)
		g_string_free (tdicon->tooltip, TRUE);
	tdicon->tooltip = NULL;

	tdicon->slotData = get_main_icon_slot ();
	if (tdicon->slotData)
		tdicon->tooltip = get_main_tooltip (&icon_main);

	if (tdicon->show && tdicon->slotData) {
		if (!tdicon->td) {
			icon_create (&(tdicon->td));
			if (!(tdicon->td->popup_menu))
				menu_main_create (tdicon->td);
		}

		GdkPixbuf *pixbuf = create_icon_pixbuf (tdicon->slotData);
		gtk_image_set_from_pixbuf (GTK_IMAGE (tdicon->td->image), pixbuf);
		g_object_unref (pixbuf);

		if (tdicon->tooltip)
			gtk_tooltips_set_tip (tdicon->td->tray_icon_tooltip, 
				GTK_WIDGET (tdicon->td->tray_icon), 
				tdicon->tooltip->str, NULL);
	} else {
		if (tdicon->td) {
			icon_destroy (&(tdicon->td));
			free_icon_structure (tdicon);
		}
	}
}
