/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <math.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gpm-array-float.h"

static void
gpm_test_array_float_func (void)
{
	GpmArrayFloat *array;
	GpmArrayFloat *kernel;
	GpmArrayFloat *result;
	gfloat value;
	gfloat sigma;
	guint size;

	/* make sure we get a non null array */
	array = gpm_array_float_new (10);
	g_assert (array != NULL);

	gpm_array_float_print (array);
	gpm_array_float_free (array);

	/* make sure we get the correct length array */
	array = gpm_array_float_new (10);
	g_assert_cmpint (array->len, ==, 10);

	/* make sure we get the correct array sum */
	value = gpm_array_float_sum (array);
	g_assert_cmpfloat (value, ==, 0.0f);

	/* remove outliers */
	gpm_array_float_set (array, 0, 30.0);
	gpm_array_float_set (array, 1, 29.0);
	gpm_array_float_set (array, 2, 31.0);
	gpm_array_float_set (array, 3, 33.0);
	gpm_array_float_set (array, 4, 100.0);
	gpm_array_float_set (array, 5, 27.0);
	gpm_array_float_set (array, 6, 30.0);
	gpm_array_float_set (array, 7, 29.0);
	gpm_array_float_set (array, 8, 31.0);
	gpm_array_float_set (array, 9, 30.0);
	kernel = gpm_array_float_remove_outliers (array, 3, 10.0);
	g_assert (kernel != NULL);
	g_assert_cmpint (kernel->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (kernel);

	/* make sure we removed the outliers */
	value = gpm_array_float_sum (kernel);
	g_assert_cmpfloat (fabs(value - 30*10), <, 1.0f);
	gpm_array_float_free (kernel);

	/* remove outliers step */
	gpm_array_float_set (array, 0, 0.0);
	gpm_array_float_set (array, 1, 0.0);
	gpm_array_float_set (array, 2, 0.0);
	gpm_array_float_set (array, 3, 0.0);
	gpm_array_float_set (array, 4, 0.0);
	gpm_array_float_set (array, 5, 0.0);
	gpm_array_float_set (array, 6, 0.0);
	gpm_array_float_set (array, 7, 10.0);
	gpm_array_float_set (array, 8, 20.0);
	gpm_array_float_set (array, 9, 50.0);
	kernel = gpm_array_float_remove_outliers (array, 3, 20.0);
	g_assert (kernel != NULL);
	g_assert_cmpint (kernel->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (kernel);

	/* make sure we removed the outliers */
	value = gpm_array_float_sum (kernel);
	g_assert_cmpfloat (fabs(value - 80), <, 1.0f);
	gpm_array_float_free (kernel);

	/* get gaussian 0.0, sigma 1.1 */
	value = gpm_array_float_guassian_value (0.0, 1.1);
	g_assert_cmpfloat (fabs (value - 0.36267), <, 0.0001f);

	/* get gaussian 0.5, sigma 1.1 */
	value = gpm_array_float_guassian_value (0.5, 1.1);
	g_assert_cmpfloat (fabs (value - 0.32708), <, 0.0001f);

	/* get gaussian 1.0, sigma 1.1 */
	value = gpm_array_float_guassian_value (1.0, 1.1);
	g_assert_cmpfloat (fabs (value - 0.23991), <, 0.0001f);

	/* get gaussian 0.5, sigma 4.5 */
	value = gpm_array_float_guassian_value (0.5, 4.5);
	g_assert_cmpfloat (fabs (value - 0.088108), <, 0.0001f);

	size = 5;
	sigma = 1.1;
	/* get inprecise gaussian array */
	kernel = gpm_array_float_compute_gaussian (size, sigma);
	g_assert (kernel == NULL);

	size = 9;
	sigma = 1.1;
	/* get gaussian-9 array */
	kernel = gpm_array_float_compute_gaussian (size, sigma);
	g_assert (kernel != NULL);
	g_assert_cmpint (kernel->len, ==, size);
	gpm_array_float_print (kernel);

	/* make sure we get an accurate gaussian */
	value = gpm_array_float_sum (kernel);
	g_assert_cmpfloat (fabs(value - 1.0), <, 0.01f);

	/* make sure we get get and set */
	gpm_array_float_set (array, 4, 100.0);
	value = gpm_array_float_get (array, 4);
	g_assert_cmpfloat (value, ==, 100.0f);
	gpm_array_float_print (array);

	/* make sure we get the correct array sum (2) */
	gpm_array_float_set (array, 0, 20.0);
	gpm_array_float_set (array, 1, 44.0);
	gpm_array_float_set (array, 2, 45.0);
	gpm_array_float_set (array, 3, 89.0);
	gpm_array_float_set (array, 4, 100.0);
	gpm_array_float_set (array, 5, 12.0);
	gpm_array_float_set (array, 6, 76.0);
	gpm_array_float_set (array, 7, 78.0);
	gpm_array_float_set (array, 8, 1.20);
	gpm_array_float_set (array, 9, 3.0);
	value = gpm_array_float_sum (array);
	g_assert_cmpfloat (fabs (value - 468.2), <, 0.0001f);

	/* test convolving with kernel #1 */
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
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #1 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 5.0);
	gpm_array_float_free (result);

	/* test convolving with kernel #2 */
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
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #2 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 10.0f);
	gpm_array_float_free (result);

	/* test convolving with kernel #3 */
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
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #3 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 10.0f);
	gpm_array_float_free (result);

	/* test convolving with kernel #4 */
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
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #4 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 100.0), <, 1.0f);

	/* test convolving with kernel #5 */
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
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);
	gpm_array_float_print (array);
	gpm_array_float_print (result);

	/* make sure we get the correct array sum of convolve #5 */
	value = gpm_array_float_sum (result);
	g_assert_cmpfloat (fabs(value - 90.0), <, 1.0f);

	/* integration down */
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
	g_assert_cmpint (size, ==, 0+1+2+3+4);

	/* integration up */
	size = gpm_array_float_compute_integral (array, 5, 9);
	g_assert_cmpint (size, ==, 5+6+7+8+9);

	/* integration all */
	size = gpm_array_float_compute_integral (array, 0, 9);
	g_assert_cmpint (size, ==, 0+1+2+3+4+5+6+7+8+9);

	/* average */
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
	g_assert_cmpfloat (value, ==, 4.5);

	gpm_array_float_free (result);
	gpm_array_float_free (array);
	gpm_array_float_free (kernel);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* tests go here */
	g_test_add_func ("/power/array_float", gpm_test_array_float_func);

	return g_test_run ();
}

