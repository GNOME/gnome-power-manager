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

#ifndef __GPMCELLUNIT_H
#define __GPMCELLUNIT_H

#define GPM_CELL_UNIT_MIN_CHARGED_PERCENTAGE	60

typedef enum {
	GPM_CELL_UNIT_KIND_PRIMARY,
	GPM_CELL_UNIT_KIND_UPS,
	GPM_CELL_UNIT_KIND_MOUSE,
	GPM_CELL_UNIT_KIND_KEYBOARD,
	GPM_CELL_UNIT_KIND_PHONE,
	GPM_CELL_UNIT_KIND_PDA,
	GPM_CELL_UNIT_KIND_UNKNOWN
} GpmCellUnitKind;

typedef enum {
	GPM_CELL_UNIT_MWH,
	GPM_CELL_UNIT_CSR,
	GPM_CELL_UNIT_PERCENT,
	GPM_CELL_UNIT_UNKNOWN
} GpmCellUnitMeasure;

typedef struct {
	GpmCellUnitKind	 kind;
	GpmCellUnitMeasure measure;
	guint		 charge_design;
	guint		 charge_last_full;
	guint		 charge_current;
	guint		 reporting_design;
	guint		 rate;
	gfloat		 percentage;
	guint		 time_charge;
	guint		 time_discharge;
	guint		 capacity;
	guint		 voltage;
	gboolean	 is_rechargeable;
	gboolean	 is_present;
	gboolean	 is_charging;
	gboolean	 is_discharging;
	gboolean         reports_percentage;
} GpmCellUnit;

gboolean	 gpm_cell_unit_init		(GpmCellUnit	*unit);
gboolean	 gpm_cell_unit_print		(GpmCellUnit	*unit);
gchar		*gpm_cell_unit_get_icon		(GpmCellUnit	*unit);
gboolean	 gpm_cell_unit_is_charged	(GpmCellUnit	*unit);
gboolean	 gpm_cell_unit_set_measure	(GpmCellUnit	*unit);
gboolean	 gpm_cell_unit_set_kind		(GpmCellUnit	*unit,
						 const gchar	*kind);
const gchar	*gpm_cell_unit_get_kind_string	(GpmCellUnit	*unit);
const gchar	*gpm_cell_unit_get_kind_localised (GpmCellUnit	*unit,
						 gboolean	 plural);

#endif	/* __GPMCELLUNIT_H */

