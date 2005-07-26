/***************************************************************************
 *
 * gpm-dbus-common.h : DBUS Common functions
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

#ifndef _GPMDBUSCOMMON_H
#define _GPMDBUSCOMMON_H

#define GPM_DBUS_SCREENSAVE	1
#define GPM_DBUS_POWEROFF	2
#define GPM_DBUS_SUSPEND	4
#define GPM_DBUS_HIBERNATE	8
#define GPM_DBUS_LOGOFF		16
#define GPM_DBUS_ALL		255

#define	GPM_DBUS_SERVICE			"net.sf.GnomePower"
#define	GPM_DBUS_PATH				"/net/sf/GnomePower"
#define	GPM_DBUS_INTERFACE			"net.sf.GnomePower"
#define	GPM_DBUS_INTERFACE_SIGNAL	"net.sf.GnomePower.Signal"
#define	GPM_DBUS_INTERFACE_ERROR	"net.sf.GnomePower.Error"

#define	PM_DBUS_SERVICE				"net.sf.PowerManager"
#define	PM_DBUS_PATH				"/PMObject"
#define	PM_DBUS_INTERFACE			"net.sf.PowerManagerInterface"
#define	PM_DBUS_INTERFACE_SIGNAL	"net.sf.PowerManager.Signal"
#define	PM_DBUS_INTERFACE_ERROR		"net.sf.PowerManager.Error"

#define	DBUS_NO_SERVICE_ERROR		"org.freedesktop.DBus.Error.ServiceDoesNotExist"

#if IGNORENONGLIB
/* remove when all g-p-m is glib only */
#else
gboolean dbus_send_signal (DBusConnection *connection, const char *action);
gboolean dbus_send_signal_string (DBusConnection *connection, const char *action, const char *messagetext);
gboolean dbus_send_signal_bool (DBusConnection *connection, const char *action, gboolean value);
gboolean dbus_send_signal_int (DBusConnection *connection, const char *action, gint value);
gboolean dbus_send_signal_int_string (DBusConnection *connection, const char *action, const gint value, const char *messagetext);

gboolean send_method_string (DBusConnection *connection, const char *action);
gboolean get_bool_value (DBusConnection *connection, const char *action, gboolean *data_bool);
#endif

GString *convert_gpmdbus_to_string (gint value);
gchar *convert_dbus_enum_to_string (gint value);

#endif	/* _GPMDBUSCOMMON_H */
