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
#include <stdlib.h>

#include "gpm-common.h"
#include "gpm-point-obj.h"
#include "gpm-graph-widget.h"
#include "egg-debug.h"
#include "egg-color.h"
#include "egg-precision.h"

G_DEFINE_TYPE (GpmGraphWidget, gpm_graph_widget, GTK_TYPE_DRAWING_AREA);
#define GPM_GRAPH_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidgetPrivate))
#define GPM_GRAPH_WIDGET_FONT "Sans 8"

struct GpmGraphWidgetPrivate
{
	gboolean		 use_grid;
	gboolean		 use_legend;
	gboolean		 autorange_x;

	GSList			*key_data; /* lines */

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

	GpmGraphWidgetType	 axis_type_x;
	GpmGraphWidgetType	 axis_type_y;
	gchar			*title;

	cairo_t			*cr;
	PangoLayout 		*layout;

	EggObjList		*data_list;
};

static gboolean gpm_graph_widget_expose (GtkWidget *graph, GdkEventExpose *event);
static void	gpm_graph_widget_finalize (GObject *object);

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
 * gpm_graph_widget_key_data_add:
 **/
gboolean
gpm_graph_widget_key_data_add (GpmGraphWidget *graph, guint32 color, const gchar *desc)
{
	GpmGraphWidgetKeyData *keyitem;

	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	egg_debug ("add to list %s", desc);
	keyitem = g_new0 (GpmGraphWidgetKeyData, 1);

	keyitem->color = color;
	keyitem->desc = g_strdup (desc);

	graph->priv->key_data = g_slist_append (graph->priv->key_data, (gpointer) keyitem);
	return TRUE;
}

/**
 * gpm_graph_widget_set_type_x:
 * @graph: This class instance
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
void
gpm_graph_widget_set_type_x (GpmGraphWidget *graph, GpmGraphWidgetType axis)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->axis_type_x = axis;
}

/**
 * gpm_graph_widget_set_type_y:
 * @graph: This class instance
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
void
gpm_graph_widget_set_type_y (GpmGraphWidget *graph, GpmGraphWidgetType axis)
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
	graph->priv->data_list = egg_obj_list_new ();
	graph->priv->key_data = NULL;
	graph->priv->axis_type_x = GPM_GRAPH_WIDGET_TYPE_TIME;
	graph->priv->axis_type_y = GPM_GRAPH_WIDGET_TYPE_PERCENTAGE;

	/* do pango stuff */
	fontmap = pango_cairo_font_map_get_default ();
	context = pango_cairo_font_map_create_context (PANGO_CAIRO_FONT_MAP (fontmap));
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);

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

	/* free data */
	g_object_unref (graph->priv->data_list);

	context = pango_layout_get_context (graph->priv->layout);
	g_object_unref (graph->priv->layout);
	g_object_unref (context);
	G_OBJECT_CLASS (gpm_graph_widget_parent_class)->finalize (object);
}

/**
 * gpm_graph_widget_data_assign:
 * @graph: This class instance
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
gboolean
gpm_graph_widget_data_assign (GpmGraphWidget *graph, EggObjList *array)
{
	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	/* get the new data */
	g_object_unref (graph->priv->data_list);
	graph->priv->data_list = g_object_ref (array);

	/* refresh */
	gtk_widget_queue_draw (GTK_WIDGET (graph));

	return TRUE;
}

/**
 * gpm_get_axis_label:
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 * @value: The data value, e.g. 120
 *
 * Unit is:
 * GPM_GRAPH_WIDGET_TYPE_TIME:		seconds
 * GPM_GRAPH_WIDGET_TYPE_POWER: 	Wh (not Ah)
 * GPM_GRAPH_WIDGET_TYPE_PERCENTAGE:	%
 *
 * Return value: a string value depending on the axis type and the value.
 **/
static gchar *
gpm_get_axis_label (GpmGraphWidgetType axis, gfloat value)
{
	gchar *text = NULL;
	if (axis == GPM_GRAPH_WIDGET_TYPE_TIME) {
		gint time = abs((gint) value);
		gint minutes = time / 60;
		gint seconds = time - (minutes * 60);
		gint hours = minutes / 60;
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
		text = g_strdup_printf (_("%i%%"), (gint) value);
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_POWER) {
		/*Translators: This is %.1f Watts*/
		text = g_strdup_printf (_("%.1fW"), value);
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_FACTOR) {
		text = g_strdup_printf ("%.1f", value);
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		/*Translators: This is %.1f Volts*/
		text = g_strdup_printf (_("%.1fV"), value);
	} else {
		text = g_strdup_printf ("%i", (gint) value);
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
	gfloat value;
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
		value = ((length_x / 10.0f) * (gfloat) a) + (gfloat) graph->priv->start_x;
		text = gpm_get_axis_label (graph->priv->axis_type_x, value);

		pango_layout_set_text (graph->priv->layout, text, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);
		/* have data points 0 and 10 bounded, but 1..9 centered */
		if (a == 0)
			offsetx = 2.0;
		else if (a == 10)
			offsetx = ink_rect.width;
		else
			offsetx = (ink_rect.width / 2.0f);

		cairo_move_to (cr, b - offsetx,
			       graph->priv->box_y + graph->priv->box_height + 2.0);

		pango_cairo_show_layout (cr, graph->priv->layout);
		g_free (text);
	}

	/* do y text */
	for (a=0; a<11; a++) {
		b = graph->priv->box_y + (a * divheight);
		value = ((gfloat) length_y / 10.0f) * (10 - a) + graph->priv->start_y;
		text = gpm_get_axis_label (graph->priv->axis_type_y, value);

		pango_layout_set_text (graph->priv->layout, text, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);

		/* have data points 0 and 10 bounded, but 1..9 centered */
		if (a == 10)
			offsety = 0;
		else if (a == 0)
			offsety = ink_rect.height;
		else
			offsety = (ink_rect.height / 2.0f);
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
		if (ink_rect.width > biggest)
			biggest = ink_rect.width;
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
	gfloat biggest_x = G_MINFLOAT;
	gfloat biggest_y = G_MINFLOAT;
	gfloat smallest_x = G_MAXFLOAT;
	gfloat smallest_y = G_MAXFLOAT;
	guint rounding_x = 1;
	guint rounding_y = 1;
	EggObjList *array;
	GpmPointObj *point;
	guint i;

	if (graph->priv->data_list->len == 0) {
		egg_debug ("no data");
		graph->priv->start_x = 0;
		graph->priv->start_y = 0;
		graph->priv->stop_x = 10;
		graph->priv->stop_y = 10;
		return;
	}

	/* get the range for the graph */
	array = graph->priv->data_list;
	for (i=0; i < array->len; i++) {
		point = (GpmPointObj *) egg_obj_list_index (array, i);
		if (point->x > biggest_x)
			biggest_x = point->x;
		if (point->y > biggest_y)
			biggest_y = point->y;
		if (point->x < smallest_x)
			smallest_x = point->x;
		if (point->y < smallest_y)
			smallest_y = point->y;
	}
	egg_debug ("Data range is %f<x<%f, %f<y<%f", smallest_x, biggest_x, smallest_y, biggest_y);
	/* don't allow no difference */
	if (biggest_x - smallest_x < 0.0001)
		biggest_x = smallest_x + 1;
	if (biggest_y - smallest_y < 0.0001)
		biggest_y = smallest_y + 1;

	if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		rounding_x = 10;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_FACTOR) {
		rounding_x = 1;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_POWER) {
		rounding_x = 10;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		rounding_x = 1000;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (biggest_x-smallest_x < 150)
			rounding_x = 150;
		else if (biggest_x-smallest_x < 5*60)
			rounding_x = 5 * 60;
		else
			rounding_x = 10 * 60;
	}
	if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		rounding_y = 10;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_FACTOR) {
		rounding_y = 1;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_POWER) {
		rounding_y = 10;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		rounding_y = 1000;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (biggest_y-smallest_y < 150)
			rounding_y = 150;
		else if (biggest_y < 5*60)
			rounding_y = 5 * 60;
		else
			rounding_y = 10 * 60;
	}

	graph->priv->start_x = egg_precision_round_down (smallest_x, rounding_x);
	graph->priv->start_y = egg_precision_round_down (smallest_y, rounding_y);
	graph->priv->stop_x = egg_precision_round_up (biggest_x, rounding_x);
	graph->priv->stop_y = egg_precision_round_up (biggest_y, rounding_y);

	/* a factor graph always is centered around zero */
	if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_FACTOR) {
		if (abs (graph->priv->stop_y) > abs (graph->priv->start_y))
			graph->priv->start_y = -graph->priv->stop_y;
		else
			graph->priv->stop_y = -graph->priv->start_y;
	}

	egg_debug ("Processed(1) range is %i<x<%i, %i<y<%i",
		   graph->priv->start_x, graph->priv->stop_x,
		   graph->priv->start_y, graph->priv->stop_y);

	/* if percentage, and close to the end points, then extend */
	if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		if (graph->priv->stop_x >= 90)
			graph->priv->stop_x = 100;
		if (graph->priv->start_x > 0 && graph->priv->start_x <= 10)
			graph->priv->start_x = 0;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (graph->priv->start_x > 0 && graph->priv->start_x <= 60*10)
			graph->priv->start_x = 0;
	}
	if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		if (graph->priv->stop_y >= 90)
			graph->priv->stop_y = 100;
		if (graph->priv->start_y > 0 && graph->priv->start_y <= 10)
			graph->priv->start_y = 0;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_TIME) {
		if (graph->priv->start_y <= 60*10)
			graph->priv->start_y = 0;
	}

	egg_debug ("Processed range is %i<x<%i, %i<y<%i",
		   graph->priv->start_x, graph->priv->stop_x,
		   graph->priv->start_y, graph->priv->stop_y);
}

/**
 * gpm_graph_widget_set_color:
 * @cr: Cairo drawing context
 * @color: The color enum
 **/
static void
gpm_graph_widget_set_color (cairo_t *cr, guint32 color)
{
	guint8 r, g, b;
	egg_color_to_rgb (color, &r, &g, &b);
	cairo_set_source_rgb (cr, ((gdouble) r)/256.0f, ((gdouble) g)/256.0f, ((gdouble) b)/256.0f);
}

/**
 * gpm_graph_widget_draw_legend_line:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the center
 * @y: The Y-coordinate for the center
 * @color: The color enum
 *
 * Draw the legend line on the graph of a specified color
 **/
static void
gpm_graph_widget_draw_legend_line (cairo_t *cr, gfloat x, gfloat y, guint32 color)
{
	gfloat width = 10;
	gfloat height = 2;
	/* background */
	cairo_rectangle (cr, (int) (x - (width/2)) + 0.5, (int) (y - (height/2)) + 0.5, width, height);
	gpm_graph_widget_set_color (cr, color);
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
 * gpm_graph_widget_draw_dot:
 **/
static void
gpm_graph_widget_draw_dot (cairo_t *cr, gfloat x, gfloat y, guint32 color)
{
	gfloat width;
	/* box */
	width = 2.0;
	cairo_rectangle (cr, (gint)x + 0.5f - (width/2), (gint)y + 0.5f - (width/2), width, width);
	gpm_graph_widget_set_color (cr, color);
	cairo_fill (cr);
	cairo_rectangle (cr, (gint)x + 0.5f - (width/2), (gint)y + 0.5f - (width/2), width, width);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
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
	EggObjList *array;
	GpmPointObj *point;
	guint i;

	if (graph->priv->data_list->len == 0) {
		egg_debug ("no data");
		return;
	}
	cairo_save (cr);

	/* do all the line on the graph */
	array = graph->priv->data_list;

	/* get the very first point so we can work out the old */
	point = (GpmPointObj *) egg_obj_list_index (array, 0);
	oldx = 0;
	oldy = 0;
	gpm_graph_widget_get_pos_on_graph (graph, point->x, point->y, &oldx, &oldy);
	gpm_graph_widget_draw_dot (cr, oldx, oldy, point->color);

	for (i=1; i < array->len; i++) {
		point = (GpmPointObj *) egg_obj_list_index (array, i);

		gpm_graph_widget_get_pos_on_graph (graph, point->x, point->y, &newx, &newy);

		/* ignore white lines */
		if (point->color == 0xffffff) {
			oldx = newx;
			oldy = newy;
			continue;
		}

		/* draw line */
		cairo_move_to (cr, oldx, oldy);
		cairo_line_to (cr, newx, newy);
		cairo_set_line_width (cr, 1.5);
		gpm_graph_widget_set_color (cr, point->color);
		cairo_stroke (cr);

		/* draw data dot */
		gpm_graph_widget_draw_dot (cr, newx, newy, point->color);

		/* save old */
		oldx = newx;
		oldy = newy;
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
gpm_graph_widget_draw_bounding_box (cairo_t *cr, gint x, gint y, gint width, gint height)
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
gpm_graph_widget_draw_legend (GpmGraphWidget *graph, gint x, gint y, gint width, gint height)
{
	cairo_t *cr = graph->priv->cr;
	gint y_count;
	gint a;
	GpmGraphWidgetKeyData *keydataitem;

	gpm_graph_widget_draw_bounding_box (cr, x, y, width, height);
	y_count = y + 10;

	/* add the line colors to the legend */
	for (a=0; a<g_slist_length (graph->priv->key_data); a++) {
		keydataitem = (GpmGraphWidgetKeyData *) g_slist_nth_data (graph->priv->key_data, a);
		if (keydataitem == NULL) {
			/* this shouldn't ever happen */
			egg_warning ("keydataitem NULL!");
			break;
		}
		gpm_graph_widget_draw_legend_line (cr, x + 8, y_count, keydataitem->color);
		cairo_move_to (cr, x + 8 + 10, y_count - 6);
		cairo_set_source_rgb (cr, 0, 0, 0);
		pango_layout_set_text (graph->priv->layout, keydataitem->desc, -1);
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

	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	/* set defaults */
	*width = 0;
	*height = 0;

	/* add the line colors to the legend */
	for (a=0; a<g_slist_length (graph->priv->key_data); a++) {
		keydataitem = (GpmGraphWidgetKeyData *) g_slist_nth_data (graph->priv->key_data, a);
		*height = *height + GPM_GRAPH_WIDGET_LEGEND_SPACING;
		pango_layout_set_text (graph->priv->layout, keydataitem->desc, -1);
		pango_layout_get_pixel_extents (graph->priv->layout, &ink_rect, &logical_rect);
		if (*width < ink_rect.width)
			*width = ink_rect.width;
	}

	/* have we got no entries? */
	if (*width == 0 && *height == 0)
		return TRUE;

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
	gfloat data_x;
	gfloat data_y;

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
	if (graph->priv->use_grid)
		gpm_graph_widget_draw_grid (graph, cr);

	/* -3 is so we can keep the lines inside the box at both extremes */
	data_x = graph->priv->stop_x - graph->priv->start_x;
	data_y = graph->priv->stop_y - graph->priv->start_y;
	graph->priv->unit_x = (float)(graph->priv->box_width - 3) / (float) data_x;
	graph->priv->unit_y = (float)(graph->priv->box_height - 3) / (float) data_y;

	gpm_graph_widget_draw_labels (graph, cr);
	gpm_graph_widget_draw_line (graph, cr);

	if (graph->priv->use_legend && legend_height > 0)
		gpm_graph_widget_draw_legend (graph, legend_x, legend_y, legend_width, legend_height);

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

