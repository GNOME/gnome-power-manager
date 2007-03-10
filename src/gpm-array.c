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

#include "config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

#include "gpm-debug.h"
#include "gpm-common.h"
#include "gpm-array.h"

static void     gpm_array_class_init (GpmArrayClass *klass);
static void     gpm_array_init       (GpmArray      *array);
static void     gpm_array_finalize   (GObject		 	*object);

#define GPM_ARRAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_ARRAY, GpmArrayPrivate))

struct GpmArrayPrivate
{
	GPtrArray		*array;		/* the data array */
	gboolean		 fixed_size;	/* if we are a fixed size */
	gboolean		 variable_size;	/* if we are allowed in insert data */
	gboolean		 has_data;	/* if we've had valid data (non zero) before */
	guint			 max_points;	/* when we should simplify data */
	guint			 max_width;	/* truncate after this */
};

G_DEFINE_TYPE (GpmArray, gpm_array, G_TYPE_OBJECT)

/**
 * gpm_array_get:
 * @array: This class instance
 * @i: The array element
 **/
GpmArrayPoint *
gpm_array_get (GpmArray *array,
	       guint	 i)
{
	GpmArrayPoint *point;
	guint length;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	length = array->priv->array->len;
	if (i + 1 > length) {
		g_warning ("out of bounds %i of %i", i, length);
		return NULL;
	}
	point = g_ptr_array_index (array->priv->array, i);

	return point;
}

/**
 * gpm_array_set:
 * @array: This class instance
 * @i: The array element
 * @x: The X data point
 * @y: The Y data point or event type
 * @data: The data for the point
 *
 * Sets the values of something already in the list
 **/
gboolean
gpm_array_set (GpmArray *array,
	       guint	 i,
	       guint	 x,
	       guint	 y,
	       guint	 data)
{
	GpmArrayPoint *point;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	point = gpm_array_get (array, i);
	point->x = x;
	point->y = y;
	point->data = data;
	return TRUE;
}

/**
 * gpm_array_append:
 * @array: This class instance
 * @x: The X data point
 * @y: The Y data point or event type
 * @data: The data for the point
 *
 * Allocates the memory and adds to the list.
 **/
gboolean
gpm_array_append (GpmArray *array,
		  guint	    x,
		  guint	    y,
		  guint	    data)
{
	GpmArrayPoint *point;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	if (array->priv->fixed_size == TRUE) {
		/* not valid as array is fixed size */
		return FALSE;
	}

	array->priv->variable_size = TRUE;

	/* we have to add a new data point */
	point = g_slice_new (GpmArrayPoint);
	point->x = x;
	point->y = y;
	point->data = data;
	g_ptr_array_add (array->priv->array, (gpointer) point);
	return TRUE;
}

/**
 * gpm_array_invert:
 * @array: This class instance
 *
 * Swaps x and y for the dataset
 **/
gboolean
gpm_array_invert (GpmArray *array)
{
	GpmArrayPoint *point;
	guint i;
	guint temp;
	guint length;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		temp = point->x;
		point->x = point->y;
		point->y = temp;
	}
	return TRUE;
}

/**
 * gpm_array_print:
 * @array: This class instance
 *
 * Prints the dataset
 **/
gboolean
gpm_array_print (GpmArray *array)
{
	GpmArrayPoint *point;
	guint i;
	guint length;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		gpm_debug ("(%u) x=%u,\ty=%u,\tdata=%u", i, point->x, point->y, point->data);
	}
	return TRUE;
}

/**
 * gpm_array_append_from_file:
 * @array: This class instance
 * @filename: CSV data file.
 *
 * Loads data from a CSV file
 **/
gboolean
gpm_array_append_from_file (GpmArray *array, const gchar *filename)
{
	gchar *contents;
	gchar **lines;
	gsize length;
	guint i;
	guint x;
	guint y;
	guint data;

	if (array->priv->fixed_size == TRUE) {
		/* not valid as array is fixed size */
		return FALSE;
	}

	g_file_get_contents (filename, &contents, &length, NULL);
	if (contents == NULL) {
		g_warning ("cannot open file %s", filename);
		return FALSE;
	}

	/* split into lines and process each one */
	lines = g_strsplit (contents, "\n", -1);
	i = 0;
	while (lines[i] != NULL) {
		if (strlen (lines[i]) > 3) {
			sscanf (lines[i], "%u, %u, %u", &x, &y, &data);
			gpm_array_append (array, x, y, data);
		}
		i++;
	}
	g_strfreev (lines);
	g_free (contents);
	return TRUE;
}

/**
 * gpm_array_load_from_file:
 * @array: This class instance
 * @filename: CSV data file.
 *
 * Loads data from a CSV file
 **/
gboolean
gpm_array_load_from_file (GpmArray *array, const gchar *filename)
{
	gchar *contents;
	gchar **lines;
	gsize length;
	guint i;
	guint x;
	guint y;
	guint data;

	if (array->priv->fixed_size == FALSE) {
		/* not valid as array is variable size */
		return FALSE;
	}

	g_file_get_contents (filename, &contents, &length, NULL);
	if (contents == NULL) {
		g_warning ("cannot open file %s", filename);
		return FALSE;
	}

	/* split into lines and process each one */
	lines = g_strsplit (contents, "\n", -1);
	i = 0;
	while (lines[i] != NULL) {
		if (strlen (lines[i]) > 3) {
			sscanf (lines[i], "%u, %u, %u", &x, &y, &data);
			gpm_array_set (array, i, x, y, data);
		}
		i++;
	}
	g_strfreev (lines);
	g_free (contents);
	return TRUE;
}

/**
 * gpm_array_save_to_file:
 * @array: This class instance
 * @filename: CSV data file.
 *
 * Saves data to a CSV file
 **/
gboolean
gpm_array_save_to_file (GpmArray *array, const gchar *filename)
{
	gchar *contents;
	guint i;
	guint length;
	gboolean ret;
	GString *string;
	GpmArrayPoint *point;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	length = gpm_array_get_size (array);

	/* create a suitable sized largish string */
	string = g_string_sized_new (length * 12);

	/* get string array */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		g_string_append_printf (string, "%u, %u, %u\n", point->x, point->y, point->data);
	}
	contents = g_string_free (string, FALSE);

	ret = g_file_set_contents (filename, contents, -1, NULL);
	if (ret == FALSE) {
		gpm_warning ("cannot write file %s", filename);
		return FALSE;
	}
	g_free (contents);
	return TRUE;
}

/**
 * gpm_array_set_fixed_size:
 * @array: This class instance
 * @x: The X data point
 * @y: The Y data point or event type
 *
 * Allocates the memory and adds to the list.
 **/
gboolean
gpm_array_set_fixed_size (GpmArray *array,
			  guint	    size)
{
	GpmArrayPoint *point;
	guint i;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);
	g_return_val_if_fail (array->priv->variable_size == FALSE, FALSE);

	array->priv->fixed_size = TRUE;

	/* we have to add a new data point */
	for (i=0; i < size; i++) {
		point = g_slice_new (GpmArrayPoint);
		point->x = 0;
		point->y = 0;
		point->data = 0;
		g_ptr_array_add (array->priv->array, (gpointer) point);
	}
	return TRUE;
}

/**
 * gpm_array_compute_rate_lsrl:
 * @array: This class instance
 * @newarray: The new rate array
 * @slew: The slew rate to be used either side of the filter.
 *
 * Computes the rate of the data using least squares regression fitting
 * Edges are truncated at half the slew rate.
 *      __
 *      \         _       _
 *      /_ (x  -  x)(y  - y)
 * m =      i        i
 *      __------------------
 *      \         _ 2
 *      /_ (x  -  x)
 *           i
 **/
gboolean
gpm_array_compute_rate_lsrl (GpmArray	*array,
			     GpmArray	*newarray,
			     guint	 slew)
{
	GpmArrayPoint *point;
	guint i, j;
	guint run;
	gfloat meanx;
	gfloat meany;
	gfloat top;
	gfloat bottom;
	gfloat gradient;
	guint length;
	gint start;
	gint end;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);

		/* i is the center position and the data we are trying to find.
		 * start and end are up to slew wide, as long as they fit in the dataset
		 * run is the width of the sample */
		start = i - slew;
		if (start < 0) {
			start = 0;
		}
		end = i + slew;
		if (end > length - 1) {
			end = length - 1;
		}
		run = end - start;

		if (run > slew * 1.25) {
			/* calculate the mean of x and y over the window */
			meanx = 0.0f;
			meany = 0.0f;
			for (j=start; j<end; j++) {
				point = gpm_array_get (array, j);
				meanx += point->x;
				meany += point->y;
			}
			meanx /= (gfloat) run+1;
			meany /= (gfloat) run+1;

			top = 0;
			bottom = 0;
			for (j=start; j<end; j++) {
				point = gpm_array_get (array, j);
				top += (point->x - meanx) * (point->y - meany);
				bottom += (point->x - meanx) * (point->x - meanx);
			}
			gradient = fabs(top) / bottom;
			/* multiply by a big number to make fit in an uint32 */
			gradient = gradient * 10000;

			point = gpm_array_get (array, i);
			gpm_array_append (newarray, point->x, (guint) gradient, 0);
		}

	}

	return TRUE;
}

/**
 * gpm_array_compute_uwe_self:
 * @array: This class instance
 * @slew: The slew rate to be used either side of the filter.
 *
 * Computes the uniform wieghted average for a dataset
 **/
gboolean
gpm_array_compute_uwe_self (GpmArray *array, guint slew)
{
	GpmArray *temp;
	guint size;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* create temp array */
	size = gpm_array_get_size (array);
	/* create an array with the same size as the target */
	temp = gpm_array_new ();
	gpm_array_set_fixed_size (temp, size);

	/* compute average */
	gpm_array_compute_uwe (array, temp, slew);
	gpm_array_copy (temp, array);

	/* we've used and abused the temp array, now kill it */
	g_object_unref (temp);
	return TRUE;
}

/**
 * gpm_array_compute_uwe:
 * @array: This class instance
 * @newarray: The new rate array
 * @slew: The slew rate to be used either side of the filter.
 *
 * Computes the uniform wieghted average for a dataset
 **/
gboolean
gpm_array_compute_uwe (GpmArray	*array,
		       GpmArray	*newarray,
		       guint	 slew)
{
	GpmArrayPoint *point;
	guint i, j;
	guint run;
	gfloat meany;
	guint length;
	gint start;
	gint end;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);

		/* i is the center position and the data we are trying to find.
		 * start and end are up to slew wide, as long as they fit in the dataset
		 * run is the width of the sample */
		start = i - slew;
		if (start < 0) {
			start = 0;
		}
		end = i + slew;
		if (end > length - 1) {
			end = length - 1;
		}
		run = end - start;

		/* calculate the mean of y over the window */
		meany = 0.0f;
		for (j=start; j<end; j++) {
			point = gpm_array_get (array, j);
			meany += point->y;
		}
		meany /= (gfloat) run;

		/* do not append, just set for speed */
		gpm_array_set (newarray, i, i, (guint) meany, point->data);
	}

	return TRUE;
}

/**
 * gpm_array_sort_by_x_compare_func
 * @a: Pointer to one element in the array
 * @b: Pointer to another element in the array
 *
 * A GCompareFunc for sorting an array.
 **/
static gint
gpm_array_sort_by_x_compare_func (gconstpointer a, gconstpointer b)
{
	GpmArrayPoint *pa, *pb;

	pa = *((GpmArrayPoint **) a);
	pb = *((GpmArrayPoint **) b);
	if (pa->x < pb->x) {
		return -1;
	} else if (pa->x > pb->x) {
		return 1;
	}
	return 0;
}

/**
 * gpm_array_sort_by_x:
 * @array: This class instance
 *
 * Sorts the array by x values.
 **/
gboolean
gpm_array_sort_by_x (GpmArray *array)
{
	g_ptr_array_sort (array->priv->array, gpm_array_sort_by_x_compare_func);
	return TRUE;
}

/**
 * gpm_array_sort_by_y_compare_func
 * @a: Pointer to one element in the array
 * @b: Pointer to another element in the array
 *
 * A GCompareFunc for sorting an array.
 **/
static gint
gpm_array_sort_by_y_compare_func (gconstpointer a, gconstpointer b)
{
	GpmArrayPoint *pa, *pb;

	pa = *((GpmArrayPoint **) a);
	pb = *((GpmArrayPoint **) b);
	if (pa->y < pb->y) {
		return -1;
	} else if (pa->y > pb->y) {
		return 1;
	}
	return 0;
}

/**
 * gpm_array_sort_by_x:
 * @array: This class instance
 *
 * Sorts the array by x values.
 **/
gboolean
gpm_array_sort_by_y (GpmArray *array)
{
	g_ptr_array_sort (array->priv->array, gpm_array_sort_by_y_compare_func);
	return TRUE;
}

/**
 * gpm_array_compute_integral:
 * @array: This class instance
 *
 * Computes complete discrete integral of dataset.
 * Will only work with a step size of one.
 **/
guint
gpm_array_compute_integral (GpmArray *array, guint x1, guint x2)
{
	GpmArrayPoint *point;
	gint i;
	guint value;

	g_return_val_if_fail (array != NULL, 0);
	g_return_val_if_fail (GPM_IS_ARRAY (array), 0);
	g_return_val_if_fail (x2 > x1, 0);

	value = 0;
	gpm_debug ("computing integral from %i to %i", x1, x2);
	for (i=x1; i < x2; i++) {
		point = gpm_array_get (array, i);
		value += point->y;
	}
	return value;
}

/**
 * gpm_array_invert_value_y:
 * @array: This class instance
 *
 * Does 1/point->y.
 **/
gboolean
gpm_array_invert_value_y (GpmArray *array)
{
	GpmArrayPoint *point;
	gint i;
	gfloat value;
	guint length;

	g_return_val_if_fail (array != NULL, 0);
	g_return_val_if_fail (GPM_IS_ARRAY (array), 0);

	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		value = (gfloat) point->y;
		if (value != 0) {
			value = 1.0f / value;
			value *= 100000;
			point->y = value;
		}
	}
	return TRUE;
}

/**
 * gpm_array_copy:
 * @from: list to copy from
 * @to: list to copy to
 *
 * Lists must be the same size
 *
 **/
gboolean
gpm_array_copy (GpmArray *from, GpmArray *to)
{
	GpmArrayPoint *point;
	gint i;
	guint lengthto;
	guint lengthfrom;

	g_return_val_if_fail (from != NULL, 0);
	g_return_val_if_fail (GPM_IS_ARRAY (from), 0);
	g_return_val_if_fail (to != NULL, 0);
	g_return_val_if_fail (GPM_IS_ARRAY (to), 0);

	lengthto = gpm_array_get_size (to);
	lengthfrom = gpm_array_get_size (from);

	/* check lengths are the same */
	if (lengthfrom != lengthto) {
		gpm_debug ("arrays are not the same length");
		return FALSE;
	}

	/* just set */
	for (i=0; i < lengthfrom; i++) {
		point = gpm_array_get (from, i);
		gpm_array_set (to, i, point->x, point->y, point->data);
	}
	return TRUE;
}

/**
 * gpm_array_copy_append:
 * @from: list to copy from
 * @to: list to copy to
 *
 **/
gboolean
gpm_array_copy_append (GpmArray *from, GpmArray *to)
{
	GpmArrayPoint *point;
	gint i;
	guint length;

	g_return_val_if_fail (from != NULL, 0);
	g_return_val_if_fail (GPM_IS_ARRAY (from), 0);
	g_return_val_if_fail (to != NULL, 0);
	g_return_val_if_fail (GPM_IS_ARRAY (to), 0);

	length = gpm_array_get_size (from);
	for (i=0; i < length; i++) {
		point = gpm_array_get (from, i);
		gpm_array_append (to, point->x, point->y, point->data);
	}
	return TRUE;
}

/**
 * gpm_array_ensure_unique_y:
 * @array: This class instance
 *
 * Ensure there is a unique y value, averaging if necessary.
 **/
gboolean
gpm_array_ensure_unique_y (GpmArray *array)
{
	GpmArrayPoint *point;
	GpmArrayPoint *unique_point;
	gint i;
	guint newvalue;
	guint oldvalue;
	guint num_average;
	guint length;
	gfloat div;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	newvalue = 0;
	oldvalue = 0;
	num_average = 0;
	unique_point = NULL;

	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		if (i != 0 && oldvalue == point->y) {
			/* point the same detected */
			gpm_debug ("point the same detected at %i: %i (removing second point)", i, oldvalue);
			newvalue += point->x;
			num_average++;
			g_ptr_array_remove_index (array->priv->array, i);
			length = gpm_array_get_size (array);
		} else {
			if (num_average != 0) {
				/* add, and use unique point */
				div = (gfloat) newvalue / (gfloat) num_average;
				gpm_debug ("replacing %i with %i", unique_point->x, (guint) div);
				unique_point->x = (guint) div;
				newvalue = 0;
				num_average = 0;
			}
			unique_point = point;
		}
		oldvalue = point->y;
	}
	return TRUE;
}

/**
 * gpm_array_interpolate_zeros:
 * @array: This class instance
 *
 * Computes zero values with interpolation
 * Assumes every x value exists with index value.
 **/
gboolean
gpm_array_interpolate_zeros (GpmArray *array)
{
	GpmArrayPoint *point;
	gint i;
	gint previous;
	gint value;
	guint length;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	previous = 0;
	value = 0;
	point = NULL;
	/* go forwards through the dataset, will miss any preceeding zeros */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		if (point->y != 0) {
			value = gpm_exponential_average (previous, point->y, 90);
			previous = value;
		} else {
			point->x = i;
			point->y = value;
			gpm_debug ("overwrite %i with %i", point->x, value);
		}
	}
	previous = 0;
	value = 0;
	point = NULL;
	/* go backwards through the dataset to hit any preceeding zeros */
	for (i=length-1; i >= 0; i--) {
		point = gpm_array_get (array, i);
		if (point->y != 0) {
			value = gpm_exponential_average (previous, point->y, 90);
			previous = value;
		} else {
			point->x = i;
			point->y = value;
			gpm_debug ("overwrite %i with %i", point->x, value);
		}
	}
	return TRUE;
}

/**
 * gpm_array_compute_relationship:
 * @array1: This class instance
 * @array2: Another class instance
 * @newarray: The new relationship array
 *
 * Computes the relationship of the x variable on both datasets.
 * Both datasets have to have the same size, or array2 has to start later than array1.
 *
 * newarray has to be preallocated to the same size as, or bigger than the largest of array1 and array2
 **/
gboolean
gpm_array_compute_relationship (GpmArray *array1,
			        GpmArray *array2,
			        GpmArray *newarray)
{
	GpmArrayPoint *point1;
	GpmArrayPoint *point2;
	GpmArrayPoint *newpoint;
	guint i;
	guint offset;

	g_return_val_if_fail (array1 != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array1), FALSE);
	g_return_val_if_fail (array2 != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array2), FALSE);
	g_return_val_if_fail (newarray != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (newarray), FALSE);
	g_return_val_if_fail (gpm_array_get_size (newarray) >= gpm_array_get_size (array1), FALSE);
	g_return_val_if_fail (gpm_array_get_size (newarray) >= gpm_array_get_size (array2), FALSE);

	gpm_debug ("array1 size = %i (bigger)", gpm_array_get_size (array1));
	gpm_debug ("array2 size = %i (smaller)", gpm_array_get_size (array2));
	gpm_debug ("newarray size = %i (biggest)", gpm_array_get_size (newarray));

	offset = 0;
	for (i=0; i < array2->priv->array->len; i++) {
		/* match up the datasets so they align with x */
		point2 = g_ptr_array_index (array2->priv->array, i);
		do {
			point1 = g_ptr_array_index (array1->priv->array, i + offset);
			if (point1->x != point2->x) {
				offset += 1;
				gpm_debug ("offset increased to %i on iteration %i", offset, i);
			}
			if (offset > 5) {
				g_warning ("offset too large");
				return FALSE;
			}
		} while (point1->x != point2->x);
		newpoint = gpm_array_get (newarray, i + offset);
		newpoint->x = point1->y + offset;
		newpoint->y = point2->y;
	}
	return TRUE;
}

/**
 * gpm_array_interpolate_points:
 * @this: The first data point
 * @last: The other data point
 * @xintersect: The x value (i.e. the x we provide)
 * Return value: The interpolated value, or 0 if invalid
 *
 * Interpolates onto the graph in the y direction. If only supplied one point
 * then don't interpolate.
 **/
static gint
gpm_array_interpolate_points (GpmArrayPoint *this,
			      GpmArrayPoint *last,
			      gint           xintersect)
{
	gint dy, dx;
	gfloat m;
	gint c;
	gint y;

	/* we have no points */
	if (this == NULL) {
		return 0;
	}

	/* we only have one point, don't interpolate */
	if (last == NULL) {
		return this->y;
	}

	/* gradient */
	dx = this->x - last->x;
	dy = this->y - last->y;
	m = (gfloat) dy / (gfloat) dx;

	/* y-intersect */
	c = (-m * (gfloat) this->x) + this->y;

	/* y = mx + c */
	y = (m * (gfloat) xintersect) + c;

	/* limit the y intersect to the last height, so we don't extend the
	 * graph into the unknown */
	if (y > this->y) {
		y = this->y;
	}
	return y;
}

/**
 * gpm_array_interpolate:
 * @xintersect: The x value
 * Return value: The interpolated Y value, or 0 if invalid
 *
 * Interpolates onto the graph in the y direction. If only supplied one point
 * then don't interpolate.
 **/
gint
gpm_array_interpolate (GpmArray *array, gint xintersect)
{
	GpmArrayPoint *oldpoint = NULL;
	GpmArrayPoint *point;
	guint i;
	guint length;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		if (point->x > xintersect) {
			return gpm_array_interpolate_points (point, oldpoint, xintersect);
		}
		oldpoint = point;
	}
	if (oldpoint != NULL) {
		return oldpoint->y;
	}
	return 0;
}

/**
 * gpm_array_set_data:
 * @array: This class instance
 *
 * Overwrites the data value for the whole array.
 **/
gboolean
gpm_array_set_data (GpmArray *array,
		    guint     data)
{
	GpmArrayPoint *point;
	guint i;
	guint length;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we have to add a new data point */
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		point->data = data;
	}
	return TRUE;
}

/**
 * gpm_array_get_size:
 * @array: This class instance
 **/
guint
gpm_array_get_size (GpmArray *array)
{
	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	return array->priv->array->len;
}

/**
 * gpm_array_set_max_points:
 * @array: This class instance
 * @max_points: The maximum number of points to show on the graph
 */
gboolean
gpm_array_set_max_points (GpmArray *array, guint max_points)
{
	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	array->priv->max_points = max_points;
	return TRUE;
}

/**
 * gpm_array_set_max_width:
 * @array: This class instance
 * @max_points: The maximum number of points to show on the graph
 */
gboolean
gpm_array_set_max_width (GpmArray *array, guint max_width)
{
	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	array->priv->max_width = max_width;
	return TRUE;
}

/**
 * gpm_array_free_point:
 * @point: A data point we want to free
 **/
static void
gpm_array_free_point (GpmArrayPoint *point)
{
	g_slice_free (GpmArrayPoint, point);
}

/**
 * gpm_array_limit_x_size:
 * @graph_data: The data we have for a specific graph
 * @max_num: The max desired points
 *
 * We need to reduce the number of data points else the graph will take a long
 * time to plot accuracy we don't need at the larger scales.
 * This will not reduce the scale or range of the data.
 **/
gboolean
gpm_array_limit_x_size (GpmArray *array,
		        guint     max_num)
{
	GpmArrayPoint *point;
	gfloat div;
	gfloat running_count = 0.0f;
	guint length;
	guint a;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	length = gpm_array_get_size (array);

	/* sanity check */
	if (length < max_num) {
		gpm_debug ("no limit possible as under limit");
		return FALSE;
	}

	/* last element */
	point = gpm_array_get (array, length-1);
	div = (gfloat) point->x / (gfloat) max_num;
	gpm_debug ("Using a x division of %f", div);

	/* Reduces the number of points to a pre-set level using a time
	 * division algorithm so we don't keep diluting the previous
	 * data with a conventional 1-in-x type algorithm. */
	for (a=0; a < length; a++) {
		point = gpm_array_get (array, a);
		if (point->x >= running_count) {
			running_count = running_count + div;
			gpm_debug ("keeping valid point %i", a);
			/* keep valid point */
		} else {
			/* remove point */
			gpm_debug ("removing invalid point %i", a);
			gpm_array_free_point (point);
			g_ptr_array_remove_index (array->priv->array, a);

			/* decrement the array length as we removed a point */
			length--;
			/* re-evaluate the 'current' item */
			a--;
		}
	}
	return TRUE;
}

/**
 * gpm_array_limit_x_width:
 * @graph_data: The data we have for a specific graph
 * @max_width: The max desired width we truncate the start to
 *
 * Trims the start of the data so that we don't store more than
 * the amount of time in the list. We have to be careful and not just remove
 * the old points, so we truncate, then limit by x to get the initial points
 * correct.
 **/
gboolean
gpm_array_limit_x_width (GpmArray *array,
			 guint	   max_width)
{
	GpmArrayPoint *point;
	guint a;
	guint length;
	guint last_x;

	/* find the last point time */
	length = gpm_array_get_size (array);
	point = gpm_array_get (array, length-1);
	last_x = point->x;

	/* points are always ordered in time */
	for (a=0; a < length; a++) {
		point = gpm_array_get (array, a);
		if (last_x - point->x > max_width) {
			/* free point */
			gpm_array_free_point (point);
		} else {
			break;
		}
	}
	if (a > 0) {
		gpm_debug ("removing %i points from start of list", a);
		g_ptr_array_remove_range (array->priv->array, 0, a);
	}
	return TRUE;
}

/**
 * gpm_array_add:
 * @array: This class instance
 * @x: The X data point
 * @y: The Y data point or event type
 * @data: The data
 *
 * Adds an x-y point to a list. We have to save the X value as an integer, as
 * when we prune the values (when we have over 100) the X and Y values are
 * lost, and the data-points becomes non-uniform.
 **/
gboolean
gpm_array_add (GpmArray *array,
	       guint	 x,
	       guint	 y,
	       guint	 data)
{
	GpmArrayPoint *point1;
	GpmArrayPoint *point2;
	guint length;
	guint diff_time;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	if (array->priv->fixed_size == TRUE) {
		/* not valid as array is fixed size */
		return FALSE;
	}

	length = gpm_array_get_size (array);

	/* there is no point storing leading zeros data */
	if (array->priv->has_data == FALSE && y == 0) {
		return FALSE;
	}

	/* mark that we now have good data */
	array->priv->has_data = TRUE;

	if (length > 3) {
		point1 = gpm_array_get (array, length-1);
		point2 = gpm_array_get (array, length-2);
		if (point1->y == y && point2->y == y) {
			/* we are the same as we were before and not the first or
			   second point, just side the data time across without
			   making a new point */
			point1->x = x;
		} else {
			/* we have to add a new data point as value is different */
			gpm_array_append (array, x, y, data);
			if (y == 0) {
				/* if the rate suddenly drops we want a line
				   going down, then across, not a diagonal line.
				   Add an extra point so that we extend it horiz. */
				gpm_array_append (array, x, y, data);
			}
			gpm_debug ("Using %i elements", length);
		}
	} else {
		/* a list of less than 3 points always requires a data point */
		gpm_array_append (array, x, y, data);
	}
	if (length > array->priv->max_points) {
		/* We have too much data, simplify */
		gpm_debug ("Too many points (%i/%i)", length, array->priv->max_points);
		gpm_array_limit_x_size (array, array->priv->max_points / 2);
	}

	/* check if we need to truncate */
	length = gpm_array_get_size (array);
	if (length > 2) {
		point1 = gpm_array_get (array, 0);
		point2 = gpm_array_get (array, length-1);
		diff_time = point2->x - point1->x;
		if (diff_time > array->priv->max_width) {
			gpm_debug ("Too much time (%i/%i)", diff_time, array->priv->max_width);
			gpm_array_limit_x_width (array, array->priv->max_width / 2);
		}
	}
	return TRUE;
}

/**
 * gpm_array_class_init:
 * @class: This class instance
 **/
static void
gpm_array_class_init (GpmArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_array_finalize;
	g_type_class_add_private (klass, sizeof (GpmArrayPrivate));
}

/**
 * gpm_array_free:
 * @object: This info data class instance
 **/
static void
gpm_array_free (GpmArray *array)
{
	GpmArrayPoint *point;
	guint length;
	int i;
	length = gpm_array_get_size (array);
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		gpm_array_free_point (point);
	}
	g_ptr_array_free (array->priv->array, TRUE);
}

/**
 * gpm_array_init:
 * @array: This class instance
 **/
static void
gpm_array_init (GpmArray *array)
{
	array->priv = GPM_ARRAY_GET_PRIVATE (array);
	array->priv->array = g_ptr_array_new ();
	array->priv->fixed_size = FALSE;
	array->priv->variable_size = FALSE;
	array->priv->has_data = FALSE;
	array->priv->max_points = 120;
	array->priv->max_width = 10 * 60;
}

/**
 * gpm_array_clear:
 * @graph: This class instance
 **/
gboolean
gpm_array_clear (GpmArray *array)
{
	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* we just want to free all the elements and start again */
	gpm_array_free (array);
	gpm_array_init (array);
	return TRUE;
}

/**
 * gpm_array_finalize:
 * @object: This info data class instance
 **/
static void
gpm_array_finalize (GObject *object)
{
	GpmArray *array;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_ARRAY (object));

	array = GPM_ARRAY (object);

	/* Free the graph data elements, the list, and also the graph data object */
	gpm_array_free (array);

	G_OBJECT_CLASS (gpm_array_parent_class)->finalize (object);
}

/**
 * gpm_array_new:
 * Return value: A new GpmArray object.
 **/
GpmArray *
gpm_array_new (void)
{
	GpmArray *array;
	array = g_object_new (GPM_TYPE_ARRAY, NULL);
	return GPM_ARRAY (array);
}

