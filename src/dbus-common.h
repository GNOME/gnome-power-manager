/***************************************************************************
 *
 * dbus-common.h : Common GLIB DBUS routines
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 **************************************************************************/

#ifndef _DBUSCOMMON_H
#define _DBUSCOMMON_H

#include <dbus/dbus-glib.h>

#if !defined (G_GNUC_WARNUNCHECKED)
#if    __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define G_GNUC_WARNUNCHECKED 		__attribute__((warn_unused_result))
#else
#define G_GNUC_WARNUNCHECKED
#endif /* __GNUC__ */
#endif

gboolean dbus_glib_error (GError *error);
gboolean dbus_get_system_connection (DBusGConnection **connection) G_GNUC_WARNUNCHECKED;
gboolean dbus_get_session_connection (DBusGConnection **connection) G_GNUC_WARNUNCHECKED;
gboolean dbus_get_service (DBusGConnection *connection, const gchar *service) G_GNUC_WARNUNCHECKED;

#endif	/* _DBUSCOMMON_H */
