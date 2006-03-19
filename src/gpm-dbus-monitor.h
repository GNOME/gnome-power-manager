/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GPM_DBUS_MONITOR_H
#define __GPM_DBUS_MONITOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_DBUS_MONITOR		(gpm_dbus_monitor_get_type ())
#define GPM_DBUS_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_DBUS_MONITOR, GpmDbusMonitor))
#define GPM_DBUS_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_DBUS_MONITOR, GpmDbusMonitorClass))
#define GPM_IS_DBUS_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_DBUS_MONITOR))
#define GPM_IS_DBUS_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_DBUS_MONITOR))
#define GPM_DBUS_MONITOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_DBUS_MONITOR, GpmDbusMonitorClass))

typedef struct GpmDbusMonitorPrivate GpmDbusMonitorPrivate;

typedef struct
{
	GObject			 parent;
	GpmDbusMonitorPrivate	*priv;
} GpmDbusMonitor;

typedef struct
{
	GObjectClass	parent_class;
	void		(* dbus_name_owner_changed_session)  (GpmDbusMonitor	*monitor,
							      const char	*name,
							      const char	*prev,
							      const char	*new);
	void		(* dbus_name_owner_changed_system)  (GpmDbusMonitor	*monitor,
							     const char		*name,
							     const char		*prev,
							     const char		*new);
} GpmDbusMonitorClass;

GType			 gpm_dbus_monitor_get_type	(void);

GpmDbusMonitor		*gpm_dbus_monitor_new		(void);

gboolean		 gpm_dbus_monitor_get_on_ac	(GpmDbusMonitor *monitor);

G_END_DECLS

#endif /* __GPM_DBUS_MONITOR_H */
