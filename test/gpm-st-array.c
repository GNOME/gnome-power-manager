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

#include "../src/gpm-array.h"
#include "../src/gpm-debug.h"

void
gpm_st_array (GpmSelfTest *test)
{
	GpmArray *array;
	GpmArray *array2;
	gboolean ret;
	guint size;
	guint x, y, data;
	GpmArrayPoint *point;
	gint svalue;
	test->type = "GpmArray         ";
	guint i;

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null array");
	array = gpm_array_new ();
	if (array != NULL) {
		gpm_st_success (test, "got GpmArray");
	} else {
		gpm_st_failed (test, "could not get GpmArray");
	}

	/************** FIXED SIZE TESTS ****************************/
	gpm_st_title (test, "set fixed size of 10");
	ret = gpm_array_set_fixed_size (array, 10);
	if (ret == TRUE) {
		gpm_st_success (test, "set size");
	} else {
		gpm_st_failed (test, "set size failed");
	}

	/************************************************************/
	gpm_st_title (test, "get fixed size");
	size = gpm_array_get_size (array);
	if (size == 10) {
		gpm_st_success (test, "get size passed");
	} else {
		gpm_st_failed (test, "get size failed");
	}

	/************************************************************/
	gpm_st_title (test, "add some data (should fail as fixed size)");
	ret = gpm_array_add (array, 1, 2, 3);
	if (ret == FALSE) {
		gpm_st_success (test, "could not append to fixed size");
	} else {
		gpm_st_failed (test, "appended to fixed size array");
	}

	/************************************************************/
	gpm_st_title (test, "get valid element (should be zero)");
	point = gpm_array_get (array, 0);
	if (point != NULL && point->x == 0 && point->y == 0 && point->data == 0) {
		gpm_st_success (test, "got blank data");
	} else {
		gpm_st_failed (test, "did not get blank data");
	}

	/************************************************************/
	gpm_st_title (test, "get out of range element (should fail)");
	point = gpm_array_get (array, 10);
	if (point == NULL) {
		gpm_st_success (test, "got NULL as OOB");
	} else {
		gpm_st_failed (test, "did not NULL for OOB");
	}

	g_object_unref (array);
	array = gpm_array_new ();

	/************* VARIABLE SIZED TESTS *************************/
	gpm_st_title (test, "add some data (should pass as variable size)");
	ret = gpm_array_add (array, 1, 2, 3);
	if (ret == TRUE) {
		gpm_st_success (test, "appended to variable size");
	} else {
		gpm_st_failed (test, "did not append to variable size array");
	}

	/************************************************************/
	gpm_st_title (test, "get variable size");
	size = gpm_array_get_size (array);
	if (size == 1) {
		gpm_st_success (test, "get size passed");
	} else {
		gpm_st_failed (test, "get size failed");
	}

	/************************************************************/
	gpm_st_title (test, "get out of range element (should fail)");
	point = gpm_array_get (array, 1);
	if (point == NULL) {
		gpm_st_success (test, "got NULL as OOB");
	} else {
		gpm_st_failed (test, "did not NULL for OOB");
	}

	/************************************************************/
	gpm_st_title (test, "clear array");
	ret = gpm_array_clear (array);
	if (ret == TRUE) {
		gpm_st_success (test, "cleared");
	} else {
		gpm_st_failed (test, "did not clear");
	}

	/************************************************************/
	gpm_st_title (test, "get cleared size");
	size = gpm_array_get_size (array);
	if (size == 0) {
		gpm_st_success (test, "get size passed");
	} else {
		gpm_st_failed (test, "get size failed");
	}

	/************************************************************/
	gpm_st_title (test, "save to disk");
	for (i=0;i<100;i++) {
		gpm_array_add (array, i, i, i);
	}
	ret = gpm_array_save_to_file (array, "/tmp/gpm-self-test.txt");
	if (ret == TRUE) {
		gpm_st_success (test, "saved to disk");
	} else {
		gpm_st_failed (test, "could not save to disk");
	}

	/************************************************************/
	gpm_st_title (test, "load from disk");
	gpm_array_clear (array);
	ret = gpm_array_append_from_file (array, "/tmp/gpm-self-test.txt");
	if (ret == TRUE) {
		gpm_st_success (test, "loaded from disk");
	} else {
		gpm_st_failed (test, "could not load from disk");
	}

	/************************************************************/
	gpm_st_title (test, "get file appended size");
	size = gpm_array_get_size (array);
	if (size == 99) {
		gpm_st_success (test, "get size passed");
	} else {
		gpm_st_failed (test, "get size failed: %i", size);
	}

	/************************************************************/
	gpm_st_title (test, "interpolate data");
	gpm_array_clear (array);
	gpm_array_add (array, 1, 2, 0);
	gpm_array_add (array, 3, 9, 0);
	svalue = gpm_array_interpolate (array, 2);
	if (svalue == 6) {
		gpm_st_success (test, "interpolated");
	} else {
		gpm_st_failed (test, "interpolated incorrect: %i", svalue);
	}

	/************************************************************/
	gpm_st_title (test, "limit x size");
	gpm_array_clear (array);
	for (i=0;i<100;i++) {
		gpm_array_add (array, i, i, i);
	}
	gpm_array_limit_x_size (array, 10);
	size = gpm_array_get_size (array);
	if (size == 10) {
		gpm_st_success (test, "limited size X");
	} else {
		gpm_st_failed (test, "did not limit size X, size: %i", size);
	}

	/************************************************************/
	gpm_st_title (test, "limit x width");
	gpm_array_clear (array);
	for (i=0;i<100;i++) {
		gpm_array_add (array, i, i, i);
	}
	gpm_array_limit_x_width (array, 10);
	size = gpm_array_get_size (array);
	if (size == 11) {
		gpm_st_success (test, "limited width X");
	} else {
		gpm_st_failed (test, "did not limit width X, size: %i", size);
	}
	gpm_array_print (array);

	/*************** COPY TEST **********************************/
	gpm_st_title (test, "test copy");
	array2 = gpm_array_new ();
	gpm_array_clear (array);
	gpm_array_set_fixed_size (array, 10);
	gpm_array_set_fixed_size (array2, 10);
	for (i=0;i<10;i++) {
		gpm_array_set (array, i, 2, 2, 2);
	}
	size = gpm_array_get_size (array2);
	gpm_array_copy (array, array2);
	x = gpm_array_get(array2,0)->x;
	y = gpm_array_get(array2,9)->y;
	data = gpm_array_get(array2,5)->data;
	if (size == 10 && x == 2 && y == 2 && gpm_array_get(array2,5)->data == 2) {
		gpm_st_success (test, "limited width X");
	} else {
		gpm_st_failed (test, "did not limit width X, size: %i (%i,%i,%i)", size, x, y, data);
	}

	/*************** INTEGRATION TEST ************************/
	gpm_st_title (test, "integration down");
	gpm_array_clear (array);
	gpm_array_set_fixed_size (array, 10);
	for (i=0;i<10;i++) {
		gpm_array_set (array, i, i, i, 3);
	}
	size = gpm_array_compute_integral (array, 0, 4);
	if (size == 0+1+2+3+4) {
		gpm_st_success (test, "intergrated okay");
	} else {
		gpm_st_failed (test, "did not intergrated okay (%i)", size);
	}
	gpm_st_title (test, "integration up");
	size = gpm_array_compute_integral (array, 5, 9);
	if (size == 5+6+7+8+9) {
		gpm_st_success (test, "intergrated okay");
	} else {
		gpm_st_failed (test, "did not intergrated okay (%i)", size);
	}
	gpm_st_title (test, "integration all");
	size = gpm_array_compute_integral (array, 0, 9);
	if (size == 0+1+2+3+4+5+6+7+8+9) {
		gpm_st_success (test, "intergrated okay");
	} else {
		gpm_st_failed (test, "did not intergrated okay (%i)", size);
	}

	/************************************************************/

	g_object_unref (array);
	g_object_unref (array2);
}

