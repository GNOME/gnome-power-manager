/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "egg-color.h"
#include "gpm-common.h"
#include "gpm-graph-widget.h"
#include "gpm-array.h"
#include "egg-debug.h"

G_DEFINE_TYPE (GpmGraphWidget, gpm_graph_widget, GTK_TYPE_DRAWING_AREA);
#define GPM_GRAPH_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidgetPrivate))
#define GPM_GRAPH_WIDGET_FONT "Sans 8"

struct GpmGraphWidgetPrivate
{
	gboolean		 use_grid;
	gboolean		 use_legend;
	gboolean		 use_events;
	gboolean		 autorange_x;

	GSList			*key_data; /* lines */
	GSList			*key_event; /* dots */

	gint			 stop_x;
	gint			 stop_y;
	gint			 start_x;
	gint			 start_y;
	gint			 box_x; /* size of the white box, not the widget */
	gint			 box_y;
	gint			 box_width;
	gint			 box_height;

	gfloat			 unit_x; /* 10th width of graph */
	gfloat			 unit_y; /* 10th width of graph */

	GpmGraphWidgetAxisType	 axis_type_x;
	GpmGraphWidgetAxisType	 axis_type_y;
	gchar			*title;

	cairo_t			*cr;
	PangoLayout 		*layout;

	GPtrArray		*data_list;
	GpmArray		*events;
};

static gboolean gpm_graph_widget_expose (GtkWidget *graph, GdkEventExpose *event);
static void	gpm_graph_widget_finalize (GObject *object);

/**
 * gpm_graph_widget_key_compare_func
 * Return value: 0 if cookie matches
 **/
static gint
gpm_graph_widget_key_compare_func (gconstpointer a, gconstpointer b)
{
	GpmGraphWidgetKeyItem *data;
	guint32 id;
	data = (GpmGraphWidgetKeyItem*) a;
	id = *((guint*) b);
	if (id == data->id)
		return 0;
	return 1;
}

/**
 * gpm_graph_widget_key_find_id:
 **/
static GpmGraphWidgetKeyItem *
gpm_graph_widget_key_find_id (GpmGraphWidget *graph, guint id)
{
	GpmGraphWidgetKeyItem *data;
	GSList *ret;
	ret = g_slist_find_custom (graph->priv->key_event, &id,
				   gpm_graph_widget_key_compare_func);
	if (ret == NULL) {
		return NULL;
	}
	data = (GpmGraphWidgetKeyItem *) ret->data;
	return data;
}

/**
 * gpm_graph_widget_key_event_add:
 **/
gboolean
gpm_graph_widget_key_event_add (GpmGraphWidget *graph,
			  guint		        id,
			  guint32               colour,
			  GpmGraphWidgetShape   shape,
			  const gchar	       *desc)
{
	GpmGraphWidgetKeyItem *keyitem;
	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	keyitem = gpm_graph_widget_key_find_id (graph, id);
	if (keyitem != NULL) {
		egg_warning ("keyitem %i already in use", id);
		return FALSE;
	}

	egg_debug ("add to hashtable '%s'", desc);
	keyitem = g_new0 (GpmGraphWidgetKeyItem, 1);
	keyitem->id = id;
	keyitem->desc = g_strdup (desc);
	keyitem->colour = colour;
	keyitem->shape = shape;

	graph->priv->key_event = g_slist_append (graph->priv->key_event, (gpointer) keyitem);
	return TRUE;
}

/**
 * gpm_graph_widget_key_data_clear:
 **/
gboolean
gpm_graph_widget_key_data_clear (GpmGraphWidget *graph)
{
	GpmGraphWidgetKeyData *keyitem;
	guint a;

	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	/* remove items in list and free */
	for (a=0; a<g_slist_length (graph->priv->key_data); a++) {
		keyitem = (GpmGraphWidgetKeyData *) g_slist_nth_data (graph->priv->key_data, a);
		g_free (keyitem->desc);
		g_free (keyitem);
	}
	g_slist_free (graph->priv->key_data);
	graph->priv->key_data = NULL;

	return TRUE;
}

/**
 * gpm_graph_widget_key_event_clear:
 **/
gboolean
gpm_graph_widget_key_event_clear (GpmGraphWidget *graph)
{
	GpmGraphWidgetKeyItem *keyitem;
	guint a;

	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	/* remove items in list and free */
	for (a=0; a<g_slist_length (graph->priv->key_event); a++) {
		keyitem = (GpmGraphWidgetKeyItem *) g_slist_nth_data (graph->priv->key_event, a);
		g_free (keyitem->desc);
		g_free (keyitem);
	}
	g_slist_free (graph->priv->key_event);
	graph->priv->key_event = NULL;

	return TRUE;
}

/**
 * gpm_graph_widget_key_data_add:
 **/
gboolean
gpm_graph_widget_key_data_add (GpmGraphWidget *graph, guint colour, const gchar *desc)
{
	GpmGraphWidgetKeyData *keyitem;

	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	egg_debug ("add to list %s", desc);
	keyitem = g_new0 (GpmGraphWidgetKeyData, 1);

	keyitem->colour = colour;
	keyitem->desc = g_strdup (desc);

	graph->priv->key_data = g_slist_append (graph->priv->key_data, (gpointer) keyitem);
	return TRUE;
}


/**
 * gpm_graph_widget_string_to_axis_type:
 * @graph: This class instance
 * @type: The axis type, e.g. "percentage"
 *
 * Return value: The enumerated axis type
 **/
GpmGraphWidgetAxisType
gpm_graph_widget_string_to_axis_type (const gchar *type)
{
	GpmGraphWidgetAxisType ret;

	ret = GPM_GRAPH_WIDGET_TYPE_INVALID;
	if (strcmp (type, "percentage") == 0) {
		ret = GPM_GRAPH_WIDGET_TYPE_PERCENTAGE;
	} else if (strcmp (type, "time") == 0) {
		ret = GPM_GRAPH_WIDGET_TYPE_TIME;
	} else if (strcmp (type, "power") == 0) {
		ret = GPM_GRAPH_WIDGET_TYPE_POWER;
	} else if (strcmp (type, "voltage") == 0) {
		ret = GPM_GRAPH_WIDGET_TYPE_VOLTAGE;
	}

	return ret;
}

/**
 * gpm_graph_widget_set_axis_type_x:
 * @graph: This class instance
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
void
gpm_graph_widget_set_axis_type_x (GpmGraphWidget *graph, GpmGraphWidgetAxisType axis)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->axis_type_x = axis;
}

/**
 * gpm_graph_widget_set_axis_type_y:
 * @graph: This class instance
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
void
gpm_graph_widget_set_axis_type_y (GpmGraphWidget *graph, GpmGraphWidgetAxisType axis)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->axis_type_y = axis;
}

/**
 * gpm_graph_widget_enable_legend:
 * @graph: This class instance
 * @enable: If we should show the legend
 **/
void
gpm_graph_widget_enable_legend (GpmGraphWidget *graph, gboolean enable)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->use_legend = enable;

	gtk_widget_hide (GTK_WIDGET (graph));
	gtk_widget_show (GTK_WIDGET (graph));
}

/**
 * gpm_graph_widget_enable_events:
 * @graph: This class instance
 * @enable: If we should show the legend
 **/
void
gpm_graph_widget_enable_events (GpmGraphWidget *graph, gboolean enable)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->use_events = enable;

	gtk_widget_hide (GTK_WIDGET (graph));
	gtk_widget_show (GTK_WIDGET (graph));
}

/**
 * gpm_graph_widget_class_init:
 * @class: This graph class instance
 **/
static void
gpm_graph_widget_class_init (GpmGraphWidgetClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	widget_class->expose_event = gpm_graph_widget_expose;
	object_class->finalize = gpm_graph_widget_finalize;

	g_type_class_add_private (class, sizeof (GpmGraphWidgetPrivate));
}

/**
 * gpm_graph_widget_init:
 * @graph: This class instance
 **/
static void
gpm_graph_widget_init (GpmGraphWidget *graph)
{
	PangoFontMap *fontmap;
	PangoContext *context;
	PangoFontDescription *desc;

	graph->priv = GPM_GRAPH_WIDGET_GET_PRIVATE (graph);
	graph->priv->start_x = 0;
	graph->priv->start_y = 0;
	graph->priv->stop_x = 60;
	graph->priv->stop_y = 100;
	graph->priv->use_grid = TRUE;
	graph->priv->use_legend = FALSE;
	graph->priv->data_list = g_ptr_array_new ();
	graph->priv->key_data = NULL;
	graph->priv->key_event = NULL;
	graph->priv->axis_type_x = GPM_GRAPH_WIDGET_TYPE_TIME;
	graph->priv->axis_type_y = GPM_GRAPH_WIDGET_TYPE_PERCENTAGE;

	/* do pango stuff */
	fontmap = pango_cairo_font_map_get_default ();
	context = pango_cairo_font_map_create_context (PANGO_CAIRO_FONT_MAP (fontmap));

	/* Comment out as this requires pango 1.16 when FC6 only supports 1.14
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO); */

	graph->priv->layout = pango_layout_new (context);
	desc = pango_font_description_from_string (GPM_GRAPH_WIDGET_FONT);
	pango_layout_set_font_description (graph->priv->layout, desc);
	pango_font_description_free (desc);
}

/**
 * gpm_graph_widget_finalize:
 * @object: This graph class instance
 **/
static void
gpm_graph_widget_finalize (GObject *object)
{
	PangoContext *context;
	GpmGraphWidget *graph = (GpmGraphWidget*) object;

	/* clear key */
	gpm_graph_widget_key_data_clear (graph);
	gpm_graph_widget_key_event_clear (graph);

	g_ptr_array_free (graph->priv->data_list, FALSE);

	context = pango_layout_get_context (graph->priv->layout);
	g_object_unref (graph->priv->layout);
	g_object_unref (context);
	G_OBJECT_CLASS (gpm_graph_widget_parent_class)->finalize (object);
}

/**
 * gpm_graph_widget_data_add:
 * @graph: This class instance
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
gboolean
gpm_graph_widget_data_add (GpmGraphWidget *graph, GpmArray *array)
{
	GpmArrayPoint *point;
	guint length;
	guint i;
	guint oldx;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);
	g_return_val_if_fail (GPM_IS_ARRAY (array), FALSE);

	/* check size is not zero */
	length = gpm_array_get_size (array);
	if (length == 0) {
		egg_warning ("Trying to assign a zero length array");
		return FALSE;
	}

	/* check X is only monotomically increasing */
	oldx = 0;
	for (i=0; i < length; i++) {
		point = gpm_array_get (array, i);
		if (point->x < oldx) {
			/* going backwards! */
			return FALSE;
		}
		oldx = point->x;
	}

	/* always add, never remove in this function */
	g_ptr_array_add (graph->priv->data_list, (gpointer) array);
	return TRUE;
}

/**
 * gpm_graph_widget_data_clear:
 * @graph: This class instance
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
void
gpm_graph_widget_data_clear (GpmGraphWidget *graph)
{
	guint i;
	guint length;
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	/* remove all in list */
	length = graph->priv->data_list->len;
	for (i=0; i < length; i++) {
		egg_debug ("Removing dataset %i", i);
		g_ptr_array_remove_index_fast (graph->priv->data_list, 0);
	}
}

/**
 * gpm_graph_widget_events_add:
 * @graph: This class instance
 * @list: The GList events to be plotted on the graph
 *
 * Sets the data for the graph. You MUST NOT free the array before the widget.
 **/
void
gpm_graph_widget_events_add (GpmGraphWidget *graph, GpmArray *array)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	graph->priv->events = array;
}

/**
 * gpm_graph_widget_events_clear:
 * @graph: This class instance
 *
 * Clears the data for the graph.
 **/
void
gpm_graph_widget_events_clear (GpmGraphWidget *graph)
{
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	/* this is managed externally, so just set to NULL */
	graph->priv->events = NULL;
}

/**
 * gpm_get_axis_label:
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 * @value: The data value, e.g. 120
 *
 * Unit is:
 * GPM_GRAPH_WIDGET_TYPE_TIME:		seconds
 * GPM_GRAPH_WIDGET_TYPE_POWER: 	mWh (not mAh)
 * GPM_GRAPH_WIDGET_TYPE_PERCENTAGE:	%
 *
 * Return value: a string value depending on the axis type and the value.
 **/
static gchar *
gpm_get_axis_label (GpmGraphWidgetAxisType axis, gint value)
{
	char *text = NULL;
	if (axis == GPM_GRAPH_WIDGET_TYPE_TIME) {
		int minutes = value / 60;
		int seconds = value - (minutes * 60);
		int hours = minutes / 60;
		minutes =  minutes - (hours * 60);
		if (hours > 0) {
			if (minutes == 0) {
				/*Translators: This is %i hours*/
				text = g_strdup_printf (_("%ih"), hours);
			} else {
				/*Translators: This is %i hours %02i minutes*/
				text = g_strdup_printf (_("%ih%02i"), hours, minutes);
			}
		} else if (minutes > 0) {
			if (seconds == 0) {
				/*Translators: This is %2i minutes*/
				text = g_strdup_printf (_("%2im"), minutes);
			} else {
				/*Translators: This is %2i minutes %02i seconds*/
				text = g_strdup_printf (_("%2im%02i"), minutes, seconds);
			}
		} else {
			/*Translators: This is %2i seconds*/
			text = g_strdup_printf (_("%2is"), seconds);
		}
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		/*Translators: This is %i Percentage*/
		text = g_strdup_printf (_("%i%%"), value);
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_POWER) {
		/*Translators: This is %.1f Watts*/
		text = g_strdup_printf (_("%.1fW"), (gfloat) value / 1000.0);
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		/*Translators: This is %.1f Volts*/
		text = g_strdup_printf (_("%.1fV"), (gfloat) value / 1000.0);
	} else {
		text = g_strdup_printf ("%i", value);
	}
	return text;
}

/**
 * gpm_graph_widget_draw_grid:
 * @graph: This class instance
 * @cr: Cairo drawing context
 *
 * Draw the 10x10 dotted grid onto the graph.
 **/
static void
gpm_graph_widget_draw_grid (GpmGraphWidget *graph, cairo_t *cr)
{
	gfloat a, b;
	gdouble dotted[] = {1., 2.};
	gfloat divwidth  = (gfloat)graph->priv->box_width / 10.0f;
	gfloat divheight = (gfloat)graph->priv->box_height / 10.0f;

	cairo_save (cr);

	cairo_set_line_width (cr, 1);
	cairo_set_dash (cr, dotted, 2, 0.0);

	/* do vertical lines */
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	for (a=1; a<10; a++) {
		b = graph->priv->box_x + (a * divwidth);
		cairo_move_to (cr, (gint)b + 0.5f, graph->priv->box_y);
		cairo_line_to (cr, (gint)b + 0.5f, graph->priv->box_y + graph->priv->box_height);
		cairo_stroke (cr);
	}

	/* do horizontal lines */
	for (a=1; a<10; a++) {
		b = graph->priv->box_y + (a * divheight);
		cairo_move_to (cr, graph->priv->box_x, (gint)b + 0.5f);
		cairo_line_to (cr, graph->priv->box_x + graph->priv->box_width, (int)b + 0.5f);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_widget_draw_labels:
 * @graph: This class instance
 * @cr: Cairo drawing context
 *
 * Draw the X and the Y labels onto the graph.
 **/
static void
gpm_graph_widget_draw_labels (GpmGraphWidget *graph, cairo_t *cr)
{
	gfloat a, b;
	gchar *text;
	gint value;
	gfloat divwidth  = (gfloat)graph->priv->box_width / 10.0f;
	gfloat divheight = (gfloat)graph->priv->box_height / 10.0f;
	gint length_x = graph->priv->stop_x - graph->priv->start_x;
	gint length_y = graph->priv->stop_y - graph->priv->start_y;
	PangoRectangle ink_rect, logical_rect;
	gfloat offsetx = 0;
	gfloat offsety = 0;

	cairo_save (cr);

	/* do x text */
	cairo_set_source_rgb (cr, 0, 0, 0);
	for (a=0; a<11; a++) {
		b = graph->priv->box_x + (a * divwidth);
		value = ((length_x / 10) * a) + graph->priv->start_x;
		text = gpm_get_axis_label (graph->priv->axis_type_x, value);

		pango_layout_set_text (graph->priv->layout, text, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);
		/* have data points 0 and 10 bounded, but 1..9 centered */
		if (a == 0) {
			offsetx = 2.0;
		} else if (a == 10) {
			offsetx = ink_rect.width;
		} else {
			offsetx = (ink_rect.width / 2.0f);
		}

		cairo_move_to (cr, b - offsetx,
			       graph->priv->box_y + graph->priv->box_height + 2.0);

		pango_cairo_show_layout (cr, graph->priv->layout);
		g_free (text);
	}

	/* do y text */
	for (a=0; a<11; a++) {
		b = graph->priv->box_y + (a * divheight);
		value = (length_y / 10) * (10 - a) + graph->priv->start_y;
		text = gpm_get_axis_label (graph->priv->axis_type_y, value);

		pango_layout_set_text (graph->priv->layout, text, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);

		/* have data points 0 and 10 bounded, but 1..9 centered */
		if (a == 10) {
			offsety = 0;
		} else if (a == 0) {
			offsety = ink_rect.height;
		} else {
			offsety = (ink_rect.height / 2.0f);
		}
		offsetx = ink_rect.width + 7;
		offsety -= 10;
		cairo_move_to (cr, graph->priv->box_x - offsetx - 2, b + offsety);
		pango_cairo_show_layout (cr, graph->priv->layout);
		g_free (text);
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_widget_get_y_label_max_width:
 * @graph: This class instance
 * @cr: Cairo drawing context
 *
 * Draw the X and the Y labels onto the graph.
 **/
static guint
gpm_graph_widget_get_y_label_max_width (GpmGraphWidget *graph, cairo_t *cr)
{
	gfloat a, b;
	gchar *text;
	gint value;
	gfloat divheight = (gfloat)graph->priv->box_height / 10.0f;
	gint length_y = graph->priv->stop_y - graph->priv->start_y;
	PangoRectangle ink_rect, logical_rect;
	guint biggest = 0;

	/* do y text */
	for (a=0; a<11; a++) {
		b = graph->priv->box_y + (a * divheight);
		value = (length_y / 10) * (10 - a) + graph->priv->start_y;
		text = gpm_get_axis_label (graph->priv->axis_type_y, value);
		pango_layout_set_text (graph->priv->layout, text, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);
		if (ink_rect.width > biggest) {
			biggest = ink_rect.width;
		}
		g_free (text);
	}
	return biggest;
}

/**
 * gpm_graph_widget_auto_range:
 * @graph: This class instance
 *
 * Autoranges the graph axis depending on the axis type, and the maximum
 * value of the data. We have to be careful to choose a number that gives good
 * resolution but also a number that scales "well" to a 10x10 grid.
 **/
static void
gpm_graph_widget_auto_range (GpmGraphWidget *graph)
{
	gint biggest_x = 0;
	gint biggest_y = 0;
	gint smallest_x = 999999;
	gint smallest_y = 999999;
	guint rounding_x = 1;
	guint rounding_y = 1;
	GpmArray *array;
	GpmArrayPoint *point;
	guint i;
	guint j;
	guint length;

	if (graph->priv->data_list->len == 0) {
		egg_debug ("no data");
		graph->priv->start_x = 0;
		graph->priv->start_y = 0;
		graph->priv->stop_x = 10;
		graph->priv->stop_y = 10;
		return;
	}

	/* get the range for all graphs */
	for (i=0; i<graph->priv->data_list->len; i++) {
		array = g_ptr_array_index (graph->priv->data_list, i);
		length = gpm_array_get_size (array);
		for (j=0; j < length; j++) {
			point = gpm_array_get (array, j);
			if (point->x > biggest_x) {
				biggest_x = point->x;
			}
			if (point->y > biggest_y) {
				biggest_y = point->y;
			}
			if (point->x < smallest_x) {
				smallest_x = point->x;
			}
			if (point->y < smallest_y) {
				smallest_y = point->y;
			}
		}
	}
	egg_debug ("Data range is %i<x<%i, %i<y<%i", smallest_x, biggest_x, smallest_y, biggest_y);

	if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		rounding_x = 10;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_POWER) {
		rounding_x = 1000;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		rounding_x = 1000;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (biggest_x < 150) {
			rounding_x = 150;
		} else if (biggest_x < 5*60) {
			rounding_x = 5 * 60;
		} else {
			rounding_x = 10 * 60;
		}
	}
	if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		rounding_y = 10;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_POWER) {
		rounding_y = 1000;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		rounding_y = 1000;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (biggest_y < 150) {
			rounding_y = 150;
		} else if (biggest_y < 5*60) {
			rounding_y = 5 * 60;
		} else {
			rounding_y = 10 * 60;
		}
	}

	graph->priv->start_x = gpm_precision_round_down (smallest_x, rounding_x);
	graph->priv->start_y = gpm_precision_round_down (smallest_y, rounding_y);
	graph->priv->stop_x = gpm_precision_round_up (biggest_x, rounding_x);
	graph->priv->stop_y = gpm_precision_round_up (biggest_y, rounding_y);

	/* if percentage, and close to the end points, then extend */
	if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		if (graph->priv->stop_x >= 90) {
			graph->priv->stop_x = 100;
		}
		if (graph->priv->start_x <= 10) {
			graph->priv->start_x = 0;
		}
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (graph->priv->start_x <= 60*10) {
			graph->priv->start_x = 0;
		}
	}
	if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		if (graph->priv->stop_y >= 90) {
			graph->priv->stop_y = 100;
		}
		if (graph->priv->start_y <= 10) {
			graph->priv->start_y = 0;
		}
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (graph->priv->start_y <= 60*10) {
			graph->priv->start_y = 0;
		}
	}

	egg_debug ("Processed range is %i<x<%i, %i<y<%i",
		   graph->priv->start_x, graph->priv->stop_x,
		   graph->priv->start_y, graph->priv->stop_y);
}

/**
 * gpm_graph_widget_set_colour:
 * @cr: Cairo drawing context
 * @colour: The colour enum
 **/
static void
gpm_graph_widget_set_colour (cairo_t *cr, guint32 colour)
{
	guint8 r, g, b;
	egg_color_to_rgb (colour, &r, &g, &b);
	cairo_set_source_rgb (cr, ((gdouble) r)/256.0f, ((gdouble) g)/256.0f, ((gdouble) b)/256.0f);
}

/**
 * gpm_graph_widget_draw_dot:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the center
 * @y: The Y-coordinate for the center
 * @colour: The colour enum
 * @shape: The shape enum
 *
 * Draw the dot on the graph of a specified colour
 **/
static void
gpm_graph_widget_draw_dot (cairo_t             *cr,
			   gfloat               x,
			   gfloat               y,
			   guint32              colour,
			   GpmGraphWidgetShape  shape)
{
	gfloat width;
	if (shape == GPM_GRAPH_WIDGET_SHAPE_CIRCLE) {
		/* circle */
		cairo_arc (cr, (gint)x + 0.5f, (gint)y + 0.5f, 4, 0, 2*M_PI);
		gpm_graph_widget_set_colour (cr, colour);
		cairo_fill (cr);
		cairo_arc (cr, (gint)x + 0.5f, (gint)y + 0.5f, 4, 0, 2*M_PI);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	} else if (shape == GPM_GRAPH_WIDGET_SHAPE_SQUARE) {
		/* box */
		width = 8.0;
		cairo_rectangle (cr, (gint)x + 0.5f - (width/2), (gint)y + 0.5f - (width/2), width, width);
		gpm_graph_widget_set_colour (cr, colour);
		cairo_fill (cr);
		cairo_rectangle (cr, (gint)x + 0.5f - (width/2), (gint)y + 0.5f - (width/2), width, width);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	} else if (shape == GPM_GRAPH_WIDGET_SHAPE_DIAMOND) {
		/* diamond */
		width = 4.0;
		cairo_new_path (cr);
		cairo_move_to (cr, x+0.5, y-width+0.5);
		cairo_line_to (cr, x+width+0.5, y+0.5);
		cairo_line_to (cr, x+0.5, y+width+0.5);
		cairo_line_to (cr, x-width+0.5, y+0.5);
		cairo_close_path (cr);
		gpm_graph_widget_set_colour (cr, colour);
		cairo_fill (cr);
		cairo_new_path (cr);
		cairo_move_to (cr, x+0.5, y-width+0.5);
		cairo_line_to (cr, x+width+0.5, y+0.5);
		cairo_line_to (cr, x+0.5, y+width+0.5);
		cairo_line_to (cr, x-width+0.5, y+0.5);
		cairo_close_path (cr);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	} else if (shape == GPM_GRAPH_WIDGET_SHAPE_TRIANGLE) {
		/* triangle */
		width = 4.0;
		cairo_new_path (cr);
		cairo_move_to (cr, x+0.5, y-width+0.5);
		cairo_line_to (cr, x+width+0.5, y+width+0.5-1.0);
		cairo_line_to (cr, x-width+0.5, y+width+0.5-1.0);
		cairo_close_path (cr);
		gpm_graph_widget_set_colour (cr, colour);
		cairo_fill (cr);
		cairo_new_path (cr);
		cairo_move_to (cr, x+0.5, y-width+0.5);
		cairo_line_to (cr, x+width+0.5, y+width+0.5-1.0);
		cairo_line_to (cr, x-width+0.5, y+width+0.5-1.0);
		cairo_close_path (cr);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	} else {
		egg_warning ("shape %i not recognised!", shape);
	}
}

/**
 * gpm_graph_widget_draw_legend_line:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the center
 * @y: The Y-coordinate for the center
 * @colour: The colour enum
 *
 * Draw the legend line on the graph of a specified colour
 **/
static void
gpm_graph_widget_draw_legend_line (cairo_t *cr, gfloat x, gfloat y, guint32 colour)
{
	gfloat width = 10;
	gfloat height = 2;
	/* background */
	cairo_rectangle (cr, (int) (x - (width/2)) + 0.5, (int) (y - (height/2)) + 0.5, width, height);
	gpm_graph_widget_set_colour (cr, colour);
	cairo_fill (cr);
	/* solid outline box */
	cairo_rectangle (cr, (int) (x - (width/2)) + 0.5, (int) (y - (height/2)) + 0.5, width, height);
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

/**
 * gpm_graph_widget_get_pos_on_graph:
 * @graph: This class instance
 * @data_x: The data X-coordinate
 * @data_y: The data Y-coordinate
 * @x: The returned X position on the cairo surface
 * @y: The returned Y position on the cairo surface
 **/
static void
gpm_graph_widget_get_pos_on_graph (GpmGraphWidget *graph, gfloat data_x, gfloat data_y, float *x, float *y)
{
	*x = graph->priv->box_x + (graph->priv->unit_x * (data_x - graph->priv->start_x)) + 1;
	*y = graph->priv->box_y + (graph->priv->unit_y * (gfloat)(graph->priv->stop_y - data_y)) + 1.5;
}

/**
 * gpm_graph_widget_draw_line:
 * @graph: This class instance
 * @cr: Cairo drawing context
 *
 * Draw the data line onto the graph with a big green line. We should already
 * limit the data to < ~100 values, so this shouldn't take too long.
 **/
static void
gpm_graph_widget_draw_line (GpmGraphWidget *graph, cairo_t *cr)
{
	gfloat oldx, oldy;
	gfloat newx, newy;
	guint j;
	guint length;
	GpmArray *array;
	GpmArrayPoint *point;
	guint i;

	if (graph->priv->data_list->len == 0) {
		egg_debug ("no data");
		return;
	}
	cairo_save (cr);

	/* do all the lines on the graphs */
	for (i=0; i<graph->priv->data_list->len; i++) {
		egg_debug ("drawing line %i", i);
		array = g_ptr_array_index (graph->priv->data_list, i);

		/* we have no data */
		if (array == NULL) {
			break;
		}

		/* get the very first point so we can work out the old */
		point = gpm_array_get (array, 0);
		oldx = 0;
		oldy = 0;
		gpm_graph_widget_get_pos_on_graph (graph, point->x, point->y, &oldx, &oldy);

		length = gpm_array_get_size (array);
		for (j=1; j < length; j++) {
			point = gpm_array_get (array, j);

			/* draw line */
			gpm_graph_widget_get_pos_on_graph (graph, point->x, point->y, &newx, &newy);
			cairo_move_to (cr, oldx, oldy);
			cairo_line_to (cr, newx, newy);
			cairo_set_line_width (cr, 1.5);
			gpm_graph_widget_set_colour (cr, point->data);
			cairo_stroke (cr);
			/* save old */
			oldx = newx;
			oldy = newy;
		}
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_widget_draw_event_dots:
 * @graph: This class instance
 * @cr: Cairo drawing context
 *
 * Draw the data line onto the graph with a big green line. We should already
 * limit the data to < ~100 values, so this shouldn't take too long.
 **/
static void
gpm_graph_widget_draw_event_dots (GpmGraphWidget *graph, cairo_t *cr)
{
	gfloat newx, newy;
	GpmGraphWidgetKeyItem *keyitem;
	guint i;
	guint length;
	GpmArray *array = NULL;
	GpmArrayPoint *point;
	gint dot;
	gint prevpos = -1;
	guint previous_y = 0;

	if (graph->priv->events == NULL) {
		/* we have no events */
		return;
	}

	cairo_save (cr);

	length = gpm_array_get_size (graph->priv->events);

	/* always use the first data array */
	if (graph->priv->data_list->len > 0) {
		array = g_ptr_array_index (graph->priv->data_list, 0);
	}

	for (i=0; i < length; i++) {
		point = gpm_array_get (graph->priv->events, i);
		if (point == NULL) {
			/* this shouldn't ever happen */
			egg_warning ("point NULL!");
			break;
		}
		/* try to position the point on the line, or at zero if there is no line */
		if (array == NULL) {
			dot = 0;
		} else {
			dot = gpm_array_interpolate (array, point->x);
		}
		gpm_graph_widget_get_pos_on_graph (graph, point->x, dot, &newx, &newy);

		/* don't overlay the points, stack vertically */
		if (abs (newx - prevpos) < 10) {
			newy = previous_y - 8;
		}

		/* save the last y point */
		previous_y = newy;

		/* only do the event dot, if it's going to be valid on the graph */
		if (point->x > graph->priv->start_x && newy > graph->priv->box_y) {
			keyitem = gpm_graph_widget_key_find_id (graph, point->y);
			if (keyitem == NULL) {
				egg_warning ("did not find id %i", point->y);
			} else {
				gpm_graph_widget_draw_dot (cr, newx, newy, keyitem->colour, keyitem->shape);
			}
		}
		prevpos = newx;
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_widget_draw_bounding_box:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the top-left
 * @y: The Y-coordinate for the top-left
 * @width: The item width
 * @height: The item height
 **/
static void
gpm_graph_widget_draw_bounding_box (cairo_t *cr,
				    gint     x,
				    gint     y,
				    gint     width,
				    gint     height)
{
	/* background */
	cairo_rectangle (cr, x, y, width, height);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_fill (cr);
	/* solid outline box */
	cairo_rectangle (cr, x + 0.5f, y + 0.5f, width - 1, height - 1);
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

/**
 * gpm_graph_widget_draw_legend:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the top-left
 * @y: The Y-coordinate for the top-left
 * @width: The item width
 * @height: The item height
 **/
static void
gpm_graph_widget_draw_legend (GpmGraphWidget *graph,
			      gint     x,
			      gint     y,
			      gint     width,
			      gint     height)
{
	cairo_t *cr = graph->priv->cr;
	gint y_count;
	gint a;
	GpmGraphWidgetKeyData *keydataitem;
	GpmGraphWidgetKeyItem *keyeventitem;

	gpm_graph_widget_draw_bounding_box (cr, x, y, width, height);
	y_count = y + 10;

	/* add the line colours to the legend */
	for (a=0; a<g_slist_length (graph->priv->key_data); a++) {
		keydataitem = (GpmGraphWidgetKeyData *) g_slist_nth_data (graph->priv->key_data, a);
		if (keydataitem == NULL) {
			/* this shouldn't ever happen */
			egg_warning ("keydataitem NULL!");
			break;
		}
		gpm_graph_widget_draw_legend_line (cr, x + 8, y_count, keydataitem->colour);
		cairo_move_to (cr, x + 8 + 10, y_count - 6);
		cairo_set_source_rgb (cr, 0, 0, 0);
		pango_layout_set_text (graph->priv->layout, keydataitem->desc, -1);
		pango_cairo_show_layout (cr, graph->priv->layout);
		y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
	}

	/* add the events to the legend */
	for (a=0; a<g_slist_length (graph->priv->key_event); a++) {
		keyeventitem = (GpmGraphWidgetKeyItem *) g_slist_nth_data (graph->priv->key_event, a);
		if (keyeventitem == NULL) {
			/* this shouldn't ever happen */
			egg_warning ("keyeventitem NULL!");
			break;
		}
		gpm_graph_widget_draw_dot (cr, x + 8, y_count,
					   keyeventitem->colour, keyeventitem->shape);
		cairo_move_to (cr, x + 8 + 10, y_count - 6);
		pango_layout_set_text (graph->priv->layout, keyeventitem->desc, -1);
		pango_cairo_show_layout (cr, graph->priv->layout);
		y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
	}
}

/**
 * gpm_graph_widget_legend_calculate_width:
 * @graph: This class instance
 * @cr: Cairo drawing context
 * Return value: The width of the legend, including borders.
 *
 * We have to find the maximum size of the text so we know the width of the
 * legend box. We can't hardcode this as the dpi or font size might differ
 * from machine to machine.
 **/
static gboolean
gpm_graph_widget_legend_calculate_size (GpmGraphWidget *graph, cairo_t *cr,
					guint *width, guint *height)
{
	guint a;
	PangoRectangle ink_rect, logical_rect;
	GpmGraphWidgetKeyData *keydataitem;
	GpmGraphWidgetKeyItem *keyeventitem;

	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	/* set defaults */
	*width = 0;
	*height = 0;

	/* add the line colours to the legend */
	for (a=0; a<g_slist_length (graph->priv->key_data); a++) {
		keydataitem = (GpmGraphWidgetKeyData *) g_slist_nth_data (graph->priv->key_data, a);
		*height = *height + GPM_GRAPH_WIDGET_LEGEND_SPACING;
		pango_layout_set_text (graph->priv->layout, keydataitem->desc, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);
		if (*width < ink_rect.width) {
			*width = ink_rect.width;
		}
	}

	/* add the events to the legend */
	for (a=0; a<g_slist_length (graph->priv->key_event); a++) {
		keyeventitem = (GpmGraphWidgetKeyItem *) g_slist_nth_data (graph->priv->key_event, a);
		*height = *height + GPM_GRAPH_WIDGET_LEGEND_SPACING;
		pango_layout_set_text (graph->priv->layout, keyeventitem->desc, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);
		if (*width < ink_rect.width) {
			*width = ink_rect.width;
		}
	}

	/* have we got no entries? */
	if (*width == 0 && *height == 0) {
		return TRUE;
	}

	/* add for borders */
	*width += 25;
	*height += 3;

	return TRUE;
}

/**
 * gpm_graph_widget_draw_graph:
 * @graph: This class instance
 * @cr: Cairo drawing context
 *
 * Draw the complete graph, with the box, the grid, the labels and the line.
 **/
static void
gpm_graph_widget_draw_graph (GtkWidget *graph_widget, cairo_t *cr)
{
	gint legend_x = 0;
	gint legend_y = 0;
	guint legend_height = 0;
	guint legend_width = 0;
	gint data_x;
	gint data_y;

	GpmGraphWidget *graph = (GpmGraphWidget*) graph_widget;
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	gpm_graph_widget_legend_calculate_size (graph, cr, &legend_width, &legend_height);

	cairo_save (cr);

	/* we need this so we know the y text */
	gpm_graph_widget_auto_range (graph);

	graph->priv->box_x = gpm_graph_widget_get_y_label_max_width (graph, cr) + 10;
	graph->priv->box_y = 5;

	graph->priv->box_height = graph_widget->allocation.height - (20 + graph->priv->box_y);

	/* make size adjustment for legend */
	if (graph->priv->use_legend && legend_height > 0) {
		graph->priv->box_width = graph_widget->allocation.width -
					 (3 + legend_width + 5 + graph->priv->box_x);
		legend_x = graph->priv->box_x + graph->priv->box_width + 6;
		legend_y = graph->priv->box_y;
	} else {
		graph->priv->box_width = graph_widget->allocation.width -
					 (3 + graph->priv->box_x);
	}

	/* graph background */
	gpm_graph_widget_draw_bounding_box (cr, graph->priv->box_x, graph->priv->box_y,
				     graph->priv->box_width, graph->priv->box_height);

	if (graph->priv->use_grid) {
		gpm_graph_widget_draw_grid (graph, cr);
	}

	/* -3 is so we can keep the lines inside the box at both extremes */
	data_x = graph->priv->stop_x - graph->priv->start_x;
	data_y = graph->priv->stop_y - graph->priv->start_y;
	graph->priv->unit_x = (float)(graph->priv->box_width - 3) / (float) data_x;
	graph->priv->unit_y = (float)(graph->priv->box_height - 3) / (float) data_y;

	gpm_graph_widget_draw_labels (graph, cr);
	gpm_graph_widget_draw_line (graph, cr);

	if (graph->priv->use_events) {
		gpm_graph_widget_draw_event_dots (graph, cr);
	}

	if (graph->priv->use_legend && legend_height > 0) {
		gpm_graph_widget_draw_legend (graph, legend_x, legend_y, legend_width, legend_height);
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_widget_expose:
 * @graph: This class instance
 * @event: The expose event
 *
 * Just repaint the entire graph widget on expose.
 **/
static gboolean
gpm_graph_widget_expose (GtkWidget *graph, GdkEventExpose *event)
{
	cairo_t *cr;

	/* get a cairo_t */
	cr = gdk_cairo_create (graph->window);
	cairo_rectangle (cr,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);
	cairo_clip (cr);
	((GpmGraphWidget *)graph)->priv->cr = cr;

	gpm_graph_widget_draw_graph (graph, cr);

	cairo_destroy (cr);
	return FALSE;
}

/**
 * gpm_graph_widget_new:
 * Return value: A new GpmGraphWidget object.
 **/
GtkWidget *
gpm_graph_widget_new (void)
{
	return g_object_new (GPM_TYPE_GRAPH_WIDGET, NULL);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef GPM_BUILD_TESTS
#include "gpm-self-test.h"

GtkWidget *window;
GtkWidget *graph;
GtkWidget *label;

static gint
close_handler (GtkWidget *widget, gpointer gdata)
{
	gtk_main_quit ();
	return FALSE;
}

static void
clicked_passed_cb (GtkWidget *widget, gpointer gdata)
{
	GpmSelfTest *test = (GpmSelfTest *) gdata;
	gpm_st_success (test, NULL);
	gtk_main_quit ();
}

static void
clicked_failed_cb (GtkWidget *widget, gpointer gdata)
{
	GpmSelfTest *test = (GpmSelfTest *) gdata;
	gpm_st_failed (test, NULL);
	gtk_main_quit ();
}

static void
create_graph_window (GpmSelfTest *test)
{
	GtkWidget *button_passed;
	GtkWidget *button_failed;
	GtkWidget *vbox;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	graph = gpm_graph_widget_new ();
	vbox = gtk_vbox_new (FALSE, 0);
	label = gtk_label_new("Title");

	button_passed = gtk_button_new_with_label("Passed");
	gtk_signal_connect(GTK_OBJECT(button_passed), "clicked", GTK_SIGNAL_FUNC(clicked_passed_cb), test);
	button_failed = gtk_button_new_with_label("Failed");
	gtk_signal_connect(GTK_OBJECT(button_failed), "clicked", GTK_SIGNAL_FUNC(clicked_failed_cb), test);

	gtk_widget_set_size_request (graph, 600, 300);
	gtk_signal_connect (GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(close_handler), test);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), graph, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button_passed, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button_failed, FALSE, FALSE, 0);
	gtk_container_border_width (GTK_CONTAINER (window), 0);

	gtk_widget_show (vbox);
	gtk_widget_show (label);
	gtk_widget_show (button_passed);
	gtk_widget_show (button_failed);
}

static void
wait_for_input (GpmSelfTest *test)
{
	gtk_widget_hide_all (window);
	gtk_widget_show_all (window);
	gtk_main ();
}

void
gpm_st_title_graph (GpmSelfTest *test, const gchar *format, ...)
{
	va_list args;
	gchar va_args_buffer [1025];
	va_start (args, format);
	g_vsnprintf (va_args_buffer, 1024, format, args);
	va_end (args);
	gpm_st_title (test, va_args_buffer);
	gtk_label_set_label (GTK_LABEL (label), va_args_buffer);
//	g_print ("> check #%u\t%s: \t%s...", test->total+1, test->type, va_args_buffer);
//	test->total++;
}

void
gpm_st_graph_widget (GpmSelfTest *test)
{
	GpmArray *data;
	GpmArray *data_more;
	GpmArray *events;
	gboolean ret;

	if (gpm_st_start (test, "GpmGraphWidget") == FALSE) {
		return;
	}

	create_graph_window (test);
	gpm_graph_widget_enable_legend (GPM_GRAPH_WIDGET (graph), TRUE);
	gpm_graph_widget_enable_events (GPM_GRAPH_WIDGET (graph), TRUE);

	/********** TYPES *************/
	gpm_st_title_graph (test, "graph loaded, visible, no key, and set to y=percent x=time");
	wait_for_input (test);

	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	gpm_graph_widget_set_axis_type_y (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_TIME);

	gpm_st_title_graph (test, "now set to y=time x=percent");
	wait_for_input (test);

	/********** KEY DATA *************/
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), EGG_COLOR_RED, "red data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), EGG_COLOR_GREEN, "green data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), EGG_COLOR_BLUE, "blue data");

	gpm_st_title_graph (test, "red green blue key data added");
	wait_for_input (test);

	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (graph));

	gpm_st_title_graph (test, "data items cleared, no key remains");
	wait_for_input (test);

	/********** KEY EVENT *************/
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 0,
					EGG_COLOR_RED,
					GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
					"red circle");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 1,
					EGG_COLOR_GREEN,
					GPM_GRAPH_WIDGET_SHAPE_SQUARE,
					"green square");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 2,
					EGG_COLOR_BLUE,
					GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
					"blue triangle");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 3,
					EGG_COLOR_WHITE,
					GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
					"white diamond");

	gpm_st_title_graph (test, "red green blue white key events added");
	wait_for_input (test);


	/********** KEY EVENT duplicate test *************/
	gpm_st_title (test, "duplicate key event test");
	ret = gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 3,
					      EGG_COLOR_WHITE,
					      GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
					      "white diamond");
	if (!ret) {
		gpm_st_success (test, "refused duplicate id");
	} else {
		gpm_st_failed (test, "added duplicate ID!");
	}

	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (graph));

	gpm_st_title_graph (test, "event items cleared, no key remains");
	wait_for_input (test);

	/********** DATA *************/
	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	gpm_graph_widget_set_axis_type_y (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);

	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), EGG_COLOR_RED, "red data");
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), EGG_COLOR_BLUE, "blue data");
	gpm_graph_widget_key_event_add (GPM_GRAPH_WIDGET (graph), 0, EGG_COLOR_GREEN, GPM_GRAPH_WIDGET_SHAPE_SQUARE, "green square");
	
	/********** ADD INVALID DATA *************/
	data = gpm_array_new ();
	gpm_array_append (data, 50, 0, EGG_COLOR_RED);
	gpm_array_append (data, 40, 100, EGG_COLOR_RED);
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_st_title (test, "add invalid data");
	ret = gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	if (!ret) {
		gpm_st_success (test, "ignored");
	} else {
		gpm_st_failed (test, "failed to ignore invalid data");
	}
	g_object_unref (data);

	/********** ADD NO DATA *************/
	data = gpm_array_new ();
	gpm_st_title (test, "add zero data");
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	ret = gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	if (!ret) {
		gpm_st_success (test, "ignored");
	} else {
		gpm_st_failed (test, "failed to ignore zero data");
	}
	g_object_unref (data);

	/********** ADD VALID DATA *************/
	data = gpm_array_new ();
	gpm_array_append (data, 0, 0, EGG_COLOR_RED);
	gpm_array_append (data, 100, 100, EGG_COLOR_RED);
	gpm_st_title (test, "add valid data");
	ret = gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	if (ret) {
		gpm_st_success (test, NULL);
	} else {
		gpm_st_failed (test, "failed to add valid data");
	}

	/********** SHOW VALID DATA *************/
	gpm_st_title_graph (test, "red line shown gradient up");
	wait_for_input (test);

	/*********** second line **************/
	data_more = gpm_array_new ();
	gpm_array_append (data_more, 0, 100, EGG_COLOR_BLUE);
	gpm_array_append (data_more, 100, 0, EGG_COLOR_BLUE);
	gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data_more);

	gpm_st_title_graph (test, "red line shown gradient up, blue gradient down");
	wait_for_input (test);

	/*********** dots **************/
	events = gpm_array_new ();
	gpm_array_append (events, 25, 0, 0);
	gpm_array_append (events, 50, 0, 0);
	gpm_array_append (events, 75, 0, 0);
	gpm_graph_widget_events_add (GPM_GRAPH_WIDGET (graph), events);

	gpm_st_title_graph (test, "events follow red line (primary)");
	wait_for_input (test);

	/*********** stacked dots **************/
	gpm_array_append (events, 76, 0, 0);
	gpm_array_append (events, 77, 0, 0);
	gpm_graph_widget_events_add (GPM_GRAPH_WIDGET (graph), events);

	gpm_st_title_graph (test, "three events stacked at ~75");
	wait_for_input (test);

	/*********** events removed **************/
	gpm_graph_widget_events_clear (GPM_GRAPH_WIDGET (graph));
	gpm_st_title_graph (test, "events removed");
	wait_for_input (test);

	/*********** data lines removed **************/
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_st_title_graph (test, "all lines and event removed");
	wait_for_input (test);

	g_object_unref (events);
	g_object_unref (data);
	g_object_unref (data_more);

	/********** AUTORANGING PERCENT (close) *************/
	gpm_graph_widget_set_axis_type_x (GPM_GRAPH_WIDGET (graph), GPM_GRAPH_WIDGET_TYPE_PERCENTAGE);
	gpm_graph_widget_key_event_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_key_data_add (GPM_GRAPH_WIDGET (graph), EGG_COLOR_RED, "red data");
	data = gpm_array_new ();
	gpm_array_append (data, 0, 75, EGG_COLOR_RED);
	gpm_array_append (data, 20, 78, EGG_COLOR_RED);
	gpm_array_append (data, 40, 74, EGG_COLOR_RED);
	gpm_array_append (data, 60, 72, EGG_COLOR_RED);
	gpm_array_append (data, 80, 78, EGG_COLOR_RED);
	gpm_array_append (data, 100, 79, EGG_COLOR_RED);
	gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	gpm_st_title_graph (test, "autorange y axis between 70 and 80");
	wait_for_input (test);
	g_object_unref (data);

	/********** AUTORANGING PERCENT (extremes) *************/
	data = gpm_array_new ();
	gpm_array_append (data, 0, 6, EGG_COLOR_RED);
	gpm_array_append (data, 100, 85, EGG_COLOR_RED);
	gpm_graph_widget_data_clear (GPM_GRAPH_WIDGET (graph));
	gpm_graph_widget_data_add (GPM_GRAPH_WIDGET (graph), data);
	gpm_st_title_graph (test, "autorange y axis between 0 and 100");
	wait_for_input (test);
	g_object_unref (data);

	/* hide window */
	gtk_widget_hide_all (window);

	gpm_st_end (test);
}

#endif

