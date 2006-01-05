/*
 * Copyright (C) 2005 Richard Hughes <hughsient@gmail.com>
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 *
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

#ifndef _GPM_PREFS_H
#define _GPM_PREFS_H

#define GPM_PREF_DIR 			"/apps/gnome-power-manager"

#define GPM_PREF_AC_SLEEP_COMPUTER	GPM_PREF_DIR "/policy/ac/sleep_computer"
#define GPM_PREF_AC_SLEEP_DISPLAY	GPM_PREF_DIR "/policy/ac/sleep_display"
#define GPM_PREF_AC_BRIGHTNESS		GPM_PREF_DIR "/policy/ac/brightness"
#define GPM_PREF_BATTERY_SLEEP_COMPUTER	GPM_PREF_DIR "/policy/battery/sleep_computer"
#define GPM_PREF_BATTERY_SLEEP_DISPLAY	GPM_PREF_DIR "/policy/battery/sleep_display"
#define GPM_PREF_BATTERY_BRIGHTNESS	GPM_PREF_DIR "/policy/battery/brightness"
#define GPM_PREF_SLEEP_TYPE		GPM_PREF_DIR "/policy/sleep_type"
#define GPM_PREF_BUTTON_SUSPEND		GPM_PREF_DIR "/policy/button_suspend"
#define GPM_PREF_BUTTON_LID		GPM_PREF_DIR "/policy/button_lid"
#define GPM_PREF_BATTERY_CRITICAL	GPM_PREF_DIR "/policy/battery_critical"
#define GPM_PREF_IDLE			GPM_PREF_DIR "/policy/idle"
#define GPM_PREF_ICON_POLICY		GPM_PREF_DIR "/general/display_icon_policy"
#define GPM_PREF_THRESHOLD_LOW		GPM_PREF_DIR "/general/threshold_low"
#define GPM_PREF_THRESHOLD_CRITICAL	GPM_PREF_DIR "/general/threshold_critical"
#define GPM_PREF_NOTIFY_ACADAPTER	GPM_PREF_DIR "/notify/ac_adapter"
#define GPM_PREF_NOTIFY_BATTCHARGED	GPM_PREF_DIR "/notify/fully_charged"

#define ICON_POLICY_ALWAYS	"always"
#define ICON_POLICY_CHARGE	"charge"
#define ICON_POLICY_CRITICAL	"critical"
#define ICON_POLICY_NEVER	"never"

#define ACTION_SUSPEND		"suspend"
#define ACTION_SHUTDOWN		"shutdown"
#define ACTION_HIBERNATE	"hibernate"
#define ACTION_NOTHING		"nothing"

#endif	/* _GPM_PREFS_H */

