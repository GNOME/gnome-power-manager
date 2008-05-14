/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#ifndef __LIBUNIQUE_H
#define __LIBUNIQUE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LIBUNIQUE_TYPE		(libunique_get_type ())
#define LIBUNIQUE_OBJECT(o)	(G_TYPE_CHECK_INSTANCE_CAST ((o), LIBUNIQUE_TYPE, LibUnique))
#define LIBUNIQUE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), LIBUNIQUE_TYPE, LibUniqueClass))
#define IS_LIBUNIQUE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LIBUNIQUE_TYPE))
#define IS_LIBUNIQUE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LIBUNIQUE_TYPE))
#define LIBUNIQUE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LIBUNIQUE_TYPE, LibUniqueClass))

typedef struct LibUniquePrivate LibUniquePrivate;

typedef struct
{
	GObject		parent;
	LibUniquePrivate *priv;
} LibUnique;

typedef struct
{
	GObjectClass	parent_class;
	void		(* activated)		(LibUnique	*unique);
} LibUniqueClass;

GType		 libunique_get_type		(void);
LibUnique	*libunique_new			(void);

gboolean	 libunique_assign		(LibUnique	*libunique,
						 const gchar	*service);

G_END_DECLS

#endif	/* __LIBUNIQUE_H */

