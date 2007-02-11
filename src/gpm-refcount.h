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

#ifndef __GPMREFCOUNT_H
#define __GPMREFCOUNT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_REFCOUNT		(gpm_refcount_get_type ())
#define GPM_REFCOUNT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_REFCOUNT, GpmRefcount))
#define GPM_REFCOUNT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_REFCOUNT, GpmRefcountClass))
#define GPM_IS_REFCOUNT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_REFCOUNT))
#define GPM_IS_REFCOUNT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_REFCOUNT))
#define GPM_REFCOUNT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_REFCOUNT, GpmRefcountClass))

typedef struct GpmRefcountPrivate GpmRefcountPrivate;

typedef struct
{
	GObject		 parent;
	GpmRefcountPrivate *priv;
} GpmRefcount;

typedef struct
{
	GObjectClass	parent_class;
	void		(* refcount_zero)	(GpmRefcount	*refcount);
	void		(* refcount_added)	(GpmRefcount	*refcount);
} GpmRefcountClass;

GType		 gpm_refcount_get_type		(void);
GpmRefcount	*gpm_refcount_new		(void);

gboolean	 gpm_refcount_add		(GpmRefcount	*refcount);
gboolean	 gpm_refcount_remove		(GpmRefcount	*refcount);
gboolean	 gpm_refcount_set_timeout	(GpmRefcount	*refcount,
						 guint		 timeout);

G_END_DECLS

#endif	/* __GPMREFCOUNT_H */
