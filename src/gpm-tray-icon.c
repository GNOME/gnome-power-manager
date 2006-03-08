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

#include <libgnomeui/gnome-help.h> /* for gnome_help_display */

#if LIBNOTIFY_VERSION_MINOR >= 2
#include <libnotify/notify.h>
#endif

#include "gpm-common.h" /* For GPM_HOMEPAGE_URL, etc */
#include "gpm-stock-icons.h"
#include "gpm-tray-icon.h"
#include "gpm-debug.h"

static void     gpm_tray_icon_class_init (GpmTrayIconClass *klass);
static void     gpm_tray_icon_init       (GpmTrayIcon      *tray_icon);
static void     gpm_tray_icon_finalize   (GObject          *object);

#define GPM_TRAY_ICON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_TRAY_ICON, GpmTrayIconPrivate))

struct GpmTrayIconPrivate
{
	GtkUIManager   *ui_manager;
	GtkActionGroup *actiongroup;
	GtkTooltips    *tooltips;
	GtkWidget      *popup_menu;
	GtkWidget      *image;
	GtkWidget      *ebox;

	gboolean        show_notifications;
	gboolean        is_visible;

	gboolean        can_suspend;
	gboolean        can_hibernate;

#if (LIBNOTIFY_VERSION_MINOR == 2)
	NotifyHandle   *notify;
#elif (LIBNOTIFY_VERSION_MINOR >= 3)
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

typedef enum {
	LIBNOTIFY_URGENCY_CRITICAL = 1,
	LIBNOTIFY_URGENCY_NORMAL   = 2,
	LIBNOTIFY_URGENCY_LOW      = 3
} LibNotifyEventType;

static void gpm_tray_icon_suspend_cb		(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_hibernate_cb		(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_info_cb	 	(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_preferences_cb	(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_help_cb		(GtkAction *action, GpmTrayIcon *icon);
static void gpm_tray_icon_show_about_cb		(GtkAction *action, GpmTrayIcon *icon);

static GtkActionEntry gpm_tray_icon_action_entries [] =
{
	{ "TraySuspend", GPM_STOCK_SUSPEND_TO_RAM, N_("_Suspend"),
	  NULL, N_("Suspend the computer"), G_CALLBACK (gpm_tray_icon_suspend_cb) },
	{ "TrayHibernate", GPM_STOCK_SUSPEND_TO_DISK, N_("Hi_bernate"),
	  NULL, N_("Make the computer go to sleep"), G_CALLBACK (gpm_tray_icon_hibernate_cb) },
	{ "TrayPreferences", GTK_STOCK_PREFERENCES, N_("_Preferences"),
	  NULL, NULL, G_CALLBACK (gpm_tray_icon_show_preferences_cb) },
	{ "TrayInfo", GPM_STOCK_POWER_INFORMATION, N_("_Information"),
	  NULL, NULL, G_CALLBACK (gpm_tray_icon_show_info_cb) },
	{ "TrayHelp", GTK_STOCK_HELP, N_("_Help"), NULL,
	  NULL, G_CALLBACK (gpm_tray_icon_show_help_cb) },
	{ "TrayAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
	  NULL, G_CALLBACK (gpm_tray_icon_show_about_cb) }
};
static guint gpm_tray_icon_n_action_entries = G_N_ELEMENTS (gpm_tray_icon_action_entries);

static GObjectClass *parent_class = NULL;
static guint         signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmTrayIcon, gpm_tray_icon, EGG_TYPE_TRAY_ICON)

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

void
gpm_tray_icon_set_tooltip (GpmTrayIcon *icon,
			   const char  *tooltip)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	g_return_if_fail (tooltip != NULL);

	gtk_tooltips_set_tip (icon->priv->tooltips,
			      GTK_WIDGET (icon),
			      tooltip, NULL);
}

void
gpm_tray_icon_set_image_from_stock (GpmTrayIcon *icon,
				    const char  *stock_id)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	gpm_debug ("Setting icon to %s", stock_id);

	if (stock_id) {
		gtk_image_set_from_stock (GTK_IMAGE (icon->priv->image),
					  stock_id,
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
	} else {
		/* FIXME: gtk_image_clear requires gtk 2.8, so until we
		 * depend on more then 2.6 (required for FC4) we have to
		 * comment it out
		gtk_image_clear (GTK_IMAGE (icon->priv->image));*/
		if (GTK_WIDGET_VISIBLE (icon->priv->image)) {
			gtk_widget_queue_resize (GTK_WIDGET (icon->priv->image));
		}
	}
}

static void
gpm_tray_icon_show_info_cb (GtkAction   *action,
			    GpmTrayIcon *icon)
{
	gpm_debug ("emitting show_info");
	g_signal_emit (icon, signals [SHOW_INFO], 0);
}

static void
gpm_tray_icon_hibernate_cb (GtkAction   *action,
			    GpmTrayIcon *icon)
{
	gpm_debug ("emitting hibernate");
	g_signal_emit (icon, signals [HIBERNATE], 0);
}

static void
gpm_tray_icon_suspend_cb (GtkAction   *action,
			  GpmTrayIcon *icon)
{
	gpm_debug ("emitting suspend");
	g_signal_emit (icon, signals [SUSPEND], 0);
}

static void
gpm_tray_icon_show_preferences_cb (GtkAction   *action,
				   GpmTrayIcon *icon)
{
	const char *command = "gnome-power-preferences";

	if (! g_spawn_command_line_async (command, NULL)) {
		gpm_warning ("Couldn't execute command: %s", command);
	}
}

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

static void
gpm_tray_icon_show_about_cb (GtkAction  *action,
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
	char        *license_trans;
	GdkPixbuf   *pixbuf;

	/*
	 * Translators comment: put your own name here to appear in the about dialog.
	 */
  	if (!strcmp (translators, "translator-credits")) {
		translators = NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (GPM_DATA "gnome-power.png", NULL);

	license_trans = g_strconcat (_(license[0]), "\n\n", _(license[1]), "\n\n",
				     _(license[2]), "\n\n", _(license[3]), "\n",  NULL);

	gtk_window_set_default_icon_from_file (GPM_DATA "gnome-power-manager.png", NULL);

	gtk_show_about_dialog (NULL,
		               "name", GPM_NAME,
		               "version", VERSION,
		               "copyright", "Copyright \xc2\xa9 2005 Richard Hughes",
		               "license", license_trans,
		               "website", GPM_HOMEPAGE_URL,
		               "comments", GPM_DESCRIPTION,
		               "authors", authors,
		               "documenters", documenters,
		               "artists", artists,
		               "translator-credits", translators,
		               "logo", pixbuf,
		               NULL);

	g_object_unref (pixbuf);
}

static void
tray_popup_position_menu (GtkMenu  *menu,
			  int      *x,
			  int      *y,
			  gboolean *push_in,
			  gpointer  user_data)
{
	GtkWidget     *widget;
	GtkRequisition requisition;
	gint           menu_xpos;
	gint           menu_ypos;

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

static void
gpm_tray_icon_popup_cleared_cd (GtkWidget   *mo,
				GpmTrayIcon *icon)
{
	/* we enable the tooltip as the menu has gone */
	gtk_tooltips_enable (icon->priv->tooltips);
}

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

static void
gpm_tray_icon_set_property (GObject		  *object,
			       guint		   prop_id,
			       const GValue	  *value,
			       GParamSpec	  *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_tray_icon_get_property (GObject		  *object,
			       guint		   prop_id,
			       GValue		  *value,
			       GParamSpec	  *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gpm_tray_icon_sync_actions (GpmTrayIcon *tray)
{
	GtkAction *action;

	if (tray->priv->actiongroup != NULL) {
		action = gtk_action_group_get_action (tray->priv->actiongroup,
						      "TraySuspend");

		gtk_action_set_visible (GTK_ACTION (action), tray->priv->can_suspend);

		action = gtk_action_group_get_action (tray->priv->actiongroup,
						      "TrayHibernate");

		gtk_action_set_visible (GTK_ACTION (action), tray->priv->can_hibernate);
	}
}

static GObject *
gpm_tray_icon_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_properties)
{
	GpmTrayIcon      *tray;
	GpmTrayIconClass *klass;
	GError           *error = NULL;
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
#if EXPERIMENTAL_FEATURES_ENABLED
					   "    <menuitem action=\"TrayInfo\" />"
#endif
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

static void
gpm_tray_icon_class_init (GpmTrayIconClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize	   = gpm_tray_icon_finalize;
	object_class->get_property = gpm_tray_icon_get_property;
	object_class->set_property = gpm_tray_icon_set_property;
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

void
gpm_tray_icon_show (GpmTrayIcon *icon,
		    gboolean     enabled)
{
	if (enabled) {
		gtk_widget_show_all (GTK_WIDGET (icon));
		icon->priv->is_visible = TRUE;
	} else {
		gtk_widget_hide_all (GTK_WIDGET (icon));
		gtk_widget_unrealize (GTK_WIDGET (icon));
		icon->priv->is_visible = FALSE;
	}
}

static void
gpm_tray_icon_init (GpmTrayIcon *icon)
{
	gboolean ret = TRUE;

	icon->priv = GPM_TRAY_ICON_GET_PRIVATE (icon);

	/* FIXME: make this a property */
	icon->priv->show_notifications = TRUE;

	icon->priv->tooltips = gtk_tooltips_new ();
	icon->priv->ui_manager = gtk_ui_manager_new ();

	icon->priv->ebox = gtk_event_box_new ();
	g_signal_connect_object (G_OBJECT (icon->priv->ebox),
		                 "button_press_event",
		                 G_CALLBACK (gpm_tray_icon_button_press_cb),
		                 icon, 0);

	icon->priv->image = gtk_image_new ();

	gtk_container_add (GTK_CONTAINER (icon->priv->ebox), icon->priv->image);

	gtk_container_add (GTK_CONTAINER (icon), icon->priv->ebox);
	gpm_tray_icon_show (GPM_TRAY_ICON (icon), FALSE);

#if (LIBNOTIFY_VERSION_MINOR >= 3)
	ret = notify_init (GPM_NAME);
#elif (LIBNOTIFY_VERSION_MINOR == 2)
	ret = notify_glib_init (GPM_NAME, NULL);
#endif
	if (!ret) {
		gpm_warning ("gpm_tray_icon_init failed");
	}
}

static void
gpm_tray_icon_finalize (GObject *object)
{
	GpmTrayIcon *tray_icon;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_TRAY_ICON (object));

	tray_icon = GPM_TRAY_ICON (object);

	g_return_if_fail (tray_icon->priv != NULL);

	gtk_object_destroy (GTK_OBJECT (tray_icon->priv->tooltips));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GpmTrayIcon *
gpm_tray_icon_new (void)
{
	GpmTrayIcon *tray_icon;

	tray_icon = g_object_new (GPM_TYPE_TRAY_ICON, NULL);

	return GPM_TRAY_ICON (tray_icon);
}

#if LIBNOTIFY_VERSION_MINOR >= 2
static gboolean
get_widget_position (GtkWidget *widget,
		     int       *x,
		     int       *y)
{
	g_assert (widget);

	gdk_window_get_origin (GDK_WINDOW (widget->window), x, y);

	*x += widget->allocation.x;
	*y += widget->allocation.y;
	*x += widget->allocation.width / 2;
	*y += widget->allocation.height;

	gpm_debug ("widget position x=%i, y=%i", *x, *y);

	return TRUE;
}
#endif

#if (LIBNOTIFY_VERSION_MINOR >= 3)

static void
notification_closed_cb (NotifyNotification *notify,
			GpmTrayIcon        *tray)
{
	/* just invalidate the pointer */
	gpm_debug ("caught notification closed signal");
	tray->priv->notify = NULL;
}

static gboolean
libnotify_event (GpmTrayIcon             *tray,
		 guint                    timeout,	/* in seconds */
		 const char              *subject,
		 const char              *content,
		 const LibNotifyEventType urgency)
{
	int x;
	int y;

	if (tray->priv->notify != NULL) {
		notify_notification_close (tray->priv->notify, NULL);
	}

	tray->priv->notify = notify_notification_new (subject, content,
						      GPM_STOCK_BATTERY_DISCHARGING_100,
						      NULL);

	notify_notification_set_timeout (tray->priv->notify, timeout * 1000);

	if (tray->priv->is_visible) {
		get_widget_position (GTK_WIDGET (tray), &x, &y);
		notify_notification_set_hint_int32 (tray->priv->notify, "x", x);
		notify_notification_set_hint_int32 (tray->priv->notify, "y", y);
	}

	if (urgency == LIBNOTIFY_URGENCY_CRITICAL) {
		gpm_warning ("libnotify: %s : %s", GPM_NAME, content);
	} else {
		gpm_debug ("libnotify: %s : %s", GPM_NAME, content);
	}

	g_signal_connect (tray->priv->notify, "closed", G_CALLBACK (notification_closed_cb), tray);

	if (! notify_notification_show (tray->priv->notify, NULL)) {
		gpm_warning ("failed to send notification (%s)", content);
		return FALSE;
	}

	return TRUE;
}

#elif (LIBNOTIFY_VERSION_MINOR == 2)

static gboolean
libnotify_event (GpmTrayIcon             *tray,
		 guint                    timeout,	/* in seconds */
		 const char              *subject,
		 const char              *content,
		 const LibNotifyEventType urgency)
{
	NotifyIcon  *icon = NULL;
	NotifyHints *hints = NULL;
	int          x;
	int          y;

	/* assertion checks */
	g_assert (content);

	hints = notify_hints_new ();
	if (tray->priv->is_visible) {
		get_widget_position (GTK_WIDGET (tray), &x, &y);
		notify_hints_set_int (hints, "x", x);
		notify_hints_set_int (hints, "y", y);
	}
	/* echo to terminal too */
	if (urgency == LIBNOTIFY_URGENCY_CRITICAL) {
		gpm_warning ("libnotify: %s : %s", GPM_NAME, content);
	} else {
		gpm_debug ("libnotify: %s : %s", GPM_NAME, content);
	}

	/* use default g-p-m icon for now */
	icon = notify_icon_new_from_uri (GPM_DATA "gnome-power.png");

	if (tray->priv->notify != NULL) {
		notify_close (tray->priv->notify);
	}

	tray->priv->notify = notify_send_notification (tray->priv->notify,
						       NULL,
						       urgency,
						       subject,
						       content,
						       icon, /* icon */
						       TRUE,
						       timeout,
						       hints, /* hints */
						       NULL, /* no user data */
						       0);   /* no actions */
	notify_icon_destroy (icon);

	if (! tray->priv->notify) {
		gpm_warning ("failed to send notification (%s)", content);
		return FALSE;
	}

	return TRUE;
}

#else

static gboolean
libnotify_event (GpmTrayIcon             *tray,
		 guint                    timeout,	/* in seconds */
		 const char              *subject,
		 const char              *content,
		 const LibNotifyEventType urgency)

{
	GtkWidget     *dialog;
	GtkMessageType msg_type;

	/* assertion checks */
	g_assert (content);

	if (urgency == LIBNOTIFY_URGENCY_CRITICAL) {
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

void
gpm_tray_icon_notify (GpmTrayIcon *icon,
		      guint        timeout,	/* in seconds */
		      const char  *primary,
		      GtkWidget   *msgicon,
		      const char  *secondary)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	if (! icon->priv->show_notifications) {
		gpm_debug ("ignoring notification: %s", primary);
		return;
	}

	gpm_debug ("doing notify: %s", primary);

	libnotify_event (icon,
			 timeout,
			 primary,
			 secondary,
			 LIBNOTIFY_URGENCY_CRITICAL);
}

void
gpm_tray_icon_cancel_notify (GpmTrayIcon *icon)
{
	GError *error;

	g_return_if_fail (GPM_IS_TRAY_ICON (icon));

	error = NULL;

#if LIBNOTIFY_VERSION_MINOR >= 2
	if (icon->priv->notify != NULL) {
#if (LIBNOTIFY_VERSION_MINOR >= 3)
		notify_notification_close (icon->priv->notify, &error);
#elif (LIBNOTIFY_VERSION_MINOR == 2)
		notify_close (icon->priv->notify);
		icon->priv->notify = NULL;
#endif
	}
	if (error != NULL) {
		g_error_free (error);
	}
#endif
}
