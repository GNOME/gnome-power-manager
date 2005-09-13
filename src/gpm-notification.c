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
#if defined(HAVE_LIBNOTIFY)
#include <libnotify/notify.h>
#endif

#include "gpm-common.h"
#include "gpm-main.h"
#include "gpm-notification.h"

/* shared with gpm-main.c */
StateData state_data;

static TrayData *eggtrayicon = NULL;
GPtrArray *objectData;

/** Get a image (pixbuf) trying the theme first, falling back to locally 
 * if not present. This means we do not have to check in configure.in for lots
 * of obscure icons.
 *
 *  @param  name	the icon name, e.g. gnome-battery
 *  @param  size	the icon size, e.g. 22
 */
static GdkPixbuf *
gtk_icon_theme_fallback (const char *name, int size)
{
	GdkPixbuf *pixbuf = NULL;
	GError *err = NULL;
	g_debug ("gtk_icon_theme_fallback : name = '%s', size = %i", name, size);
	if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), name)) {
		GtkIconInfo *iinfo = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (), name, size, GTK_ICON_LOOKUP_USE_BUILTIN);
		pixbuf = gtk_icon_info_load_icon (iinfo, &err);
		gtk_icon_info_free (iinfo);
	} else {
		/* 
		 * We cannot find this specific themed GNOME icon so use builtin
		 * fallbacks. This makes GPM more portible between distros
		 */
		GString *fallback;
		g_debug ("gtk_icon_theme_fallback: doing fallback as not found in theme");
		fallback = g_string_new ("error?");
		g_string_printf (fallback, "%s%s.png", GPM_DATA, name);
		pixbuf = gdk_pixbuf_new_from_file (fallback->str, &err);
	}
	return pixbuf;
}

/** Gets an icon (pixbuf) suitable for the object
 *
 *  @param  td			Address of the icon
 */
static GdkPixbuf *
create_icon_pixbuf (GenericObject *slotData)
{
	GdkPixbuf *pixbuf = NULL;
	GenericObject slotDataVirt;
	gint num;
	gchar *computed_name;

	g_assert (slotData);

	if (slotData->powerDevice == POWER_PRIMARY_BATTERY) {
		/* have to work out for all objects for multibattery setups */
		slotDataVirt.percentageCharge = 100;
		create_virtual_of_type (&slotDataVirt, slotData->powerDevice);

		num = ((slotDataVirt.percentageCharge + 4) * 8 ) / 100;
		if (num < 0)
			num = 0;
		else if (num > 8)
			num = 8;
		computed_name = g_strdup_printf ("gnome-power-system%s-%d-of-8", 
						 state_data.onBatteryPower ? "" : "-ac", num);
		pixbuf = gtk_icon_theme_fallback (computed_name, 22);
		g_debug ("computed_name = %s", computed_name);
		g_assert (pixbuf != NULL);
		g_free (computed_name);
	} else if (slotData->powerDevice == POWER_UPS) {
		num = ((slotData->percentageCharge + 4) * 8 ) / 100;
		if (num < 0)
			num = 0;
		else if (num > 8)
			num = 8;
		computed_name = g_strdup_printf ("gnome-power-system-ups-%d-of-8", num);
		pixbuf = gtk_icon_theme_fallback (computed_name, 22);
		g_debug ("computed_name = %s", computed_name);
		g_assert (pixbuf != NULL);
		g_free (computed_name);
	}

	return pixbuf;
}

/** Frees resources and hides notification area icon
 *
 *  @param  td			Address of the icon
 */
void
icon_destroy (void)
{
	g_return_if_fail (eggtrayicon);
	g_debug ("icon_destroy");
	if (eggtrayicon->popup_menu)
		g_free (eggtrayicon->popup_menu);
	if (eggtrayicon->tray_icon_tooltip)
		g_free (eggtrayicon->tray_icon_tooltip);
	gtk_widget_hide_all (GTK_WIDGET (eggtrayicon->tray_icon));
	g_free (eggtrayicon);
	eggtrayicon = NULL;
}

/* wrapper function */
void
gpn_icon_initialise ()
{
	eggtrayicon = NULL;
}

/* wrapper function */
void
gpn_icon_destroy ()
{
	if (eggtrayicon)
		icon_destroy ();
	eggtrayicon = NULL;
}

/* wrapper function */
void
callback_gconf_key_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	g_return_if_fail (client);
	g_return_if_fail (entry);
	g_debug ("callback_gconf_key_changed (%s)", entry->key);

	if (gconf_entry_get_value (entry) == NULL)
		return;

	if (strcmp (entry->key, GCONF_ROOT "general/display_icon") == 0) {
		gpn_icon_update ();
	} else if (strcmp (entry->key, GCONF_ROOT "general/display_icon_full") == 0) {
		gpn_icon_update ();
	}
}

GString *
get_object_tooltip (GenericObject *slotData)
{
	GString *remaining = NULL;
	GString *tooltip = NULL;
	gchar *devicestr;
	gchar *chargestate;

	g_return_val_if_fail (slotData, NULL);

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
 */
GString *
get_main_tooltip (void)
{
	GenericObject *slotData = NULL;
	GString *tooltip = NULL;
	GString* temptip;
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
 *  @param	powerDevice		the power type, e.g. POWER_UPS
 *  @param	displayFull		should we display icons for full devices?
 *  @return				pointer to the applicable data type
 */
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


/** Finds the best selection for the icon in the notification area
 *
 *  @return  		The pointer to the main icon, or NULL if none needed
 */
GenericObject *
get_main_icon_slot (void)
{
	GenericObject *slotData;
	GConfClient *client = gconf_client_get_default ();
	gboolean showIfFull = gconf_client_get_bool (client, GCONF_ROOT "general/display_icon_full", NULL);
	/*
	 * Our preferred choice is:
	 * Battery, UPS, AC_ADAPTER
	 * PDA and others should never be a main icon.
	 */

	slotData = get_object_of_powertype (POWER_PRIMARY_BATTERY, showIfFull);
	if (slotData)
		return slotData;

	slotData = get_object_of_powertype (POWER_UPS, showIfFull);
	if (slotData)
		return slotData;

	slotData = get_object_of_powertype (POWER_AC_ADAPTER, showIfFull);
	if (slotData)
		return slotData;

	g_warning ("Cannot find preferred main device. This may be because you are running on a desktop machine!");
	return NULL;
}

/** Callback for actions boxes
 *
 */
static void
callback_actions_activated (GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *action = g_object_get_data ((GObject*) menuitem, "action");
	g_debug ("action = '%s'", action);
	if (strcmp (action, "suspend") == 0)
		action_policy_do (ACTION_SUSPEND);
	else if (strcmp (action, "hibernate") == 0)
		action_policy_do (ACTION_HIBERNATE);
	else
		g_warning ("No handler for '%s'", action);
}

/** Gets the position to "point" to (i.e. bottom of the icon)
 *
 *  @param	x				X co-ordinate return
 *  @param	y				Y co-ordinate return
 *  @return					Success, return FALSE when no icon present
 *
 * TODO : Need to cope when panel is on left, right, or bottom of screen.
 */
gboolean
get_icon_position (gint *x, gint *y)
{
	GdkPixbuf* pixbuf;

	g_return_val_if_fail (eggtrayicon, FALSE);
	g_return_val_if_fail (eggtrayicon->image, FALSE);
	g_return_val_if_fail (eggtrayicon->image->window, FALSE);

	gdk_window_get_origin (eggtrayicon->image->window, x, y);
	g_debug ("x1=%i, y1=%i\n", *x, *y);

	pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (eggtrayicon->image));
	*x += (gdk_pixbuf_get_width (pixbuf) / 2);
	*y += gdk_pixbuf_get_height (pixbuf);
	g_debug ("x2=%i, y2=%i\n", *x, *y);
	return TRUE;
}

/** Callback for "about" box
 *
 */
static void
callback_about_activated (GtkMenuItem *menuitem, gpointer user_data)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors[] = { "Richard Hughes <richard@hughsie.com>", NULL };
	const gchar *documenters[] = { NULL };
	const gchar *translator = _("Unknown Translator");

	if (about) {
		gdk_window_raise (about->window);
		gdk_window_show (about->window);
		return;
	}

	/* no point displaying translator is it's me */
	if (strcmp (translator, "Unknown Translator") == 0)
		translator = NULL;

	pixbuf = gdk_pixbuf_new_from_file (GPM_DATA "gnome-power.png", NULL);
	about = gnome_about_new(NICENAME, VERSION,
			"Copyright \xc2\xa9 2005 Richard Hughes",
			_(NICEDESC),
			(const char **)authors,
			(const char **)documenters,
			(const char *)translator,
			pixbuf);

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);

	g_signal_connect (G_OBJECT (about), "destroy", 
			  G_CALLBACK (gtk_widget_destroyed), &about);
	g_object_add_weak_pointer (G_OBJECT (about), (void**)&(about));
	gtk_widget_show(about);
}

/** Callback for preferences
 *
 */
static void
callback_prefs_activated (GtkMenuItem *menuitem, gpointer user_data)
{
	gboolean retval;
	gchar *path;

	path = g_strconcat (BINDIR, "/", "gnome-power-preferences", NULL);
	g_debug ("callback_prefs_activated: %s", path);

	retval = g_spawn_command_line_async (path, NULL);
	if (retval == FALSE)
		g_warning ("Couldn't execute command: %s", path);
	g_free (path);
}

static void
menu_add_action_item (GtkWidget *menu, const char *icon, const char *name, char *type)
{
	/* get image */
	GtkWidget *item;
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	image = gtk_image_new ();
	pixbuf = gtk_icon_theme_fallback (icon, 16);

	g_return_if_fail (pixbuf);

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

	item = gtk_image_menu_item_new_with_label (name);
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
menu_main_create (void)
{
	GtkWidget *item;

	g_return_if_fail (eggtrayicon);
	g_return_if_fail (eggtrayicon->popup_menu == NULL);

	eggtrayicon->popup_menu = gtk_menu_new ();

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES, NULL);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (callback_prefs_activated), (gpointer) eggtrayicon->popup_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (eggtrayicon->popup_menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_from_stock (GNOME_STOCK_ABOUT, NULL);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (callback_about_activated), (gpointer) eggtrayicon->popup_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (eggtrayicon->popup_menu), item);
	gtk_widget_show (item);

#if 0
	GtkWidget *image;
	GdkPixbuf *pixbuf;
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (eggtrayicon->popup_menu), item);
	item = gtk_image_menu_item_new_with_label ("LCD Brightness");
	image = gtk_image_new ();
	pixbuf = gtk_icon_theme_fallback ("brightness-100", 16);
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	gtk_image_menu_item_set_image ((GtkImageMenuItem*) item, GTK_WIDGET (image));
	gtk_menu_shell_append (GTK_MENU_SHELL (eggtrayicon->popup_menu), item);
	gtk_widget_show(item);
	GtkWidget *submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM(item), submenu);
	gint a = hal_get_brightness_steps ();
	gint b;
	for (b = 0; b < a; b++) {
		menu_add_action_item (submenu, "brightness-000", "0%", "brightness000");
	}
#endif

	/* add the actions */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (eggtrayicon->popup_menu), item);
	menu_add_action_item (eggtrayicon->popup_menu, "gnome-dev-memory",
			      _("Suspend"), "suspend");
	menu_add_action_item (eggtrayicon->popup_menu, "gnome-dev-harddisk",
			      _("Hibernate"), "hibernate");
}

/** private click release callback
 *
 */
static gboolean
tray_icon_release (GtkWidget *widget, GdkEventButton *event, TrayData *ignore)
{
	if (!eggtrayicon || !eggtrayicon->popup_menu)
		return TRUE;
	if (event->button == 3) {
		gtk_menu_popdown (GTK_MENU (eggtrayicon->popup_menu));
		return FALSE;
	}
	return TRUE;
}

/** private click press callback
 *
 */
static gboolean
tray_icon_press (GtkWidget *widget, GdkEventButton *event, TrayData *ignore)
{
	g_debug ("button : %i", event->button);
	if (!eggtrayicon || !(eggtrayicon->popup_menu))
		return TRUE;
	if (event->button == 3) {
		gtk_menu_popup (GTK_MENU (eggtrayicon->popup_menu), NULL, NULL, NULL, 
			NULL, event->button, event->time);
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
	GtkWidget *evbox;

	g_return_if_fail (!eggtrayicon);

	/* create new tray object */
	eggtrayicon = g_new0 (TrayData, 1);

	/* Event box */
	evbox = gtk_event_box_new ();
	eggtrayicon->evbox = evbox;
	eggtrayicon->tray_icon = egg_tray_icon_new (NICENAME);
	eggtrayicon->tray_icon_tooltip = gtk_tooltips_new ();
	eggtrayicon->popup_menu = NULL;

#if 0
	/* image */
	gchar *fullpath = g_strconcat (GPM_DATA, filename, NULL);
	eggtrayicon->image = gtk_image_new_from_file (fullpath);
	g_free (fullpath);
#endif
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

	GConfClient *client;
	gboolean iconShow;
	GenericObject *slotData;
	GdkPixbuf *pixbuf;
	GString *tooltip;

	client = gconf_client_get_default ();
	iconShow = gconf_client_get_bool (client, GCONF_ROOT "general/display_icon", NULL);

	if (!iconShow)
		g_warning ("The key " GCONF_ROOT "general/display_icon is set to false, no icon will be displayed");

	slotData = get_main_icon_slot ();
	if (iconShow && slotData) {
		if (!eggtrayicon) {
			/* create icon */
			icon_create ();
			if (!(eggtrayicon->popup_menu))
				menu_main_create ();
		}
		/* modify icon */
		pixbuf = create_icon_pixbuf (slotData);
		gtk_image_set_from_pixbuf (GTK_IMAGE (eggtrayicon->image), pixbuf);
		g_object_unref (pixbuf);

		tooltip = get_main_tooltip ();
		gtk_tooltips_set_tip (eggtrayicon->tray_icon_tooltip,
			GTK_WIDGET (eggtrayicon->tray_icon),
			tooltip->str, NULL);
		g_string_free (tooltip, TRUE);
	} else {
		/* remove icon */
		if (eggtrayicon) {
			icon_destroy ();
			eggtrayicon = NULL;
		}
	}
}
