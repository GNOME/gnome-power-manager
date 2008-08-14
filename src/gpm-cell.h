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

#ifndef __GPMCELL_H
#define __GPMCELL_H

#include <glib-object.h>
#include "gpm-cell-unit.h"

G_BEGIN_DECLS

#define GPM_TYPE_CELL		(gpm_cell_get_type ())
#define GPM_CELL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_CELL, GpmCell))
#define GPM_CELL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_CELL, GpmCellClass))
#define GPM_IS_CELL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_CELL))
#define GPM_IS_CELL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_CELL))
#define GPM_CELL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_CELL, GpmCellClass))

typedef struct GpmCellPrivate GpmCellPrivate;

typedef struct
{
	GObject		 parent;
	GpmCellPrivate *priv;
} GpmCell;

typedef struct
{
	GObjectClass	parent_class;
	void		(* percent_changed)	(GpmCell	*cell,
						 gfloat		 percent);
	void		(* charging_changed)	(GpmCell	*cell,
						 gboolean	 charging);
	void		(* discharging_changed)	(GpmCell	*cell,
						 gboolean	 discharging);
	void		(* perhaps_recall)	(GpmCell	*cell,
						 const gchar	*oem_vendor,
						 const gchar	*website);
	void		(* low_capacity)	(GpmCell	*cell,
						 guint		 capacity);
} GpmCellClass;

GType		 gpm_cell_get_type			(void);
GpmCell		*gpm_cell_new				(void);

GpmCellUnit	*gpm_cell_get_unit			(GpmCell	*cell);
gboolean	 gpm_cell_set_type			(GpmCell	*cell,
							 GpmCellUnitKind kind);
gboolean	 gpm_cell_set_device_id			(GpmCell	*cell,
							 const gchar	*udi);
gboolean	 gpm_cell_set_phone_index		(GpmCell	*cell,
							 guint		 index);
const gchar	*gpm_cell_get_device_id			(GpmCell	*cell);
gchar		*gpm_cell_get_icon			(GpmCell	*cell);
gchar		*gpm_cell_get_id			(GpmCell	*cell);
gboolean	 gpm_cell_print				(GpmCell	*cell);
gchar		*gpm_cell_get_description		(GpmCell	*cell);

G_END_DECLS

#endif	/* __GPMCELL_H */

