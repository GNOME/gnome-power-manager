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

#include "gpm-marshal.h"
#include "gpm-debug.h"

#include "gpm-info-data.h"

static void     gpm_info_data_class_init (GpmInfoDataClass *klass);
static void     gpm_info_data_init       (GpmInfoData      *info_data);
static void     gpm_info_data_finalize   (GObject		 	*object);

#define GPM_INFO_DATA_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_INFO_DATA, GpmInfoDataPrivate))

struct GpmInfoDataPrivate
{
	GList			*list;
	int			 length;	/* we track this to save time on add */
	GpmInfoDataPoint	*last_point;	/* to avoid having to go thru the list */
};

G_DEFINE_TYPE (GpmInfoData, gpm_info_data, G_TYPE_OBJECT)

/* this should be setable */
#define GPM_INFO_DATA_MAX_POINTS		120	/* when we should simplify data */
#define GPM_INFO_DATA_MAX_TIME			60	/* seconds, truncate after this */

GList*
gpm_info_data_get_list (GpmInfoData *info_data)
{
	g_return_val_if_fail (info_data != NULL, NULL);
	g_return_val_if_fail (GPM_IS_INFO_DATA (info_data), NULL);

	return info_data->priv->list;
}

/**
 * gpm_info_data_add_always:
 * @info_data: This InfoData instance
 * @time: The X data point
 * @value: The Y data point or event type
 * @colour: The colour of the point
 *
 * Allocates the memory and adds to the list.
 **/
void
gpm_info_data_add_always (GpmInfoData *info_data,
			  int	       time,
			  int	       value,
			  int	       colour,
			  const char  *desc)
{
	GpmInfoDataPoint *new;

	g_return_if_fail (info_data != NULL);
	g_return_if_fail (GPM_IS_INFO_DATA (info_data));

	/* we have to add a new data point */
	new = g_slice_new (GpmInfoDataPoint);
	new->time = time;
	new->value = value;
	new->colour = colour;
	if (desc) {
		new->desc = g_strdup (desc);
	} else {
		new->desc = NULL;
	}
	info_data->priv->list = g_list_append (info_data->priv->list, (gpointer) new);
	info_data->priv->length++;
	info_data->priv->last_point = new;
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
void
gpm_info_data_limit_time (GpmInfoData  *info_data,
			  int		max_num)
{
	GpmInfoDataPoint *point;
	GList *l;
	GList *list;

	g_return_if_fail (info_data != NULL);
	g_return_if_fail (GPM_IS_INFO_DATA (info_data));

	list = g_list_last (info_data->priv->list);
	point = (GpmInfoDataPoint *) list->data;
	gpm_debug ("Last point: x=%i, y=%i", point->time, point->value);
	float div = (float) point->time / (float) max_num;
	gpm_debug ("Using a time division of %f", div);

	GList *new = NULL;
	/* Reduces the number of points to a pre-set level using a time
	 * division algorithm so we don't keep diluting the previous
	 * data with a conventional 1-in-x type algorithm. */
	float a = 0;
	for (l=info_data->priv->list; l != NULL; l=l->next) {
		point = (GpmInfoDataPoint *) l->data;
		if (point->time >= a) {
			/* adding valid point */
			new = g_list_append (new, (gpointer) point);
			a = a + div;
		} else {
			/* removing point */
			gpm_info_data_free_point (point);
		}
	}
	/* freeing old list */
	g_list_free (info_data->priv->list);
	/* setting new data */
	info_data->priv->list = new;
	info_data->priv->length = g_list_length (info_data->priv->list);
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
void
gpm_info_data_limit_dilute (GpmInfoData *info_data,
			    int		 max_num)
{
	GpmInfoDataPoint *point;
	GList *l;
	GList *list;

	g_return_if_fail (info_data != NULL);
	g_return_if_fail (GPM_IS_INFO_DATA (info_data));

	int count = 0;
	for (l=info_data->priv->list; l != NULL; l=l->next) {
		point = (GpmInfoDataPoint *) l->data;
		count++;
		if (count == 3) {
			list = l->prev;
			/* we need to free the data */
			gpm_info_data_free_point (l->data);
			l->data = NULL;
			info_data->priv->list = g_list_delete_link (info_data->priv->list, l);
			l = list;
			count = 0;
		}
	}
	info_data->priv->length = g_list_length (info_data->priv->list);
}

/**
 * gpm_info_data_limit_truncate:
 * @graph_data: The data we have for a specific graph
 * @max_num: The max desired time we truncate the start to
 *
 * Trims the start of the data so that we don't store more than
 * the amount of time in the list. We have to be careful and not just remove
 * the old points, so we truncate, then limit by time to get the initial points
 * correct.
 *
 * DOES NOT WORK CORRECTLY YET.
 **/
void
gpm_info_data_limit_truncate (GpmInfoData *info_data,
			      int	   max_num)
{
	GpmInfoDataPoint *point;
	GList *l;
	GList *list;
	GList *last;
	int smallest;

	g_return_if_fail (info_data != NULL);
	g_return_if_fail (GPM_IS_INFO_DATA (info_data));

	last = g_list_last (info_data->priv->list);
	point = (GpmInfoDataPoint *)last->data;
	smallest = point->time - max_num;

	gpm_debug ("max=%i", max_num);
	for (l=info_data->priv->list; l != NULL; l=l->next) {
		point = (GpmInfoDataPoint *) l->data;
		if (point->time > smallest) {
			info_data->priv->list = l;
			break;
		} else {
			list = l->prev;
			/* remove link and point */
			gpm_info_data_free_point (l->data);
			l->data = NULL;
			info_data->priv->list = g_list_delete_link (info_data->priv->list, l);
			l = list;
		}
	}
	info_data->priv->length = g_list_length (info_data->priv->list);
}


/**
 * gpm_info_data_add:
 * @info_data: This InfoData instance
 * @time: The X data point
 * @value: The Y data point or event type
 * @colour: The colour of the point
 *
 * Adds an x-y point to a list. We have to save the X value as an integer, as
 * when we prune the values (when we have over 100) the X and Y values are
 * lost, and the data-points becomes non-uniform.
 **/
void
gpm_info_data_add (GpmInfoData *info_data,
		   int		time,
		   int		value,
		   int		colour)
{
	g_return_if_fail (info_data != NULL);
	g_return_if_fail (GPM_IS_INFO_DATA (info_data));

	gpm_debug ("%ix%i (%i)", time, value, colour);

	if (info_data->priv->length > 2) {
		if (info_data->priv->last_point->value == value) {
			/* we are the same as we were before and not the first or
			   second point, just side the data time across without
			   making a new point */
			info_data->priv->last_point->time = time;
		} else {
			/* we have to add a new data point as value is different */
			gpm_info_data_add_always (info_data, time, value, colour, NULL);
			if (value == 0) {
				/* if the rate suddenly drops we want a line
				   going down, then across, not a diagonal line.
				   Add an extra point so that we extend it horiz. */
				gpm_info_data_add_always (info_data, time, value, colour, NULL);
			}
		}
	} else {
		/* a new list requires a data point */
		gpm_info_data_add_always (info_data, time, value, colour, NULL);
	}
	if (info_data->priv->length > GPM_INFO_DATA_MAX_POINTS) {
		/* We have too much data, simplify */
		gpm_debug ("Too many points (%i/%i)",
			   info_data->priv->length,
			   GPM_INFO_DATA_MAX_POINTS);
		gpm_info_data_limit_time (info_data,
					  GPM_INFO_DATA_MAX_POINTS / 2);
	}
	gpm_debug ("Using %i lines", info_data->priv->length);
#if 0
	GList *first = g_list_first (info_data->priv->list);
	if (info_data->priv->length > 2) {
		GpmInfoDataPoint *first_point = (GpmInfoDataPoint *)first->data;
		int diff_time = info_data->priv->last_point->time - first_point->time;
		gpm_debug ("diff time = %i", diff_time);
		if (diff_time > GPM_INFO_DATA_MAX_TIME) {
			gpm_debug ("Too much time (%i/%i)",
				   diff_time,
				   GPM_INFO_DATA_MAX_TIME);
			gpm_info_data_limit_truncate (info_data,
						      GPM_INFO_DATA_MAX_TIME / 2);
		}
	}
#endif
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
 * gpm_info_data_init:
 * @graph: This info data instance
 **/
static void
gpm_info_data_init (GpmInfoData *info_data)
{
	info_data->priv = GPM_INFO_DATA_GET_PRIVATE (info_data);
	info_data->priv->list = NULL;
	info_data->priv->last_point = NULL;
	info_data->priv->length = 0;
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
	GList *l;
	for (l=info_data->priv->list; l != NULL; l=l->next) {
		gpm_info_data_free_point (l->data);
		l->data = NULL;
	}
	g_list_free (info_data->priv->list);
	info_data->priv->list = NULL;

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
