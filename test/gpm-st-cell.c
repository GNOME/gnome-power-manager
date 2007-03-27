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
#include "gpm-st-main.h"

#include "../src/gpm-cell.h"

guint recall_count = 0;

static void
gpm_cell_perhaps_recall_cb (GpmCell *cell, gchar *oem_vendor, gchar *website, gpointer data)
{
	recall_count++;
}

void
gpm_st_cell (GpmSelfTest *test)
{
	GpmCell *cell;
	GpmCellUnit *unit;
	gchar *desc;
	gboolean ret;
	test->type = "GpmCell          ";

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null cell");
	cell = gpm_cell_new ();
	g_signal_connect (cell, "perhaps-recall",
			  G_CALLBACK (gpm_cell_perhaps_recall_cb), NULL);
	if (cell != NULL) {
		gpm_st_success (test, "got GpmCell");
	} else {
		gpm_st_failed (test, "could not get GpmCell");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null unit");
	unit = gpm_cell_get_unit (cell);
	if (unit != NULL) {
		gpm_st_success (test, "got GpmCellUnit");
	} else {
		gpm_st_failed (test, "could not get GpmCellUnit");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a null description for nonassigned cell");
	desc = gpm_cell_get_description (cell);
	if (desc != NULL) {
		gpm_st_success (test, "got description %s", desc);
	} else {
		gpm_st_failed (test, "could not get description");
	}

	/************************************************************/
	gpm_st_title (test, "can we assign device");
	ret = gpm_cell_set_type (cell, GPM_CELL_UNIT_KIND_PRIMARY, "/org/freedesktop/Hal/devices/acpi_BAT1");
	if (ret == TRUE) {
		gpm_st_success (test, "set type okay");
	} else {
		gpm_st_failed (test, "could not set type");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we got a single recall notice");
	if (recall_count == 1) {
		gpm_st_success (test, "got recall");
	} else {
		gpm_st_failed (test, "did not get recall (install fdi?)");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a full description for set cell");
	desc = gpm_cell_get_description (cell);
	if (desc != NULL) {
		gpm_st_success (test, "got description %s", desc);
	} else {
		gpm_st_failed (test, "could not get description");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a valid unit");
	unit = gpm_cell_get_unit (cell);
	if (unit->voltage == 11100) {
		gpm_st_success (test, "got correct voltage");
	} else {
		gpm_st_failed (test, "could not get correct voltage %i", unit->voltage);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a id");
	desc = gpm_cell_get_id (cell);
	if (desc != NULL) {
		gpm_st_success (test, "got valid id %s", desc);
	} else {
		gpm_st_failed (test, "could not get valid id");
	}

	g_object_unref (cell);
}

