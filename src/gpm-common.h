/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GPMCOMMON_H
#define __GPMCOMMON_H

#include <glib.h>

G_BEGIN_DECLS

#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_INTERFACE		"org.gnome.PowerManager"
#define	GPM_DBUS_INTERFACE_BACKLIGHT	"org.gnome.PowerManager.Backlight"
#define	GPM_DBUS_PATH			"/org/gnome/PowerManager"
#define	GPM_DBUS_PATH_BACKLIGHT		"/org/gnome/PowerManager/Backlight"

/* common descriptions of this program */
#define GPM_NAME 			_("Power Manager")
#define GPM_DESCRIPTION 		_("Power Manager for the GNOME desktop")

/* change general/installed_schema whenever adding or moving keys */
#define GPM_SETTINGS_SCHEMA				"org.gnome.power-manager"
#define GSD_SETTINGS_SCHEMA				"org.gnome.settings-daemon.plugins.power"

/* actions */
#define GSD_SETTINGS_ACTION_CRITICAL_BATT		"critical-battery-action"
#define GSD_SETTINGS_ACTION_SLEEP_TYPE_AC		"sleep-inactive-ac-type"
#define GSD_SETTINGS_ACTION_SLEEP_TYPE_BATT		"sleep-inactive-battery-type"
#define GPM_SETTINGS_SLEEP_WHEN_CLOSED			"event-when-closed-battery"

/* backlight stuff */
#define GPM_SETTINGS_BACKLIGHT_ENABLE			"backlight-enable"
#define GPM_SETTINGS_BACKLIGHT_BATTERY_REDUCE		"backlight-battery-reduce"
#define GPM_SETTINGS_DPMS_METHOD_AC			"dpms-method-ac"
#define GPM_SETTINGS_DPMS_METHOD_BATT			"dpms-method-battery"
#define GSD_SETTINGS_IDLE_BRIGHTNESS			"idle-brightness"
#define GSD_SETTINGS_IDLE_DIM_AC			"idle-dim-ac"
#define GSD_SETTINGS_IDLE_DIM_BATT			"idle-dim-battery"
#define GSD_SETTINGS_IDLE_DIM_TIME			"idle-dim-time"
#define GPM_SETTINGS_BRIGHTNESS_AC			"brightness-ac"
#define GPM_SETTINGS_BRIGHTNESS_DIM_BATT		"brightness-dim-battery"

/* buttons */
#define GSD_SETTINGS_BUTTON_LID_AC			"lid-close-ac-action"
#define GSD_SETTINGS_BUTTON_LID_BATT			"lid-close-battery-action"
#define GSD_SETTINGS_BUTTON_SUSPEND			"button-suspend"
#define GSD_SETTINGS_BUTTON_HIBERNATE			"button-hibernate"
#define GSD_SETTINGS_BUTTON_POWER			"button-power"

/* general */
#define GPM_SETTINGS_USE_TIME_POLICY			"use-time-for-policy"

/* lock */
#define GPM_SETTINGS_LOCK_USE_SCREENSAVER		"lock-use-screensaver"
#define GPM_SETTINGS_LOCK_ON_BLANK_SCREEN		"lock-blank-screen"
#define GPM_SETTINGS_LOCK_ON_SUSPEND			"lock-suspend"
#define GPM_SETTINGS_LOCK_ON_HIBERNATE			"lock-hibernate"
#define GPM_SETTINGS_LOCK_KEYRING_SUSPEND		"lock-keyring-suspend"
#define GPM_SETTINGS_LOCK_KEYRING_HIBERNATE		"lock-keyring-hibernate"

/* disks */
#define GPM_SETTINGS_SPINDOWN_ENABLE_AC			"spindown-enable-ac"
#define GPM_SETTINGS_SPINDOWN_ENABLE_BATT		"spindown-enable-battery"
#define GPM_SETTINGS_SPINDOWN_TIMEOUT_AC		"spindown-timeout-ac"
#define GPM_SETTINGS_SPINDOWN_TIMEOUT_BATT		"spindown-timeout-battery"

/* notify */
#define GPM_SETTINGS_NOTIFY_PERHAPS_RECALL		"notify-perhaps-recall"
#define GPM_SETTINGS_NOTIFY_LOW_CAPACITY		"notify-low-capacity"
#define GPM_SETTINGS_NOTIFY_DISCHARGING			"notify-discharging"
#define GPM_SETTINGS_NOTIFY_FULLY_CHARGED		"notify-fully-charged"
#define GPM_SETTINGS_NOTIFY_SLEEP_FAILED		"notify-sleep-failed"
#define GPM_SETTINGS_NOTIFY_SLEEP_FAILED_URI		"notify-sleep-failed-uri"
#define GPM_SETTINGS_NOTIFY_LOW_POWER_SYSTEM		"notify-low-power-system"
#define GPM_SETTINGS_NOTIFY_LOW_POWER_DEVICE		"notify-low-power-device"

/* thresholds */
#define GPM_SETTINGS_PERCENTAGE_LOW			"percentage-low"
#define GPM_SETTINGS_PERCENTAGE_CRITICAL		"percentage-critical"
#define GPM_SETTINGS_PERCENTAGE_ACTION			"percentage-action"
#define GPM_SETTINGS_TIME_LOW				"time-low"
#define GPM_SETTINGS_TIME_CRITICAL			"time-critical"
#define GPM_SETTINGS_TIME_ACTION			"time-action"

/* timeout */
#define GSD_SETTINGS_SLEEP_COMPUTER_AC			"sleep-inactive-ac-timeout"
#define GSD_SETTINGS_SLEEP_COMPUTER_BATT		"sleep-inactive-battery-timeout"
#define GSD_SETTINGS_SLEEP_COMPUTER_AC_EN			"sleep-inactive-ac"
#define GSD_SETTINGS_SLEEP_COMPUTER_BATT_EN		"sleep-inactive-battery"
#define GSD_SETTINGS_SLEEP_DISPLAY_AC			"sleep-display-ac"
#define GSD_SETTINGS_SLEEP_DISPLAY_BATT			"sleep-display-battery"

/* ui */
#define GPM_SETTINGS_ICON_POLICY			"icon-policy"
#define GPM_SETTINGS_ENABLE_SOUND			"enable-sound"
#define GPM_SETTINGS_SHOW_ACTIONS			"show-actions"

/* statistics */
#define GPM_SETTINGS_INFO_HISTORY_TIME			"info-history-time"
#define GPM_SETTINGS_INFO_HISTORY_TYPE			"info-history-type"
#define GPM_SETTINGS_INFO_HISTORY_GRAPH_SMOOTH		"info-history-graph-smooth"
#define GPM_SETTINGS_INFO_HISTORY_GRAPH_POINTS		"info-history-graph-points"
#define GPM_SETTINGS_INFO_STATS_TYPE			"info-stats-type"
#define GPM_SETTINGS_INFO_STATS_GRAPH_SMOOTH		"info-stats-graph-smooth"
#define GPM_SETTINGS_INFO_STATS_GRAPH_POINTS		"info-stats-graph-points"
#define GPM_SETTINGS_INFO_PAGE_NUMBER			"info-page-number"
#define GPM_SETTINGS_INFO_LAST_DEVICE			"info-last-device"

/* gnome-screensaver */
#define GS_CONF_DIR					"/apps/gnome-screensaver"
#define GS_CONF_PREF_LOCK_ENABLED			GS_CONF_DIR "/lock_enabled"

typedef enum {
	GPM_ICON_POLICY_PRESENT,
	GPM_ICON_POLICY_CHARGE,
	GPM_ICON_POLICY_LOW,
	GPM_ICON_POLICY_CRITICAL,
	GPM_ICON_POLICY_NEVER
} GpmIconPolicy;

typedef enum {
	GPM_ACTION_POLICY_BLANK,
	GPM_ACTION_POLICY_SUSPEND,
	GPM_ACTION_POLICY_SHUTDOWN,
	GPM_ACTION_POLICY_HIBERNATE,
	GPM_ACTION_POLICY_INTERACTIVE,
	GPM_ACTION_POLICY_NOTHING
} GpmActionPolicy;
#define	GPM_COLOR_WHITE			0xffffff
#define	GPM_COLOR_BLACK			0x000000
#define	GPM_COLOR_RED			0xff0000
#define	GPM_COLOR_GREEN			0x00ff00
#define	GPM_COLOR_BLUE			0x0000ff
#define	GPM_COLOR_CYAN			0x00ffff
#define	GPM_COLOR_MAGENTA		0xff00ff
#define	GPM_COLOR_YELLOW		0xffff00
#define	GPM_COLOR_GREY			0xcccccc
#define	GPM_COLOR_DARK_RED		0x600000
#define	GPM_COLOR_DARK_GREEN		0x006000
#define	GPM_COLOR_DARK_BLUE		0x000060
#define	GPM_COLOR_DARK_CYAN		0x006060
#define	GPM_COLOR_DARK_MAGENTA		0x600060
#define	GPM_COLOR_DARK_YELLOW		0x606000
#define	GPM_COLOR_DARK_GREY		0x606060

gchar		*gpm_get_timestring				(guint		 time);
void 		 gpm_help_display				(const gchar	*link_id);
gint		 gpm_precision_round_up				(gfloat		 value,
								gint		 smallest);
gint		 gpm_precision_round_down			(gfloat		 value,
								gint		 smallest);
guint		 gpm_discrete_from_percent			(guint		 percentage,
								guint		 levels);
guint		 gpm_discrete_to_percent			(guint		 discrete,
								guint		 levels);
gfloat		 gpm_discrete_to_fraction			(guint		 discrete,
								guint		 levels);
guint32		 gpm_color_from_rgb				(guint8		 red,
								guint8		 green,
								guint8		 blue);
void		 gpm_color_to_rgb				(guint32	 color,
								guint8		*red,
								guint8		*green,
								guint8		*blue);

G_END_DECLS

#endif	/* __GPMCOMMON_H */
