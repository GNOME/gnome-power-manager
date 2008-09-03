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

#include "gpm-conf.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-notify.h"
#include "gpm-cell-array.h"
#include "gpm-cell.h"
#include "gpm-cell-unit.h"
#include "gpm-stock-icons.h"
#include "gpm-tray-icon.h"

static void     gpm_tray_icon_class_init (GpmTrayIconClass *klass);
static void     gpm_tray_icon_init       (GpmTrayIcon      *tray_icon);
static void     gpm_tray_icon_finalize   (GObject	   *object);

#define GPM_TRAY_ICON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_TRAY_ICON, GpmTrayIconPrivate))

struct GpmTrayIconPrivate
{
	GpmConf			*conf;
	GpmControl		*control;
	GpmNotify		*notify;
	GpmEngineCollection	*collection;

	GtkStatusIcon		*status_icon;
	gboolean		 is_visible;
	gboolean		 show_suspend;
	gboolean		 show_hibernate;
	gboolean		 show_context_menu;
};

enum {
	SUSPEND,
	HIBERNATE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_MODE
};

static guint	 signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmTrayIcon, gpm_tray_icon, G_TYPE_OBJECT)

/**
 * gpm_tray_icon_set_collection_data:
 **/
gboolean
gpm_tray_icon_set_collection_data (GpmTrayIcon         *icon,
			           GpmEngineCollection *collection)
{
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), FALSE);

	icon->priv->collection = collection;
	return TRUE;
}

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

static void
gpm_tray_icon_enable_context_menu (GpmTrayIcon *icon,
				   gboolean     enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	icon->priv->show_context_menu = enabled;
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
gboolean
gpm_tray_icon_set_tooltip (GpmTrayIcon  *icon,
			   const gchar  *tooltip)
{
	g_return_val_if_fail (icon != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), FALSE);
	g_return_val_if_fail (tooltip != NULL, FALSE);

	gtk_status_icon_set_tooltip (GTK_STATUS_ICON (icon->priv->status_icon), tooltip);
	return TRUE;
}

/**
 * gpm_tray_icon_set_image_from_stock:
 * @icon: This TrayIcon class instance
 * @filename: The icon name, e.g. GPM_STOCK_APP_ICON, or NULL to remove.
 *
 * Loads a pixmap from disk, and sets as the tooltip icon
 **/
gboolean
gpm_tray_icon_set_icon (GpmTrayIcon *icon, const gchar *filename)
{
	g_return_val_if_fail (icon != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), FALSE);

	if (filename != NULL) {
		egg_debug ("Setting icon to %s", filename);
		gtk_status_icon_set_from_icon_name (GTK_STATUS_ICON (icon->priv->status_icon), filename);

		/* make sure that we are visible */
		gpm_tray_icon_show (icon, TRUE);
	} else {
		/* remove icon */
		egg_debug ("no icon will be displayed");

		/* make sure that we are hidden */
		gpm_tray_icon_show (icon, FALSE);
	}
	return TRUE;
}

/**
 * gpm_tray_icon_show_info_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_info_cb (GtkMenuItem *item, gpointer data)
{
	GpmCell *cell;
	GtkWidget *dialog;
	GtkWidget *image;
	gchar *icon_name;
	gchar *description;

	cell = g_object_get_data (G_OBJECT (item), "cell");

	/* get long description */
	description = gpm_cell_get_description (cell);
	icon_name = gpm_cell_get_icon (cell);

	image = gtk_image_new ();
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s",
					 _("Device information"));
	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
						    "%s", description);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
	gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_DIALOG);
	gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
	gtk_widget_show (image);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));

	g_free (description);
	g_free (icon_name);
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
	egg_debug ("emitting hibernate");
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
	egg_debug ("emitting suspend");
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

	if (g_spawn_command_line_async (command, NULL) == FALSE) {
		egg_warning ("Couldn't execute command: %s", command);
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

	if (g_spawn_command_line_async (command, NULL) == FALSE) {
		egg_warning ("Couldn't execute command: %s", command);
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
	gpm_help_display (NULL);
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
			       "version", VERSION,
			       "copyright", "Copyright \xc2\xa9 2005-2007 Richard Hughes",
			       "license", license_trans,
			       "website-label", _("GNOME Power Manager Website"),
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
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	egg_debug ("clear tray (icon = %p)", icon);
}

/**
 * gpm_tray_icon_class_init:
 **/
static void
gpm_tray_icon_class_init (GpmTrayIconClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	   = gpm_tray_icon_finalize;

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

	egg_debug ("icon right clicked");

	if (!icon->priv->show_context_menu)
		return;

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
gpm_tray_icon_add_device (GpmTrayIcon  *icon,
			  GtkMenu      *menu,
			  GpmCellArray *array)
{
	GtkWidget *item;
	GtkWidget *image;
	GpmCell *cell;
	GpmCellUnit *unit;
	guint i;
	gchar *icon_name;
	gchar *label;
	const gchar *desc;
	guint max;

	max = gpm_cell_array_get_num_cells (array);
	/* shortcut */
	if (max == 0) {
		return max;
	}

	for (i=0; i<max; i++) {
		cell = gpm_cell_array_get_cell (array, i);
		unit = gpm_cell_get_unit (cell);

		egg_debug ("adding device '%i'", i);

		/* generate the label */
		desc = gpm_cell_unit_get_kind_localised (unit, FALSE);
		label = g_strdup_printf ("%s (%.1f%%)", desc, unit->percentage);
		item = gtk_image_menu_item_new_with_label (label);
		g_free (label);

		/* generate the image */
		icon_name = gpm_cell_unit_get_icon (unit);
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		g_free (icon_name);

		/* callback with the UDI and add the the menu */
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (gpm_tray_icon_show_info_cb), icon);
		g_object_set_data (G_OBJECT (item), "cell", (gpointer) cell);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	return max;
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

	egg_debug ("icon left clicked");

	/* add all device types to the drop down menu */
	dev_cnt += gpm_tray_icon_add_device (icon, menu, icon->priv->collection->primary);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, icon->priv->collection->ups);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, icon->priv->collection->phone);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, icon->priv->collection->mouse);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, icon->priv->collection->keyboard);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, icon->priv->collection->pda);

	if (dev_cnt == 0 &&
	    icon->priv->show_suspend == FALSE &&
	    icon->priv->show_hibernate == FALSE) {
		/* nothing to display! */
		return;
	}

	/* only do the seporator if we have at least one device and can do an action */
	if (dev_cnt != 0 && (icon->priv->show_suspend || icon->priv->show_hibernate == TRUE)) {
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

	if (strcmp (key, GPM_CONF_CAN_SUSPEND) == 0) {
		gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
		gpm_conf_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
		gpm_tray_icon_enable_suspend (icon, allowed_in_menu && enabled);

	} else if (strcmp (key, GPM_CONF_CAN_HIBERNATE) == 0) {
		gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
		gpm_conf_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
		gpm_tray_icon_enable_hibernate (icon, allowed_in_menu && enabled);

	} else if (strcmp (key, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU) == 0) {
		gpm_conf_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
		gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
		gpm_tray_icon_enable_suspend (icon, allowed_in_menu && enabled);
		gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
		gpm_tray_icon_enable_hibernate (icon, allowed_in_menu && enabled);
	} else if (strcmp (key, GPM_CONF_UI_SHOW_CONTEXT_MENU) == 0) {
		gpm_conf_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_CONTEXT_MENU, &allowed_in_menu);
		gpm_tray_icon_enable_context_menu (icon, allowed_in_menu);
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

	icon->priv->collection = NULL;

	/* use libnotify */
	icon->priv->notify = gpm_notify_new ();

	/* use the policy object */
	icon->priv->control = gpm_control_new ();

	icon->priv->conf = gpm_conf_new ();
	g_signal_connect (icon->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), icon);

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
	gpm_conf_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU, &allowed_in_menu);
	gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
	gpm_tray_icon_enable_suspend (icon, enabled && allowed_in_menu);
	gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
	gpm_tray_icon_enable_hibernate (icon, enabled && allowed_in_menu);

	gpm_conf_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_CONTEXT_MENU, &allowed_in_menu);
	gpm_tray_icon_enable_context_menu (icon, allowed_in_menu);

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

	if (tray_icon->priv->notify != NULL) {
		g_object_unref (tray_icon->priv->notify);
	}
	g_object_unref (tray_icon->priv->control);
	g_object_unref (tray_icon->priv->status_icon);

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
