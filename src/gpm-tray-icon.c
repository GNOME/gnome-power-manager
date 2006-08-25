/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define USE_EGGTRAYICON		(GTK_MINOR_VERSION < 9)

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

#if ! USE_EGGTRAYICON
#include <gtk/gtkstatusicon.h>
#endif

#include <libgnomeui/gnome-help.h> /* for gnome_help_display */

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include "gpm-common.h" /* For GPM_HOMEPAGE_URL, etc */
#include "gpm-stock-icons.h"
#include "gpm-tray-icon.h"
#include "gpm-debug.h"

static void     gpm_tray_icon_class_init (GpmTrayIconClass *klass);
static void     gpm_tray_icon_init       (GpmTrayIcon      *tray_icon);
static void     gpm_tray_icon_finalize   (GObject	   *object);

#define GPM_TRAY_ICON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_TRAY_ICON, GpmTrayIconPrivate))

struct GpmTrayIconPrivate
{
	GtkUIManager	*ui_manager;
	GtkActionGroup	*actiongroup;
	GtkWidget	*popup_menu;
#if USE_EGGTRAYICON
	GtkTooltips	*tooltips;
	GtkWidget	*image;
	GtkWidget	*ebox;
#else
	GtkStatusIcon	*status_icon;
#endif

	gboolean	 show_notifications;
	gboolean	 is_visible;

	gboolean	 can_suspend;
	gboolean	 can_hibernate;

	char		*stock_id;
#ifdef HAVE_LIBNOTIFY
	NotifyNotification *notify;
#endif
};

enum {
	SUSPEND,
	HIBERNATE,
	SHOW_INFO,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_MODE
};

static void gpm_tray_icon_suspend_cb		(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_hibernate_cb		(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_info_cb	 	(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_preferences_cb	(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_help_cb		(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_about_cb		(GtkAction *action, GpmTrayIcon *icon);

static GtkActionEntry gpm_tray_icon_action_entries [] =
{
	{ "TraySuspend", GPM_STOCK_SUSPEND, N_("_Suspend"),
	  NULL, N_("Suspend the computer"), G_CALLBACK (gpm_tray_icon_suspend_cb) },
	{ "TrayHibernate", GPM_STOCK_HIBERNATE, N_("Hi_bernate"),
	  NULL, N_("Make the computer go to sleep"), G_CALLBACK (gpm_tray_icon_hibernate_cb) },
	{ "TrayPreferences", GTK_STOCK_PREFERENCES, N_("_Preferences"),
	  NULL, NULL, G_CALLBACK (gpm_tray_icon_show_preferences_cb) },
	{ "TrayInfo", GTK_STOCK_DIALOG_INFO, N_("_Information"),
	  NULL, NULL, G_CALLBACK (gpm_tray_icon_show_info_cb) },
	{ "TrayHelp", GTK_STOCK_HELP, N_("_Help"), NULL,
	  NULL, G_CALLBACK (gpm_tray_icon_show_help_cb) },
	{ "TrayAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
	  NULL, G_CALLBACK (gpm_tray_icon_show_about_cb) }
};
static guint gpm_tray_icon_n_action_entries = G_N_ELEMENTS (gpm_tray_icon_action_entries);

static guint	 signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmTrayIcon, gpm_tray_icon, EGG_TYPE_TRAY_ICON)

/**
 * gpm_tray_icon_enable_suspend:
 * @icon: This TrayIcon class instance
 * @enabled: If we should enable (i.e. show) the suspend icon
 **/
void
gpm_tray_icon_enable_suspend (GpmTrayIcon *icon,
			      gboolean     enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	if (icon->priv->can_suspend != enabled) {
		GtkAction *action;

		icon->priv->can_suspend = enabled;
		action = gtk_action_group_get_action (icon->priv->actiongroup,
						      "TraySuspend");
		gtk_action_set_visible (GTK_ACTION (action),
					enabled);
	}
}

/**
 * gpm_tray_icon_enable_hibernate:
 * @icon: This TrayIcon class instance
 * @enabled: If we should enable (i.e. show) the hibernate icon
 **/
void
gpm_tray_icon_enable_hibernate (GpmTrayIcon *icon,
				gboolean     enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	if (icon->priv->can_hibernate != enabled) {
		GtkAction *action;

		icon->priv->can_hibernate = enabled;
		action = gtk_action_group_get_action (icon->priv->actiongroup,
						      "TrayHibernate");
		gtk_action_set_visible (GTK_ACTION (action),
					enabled);

	}
}

/**
 * gpm_tray_icon_set_tooltip:
 * @icon: This TrayIcon class instance
 * @tooltip: The tooltip text, e.g. "Batteries fully charged"
 **/
void
gpm_tray_icon_set_tooltip (GpmTrayIcon *icon,
			   const char  *tooltip)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	g_return_if_fail (tooltip != NULL);

#if USE_EGGTRAYICON
	gtk_tooltips_set_tip (icon->priv->tooltips,
			      GTK_WIDGET (icon),
			      tooltip, NULL);
#else
	gtk_status_icon_set_tooltip (GTK_STATUS_ICON (icon->priv->status_icon),
				     tooltip);
#endif
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
				    const char  *stock_id)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	if (stock_id) {
		/* we only set a new icon if the name differs */
		if (strcmp (icon->priv->stock_id, stock_id) != 0) {
			gpm_debug ("Setting icon to %s", stock_id);
#if USE_EGGTRAYICON
			gtk_image_set_from_icon_name (GTK_IMAGE (icon->priv->image),
						      stock_id,
						      GTK_ICON_SIZE_LARGE_TOOLBAR);
#else
			gtk_status_icon_set_from_icon_name (GTK_STATUS_ICON (icon->priv->status_icon), stock_id);
			gtk_status_icon_set_visible (GTK_STATUS_ICON (icon->priv->status_icon), TRUE);
#endif
			/* don't keep trying to set the same icon */
		        g_free (icon->priv->stock_id);
			icon->priv->stock_id = g_strdup (stock_id);
		}
	} else {
		/* get rid of the icon */
#if USE_EGGTRAYICON
		gtk_image_clear (GTK_IMAGE (icon->priv->image));
		if (GTK_WIDGET_VISIBLE (icon->priv->image)) {
			gtk_widget_queue_resize (GTK_WIDGET (icon->priv->image));
		}
#else
		gtk_status_icon_set_visible (GTK_STATUS_ICON (icon->priv->status_icon), FALSE);
#endif
	}
}

/**
 * gpm_tray_icon_show_info_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_info_cb (GtkAction   *action,
			    GpmTrayIcon *icon)
{
	gpm_debug ("emitting show_info");
	g_signal_emit (icon, signals [SHOW_INFO], 0);
}

/**
 * gpm_tray_icon_hibernate_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_hibernate_cb (GtkAction   *action,
			    GpmTrayIcon *icon)
{
	gpm_debug ("emitting hibernate");
	g_signal_emit (icon, signals [HIBERNATE], 0);
}

/**
 * gpm_tray_icon_suspend_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_suspend_cb (GtkAction   *action,
			  GpmTrayIcon *icon)
{
	gpm_debug ("emitting suspend");
	g_signal_emit (icon, signals [SUSPEND], 0);
}

/**
 * gpm_tray_icon_show_preferences_cb:
 * @action: A valid GtkAction
 * @icon: This TrayIcon class instance
 **/
static void
gpm_tray_icon_show_preferences_cb (GtkAction   *action,
				   GpmTrayIcon *icon)
{
	const char *command = "gnome-power-preferences";

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
gpm_tray_icon_show_help_cb (GtkAction   *action,
			    GpmTrayIcon *icon)
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
gpm_tray_icon_show_about_cb (GtkAction   *action,
			     GpmTrayIcon *icon)
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
			       "copyright", "Copyright \xc2\xa9 2005-2006 Richard Hughes",
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

#if USE_EGGTRAYICON
/**
 * tray_popup_position_menu:
 *
 * Popup the drop-down menu at the base of the icon
 **/
static void
tray_popup_position_menu (GtkMenu  *menu,
			  int      *x,
			  int      *y,
			  gboolean *push_in,
			  gpointer  user_data)
{
	GtkWidget     *widget;
	GtkRequisition requisition;
	gint	       menu_xpos;
	gint	       menu_ypos;

	widget = GTK_WIDGET (user_data);

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

	menu_xpos += widget->allocation.x;
	menu_ypos += widget->allocation.y;

	if (menu_ypos > gdk_screen_get_height (gtk_widget_get_screen (widget)) / 2) {
		menu_ypos -= (requisition.height + 1);
	} else {
		menu_ypos += widget->allocation.height + 1;
	}

	*x = menu_xpos;
	*y = menu_ypos;
	*push_in = TRUE;
}
#endif

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

#if USE_EGGTRAYICON
	/* we enable the tooltip as the menu has gone */
	gtk_tooltips_enable (icon->priv->tooltips);
#endif
}

#if USE_EGGTRAYICON
/**
 * gpm_tray_icon_button_press_cb:
 * @widget: The tray icon widget
 * @event: Valid event
 * @icon: This TrayIcon class instance
 *
 * What do do when the button is left, right, middle clicked etc.
 **/
static gboolean
gpm_tray_icon_button_press_cb (GtkWidget      *widget,
			       GdkEventButton *event,
			       GpmTrayIcon    *icon)
{
	GtkWidget *popup;

	popup = gtk_ui_manager_get_widget (GTK_UI_MANAGER (icon->priv->ui_manager),
					   "/GpmTrayPopup");
	gtk_menu_set_screen (GTK_MENU (popup),
			     gtk_widget_get_screen (GTK_WIDGET (icon)));

	/* we disable the tooltip so it doesn't clash with the menu. See #331075 */
	gtk_tooltips_disable (icon->priv->tooltips);
	gtk_menu_popup (GTK_MENU (popup), NULL, NULL,
			tray_popup_position_menu, widget,
			2,
			gtk_get_current_event_time ());
	return TRUE;
}
#endif

/**
 * gpm_tray_icon_sync_actions:
 * @icon: This TrayIcon class instance
 *
 * Syncs the private icon->priv->can_* variables with the icon states
 **/
static void
gpm_tray_icon_sync_actions (GpmTrayIcon *icon)
{
	GtkAction *action;
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	if (icon->priv->actiongroup != NULL) {
		action = gtk_action_group_get_action (icon->priv->actiongroup,
						      "TraySuspend");
		gtk_action_set_visible (GTK_ACTION (action), icon->priv->can_suspend);

		action = gtk_action_group_get_action (icon->priv->actiongroup,
						      "TrayHibernate");
		gtk_action_set_visible (GTK_ACTION (action), icon->priv->can_hibernate);
	}
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
	GError		 *error = NULL;
	GtkWidget	 *widget;

	klass = GPM_TRAY_ICON_CLASS (g_type_class_peek (GPM_TYPE_TRAY_ICON));

	tray = GPM_TRAY_ICON (G_OBJECT_CLASS (gpm_tray_icon_parent_class)->constructor
			      (type, n_construct_properties,
			       construct_properties));

	tray->priv->actiongroup = gtk_action_group_new ("TrayActions");

	gtk_action_group_set_translation_domain (tray->priv->actiongroup,
						 GETTEXT_PACKAGE);

	gtk_action_group_add_actions (tray->priv->actiongroup,
				      gpm_tray_icon_action_entries,
				      gpm_tray_icon_n_action_entries,
				      tray);
	gpm_tray_icon_sync_actions (tray);

	gtk_ui_manager_insert_action_group (tray->priv->ui_manager, tray->priv->actiongroup, 0);
	gtk_ui_manager_add_ui_from_string (tray->priv->ui_manager,
					   "<ui>"
					   "  <popup name=\"GpmTrayPopup\">"
					   "    <menuitem action=\"TraySuspend\" />"
					   "    <menuitem action=\"TrayHibernate\" />"
					   "    <separator />"
					   "    <menuitem action=\"TrayPreferences\" />"
					   "    <menuitem action=\"TrayInfo\" />"
					   "    <separator />"
					   "    <menuitem action=\"TrayHelp\" />"
					   "    <menuitem action=\"TrayAbout\" />"
					   "  </popup>"
					   "</ui>",
					   -1, &error);
	if (error != NULL) {
		gpm_warning ("Couldn't merge user interface for popup: %s", error->message);
		g_clear_error (&error);
	}

	gtk_ui_manager_ensure_update (tray->priv->ui_manager);

	/* Get notified of when the menu goes, as we have to re-enable the tooltip */
	widget = gtk_ui_manager_get_widget (tray->priv->ui_manager, "/GpmTrayPopup");
	g_signal_connect (GTK_WIDGET (widget), "hide", G_CALLBACK (gpm_tray_icon_popup_cleared_cd), tray);

	g_object_unref (tray->priv->actiongroup);

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
	signals [SHOW_INFO] =
		g_signal_new ("show-info",
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
 * gpm_tray_icon_show:
 * @icon: This TrayIcon class instance
 * @enabled: If we should show the tray
 **/
void
gpm_tray_icon_show (GpmTrayIcon *icon,
		    gboolean     enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	if (enabled) {
#if USE_EGGTRAYICON
		gtk_widget_show_all (GTK_WIDGET (icon));
#else
		gtk_status_icon_set_visible (GTK_STATUS_ICON (icon->priv->status_icon), TRUE);
#endif
		icon->priv->is_visible = TRUE;
	} else {
#if USE_EGGTRAYICON
		gtk_widget_hide_all (GTK_WIDGET (icon));
#else
		gtk_status_icon_set_visible (GTK_STATUS_ICON (icon->priv->status_icon), FALSE);
#endif
		icon->priv->is_visible = FALSE;
	}
}

#if !USE_EGGTRAYICON
/**
 * gpm_tray_icon_popup_menu_cb:
 * @button: Which buttons are pressed
 * @icon: This TrayIcon class instance
 *
 * Display the popup menu.
 **/
static void
gpm_tray_icon_popup_menu_cb (GtkStatusIcon *status_icon,
			     guint          button,
			     guint32        activate_time,
			     GpmTrayIcon    *icon)
{
	GtkWidget *popup;
	popup = gtk_ui_manager_get_widget (GTK_UI_MANAGER (icon->priv->ui_manager),
					   "/GpmTrayPopup");
	gtk_menu_set_screen (GTK_MENU (popup),
			     gtk_widget_get_screen (GTK_WIDGET (icon)));

	gtk_menu_popup (GTK_MENU (popup), NULL, NULL,
			gtk_status_icon_position_menu, status_icon,
			button, activate_time);
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
	g_debug ("icon activated");
}
#endif

/**
 * gpm_tray_icon_init:
 * @icon: This TrayIcon class instance
 *
 * Initialise the tray object, and set up libnotify
 **/
static void
gpm_tray_icon_init (GpmTrayIcon *icon)
{
	gboolean ret = TRUE;

	icon->priv = GPM_TRAY_ICON_GET_PRIVATE (icon);

	/* FIXME: make this a property */
	icon->priv->show_notifications = TRUE;
	icon->priv->stock_id = g_strdup ("about-blank");

	icon->priv->ui_manager = gtk_ui_manager_new ();

#if USE_EGGTRAYICON
	icon->priv->tooltips = gtk_tooltips_new ();
	icon->priv->ebox = gtk_event_box_new ();
	g_signal_connect_object (G_OBJECT (icon->priv->ebox),
				 "button_press_event",
				 G_CALLBACK (gpm_tray_icon_button_press_cb),
				 icon, 0);

	icon->priv->image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (icon->priv->ebox), icon->priv->image);

	gtk_container_add (GTK_CONTAINER (icon), icon->priv->ebox);
#else
	icon->priv->status_icon = gtk_status_icon_new ();
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "popup_menu",
				 G_CALLBACK (gpm_tray_icon_popup_menu_cb),
				 icon, 0);
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "activate",
				 G_CALLBACK (gpm_tray_icon_activate_cb),
				 icon, 0);
#endif

	gpm_tray_icon_show (GPM_TRAY_ICON (icon), FALSE);

#ifdef HAVE_LIBNOTIFY
	ret = notify_init (GPM_NAME);
#endif
	if (!ret) {
		gpm_warning ("gpm_tray_icon_init failed");
	}
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

	g_return_if_fail (tray_icon->priv != NULL);

#if USE_EGGTRAYICON
	gtk_object_destroy (GTK_OBJECT (tray_icon->priv->tooltips));
#endif

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

#ifdef HAVE_LIBNOTIFY

/**
 * notification_closed_cb:
 * @notify: our libnotify instance
 * @icon: This TrayIcon class instance
 **/
static void
notification_closed_cb (NotifyNotification *notify,
			GpmTrayIcon	*icon)
{
	/* just invalidate the pointer */
	gpm_debug ("caught notification closed signal");
	icon->priv->notify = NULL;
}

/**
 * libnotify_event:
 * @icon: This icon class instance
 * @title: The title, e.g. "Battery Low"
 * @content: The contect, e.g. "17 minutes remaining"
 * @timeout: The time we should remain on screen in seconds
 * @msgicon: The icon to display, or NULL, e.g. GPM_STOCK_UPS_CHARGING_080
 * @urgency: The urgency type, e.g. GPM_NOTIFY_URGENCY_CRITICAL
 *
 * Does a libnotify messagebox dialogue.
 * Return value: success
 **/
static gboolean
libnotify_event (GpmTrayIcon    *icon,
		 const char	*title,
		 const char	*content,
		 guint		 timeout,
		 const char	*msgicon,
		 GpmNotifyLevel	 urgency)
{
	GtkWidget *point = NULL;

	if (icon->priv->notify != NULL) {
		notify_notification_close (icon->priv->notify, NULL);
	}

	/* Point to the center of the icon as per the GNOME HIG, #338638 */
#if USE_EGGTRAYICON
	if (icon->priv->is_visible) {
		point = icon->priv->image;
	}
#endif
	icon->priv->notify = notify_notification_new (title, content,
						      msgicon, point);

	notify_notification_set_timeout (icon->priv->notify, timeout * 1000);

	if (urgency == GPM_NOTIFY_URGENCY_CRITICAL) {
		gpm_warning ("libnotify: %s : %s", GPM_NAME, content);
	} else {
		gpm_debug ("libnotify: %s : %s", GPM_NAME, content);
	}

	g_signal_connect (icon->priv->notify, "closed", G_CALLBACK (notification_closed_cb), icon);

	if (! notify_notification_show (icon->priv->notify, NULL)) {
		gpm_warning ("failed to send notification (%s)", content);
		return FALSE;
	}

	return TRUE;
}

#else

/**
 * libnotify_event:
 * @icon: This icon class instance
 * @title: The title, e.g. "Battery Low"
 * @content: The contect, e.g. "17 minutes remaining"
 * @timeout: The time we should remain on screen in seconds
 * @msgicon: The icon to display, or NULL, e.g. GPM_STOCK_UPS_CHARGING_080
 * @urgency: The urgency type, e.g. GPM_NOTIFY_URGENCY_CRITICAL
 *
 * Does a gtk messagebox dialogue.
 * Return value: success
 **/
static gboolean
libnotify_event (GpmTrayIcon    *icon,
		 const char	*title,
		 const char	*content,
		 guint		 timeout,
		 const char	*msgicon,
		 GpmNotifyLevel	 urgency)
{
	GtkWidget     *dialog;
	GtkMessageType msg_type;

	if (urgency == GPM_NOTIFY_URGENCY_CRITICAL) {
		msg_type = GTK_MESSAGE_WARNING;
	} else {
		msg_type = GTK_MESSAGE_INFO;
	}

	dialog = gtk_message_dialog_new_with_markup (NULL,
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     msg_type,
						     GTK_BUTTONS_CLOSE,
						     "<span size='larger'><b>%s</b></span>",
						     GPM_NAME);

	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), content);

	g_signal_connect_swapped (dialog,
				  "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);

	gtk_window_present (GTK_WINDOW (dialog));

	return TRUE;
}
#endif

/**
 * gpm_tray_icon_notify:
 * @icon: This icon class instance
 * @title: The title, e.g. "Battery Low"
 * @content: The contect, e.g. "17 minutes remaining"
 * @timeout: The time we should remain on screen in seconds
 * @msgicon: The icon to display, or NULL, e.g. GPM_STOCK_UPS_CHARGING_080
 * @urgency: The urgency type, e.g. GPM_NOTIFY_URGENCY_CRITICAL
 *
 * Does a libnotify or gtk messagebox dialogue.
 **/
void
gpm_tray_icon_notify (GpmTrayIcon	*icon,
		      const char	*title,
		      const char	*content,
		      guint		 timeout,
		      const char	*msgicon,
		      GpmNotifyLevel	 urgency)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	if (! icon->priv->show_notifications) {
		gpm_debug ("ignoring notification: %s", title);
		return;
	}

	gpm_debug ("doing notify: %s", title);
	libnotify_event (icon, title, content, timeout, msgicon, urgency);
}

/**
 * gpm_tray_icon_cancel_notify:
 * @icon: This icon class instance
 *
 * Cancels the notification, i.e. removes it from the screen.
 **/
void
gpm_tray_icon_cancel_notify (GpmTrayIcon *icon)
{
	GError *error;
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	error = NULL;

#ifdef HAVE_LIBNOTIFY
	if (icon->priv->notify != NULL) {
		notify_notification_close (icon->priv->notify, &error);
	}
	if (error != NULL) {
		g_error_free (error);
	}
#endif
}
