/*! @file	gpm-libnotify.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gnome.h>
#include "gpm-common.h"
#include "gpm-libnotify.h"
#if HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

/** Convenience function to call libnotify
 *
 *  @param	content		The content text, e.g. "Battery low"
 *  @param	urgency		The urgency, e.g NOTIFY_URGENCY_CRITICAL
 *  @param	point		The GtkWidget to point to. NULL is valid.
 *  @return			Success, if a notification is displayed.
 *
 *  @note	The message is printed to console with either g_warning or
 *		g_debug functions depending on the urgency.
 */
gboolean
libnotify_event (const gchar *content, const gint urgency, GtkWidget *point)
{
#if HAVE_LIBNOTIFY
	NotifyHandle *n = NULL;
	NotifyIcon *icon = NULL;
	NotifyHints *hints = NULL;
	gint x, y;

	/* assertion checks */
	g_assert (content);

	if (point) {
		get_widget_position (point, &x, &y);
		hints = notify_hints_new();
		notify_hints_set_int (hints, "x", x);
		notify_hints_set_int (hints, "y", y);
	}

	/* echo to terminal too */
	if (urgency == NOTIFY_URGENCY_CRITICAL)
		g_warning ("libnotify: %s : %s", NICENAME, content);
	else
		g_debug ("libnotify: %s : %s", NICENAME, content);

	/* use default g-p-m icon for now */
	icon = notify_icon_new_from_uri (GPM_DATA "gnome-power.png");
	n = notify_send_notification (NULL, /* replaces nothing */
			   NULL,
			   urgency,
			   NICENAME, content,
			   icon, /* icon */
			   TRUE, NOTIFY_TIMEOUT,
			   hints, /* hints */
			   NULL, /* no user data */
			   0);   /* no actions */
	notify_icon_destroy(icon);
	if (!n) {
		g_warning ("failed to send notification (%s)", content);
		return FALSE;
	}
	return TRUE;
#else
	GtkWidget *widget = NULL;

	/* assertion checks */
	g_assert (content);

	widget = gnome_message_box_new (content,
			GNOME_MESSAGE_BOX_WARNING,
			GNOME_STOCK_BUTTON_OK,
			NULL);
	gtk_window_set_title (GTK_WINDOW (widget), NICENAME);
	gtk_widget_show (widget);
	return TRUE;
#endif
}

/** Initialiser for libnotify
 *
 *  @param	nicename	The nicename, e.g. "GNOME Power Manager"
 *  @return			If we initialised correctly.
 *
 *  @note	This function must be called before any calls to
 *		libnotify_event are made.
 *
 *  @todo	When libnotify has settled down we will switch to runtime
 *		detection like we do for gnome-screensaver
 */
gboolean
libnotify_init (const gchar *nicename)
{
	gboolean ret = TRUE;

	/* assertion checks */
	g_assert (nicename);
#if HAVE_LIBNOTIFY
	ret = notify_glib_init (nicename, NULL);
#endif
	return ret;
}
