/***************************************************************************
 *
 * gpm-notification.h : gnome-power-manager
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
 * Taken in part from:
 * - notibat (C) 2004 Benjamin Kahn <xkahn@zoned.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

#ifndef _GPN_H
#define _GPN_H

typedef struct {
	EggTrayIcon *tray_icon;
	GtkTooltips *tray_icon_tooltip;
	GtkWidget *popup_menu;
	GtkWidget *image;
	GtkWidget *evbox;
} TrayData;

typedef struct {
	gboolean show;
	gboolean showIfFull;
	GString *tooltip;
	TrayData *td;
	GenericObject *slotData;
	gint currentObject;
	gint displayOptions;
} IconData;

void free_icon_structure (void);
GenericObject * get_main_icon_slot (void);
GString * get_main_tooltip (void);

/* doesn't belong here, but maybe if gpn gets it's own process it will belong :-) */
void callback_gconf_key_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);

/* wrapper functions */
void gpn_icon_initialise ();
void gpn_icon_destroy ();
void gpn_icon_update ();

#endif	/* _GPN_H */
