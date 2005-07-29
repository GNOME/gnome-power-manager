/***************************************************************************
 *
 * gpm-dbus-server.h : DBUS listener
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

#ifndef _GPMDBUSSERVER_H
#define _GPMDBUSSERVER_H

typedef struct {
	GString *dbusName;
	GString *appName;
	GString *reason;
	gint flags;
	gint timeout;
	gboolean isNACK;
	gboolean isACK;
} RegProgram;

DBusHandlerResult dbus_signal_filter (DBusConnection *connection, DBusMessage *message, void *user_data);
DBusMessage *dbus_method_handler (DBusMessage *message, DBusError *error);
DBusHandlerResult dbus_message_handler (DBusConnection *connection, DBusMessage *message, void *user_data);

#endif	/* _GPMDBUSSERVER_H */
