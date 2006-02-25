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

#define ICON_PREFIX_PRIMARY			"battery"
#define ICON_PREFIX_UPS				"ups"

#define GPM_STOCK_AC_ADAPTER			"ac-adapter"
#define GPM_STOCK_KEYBOARD_LOW			"keyboard-power-low"
#define GPM_STOCK_MOUSE_LOW			"mouse-power-low"
#define GPM_STOCK_BATTERY_CHARGED		"battery-charged"
#define GPM_STOCK_BATTERY_CHARGING_000		"battery-charging-000"
#define GPM_STOCK_BATTERY_CHARGING_020		"battery-charging-020"
#define GPM_STOCK_BATTERY_CHARGING_040		"battery-charging-040"
#define GPM_STOCK_BATTERY_CHARGING_060		"battery-charging-060"
#define GPM_STOCK_BATTERY_CHARGING_080		"battery-charging-080"
#define GPM_STOCK_BATTERY_CHARGING_100		"battery-charging-100"
#define GPM_STOCK_BATTERY_DISCHARGING_000	"battery-discharging-000"
#define GPM_STOCK_BATTERY_DISCHARGING_020	"battery-discharging-020"
#define GPM_STOCK_BATTERY_DISCHARGING_040	"battery-discharging-040"
#define GPM_STOCK_BATTERY_DISCHARGING_060	"battery-discharging-060"
#define GPM_STOCK_BATTERY_DISCHARGING_080	"battery-discharging-080"
#define GPM_STOCK_BATTERY_DISCHARGING_100	"battery-discharging-100"
#define GPM_STOCK_UPS_CHARGED			"ups-charged"
#define GPM_STOCK_UPS_CHARGING_000		"ups-charging-000"
#define GPM_STOCK_UPS_CHARGING_020		"ups-charging-020"
#define GPM_STOCK_UPS_CHARGING_040		"ups-charging-040"
#define GPM_STOCK_UPS_CHARGING_060		"ups-charging-060"
#define GPM_STOCK_UPS_CHARGING_080		"ups-charging-080"
#define GPM_STOCK_UPS_CHARGING_100		"ups-charging-100"
#define GPM_STOCK_UPS_DISCHARGING_000		"ups-discharging-000"
#define GPM_STOCK_UPS_DISCHARGING_020		"ups-discharging-020"
#define GPM_STOCK_UPS_DISCHARGING_040		"ups-discharging-040"
#define GPM_STOCK_UPS_DISCHARGING_060		"ups-discharging-060"
#define GPM_STOCK_UPS_DISCHARGING_080		"ups-discharging-080"
#define GPM_STOCK_UPS_DISCHARGING_100		"ups-discharging-100"
#define GPM_STOCK_SUSPEND_TO_DISK		"suspend-to-disk"
#define GPM_STOCK_SUSPEND_TO_RAM		"suspend-to-ram"

gboolean gpm_stock_icons_init     (void);
void     gpm_stock_icons_shutdown (void);

G_END_DECLS

#endif /* __GPM_STOCK_ICONS_H */
