/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
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

#ifndef __GPM_PREFS_H
#define __GPM_PREFS_H

G_BEGIN_DECLS

#define GPM_PREF_DIR 			"/apps/gnome-power-manager"

#define GPM_PREF_AC_SLEEP_COMPUTER	GPM_PREF_DIR "/ac_sleep_computer"
#define GPM_PREF_AC_SLEEP_DISPLAY	GPM_PREF_DIR "/ac_sleep_display"
#define GPM_PREF_AC_BRIGHTNESS		GPM_PREF_DIR "/ac_brightness"
#define GPM_PREF_BATTERY_SLEEP_COMPUTER	GPM_PREF_DIR "/battery_sleep_computer"
#define GPM_PREF_BATTERY_SLEEP_DISPLAY	GPM_PREF_DIR "/battery_sleep_display"
#define GPM_PREF_BATTERY_BRIGHTNESS	GPM_PREF_DIR "/battery_brightness"
#define GPM_PREF_SLEEP_TYPE		GPM_PREF_DIR "/action_sleep_type"
#define GPM_PREF_BUTTON_SUSPEND		GPM_PREF_DIR "/action_button_suspend"
#define GPM_PREF_AC_BUTTON_LID		GPM_PREF_DIR "/action_ac_button_lid"
#define GPM_PREF_BATTERY_BUTTON_LID	GPM_PREF_DIR "/action_battery_button_lid"
#define GPM_PREF_BATTERY_CRITICAL	GPM_PREF_DIR "/action_battery_critical"
#define GPM_PREF_ICON_POLICY		GPM_PREF_DIR "/display_icon_policy"
#define GPM_PREF_NOTIFY_ACADAPTER	GPM_PREF_DIR "/notify_ac_adapter"
#define GPM_PREF_NOTIFY_BATTCHARGED	GPM_PREF_DIR "/notify_fully_charged"
#define GPM_PREF_CAN_SUSPEND		GPM_PREF_DIR "/can_suspend"
#define GPM_PREF_CAN_HIBERNATE		GPM_PREF_DIR "/can_hibernate"

typedef enum {
	GPM_ICON_POLICY_ALWAYS,
	GPM_ICON_POLICY_CHARGE,
	GPM_ICON_POLICY_CRITICAL,
	GPM_ICON_POLICY_NEVER
} GpmIconPolicy;

#define ACTION_SUSPEND			"suspend"
#define ACTION_SHUTDOWN			"shutdown"
#define ACTION_HIBERNATE		"hibernate"
#define ACTION_NOTHING			"nothing"

G_END_DECLS

#endif	/* __GPM_PREFS_H */

