/** @file	gpm-libnotify.c
 *  @brief	LibNotify shared code and fallback code
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *  @note	This code can still be used without libnotify compiled in,
 *    		as it fall backs to a standard modal messsagebox.
 *
 * This module allows a really easy way to provide libnotify boxes to
 * the user, and is used throughout g-p-m and g-p-p.
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
/**
 * @addtogroup	libnotify	libnotify notification system
 * @brief			Integrates with libnotity and
 *				notification-daemon.
 *
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gnome.h>
#include "gpm-common.h"
#include "gpm-libnotify.h"
#include "gpm-stock-icons.h"

#if defined(HAVE_LIBNOTIFY)
#include <libnotify/notify.h>
#endif

#if defined(HAVE_LIBNOTIFY)
  #define HAVE_OLD_LIBNOTIFY
#endif
/*
 * We can only enable HAVE_NEW_LIBNOTIFY when we use libnotify > 0.3.0
 * which depends on DBUS 0.60, and a whole lot of stuff
 * won't build with the new DBUS -- We better wait for the
 * distros to start carrying this before we dump this on the
 * users / ISV's
 */

#if defined(HAVE_OLD_LIBNOTIFY)
static NotifyHandle *globalnotify = NULL;
#elif defined(HAVE_NEW_LIBNOTIFY)
static NotifyNotification *globalnotify;
#endif

/** Gets the position to "point" to (i.e. center of the icon)
 *
 *  @param	widget		the GtkWidget
 *  @param	x		X co-ordinate return
 *  @param	y		Y co-ordinate return
 *  @return			Success, return FALSE when no icon present
 */
static gboolean
get_widget_position (GtkWidget *widget, gint *x, gint *y)
{
	GdkPixbuf* pixbuf = NULL;

	/* assertion checks */
	g_assert (widget);
	g_assert (x);
	g_assert (y);

	gdk_window_get_origin (GDK_WINDOW (widget->window), x, y);
	pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (widget));
	*x += (gdk_pixbuf_get_width (pixbuf) / 2);
	*y += gdk_pixbuf_get_height (pixbuf);
	g_debug ("widget position x=%i, y=%i", *x, *y);
	return TRUE;
}

/** Convenience function to call libnotify
 *
 *  @param	subject		The subject text, e.g. "Battery Low"
 *  @param	content		The content text, e.g. "The battery has 15 minutes remaining"
 *  @param	urgency		The urgency, e.g NOTIFY_URGENCY_CRITICAL
 *  @param	point		The GtkWidget to point to. NULL is valid.
 *  @return			Success, if a notification is displayed.
 *
 *  @note	The message is printed to console with either g_warning or
 *		g_debug functions depending on the urgency.
 */
gboolean
libnotify_event (const gchar *subject, const gchar *content, const LibNotifyEventType urgency, GtkWidget *point)
{
#if defined(HAVE_NEW_LIBNOTIFY)
	gint x, y;
	globalnotify = notify_notification_new (subject, content, GPM_STOCK_AC_8_OF_8, NULL);

        notify_notification_set_timeout (globalnotify, 3000);

	if (point) {
		get_widget_position (point, &x, &y);
		notify_notification_set_hint_int32 (globalnotify, "x", x+12);
		notify_notification_set_hint_int32 (globalnotify, "y", y+24);
	}

	if (urgency == LIBNOTIFY_URGENCY_CRITICAL)
		g_warning ("libnotify: %s : %s", NICENAME, content);
	else
		g_debug ("libnotify: %s : %s", NICENAME, content);

	if (!notify_notification_show_and_forget (globalnotify, NULL)) {
		g_warning ("failed to send notification (%s)", content);
		return FALSE;
	}
	return TRUE;
#elif defined(HAVE_OLD_LIBNOTIFY)
	NotifyIcon *icon = NULL;
	NotifyHints *hints = NULL;
	gint x, y;

	/* assertion checks */
	g_assert (content);

	if (point) {
		get_widget_position (point, &x, &y);
		hints = notify_hints_new();
		notify_hints_set_int (hints, "x", x+12);
		notify_hints_set_int (hints, "y", y+24);
	}

	/* echo to terminal too */
	if (urgency == LIBNOTIFY_URGENCY_CRITICAL)
		g_warning ("libnotify: %s : %s", NICENAME, content);
	else
		g_debug ("libnotify: %s : %s", NICENAME, content);

	/* use default g-p-m icon for now */
	icon = notify_icon_new_from_uri (GPM_DATA "gnome-power.png");
	globalnotify = notify_send_notification (globalnotify, /* replaces all */
			   NULL,
			   urgency,
			   subject, content,
			   icon, /* icon */
			   TRUE, NOTIFY_TIMEOUT,
			   hints, /* hints */
			   NULL, /* no user data */
			   0);   /* no actions */
	notify_icon_destroy(icon);
	if (!globalnotify) {
		g_warning ("failed to send notification (%s)", content);
		return FALSE;
	}
	return TRUE;
#else
	GtkWidget *dialog;
	GtkMessageType msg_type;

	/* assertion checks */
	g_assert (content);

	if (urgency == LIBNOTIFY_URGENCY_CRITICAL)
		msg_type = GTK_MESSAGE_WARNING;
	else
		msg_type = GTK_MESSAGE_INFO;

	dialog = gtk_message_dialog_new_with_markup (NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		msg_type,
		GTK_BUTTONS_CLOSE,
		"<span size='larger'><b>%s</b></span>",
		NICENAME);

	gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), content);

	g_signal_connect_swapped (dialog,
		"response",
		G_CALLBACK (gtk_widget_destroy),
		dialog);

	gtk_window_present (GTK_WINDOW (dialog));

	return TRUE;
#endif
}

/** Clear the libnotify message, for where we add back the ac_adapter before
 *  the message times out.
 *
 *  @return			If we removed the message.
 */
gboolean
libnotify_clear (void)
{
#if defined(HAVE_OLD_LIBNOTIFY)
	if (globalnotify)
		notify_close (globalnotify);
#elif defined(HAVE_NEW_LIBNOTIFY)
	GError *error;
	if (globalnotify)
		notify_notification_close (globalnotify, &error);
#endif
	return TRUE;
}

/** Initialiser for libnotify
 *
 *  @param	nicename	The nicename, e.g. "GNOME Power Manager"
 *  @return			If we initialised correctly.
 *
 *  @note	This function must be called before any calls to
 *		libnotify_event are made.
 */
gboolean
libnotify_init (const gchar *nicename)
{
	gboolean ret = TRUE;
	g_assert (nicename);
#if defined(HAVE_OLD_LIBNOTIFY)
	globalnotify = NULL;
	ret = notify_glib_init (nicename, NULL);
#elif defined(HAVE_NEW_LIBNOTIFY)
	globalnotify = NULL;
	ret = notify_init (nicename);
#endif
	return ret;
}
/** @} */
