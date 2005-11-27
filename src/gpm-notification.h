/** @file	gpm-notification.h
 *  @brief	GNOME Power Notification
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
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

#ifndef _GPMNOTIFICATION_H
#define _GPMNOTIFICATION_H

#include "eggtrayicon.h"

/** The TrayData struct holds all the global pointers to tray objects
 *
 */
typedef struct {
	EggTrayIcon *tray_icon;		/**< The tray icon		*/
	GtkTooltips *tray_icon_tooltip;	/**< The tooltip		*/
	GtkWidget *popup_menu;		/**< The pop-down menu		*/
	GtkWidget *image;		/**< The image shown in the tray*/
	GtkWidget *evbox;		/**< The event box (click)	*/
} TrayData;

GtkWidget *get_notification_icon (void);

/* wrapper functions */
void gpn_icon_destroy (void);
void gpn_icon_update (void);

#endif	/* _GPMNOTIFICATION_H */
