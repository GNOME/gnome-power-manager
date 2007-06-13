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
#include <math.h>
#include "gpm-st-main.h"

#include "../src/gpm-array-float.h"
#include "../src/gpm-debug.h"

void
gpm_st_array_float (GpmSelfTest *test)
{
	test->type = "GpmArrayFloat    ";
	GArray *array;
	GArray *kernel;
	GArray *result;
	gfloat value;
	gfloat sigma;
	guint size;

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null array");
	array = gpm_array_float_new (10);
	if (array != NULL) {
		gpm_st_success (test, "got GArray");
	} else {
		gpm_st_failed (test, "could not get GArray");
	}
	gpm_array_float_print (array);

	/************************************************************/
	gpm_st_title (test, "make sure we get the correct length array");
	array = gpm_array_float_new (10);
	if (array->len == 10) {
		gpm_st_success (test, "got correct size");
	} else {
		gpm_st_failed (test, "got wrong size");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get the correct array sum");
	value = gpm_array_float_sum (array);
	if (value == 0.0) {
		gpm_st_success (test, "got correct sum");
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}

	/************************************************************/
	gpm_st_title (test, "get gaussian 0.0, sigma 1.0");
	value = gpm_array_float_guassian_value (0.0, 1.0);
	if (value - 0.398942 < 0.0001) {
		gpm_st_success (test, "got correct gaussian");
	} else {
		gpm_st_failed (test, "got wrong gaussian (%f)", value);
	}

	/************************************************************/
	gpm_st_title (test, "get gaussian 1.0, sigma 1.0");
	value = gpm_array_float_guassian_value (1.0, 1.0);
	if (value - 0.241971 < 0.0001) {
		gpm_st_success (test, "got correct gaussian");
	} else {
		gpm_st_failed (test, "got wrong gaussian (%f)", value);
	}

	/************************************************************/
	size = 9;
	sigma = 1.1;
	gpm_st_title (test, "get gaussian array (%i), sigma %f", size, sigma);
	kernel = gpm_array_float_compute_gaussian (size, sigma);
	if (kernel != NULL && kernel->len == size) {
		gpm_st_success (test, "got correct length gaussian array");
	} else {
		gpm_st_failed (test, "got gaussian array length (%i)", array->len);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get an accurate gaussian");
	value = gpm_array_float_sum (kernel);
	if (fabs(value - 1.0) < 0.01) {
		gpm_st_success (test, "got sum (%f)", value);
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}
	gpm_array_float_print (kernel);

	/************************************************************/
	gpm_st_title (test, "make sure we get get and set");
	gpm_array_float_set (array, 4, 100.0);
	value = gpm_array_float_get (array, 4);
	if (value == 100.0) {
		gpm_st_success (test, "got value okay", value);
	} else {
		gpm_st_failed (test, "got wrong value (%f)", value);
	}
	gpm_array_float_print (array);


	/************************************************************/
	gpm_st_title (test, "make sure we get the correct array sum (2)");
	value = gpm_array_float_sum (array);
	if (value == 100.0) {
		gpm_st_success (test, "got correct sum");
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}

	/************************************************************/
	gpm_st_title (test, "test convolving with kernel #1");
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 100.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 0.0);
	gpm_array_float_set (array, 8, 0.0);
	gpm_array_float_set (array, 9, 0.0);
	result = gpm_array_float_convolve (array, kernel);
	if (result->len == 10) {
		gpm_st_success (test, "got correct size convolve product");
	} else {
		gpm_st_failed (test, "got correct size convolve product (%f)", result->len);
	}
	gpm_array_float_print (result);

	/************************************************************/
	gpm_st_title (test, "make sure we get the correct array sum of convolve #1");
	value = gpm_array_float_sum (result);
	if (fabs(value - 100.0) < 5.0) {
		gpm_st_success (test, "got correct (enough) sum (%f)", value);
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}
	gpm_array_float_free (result);

	/************************************************************/
	gpm_st_title (test, "test convolving with kernel #2");
	gpm_array_float_set (array, 0, 100.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 0.0);
	gpm_array_float_set (array, 8, 0.0);
	gpm_array_float_set (array, 9, 0.0);
	result = gpm_array_float_convolve (array, kernel);
	if (result->len == 10) {
		gpm_st_success (test, "got correct size convolve product");
	} else {
		gpm_st_failed (test, "got correct size convolve product (%f)", result->len);
	}
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/************************************************************/
	gpm_st_title (test, "make sure we get the correct array sum of convolve #2");
	value = gpm_array_float_sum (result);
	if (fabs(value - 100.0) < 10.0) {
		gpm_st_success (test, "got correct (enough) sum (%f)", value);
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}

	gpm_array_float_free (result);

	/************************************************************/
	gpm_st_title (test, "test convolving with kernel #3");
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 0.0);
	gpm_array_float_set (array, 8, 0.0);
	gpm_array_float_set (array, 9, 100.0);
	result = gpm_array_float_convolve (array, kernel);
	if (result->len == 10) {
		gpm_st_success (test, "got correct size convolve product");
	} else {
		gpm_st_failed (test, "got correct size convolve product (%f)", result->len);
	}
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/************************************************************/
	gpm_st_title (test, "make sure we get the correct array sum of convolve #3");
	value = gpm_array_float_sum (result);
	if (fabs(value - 100.0) < 10.0) {
		gpm_st_success (test, "got correct (enough) sum (%f)", value);
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}
	gpm_array_float_free (result);

	/************************************************************/
	gpm_st_title (test, "test convolving with kernel #4");
	gpm_array_float_set (array, 0, 10.0);
	gpm_array_float_set (array, 1, 10.0);
	gpm_array_float_set (array, 2, 10.0);
	gpm_array_float_set (array, 3, 10.0);
	gpm_array_float_set (array, 4, 10.0);
	gpm_array_float_set (array, 5, 10.0);
	gpm_array_float_set (array, 6, 10.0);
	gpm_array_float_set (array, 7, 10.0);
	gpm_array_float_set (array, 8, 10.0);
	gpm_array_float_set (array, 9, 10.0);
	result = gpm_array_float_convolve (array, kernel);
	if (result->len == 10) {
		gpm_st_success (test, "got correct size convolve product");
	} else {
		gpm_st_failed (test, "got incorrect size convolve product (%f)", result->len);
	}
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/************************************************************/
	gpm_st_title (test, "make sure we get the correct array sum of convolve #4");
	value = gpm_array_float_sum (result);
	if (fabs(value - 100.0) < 1.0) {
		gpm_st_success (test, "got correct (enough) sum (%f)", value);
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}

	/************************************************************/
	gpm_st_title (test, "test convolving with kernel #5");
	gpm_array_float_set (array, 0, 10.0);
	gpm_array_float_set (array, 1, 10.0);
	gpm_array_float_set (array, 2, 10.0);
	gpm_array_float_set (array, 3, 10.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 10.0);
	gpm_array_float_set (array, 6, 10.0);
	gpm_array_float_set (array, 7, 10.0);
	gpm_array_float_set (array, 8, 10.0);
	gpm_array_float_set (array, 9, 10.0);
	result = gpm_array_float_convolve (array, kernel);
	if (result->len == 10) {
		gpm_st_success (test, "got correct size convolve product");
	} else {
		gpm_st_failed (test, "got incorrect size convolve product (%f)", result->len);
	}
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/************************************************************/
	gpm_st_title (test, "make sure we get the correct array sum of convolve #5");
	value = gpm_array_float_sum (result);
	if (fabs(value - 90.0) < 1.0) {
		gpm_st_success (test, "got correct (enough) sum (%f)", value);
	} else {
		gpm_st_failed (test, "got wrong sum (%f)", value);
	}

	/*************** INTEGRATION TEST ************************/
	gpm_st_title (test, "integration down");
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 1.0);
	gpm_array_float_set (array, 2, 2.0);
	gpm_array_float_set (array, 3, 3.0);
	gpm_array_float_set (array, 4, 4.0);
	gpm_array_float_set (array, 5, 5.0);
	gpm_array_float_set (array, 6, 6.0);
	gpm_array_float_set (array, 7, 7.0);
	gpm_array_float_set (array, 8, 8.0);
	gpm_array_float_set (array, 9, 9.0);
	size = gpm_array_float_compute_integral (array, 0, 4);
	if (size == 0+1+2+3+4) {
		gpm_st_success (test, "intergrated okay");
	} else {
		gpm_st_failed (test, "did not intergrated okay (%i)", size);
	}
	gpm_st_title (test, "integration up");
	size = gpm_array_float_compute_integral (array, 5, 9);
	if (size == 5+6+7+8+9) {
		gpm_st_success (test, "intergrated okay");
	} else {
		gpm_st_failed (test, "did not intergrated okay (%i)", size);
	}
	gpm_st_title (test, "integration all");
	size = gpm_array_float_compute_integral (array, 0, 9);
	if (size == 0+1+2+3+4+5+6+7+8+9) {
		gpm_st_success (test, "intergrated okay");
	} else {
		gpm_st_failed (test, "did not intergrated okay (%i)", size);
	}

	/*************** AVERAGE TEST ************************/
	gpm_st_title (test, "average");
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 1.0);
	gpm_array_float_set (array, 2, 2.0);
	gpm_array_float_set (array, 3, 3.0);
	gpm_array_float_set (array, 4, 4.0);
	gpm_array_float_set (array, 5, 5.0);
	gpm_array_float_set (array, 6, 6.0);
	gpm_array_float_set (array, 7, 7.0);
	gpm_array_float_set (array, 8, 8.0);
	gpm_array_float_set (array, 9, 9.0);
	value = gpm_array_float_get_average (array);
	if (value == 4.5) {
		gpm_st_success (test, "averaged okay");
	} else {
		gpm_st_failed (test, "did not average okay (%i)", value);
	}

	gpm_array_float_free (result);
	gpm_array_float_free (array);
	gpm_array_float_free (kernel);
}

