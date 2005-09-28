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

gboolean dbus_glib_error (GError *error);
gboolean dbus_get_system_connection (DBusGConnection **connection);
gboolean dbus_get_session_connection (DBusGConnection **connection);
gboolean dbus_get_service (DBusGConnection *connection, const gchar *service);

#endif	/* _DBUSCOMMON_H */
