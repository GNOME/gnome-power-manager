/*! @file	gpm-common.h
 *  @brief	Common functions shared between modules
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 */
/*
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

#ifndef _GPMCOMMON_H
#define _GPMCOMMON_H

#include "eggtrayicon.h"
#include <gnome.h>

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

/** The action type */
typedef enum {
	ACTION_NOTHING,
	ACTION_WARNING,
	ACTION_SUSPEND,
	ACTION_HIBERNATE,
	ACTION_SHUTDOWN,
	ACTION_UNKNOWN,
	ACTION_REBOOT,
	ACTION_NOW_BATTERYPOWERED,
	ACTION_NOW_MAINSPOWERED
} ActionType;

/** The device type of the cached object */
typedef enum {
	POWER_NONE,		/**< The blank device			*/
	POWER_UNKNOWN,		/**< An unknown device			*/
	POWER_AC_ADAPTER,	/**< An AC Adapter			*/
	POWER_PRIMARY_BATTERY,	/**< A laptop battery			*/
	POWER_UPS,		/**< An Uninterruptible Power Supply	*/
	POWER_MOUSE,		/**< A wireless, battery mouse		*/
	POWER_KEYBOARD,		/**< A wireless, battery keyboard	*/
	POWER_PDA		/**< A Personal Digital Assistant	*/
} PowerDevice;

/** The state object used to cache the state of the computer */
typedef struct {
	gboolean onBatteryPower;/**< Are we on battery power?		*/
	gboolean onUPSPower;	/**< Are we on UPS power?		*/
} StateData;

/** The generic object used to cache the hal objects locally.
 *
 * This is used to minimise the number of lookups to make sure
 * that the DBUS traffic is kept to a minimum.
 */
typedef struct {
	gboolean present;	/**< If the device is present		*/
	gchar udi[128];		/**< The HAL UDI			*/
	gint powerDevice;	/**< The device type from PowerDevice	*/
	gint isRechargeable;	/**< If device is rechargeable		*/
	gint percentageCharge;	/**< The percentage charge remaining	*/
	gint minutesRemaining;	/**< Minutes remaining until charged	*/
	gboolean isCharging;	/**< If device is charging		*/
	gboolean isDischarging;	/**< If device is discharging		*/
} GenericObject;

void g_log_ignore (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);

gboolean get_widget_position (GtkWidget *widget, gint *x, gint *y);
GString *convert_gpmdbus_to_string (gint value);
gchar *convert_dbus_enum_to_string (gint value);

gint convert_string_to_policy (const gchar *gconfstring);
gint convert_haltype_to_powerdevice (const gchar *type);
gchar *convert_policy_to_string (gint value);

GString *get_timestring_from_minutes (gint minutes);
gchar *convert_powerdevice_to_string (gint powerDevice);
gchar *get_chargestate_string (GenericObject *slotData);
void create_virtual_of_type (GPtrArray *objectData, GenericObject *slotDataReturn, gint powerDevice);
GString *get_time_string (GenericObject *slotData);
gboolean run_gconf_script (const char *path);
gboolean run_bin_program (const gchar *program);

gint find_udi_parray_index (GPtrArray *parray, const gchar *udi);
GenericObject *genericobject_find (GPtrArray *parray, const gchar *udi);
GenericObject *genericobject_add (GPtrArray *parray, const gchar *udi);

#endif	/* _GPMCOMMON_H */
