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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __LIBDBUS_MONITOR_SYSTEM_H
#define __LIBDBUS_MONITOR_SYSTEM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define DBUS_TYPE_MONITOR_SYSTEM		(dbus_monitor_system_get_type ())
#define DBUS_MONITOR_SYSTEM(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DBUS_TYPE_MONITOR_SYSTEM, DbusMonitorSystem))
#define DBUS_MONITOR_SYSTEM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DBUS_TYPE_MONITOR_SYSTEM, DbusMonitorSystemClass))
#define DBUS_IS_MONITOR_SYSTEM(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DBUS_TYPE_MONITOR_SYSTEM))
#define DBUS_IS_MONITOR_SYSTEM_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), DBUS_TYPE_MONITOR_SYSTEM))
#define DBUS_MONITOR_SYSTEM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DBUS_TYPE_MONITOR_SYSTEM, DbusMonitorSystemClass))

typedef struct DbusMonitorSystemPrivate DbusMonitorSystemPrivate;

typedef struct
{
	GObject				 parent;
	DbusMonitorSystemPrivate	*priv;
} DbusMonitorSystem;

typedef struct
{
	GObjectClass	parent_class;
	void		(* name_owner_changed) (DbusMonitorSystem	*monitor,
					       const gchar	*name,
					       const gchar	*prev,
					       const gchar	*new);
} DbusMonitorSystemClass;

GType			 dbus_monitor_system_get_type		(void);
DbusMonitorSystem	*dbus_monitor_system_new		(void);

G_END_DECLS

#endif /* __LIBDBUS_MONITOR_SYSTEM_H */

