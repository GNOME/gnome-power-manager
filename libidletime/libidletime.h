/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __LIBIDLETIME_H
#define __LIBIDLETIME_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LIBIDLETIME_TYPE		(idletime_get_type ())
#define LIBIDLETIME(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), LIBIDLETIME_TYPE, LibIdletime))
#define LIBIDLETIME_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LIBIDLETIME_TYPE, LibIdletimeClass))
#define LIBIDLETIME_IS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LIBIDLETIME_TYPE))
#define LIBIDLETIME_IS_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), LIBIDLETIME_TYPE))
#define LIBIDLETIME_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LIBIDLETIME_TYPE, LibIdletimeClass))

typedef struct LibIdletimePrivate LibIdletimePrivate;

typedef struct
{
	GObject		 parent;
	LibIdletimePrivate	*priv;
} LibIdletime;

typedef struct
{
	GObjectClass	parent_class;
	void		(* alarm_expired)		(LibIdletime	*idletime,
							 guint		 id);
} LibIdletimeClass;

GType		 idletime_get_type			(void);
LibIdletime	*idletime_new				(void);

void		 idletime_alarm_reset_all		(LibIdletime	*idletime);
guint		 idletime_alarm_get			(LibIdletime	*idletime);
gboolean	 idletime_alarm_set			(LibIdletime	*idletime,
							 guint		 id,
							 guint		 timeout);
gboolean	 idletime_alarm_remove			(LibIdletime	*idletime,
							 guint		 id);

G_END_DECLS

#endif	/* __LIBIDLETIME_H */
