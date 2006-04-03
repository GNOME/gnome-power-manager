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
#include <gtk/gtk.h>
#include <math.h>

#include "gpm-graph-widget.h"
#include "gpm-debug.h"

G_DEFINE_TYPE (GpmGraph, gpm_graph, GTK_TYPE_DRAWING_AREA);
#define GPM_GRAPH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_GRAPH, GpmGraphPrivate))

struct GpmGraphPrivate
{
	gboolean	use_grid;
	gboolean	use_legend;

	gboolean	invert_x;
	gboolean	invert_y;

	gint		stop_x;
	gint		stop_y;

	gint		box_x; /* size of the white box, not the widget */
	gint		box_y;
	gint		box_width;
	gint		box_height;

	float		unit_x; /* 10th width of graph */
	float		unit_y; /* 10th width of graph */

	GpmGraphAxisType	 axis_x;
	GpmGraphAxisType	 axis_y;
	cairo_font_options_t	*options;

	GList		*list;
};

static gboolean gpm_graph_expose (GtkWidget *graph, GdkEventExpose *event);
static void	gpm_graph_finalize (GObject *object);


/**
 * gpm_graph_event_description:
 * @event: The event type, e.g. GPM_GRAPH_EVENT_SCREEN_DIM
 * @colour: The colour enum, e.g. GPM_GRAPH_COLOUR_DARK_BLUE (returned)
 **/
const char *
gpm_graph_event_description (GpmGraphEvent    event,
			     GpmGraphColour  *colour)
{
	const char *event_desc;
	if (event == GPM_GRAPH_EVENT_AC_REMOVED) {
		event_desc = "AC power";
		*colour = GPM_GRAPH_COLOUR_BLUE;
	} else if (event == GPM_GRAPH_EVENT_LOW_POWER) {
		event_desc = "Low Power";
		*colour = GPM_GRAPH_COLOUR_DARK_BLUE;
	} else if (event == GPM_GRAPH_EVENT_SCREEN_DIM) {
		event_desc = "Screen dim";
		*colour = GPM_GRAPH_COLOUR_YELLOW;
	} else if (event == GPM_GRAPH_EVENT_DPMS_OFF) {
		event_desc = "Screen off";
		*colour = GPM_GRAPH_COLOUR_DARK_YELLOW;
	} else if (event == GPM_GRAPH_EVENT_SUSPEND) {
		event_desc = "Suspend";
		*colour = GPM_GRAPH_COLOUR_RED;
	} else if (event == GPM_GRAPH_EVENT_HIBERNATE) {
		event_desc = "Hibernate";
		*colour = GPM_GRAPH_COLOUR_DARK_RED;
	} else {
		event_desc = NULL;
		*colour = 0;
	}
	return event_desc;
}

/**
 * gpm_graph_set_axis_x:
 * @graph: This simple graph class instance
 * @axis: The axis type, e.g. GPM_GRAPH_TYPE_TIME
 **/
void
gpm_graph_set_axis_x (GpmGraph *graph, GpmGraphAxisType axis)
{
	graph->priv->axis_x = axis;
}

/**
 * gpm_graph_set_axis_y:
 * @graph: This simple graph class instance
 * @axis: The axis type, e.g. GPM_GRAPH_TYPE_TIME
 **/
void
gpm_graph_set_axis_y (GpmGraph *graph, GpmGraphAxisType axis)
{
	graph->priv->axis_y = axis;
}

/**
 * gpm_graph_class_init:
 * @class: This simple graph class instance
 **/
static void
gpm_graph_class_init (GpmGraphClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	widget_class->expose_event = gpm_graph_expose;
	object_class->finalize = gpm_graph_finalize;

	g_type_class_add_private (class, sizeof (GpmGraphPrivate));
}

/**
 * gpm_graph_init:
 * @graph: This simple graph class instance
 **/
static void
gpm_graph_init (GpmGraph *graph)
{
	graph->priv = GPM_GRAPH_GET_PRIVATE (graph);
	graph->priv->invert_x = FALSE;
	graph->priv->invert_y = FALSE;
	graph->priv->stop_x = 60;
	graph->priv->stop_y = 100;
	graph->priv->use_grid = TRUE;
	graph->priv->use_legend = TRUE;
	graph->priv->list = NULL;
	graph->priv->axis_x = GPM_GRAPH_TYPE_TIME;
	graph->priv->axis_y = GPM_GRAPH_TYPE_PERCENTAGE;
	/* setup font */
	graph->priv->options = cairo_font_options_create ();
}

/**
 * gpm_graph_finalize:
 * @object: This simple graph class instance
 **/
static void
gpm_graph_finalize (GObject *object)
{
	GpmGraph *graph = (GpmGraph*) object;
	cairo_font_options_destroy (graph->priv->options);
}

/**
 * gpm_graph_set_invert_x:
 * @graph: This simple graph class instance
 * @inv: If we should invert the axis
 *
 * Sets the inverse policy for the X axis, i.e. to count from 0..X or X..0
 **/
void
gpm_graph_set_invert_x (GpmGraph *graph, gboolean inv)
{
	graph->priv->invert_x = inv;
}

/**
 * gpm_graph_set_invert_y:
 * @graph: This simple graph class instance
 * @inv: If we should invert the axis
 *
 * Sets the inverse policy for the Y axis, i.e. to count from 0..Y or Y..0
 **/
void
gpm_graph_set_invert_y (GpmGraph *graph, gboolean inv)
{
	graph->priv->invert_y = inv;
}

/**
 * gpm_graph_set_data:
 * @graph: This simple graph class instance
 * @list: The GList values to be plotted on the graph
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
void
gpm_graph_set_data (GpmGraph *graph, GList *list)
{
	graph->priv->list = list;
}

/**
 * gpm_get_axis_label:
 * @axis: The axis type, e.g. GPM_GRAPH_TYPE_TIME
 * @value: The data value, e.g. 120
 *
 * Unit is:
 * GPM_GRAPH_TYPE_TIME:		seconds
 * GPM_GRAPH_TYPE_RATE: 	mWh (not mAh)
 * GPM_GRAPH_TYPE_PERCENTAGE:	%
 *
 * Return value: a string value depending on the axis type and the value.
 **/
static char *
gpm_get_axis_label (GpmGraphAxisType axis, int value)
{
	char *text = NULL;
	if (axis == GPM_GRAPH_TYPE_TIME) {
		int minutes = value / 60;
		int seconds = value - (minutes * 60);
		int hours = minutes / 60;
		minutes =  minutes - (hours * 60);
		if (hours > 0) {
			if (minutes == 0) {
				text = g_strdup_printf ("%ih", hours);
			} else {
				text = g_strdup_printf ("%ih%02i", hours, minutes);
			}
		} else if (minutes > 0) {
			if (seconds == 0) {
				text = g_strdup_printf ("%2im", minutes);
			} else {
				text = g_strdup_printf ("%2im%02i", minutes, seconds);
			}
		} else {
			text = g_strdup_printf ("%2is", seconds);
		}
	} else if (axis == GPM_GRAPH_TYPE_PERCENTAGE) {
		text = g_strdup_printf ("%i%%", value);
	} else if (axis == GPM_GRAPH_TYPE_RATE) {
		text = g_strdup_printf ("%iW", value / 1000);
	} else {
		text = g_strdup_printf ("%i??", value);
	}
	return text;
}

/**
 * gpm_graph_draw_grid:
 * @graph: This simple graph class instance
 * @cr: Cairo drawing context
 *
 * Draw the 10x10 dotted grid onto the graph.
 **/
static void
gpm_graph_draw_grid (GpmGraph *graph, cairo_t *cr)
{
	float a, b;
	double dotted[] = {1., 2.};
	float divwidth  = (float)graph->priv->box_width / 10.0f;
	float divheight = (float)graph->priv->box_height / 10.0f;

	cairo_save (cr);

	cairo_set_line_width (cr, 1);
	cairo_set_dash (cr, dotted, 2, 0.0);

	/* do vertical lines */
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	for (a=1; a<10; a++) {
		b = graph->priv->box_x + (a * divwidth);
		cairo_move_to (cr, (int)b + 0.5f, graph->priv->box_y);
		cairo_line_to (cr, (int)b + 0.5f, graph->priv->box_y + graph->priv->box_height);
		cairo_stroke (cr);
	}

	/* do horizontal lines */
	for (a=1; a<10; a++) {
		b = graph->priv->box_y + (a * divheight);
		cairo_move_to (cr, graph->priv->box_x, (int)b + 0.5f);
		cairo_line_to (cr, graph->priv->box_x + graph->priv->box_width, (int)b + 0.5f);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_draw_labels:
 * @graph: This simple graph class instance
 * @cr: Cairo drawing context
 *
 * Draw the X and the Y labels onto the graph.
 **/
static void
gpm_graph_draw_labels (GpmGraph *graph, cairo_t *cr)
{
	float a, b;
	gchar *text;
	gint value;
	float divwidth  = (float)graph->priv->box_width / 10.0f;
	float divheight = (float)graph->priv->box_height / 10.0f;
	gint length_x = graph->priv->stop_x;
	gint length_y = graph->priv->stop_y;
	cairo_text_extents_t extents;
	float offsetx = 0;
	float offsety = 0;

	cairo_save (cr);

	cairo_set_font_options (cr, graph->priv->options);

	/* do x text */
	cairo_set_source_rgb (cr, 0, 0, 0);
	for (a=0; a<11; a++) {
		b = graph->priv->box_x + (a * divwidth);
		if (graph->priv->invert_x) {
			value = (length_x / 10) * (10 - a);
		} else {
			value = (length_x / 10) * a;
		}
		text = gpm_get_axis_label (graph->priv->axis_x, value);

		cairo_text_extents (cr, text, &extents);
		/* have data points 0 and 10 bounded, but 1..9 centered */
		if (a == 0) {
			offsetx = 2;
		} else if (a == 10) {
			offsetx = extents.width;
		} else {
			offsetx = (extents.width / 2.0f);
		}

		cairo_move_to (cr, b - offsetx,
			       graph->priv->box_y + graph->priv->box_height + 15);

		cairo_show_text (cr, text);
		g_free (text);
	}

	/* do y text */
	for (a=0; a<11; a++) {
		b = graph->priv->box_y + (a * divheight);
		if (graph->priv->invert_y) {
			value = (length_y / 10) * a;
		} else {
			value = (length_y / 10) * (10 - a);
		}
		text = gpm_get_axis_label (graph->priv->axis_y, value);

		cairo_text_extents (cr, text, &extents);
		/* have data points 0 and 10 bounded, but 1..9 centered */
		if (a == 10) {
			offsety = 0;
		} else if (a == 0) {
			offsety = extents.height;
		} else {
			offsety = (extents.height / 2.0f);
		}
		offsetx = extents.width + 5;
		cairo_move_to (cr, graph->priv->box_x - offsetx, b + offsety);
		cairo_show_text (cr, text);
		g_free (text);
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_check_range:
 * @graph: This simple graph class instance
 *
 * Checks all points are displayable on the graph, nobbling if required.
 **/
static void
gpm_graph_check_range (GpmGraph *graph)
{
	if (! graph->priv->list) {
		gpm_debug ("no data");
		return;
	}
	GpmDataPoint *new = NULL;
	GList *l;
	for (l=graph->priv->list; l != NULL; l=l->next) {
		new = (GpmDataPoint *) l->data;
		if (new->x < 0) {
			gpm_warning ("point out of range (x=%i)", new->x);
			new->x = 0;
		}
		if (new->x > graph->priv->stop_x) {
			gpm_warning ("point out of range (x=%i)", new->x);
			new->x = graph->priv->stop_x;
		}
		if (new->y < 0) {
			gpm_warning ("point out of range (y=%i)", new->y);
			new->y = 0;
		}
		if (new->y > graph->priv->stop_y) {
			gpm_warning ("point out of range (y=%i)", new->y);
			new->y = graph->priv->stop_y;
		}
	}
}

/**
 * gpm_graph_auto_range:
 * @graph: This simple graph class instance
 *
 * Autoranges the graph axis depending on the axis type, and the maximum
 * value of the data. We have to be careful to choose a number that gives good
 * resolution but also a number that scales "well" to a 10x10 grid.
 **/
static void
gpm_graph_auto_range (GpmGraph *graph)
{
	if (! graph->priv->list) {
		gpm_debug ("no data");
		return;
	}

	int biggest_x = 0;
	int biggest_y = 0;
	GpmDataPoint *new = NULL;
	GList *l;
	for (l=graph->priv->list; l != NULL; l=l->next) {
		new = (GpmDataPoint *) l->data;
		if (new->x > biggest_x) {
			biggest_x = new->x;
		}
		if (new->y > biggest_y) {
			biggest_y = new->y;
		}
	}

	/* x */
	if (graph->priv->axis_x == GPM_GRAPH_TYPE_PERCENTAGE) {
		graph->priv->stop_x = 100;
	} else if (graph->priv->axis_x == GPM_GRAPH_TYPE_TIME) {
		graph->priv->stop_x = ((biggest_x/60)+1)*60;
		if (graph->priv->stop_x > 60) {
			graph->priv->stop_x = ((biggest_x/(10*60))+1)*(10*60);
		}
	} else if (graph->priv->axis_x == GPM_GRAPH_TYPE_RATE) {
		graph->priv->stop_x = ((biggest_x/10000)+2)*10000;
		if (graph->priv->stop_x < 10000) {
			graph->priv->stop_x = 10000;
		}
	} else {
		graph->priv->stop_x = ((biggest_x/10)+1)*10;
	}

	/* y */
	if (graph->priv->axis_y == GPM_GRAPH_TYPE_PERCENTAGE) {
		graph->priv->stop_y = 100;
	} else if (graph->priv->axis_y == GPM_GRAPH_TYPE_TIME) {
		graph->priv->stop_y = ((biggest_y/60)+2)*60;
		if (graph->priv->stop_y > 60) {
			graph->priv->stop_y = ((biggest_y/(10*60))+2)*(10*60);
		}
	} else if (graph->priv->axis_y == GPM_GRAPH_TYPE_RATE) {
		graph->priv->stop_y = ((biggest_y/10000)+2)*10000;
		if (graph->priv->stop_y < 10000) {
			graph->priv->stop_y = 10000;
		}
	} else {
		graph->priv->stop_y = ((biggest_y/10)+1)*10;
	}
}

/**
 * gpm_graph_set_colour:
 * @cr: Cairo drawing context
 * @colour: The colour enum
 **/
static void
gpm_graph_set_colour (cairo_t *cr, GpmGraphColour colour)
{
	if (colour == GPM_GRAPH_COLOUR_DEFAULT) {
		cairo_set_source_rgb (cr, 0, 0.7, 0);
	} else if (colour == GPM_GRAPH_COLOUR_BLACK) {
		cairo_set_source_rgb (cr, 0, 0, 0);
	} else if (colour == GPM_GRAPH_COLOUR_WHITE) {
		cairo_set_source_rgb (cr, 1, 1, 1);
	} else if (colour == GPM_GRAPH_COLOUR_RED) {
		cairo_set_source_rgb (cr, 1, 0, 0);
	} else if (colour == GPM_GRAPH_COLOUR_BLUE) {
		cairo_set_source_rgb (cr, 0, 0, 1);
	} else if (colour == GPM_GRAPH_COLOUR_PURPLE) {
		cairo_set_source_rgb (cr, 1, 1, 0);
	} else if (colour == GPM_GRAPH_COLOUR_YELLOW) {
		cairo_set_source_rgb (cr, 0, 1, 1);
	} else if (colour == GPM_GRAPH_COLOUR_DARK_RED) {
		cairo_set_source_rgb (cr, 0.5, 0, 0);
	} else if (colour == GPM_GRAPH_COLOUR_DARK_BLUE) {
		cairo_set_source_rgb (cr, 0, 0, 0.5);
	} else if (colour == GPM_GRAPH_COLOUR_DARK_PURPLE) {
		cairo_set_source_rgb (cr, 0.5, 0.5, 0);
	} else if (colour == GPM_GRAPH_COLOUR_DARK_YELLOW) {
		cairo_set_source_rgb (cr, 0, 0.5, 0.5);
	} else {
		gpm_critical_error ("Unknown colour!");
	}
}

/**
 * gpm_graph_draw_dot:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the center
 * @y: The Y-coordinate for the center
 * @colour: The colour enum
 *
 * Draw the dot on the graph of a specified colour
 **/
static void
gpm_graph_draw_dot (cairo_t *cr, float x, float y, GpmGraphColour colour)
{
	cairo_arc (cr, (int)x + 0.5f, (int)y + 0.5f, 4, 0, 2*M_PI);
	gpm_graph_set_colour (cr, colour);
	cairo_fill (cr);
	cairo_arc (cr, (int)x + 0.5f, (int)y + 0.5f, 4, 0, 2*M_PI);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

/**
 * gpm_graph_get_pos_on_graph:
 * @graph: This simple graph class instance
 * @data_x: The data X-coordinate
 * @data_y: The data Y-coordinate
 * @x: The returned X position on the cairo surface
 * @y: The returned Y position on the cairo surface
 **/
static void
gpm_graph_get_pos_on_graph (GpmGraph *graph, float data_x, float data_y, float *x, float *y)
{
	*x = graph->priv->box_x + (graph->priv->unit_x * data_x) + 1;
	*y = graph->priv->box_y + (graph->priv->unit_y * (float)(graph->priv->stop_y - data_y)) + 1.5;
}

/**
 * gpm_graph_draw_line:
 * @graph: This simple graph class instance
 * @cr: Cairo drawing context
 *
 * Draw the data line onto the graph with a big green line. We should already
 * limit the data to < 100 values, so this shouldn't take too long.
 **/
static void
gpm_graph_draw_line (GpmGraph *graph, cairo_t *cr)
{
	float oldx, oldy;
	float newx, newy;

	int length_x = graph->priv->stop_x;
	int length_y = graph->priv->stop_y;

	GList *link;
	GpmDataPoint *old;

	/* -3 is so we can keep the lines inside the box at both extremes */
	graph->priv->unit_x = (float)(graph->priv->box_width - 3) / (float) length_x;
	graph->priv->unit_y = (float)(graph->priv->box_height - 3) / (float) length_y;

	if (! graph->priv->list) {
		gpm_debug ("no data");
		return;
	}

	gpm_graph_check_range (graph);

	cairo_save (cr);

	/* do the line */
	link = graph->priv->list;
	old = link->data;
	gpm_graph_get_pos_on_graph (graph, old->x, old->y, &oldx, &oldy);
	while (link->next && link->next->data) {
		GpmDataPoint *new = link->next->data;
		if (! new->info_point) {
			/* do line */
			gpm_graph_get_pos_on_graph (graph, new->x, new->y, &newx, &newy);
			cairo_move_to (cr, oldx, oldy);
			cairo_line_to (cr, newx, newy);
			cairo_set_line_width (cr, 2);
			gpm_graph_set_colour (cr, new->colour);
			cairo_stroke (cr);
			/* save old */
			oldx = newx;
			oldy = newy;
		}
		link=link->next;
	}

	/* do the events on the graph */
	link = graph->priv->list;
	old = link->data;
	gpm_graph_get_pos_on_graph (graph, old->x, old->y, &oldx, &oldy);
	while (link->next && link->next->data) {
		GpmDataPoint *new = link->next->data;
		if (new->info_point) {
			gpm_graph_draw_dot (cr, oldx, oldy, new->colour);
		} else {
			/* save old */
			gpm_graph_get_pos_on_graph (graph, new->x, new->y, &newx, &newy);
			oldx = newx;
			oldy = newy;
		}
		link=link->next;
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_draw_bounding_box:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the top-left
 * @y: The Y-coordinate for the top-left
 * @width: The item width
 * @height: The item height
 **/
static void
gpm_graph_draw_bounding_box (cairo_t *cr, int x, int y, int width, int height)
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
 * gpm_graph_draw_legend:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the top-left
 * @y: The Y-coordinate for the top-left
 * @width: The item width
 * @height: The item height
 **/
static void
gpm_graph_draw_legend (cairo_t *cr, int x, int y, int width, int height)
{
	const char *desc;
	int y_count;
	int a;
	GpmGraphColour colour;

	gpm_graph_draw_bounding_box (cr, x, y, width, height);
	y_count = y + 10;
	for (a=0; a<GPM_GRAPH_EVENT_LAST; a++) {
		desc = 	gpm_graph_event_description (a, &colour);
		gpm_graph_draw_dot (cr, x + 8, y_count, colour);
		cairo_move_to (cr, x + 8 + 10, y_count + 3);
		cairo_show_text (cr, desc);
		y_count = y_count + 20;
	}
}

/**
 * gpm_graph_draw_graph:
 * @graph: This simple graph class instance
 * @cr: Cairo drawing context
 *
 * Draw the complete graph, with the box, the grid, the labels and the line.
 **/
static void
gpm_graph_draw_graph (GtkWidget *graph_widget, cairo_t *cr)
{
	int legend_x = 0;
	int legend_y = 0;
	int legend_height;
	int legend_width;

	GpmGraph *graph = (GpmGraph*) graph_widget;

	cairo_save (cr);

	graph->priv->box_x = 35;
	graph->priv->box_y = 5;

	if (graph->priv->use_legend) {
		legend_width = 77;
		legend_height = GPM_GRAPH_EVENT_LAST * 20;
		graph->priv->box_width = graph_widget->allocation.width - (3 + legend_width + 5 + graph->priv->box_x);
		legend_x = graph->priv->box_x + graph->priv->box_width + 6;
		legend_y = graph->priv->box_y;
	} else {
		graph->priv->box_width = graph_widget->allocation.width - (3 + graph->priv->box_x);
	}
	graph->priv->box_height = graph_widget->allocation.height - (20 + graph->priv->box_y);

	/* graph background */
	gpm_graph_draw_bounding_box (cr, graph->priv->box_x, graph->priv->box_y,
				     graph->priv->box_width, graph->priv->box_height);

	if (graph->priv->use_grid) {
		gpm_graph_draw_grid (graph, cr);
	}

	gpm_graph_auto_range (graph);

	gpm_graph_draw_labels (graph, cr);
	gpm_graph_draw_line (graph, cr);

	if (graph->priv->use_legend) {
		gpm_graph_draw_legend (cr, legend_x, legend_y, legend_width, legend_height);
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_expose:
 * @graph: This simple graph class instance
 * @event: The expose event
 *
 * Just repaint the entire graph widget on expose.
 **/
static gboolean
gpm_graph_expose (GtkWidget *graph, GdkEventExpose *event)
{
	cairo_t *cr;

	/* get a cairo_t */
	cr = gdk_cairo_create (graph->window);

	cairo_rectangle (cr,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);
	cairo_clip (cr);

	gpm_graph_draw_graph (graph, cr);

	cairo_destroy (cr);
	return FALSE;
}

/**
 * gpm_graph_new:
 * Return value: A new GpmGraph object.
 **/
GtkWidget *
gpm_graph_new (void)
{
	return g_object_new (GPM_TYPE_GRAPH, NULL);
}
