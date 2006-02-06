/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Jorn Baayen
 * Copyright (C) 2003,2004 Colin Walters <walters@verbum.org>
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

#ifndef __GPM_STOCK_ICONS_H
#define __GPM_STOCK_ICONS_H

G_BEGIN_DECLS

#define GPM_STOCK_AC_ADAPTER	"gnome-dev-acadapter"
#define GPM_STOCK_MOUSE		"gnome-power-mouse"
#define GPM_STOCK_KEYBOARD	"gnome-power-keyboard"
#define GPM_STOCK_AC_0_OF_8	"gnome-power-ac-0-of-8"
#define GPM_STOCK_AC_1_OF_8	"gnome-power-ac-1-of-8"
#define GPM_STOCK_AC_2_OF_8	"gnome-power-ac-2-of-8"
#define GPM_STOCK_AC_3_OF_8	"gnome-power-ac-3-of-8"
#define GPM_STOCK_AC_4_OF_8 	"gnome-power-ac-4-of-8"
#define GPM_STOCK_AC_5_OF_8	"gnome-power-ac-5-of-8"
#define GPM_STOCK_AC_6_OF_8	"gnome-power-ac-6-of-8"
#define GPM_STOCK_AC_7_OF_8	"gnome-power-ac-7-of-8"
#define GPM_STOCK_AC_8_OF_8	"gnome-power-ac-8-of-8"
#define GPM_STOCK_AC_CHARGED	"gnome-power-ac-charged"
#define GPM_STOCK_BAT_0_OF_8	"gnome-power-bat-0-of-8"
#define GPM_STOCK_BAT_1_OF_8	"gnome-power-bat-1-of-8"
#define GPM_STOCK_BAT_2_OF_8	"gnome-power-bat-2-of-8"
#define GPM_STOCK_BAT_3_OF_8	"gnome-power-bat-3-of-8"
#define GPM_STOCK_BAT_4_OF_8	"gnome-power-bat-4-of-8"
#define GPM_STOCK_BAT_5_OF_8	"gnome-power-bat-5-of-8"
#define GPM_STOCK_BAT_6_OF_8	"gnome-power-bat-6-of-8"
#define GPM_STOCK_BAT_7_OF_8	"gnome-power-bat-7-of-8"
#define GPM_STOCK_BAT_8_OF_8	"gnome-power-bat-8-of-8"
#define GPM_STOCK_UPS_0_OF_8	"gnome-power-ups-0-of-8"
#define GPM_STOCK_UPS_1_OF_8	"gnome-power-ups-1-of-8"
#define GPM_STOCK_UPS_2_OF_8	"gnome-power-ups-2-of-8"
#define GPM_STOCK_UPS_3_OF_8	"gnome-power-ups-3-of-8"
#define GPM_STOCK_UPS_4_OF_8	"gnome-power-ups-4-of-8"
#define GPM_STOCK_UPS_5_OF_8	"gnome-power-ups-5-of-8"
#define GPM_STOCK_UPS_6_OF_8	"gnome-power-ups-6-of-8"
#define GPM_STOCK_UPS_7_OF_8	"gnome-power-ups-7-of-8"
#define GPM_STOCK_UPS_8_OF_8	"gnome-power-ups-8-of-8"

#define GNOME_DEV_BATTERY       "gnome-dev-battery"
#define GNOME_DEV_HARDDISK      "gnome-dev-harddisk"
#define GNOME_DEV_MEMORY        "gnome-dev-memory"

gboolean gpm_stock_icons_init     (void);
void     gpm_stock_icons_shutdown (void);

G_END_DECLS

#endif /* __GPM_STOCK_ICONS_H */
