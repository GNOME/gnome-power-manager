/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Alex Murray <murray.alex@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "gpm-osd-dialog.h"

/**
 * gpm_osd_dialog_init:
 *
 * Initialises a popup dialog, and makes sure that it matches the compositing of the screen.
 **/
void
gpm_osd_dialog_init (GtkWidget **popup,
		     const gchar *icon_name)
{
	if (*popup != NULL
	    && !gsd_osd_window_is_valid (GSD_OSD_WINDOW (*popup))) {
		gtk_widget_destroy (*popup);
		*popup = NULL;
	}

	if (*popup == NULL) {
		*popup= gsd_media_keys_window_new ();
		gsd_media_keys_window_set_action_custom (GSD_MEDIA_KEYS_WINDOW (*popup),
							 icon_name,
							 TRUE);
		gtk_window_set_position (GTK_WINDOW (*popup), GTK_WIN_POS_NONE);
	}
}

/**
 * gpm_osd_dialog_show:
 *
 * Show the brightness popup, and place it nicely on the screen.
 **/
void
gpm_osd_dialog_show (GtkWidget *popup)
{
	int            orig_w;
	int            orig_h;
	int            screen_w;
	int            screen_h;
	int            x;
	int            y;
	int            pointer_x;
	int            pointer_y;
	GtkRequisition win_req;
	GdkScreen     *pointer_screen;
	GdkRectangle   geometry;
	int            monitor;
        GdkDisplay    *display;
        GdkDeviceManager *device_manager;
        GdkDevice     *device;

	/*
	 * get the window size
	 * if the window hasn't been mapped, it doesn't necessarily
	 * know its true size, yet, so we need to jump through hoops
	 */
	gtk_window_get_default_size (GTK_WINDOW (popup), &orig_w, &orig_h);
	gtk_widget_get_preferred_size(popup, &win_req, NULL);

	if (win_req.width > orig_w) {
		orig_w = win_req.width;
	}
	if (win_req.height > orig_h) {
		orig_h = win_req.height;
	}

	pointer_screen = NULL;
        display = gtk_widget_get_display (popup);
        device_manager = gdk_display_get_device_manager (display);
        device = gdk_device_manager_get_client_pointer (device_manager);
        gdk_device_get_position (device,
				 &pointer_screen,
				 &pointer_x,
				 &pointer_y);
	monitor = gdk_screen_get_monitor_at_point (pointer_screen,
						   pointer_x,
						   pointer_y);

	gdk_screen_get_monitor_geometry (pointer_screen,
					 monitor,
					 &geometry);

	screen_w = geometry.width;
	screen_h = geometry.height;

	x = ((screen_w - orig_w) / 2) + geometry.x;
	y = geometry.y + (screen_h / 2) + (screen_h / 2 - orig_h) / 2;

	gtk_window_move (GTK_WINDOW (popup), x, y);

	gtk_widget_show (popup);

	gdk_display_sync (gtk_widget_get_display (popup));
}

