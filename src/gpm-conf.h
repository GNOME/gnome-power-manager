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

#ifndef __GPMCONF_H
#define __GPMCONF_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_CONF		(gpm_conf_get_type ())
#define GPM_CONF(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_CONF, GpmConf))
#define GPM_CONF_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_CONF, GpmConfClass))
#define GPM_IS_CONF(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_CONF))
#define GPM_IS_CONF_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_CONF))
#define GPM_CONF_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_CONF, GpmConfClass))


#define GPM_CONF_DIR 			"/apps/gnome-power-manager"

#define GPM_CONF_AC_SLEEP_COMPUTER	GPM_CONF_DIR "/ac_sleep_computer"
#define GPM_CONF_AC_SLEEP_DISPLAY	GPM_CONF_DIR "/ac_sleep_display"
#define GPM_CONF_AC_DPMS_METHOD		GPM_CONF_DIR "/ac_dpms_sleep_method"
#define GPM_CONF_AC_BRIGHTNESS		GPM_CONF_DIR "/ac_brightness"
#define GPM_CONF_AC_BRIGHTNESS_KBD	GPM_CONF_DIR "/ac_brightness_kbd"
#define GPM_CONF_AC_IDLE_DIM		GPM_CONF_DIR "/ac_dim_on_idle"
#define GPM_CONF_BATTERY_SLEEP_COMPUTER	GPM_CONF_DIR "/battery_sleep_computer"
#define GPM_CONF_BATTERY_SLEEP_DISPLAY	GPM_CONF_DIR "/battery_sleep_display"
#define GPM_CONF_BATTERY_DPMS_METHOD	GPM_CONF_DIR "/battery_dpms_sleep_method"
#define GPM_CONF_BATTERY_BRIGHTNESS	GPM_CONF_DIR "/battery_brightness"
#define GPM_CONF_BATTERY_BRIGHTNESS_KBD	GPM_CONF_DIR "/battery_brightness_kbd"
#define GPM_CONF_BATTERY_IDLE_DIM	GPM_CONF_DIR "/battery_dim_on_idle"

#define GPM_CONF_DISPLAY_STATE_CHANGE	GPM_CONF_DIR "/display_state_change"
#define GPM_CONF_DISPLAY_AMBIENT	GPM_CONF_DIR "/display_ambient"

#define GPM_CONF_AC_SLEEP_TYPE		GPM_CONF_DIR "/action_ac_sleep_type"
#define GPM_CONF_BATTERY_SLEEP_TYPE	GPM_CONF_DIR "/action_battery_sleep_type"
#define GPM_CONF_BUTTON_SUSPEND		GPM_CONF_DIR "/action_button_suspend"
#define GPM_CONF_BUTTON_HIBERNATE	GPM_CONF_DIR "/action_button_hibernate"
#define GPM_CONF_BUTTON_POWER		GPM_CONF_DIR "/action_button_power"
#define GPM_CONF_AC_BUTTON_LID		GPM_CONF_DIR "/action_ac_button_lid"
#define GPM_CONF_BATTERY_BUTTON_LID	GPM_CONF_DIR "/action_battery_button_lid"
#define GPM_CONF_BATTERY_CRITICAL	GPM_CONF_DIR "/action_battery_critical"
#define GPM_CONF_UPS_CRITICAL		GPM_CONF_DIR "/action_ups_critical"
#define GPM_CONF_UPS_LOW		GPM_CONF_DIR "/action_ups_low"

#define GPM_CONF_USE_TIME_POLICY	GPM_CONF_DIR "/use_time_for_policy"
#define GPM_CONF_ICON_POLICY		GPM_CONF_DIR "/display_icon_policy"
#define GPM_CONF_NOTIFY_ACADAPTER	GPM_CONF_DIR "/notify_ac_adapter"
#define GPM_CONF_NOTIFY_BATTCHARGED	GPM_CONF_DIR "/notify_fully_charged"
#define GPM_CONF_NOTIFY_HAL_ERROR	GPM_CONF_DIR "/notify_hal_error"
#define GPM_CONF_NOTIFY_LOW_POWER	GPM_CONF_DIR "/notify_low_power"

#define GPM_CONF_CAN_SUSPEND		GPM_CONF_DIR "/can_suspend"
#define GPM_CONF_CAN_HIBERNATE		GPM_CONF_DIR "/can_hibernate"

#define GPM_CONF_NETWORKMANAGER_SLEEP	GPM_CONF_DIR "/networkmanager_sleep"
#define GPM_CONF_SHOW_BATTERY_WARNING	GPM_CONF_DIR "/show_recalled_battery_warning"

#define GPM_CONF_LOCK_USE_SCREENSAVER	GPM_CONF_DIR "/lock_use_screensaver_settings"
/* These are only effective if the system default is turned off. See bug #331164 */
#define GPM_CONF_LOCK_ON_BLANK_SCREEN	GPM_CONF_DIR "/lock_on_blank_screen"
#define GPM_CONF_LOCK_ON_SUSPEND	GPM_CONF_DIR "/lock_on_suspend"
#define GPM_CONF_LOCK_ON_HIBERNATE	GPM_CONF_DIR "/lock_on_hibernate"

#define GPM_CONF_IDLE_CHECK_CPU		GPM_CONF_DIR "/check_type_cpu"
#define GPM_CONF_INVALID_TIMEOUT	GPM_CONF_DIR "/invalid_timeout"

#define GPM_CONF_LAPTOP_USES_EXT_MON	GPM_CONF_DIR "/laptop_uses_external_monitor"

/* This allows us to ignore policy on resume for a bit */
#define GPM_CONF_POLICY_TIMEOUT		GPM_CONF_DIR "/policy_suppression_timeout"

/* only valid when use_time_for_policy FALSE */
#define GPM_CONF_LOW_PERCENTAGE		GPM_CONF_DIR "/battery_percentage_low"
#define GPM_CONF_VERY_LOW_PERCENTAGE	GPM_CONF_DIR "/battery_percentage_very_low"
#define GPM_CONF_CRITICAL_PERCENTAGE	GPM_CONF_DIR "/battery_percentage_critical"
#define GPM_CONF_ACTION_PERCENTAGE	GPM_CONF_DIR "/battery_percentage_action"

/* only valid when use_time_for_policy TRUE */
#define GPM_CONF_LOW_TIME		GPM_CONF_DIR "/battery_time_low"
#define GPM_CONF_VERY_LOW_TIME		GPM_CONF_DIR "/battery_time_very_low"
#define GPM_CONF_CRITICAL_TIME		GPM_CONF_DIR "/battery_time_critical"
#define GPM_CONF_ACTION_TIME		GPM_CONF_DIR "/battery_time_action"

#define GPM_CONF_PANEL_DIM_BRIGHTNESS	GPM_CONF_DIR "/laptop_panel_dim_brightness"
#define GPM_CONF_SHOW_ACTIONS_IN_MENU	GPM_CONF_DIR "/show_actions_in_menu"
#define GPM_CONF_RATE_EXP_AVE_FACTOR	GPM_CONF_DIR "/rate_exponential_average_factor"
#define GPM_CONF_BATT_EVENT_WHEN_CLOSED	GPM_CONF_DIR "/battery_event_when_closed"
#define GPM_CONF_GRAPH_DATA_MAX_TIME	GPM_CONF_DIR "/graph_data_max_time"
#define GPM_CONF_ENABLE_BEEPING		GPM_CONF_DIR "/enable_sounds"
#define GPM_CONF_IGNORE_INHIBITS	GPM_CONF_DIR "/ignore_inhibit_requests"

#define GPM_CONF_AC_LOWPOWER		GPM_CONF_DIR "/use_lowpower_ac"
#define GPM_CONF_UPS_LOWPOWER		GPM_CONF_DIR "/use_lowpower_ups"
#define GPM_CONF_BATTERY_LOWPOWER	GPM_CONF_DIR "/use_lowpower_battery"

#define GPM_CONF_AC_CPUFREQ_POLICY	GPM_CONF_DIR "/cpufreq_ac_policy"
#define GPM_CONF_AC_CPUFREQ_VALUE	GPM_CONF_DIR "/cpufreq_ac_performance"
#define GPM_CONF_BATTERY_CPUFREQ_POLICY	GPM_CONF_DIR "/cpufreq_battery_policy"
#define GPM_CONF_BATTERY_CPUFREQ_VALUE	GPM_CONF_DIR "/cpufreq_battery_performance"
#define GPM_CONF_USE_NICE		GPM_CONF_DIR "/cpufreq_consider_nice"

#define GPM_CONF_STAT_SHOW_LEGEND	GPM_CONF_DIR "/statistics_show_legend"
#define GPM_CONF_STAT_SHOW_EVENTS	GPM_CONF_DIR "/statistics_show_events"
#define GPM_CONF_STAT_GRAPH_TYPE	GPM_CONF_DIR "/statistics_graph_type"

/* we use the gnome-session key now */
#define GPM_CONF_SESSION_REQUEST_SAVE	"/apps/gnome-session/options/auto_save_session"

typedef struct GpmConfPrivate GpmConfPrivate;

typedef struct
{
	GObject		 parent;
	GpmConfPrivate	*priv;
} GpmConf;

typedef struct
{
	GObjectClass	parent_class;
	void		(* value_changed)		(GpmConf	*conf,
							 const gchar	*udi);
} GpmConfClass;

GType		 gpm_conf_get_type			(void);
GpmConf		*gpm_conf_new				(void);

gboolean	 gpm_conf_get_bool			(GpmConf	*conf,
							 const gchar	*key,
							 gboolean	*value);
gboolean	 gpm_conf_get_string			(GpmConf	*conf,
							 const gchar	*key,
							 gchar		**value);
gboolean	 gpm_conf_get_int			(GpmConf	*conf,
							 const gchar	*key,
							 gint		*value);
gboolean	 gpm_conf_get_uint			(GpmConf	*conf,
							 const gchar	*key,
							 guint		*value);

gboolean	 gpm_conf_set_bool			(GpmConf	*conf,
							 const gchar	*key,
							 gboolean	 value);
gboolean	 gpm_conf_set_string			(GpmConf	*conf,
							 const gchar	*key,
							 const gchar	*value);
gboolean	 gpm_conf_set_int			(GpmConf	*conf,
							 const gchar	*key,
							 gint		 value);
gboolean	 gpm_conf_set_uint			(GpmConf	*conf,
							 const gchar	*key,
							 guint		 value);

gboolean	 gpm_conf_is_writable			(GpmConf	*conf,
							 const gchar	*key,
							 gboolean	*writable);

G_END_DECLS

#endif	/* __GPMCONF_H */
