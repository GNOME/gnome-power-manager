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

#ifndef __GPM_ARRAY_H
#define __GPM_ARRAY_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_ARRAY		(gpm_array_get_type ())
#define GPM_ARRAY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_ARRAY, GpmArray))
#define GPM_ARRAY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_ARRAY, GpmArrayClass))
#define GPM_IS_ARRAY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_ARRAY))
#define GPM_IS_ARRAY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_ARRAY))
#define GPM_ARRAY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_ARRAY, GpmArrayClass))

typedef struct GpmArrayPrivate GpmArrayPrivate;

typedef struct
{
	GObject parent;
	GpmArrayPrivate	*priv;
} GpmArray;

typedef struct
{
	GObjectClass	parent_class;
} GpmArrayClass;

typedef struct {
	guint		 x;
	guint		 y;
	guint		 data;
} GpmArrayPoint;

GType			 gpm_array_get_type		(void);
GpmArray		*gpm_array_new			(void);
gboolean		 gpm_array_clear		(GpmArray	*array);
GpmArrayPoint		*gpm_array_get			(GpmArray	*array,
							 guint		 i);
gboolean		 gpm_array_set			(GpmArray	*array,
							 guint		 i,
							 guint		 x,
							 guint		 y,
							 guint		 data);
gboolean		 gpm_array_append		(GpmArray	*array,
							 guint		 x,
							 guint		 y,
							 guint		 data);
gboolean		 gpm_array_append_from_file	(GpmArray	*array,
							 const gchar	*filename);
gboolean		 gpm_array_load_from_file	(GpmArray	*array,
							 const gchar	*filename);
gboolean		 gpm_array_save_to_file		(GpmArray	*array,
							 const gchar	*filename);
gboolean		 gpm_array_set_data		(GpmArray	*array,
							 guint		 data);
gboolean		 gpm_array_set_fixed_size	(GpmArray	*array,
							 guint		 size);
guint			 gpm_array_get_size		(GpmArray	*array);
gint			 gpm_array_interpolate		(GpmArray	*array,
							 gint		 xintersect);
gboolean		 gpm_array_invert		(GpmArray	*array);
gboolean		 gpm_array_print		(GpmArray	*array);
gboolean		 gpm_array_copy			(GpmArray	*from,
							 GpmArray	*to);
gboolean		 gpm_array_copy_insert		(GpmArray	*from,
							 GpmArray	*to);
gboolean		 gpm_array_sort_by_x		(GpmArray	*array);
gboolean		 gpm_array_sort_by_y		(GpmArray	*array);
gboolean		 gpm_array_set_max_points	(GpmArray	*array,
							 guint		 max_points);
gboolean		 gpm_array_set_max_width	(GpmArray	*array,
							 guint		 max_width);
gboolean		 gpm_array_limit_x_size		(GpmArray	*array,
							 guint		 max_num);
gboolean		 gpm_array_limit_x_width	(GpmArray	*array,
							 guint		 max_width);
gboolean		 gpm_array_add			(GpmArray	*array,
							 guint		 x,
							 guint		 y,
							 guint		 data);

G_END_DECLS

#endif /* __GPM_ARRAY_H */
