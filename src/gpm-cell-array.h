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

#ifndef __GPMCELL_ARRAY_H
#define __GPMCELL_ARRAY_H

#include <glib-object.h>
#include "gpm-cell.h"
#include "gpm-cell-unit.h"

G_BEGIN_DECLS

#define GPM_TYPE_CELL_ARRAY		(gpm_cell_array_get_type ())
#define GPM_CELL_ARRAY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_CELL_ARRAY, GpmCellArray))
#define GPM_CELL_ARRAY_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_CELL_ARRAY, GpmCellArrayClass))
#define GPM_IS_CELL_ARRAY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_CELL_ARRAY))
#define GPM_IS_CELL_ARRAY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_CELL_ARRAY))
#define GPM_CELL_ARRAY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_CELL_ARRAY, GpmCellArrayClass))

typedef struct GpmCellArrayPrivate GpmCellArrayPrivate;

typedef struct
{
	GObject		 parent;
	GpmCellArrayPrivate *priv;
} GpmCellArray;

typedef struct
{
	GObjectClass	parent_class;
	void		(* percent_changed)	(GpmCellArray	*cell_array,
						 gfloat		 percent);
	void		(* charging_changed)	(GpmCellArray	*cell_array,
						 gboolean	 charging);
	void		(* discharging_changed)	(GpmCellArray	*cell_array,
						 gboolean	 discharging);
	void		(* perhaps_recall)	(GpmCellArray	*cell_array,
						 const gchar	*oem_vendor,
						 const gchar	*website);
	void		(* low_capacity)	(GpmCellArray	*cell_array,
						 guint		 capacity);
	void		(* charge_low)		(GpmCellArray	*cell_array,
						 gfloat		 percent);
	void		(* charge_critical)	(GpmCellArray	*cell_array,
						 gfloat		 percent);
	void		(* charge_action)	(GpmCellArray	*cell_array,
						 gfloat		 percent);
	void		(* fully_charged)	(GpmCellArray	*cell_array);
	void		(* collection_changed)	(GpmCellArray	*cell_array);
	void		(* discharging)		(GpmCellArray	*cell_array);
} GpmCellArrayClass;

GType		 gpm_cell_array_get_type		(void);
GpmCellArray	*gpm_cell_array_new			(void);

gboolean	 gpm_cell_array_set_type		(GpmCellArray	*cell_array,
							 GpmCellUnitKind type);
GpmCellUnit	*gpm_cell_array_get_unit		(GpmCellArray	*cell_array);
gchar		*gpm_cell_array_get_icon		(GpmCellArray	*cell_array);
GpmCellUnitKind	 gpm_cell_array_get_kind		(GpmCellArray	*cell_array);
GpmCell		*gpm_cell_array_get_cell		(GpmCellArray	*cell_array,
							 guint		 id);
guint		 gpm_cell_array_get_time_until_action	(GpmCellArray	*cell_array);
guint		 gpm_cell_array_get_num_cells		(GpmCellArray	*cell_array);
gchar		*gpm_cell_array_get_description		(GpmCellArray	*cell_array);
gboolean	 gpm_cell_array_refresh			(GpmCellArray	*cell_array);

G_END_DECLS

#endif	/* __GPMCELL_ARRAY_H */
