/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkstatusicon.h>
#include <libgnomeui/gnome-help.h>

#include <libhal-gmanager.h>

#include "gpm-ac-adapter.h"
#include "gpm-battery.h"
#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-notify.h"
#include "gpm-control.h"
#include "gpm-power.h"
#include "gpm-stock-icons.h"
#include "gpm-tray-icon.h"

static void     gpm_tray_icon_class_init (GpmTrayIconClass *klass);
static void     gpm_tray_icon_init       (GpmTrayIcon      *tray_icon);
static void     gpm_tray_icon_finalize   (GObject	   *object);

#define GPM_TRAY_ICON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_TRAY_ICON, GpmTrayIconPrivate))

#define MAX_BATTERIES_PER_TYPE 5 /* the cosmetic restriction on the dropdown */

struct GpmTrayIconPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmConf			*conf;
	HalGManager		*hal_manager;
	GpmControl		*control;
	GpmPower		*power;
	GpmBattery		*battery;
	GpmNotify		*notify;

	GtkStatusIcon		*status_icon;
	guint			 low_percentage;
	gboolean		 is_visible;
	gboolean		 show_suspend;
	gboolean		 show_hibernate;
	gchar			*stock_id;
};

enum {
	SUSPEND,
	HIBERNATE,
	DESCRIPTION_CHANGED,
	ICON_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_MODE
};

static guint	 signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmTrayIcon, gpm_tray_icon, G_TYPE_OBJECT)

/**
 * gpm_tray_icon_enable_suspend:
 * @icon: This TrayIcon class instance
 * @enabled: If we should enable (i.e. show) the suspend icon
 **/
static void
gpm_tray_icon_enable_suspend (GpmTrayIcon *icon,
			      gboolean     enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	icon->priv->show_suspend = enabled;
}

/**
 * gpm_tray_icon_enable_hibernate:
 * @icon: This TrayIcon class instance
 * @enabled: If we should enable (i.e. show) the hibernate icon
 **/
static void
gpm_tray_icon_enable_hibernate (GpmTrayIcon *icon,
				gboolean     enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	icon->priv->show_hibernate = enabled;
}

/**
 * gpm_tray_icon_show:
 * @icon: This TrayIcon class instance
 * @enabled: If we should show the tray
 **/
static void
gpm_tray_icon_show (GpmTrayIcon *icon,
		    gboolean     enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	gtk_status_icon_set_visible (GTK_STATUS_ICON (icon->priv->status_icon), enabled);
	icon->priv->is_visible = enabled != FALSE;
}

/**
 * gpm_tray_icon_set_tooltip:
 * @icon: This TrayIcon class instance
 * @tooltip: The tooltip text, e.g. "Batteries fully charged"
 **/
void
gpm_tray_icon_set_tooltip (GpmTrayIcon  *icon,
			   const gchar  *tooltip)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	g_return_if_fail (tooltip != NULL);

	gtk_status_icon_set_tooltip (GTK_STATUS_ICON (icon->priv->status_icon), tooltip);
}

/**
 * gpm_tray_icon_set_image_from_stock:
 * @icon: This TrayIcon class instance
 * @stock_id: The icon name, e.g. GPM_STOCK_APP_ICON, or NULL to remove.
 *
 * Loads a pixmap from disk, and sets as the tooltip icon
 **/
void
gpm_tray_icon_set_image_from_stock (GpmTrayIcon *icon,
				    const gchar *stock_id)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	gpm_debug ("emitting icon-changed %s", stock_id);
	g_signal_emit (icon, signals [ICON_CHANGED], 0, stock_id);

	if (stock_id != NULL) {
		/* we only set a new icon if the name differs */
		if (strcmp (icon->priv->stock_id, stock_id) != 0) {
			gpm_debug ("Setting icon to %s", stock_id);
			gtk_status_icon_set_from_icon_name (GTK_STATUS_ICON (icon->priv->status_icon), stock_id);
			/* don't keep trying to set the same icon */
		        g_free (icon->priv->stock_id);
			icon->priv->stock_id = g_strdup (stock_id);
		}
		/* make sure that we are visible */
		gpm_tray_icon_show (icon, TRUE);
	} else {
		/* remove icon */
		gpm_debug ("no icon will be displayed");

		/* make sure that we are hidden */
		gpm_tray_icon_show (icon, FALSE);
	}
}

/**
 * gpm_tray_icon_show_info_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_info_cb (GtkMenuItem *item, gpointer data)
{
	GpmTrayIcon *icon = GPM_TRAY_ICON (data);
	const char *udi = g_object_get_data (G_OBJECT (item), "udi");
	GpmPowerDevice *device;
	char *msgicon;
	char *longdesc;

	gpm_debug ("udi=%s", udi);
	device = gpm_power_get_device_from_udi (icon->priv->power, udi);
	if (device == NULL) {
		return;
	}

	/* get long description */
	longdesc = gpm_power_status_for_device (device);
	msgicon = gpm_power_get_icon_from_status (&device->battery_status, device->battery_kind);
	gpm_notify_display (icon->priv->notify, _("Device information"), longdesc,
			    GPM_NOTIFY_TIMEOUT_NEVER, msgicon,
			    GPM_NOTIFY_URGENCY_LOW);
	g_free (longdesc);
	g_free (msgicon);
}

/**
 * gpm_tray_icon_hibernate_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_hibernate_cb (GtkMenuItem *item, gpointer data)
{
	GpmTrayIcon *icon = GPM_TRAY_ICON (data);
	gpm_debug ("emitting hibernate");
	g_signal_emit (icon, signals [HIBERNATE], 0);
}

/**
 * gpm_tray_icon_suspend_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_suspend_cb (GtkMenuItem *item, gpointer data)
{
	GpmTrayIcon *icon = GPM_TRAY_ICON (data);
	gpm_debug ("emitting suspend");
	g_signal_emit (icon, signals [SUSPEND], 0);
}

/**
 * gpm_tray_icon_show_statistics_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_statistics_cb (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "gnome-power-statistics";

	if (! g_spawn_command_line_async (command, NULL)) {
		gpm_warning ("Couldn't execute command: %s", command);
	}
}

/**
 * gpm_tray_icon_show_preferences_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_preferences_cb (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "gnome-power-preferences";

	if (! g_spawn_command_line_async (command, NULL)) {
		gpm_warning ("Couldn't execute command: %s", command);
	}
}

/**
 * gpm_tray_icon_show_help_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_help_cb (GtkMenuItem *item, gpointer data)
{
	GError *error = NULL;

	gnome_help_display ("gnome-power-manager.xml", NULL, &error);
	if (error != NULL) {
		gpm_warning (error->message);
		g_error_free (error);
	}
}

/**
 * gpm_tray_icon_show_about_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_about_cb (GtkMenuItem *item, gpointer data)
{
	const char *authors[] = {
		"Richard Hughes <richard@hughsie.com>",
		"William Jon McCann <mccann@jhu.edu>",
		"Jaap A. Haitsma <jaap@haitsma.org>",
		NULL};
	const char *documenters[] = {
		"Richard Hughes <richard@hughsie.com>",
		NULL};
	const char *artists[] = {
		"Diana Fong <dfong@redhat.com>",
		"Jakub Steiner <jimmac@ximian.com>",
		NULL};
	const char *license[] = {
		N_("Licensed under the GNU General Public License Version 2"),
		N_("Power Manager is free software; you can redistribute it and/or\n"
		   "modify it under the terms of the GNU General Public License\n"
		   "as published by the Free Software Foundation; either version 2\n"
		   "of the License, or (at your option) any later version."),
		N_("Power Manager is distributed in the hope that it will be useful,\n"
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License\n"
		   "along with this program; if not, write to the Free Software\n"
		   "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA\n"
		   "02110-1301, USA.")
	};
  	const char  *translators = _("translator-credits");
	char	    *license_trans;

	/* Translators comment: put your own name here to appear in the about dialog. */
  	if (!strcmp (translators, "translator-credits")) {
		translators = NULL;
	}

	license_trans = g_strconcat (_(license[0]), "\n\n", _(license[1]), "\n\n",
				     _(license[2]), "\n\n", _(license[3]), "\n",  NULL);

	gtk_window_set_default_icon_name (GPM_STOCK_APP_ICON);
	gtk_show_about_dialog (NULL,
			       "name", GPM_NAME,
			       "version", VERSION,
			       "copyright", "Copyright \xc2\xa9 2005-2007 Richard Hughes",
			       "license", license_trans,
			       "website", GPM_HOMEPAGE_URL,
			       "comments", GPM_DESCRIPTION,
			       "authors", authors,
			       "documenters", documenters,
			       "artists", artists,
			       "translator-credits", translators,
			       "logo-icon-name", GPM_STOCK_APP_ICON,
			       NULL);
	g_free (license_trans);
}

/**
 * gpm_tray_icon_popup_cleared_cd:
 * @widget: The popup Gtkwidget
 * @icon: This TrayIcon class instance
 *
 * We have to re-enable the tooltip when the popup is removed
 **/
static void
gpm_tray_icon_popup_cleared_cd (GtkWidget   *widget,
				GpmTrayIcon *icon)
{
//	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	gpm_debug ("clear tray (icon = %p)", icon);
}

/**
 * gpm_tray_icon_constructor:
 *
 * Connects the UI to the tray icon instance
 **/
static GObject *
gpm_tray_icon_constructor (GType		  type,
			   guint		  n_construct_properties,
			   GObjectConstructParam *construct_properties)
{
	GpmTrayIcon      *tray;
	GpmTrayIconClass *klass;

	klass = GPM_TRAY_ICON_CLASS (g_type_class_peek (GPM_TYPE_TRAY_ICON));

	tray = GPM_TRAY_ICON (G_OBJECT_CLASS (gpm_tray_icon_parent_class)->constructor
			      (type, n_construct_properties,
			       construct_properties));

	return G_OBJECT (tray);
}

/**
 * gpm_tray_icon_class_init:
 **/
static void
gpm_tray_icon_class_init (GpmTrayIconClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = gpm_tray_icon_finalize;
	object_class->constructor  = gpm_tray_icon_constructor;

	g_type_class_add_private (klass, sizeof (GpmTrayIconPrivate));

	signals [SUSPEND] =
		g_signal_new ("suspend",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmTrayIconClass, suspend),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals [HIBERNATE] =
		g_signal_new ("hibernate",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmTrayIconClass, hibernate),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals [DESCRIPTION_CHANGED] =
		g_signal_new ("description-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmTrayIconClass, description_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
	signals [ICON_CHANGED] =
		g_signal_new ("icon-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmTrayIconClass, icon_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
}

/**
 * gpm_tray_icon_popup_menu_cb:
 *
 * Display the popup menu.
 **/
static void
gpm_tray_icon_popup_menu_cb (GtkStatusIcon *status_icon,
			     guint          button,
			     guint32        timestamp,
			     GpmTrayIcon   *icon)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;

	gpm_debug ("icon right clicked");

	/* Preferences */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_preferences_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* Statistics */
	item = gtk_image_menu_item_new_with_mnemonic (_("Power _History"));
	image = gtk_image_new_from_icon_name (GPM_STOCK_STATISTICS, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_statistics_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* Separator for HIG? */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* Help */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_HELP, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_help_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* About */
	item = gtk_image_menu_item_new_with_mnemonic (_("_About"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_ABOUT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_about_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* show the menu */
	gtk_widget_show_all (GTK_WIDGET (menu));
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			gtk_status_icon_position_menu, status_icon,
			button, timestamp);
	if (button == 0) {
		gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
	}

	g_signal_connect (GTK_WIDGET (menu), "hide",
			  G_CALLBACK (gpm_tray_icon_popup_cleared_cd), icon);
}

/**
 * gpm_tray_icon_add_device:
 *
 * Add all the selected type of devices to the menu to form "drop down" info.
 **/
static guint
gpm_tray_icon_add_device (GpmTrayIcon *icon,
			  GtkMenu     *menu,
			  GpmPowerKind kind)
{
	GtkWidget *item;
	GtkWidget *image;
	GpmPowerDevice *device;
	guint i;
	gchar *icon_name;
	gchar *label;
	const gchar *desc;
	gint percentage;
	const gchar *udi;

	for (i=0; i<MAX_BATTERIES_PER_TYPE; i++) {
		device = gpm_power_get_battery_device_entry (icon->priv->power, kind, i);
		if (device == NULL) {
			break;
		}

		/* only add battery to list if present */
		udi = hal_gdevice_get_udi (device->hal_device);
		if (device->battery_status.is_present == FALSE) {
			gpm_debug ("not adding device '%s' as not present", udi);
			break;
		}
		gpm_debug ("adding device '%s'", udi);

		/* generate the label */
		percentage = device->battery_status.percentage_charge;
		desc = gpm_power_kind_to_localised_string (kind);
		label = g_strdup_printf ("%s (%i%%)", desc, percentage);
		item = gtk_image_menu_item_new_with_label (label);
		g_free (label);

		/* generate the image */
		icon_name = gpm_power_get_icon_from_status (&device->battery_status, kind);
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		g_free (icon_name);

		/* callback with the UDI and add the the menu */
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (gpm_tray_icon_show_info_cb), icon);
		g_object_set_data (G_OBJECT (item), "udi", (gpointer) udi);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	}
	return i;
}

/**
 * gpm_tray_icon_activate_cb:
 * @button: Which buttons are pressed
 * @icon: This TrayIcon class instance
 *
 * Callback when the icon is clicked
 **/
static void
gpm_tray_icon_activate_cb (GtkStatusIcon *status_icon,
			   GpmTrayIcon   *icon)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;
	guint dev_cnt = 0;

	gpm_debug ("icon left clicked");

	/* add all device types to the drop down menu */
	dev_cnt += gpm_tray_icon_add_device (icon, menu, GPM_POWER_KIND_PRIMARY);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, GPM_POWER_KIND_UPS);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, GPM_POWER_KIND_MOUSE);

	/* only do the seporator if we have at least one device */
	if (dev_cnt != 0) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* Suspend if available */
	if (icon->priv->show_suspend) {
		item = gtk_image_menu_item_new_with_mnemonic (_("_Suspend"));
		image = gtk_image_new_from_icon_name (GPM_STOCK_SUSPEND, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (gpm_tray_icon_suspend_cb), icon);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* Hibernate if available */
	if (icon->priv->show_hibernate) {
		item = gtk_image_menu_item_new_with_mnemonic (_("Hi_bernate"));
		image = gtk_image_new_from_icon_name (GPM_STOCK_HIBERNATE, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (gpm_tray_icon_hibernate_cb), icon);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* show the menu */
	gtk_widget_show_all (GTK_WIDGET (menu));
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			gtk_status_icon_position_menu, status_icon,
			1, gtk_get_current_event_time());

	g_signal_connect (GTK_WIDGET (menu), "hide",
			  G_CALLBACK (gpm_tray_icon_popup_cleared_cd), icon);
}

/**
 * get_stock_id:
 * @icon: This class instance
 * @icon_policy: The policy set from gconf
 *
 * Get the stock filename id after analysing the state of all the devices
 * attached to the computer, and applying policy from gconf.
 *
 * Return value: The icon filename, must free using g_free.
 **/
static char *
get_stock_id (GpmTrayIcon *icon,
	      guint	   icon_policy)
{
	GpmPowerStatus status_primary;
	GpmPowerStatus status_ups;
	GpmPowerStatus status_mouse;
	GpmPowerStatus status_keyboard;
	gboolean present;

	if (icon_policy == GPM_ICON_POLICY_NEVER) {
		gpm_debug ("The key " GPM_CONF_ICON_POLICY
			   " is set to never, so no icon will be displayed.\n"
			   "You can change this using gnome-power-preferences");
		return NULL;
	}

	/* Finds if a device was found in the cache AND that it is present */
	present = gpm_power_get_battery_status (icon->priv->power,
						GPM_POWER_KIND_PRIMARY,
						&status_primary);
	status_primary.is_present &= present;
	present = gpm_power_get_battery_status (icon->priv->power,
						GPM_POWER_KIND_UPS,
						&status_ups);
	status_ups.is_present &= present;
	present = gpm_power_get_battery_status (icon->priv->power,
						GPM_POWER_KIND_MOUSE,
						&status_mouse);
	status_mouse.is_present &= present;
	present = gpm_power_get_battery_status (icon->priv->power,
						GPM_POWER_KIND_KEYBOARD,
						&status_keyboard);
	status_keyboard.is_present &= present;

	/* we try CRITICAL: PRIMARY, UPS, MOUSE, KEYBOARD */
	gpm_debug ("Trying CRITICAL icon: primary, ups, mouse, keyboard");
	if (status_primary.is_present &&
	    status_primary.percentage_charge < icon->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_primary, GPM_POWER_KIND_PRIMARY);

	} else if (status_ups.is_present &&
		   status_ups.percentage_charge < icon->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_UPS);

	} else if (status_mouse.is_present &&
		   status_mouse.percentage_charge < icon->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_MOUSE);

	} else if (status_keyboard.is_present &&
		   status_keyboard.percentage_charge < icon->priv->low_percentage) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_KEYBOARD);
	}

	if (icon_policy == GPM_ICON_POLICY_CRITICAL) {
		gpm_debug ("no devices critical, so no icon will be displayed.");
		return NULL;
	}

	/* we try (DIS)CHARGING: PRIMARY, UPS */
	gpm_debug ("Trying CHARGING icon: primary, ups");
	if (status_primary.is_present &&
	    (status_primary.is_charging || status_primary.is_discharging) ) {
		return gpm_power_get_icon_from_status (&status_primary, GPM_POWER_KIND_PRIMARY);

	} else if (status_ups.is_present &&
		   (status_ups.is_charging || status_ups.is_discharging) ) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_UPS);
	}

	/* Check if we should just show the icon all the time */
	if (icon_policy == GPM_ICON_POLICY_CHARGE) {
		gpm_debug ("no devices (dis)charging, so no icon will be displayed.");
		return NULL;
	}

	/* we try PRESENT: PRIMARY, UPS */
	gpm_debug ("Trying PRESENT icon: primary, ups");
	if (status_primary.is_present) {
		return gpm_power_get_icon_from_status (&status_primary, GPM_POWER_KIND_PRIMARY);

	} else if (status_ups.is_present) {
		return gpm_power_get_icon_from_status (&status_ups, GPM_POWER_KIND_UPS);
	}

	/* Check if we should just fallback to the ac icon */
	if (icon_policy == GPM_ICON_POLICY_PRESENT) {
		gpm_debug ("no devices present, so no icon will be displayed.");
		return NULL;
	}

	/* we fallback to the ac_adapter icon */
	gpm_debug ("Using fallback");
	return g_strdup_printf (GPM_STOCK_AC_ADAPTER);
}

/**
 * gpm_tray_icon_sync:
 * @icon: This class instance
 *
 * Update the tray icon and set the correct tooltip when required, or remove
 * (hide) the icon when no longer required by policy.
 **/
void
gpm_tray_icon_sync (GpmTrayIcon *icon)
{
	gchar *stock_id = NULL;
	gchar *icon_policy_str;
	gint   icon_policy;

	/* do we want to display the icon */
	gpm_conf_get_string (icon->priv->conf, GPM_CONF_ICON_POLICY, &icon_policy_str);
	icon_policy = gpm_tray_icon_mode_from_string (icon_policy_str);

	g_free (icon_policy_str);

	/* try to get stock image */
	stock_id = get_stock_id (icon, icon_policy);

	gpm_debug ("Going to use stock id: %s", stock_id);

	gpm_tray_icon_set_image_from_stock (icon, stock_id);

	/* only create if we have a valid filename */
	if (stock_id) {
		gchar *tooltip = NULL;

		gpm_debug ("emitting description-changed %s", stock_id);
		g_signal_emit (icon, signals [DESCRIPTION_CHANGED], 0, stock_id);
		g_free (stock_id);

		gpm_power_get_status_summary (icon->priv->power, &tooltip, NULL);

		gpm_tray_icon_set_tooltip (icon, tooltip);
		g_free (tooltip);
	}
}

gboolean
gpm_ui_get_description (GpmTrayIcon  *icon,
		        gchar       **description,
		        GError      **error)
{
	gpm_debug ("org.gnome.PowerManager.UI.GetDescription");

	/* return tooltip */
	gpm_power_get_status_summary (icon->priv->power, description, NULL);
	return TRUE;
}

gboolean
gpm_ui_get_icon (GpmTrayIcon  *icon,
		 gchar       **iconname,
		 GError      **error)
{
	gchar *icon_policy_str;
	gint   icon_policy;

	/* do we want to display the icon */
	gpm_conf_get_string (icon->priv->conf, GPM_CONF_ICON_POLICY, &icon_policy_str);
	icon_policy = gpm_tray_icon_mode_from_string (icon_policy_str);

	g_free (icon_policy_str);

	gpm_debug ("org.gnome.PowerManager.UI.GetIcon");

	/* return icon name */
	*iconname = get_stock_id (icon, icon_policy);
	return TRUE;
}

/**
 * power_on_ac_changed_cb:
 * @power: The power class instance
 * @on_ac: if we are on AC power
 * @icon: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter     *ac_adapter,
		       GpmAcAdapterState state,
		       GpmTrayIcon      *icon)
{
	gpm_tray_icon_sync (icon);
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmTrayIcon *icon)
{
	gboolean    enabled;
	gboolean    allowed_in_menu;

	if (strcmp (key, GPM_CONF_ICON_POLICY) == 0) {
		gpm_tray_icon_sync (icon);

	} else if (strcmp (key, GPM_CONF_CAN_SUSPEND) == 0) {
		gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
		gpm_conf_get_bool (icon->priv->conf, GPM_CONF_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
		gpm_tray_icon_enable_suspend (icon, allowed_in_menu && enabled);

	} else if (strcmp (key, GPM_CONF_CAN_HIBERNATE) == 0) {
		gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
		gpm_conf_get_bool (icon->priv->conf, GPM_CONF_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
		gpm_tray_icon_enable_hibernate (icon, allowed_in_menu && enabled);

	} else if (strcmp (key, GPM_CONF_SHOW_ACTIONS_IN_MENU) == 0) {
		gpm_conf_get_bool (icon->priv->conf, GPM_CONF_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
		gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
		gpm_tray_icon_enable_suspend (icon, allowed_in_menu && enabled);
		gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
		gpm_tray_icon_enable_hibernate (icon, allowed_in_menu && enabled);
	}
}

/**
 * battery_removed_cb:
 * @battery: The battery class
 * @udi: The HAL udi of the device that was removed
 * @manager: This class instance
 **/
static void
battery_removed_cb (GpmBattery *battery,
		    const gchar *udi,
		    GpmTrayIcon *icon)
{
	gpm_debug ("Battery Removed: %s", udi);
	gpm_tray_icon_sync (icon);
}

/**
 * hal_daemon_monitor_cb:
 * @hal: The HAL class instance
 **/
static void
hal_daemon_monitor_cb (HalGManager *hal_manager,
		       GpmTrayIcon *icon)
{
	gpm_tray_icon_sync (icon);
}

/**
 * control_sleep_failure_cb:
 * @control: The control class instance
 * @icon: This icon class instance
 *
 * We have to notify the user is suspend failed
 **/
static void
control_sleep_failure_cb (GpmControl  *control,
			  GpmControlAction action,
		          GpmTrayIcon *icon)
{
	gchar *message;
	gboolean show_notify;
	gchar *title;

	/* We only show the HAL failed notification if set in gconf */
	gpm_conf_get_bool (icon->priv->conf, GPM_CONF_NOTIFY_HAL_ERROR, &show_notify);
	if (show_notify) {
		if (action == GPM_CONTROL_ACTION_HIBERNATE) {
			title = _("Hibernate Problem");
			message = g_strdup_printf (_("HAL failed to %s. "
						     "Check the help file for common problems."),
						     _("hibernate"));
			gpm_syslog ("hibernate failed");
		} else {
			message = g_strdup_printf (_("Your computer failed to %s.\n"
						     "Check the help file for common problems."),
						     _("suspend"));
			title = _("Suspend Problem");
			gpm_syslog ("suspend failed");
		}
		gpm_notify_display (icon->priv->notify,
				    title, message,
				    GPM_NOTIFY_TIMEOUT_LONG,
				    GTK_STOCK_DIALOG_WARNING,
				    GPM_NOTIFY_URGENCY_LOW);
		g_free (message);
	}
}

/**
 * gpm_tray_icon_init:
 * @icon: This TrayIcon class instance
 *
 * Initialise the tray object
 **/
static void
gpm_tray_icon_init (GpmTrayIcon *icon)
{
	gboolean enabled;
	gboolean allowed_in_menu;

	icon->priv = GPM_TRAY_ICON_GET_PRIVATE (icon);

	icon->priv->stock_id = g_strdup ("about-blank");

	/* we use power for the messages and the icon state */
	icon->priv->power = gpm_power_new ();

	/* use libnotify */
	icon->priv->notify = gpm_notify_new ();

	/* use the policy object */
	icon->priv->control = gpm_control_new ();
	g_signal_connect (icon->priv->control, "sleep-failure",
			  G_CALLBACK (control_sleep_failure_cb), icon);

	icon->priv->battery = gpm_battery_new ();
	/* we need these to refresh the tooltip and icon */
	g_signal_connect (icon->priv->battery, "battery-removed",
			  G_CALLBACK (battery_removed_cb), icon);

	/* we need this to refresh the tooltip and icon on hal restart */
	icon->priv->hal_manager = hal_gmanager_new ();
	g_signal_connect (icon->priv->hal_manager, "daemon-start",
			  G_CALLBACK (hal_daemon_monitor_cb), icon);
	g_signal_connect (icon->priv->hal_manager, "daemon-stop",
			  G_CALLBACK (hal_daemon_monitor_cb), icon);

	icon->priv->conf = gpm_conf_new ();
	g_signal_connect (icon->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), icon);

	/* get percentage policy */
	gpm_conf_get_uint (icon->priv->conf, GPM_CONF_LOW_PERCENTAGE, &icon->priv->low_percentage);

	/* we use ac_adapter so we can log the event */
	icon->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (icon->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), icon);

	icon->priv->status_icon = gtk_status_icon_new ();
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "popup_menu",
				 G_CALLBACK (gpm_tray_icon_popup_menu_cb),
				 icon, 0);
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "activate",
				 G_CALLBACK (gpm_tray_icon_activate_cb),
				 icon, 0);
	gpm_notify_use_status_icon (icon->priv->notify, icon->priv->status_icon);

	/* only show the suspend and hibernate icons if we can do the action,
	   and the policy allows the actions in the menu */
	gpm_conf_get_bool (icon->priv->conf, GPM_CONF_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
	gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
	gpm_tray_icon_enable_suspend (icon, enabled && allowed_in_menu);
	gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
	gpm_tray_icon_enable_hibernate (icon, enabled && allowed_in_menu);

	gpm_tray_icon_show (GPM_TRAY_ICON (icon), FALSE);
}

/**
 * gpm_tray_icon_finalize:
 * @object: This TrayIcon class instance
 **/
static void
gpm_tray_icon_finalize (GObject *object)
{
	GpmTrayIcon *tray_icon;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_TRAY_ICON (object));

	tray_icon = GPM_TRAY_ICON (object);

	if (tray_icon->priv->stock_id != NULL) {
		g_free (tray_icon->priv->stock_id);
	}
	if (tray_icon->priv->notify != NULL) {
		g_object_unref (tray_icon->priv->notify);
	}
	if (tray_icon->priv->control != NULL) {
		g_object_unref (tray_icon->priv->control);
	}
	if (tray_icon->priv->power != NULL) {
		g_object_unref (tray_icon->priv->power);
	}
	if (tray_icon->priv->status_icon != NULL) {
		g_object_unref (tray_icon->priv->status_icon);
	}
	if (tray_icon->priv->ac_adapter != NULL) {
		g_object_unref (tray_icon->priv->ac_adapter);
	}
	if (tray_icon->priv->battery != NULL) {
		g_object_unref (tray_icon->priv->battery);
	}
	if (tray_icon->priv->hal_manager != NULL) {
		g_object_unref (tray_icon->priv->hal_manager);
	}

	g_return_if_fail (tray_icon->priv != NULL);

	G_OBJECT_CLASS (gpm_tray_icon_parent_class)->finalize (object);
}

/**
 * gpm_tray_icon_new:
 * Return value: A new TrayIcon object.
 **/
GpmTrayIcon *
gpm_tray_icon_new (void)
{
	GpmTrayIcon *tray_icon;
	tray_icon = g_object_new (GPM_TYPE_TRAY_ICON, NULL);
	return GPM_TRAY_ICON (tray_icon);
}
