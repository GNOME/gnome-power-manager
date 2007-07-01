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

#include "../src/gpm-cell-array.h"

guint recall_count2 = 0;

static void
gpm_cell_array_perhaps_recall_cb (GpmCellArray *cell_array, gchar *oem_vendor, gchar *website, gpointer data)
{
	recall_count2++;
}

void
gpm_st_cell_array (GpmSelfTest *test)
{
	GpmCell *cell;
	GpmCellArray *cell_array;
//	GpmCellUnit *unit;
	gchar *desc;
	gboolean ret;
	guint count;

	if (gpm_st_start (test, "GpmCellArray", CLASS_AUTO) == FALSE) {
		return;
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null cell array");
	cell_array = gpm_cell_array_new ();
	g_signal_connect (cell_array, "perhaps-recall",
			  G_CALLBACK (gpm_cell_array_perhaps_recall_cb), NULL);
	if (cell_array != NULL) {
		gpm_st_success (test, "got GpmCellArray");
	} else {
		gpm_st_failed (test, "could not get GpmCellArray");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get set type");
	ret = gpm_cell_array_set_type (cell_array, GPM_CELL_UNIT_KIND_PRIMARY);
	if (ret == TRUE) {
		gpm_st_success (test, "set type");
	} else {
		gpm_st_failed (test, "could not set type");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we got a single recall notice");
	if (recall_count2 == 1) {
		gpm_st_success (test, "got recall");
	} else {
		gpm_st_failed (test, "did not get recall (install fdi?)");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we got 1 cell");
	count = gpm_cell_array_get_num_cells (cell_array);
	if (count == 1) {
		gpm_st_success (test, "got 1 cell");
	} else {
		gpm_st_failed (test, "got %i cells", count);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a full description for cell array");
	desc = gpm_cell_array_get_description (cell_array);
	if (desc != NULL) {
		gpm_st_success (test, "got description %s", desc);
	} else {
		gpm_st_failed (test, "could not get description");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a valid cell");
	cell = gpm_cell_array_get_cell (cell_array, 0);
	if (cell != NULL) {
		gpm_st_success (test, "got correct cell");
	} else {
		gpm_st_failed (test, "could not get correct cell");
	}

	g_object_unref (cell_array);

	gpm_st_end (test);
}

