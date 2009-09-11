/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2009 Richard Hughes <richard@hughsie.com>
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
#include <gconf/gconf-client.h>
#include <devkit-power-gobject/devicekit-power.h>

#include "egg-debug.h"

#include "gpm-devicekit.h"
#include "gpm-engine.h"
#include "gpm-control.h"
#include "gpm-common.h"
#include "gpm-stock-icons.h"
#include "gpm-tray-icon.h"

static void     gpm_tray_icon_finalize   (GObject	   *object);

#define GPM_TRAY_ICON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_TRAY_ICON, GpmTrayIconPrivate))

struct GpmTrayIconPrivate
{
	GConfClient		*conf;
	GpmControl		*control;
	GpmEngine		*engine;
	GtkStatusIcon		*status_icon;
	gboolean		 show_suspend;
	gboolean		 show_hibernate;
	gboolean		 show_context_menu;
};

enum {
	SUSPEND,
	HIBERNATE,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmTrayIcon, gpm_tray_icon, G_TYPE_OBJECT)

/**
 * gpm_tray_icon_enable_suspend:
 * @enabled: If we should enable (i.e. show) the suspend icon
 **/
static void
gpm_tray_icon_enable_suspend (GpmTrayIcon *icon, gboolean enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	icon->priv->show_suspend = enabled;
}

/**
 * gpm_tray_icon_enable_hibernate:
 * @enabled: If we should enable (i.e. show) the hibernate icon
 **/
static void
gpm_tray_icon_enable_hibernate (GpmTrayIcon *icon, gboolean enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	icon->priv->show_hibernate = enabled;
}

/**
 * gpm_tray_icon_enable_context_menu:
 **/
static void
gpm_tray_icon_enable_context_menu (GpmTrayIcon *icon, gboolean enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	icon->priv->show_context_menu = enabled;
}

/**
 * gpm_tray_icon_show:
 * @enabled: If we should show the tray
 **/
static void
gpm_tray_icon_show (GpmTrayIcon *icon, gboolean enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	gtk_status_icon_set_visible (icon->priv->status_icon, enabled);
}

/**
 * gpm_tray_icon_set_tooltip:
 * @tooltip: The tooltip text, e.g. "Batteries fully charged"
 **/
gboolean
gpm_tray_icon_set_tooltip (GpmTrayIcon *icon, const gchar *tooltip)
{
	g_return_val_if_fail (icon != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), FALSE);
	g_return_val_if_fail (tooltip != NULL, FALSE);

#if GTK_CHECK_VERSION(2,15,0)
	gtk_status_icon_set_tooltip_text (icon->priv->status_icon, tooltip);
#else
	gtk_status_icon_set_tooltip (icon->priv->status_icon, tooltip);
#endif
	return TRUE;
}

/**
 * gpm_tray_icon_get_status_icon:
 **/
GtkStatusIcon *
gpm_tray_icon_get_status_icon (GpmTrayIcon *icon)
{
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), NULL);
	return g_object_ref (icon->priv->status_icon);
}

/**
 * gpm_tray_icon_set_image_from_stock:
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
		gtk_status_icon_set_from_icon_name (icon->priv->status_icon, filename);

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
 **/
static void
gpm_tray_icon_show_info_cb (GtkMenuItem *item, gpointer data)
{
	gchar *icon_name = NULL;
	gchar *description = NULL;
	GtkWidget *dialog;
	GtkWidget *image;
	const gchar *object_path;
	DkpDevice *device = NULL;
	gboolean ret;

	object_path = g_object_get_data (G_OBJECT (item), "object-path");
	egg_debug ("object_path=%s", object_path);
	if (object_path == NULL) {
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s",
						 _("Device information"));
		gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
							    "%s", _("There is no detailed information for this device"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (GTK_WIDGET (dialog));
		goto out;
	}

	device = dkp_device_new ();
	ret = dkp_device_set_object_path (device, object_path, NULL);
	if (!ret)
		goto out;
	icon_name = gpm_devicekit_get_object_icon (device);
	description = gpm_devicekit_get_object_description (device);

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

out:
	if (device != NULL)
		g_object_unref (device);
	g_free (description);
	g_free (icon_name);
}

/**
 * gpm_tray_icon_hibernate_cb:
 * @action: A valid GtkAction
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
 **/
static void
gpm_tray_icon_show_statistics_cb (GtkMenuItem *item, gpointer data)
{
	gchar *path;

	path = g_build_filename (BINDIR, "gnome-power-statistics", NULL);
	if (g_spawn_command_line_async (path, NULL) == FALSE)
		egg_warning ("Couldn't execute command: %s", path);
	g_free (path);
}

/**
 * gpm_tray_icon_show_preferences_cb:
 * @action: A valid GtkAction
 **/
static void
gpm_tray_icon_show_preferences_cb (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "gnome-power-preferences";

	if (g_spawn_command_line_async (command, NULL) == FALSE)
		egg_warning ("Couldn't execute command: %s", command);
}

/**
 * gpm_tray_icon_show_help_cb:
 * @action: A valid GtkAction
 **/
static void
gpm_tray_icon_show_help_cb (GtkMenuItem *item, gpointer data)
{
	gpm_help_display (NULL);
}

/**
 * gpm_tray_icon_show_about_cb:
 * @action: A valid GtkAction
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
  	if (!strcmp (translators, "translator-credits"))
		translators = NULL;

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
 *
 * We have to re-enable the tooltip when the popup is removed
 **/
static void
gpm_tray_icon_popup_cleared_cd (GtkWidget *widget, GpmTrayIcon *icon)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	egg_debug ("clear tray");
	g_object_ref_sink (widget);
	g_object_unref (widget);
}

/**
 * gpm_tray_icon_class_init:
 **/
static void
gpm_tray_icon_class_init (GpmTrayIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_tray_icon_finalize;
	g_type_class_add_private (klass, sizeof (GpmTrayIconPrivate));

	signals [SUSPEND] =
		g_signal_new ("suspend",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmTrayIconClass, suspend),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals [HIBERNATE] =
		g_signal_new ("hibernate",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmTrayIconClass, hibernate),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

/**
 * gpm_tray_icon_popup_menu_cb:
 *
 * Display the popup menu.
 **/
static void
gpm_tray_icon_popup_menu_cb (GtkStatusIcon *status_icon, guint button, guint32 timestamp, GpmTrayIcon *icon)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;
	gchar *path;
	gboolean ret;

	egg_debug ("icon right clicked");

	if (!icon->priv->show_context_menu)
		return;

	/* preferences */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_preferences_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* this program is optional, and may be in an uninstalled subpackage */
	path = g_build_filename (BINDIR, "gnome-power-statistics", NULL);
	ret = g_file_test (path, G_FILE_TEST_EXISTS);
	g_free (path);

	/* statistics */
	if (ret) {
		item = gtk_image_menu_item_new_with_mnemonic (_("Power _History"));
		image = gtk_image_new_from_icon_name (GPM_STOCK_STATISTICS, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (gpm_tray_icon_show_statistics_cb), icon);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* separator for HIG */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* help */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_HELP, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_help_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* about */
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
	if (button == 0)
		gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);

	g_signal_connect (GTK_WIDGET (menu), "hide",
			  G_CALLBACK (gpm_tray_icon_popup_cleared_cd), icon);
}

/**
 * gpm_tray_icon_add_device:
 **/
static guint
gpm_tray_icon_add_device (GpmTrayIcon *icon, GtkMenu *menu, const GPtrArray *array, DkpDeviceType type)
{
	guint i;
	guint added = 0;
	gchar *icon_name;
	gchar *label;
	GtkWidget *item;
	GtkWidget *image;
	const gchar *object_path;
	const gchar *desc;
	DkpDevice *device;
	DkpDeviceType type_tmp;
	gdouble percentage;

	/* find type */
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (array, i);

		/* get device properties */
		g_object_get (device,
			      "type", &type_tmp,
			      "percentage", &percentage,
			      NULL);

		if (type != type_tmp)
			continue;

		object_path = dkp_device_get_object_path (device);
		egg_debug ("adding device %s", object_path);
		added++;

		/* generate the label */
		desc = gpm_device_type_to_localised_text (type, 1);
		label = g_strdup_printf ("%s (%.1f%%)", desc, percentage);
		item = gtk_image_menu_item_new_with_label (label);

		/* generate the image */
		icon_name = gpm_devicekit_get_object_icon (device);
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), TRUE);

		/* callback and add the the menu */
		g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (gpm_tray_icon_show_info_cb), icon);
		g_object_set_data (G_OBJECT (item), "object-path", (gpointer) object_path);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		g_free (icon_name);
		g_free (label);
	}
	return added;
}

/**
 * gpm_tray_icon_activate_cb:
 * @button: Which buttons are pressed
 *
 * Callback when the icon is clicked
 **/
static void
gpm_tray_icon_activate_cb (GtkStatusIcon *status_icon, GpmTrayIcon *icon)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;
	guint dev_cnt = 0;
	GPtrArray *array;
	egg_debug ("icon left clicked");

	/* add all device types to the drop down menu */
	array = gpm_engine_get_devices (icon->priv->engine);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, DKP_DEVICE_TYPE_BATTERY);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, DKP_DEVICE_TYPE_UPS);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, DKP_DEVICE_TYPE_MOUSE);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, DKP_DEVICE_TYPE_KEYBOARD);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, DKP_DEVICE_TYPE_PDA);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, DKP_DEVICE_TYPE_PHONE);
	g_ptr_array_unref (array);

	/* nothing to display! */
	if (dev_cnt == 0 && !icon->priv->show_suspend && !icon->priv->show_hibernate)
		return;

	/* only do the seporator if we have at least one device and can do an action */
	if (dev_cnt != 0 && (icon->priv->show_suspend || icon->priv->show_hibernate)) {
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
 * gpm_conf_gconf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
gpm_conf_gconf_key_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, GpmTrayIcon *icon)
{
	GConfValue *value;
	gboolean enabled;
	gboolean allowed_in_menu;

	value = gconf_entry_get_value (entry);
	if (value == NULL)
		return;

	if (strcmp (entry->key, GPM_CONF_CAN_SUSPEND) == 0) {
		gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
		allowed_in_menu = gconf_client_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU, NULL);
		gpm_tray_icon_enable_suspend (icon, allowed_in_menu && enabled);

	} else if (strcmp (entry->key, GPM_CONF_CAN_HIBERNATE) == 0) {
		gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
		allowed_in_menu = gconf_client_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU, NULL);
		gpm_tray_icon_enable_hibernate (icon, allowed_in_menu && enabled);

	} else if (strcmp (entry->key, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU) == 0) {
		allowed_in_menu = gconf_value_get_bool (value);
		gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
		gpm_tray_icon_enable_suspend (icon, allowed_in_menu && enabled);
		gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
		gpm_tray_icon_enable_hibernate (icon, allowed_in_menu && enabled);
	} else if (strcmp (entry->key, GPM_CONF_UI_SHOW_CONTEXT_MENU) == 0) {
		allowed_in_menu = gconf_value_get_bool (value);
		gpm_tray_icon_enable_context_menu (icon, allowed_in_menu);
	}
}

/**
 * gpm_tray_icon_init:
 *
 * Initialise the tray object
 **/
static void
gpm_tray_icon_init (GpmTrayIcon *icon)
{
	gboolean enabled;
	gboolean allowed_in_menu;

	icon->priv = GPM_TRAY_ICON_GET_PRIVATE (icon);

	icon->priv->engine = gpm_engine_new ();

	/* use the policy object */
	icon->priv->control = gpm_control_new ();

	icon->priv->conf = gconf_client_get_default ();
	/* watch gnome-power-manager keys */
	gconf_client_add_dir (icon->priv->conf, GPM_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
	gconf_client_notify_add (icon->priv->conf, GPM_CONF_DIR,
				 (GConfClientNotifyFunc) gpm_conf_gconf_key_changed_cb,
				 icon, NULL, NULL);

	icon->priv->status_icon = gtk_status_icon_new ();
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "popup_menu",
				 G_CALLBACK (gpm_tray_icon_popup_menu_cb),
				 icon, 0);
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "activate",
				 G_CALLBACK (gpm_tray_icon_activate_cb),
				 icon, 0);

	/* only show the suspend and hibernate icons if we can do the action,
	   and the policy allows the actions in the menu */
	allowed_in_menu = gconf_client_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_ACTIONS_IN_MENU, NULL);
	gpm_control_allowed_suspend (icon->priv->control, &enabled, NULL);
	gpm_tray_icon_enable_suspend (icon, enabled && allowed_in_menu);
	gpm_control_allowed_hibernate (icon->priv->control, &enabled, NULL);
	gpm_tray_icon_enable_hibernate (icon, enabled && allowed_in_menu);

	allowed_in_menu = gconf_client_get_bool (icon->priv->conf, GPM_CONF_UI_SHOW_CONTEXT_MENU, NULL);
	gpm_tray_icon_enable_context_menu (icon, allowed_in_menu);
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

	g_object_unref (tray_icon->priv->control);
	g_object_unref (tray_icon->priv->status_icon);
	g_object_unref (tray_icon->priv->engine);
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

