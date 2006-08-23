/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMPROXY_H
#define __GPMPROXY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_PROXY		(gpm_proxy_get_type ())
#define GPM_PROXY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_PROXY, GpmProxy))
#define GPM_PROXY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_PROXY, GpmProxyClass))
#define GPM_IS_PROXY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_PROXY))
#define GPM_IS_PROXY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_PROXY))
#define GPM_PROXY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_PROXY, GpmProxyClass))

typedef struct GpmProxyPrivate GpmProxyPrivate;

typedef struct
{
	GObject		 parent;
	GpmProxyPrivate *priv;
} GpmProxy;

typedef struct
{
	GObjectClass	parent_class;
	void		(* proxy_status)	(GpmProxy	*proxy,
						 gboolean	 status);
} GpmProxyClass;

typedef enum {
        GPM_PROXY_SESSION,
        GPM_PROXY_SYSTEM,
        GPM_PROXY_UNKNOWN
} GpmProxyBusType;

GType		 gpm_proxy_get_type		(void);
GpmProxy	*gpm_proxy_new			(void);

DBusGProxy	*gpm_proxy_assign		(GpmProxy	*gproxy,
						 GpmProxyBusType bus_type,
						 const char	*service,
						 const char	*path,
						 const char	*interface);
DBusGProxy	*gpm_proxy_get_proxy		(GpmProxy	*gproxy);
char		*gpm_proxy_get_service		(GpmProxy	*gproxy);
char		*gpm_proxy_get_interface	(GpmProxy	*gproxy);
char		*gpm_proxy_get_path		(GpmProxy	*gproxy);
GpmProxyBusType	 gpm_proxy_get_bus_type		(GpmProxy	*gproxy);

G_END_DECLS

#endif	/* __GPMPROXY_H */
