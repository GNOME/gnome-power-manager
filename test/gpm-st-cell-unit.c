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

#include <glib.h>
#include <string.h>
#include "gpm-st-main.h"

#include "../src/gpm-cell-unit.h"

void
gpm_st_cell_unit (GpmSelfTest *test)
{
	GpmCellUnit unit_d;
	GpmCellUnit *unit = &unit_d;
	gchar *value;
	gboolean ret;
	test->type = "GpmCellUnit      ";

	gpm_cell_unit_init (unit);

	/************************************************************/
	gpm_st_title (test, "make sure full battery isn't charged");
	unit->percentage = 100;
	unit->kind = GPM_CELL_UNIT_KIND_PRIMARY;
	unit->is_charging = FALSE;
	unit->is_discharging = TRUE;
	ret = gpm_cell_unit_is_charged (unit);
	if (ret == FALSE) {
		gpm_st_success (test, "not charged");
	} else {
		gpm_st_failed (test, "declaring charged");
	}

	/************************************************************/
	gpm_st_title (test, "make sure charging battery isn't charged");
	unit->percentage = 99;
	unit->is_charging = TRUE;
	unit->is_discharging = FALSE;
	ret = gpm_cell_unit_is_charged (unit);
	if (ret == FALSE) {
		gpm_st_success (test, "not charged");
	} else {
		gpm_st_failed (test, "declaring charged");
	}

	/************************************************************/
	gpm_st_title (test, "make sure full battery is charged");
	unit->percentage = 95;
	unit->is_charging = FALSE;
	unit->is_discharging = FALSE;
	ret = gpm_cell_unit_is_charged (unit);
	if (ret == TRUE) {
		gpm_st_success (test, "charged");
	} else {
		gpm_st_failed (test, "declaring non-charged");
	}

	/************************************************************/
	gpm_st_title (test, "make sure broken battery isn't charged");
	unit->percentage = 30;
	unit->is_charging = FALSE;
	unit->is_discharging = FALSE;
	ret = gpm_cell_unit_is_charged (unit);
	if (ret == FALSE) {
		gpm_st_success (test, "not charged");
	} else {
		gpm_st_failed (test, "declaring charged");
	}

	/************************************************************/
	gpm_st_title (test, "get missing icon");
	unit->percentage = 30;
	unit->is_present = FALSE;
	value = gpm_cell_unit_get_icon (unit);
	if (strcmp (value, "gpm-primary-missing") == 0) {
		gpm_st_success (test, "icon correct");
	} else {
		gpm_st_failed (test, "icon not correct: %s", value);
	}
	g_free (value);

	/************************************************************/
	gpm_st_title (test, "get middle icon");
	unit->percentage = 30;
	unit->is_present = TRUE;
	value = gpm_cell_unit_get_icon (unit);
	if (strcmp (value, "gpm-primary-040") == 0) {
		gpm_st_success (test, "icon correct");
	} else {
		gpm_st_failed (test, "icon not correct: %s", value);
	}
	g_free (value);

	/************************************************************/
	gpm_st_title (test, "get charged icon");
	unit->is_charging = FALSE;
	unit->is_discharging = FALSE;
	unit->percentage = 95;
	unit->is_present = TRUE;
	value = gpm_cell_unit_get_icon (unit);
	if (strcmp (value, "gpm-primary-charged") == 0) {
		gpm_st_success (test, "icon correct");
	} else {
		gpm_st_failed (test, "icon not correct: %s", value);
	}
	g_free (value);
	
	/************************************************************/
	gpm_st_title (test, "setting measure");
	gpm_cell_unit_set_measure (unit);
	if (unit->measure == GPM_CELL_UNIT_MWH) {
		gpm_st_success (test, "measure correct");
	} else {
		gpm_st_failed (test, "measre not correct: %s", unit->measure);
	}
}

