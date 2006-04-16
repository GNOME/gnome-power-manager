/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_DBUS_SESSION_MONITOR_H
#define __GPM_DBUS_SESSION_MONITOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_DBUS_SESSION_MONITOR		(gpm_dbus_session_monitor_get_type ())
#define GPM_DBUS_SESSION_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_DBUS_SESSION_MONITOR, GpmDbusSessionMonitor))
#define GPM_DBUS_SESSION_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_DBUS_SESSION_MONITOR, GpmDbusSessionMonitorClass))
#define GPM_IS_DBUS_SESSION_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_DBUS_SESSION_MONITOR))
#define GPM_IS_DBUS_SESSION_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_DBUS_SESSION_MONITOR))
#define GPM_DBUS_SESSION_MONITOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_DBUS_SESSION_MONITOR, GpmDbusSessionMonitorClass))

typedef struct GpmDbusSessionMonitorPrivate GpmDbusSessionMonitorPrivate;

typedef struct
{
	GObject			 parent;
	GpmDbusSessionMonitorPrivate	*priv;
} GpmDbusSessionMonitor;

typedef struct
{
	GObjectClass	parent_class;
	void		(* dbus_name_owner_changed)  (GpmDbusSessionMonitor	*monitor,
						      const char	*name,
						      const char	*prev,
						      const char	*new);
} GpmDbusSessionMonitorClass;

GType			 gpm_dbus_session_monitor_get_type	(void);
GpmDbusSessionMonitor	*gpm_dbus_session_monitor_new		(void);
gboolean		 gpm_dbus_session_monitor_get_on_ac	(GpmDbusSessionMonitor *monitor);

G_END_DECLS

#endif /* __GPM_DBUS_SESSION_MONITOR_H */
