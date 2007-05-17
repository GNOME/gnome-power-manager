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

#ifndef __DBUSWATCH_H
#define __DBUSWATCH_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define DBUS_TYPE_WATCH		(dbus_watch_get_type ())
#define DBUS_WATCH(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DBUS_TYPE_WATCH, DbusWatch))
#define DBUS_WATCH_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DBUS_TYPE_WATCH, DbusWatchClass))
#define DBUS_IS_WATCH(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), DBUS_TYPE_WATCH))
#define DBUS_IS_WATCH_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DBUS_TYPE_WATCH))
#define DBUS_WATCH_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DBUS_TYPE_WATCH, DbusWatchClass))

typedef struct DbusWatchPrivate DbusWatchPrivate;

typedef struct
{
	GObject		 parent;
	DbusWatchPrivate *priv;
} DbusWatch;

typedef struct
{
	GObjectClass	parent_class;
	void		(* connection_changed)	(DbusWatch	*watch,
						 gboolean	 connected);
} DbusWatchClass;

typedef enum {
        DBUS_WATCH_SESSION,
        DBUS_WATCH_SYSTEM
} DbusWatchType;

GType		 dbus_watch_get_type		(void);
DbusWatch	*dbus_watch_new			(void);

gboolean	 dbus_watch_assign		(DbusWatch	*dbus_watch,
						 DbusWatchType	 bus_type,
						 const gchar	*service);
gboolean	 dbus_watch_is_connected	(DbusWatch	*dbus_watch);

G_END_DECLS

#endif	/* __DBUSWATCH_H */

