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
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gpm-graph-widget.h"
#include "gpm-info-data.h"
#include "gpm-debug.h"

G_DEFINE_TYPE (GpmGraphWidget, gpm_graph_widget, GTK_TYPE_DRAWING_AREA);
#define GPM_GRAPH_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidgetPrivate))

struct GpmGraphWidgetPrivate
{
	gboolean		 use_grid;
	gboolean		 use_legend;
	gboolean		 use_events;

	gboolean		 invert_x;
	gboolean		 autorange_x;
	gboolean		 invert_y;

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

	GpmGraphWidgetAxisType	 axis_x;
	GpmGraphWidgetAxisType	 axis_y;
	cairo_font_options_t	*options;

	GList			*list;
	GList			*events;
};

static gboolean gpm_graph_widget_expose (GtkWidget *graph, GdkEventExpose *event);
static void	gpm_graph_widget_finalize (GObject *object);

/**
 * gpm_graph_widget_event_description:
 * @event: The event type, e.g. GPM_GRAPH_WIDGET_EVENT_SUSPEND
 * Return value: a string value describing the event
 **/
const char *
gpm_graph_widget_event_description (GpmGraphWidgetEvent event)
{
	if (event == GPM_GRAPH_WIDGET_EVENT_ON_AC) {
		return _("On AC");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_ON_BATTERY) {
		return _("On battery");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_SCREEN_DIM) {
		return _("LCD dim");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_SCREEN_RESUME) {
		return _("LCD resume");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_DPMS_OFF) {
		return _("DPMS off");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_DPMS_ON) {
		return _("DPMS on");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_SUSPEND) {
		return _("Suspend");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_RESUME) {
		return _("Resume");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_HIBERNATE) {
		return _("Hibernate");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_LID_CLOSED) {
		return _("Lid closed");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_LID_OPENED) {
		return _("Lid opened");
	} else if (event == GPM_GRAPH_WIDGET_EVENT_NOTIFICATION) {
		return _("Notification");
	} else {
		return _("Unknown event!");
	}
}

/**
 * gpm_graph_widget_event_colour:
 * @event: The event type, e.g. GPM_GRAPH_WIDGET_EVENT_SUSPEND
 * Return value: a colout, e.g. GPM_GRAPH_WIDGET_COLOUR_DARK_BLUE
 **/
GpmGraphWidgetColour
gpm_graph_widget_event_colour (GpmGraphWidgetEvent event)
{
	if (event == GPM_GRAPH_WIDGET_EVENT_ON_AC) {
		return GPM_GRAPH_WIDGET_COLOUR_BLUE;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_ON_BATTERY) {
		return GPM_GRAPH_WIDGET_COLOUR_DARK_BLUE;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_SCREEN_DIM) {
		return GPM_GRAPH_WIDGET_COLOUR_CYAN;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_SCREEN_RESUME) {
		return GPM_GRAPH_WIDGET_COLOUR_DARK_CYAN;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_DPMS_OFF) {
		return GPM_GRAPH_WIDGET_COLOUR_DARK_YELLOW;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_DPMS_ON) {
		return GPM_GRAPH_WIDGET_COLOUR_YELLOW;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_SUSPEND) {
		return GPM_GRAPH_WIDGET_COLOUR_RED;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_RESUME) {
		return GPM_GRAPH_WIDGET_COLOUR_DARK_RED;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_HIBERNATE) {
		return GPM_GRAPH_WIDGET_COLOUR_MAGENTA;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_LID_CLOSED) {
		return GPM_GRAPH_WIDGET_COLOUR_GREEN;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_LID_OPENED) {
		return GPM_GRAPH_WIDGET_COLOUR_DARK_GREEN;
	} else if (event == GPM_GRAPH_WIDGET_EVENT_NOTIFICATION) {
		return GPM_GRAPH_WIDGET_COLOUR_GREY;
	} else {
		return GPM_GRAPH_WIDGET_COLOUR_DEFAULT;
	}
}

/**
 * gpm_graph_widget_string_to_axis_type:
 * @graph: This graph class instance
 * @type: The axis type, e.g. "percentage"
 *
 * Return value: The enumerated axis type
 **/
GpmGraphWidgetAxisType
gpm_graph_widget_string_to_axis_type (const gchar *type)
{
	GpmGraphWidgetAxisType ret;
	g_return_val_if_fail (type != NULL, GPM_GRAPH_WIDGET_TYPE_INVALID);

	ret = GPM_GRAPH_WIDGET_TYPE_INVALID;
	if (strcmp (type, "percentage") == 0) {
		ret = GPM_GRAPH_WIDGET_TYPE_PERCENTAGE;
	} else if (strcmp (type, "time") == 0) {
		ret = GPM_GRAPH_WIDGET_TYPE_TIME;
	} else if (strcmp (type, "power") == 0) {
		ret = GPM_GRAPH_WIDGET_TYPE_POWER;
	}
	
	return ret;
}

/**
 * gpm_graph_widget_set_axis_x:
 * @graph: This graph class instance
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
void
gpm_graph_widget_set_axis_x (GpmGraphWidget *graph, GpmGraphWidgetAxisType axis)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->axis_x = axis;
}

/**
 * gpm_graph_widget_set_axis_y:
 * @graph: This graph class instance
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
void
gpm_graph_widget_set_axis_y (GpmGraphWidget *graph, GpmGraphWidgetAxisType axis)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->axis_y = axis;
}

/**
 * gpm_graph_widget_enable_legend:
 * @graph: This graph class instance
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
 * @graph: This graph class instance
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
 * @graph: This graph class instance
 **/
static void
gpm_graph_widget_init (GpmGraphWidget *graph)
{
	graph->priv = GPM_GRAPH_WIDGET_GET_PRIVATE (graph);
	graph->priv->invert_x = FALSE;
	graph->priv->invert_y = FALSE;
	graph->priv->start_x = 0;
	graph->priv->start_y = 0;
	graph->priv->stop_x = 60;
	graph->priv->stop_y = 100;
	graph->priv->use_grid = TRUE;
	graph->priv->use_legend = FALSE;
	graph->priv->autorange_x = TRUE;
	graph->priv->list = NULL;
	graph->priv->axis_x = GPM_GRAPH_WIDGET_TYPE_TIME;
	graph->priv->axis_y = GPM_GRAPH_WIDGET_TYPE_PERCENTAGE;
	/* setup font */
	graph->priv->options = cairo_font_options_create ();
}

/**
 * gpm_graph_widget_finalize:
 * @object: This graph class instance
 **/
static void
gpm_graph_widget_finalize (GObject *object)
{
	GpmGraphWidget *graph = (GpmGraphWidget*) object;
	cairo_font_options_destroy (graph->priv->options);
	if (graph->priv->events) {
		g_list_free (graph->priv->events);
	}
}

/**
 * gpm_graph_widget_set_invert_x:
 * @graph: This graph class instance
 * @inv: If we should invert the axis
 *
 * Sets the inverse policy for the X axis, i.e. to count from 0..X or X..0
 **/
void
gpm_graph_widget_set_invert_x (GpmGraphWidget *graph, gboolean inv)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->invert_x = inv;
}

/**
 * gpm_graph_widget_set_invert_y:
 * @graph: This graph class instance
 * @inv: If we should invert the axis
 *
 * Sets the inverse policy for the Y axis, i.e. to count from 0..Y or Y..0
 **/
void
gpm_graph_widget_set_invert_y (GpmGraphWidget *graph, gboolean inv)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->invert_y = inv;
}

/**
 * gpm_graph_widget_set_data:
 * @graph: This graph class instance
 * @list: The GList values to be plotted on the graph
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
void
gpm_graph_widget_set_data (GpmGraphWidget *graph, GList *list)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	if (graph->priv->list) {
		g_list_free (graph->priv->list);
	}
	graph->priv->list = list;
}

/**
 * gpm_graph_widget_set_events:
 * @graph: This graph class instance
 * @list: The GList events to be plotted on the graph
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
void
gpm_graph_widget_set_events (GpmGraphWidget *graph, GList *list)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	if (graph->priv->events) {
		g_list_free (graph->priv->events);
	}
	graph->priv->events = list;
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
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		text = g_strdup_printf ("%i%%", value);
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_POWER) {
		text = g_strdup_printf ("%iW", value / 1000);
	} else {
		text = g_strdup_printf ("%i??", value);
	}
	return text;
}

/**
 * gpm_graph_widget_draw_grid:
 * @graph: This graph class instance
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
 * @graph: This graph class instance
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
	cairo_text_extents_t extents;
	gfloat offsetx = 0;
	gfloat offsety = 0;

	cairo_save (cr);

	cairo_set_font_options (cr, graph->priv->options);

	/* do x text */
	cairo_set_source_rgb (cr, 0, 0, 0);
	for (a=0; a<11; a++) {
		b = graph->priv->box_x + (a * divwidth);
		if (graph->priv->invert_x) {
			value = (length_x / 10) * (10 - a) + graph->priv->start_x;
		} else {
			value = ((length_x / 10) * a) + graph->priv->start_x;
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
			value = (length_y / 10) * a - graph->priv->start_y;
		} else {
			value = (length_y / 10) * (10 - a) - graph->priv->start_y;
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
 * gpm_graph_widget_check_range:
 * @graph: This graph class instance
 *
 * Checks all points are displayable on the graph, nobbling if required.
 **/
static void
gpm_graph_widget_check_range (GpmGraphWidget *graph)
{
	GpmInfoDataPoint *new = NULL;
	GList *l;

	if (graph->priv->list == NULL || graph->priv->list->data == NULL) {
		gpm_debug ("no data");
		return;
	}
	for (l=graph->priv->list; l != NULL; l=l->next) {
		new = (GpmInfoDataPoint *) l->data;
		if (new->time < graph->priv->start_x) {
			gpm_warning ("point out of range (x=%i)", new->time);
			new->time = graph->priv->start_x;
		}
		if (new->time > graph->priv->stop_x) {
			gpm_warning ("point out of range (x=%i)", new->time);
			new->time = graph->priv->stop_x;
		}
		if (new->value < graph->priv->start_y) {
			gpm_warning ("point out of range (y=%i)", new->value);
			new->value = graph->priv->start_y;
		}
		if (new->value > graph->priv->stop_y) {
			gpm_warning ("point out of range (y=%i)", new->value);
			new->value = graph->priv->stop_y;
		}
	}
}

/**
 * gpm_graph_widget_auto_range:
 * @graph: This graph class instance
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
	GpmInfoDataPoint *new = NULL;
	GList *l;

	if (graph->priv->list == NULL || graph->priv->list->data == NULL) {
		gpm_debug ("no data");
		graph->priv->start_x = 0;
		graph->priv->start_y = 0;
		graph->priv->stop_x = 10;
		graph->priv->stop_y = 10;
		return;
	}

	for (l=graph->priv->list; l != NULL; l=l->next) {
		new = (GpmInfoDataPoint *) l->data;
		if (new->time > biggest_x) {
			biggest_x = new->time;
		}
		if (new->value > biggest_y) {
			biggest_y = new->value;
		}
		if (new->time < smallest_x) {
			smallest_x = new->time;
		}
		if (new->value < smallest_y) {
			smallest_y = new->value;
		}
	}

	/* do we autorange the start (so it starts at non-zero)? */
	if (graph->priv->autorange_x) {
		/* x is always time and always autoranges to the minute scale */
		smallest_x = (smallest_x / 60) * 60;
		if (smallest_x < 60) {
			smallest_x = 0;
		}
		graph->priv->start_x = smallest_x;
	} else {
		graph->priv->start_x = 0;
	}

	/* x */
	if (graph->priv->axis_x == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		graph->priv->stop_x = 100;
	} else if (graph->priv->axis_x == GPM_GRAPH_WIDGET_TYPE_TIME) {
		graph->priv->stop_x = (((biggest_x - smallest_x) / (10 * 60)) + 1) * (10 * 60) + smallest_x;
	} else if (graph->priv->axis_x == GPM_GRAPH_WIDGET_TYPE_POWER) {
		graph->priv->stop_x = (((biggest_x - smallest_x) / 10000) + 2) * 10000 + smallest_x;
		if (graph->priv->stop_x < 10000) {
			graph->priv->stop_x = 10000 + smallest_x;
		}
	} else {
		graph->priv->stop_x = ((biggest_x / 10) + 1) * 10 + smallest_x;
	}

	/* y */
	if (graph->priv->axis_y == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		graph->priv->start_y = 0;
		graph->priv->stop_y = 100;
	} else if (graph->priv->axis_y == GPM_GRAPH_WIDGET_TYPE_TIME) {
		graph->priv->start_y = 0;
		graph->priv->stop_y = ((biggest_y / 60) + 2)* 60;
		if (graph->priv->stop_y > 60) {
			graph->priv->stop_y = ((biggest_y / (10 * 60)) + 2) * (10 * 60);
		}
		if (graph->priv->stop_y < 60) {
			graph->priv->stop_y = 60;
		}
	} else if (graph->priv->axis_y == GPM_GRAPH_WIDGET_TYPE_POWER) {
		graph->priv->start_y = 0;
		graph->priv->stop_y = ((biggest_y / 10000) + 2) * 10000;
		if (graph->priv->stop_y < 10000) {
			graph->priv->stop_y = 10000;
		}
	} else {
		graph->priv->start_y = 0;
		graph->priv->stop_y = ((biggest_y / 10) + 1) * 10;
		if (graph->priv->stop_y < 10) {
			graph->priv->stop_y = 10;
		}
	}
}

/**
 * gpm_graph_widget_set_colour:
 * @cr: Cairo drawing context
 * @colour: The colour enum
 **/
static void
gpm_graph_widget_set_colour (cairo_t *cr, GpmGraphWidgetColour colour)
{
	if (colour == GPM_GRAPH_WIDGET_COLOUR_DEFAULT) {
		cairo_set_source_rgb (cr, 0, 0.7, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_BLACK) {
		cairo_set_source_rgb (cr, 0, 0, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_WHITE) {
		cairo_set_source_rgb (cr, 1, 1, 1);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_RED) {
		cairo_set_source_rgb (cr, 1, 0, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_BLUE) {
		cairo_set_source_rgb (cr, 0, 0, 1);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_GREEN) {
		cairo_set_source_rgb (cr, 0, 1, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_MAGENTA) {
		cairo_set_source_rgb (cr, 1, 0, 1);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_YELLOW) {
		cairo_set_source_rgb (cr, 1, 1, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_CYAN) {
		cairo_set_source_rgb (cr, 0, 1, 1);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_GREY) {
		cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_DARK_RED) {
		cairo_set_source_rgb (cr, 0.5, 0, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_DARK_BLUE) {
		cairo_set_source_rgb (cr, 0, 0, 0.5);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_DARK_GREEN) {
		cairo_set_source_rgb (cr, 0, 0.5, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_DARK_MAGENTA) {
		cairo_set_source_rgb (cr, 0.5, 0, 0.5);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_DARK_YELLOW) {
		cairo_set_source_rgb (cr, 0.5, 0.5, 0);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_DARK_CYAN) {
		cairo_set_source_rgb (cr, 0, 0.5, 0.5);
	} else if (colour == GPM_GRAPH_WIDGET_COLOUR_DARK_GREY) {
		cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
	} else {
		gpm_critical_error ("Unknown colour!");
	}
}

/**
 * gpm_graph_widget_draw_dot:
 * @cr: Cairo drawing context
 * @x: The X-coordinate for the center
 * @y: The Y-coordinate for the center
 * @colour: The colour enum
 *
 * Draw the dot on the graph of a specified colour
 **/
static void
gpm_graph_widget_draw_dot (cairo_t *cr, gfloat x, gfloat y, GpmGraphWidgetColour colour)
{
	cairo_arc (cr, (gint)x + 0.5f, (gint)y + 0.5f, 4, 0, 2*M_PI);
	gpm_graph_widget_set_colour (cr, colour);
	cairo_fill (cr);
	cairo_arc (cr, (gint)x + 0.5f, (gint)y + 0.5f, 4, 0, 2*M_PI);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
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
gpm_graph_widget_draw_legend_line (cairo_t *cr, gfloat x, gfloat y, GpmGraphWidgetColour colour)
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
 * @graph: This graph class instance
 * @data_x: The data X-coordinate
 * @data_y: The data Y-coordinate
 * @x: The returned X position on the cairo surface
 * @y: The returned Y position on the cairo surface
 **/
static void
gpm_graph_widget_get_pos_on_graph (GpmGraphWidget *graph, gfloat data_x, gfloat data_y, float *x, float *y)
{
	*x = graph->priv->box_x + (graph->priv->unit_x * (data_x - graph->priv->start_x)) + 1;
	*y = graph->priv->box_y + (graph->priv->unit_y * (gfloat)(graph->priv->stop_y - (data_y - graph->priv->start_y))) + 1.5;
}

/**
 * gpm_graph_widget_interpolate_value:
 * @this: The first data point
 * @last: The other data point
 * @xintersect: The x value (i.e. the x we provide)
 * Return value: The interpolated value, or 0 if invalid
 *
 * Interpolates onto the graph in the y direction. If only supplied one point
 * then don't interpolate.
 **/
static gint
gpm_graph_widget_interpolate_value (GpmInfoDataPoint *this,
			     GpmInfoDataPoint *last,
			     gint xintersect)
{
	gint dy, dx;
	gfloat m;
	gint c;
	gint y;

	/* we have no points */
	if (! this) {
		return 0;
	}

	/* we only have one point, don't interpolate */
	if (! last) {
		return this->value;
	}

	/* gradient */
	dx = this->time - last->time;
	dy = this->value - last->value;
	m = (gfloat) dy / (gfloat) dx;

	/* y-intersect */
	c = (-m * (gfloat) this->time) + this->value;

	/* y = mx + c */
	y = (m * (gfloat) xintersect) + c;

	/* limit the y intersect to the last height, so we don't extend the
	 * graph into the unknown */
	if (y > this->value) {
		y = this->value;
	}
	return y;
}

/**
 * gpm_graph_widget_draw_line:
 * @graph: This graph class instance
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
	GpmInfoDataPoint *eventdata;
	GList *l;
	GpmInfoDataPoint *new;

	if (graph->priv->list == NULL || graph->priv->list->data == NULL) {
		gpm_debug ("no data");
		return;
	}

	/* I don't think we need to do this anymore.... */
	if (FALSE) {
		gpm_graph_widget_check_range (graph);
	}

	cairo_save (cr);

	/* do the line on the graph */
	l=graph->priv->list;
	new = (GpmInfoDataPoint *) l->data;
	gpm_graph_widget_get_pos_on_graph (graph, new->time, new->value, &oldx, &oldy);
	for (l=l->next; l != NULL; l=l->next) {
		new = (GpmInfoDataPoint *) l->data;
		/* do line */
		gpm_graph_widget_get_pos_on_graph (graph, new->time, new->value, &newx, &newy);
		cairo_move_to (cr, oldx, oldy);
		cairo_line_to (cr, newx, newy);
		cairo_set_line_width (cr, 2);
		gpm_graph_widget_set_colour (cr, new->colour);
		cairo_stroke (cr);
		/* save old */
		oldx = newx;
		oldy = newy;
	}

	/* only do the events sometimes */
	if (graph->priv->use_events) {
		gint previous_point = 0;
		gint prevpos = -1;
		GpmInfoDataPoint *point_this = NULL;
		GpmInfoDataPoint *point_last = NULL;

		/* we track the list so we can put the point on the line */
		GList *l2 = graph->priv->list;
		if (l2) {
			point_this = (GpmInfoDataPoint *) l2->data;
			l2 = l2->next;
		}
		for (l=graph->priv->events; l != NULL; l=l->next) {
			gint pos;

			eventdata = (GpmInfoDataPoint *) l->data;
			/* If we have valid list data, go through the list data
			   until we get a data point time value greater than the
			   event we have. */
			while (l2 && eventdata->time > point_this->time) {
				point_last = point_this;
				point_this = (GpmInfoDataPoint *) l2->data;
				l2 = l2->next;
			}
			/* Interpolate in the y direction with the two
			   previous points. This should be 100% accurate */
			pos = gpm_graph_widget_interpolate_value (point_this,
							       point_last,
							       eventdata->time);
			gpm_graph_widget_get_pos_on_graph (graph, eventdata->time,
						    pos, &newx, &newy);
			/* don't overlay the points, stack vertically */
			if (abs (prevpos - newx) < 8) {
				previous_point++;
			} else {
				previous_point = 0;
			}
			newy -= (8 * previous_point);
			/* only do the event dot, if it's going to fit on the graph */
			if (eventdata->time > graph->priv->start_x) {
				gpm_graph_widget_draw_dot (cr, newx, newy, eventdata->colour);
			}
			prevpos = newx;
		}
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
gpm_graph_widget_draw_legend (cairo_t *cr,
			      gint     x,
			      gint     y,
			      gint     width,
			      gint     height)
{
	const gchar *desc;
	gint y_count;
	gint a;
	GpmGraphWidgetColour colour;

	gpm_graph_widget_draw_bounding_box (cr, x, y, width, height);
	y_count = y + 10;
	for (a=0; a<GPM_GRAPH_WIDGET_EVENT_LAST; a++) {
		desc = gpm_graph_widget_event_description (a);
		colour = gpm_graph_widget_event_colour (a);
		gpm_graph_widget_draw_dot (cr, x + 8, y_count, colour);
		cairo_move_to (cr, x + 8 + 10, y_count + 3);
		cairo_show_text (cr, desc);
		y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
	}

	/* add the line colours to the legend */
	gpm_graph_widget_draw_legend_line (cr, x + 8, y_count,
					   GPM_GRAPH_WIDGET_COLOUR_CHARGING);
	cairo_move_to (cr, x + 8 + 10, y_count + 3);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_show_text (cr, _("Charging"));
	y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
	gpm_graph_widget_draw_legend_line (cr, x + 8, y_count,
					   GPM_GRAPH_WIDGET_COLOUR_DISCHARGING);
	cairo_move_to (cr, x + 8 + 10, y_count + 3);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_show_text (cr, _("Discharging"));
	y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
	gpm_graph_widget_draw_legend_line (cr, x + 8, y_count,
					   GPM_GRAPH_WIDGET_COLOUR_CHARGED);
	cairo_move_to (cr, x + 8 + 10, y_count + 3);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_show_text (cr, _("Charged"));
}

/**
 * gpm_graph_widget_legend_calculate_width:
 * @graph: This graph class instance
 * @cr: Cairo drawing context
 * Return value: The width of the legend, including borders.
 *
 * We have to find the maximum size of the text so we know the width of the
 * legend box. We can't hardcode this as the dpi or font size might differ
 * from machine to machine.
 **/
static gfloat
gpm_graph_widget_legend_calculate_width (GpmGraphWidget *graph, cairo_t *cr)
{
	gint a;
	const gchar *desc;
	cairo_text_extents_t extents;
	gfloat max_width = 0.0f;

	g_return_val_if_fail (graph != NULL, 0.0f);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), 0.0f);

	cairo_set_font_options (cr, graph->priv->options);
	for (a=0; a<GPM_GRAPH_WIDGET_EVENT_LAST; a++) {
		desc = gpm_graph_widget_event_description (a);
		cairo_text_extents (cr, desc, &extents);
		if (max_width < extents.width) {
			max_width = extents.width;
		}
	}

	/* add for borders */
	max_width += 25;

	return max_width;
}

/**
 * gpm_graph_widget_draw_graph:
 * @graph: This graph class instance
 * @cr: Cairo drawing context
 *
 * Draw the complete graph, with the box, the grid, the labels and the line.
 **/
static void
gpm_graph_widget_draw_graph (GtkWidget *graph_widget, cairo_t *cr)
{
	gint legend_x = 0;
	gint legend_y = 0;
	gint legend_height;
	gint legend_width;
	gint data_x;
	gint data_y;

	GpmGraphWidget *graph = (GpmGraphWidget*) graph_widget;
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	legend_width = gpm_graph_widget_legend_calculate_width (graph, cr);
	legend_height = (GPM_GRAPH_WIDGET_EVENT_LAST + 3) * GPM_GRAPH_WIDGET_LEGEND_SPACING + 2;

	cairo_save (cr);

	graph->priv->box_x = 35;
	graph->priv->box_y = 5;

	if (graph->priv->use_legend) {
		graph->priv->box_width = graph_widget->allocation.width -
					 (3 + legend_width + 5 + graph->priv->box_x);
		legend_x = graph->priv->box_x + graph->priv->box_width + 6;
		legend_y = graph->priv->box_y;
	} else {
		graph->priv->box_width = graph_widget->allocation.width -
					 (3 + graph->priv->box_x);
	}
	graph->priv->box_height = graph_widget->allocation.height - (20 + graph->priv->box_y);

	/* graph background */
	gpm_graph_widget_draw_bounding_box (cr, graph->priv->box_x, graph->priv->box_y,
				     graph->priv->box_width, graph->priv->box_height);

	if (graph->priv->use_grid) {
		gpm_graph_widget_draw_grid (graph, cr);
	}

	gpm_graph_widget_auto_range (graph);

	/* -3 is so we can keep the lines inside the box at both extremes */
	data_x = graph->priv->stop_x - graph->priv->start_x;
	data_y = graph->priv->stop_y - graph->priv->start_y;
	graph->priv->unit_x = (float)(graph->priv->box_width - 3) / (float) data_x;
	graph->priv->unit_y = (float)(graph->priv->box_height - 3) / (float) data_y;

	gpm_graph_widget_draw_labels (graph, cr);
	gpm_graph_widget_draw_line (graph, cr);

	if (graph->priv->use_legend) {
		gpm_graph_widget_draw_legend (cr, legend_x, legend_y, legend_width, legend_height);
	}

	cairo_restore (cr);
}

/**
 * gpm_graph_widget_expose:
 * @graph: This graph class instance
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
