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

#ifndef __GPM_INFO_DATA_H
#define __GPM_INFO_DATA_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_INFO_DATA		(gpm_info_data_get_type ())
#define GPM_INFO_DATA(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_INFO_DATA, GpmInfoData))
#define GPM_INFO_DATA_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_INFO_DATA, GpmInfoDataClass))
#define GPM_IS_INFO_DATA(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_INFO_DATA))
#define GPM_IS_INFO_DATA_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_INFO_DATA))
#define GPM_INFO_DATA_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_INFO_DATA, GpmInfoDataClass))

typedef struct GpmInfoDataPrivate GpmInfoDataPrivate;

typedef struct
{
	GObject			 parent;
	GpmInfoDataPrivate	*priv;
} GpmInfoData;

typedef struct
{
	GObjectClass	parent_class;
} GpmInfoDataClass;

typedef struct {
	guint		 time;	/* seconds */
	guint		 value;
	guint8		 colour;
	gchar		*desc; /* description, or NULL if missing */
} GpmInfoDataPoint;

GType			 gpm_info_data_get_type		(void);
GpmInfoData		*gpm_info_data_new		(void);

GList			*gpm_info_data_get_list		(GpmInfoData	*info_data);
gboolean		 gpm_info_data_add		(GpmInfoData	*info_data,
							 guint		 time,
							 guint		 value,
							 guint8		 colour);
gboolean		 gpm_info_data_add_always	(GpmInfoData	*info_data,
							 guint		 time,
							 guint		 value,
							 guint8		 colour,
							 const gchar	*desc);
gboolean		 gpm_info_data_limit_time	(GpmInfoData	*info_data,
							 guint		 max_num);
gboolean		 gpm_info_data_limit_dilute	(GpmInfoData	*info_data,
							 guint		 max_num);
gboolean		 gpm_info_data_limit_truncate	(GpmInfoData	*info_data,
							 guint		 max_num);
gboolean		 gpm_info_data_set_max_points	(GpmInfoData	*info_data,
							 guint		 max_points);
gboolean		 gpm_info_data_set_max_time	(GpmInfoData	*info_data,
							 guint		 max_time);
gboolean		 gpm_info_data_clear		(GpmInfoData	*info_data);

G_END_DECLS

#endif /* __GPM_INFO_DATA_H */
