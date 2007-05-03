/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#define GPM_CONF_NOTIFY_PERHAPS_RECALL	GPM_CONF_DIR "/notify/perhaps_recall"
#define GPM_CONF_NOTIFY_LOW_CAPACITY	GPM_CONF_DIR "/notify/low_capacity"
#define GPM_CONF_NOTIFY_DISCHARGING	GPM_CONF_DIR "/notify/discharging"
#define GPM_CONF_NOTIFY_FULLY_CHARGED	GPM_CONF_DIR "/notify/fully_charged"
#define GPM_CONF_NOTIFY_SLEEP_FAILED	GPM_CONF_DIR "/notify/sleep_failed"
#define GPM_CONF_NOTIFY_LOW_POWER	GPM_CONF_DIR "/notify/low_power"
#define GPM_CONF_NOTIFY_ESTIMATED_DATA	GPM_CONF_DIR "/notify/estimated_data"

#define GPM_CONF_LOW_PERCENTAGE		GPM_CONF_DIR "/thresholds/percentage_low"
#define GPM_CONF_CRITICAL_PERCENTAGE	GPM_CONF_DIR "/thresholds/percentage_critical"
#define GPM_CONF_ACTION_PERCENTAGE	GPM_CONF_DIR "/thresholds/percentage_action"
#define GPM_CONF_LOW_TIME		GPM_CONF_DIR "/thresholds/time_low"
#define GPM_CONF_CRITICAL_TIME		GPM_CONF_DIR "/thresholds/time_critical"
#define GPM_CONF_ACTION_TIME		GPM_CONF_DIR "/thresholds/time_action"

#define GPM_CONF_AC_SLEEP_TYPE		GPM_CONF_DIR "/ac/sleep_type"
#define GPM_CONF_AC_BUTTON_LID		GPM_CONF_DIR "/ac/button_lid"
#define GPM_CONF_AC_SLEEP_COMPUTER	GPM_CONF_DIR "/ac/sleep_computer"
#define GPM_CONF_AC_SLEEP_DISPLAY	GPM_CONF_DIR "/ac/sleep_display"
#define GPM_CONF_AC_DPMS_METHOD		GPM_CONF_DIR "/ac/dpms_sleep_method"
#define GPM_CONF_AC_BRIGHTNESS		GPM_CONF_DIR "/ac/brightness"
#define GPM_CONF_AC_BRIGHTNESS_KBD	GPM_CONF_DIR "/ac/brightness_kbd"
#define GPM_CONF_AC_IDLE_DIM		GPM_CONF_DIR "/ac/dim_on_idle"
#define GPM_CONF_AC_CPUFREQ_POLICY	GPM_CONF_DIR "/ac/cpufreq_policy"
#define GPM_CONF_AC_CPUFREQ_VALUE	GPM_CONF_DIR "/ac/cpufreq_performance"
#define GPM_CONF_AC_LOWPOWER		GPM_CONF_DIR "/ac/use_lowpower"

#define GPM_CONF_BATT_SLEEP_TYPE	GPM_CONF_DIR "/battery/sleep_type"
#define GPM_CONF_BATT_BUTTON_LID	GPM_CONF_DIR "/battery/button_lid"
#define GPM_CONF_BATT_CRITICAL		GPM_CONF_DIR "/battery/critical"
#define GPM_CONF_BATT_SLEEP_COMPUTER	GPM_CONF_DIR "/battery/sleep_computer"
#define GPM_CONF_BATT_SLEEP_DISPLAY	GPM_CONF_DIR "/battery/sleep_display"
#define GPM_CONF_BATT_DPMS_METHOD	GPM_CONF_DIR "/battery/dpms_sleep_method"
#define GPM_CONF_BATT_BRIGHTNESS	GPM_CONF_DIR "/battery/brightness"
#define GPM_CONF_BATT_BRIGHTNESS_KBD	GPM_CONF_DIR "/battery/brightness_kbd"
#define GPM_CONF_BATT_IDLE_DIM		GPM_CONF_DIR "/battery/dim_on_idle"
#define GPM_CONF_BATT_CPUFREQ_POLICY	GPM_CONF_DIR "/battery/cpufreq_policy"
#define GPM_CONF_BATT_CPUFREQ_VALUE	GPM_CONF_DIR "/battery/cpufreq_performance"
#define GPM_CONF_BATT_EVENT_WHEN_CLOSED	GPM_CONF_DIR "/battery/event_when_closed"
#define GPM_CONF_BATT_LOWPOWER		GPM_CONF_DIR "/battery/use_lowpower"

#define GPM_CONF_UPS_CRITICAL		GPM_CONF_DIR "/ups/critical"
#define GPM_CONF_UPS_LOW		GPM_CONF_DIR "/ups/low"
#define GPM_CONF_UPS_LOWPOWER		GPM_CONF_DIR "/ups/use_lowpower"

#define GPM_CONF_BUTTON_SUSPEND		GPM_CONF_DIR "/buttons/suspend"
#define GPM_CONF_BUTTON_HIBERNATE	GPM_CONF_DIR "/buttons/hibernate"
#define GPM_CONF_BUTTON_POWER		GPM_CONF_DIR "/buttons/power"

/* These are only effective if the system default is turned off. See bug #331164 */
#define GPM_CONF_LOCK_USE_SCREENSAVER	GPM_CONF_DIR "/lock/use_screensaver_settings"
#define GPM_CONF_LOCK_ON_BLANK_SCREEN	GPM_CONF_DIR "/lock/blank_screen"
#define GPM_CONF_LOCK_ON_SUSPEND	GPM_CONF_DIR "/lock/suspend"
#define GPM_CONF_LOCK_ON_HIBERNATE	GPM_CONF_DIR "/lock/hibernate"
#define GPM_CONF_LOCK_GNOME_KEYRING	GPM_CONF_DIR "/lock/gnome_keyring"

#define GPM_CONF_STAT_SHOW_AXIS_LABELS	GPM_CONF_DIR "/statistics/show_axis_labels"
#define GPM_CONF_STAT_SHOW_EVENTS	GPM_CONF_DIR "/statistics/show_events"
#define GPM_CONF_STAT_SMOOTH_DATA	GPM_CONF_DIR "/statistics/smooth_data"
#define GPM_CONF_STAT_GRAPH_TYPE	GPM_CONF_DIR "/statistics/graph_type"
#define GPM_CONF_GRAPH_DATA_MAX_TIME	GPM_CONF_DIR "/statistics/data_max_time"

#define GPM_CONF_ICON_POLICY		GPM_CONF_DIR "/ui/display_icon_policy"
#define GPM_CONF_UI_SHOW_CPUFREQ	GPM_CONF_DIR "/ui/cpufreq_show"
#define GPM_CONF_SHOW_ACTIONS_IN_MENU	GPM_CONF_DIR "/ui/show_actions_in_menu"
#define GPM_CONF_ENABLE_BEEPING		GPM_CONF_DIR "/ui/enable_sound"

#define GPM_CONF_USE_NICE		GPM_CONF_DIR "/cpufreq_consider_nice"
#define GPM_CONF_DISPLAY_STATE_CHANGE	GPM_CONF_DIR "/display_state_change"
#define GPM_CONF_CAN_SUSPEND		GPM_CONF_DIR "/can_suspend"
#define GPM_CONF_CAN_HIBERNATE		GPM_CONF_DIR "/can_hibernate"
#define GPM_CONF_DISPLAY_AMBIENT	GPM_CONF_DIR "/display_ambient"
#define GPM_CONF_PANEL_DIM_BRIGHTNESS	GPM_CONF_DIR "/laptop_panel_dim_brightness"
#define GPM_CONF_USE_TIME_POLICY	GPM_CONF_DIR "/use_time_for_policy"
#define GPM_CONF_USE_PROFILE_TIME	GPM_CONF_DIR "/use_profile_time"
#define GPM_CONF_NETWORKMANAGER_SLEEP	GPM_CONF_DIR "/network_sleep"
#define GPM_CONF_IDLE_CHECK_CPU		GPM_CONF_DIR "/check_type_cpu"
#define GPM_CONF_INVALID_TIMEOUT	GPM_CONF_DIR "/invalid_timeout"
#define GPM_CONF_LAPTOP_USES_EXT_MON	GPM_CONF_DIR "/using_external_monitor"
#define GPM_CONF_POLICY_TIMEOUT		GPM_CONF_DIR "/policy_suppression_timeout"
#define GPM_CONF_IGNORE_INHIBITS	GPM_CONF_DIR "/ignore_inhibit_requests"

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
