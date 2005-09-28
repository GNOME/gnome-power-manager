/***************************************************************************
 *
 * gpm-common.h : Common functions shared between modules
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
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

#ifndef _COMMON_H
#define _COMMON_H

#include "eggtrayicon.h"
#include <gnome.h>

/* Set the timeout of the libnotify notifications */
#define NOTIFY_TIMEOUT			5

/* Do no actions. Set to true for testing */
#define GPM_SIMULATE			FALSE

/* where our settings are stored in the gconf tree */
#define GCONF_ROOT_SANS_SLASH		"/apps/gnome-power-manager"
#define GCONF_ROOT			GCONF_ROOT_SANS_SLASH "/"

/* common descriptions of this program */
#define NICENAME 			_("GNOME Power Manager")
#define NICEDESC 			_("Power Manager for the GNOME desktop")

/* help location */
#define GPMURL	 			"http://gnome-power.sourceforge.net/"

#define GPM_DBUS_SCREENSAVE		1
#define GPM_DBUS_SHUTDOWN		2
#define GPM_DBUS_SUSPEND		4
#define GPM_DBUS_HIBERNATE		8
#define GPM_DBUS_LOGOFF			16
#define GPM_DBUS_ALL			255

#define	GPM_DBUS_SERVICE		"org.gnome.GnomePowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/GnomePowerManager"
#define	GPM_DBUS_INTERFACE		"org.gnome.GnomePowerManager"

#define	DBUS_NO_SERVICE_ERROR		"org.freedesktop.DBus.Error.ServiceDoesNotExist"

typedef enum {
	ACTION_NOTHING,
	ACTION_WARNING,
	ACTION_SUSPEND,
	ACTION_HIBERNATE,
	ACTION_SHUTDOWN,
	ACTION_UNKNOWN,
	ACTION_REBOOT,
	ACTION_POWER_STATE_CHANGE,
	ACTION_UPS_LOW,
	ACTION_UPS_CHARGE,
	ACTION_UPS_DISCHARGE,
	ACTION_BATTERY_CHARGE,
	ACTION_BATTERY_DISCHARGE,
	ACTION_BATTERY_LOW,
	ACTION_NOW_BATTERYPOWERED,
	ACTION_NOW_MAINSPOWERED,
	ACTION_NOW_HASBATTERIES,
	ACTION_NOW_NOBATTERIES
} ActionType;

typedef enum {
	POWER_NONE,
	POWER_UNKNOWN,
	POWER_AC_ADAPTER,
	POWER_PRIMARY_BATTERY,
	POWER_UPS,
	POWER_MOUSE,
	POWER_KEYBOARD,
	POWER_LCD,
	POWER_PDA
} PowerDevice;

typedef enum {
	BUTTON_POWER,
	BUTTON_SLEEP,
	BUTTON_LID,
	BUTTON_UNKNOWN
} ButtonDevice;

typedef struct {
	int idleTime;
	gboolean onBatteryPower;
	gboolean onUPSPower;
} StateData;

typedef struct {
	gboolean present;
	gint slot;
	gchar udi[128];
	gint powerDevice;
	gint isRechargeable;
	gint percentageCharge;
	gint minutesRemaining;
	gboolean isCharging;
	gboolean isDischarging;
} GenericObject;

void g_log_ignore (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);

GString *convert_gpmdbus_to_string (gint value);
gchar *convert_dbus_enum_to_string (gint value);

gint convert_string_to_policy (const gchar *gconfstring);
gint convert_haltype_to_powerdevice (const gchar *type);
gchar *convert_policy_to_string (gint value);

void update_percentage_charge (GenericObject *slotData);
GString *get_timestring_from_minutes (gint minutes);
gchar *convert_powerdevice_to_string (gint powerDevice);
gchar *get_chargestate_string (GenericObject *slotData);
void create_virtual_of_type (GenericObject *slotDataReturn, gint powerDevice);
GString *get_time_string (GenericObject *slotData);
gboolean run_gconf_script (const char *path);

gint find_udi_parray_index (GPtrArray *parray, const gchar *udi);
GenericObject *genericobject_find (GPtrArray *parray, const gchar *udi);
GenericObject *genericobject_add (GPtrArray *parray, const gchar *udi);


#endif	/* _COMMON_H */
