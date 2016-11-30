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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "gpm-array-float.h"

/**
 * gpm_array_float_guassian_value:
 *
 * @x: input value
 * @sigma: sigma value
 * Return value: the gaussian, in floating point precision
 **/
gfloat
gpm_array_float_guassian_value (gfloat x, gfloat sigma)
{
	return (1.0 / (sqrtf(2.0*3.1415927) * sigma)) * (expf((-(powf(x,2.0)))/(2.0 * powf(sigma, 2.0))));
}

/**
 * gpm_array_float_new:
 *
 * @length: length of array
 * Return value: Allocate array
 *
 * Creates a new size array which is zeroed. Free with g_array_free();
 **/
GpmArrayFloat *
gpm_array_float_new (guint length)
{
	guint i;
	GpmArrayFloat *array;
	array = g_array_sized_new (TRUE, FALSE, sizeof(gfloat), length);
	array->len = length;

	/* clear to 0.0 */
	for (i = 0; i < length; i++)
		g_array_index (array, gfloat, i) = 0.0;
	return array;
}

gfloat
gpm_array_float_get (GpmArrayFloat *array, guint i)
{
	if (i >= array->len)
		g_error ("above index! (%u)", i);
	return g_array_index (array, gfloat, i);
}

void
gpm_array_float_set (GpmArrayFloat *array, guint i, gfloat value)
{
	g_array_index (array, gfloat, i) = value;
}

/**
 * gpm_array_float_free:
 *
 * @array: input array
 *
 * Frees the array, deallocating data
 **/
void
gpm_array_float_free (GpmArrayFloat *array)
{
	if (array != NULL)
		g_array_free (array, TRUE);
}

/**
 * gpm_array_float_get_average:
 * @array: This class instance
 *
 * Gets the average value.
 **/
gfloat
gpm_array_float_get_average (GpmArrayFloat *array)
{
	guint i;
	guint length;
	gfloat average = 0;

	length = array->len;
	for (i = 0; i < length; i++)
		average += g_array_index (array, gfloat, i);
	return average / (gfloat) length;
}

/**
 * gpm_array_float_compute_gaussian:
 *
 * @length: length of output array
 * @sigma: sigma value
 * Return value: Gaussian array
 *
 * Create a set of Gaussian array of a specified size
 **/
GpmArrayFloat *
gpm_array_float_compute_gaussian (guint length, gfloat sigma)
{
	GpmArrayFloat *array;
	guint half_length;
	guint i;
	gfloat division;
	gfloat value;

	g_return_val_if_fail (length % 2 == 1, NULL);

	array = gpm_array_float_new (length);

	/* array positions 0..length, has to be an odd number */
	half_length = (length / 2) + 1;
	for (i = 0; i < half_length; i++) {
		division = half_length - (i + 1);
		g_debug ("half_length=%u, div=%f, sigma=%f", half_length, division, sigma);
		g_array_index (array, gfloat, i) = gpm_array_float_guassian_value (division, sigma);
	}

	/* no point working these out, we can just reflect the gaussian */
	for (i=half_length; i<length; i++) {
		division = g_array_index (array, gfloat, length-(i+1));
		g_array_index (array, gfloat, i) = division;
	}

	/* make sure we get an accurate gaussian */
	value = gpm_array_float_sum (array);
	if (fabs (value - 1.0f) > 0.01f) {
		g_debug ("got wrong sum (%f), perhaps sigma too high for size?", value);
		gpm_array_float_free (array);
		array = NULL;
	}

	return array;
}

/**
 * gpm_array_float_sum:
 *
 * @array: input array
 *
 * Sum the elements of the array
 **/
gfloat
gpm_array_float_sum (GpmArrayFloat *array)
{
	guint length;
	guint i;
	gfloat total = 0;

	length = array->len;
	for (i = 0; i < length; i++)
		total += g_array_index (array, gfloat, i);
	return total;
}

/**
 * gpm_array_float_print:
 *
 * @array: input array
 *
 * Print the array
 **/
gboolean
gpm_array_float_print (GpmArrayFloat *array)
{
	guint length;
	guint i;

	length = array->len;
	/* debug out */
	for (i = 0; i < length; i++)
		g_debug ("[%u]\tval=%f", i, g_array_index (array, gfloat, i));
	return TRUE;
}

/**
 * gpm_array_float_convolve:
 *
 * @data: input array
 * @kernel: kernel array
 * Return value: Colvolved array, same length as data
 *
 * Convolves an array with a kernel, and returns an array the same size.
 * THIS FUNCTION IS REALLY SLOW...
 **/
GpmArrayFloat *
gpm_array_float_convolve (GpmArrayFloat *data, GpmArrayFloat *kernel)
{
	gint length_data;
	gint length_kernel;
	GpmArrayFloat *result;
	gfloat value;
	gint i;
	gint j;
	gint idx;

	length_data = data->len;
	length_kernel = kernel->len;

	result = gpm_array_float_new (length_data);

	/* convolve */
	for (i=0;i<length_data;i++) {
		value = 0;
		for (j=0;j<length_kernel;j++) {
			idx = i+j-(length_kernel/2);
			if (idx < 0)
				idx = 0;
			else if (idx >= length_data)
				idx = length_data - 1;
			value += g_array_index (data, gfloat, idx) * g_array_index (kernel, gfloat, j);
		}
		g_array_index (result, gfloat, i) = value;
	}
	return result;
}

/**
 * gpm_array_float_compute_integral:
 * @array: This class instance
 *
 * Computes complete discrete integral of dataset.
 * Will only work with a step size of one.
 **/
gfloat
gpm_array_float_compute_integral (GpmArrayFloat *array, guint x1, guint x2)
{
	gfloat value;
	guint i;

	g_return_val_if_fail (x2 >= x1, 0.0);

	/* if the same point, then we have no area */
	if (x1 == x2)
		return 0.0;

	value = 0.0;
	for (i=x1; i <= x2; i++)
		value += g_array_index (array, gfloat, i);
	return value;
}

static gfloat
powfi (gfloat base, guint n)
{
	guint i;
	gfloat retval = 1;
	for (i=1; i <= n; i++)
		retval *= base;
	return retval;
}

/**
 * gpm_array_float_remove_outliers:
 *
 * @data: input array
 * @size: size to analyse
 * @sigma: sigma for standard deviation
 * Return value: Data with outliers removed
 *
 * Compares local sections of the data, removing outliers if they fall
 * ouside of sigma, and using the average of the other points in it's place.
 **/
GpmArrayFloat *
gpm_array_float_remove_outliers (GpmArrayFloat *data, guint length, gfloat sigma)
{
	guint i;
	guint j;
	guint half_length;
	gfloat value;
	gfloat average;
	gfloat average_not_inc;
	gfloat average_square;
	gfloat biggest_difference;
	gfloat outlier_value;
	GpmArrayFloat *result;

	g_return_val_if_fail (length % 2 == 1, NULL);
	result = gpm_array_float_new (data->len);

	/* check for no data */
	if (data->len == 0)
		goto out;

	half_length = (length - 1) / 2;

	/* copy start and end of array */
	for (i=0; i < half_length; i++)
		g_array_index (result, gfloat, i) = g_array_index (data, gfloat, i);
	for (i=data->len-half_length; i < data->len; i++)
		g_array_index (result, gfloat, i) = g_array_index (data, gfloat, i);

	/* find the standard deviation of a block off data */
	for (i=half_length; i < data->len-half_length; i++) {
		average = 0;
		average_square = 0;

		/* find the average and the squared average */
		for (j=i-half_length; j<i+half_length+1; j++) {
			value = g_array_index (data, gfloat, j);
			average += value;
			average_square += powfi (value, 2);
		}

		/* divide by length to get average */
		average /= length;
		average_square /= length;

		/* find the standard deviation */
		value = sqrtf (average_square - powfi (average, 2));

		/* stddev is okay */
		if (value < sigma) {
			g_array_index (result, gfloat, i) = g_array_index (data, gfloat, i);
		} else {
			/* ignore the biggest difference from the average */
			biggest_difference = 0;
			outlier_value = 0;
			for (j=i-half_length; j<i+half_length+1; j++) {
				value = fabs (g_array_index (data, gfloat, j) - average);
				if (value > biggest_difference) {
					biggest_difference = value;
					outlier_value = g_array_index (data, gfloat, j);
				}
			}
			average_not_inc = (average * length) - outlier_value;
			average_not_inc /= length - 1;
			g_array_index (result, gfloat, i) = average_not_inc;
		}
	}
out:
	return result;
}
