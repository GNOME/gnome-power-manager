/** @file	gpm-common.h
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

#include <gnome.h>

#include "gpm-sysdev.h"
#include "compiler.h"

/* where our settings are stored in the gconf tree */
#define GCONF_ROOT_SANS_SLASH		"/apps/gnome-power-manager"
#define GCONF_ROOT			GCONF_ROOT_SANS_SLASH "/"

/* common descriptions of this program */
#define NICENAME 			_("GNOME Power Manager")
#define NICEDESC 			_("Power Manager for the GNOME desktop")

/* help location */
#define GPMURL	 			"http://gnome.org/projects/gnome-power-manager/"

#define	GPM_DBUS_SERVICE		"org.gnome.GnomePowerManager"
#define	GPM_DBUS_PATH			"/org/gnome/GnomePowerManager"
#define	GPM_DBUS_INTERFACE		"org.gnome.GnomePowerManager"

#define	DBUS_NO_SERVICE_ERROR		"org.freedesktop.DBus.Error.ServiceDoesNotExist"

/** The action type */
typedef enum {
	ACTION_NOTHING,		/**< Do nothing! Yes nothing.		*/
	ACTION_UNKNOWN,		/**< The action is unknown		*/
	ACTION_WARNING,		/**< Use libnotify and send warning	*/
	ACTION_SUSPEND,		/**< Suspend please.			*/
	ACTION_HIBERNATE,	/**< Hibernate please			*/
	ACTION_SHUTDOWN,	/**< Shutdown please			*/
	ACTION_REBOOT,		/**< Reboot please			*/
	ACTION_NOW_BATTERYPOWERED,	/**< We are now battery powered	*/
	ACTION_NOW_MAINSPOWERED	/**< We are now mains powered		*/
} ActionType;

void g_log_ignore (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);

gboolean get_widget_position (GtkWidget *widget, gint *x, gint *y);

ActionType convert_string_to_policy (const gchar *gconfstring);
DeviceType convert_haltype_to_batttype (const gchar *type);
gchar *convert_policy_to_string (gint value);

gchar *get_timestring_from_minutes (gint minutes);
gchar *get_time_string (int minutesRemaining, gboolean isCharging);
gboolean run_gconf_script (const char *path);
gboolean run_bin_program (const gchar *program);

#endif	/* _GPMCOMMON_H */
