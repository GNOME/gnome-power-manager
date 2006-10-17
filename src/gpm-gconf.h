/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMGCONF_H
#define __GPMGCONF_H

#include <glib.h>

G_BEGIN_DECLS

#define GPM_PREF_DIR 			"/apps/gnome-power-manager"

#define GPM_PREF_AC_SLEEP_COMPUTER	GPM_PREF_DIR "/ac_sleep_computer"
#define GPM_PREF_AC_SLEEP_DISPLAY	GPM_PREF_DIR "/ac_sleep_display"
#define GPM_PREF_AC_DPMS_METHOD		GPM_PREF_DIR "/ac_dpms_sleep_method"
#define GPM_PREF_AC_BRIGHTNESS		GPM_PREF_DIR "/ac_brightness"
#define GPM_PREF_AC_BRIGHTNESS_KBD	GPM_PREF_DIR "/ac_brightness_kbd"
#define GPM_PREF_BATTERY_SLEEP_COMPUTER	GPM_PREF_DIR "/battery_sleep_computer"
#define GPM_PREF_BATTERY_SLEEP_DISPLAY	GPM_PREF_DIR "/battery_sleep_display"
#define GPM_PREF_BATTERY_DPMS_METHOD	GPM_PREF_DIR "/battery_dpms_sleep_method"
#define GPM_PREF_BATTERY_BRIGHTNESS	GPM_PREF_DIR "/battery_brightness"
#define GPM_PREF_BATTERY_BRIGHTNESS_KBD	GPM_PREF_DIR "/battery_brightness_kbd"

#define GPM_PREF_DISPLAY_IDLE_DIM	GPM_PREF_DIR "/display_dim_on_idle"
#define GPM_PREF_DISPLAY_STATE_CHANGE	GPM_PREF_DIR "/display_state_change"
#define GPM_PREF_DISPLAY_AMBIENT	GPM_PREF_DIR "/display_ambient"

#define GPM_PREF_AC_SLEEP_TYPE		GPM_PREF_DIR "/action_ac_sleep_type"
#define GPM_PREF_BATTERY_SLEEP_TYPE	GPM_PREF_DIR "/action_battery_sleep_type"
#define GPM_PREF_BUTTON_SUSPEND		GPM_PREF_DIR "/action_button_suspend"
#define GPM_PREF_BUTTON_HIBERNATE	GPM_PREF_DIR "/action_button_hibernate"
#define GPM_PREF_BUTTON_POWER		GPM_PREF_DIR "/action_button_power"
#define GPM_PREF_AC_BUTTON_LID		GPM_PREF_DIR "/action_ac_button_lid"
#define GPM_PREF_BATTERY_BUTTON_LID	GPM_PREF_DIR "/action_battery_button_lid"
#define GPM_PREF_BATTERY_CRITICAL	GPM_PREF_DIR "/action_battery_critical"
#define GPM_PREF_UPS_CRITICAL		GPM_PREF_DIR "/action_ups_critical"
#define GPM_PREF_UPS_LOW		GPM_PREF_DIR "/action_ups_low"

#define GPM_PREF_USE_TIME_POLICY	GPM_PREF_DIR "/use_time_for_policy"
#define GPM_PREF_ICON_POLICY		GPM_PREF_DIR "/display_icon_policy"
#define GPM_PREF_NOTIFY_ACADAPTER	GPM_PREF_DIR "/notify_ac_adapter"
#define GPM_PREF_NOTIFY_BATTCHARGED	GPM_PREF_DIR "/notify_fully_charged"
#define GPM_PREF_NOTIFY_HAL_ERROR	GPM_PREF_DIR "/notify_hal_error"

#define GPM_PREF_CAN_SUSPEND		GPM_PREF_DIR "/can_suspend"
#define GPM_PREF_CAN_HIBERNATE		GPM_PREF_DIR "/can_hibernate"

#define GPM_PREF_NETWORKMANAGER_SLEEP	GPM_PREF_DIR "/networkmanager_sleep"

#define GPM_PREF_LOCK_USE_SCREENSAVER	GPM_PREF_DIR "/lock_use_screensaver_settings"
/* These are only effective if the system default is turned off. See bug #331164 */
#define GPM_PREF_LOCK_ON_BLANK_SCREEN	GPM_PREF_DIR "/lock_on_blank_screen"
#define GPM_PREF_LOCK_ON_SUSPEND	GPM_PREF_DIR "/lock_on_suspend"
#define GPM_PREF_LOCK_ON_HIBERNATE	GPM_PREF_DIR "/lock_on_hibernate"

#define GPM_PREF_IDLE_CHECK_CPU		GPM_PREF_DIR "/check_type_cpu"

/* This allows us to ignore policy on resume for a bit */
#define GPM_PREF_POLICY_TIMEOUT		GPM_PREF_DIR "/policy_suppression_timeout"

/* only valid when use_time_for_policy FALSE */
#define GPM_PREF_LOW_PERCENTAGE		GPM_PREF_DIR "/battery_percentage_low"
#define GPM_PREF_VERY_LOW_PERCENTAGE	GPM_PREF_DIR "/battery_percentage_very_low"
#define GPM_PREF_CRITICAL_PERCENTAGE	GPM_PREF_DIR "/battery_percentage_critical"
#define GPM_PREF_ACTION_PERCENTAGE	GPM_PREF_DIR "/battery_percentage_action"

/* only valid when use_time_for_policy TRUE */
#define GPM_PREF_LOW_TIME		GPM_PREF_DIR "/battery_time_low"
#define GPM_PREF_VERY_LOW_TIME		GPM_PREF_DIR "/battery_time_very_low"
#define GPM_PREF_CRITICAL_TIME		GPM_PREF_DIR "/battery_time_critical"
#define GPM_PREF_ACTION_TIME		GPM_PREF_DIR "/battery_time_action"

#define GPM_PREF_PANEL_DIM_BRIGHTNESS	GPM_PREF_DIR "/laptop_panel_dim_brightness"
#define GPM_PREF_SHOW_ACTIONS_IN_MENU	GPM_PREF_DIR "/show_actions_in_menu"
#define GPM_PREF_RATE_EXP_AVE_FACTOR	GPM_PREF_DIR "/rate_exponential_average_factor"
#define GPM_PREF_BATT_EVENT_WHEN_CLOSED	GPM_PREF_DIR "/battery_event_when_closed"
#define GPM_PREF_GRAPH_DATA_MAX_TIME	GPM_PREF_DIR "/graph_data_max_time"
#define GPM_PREF_ENABLE_BEEPING		GPM_PREF_DIR "/enable_pcskr_beeping"
#define GPM_PREF_IGNORE_INHIBITS	GPM_PREF_DIR "/ignore_inhibit_requests"

#define GPM_PREF_AC_LOWPOWER		GPM_PREF_DIR "/use_lowpower_ac"
#define GPM_PREF_UPS_LOWPOWER		GPM_PREF_DIR "/use_lowpower_ups"
#define GPM_PREF_BATTERY_LOWPOWER	GPM_PREF_DIR "/use_lowpower_battery"

#define GPM_PREF_AC_CPUFREQ_POLICY	GPM_PREF_DIR "/cpufreq_ac_policy"
#define GPM_PREF_AC_CPUFREQ_VALUE	GPM_PREF_DIR "/cpufreq_ac_performance"
#define GPM_PREF_BATTERY_CPUFREQ_POLICY	GPM_PREF_DIR "/cpufreq_battery_policy"
#define GPM_PREF_BATTERY_CPUFREQ_VALUE	GPM_PREF_DIR "/cpufreq_battery_performance"
#define GPM_PREF_USE_NICE		GPM_PREF_DIR "/cpufreq_consider_nice"

#define GPM_PREF_SESSION_REQUEST_SAVE	GPM_PREF_DIR "/session_request_save"

#define GPM_PREF_STAT_SHOW_LEGEND	GPM_PREF_DIR "/statistics_show_legend"
#define GPM_PREF_STAT_SHOW_EVENTS	GPM_PREF_DIR "/statistics_show_events"
#define GPM_PREF_STAT_GRAPH_TYPE	GPM_PREF_DIR "/statistics_graph_type"

G_END_DECLS

#endif	/* __GPMGCONF_H */
