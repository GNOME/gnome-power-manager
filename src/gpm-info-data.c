/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib.h>
#include <glib/gi18n.h>

#include "gpm-debug.h"
#include "gpm-graph-widget.h"
#include "gpm-info-data.h"
#include "gpm-marshal.h"

static void     gpm_info_data_class_init (GpmInfoDataClass *klass);
static void     gpm_info_data_init       (GpmInfoData      *info_data);
static void     gpm_info_data_finalize   (GObject		 	*object);

#define GPM_INFO_DATA_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO_DATA, GpmInfoDataPrivate))

struct GpmInfoDataPrivate
{
	GPtrArray		*array;		/* the data array */
	guint			 max_points;	/* when we should simplify data */
	guint			 max_time;	/* truncate after this */
	gboolean		 has_data;	/* if we've had valid data (non zero) before */
};

G_DEFINE_TYPE (GpmInfoData, gpm_info_data, G_TYPE_OBJECT)

/**
 * gpm_info_data_set_max_points:
 * @info_data: This class instance
 * @max_points: The maximum number of points to show on the graph
 */
gboolean
gpm_info_data_set_max_points (GpmInfoData *info_data, guint max_points)
{
	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);

	info_data->priv->max_points = max_points;
	return TRUE;
}

/**
 * gpm_info_data_set_max_time:
 * @info_data: This class instance
 * @max_points: The maximum number of points to show on the graph
 */
gboolean
gpm_info_data_set_max_time (GpmInfoData *info_data, guint max_time)
{
	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);

	if (max_time > 10 * 60) {
		info_data->priv->max_time = max_time;
	} else {
		gpm_debug ("max_time value %i invalid", max_time);
	}
	return TRUE;
}

/**
 * gpm_info_data_get_list:
 * @info_data: This class instance
 *
 * Gets a GList of the data in the array.
 * NOTE: You have to use need to g_list_free (list) to free the returned list.
 **/
GList *
gpm_info_data_get_list (GpmInfoData *info_data)
{
	GList *list = NULL;
	GpmInfoDataPoint *point;
	int a;

	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);
	
	if (info_data->priv->array->len == 0) {
		gpm_debug ("no items in array!");
		return NULL;
	}
	for (a=0; a < info_data->priv->array->len; a++) {
		point = g_ptr_array_index (info_data->priv->array, a);
		list = g_list_append (list, (gpointer) point);
	}
	return list;
}

/**
 * gpm_info_data_add_always:
 * @info_data: This class instance
 * @time_secs: The X data point
 * @value: The Y data point or event type
 * @colour: The colour of the point
 *
 * Allocates the memory and adds to the list.
 **/
gboolean
gpm_info_data_add_always (GpmInfoData *info_data,
			  guint	       time_secs,
			  guint	       value,
			  guint8       colour,
			  const gchar *desc)
{
	GpmInfoDataPoint *new;

	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);

	/* we have to add a new data point */
	new = g_slice_new (GpmInfoDataPoint);
	new->time = time_secs;
	new->value = value;
	new->colour = colour;
	if (desc) {
		new->desc = g_strdup (desc);
	} else {
		new->desc = NULL;
	}
	g_ptr_array_add (info_data->priv->array, (gpointer) new);
	return TRUE;
}

/**
 * gpm_info_data_free_point:
 * @point: A data point we want to free
 **/
static void
gpm_info_data_free_point (GpmInfoDataPoint *point)
{
	/* we need to free the desc data if non-NULL */
	if (point->desc) {
		g_free (point->desc);
	}
	g_slice_free (GpmInfoDataPoint, point);
}

/**
 * gpm_info_data_limit_time:
 * @graph_data: The data we have for a specific graph
 * @max_num: The max desired points
 *
 * We need to reduce the number of data points else the graph will take a long
 * time to plot accuracy we don't need at the larger scales.
 * This will not reduce the scale or range of the data.
 **/
gboolean
gpm_info_data_limit_time (GpmInfoData  *info_data,
			  guint		max_num)
{
	GpmInfoDataPoint *point;
	gfloat div;
	gfloat running_count = 0.0f;
	guint len = info_data->priv->array->len;
	guint a;

	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);

	point = g_ptr_array_index (info_data->priv->array, len-1);
	div = (float) point->time / (float) max_num;
	gpm_debug ("Using a time division of %f", div);

	/* Reduces the number of points to a pre-set level using a time
	 * division algorithm so we don't keep diluting the previous
	 * data with a conventional 1-in-x type algorithm. */
	for (a=0; a < info_data->priv->array->len; a++) {
		point = g_ptr_array_index (info_data->priv->array, a);
		if (point->time >= running_count) {
			running_count = running_count + div;
			/* keep valid point */
		} else {
			/* removing point */
			gpm_info_data_free_point (point);
			g_ptr_array_remove_index (info_data->priv->array, a);
		}
	}
	return TRUE;
}

/**
 * gpm_info_data_limit_dilute:
 * @graph_data: The data we have for a specific graph
 * @max_num: The max desired points
 * @use_time: If we should use a per-time formula (better)
 *
 * Do a conventional 1-in-x type algorithm. This dilutes the data if run
 * again and again.
 **/
gboolean
gpm_info_data_limit_dilute (GpmInfoData *info_data,
			    guint	 max_num)
{
	GpmInfoDataPoint *point;
	guint count = 0;
	guint a;

	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);

	for (a=0; a < info_data->priv->array->len; a++) {
		point = g_ptr_array_index (info_data->priv->array, a);
		count++;
		if (count == 3) {
			gpm_info_data_free_point (point);
			g_ptr_array_remove_index (info_data->priv->array, a);
			count = 0;
		}
	}
	return TRUE;
}

/**
 * gpm_info_data_limit_truncate:
 * @graph_data: The data we have for a specific graph
 * @max_time: The max desired time we truncate the start to
 *
 * Trims the start of the data so that we don't store more than
 * the amount of time in the list. We have to be careful and not just remove
 * the old points, so we truncate, then limit by time to get the initial points
 * correct.
 **/
gboolean
gpm_info_data_limit_truncate (GpmInfoData *info_data,
			      guint	   max_time)
{
	GpmInfoDataPoint *point;
	guint a;
	guint len = info_data->priv->array->len;
	guint last_time;

	/* find the last point time */
	point = g_ptr_array_index (info_data->priv->array, len-1);
	last_time = point->time;

	/* points are always ordered in time */
	for (a=0; a < len; a++) {
		point = g_ptr_array_index (info_data->priv->array, a);
		if (last_time - point->time > max_time) {
			/* free point */
			gpm_info_data_free_point (point);
		} else {
			break;
		}
	}
	if (a > 0) {
		gpm_debug ("removing %i points from start of list", a);
		g_ptr_array_remove_range (info_data->priv->array, 0, a);
	}
	return TRUE;
}


/**
 * gpm_info_data_add:
 * @info_data: This class instance
 * @time_secs: The X data point
 * @value: The Y data point or event type
 * @colour: The colour of the point
 *
 * Adds an x-y point to a list. We have to save the X value as an integer, as
 * when we prune the values (when we have over 100) the X and Y values are
 * lost, and the data-points becomes non-uniform.
 **/
gboolean
gpm_info_data_add (GpmInfoData *info_data,
		   guint	time_secs,
		   guint	value,
		   guint8	colour)
{
	GpmInfoDataPoint *point;
	GpmInfoDataPoint *point2;
	guint len = info_data->priv->array->len;

	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);

	/* there is no point storing leading zeros data */
	if (info_data->priv->has_data == FALSE && value == 0) {
		gpm_debug ("not storing NULL data");
		return FALSE;
	}

	/* mark that we now have good data */
	info_data->priv->has_data = TRUE;

	if (len > 3) {
		point = g_ptr_array_index (info_data->priv->array, len-1);
		point2 = g_ptr_array_index (info_data->priv->array, len-2);
		if (point->value == value && point2->value == value) {
			/* we are the same as we were before and not the first or
			   second point, just side the data time across without
			   making a new point */
			point->time = time_secs;
		} else {
			/* we have to add a new data point as value is different */
			gpm_info_data_add_always (info_data, time_secs, value, colour, FALSE);
			if (value == 0) {
				/* if the rate suddenly drops we want a line
				   going down, then across, not a diagonal line.
				   Add an extra point so that we extend it horiz. */
				gpm_info_data_add_always (info_data, time_secs, value, colour, FALSE);
			}
		}
	} else {
		/* a list of less than 3 points always requires a data point */
		gpm_info_data_add_always (info_data, time_secs, value, colour, FALSE);
	}
	if (len > info_data->priv->max_points) {
		/* We have too much data, simplify */
		gpm_debug ("Too many points (%i/%i)",
			   info_data->priv->array->len,
			   info_data->priv->max_points);
		gpm_info_data_limit_time (info_data, info_data->priv->max_points / 2);
	}
	gpm_debug ("Using %i lines", info_data->priv->array->len);

	/* check if we need to truncate */
	if (info_data->priv->array->len > 2) {
		GpmInfoDataPoint *first = g_ptr_array_index (info_data->priv->array, 0);
		GpmInfoDataPoint *last = g_ptr_array_index (info_data->priv->array, len-1);
		int diff_time = last->time - first->time;
		if (diff_time > info_data->priv->max_time) {
			gpm_debug ("Too much time (%i/%i)", diff_time,
				   info_data->priv->max_time);
			gpm_info_data_limit_truncate (info_data, info_data->priv->max_time / 2);
		}
	}
	return TRUE;
}

/**
 * gpm_info_data_class_init:
 * @class: This info data instance
 **/
static void
gpm_info_data_class_init (GpmInfoDataClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_info_data_finalize;
	g_type_class_add_private (klass, sizeof (GpmInfoDataPrivate));
}

/**
 * gpm_info_data_free:
 * @object: This info data class instance
 **/
static void
gpm_info_data_free (GpmInfoData *info_data)
{
	GpmInfoDataPoint *point;
	int i;
	for (i=0; i < info_data->priv->array->len; i++) {
		point = g_ptr_array_index (info_data->priv->array, i);
		gpm_info_data_free_point (point);
	}
	g_ptr_array_free (info_data->priv->array, TRUE);
}

/**
 * gpm_info_data_init:
 * @graph: This info data instance
 **/
static void
gpm_info_data_init (GpmInfoData *info_data)
{
	info_data->priv = GPM_INFO_DATA_GET_PRIVATE (info_data);
	info_data->priv->array = g_ptr_array_new ();
	info_data->priv->max_points = 120;
	info_data->priv->max_time = 10 * 60;
	info_data->priv->has_data = FALSE;
}

/**
 * gpm_info_data_clear:
 * @graph: This info data instance
 **/
gboolean
gpm_info_data_clear (GpmInfoData *info_data)
{
	g_return_val_if_fail (info_data != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), FALSE);

	/* we just want to free all the elements and start again */
	gpm_info_data_free (info_data);
	gpm_info_data_init (info_data);
	return TRUE;
}

/**
 * gpm_info_data_finalize:
 * @object: This info data class instance
 **/
static void
gpm_info_data_finalize (GObject *object)
{
	GpmInfoData *info_data;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_INFO_DATA (object));

	info_data = GPM_INFO_DATA (object);

	/* Free the graph data elements, the list, and also the graph data object */
	gpm_info_data_free (info_data);

	G_OBJECT_CLASS (gpm_info_data_parent_class)->finalize (object);
}

/**
 * gpm_info_data_new:
 * Return value: A new GpmInfoData object.
 **/
GpmInfoData *
gpm_info_data_new (void)
{
	return GPM_INFO_DATA (g_object_new (GPM_TYPE_INFO_DATA, NULL));
}
