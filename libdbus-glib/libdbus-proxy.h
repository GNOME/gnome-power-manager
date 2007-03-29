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

#ifndef __DBUSPROXY_H
#define __DBUSPROXY_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define DBUS_TYPE_PROXY		(dbus_proxy_get_type ())
#define DBUS_PROXY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DBUS_TYPE_PROXY, DbusProxy))
#define DBUS_PROXY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DBUS_TYPE_PROXY, DbusProxyClass))
#define DBUS_IS_PROXY(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), DBUS_TYPE_PROXY))
#define DBUS_IS_PROXY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DBUS_TYPE_PROXY))
#define DBUS_PROXY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DBUS_TYPE_PROXY, DbusProxyClass))

typedef struct DbusProxyPrivate DbusProxyPrivate;

typedef struct
{
	GObject		 parent;
	DbusProxyPrivate *priv;
} DbusProxy;

typedef struct
{
	GObjectClass	parent_class;
	void		(* proxy_status)	(DbusProxy	*proxy,
						 gboolean	 status);
} DbusProxyClass;

typedef enum {
        DBUS_PROXY_SESSION,
        DBUS_PROXY_SYSTEM,
        DBUS_PROXY_UNKNOWN
} DbusProxyType;

GType		 dbus_proxy_get_type		(void);
DbusProxy	*dbus_proxy_new			(void);

DBusGProxy	*dbus_proxy_assign		(DbusProxy	*dbus_proxy,
						 DbusProxyType	 bus_type,
						 const gchar	*service,
						 const gchar	*path,
						 const gchar	*interface);
DBusGProxy	*dbus_proxy_get_proxy		(DbusProxy	*dbus_proxy);
gchar		*dbus_proxy_get_service		(DbusProxy	*dbus_proxy);
gchar		*dbus_proxy_get_interface	(DbusProxy	*dbus_proxy);
gchar		*dbus_proxy_get_path		(DbusProxy	*dbus_proxy);
DbusProxyType	 dbus_proxy_get_bus_type	(DbusProxy	*dbus_proxy);
gboolean	 dbus_proxy_is_connected	(DbusProxy	*dbus_proxy);

G_END_DECLS

#endif	/* __DBUSPROXY_H */

