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

#define GPM_STOCK_APP_ICON			"gnome-power-manager"
#define GPM_STOCK_AC_ADAPTER			"gpm-ac-adapter"
#define GPM_STOCK_KEYBOARD_LOW			"gpm-keyboard-power-low"
#define GPM_STOCK_MOUSE_LOW			"gpm-mouse-power-low"
#define GPM_STOCK_BATTERY_LOW			"gpm-battery-low"
#define GPM_STOCK_BATTERY_CRITICAL		"gpm-battery-critical"
#define GPM_STOCK_BATTERY_CHARGED		"gpm-battery-charged"
#define GPM_STOCK_BATTERY_BROKEN		"gpm-battery-broken"
#define GPM_STOCK_BATTERY_CHARGING_000		"gpm-battery-charging-000"
#define GPM_STOCK_BATTERY_CHARGING_020		"gpm-battery-charging-020"
#define GPM_STOCK_BATTERY_CHARGING_040		"gpm-battery-charging-040"
#define GPM_STOCK_BATTERY_CHARGING_060		"gpm-battery-charging-060"
#define GPM_STOCK_BATTERY_CHARGING_080		"gpm-battery-charging-080"
#define GPM_STOCK_BATTERY_CHARGING_100		"gpm-battery-charging-100"
#define GPM_STOCK_BATTERY_DISCHARGING_000	"gpm-battery-discharging-000"
#define GPM_STOCK_BATTERY_DISCHARGING_020	"gpm-battery-discharging-020"
#define GPM_STOCK_BATTERY_DISCHARGING_040	"gpm-battery-discharging-040"
#define GPM_STOCK_BATTERY_DISCHARGING_060	"gpm-battery-discharging-060"
#define GPM_STOCK_BATTERY_DISCHARGING_080	"gpm-battery-discharging-080"
#define GPM_STOCK_BATTERY_DISCHARGING_100	"gpm-battery-discharging-100"
#define GPM_STOCK_UPS_BROKEN			"gpm-ups-broken"
#define GPM_STOCK_UPS_LOW			"gpm-ups-low"
#define GPM_STOCK_UPS_CRITICAL			"gpm-ups-critical"
#define GPM_STOCK_UPS_CHARGED			"gpm-ups-charged"
#define GPM_STOCK_UPS_CHARGING_000		"gpm-ups-charging-000"
#define GPM_STOCK_UPS_CHARGING_020		"gpm-ups-charging-020"
#define GPM_STOCK_UPS_CHARGING_040		"gpm-ups-charging-040"
#define GPM_STOCK_UPS_CHARGING_060		"gpm-ups-charging-060"
#define GPM_STOCK_UPS_CHARGING_080		"gpm-ups-charging-080"
#define GPM_STOCK_UPS_CHARGING_100		"gpm-ups-charging-100"
#define GPM_STOCK_UPS_DISCHARGING_000		"gpm-ups-discharging-000"
#define GPM_STOCK_UPS_DISCHARGING_020		"gpm-ups-discharging-020"
#define GPM_STOCK_UPS_DISCHARGING_040		"gpm-ups-discharging-040"
#define GPM_STOCK_UPS_DISCHARGING_060		"gpm-ups-discharging-060"
#define GPM_STOCK_UPS_DISCHARGING_080		"gpm-ups-discharging-080"
#define GPM_STOCK_UPS_DISCHARGING_100		"gpm-ups-discharging-100"
#define GPM_STOCK_SUSPEND_TO_DISK		"gpm-suspend-to-disk"
#define GPM_STOCK_SUSPEND_TO_RAM		"gpm-suspend-to-ram"
#define GPM_STOCK_BRIGHTNESS			"gpm-brightness"

gboolean gpm_stock_icons_init     (void);
void     gpm_stock_icons_shutdown (void);

G_END_DECLS

#endif /* __GPM_STOCK_ICONS_H */
