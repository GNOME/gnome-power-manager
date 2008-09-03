/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#include "gpm-ac-adapter.h"
#include "gpm-common.h"
#include "gpm-conf.h"
#include "egg-debug.h"
#include "gpm-notify.h"
#include "gpm-stock-icons.h"

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#define GPM_NOTIFY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_NOTIFY, GpmNotifyPrivate))
#define QUIRK_WEBSITE	"http://people.freedesktop.org/~hughsient/quirk/"

struct GpmNotifyPrivate
{
	GpmAcAdapter		*ac_adapter;
	GpmConf			*conf;
	GtkStatusIcon		*status_icon;
	gchar			*internet_url;
	const gchar		*do_not_show_gconf;
#ifdef HAVE_LIBNOTIFY
	NotifyNotification	*libnotify;
#endif
};

enum {
	NOTIFY_CHANGED,
	LAST_SIGNAL
};

//static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_notify_object = NULL;

G_DEFINE_TYPE (GpmNotify, gpm_notify, G_TYPE_OBJECT)

#ifdef HAVE_LIBNOTIFY
/**
 * notify_closed_cb:
 * @notify: our libnotify instance
 * @notify: This TrayIcon class instance
 **/
static void
notify_closed_cb (NotifyNotification *libnotify,
		  GpmNotify	     *notify)
{
	/* just invalidate the pointer */
	egg_debug ("caught notification closed signal");
//	notify->priv->libnotify = NULL;
}

static gboolean
gpm_notify_create (GpmNotify 	 *notify,
	 	   const gchar	 *title,
		   const gchar	 *content,
		   GpmNotifyTimeout timeout,
		   const gchar	 *msgicon,
		   GpmNotifyUrgency urgency)
{
	if (notify->priv->libnotify != NULL) {
//		notify_notification_close (notify->priv->libnotify, NULL);
//		notify->priv->libnotify = NULL;
	}

	if (notify->priv->status_icon != NULL &&
	    gtk_status_icon_get_visible (notify->priv->status_icon)) {
		notify->priv->libnotify = notify_notification_new_with_status_icon (title, content,
										    msgicon,
										    notify->priv->status_icon);
	} else {
		notify->priv->libnotify = notify_notification_new (title, content, msgicon, NULL);
	}

	if (timeout == GPM_NOTIFY_TIMEOUT_NEVER) {
		notify_notification_set_timeout (notify->priv->libnotify, 0);
	} else if (timeout == GPM_NOTIFY_TIMEOUT_LONG) {
		notify_notification_set_timeout (notify->priv->libnotify, 30 * 1000);
	} else if (timeout == GPM_NOTIFY_TIMEOUT_SHORT) {
		notify_notification_set_timeout (notify->priv->libnotify, 10 * 1000);
	}

	if (urgency == GPM_NOTIFY_URGENCY_CRITICAL) {
		egg_warning ("libnotify: %s : %s", GPM_NAME, content);
	} else {
		egg_debug ("libnotify: %s : %s", GPM_NAME, content);
	}

	g_signal_connect (notify->priv->libnotify, "closed", G_CALLBACK (notify_closed_cb), notify);
	return TRUE;
}

static gboolean
gpm_notify_show (GpmNotify *notify)
{
	gboolean ret;
	ret = notify_notification_show (notify->priv->libnotify, NULL);
	if (!ret) {
		egg_warning ("failed to send notification");
	}
	return ret;
}

/**
 * gpm_notify_display:
 * @notify: This class instance
 * @title: The title, e.g. "Battery Low"
 * @content: The contect, e.g. "17 minutes remaining"
 * @timeout: The time we should remain, e.g. GPM_NOTIFY_TIMEOUT_SHORT
 * @msgicon: The icon to display, or NULL, e.g. GPM_STOCK_UPS_CHARGING_080
 * @urgency: The urgency type, e.g. GPM_NOTIFY_URGENCY_CRITICAL
 *
 * Does a simple libnotify messagebox dialogue.
 * Return value: success
 **/
gboolean
gpm_notify_display (GpmNotify 	 *notify,
	 	    const gchar	 *title,
		    const gchar	 *content,
		    GpmNotifyTimeout timeout,
		    const gchar	 *msgicon,
		    GpmNotifyUrgency urgency)
{
	gpm_notify_create (notify, title, content, timeout, msgicon, urgency);
	gpm_notify_show (notify);
	return TRUE;
}

#else

/**
 * gpm_notify_show:
 *
 * Stub.
 **/
static gboolean
gpm_notify_show (GpmNotify *notify)
{
	return TRUE;
}

/**
 * gpm_notify_create:
 *
 * Stub.
 **/
gboolean
gpm_notify_create (GpmNotify 	 *notify,
	 	   const gchar	 *title,
		   const gchar	 *content,
		   GpmNotifyTimeout timeout,
		   const gchar	 *msgicon,
		   GpmNotifyUrgency urgency)
{
	gpm_notify_display (notify, title, content, timeout, msgicon, urgency);
	return TRUE;
}

/**
 * gpm_notify_display:
 * @notify: This class instance
 * @title: The title, e.g. "Battery Low"
 * @content: The contect, e.g. "17 minutes remaining"
 * @timeout: The time we should remain on screen in seconds
 * @msgicon: The icon to display, or NULL, e.g. GPM_STOCK_UPS_CHARGING_080
 * @urgency: The urgency type, e.g. GPM_NOTIFY_URGENCY_CRITICAL
 *
 * Does a gtk messagebox dialogue.
 * Return value: success
 **/
gboolean
gpm_notify_display (GpmNotify 	 *notify,
	 	    const gchar	 *title,
		    const gchar	 *content,
		    GpmNotifyTimeout timeout,
		    const gchar	 *msgicon,
		    GpmNotifyUrgency urgency)
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

	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), "%s", content);

	g_signal_connect_swapped (dialog,
				  "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);

	gtk_window_present (GTK_WINDOW (dialog));

	return TRUE;
}
#endif

/**
 * gpm_notify_cancel:
 * @notify: This class instance
 *
 * Cancels the notification, i.e. removes it from the screen.
 **/
void
gpm_notify_cancel (GpmNotify *notify)
{
	g_return_if_fail (GPM_IS_NOTIFY (notify));

#ifdef HAVE_LIBNOTIFY
	if (notify->priv->libnotify != NULL) {
		notify_notification_close (notify->priv->libnotify, NULL);
		g_object_unref (notify->priv->libnotify);
		notify->priv->libnotify = NULL;
	}
#endif
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
ac_adapter_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean      on_ac,
		       GpmNotify    *notify)
{
	/* for where we add back the ac_adapter before the "AC Power unplugged"
	 * message times out. */
	if (on_ac) {
		egg_debug ("clearing notify due ac being present");
		gpm_notify_cancel (notify);
	}
}

/**
 * gpm_notify_cancel:
 *
 * Allows the libnotify arrow to point at the icon
 **/
void
gpm_notify_use_status_icon (GpmNotify *notify, GtkStatusIcon *status_icon)
{
	notify->priv->status_icon = status_icon;
}

#ifdef HAVE_LIBNOTIFY
static void
notify_general_clicked_cb (NotifyNotification *libnotify,
                           gchar *action, GpmNotify *notify)
{
	gboolean ret;
	GError *error;
 	char *cmdline;
	GdkScreen *gscreen;
	GtkWidget *error_dialog;

	gscreen = gdk_screen_get_default();

	if (strcmp (action, "dont-show-again") == 0) {
		egg_debug ("not showing warning anymore for %s!", notify->priv->do_not_show_gconf);
		gpm_conf_set_bool (notify->priv->conf, notify->priv->do_not_show_gconf, FALSE);
		notify->priv->do_not_show_gconf = NULL;
		return;
	}
	if (strcmp (action, "visit-website") == 0) {
		egg_debug ("autovisit website %s", notify->priv->internet_url);
		error = NULL;

 		cmdline = g_strconcat ("gnome-open ", notify->priv->internet_url, NULL);
		ret = gdk_spawn_command_line_on_screen (gscreen, cmdline, &error);
		g_free (cmdline);

		if (ret)
			return;

		g_error_free (error);
		error = NULL;

 		cmdline = g_strconcat ("xdg-open ", notify->priv->internet_url, NULL);
		ret = gdk_spawn_command_line_on_screen (gscreen, cmdline, &error);
		g_free (cmdline);

		if (!ret) {
			error_dialog = gtk_message_dialog_new ( NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Failed to show url %s", error->message); 
			gtk_dialog_run (GTK_DIALOG (error_dialog));
			g_error_free (error);
		}

		/* free the stored string */
		g_free (notify->priv->internet_url);
		notify->priv->internet_url = NULL;
		return;
	}
	egg_debug ("action %s unknown", action);
}
#endif

/**
 * gpm_notify_perhaps_recall:
 **/
gboolean
gpm_notify_perhaps_recall (GpmNotify   *notify,
			   const gchar *oem_vendor,
			   const gchar *website)
{
	gchar *msg;
	const gchar *title;

	/* don't show when running under GDM */
	if (g_getenv ("RUNNING_UNDER_GDM") != NULL) {
		egg_debug ("running under gdm, so no notification");
		return FALSE;
	}

	/* save in state */
	title = _("Battery may be recalled");
	msg = g_strdup_printf (_("The battery in your computer may have been "
			       "recalled by %s and you may be "
			       "at risk.\n\n"
			       "For more information visit the %s battery recall website."),
			       oem_vendor, oem_vendor);

	gpm_notify_create (notify, title, msg, 0,
			   GTK_STOCK_DIALOG_WARNING,
			   GPM_NOTIFY_URGENCY_CRITICAL);

	/* add extra stuff */
#ifdef HAVE_LIBNOTIFY
	notify->priv->internet_url = g_strdup (website);
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "visit-website",
	                                 _("Visit recall website"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
	notify->priv->do_not_show_gconf = GPM_CONF_NOTIFY_PERHAPS_RECALL;
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "dont-show-again",
	                                 _("Do not show me this again"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
#endif

	gpm_notify_show (notify);
	g_free (msg);
	return TRUE;
}

/**
 * gpm_notify_low_capacity:
 **/
gboolean
gpm_notify_low_capacity (GpmNotify *notify,
			 guint      capacity)
{
	gchar *msg;
	const gchar *title;

	/* don't show when running under GDM */
	if (g_getenv ("RUNNING_UNDER_GDM") != NULL) {
		egg_debug ("running under gdm, so no notification");
		return FALSE;
	}

	title = _("Battery may be broken");
	msg = g_strdup_printf (_("Your battery has a very low capacity (%i%%), "
				 "which means that it may be old or broken."),
			       capacity);

	gpm_notify_create (notify, title, msg, GPM_NOTIFY_TIMEOUT_LONG,
			   GTK_STOCK_DIALOG_WARNING,
			   GPM_NOTIFY_URGENCY_CRITICAL);

	/* add extra stuff */
#ifdef HAVE_LIBNOTIFY
	notify->priv->do_not_show_gconf = GPM_CONF_NOTIFY_LOW_CAPACITY;
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "dont-show-again",
	                                 _("Do not show me this again"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
#endif

	gpm_notify_show (notify);
	g_free (msg);
	return TRUE;
}

/**
 * gpm_notify_inhibit_lid:
 **/
gboolean
gpm_notify_inhibit_lid (GpmNotify *notify)
{
	gchar *msg;
	const gchar *title;

	/* don't show when running under GDM */
	if (g_getenv ("RUNNING_UNDER_GDM") != NULL) {
		egg_debug ("running under gdm, so no notification");
		return FALSE;
	}

	title = _("Sleep warning");
	msg = g_strdup (_("Your laptop will not sleep if you shut the "
			  "lid as a running program has prevented this.\n"
			  "Some laptops can overheat if they do not sleep "
			  "when the lid is closed."));

	gpm_notify_create (notify, title, msg, GPM_NOTIFY_TIMEOUT_LONG,
			   GPM_STOCK_INHIBIT,
			   GPM_NOTIFY_URGENCY_CRITICAL);

	/* add extra stuff */
#ifdef HAVE_LIBNOTIFY
	notify->priv->do_not_show_gconf = GPM_CONF_NOTIFY_INHIBIT_LID;
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "dont-show-again",
	                                 _("Do not show me this again"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
#endif

	gpm_notify_show (notify);
	g_free (msg);
	return TRUE;
}

/**
 * gpm_notify_fully_charged_primary:
 **/
gboolean
gpm_notify_fully_charged_primary (GpmNotify *notify)
{
	const gchar *msg;
	const gchar *title;

	title = _("Battery Charged");
	msg = _("Your laptop battery is now fully charged");

	gpm_notify_create (notify, title, msg, GPM_NOTIFY_TIMEOUT_SHORT,
			   GTK_STOCK_DIALOG_WARNING,
			   GPM_NOTIFY_URGENCY_CRITICAL);

	/* add extra stuff */
#ifdef HAVE_LIBNOTIFY
	notify->priv->do_not_show_gconf = GPM_CONF_NOTIFY_FULLY_CHARGED;
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "dont-show-again",
	                                 _("Do not show me this again"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
#endif

	gpm_notify_show (notify);
	return TRUE;
}

/**
 * gpm_notify_discharging_primary:
 **/
gboolean
gpm_notify_discharging_primary (GpmNotify *notify)
{
	const gchar *msg;
	const gchar *title;

	title = _("Battery Discharging");
	msg = _("The AC power has been unplugged. The system is now using battery power.");

	gpm_notify_create (notify, title, msg, GPM_NOTIFY_TIMEOUT_SHORT,
			   GTK_STOCK_DIALOG_WARNING,
			   GPM_NOTIFY_URGENCY_CRITICAL);

	/* add extra stuff */
#ifdef HAVE_LIBNOTIFY
	notify->priv->do_not_show_gconf = GPM_CONF_NOTIFY_DISCHARGING;
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "dont-show-again",
	                                 _("Do not show me this again"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
#endif

	gpm_notify_show (notify);
	return TRUE;
}

/**
 * gpm_notify_discharging_ups:
 **/
gboolean
gpm_notify_discharging_ups (GpmNotify *notify)
{
	const gchar *msg;
	const gchar *title;

	title = _("UPS Discharging");
	msg = _("The AC power has been unplugged. The system is now using backup power.");

	gpm_notify_create (notify, title, msg, GPM_NOTIFY_TIMEOUT_SHORT,
			   GTK_STOCK_DIALOG_WARNING,
			   GPM_NOTIFY_URGENCY_CRITICAL);

	/* add extra stuff */
#ifdef HAVE_LIBNOTIFY
	notify->priv->do_not_show_gconf = GPM_CONF_NOTIFY_DISCHARGING;
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "dont-show-again",
	                                 _("Do not show me this again"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
#endif

	gpm_notify_show (notify);
	return TRUE;
}

/**
 * gpm_notify_sleep_failed:
 **/
gboolean
gpm_notify_sleep_failed (GpmNotify *notify, gboolean hibernate)
{
	const gchar *msg;
	const gchar *title;
	const gchar *icon;

	title = _("Sleep Problem");
	if (hibernate) {
		msg = _("Your computer failed to hibernate.\nCheck the help file for common problems.");
		icon = GPM_STOCK_HIBERNATE;
	} else {
		msg = _("Your computer failed to suspend.\nCheck the help file for common problems.");
		icon = GPM_STOCK_SUSPEND;
	}

	gpm_notify_create (notify, title, msg, GPM_NOTIFY_TIMEOUT_NEVER, icon,
			   GPM_NOTIFY_URGENCY_CRITICAL);

	/* add extra stuff */
#ifdef HAVE_LIBNOTIFY
	notify->priv->do_not_show_gconf = GPM_CONF_NOTIFY_SLEEP_FAILED;
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "dont-show-again",
	                                 _("Do not show me this again"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);

	notify->priv->internet_url = g_strdup (QUIRK_WEBSITE);
	/* Translator: Quirks refer to strange/weird/undocumented behaviour of
	 * hardware.  Because the manufacturers do not provide the necessary
	 * information for non-FOSS software support, it is up to the users to
	 * supply the info by following the procedures outlined in the "Quirks
	 * website" at http://people.freedesktop.org/~hughsient/quirk/.
	 */
	notify_notification_add_action  (notify->priv->libnotify,
	                                 "visit-website",
	                                 _("Visit quirk website"),
	                                 (NotifyActionCallback) notify_general_clicked_cb,
	                                 notify, NULL);
#endif

	gpm_notify_show (notify);
	return TRUE;
}

/**
 * gpm_notify_finalize:
 **/
static void
gpm_notify_finalize (GObject *object)
{
	GpmNotify *notify;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_NOTIFY (object));
	notify = GPM_NOTIFY (object);
	g_return_if_fail (notify->priv != NULL);

#ifdef HAVE_LIBNOTIFY
	if (notify->priv->libnotify != NULL) {
		notify_notification_close (notify->priv->libnotify, NULL);
	}
#endif
	g_object_unref (notify->priv->conf);
	if (notify->priv->ac_adapter != NULL) {
		g_object_unref (notify->priv->ac_adapter);
	}

	G_OBJECT_CLASS (gpm_notify_parent_class)->finalize (object);
}

/**
 * gpm_notify_class_init:
 **/
static void
gpm_notify_class_init (GpmNotifyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_notify_finalize;

	g_type_class_add_private (klass, sizeof (GpmNotifyPrivate));
}

/**
 * gpm_notify_init:
 * @notify: This class instance
 *
 * initialises the notify class. NOTE: We expect notify objects
 * to *NOT* be removed or added during the session.
 * We only control the first notify object if there are more than one.
 **/
static void
gpm_notify_init (GpmNotify *notify)
{
	notify->priv = GPM_NOTIFY_GET_PRIVATE (notify);

	notify->priv->conf = gpm_conf_new ();
	notify->priv->do_not_show_gconf = NULL;

	/* we use ac_adapter so we can log the event */
	notify->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (notify->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), notify);

	notify->priv->status_icon = NULL;
#ifdef HAVE_LIBNOTIFY
	notify->priv->libnotify = NULL;
	notify_init (GPM_NAME);
#endif
}

/**
 * gpm_notify_new:
 * Return value: A new notify class instance.
 **/
GpmNotify *
gpm_notify_new (void)
{
	if (gpm_notify_object != NULL) {
		g_object_ref (gpm_notify_object);
	} else {
		gpm_notify_object = g_object_new (GPM_TYPE_NOTIFY, NULL);
		g_object_add_weak_pointer (gpm_notify_object, &gpm_notify_object);
	}
	return GPM_NOTIFY (gpm_notify_object);
}
