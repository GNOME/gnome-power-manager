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
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gpm-common.h"
#include "gpm-graph-widget.h"
#include "gpm-array.h"
#include "gpm-debug.h"

G_DEFINE_TYPE (GpmGraphWidget, gpm_graph_widget, GTK_TYPE_DRAWING_AREA);
#define GPM_GRAPH_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidgetPrivate))

struct GpmGraphWidgetPrivate
{
	gboolean		 use_grid;
	gboolean		 use_legend;
	gboolean		 use_axis_labels;
	gboolean		 use_events;

	gboolean		 invert_x;
	gboolean		 autorange_x;
	gboolean		 invert_y;

	GSList			*keyvals; /* to hold all the key data */

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
	const gchar		*axis_label_x;
	const gchar		*axis_label_y;
	gchar			*title;

	cairo_t			*cr;
	cairo_font_options_t	*options;

	GPtrArray		*data_list;
	GpmArray		*events;
};

static gboolean gpm_graph_widget_expose (GtkWidget *graph, GdkEventExpose *event);
static void	gpm_graph_widget_finalize (GObject *object);

#define GPM_GRAPH_WIDGET_KEY_MAX	14
#define GPM_CHARGED_TEXT		_("Charged")
#define GPM_CHARGING_TEXT		_("Charging")
#define GPM_DISCHARGING_TEXT		_("Discharging")

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
	ret = g_slist_find_custom (graph->priv->keyvals, &id,
				   gpm_graph_widget_key_compare_func);
	if (! ret) {
		return NULL;
	}
	data = (GpmGraphWidgetKeyItem *) ret->data;
	return data;
}

/**
 * gpm_graph_widget_key_add:
 **/
gboolean
gpm_graph_widget_key_add (GpmGraphWidget       *graph,
			  const gchar	       *name,
			  guint		        id,
			  guint32               colour,
			  GpmGraphWidgetShape   shape)
{
	GpmGraphWidgetKeyItem *keyitem;
	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	gpm_debug ("add to hashtable %s", name);
	keyitem = g_new0 (GpmGraphWidgetKeyItem, 1);

	keyitem->id = id;
	keyitem->name = name;
	keyitem->colour = colour;
	keyitem->shape = shape;

	graph->priv->keyvals = g_slist_append (graph->priv->keyvals, (gpointer) keyitem);
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
	g_return_val_if_fail (type != NULL, GPM_GRAPH_WIDGET_TYPE_INVALID);

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
 * gpm_graph_widget_get_axis_label_y:
 * @graph: This class instance
 * @type: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
static const gchar *
gpm_graph_widget_get_axis_label_y (GpmGraphWidget *graph, GpmGraphWidgetAxisType type)
{
	g_return_val_if_fail (graph != NULL, NULL);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), NULL);

	if (type == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		return _("Charge percentage");
	}
	if (type == GPM_GRAPH_WIDGET_TYPE_TIME) {
		return _("Time remaining");
	}
	if (type == GPM_GRAPH_WIDGET_TYPE_POWER) {
		return _("Power");
	}
	if (type == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		return _("Cell Voltage");
	}
	return _("Unknown caption");
}

/**
 * gpm_graph_widget_get_axis_label_x:
 * @graph: This class instance
 * @type: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
static const gchar *
gpm_graph_widget_get_axis_label_x (GpmGraphWidget *graph, GpmGraphWidgetAxisType type)
{
	g_return_val_if_fail (graph != NULL, NULL);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), NULL);

	if (type == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		return _("Battery percentage");
	}
	if (type == GPM_GRAPH_WIDGET_TYPE_TIME) {
		/* I want this translated please */
		const char *moo;
		moo = _("Time");
		return _("Time since startup");
	}
	return _("Unknown caption");
}

/**
 * gpm_graph_widget_set_title:
 * @graph: This class instance
 * @axis: The axis type, e.g. GPM_GRAPH_WIDGET_TYPE_TIME
 **/
void
gpm_graph_widget_set_title (GpmGraphWidget *graph, const gchar *title)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	if (graph->priv->title != NULL) {
		g_free (graph->priv->title);
	}
	graph->priv->title = g_strdup (title);
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
	graph->priv->axis_label_x = gpm_graph_widget_get_axis_label_x (graph, axis);
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
	graph->priv->axis_label_y = gpm_graph_widget_get_axis_label_y (graph, axis);
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
 * gpm_graph_widget_enable_axis_labels:
 * @graph: This class instance
 * @enable: If we should show the axis labels
 **/
void
gpm_graph_widget_enable_axis_labels (GpmGraphWidget *graph, gboolean enable)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));
	graph->priv->use_axis_labels = enable;

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
	graph->priv = GPM_GRAPH_WIDGET_GET_PRIVATE (graph);
	graph->priv->invert_x = FALSE;
	graph->priv->invert_y = FALSE;
	graph->priv->start_x = 0;
	graph->priv->start_y = 0;
	graph->priv->stop_x = 60;
	graph->priv->stop_y = 100;
	graph->priv->use_grid = TRUE;
	graph->priv->use_legend = FALSE;
	graph->priv->use_axis_labels = FALSE;
	graph->priv->autorange_x = TRUE;
	graph->priv->data_list = g_ptr_array_new ();
	graph->priv->keyvals = NULL;
	graph->priv->axis_label_x = NULL;
	graph->priv->axis_label_y = NULL;
	graph->priv->title = NULL;
	graph->priv->axis_type_x = GPM_GRAPH_WIDGET_TYPE_TIME;
	graph->priv->axis_type_y = GPM_GRAPH_WIDGET_TYPE_PERCENTAGE;
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
	guint a;
	GpmGraphWidget *graph = (GpmGraphWidget*) object;
	cairo_font_options_destroy (graph->priv->options);

	GpmGraphWidgetKeyItem *keyitem;
	/* remove items in list and free */
	for (a=0; a<g_slist_length (graph->priv->keyvals); a++) {
		keyitem = (GpmGraphWidgetKeyItem *) g_slist_nth_data (graph->priv->keyvals, a);
		g_free (keyitem);
	}
	g_slist_free (graph->priv->keyvals);
	g_ptr_array_free (graph->priv->data_list, FALSE);

	g_free (graph->priv->title);
}

/**
 * gpm_graph_widget_set_invert_x:
 * @graph: This class instance
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
 * @graph: This class instance
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
 * @graph: This class instance
 * @list: The GList values to be plotted on the graph
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
void
gpm_graph_widget_set_data (GpmGraphWidget *graph, GpmArray *array, guint id)
{
	g_return_if_fail (array != NULL);
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	if (gpm_array_get_size (array) == 0) {
		gpm_warning ("Trying to assign a zero length array");
		return;
	}

	/* fresh list */
	if (graph->priv->data_list->len == 0 || graph->priv->data_list->len < id + 1) {
		g_ptr_array_add (graph->priv->data_list, (gpointer) array);
	} else {
		/* remove existing, and add new */
		gpm_debug ("Re-assigning dataset");
		g_ptr_array_remove_index (graph->priv->data_list, id);
		g_ptr_array_add (graph->priv->data_list, (gpointer) array);
	}

}

/**
 * gpm_graph_widget_set_events:
 * @graph: This class instance
 * @list: The GList events to be plotted on the graph
 *
 * Sets the data for the graph. You MUST NOT free the list before the widget.
 **/
void
gpm_graph_widget_set_events (GpmGraphWidget *graph, GpmArray *array)
{
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	graph->priv->events = array;
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
		/*Translators: This is %i Watts*/
		text = g_strdup_printf (_("%iW"), value / 1000);
	} else if (axis == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		/*Translators: This is %i Volts*/
		text = g_strdup_printf (_("%iV"), value / 1000);
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
		text = gpm_get_axis_label (graph->priv->axis_type_x, value);

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
			value = (length_y / 10) * a + graph->priv->start_y;
		} else {
			value = (length_y / 10) * (10 - a) + graph->priv->start_y;
		}
		text = gpm_get_axis_label (graph->priv->axis_type_y, value);

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
	GpmArray *array;
	GpmArrayPoint *point;
	guint i;
	guint j;
	guint length;

	if (graph->priv->data_list->len == 0) {
		gpm_debug ("no data");
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
	gpm_debug ("Data range is %i<x<%i, %i<y<%i", smallest_x, biggest_x, smallest_y, biggest_y);

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
	if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		graph->priv->stop_x = 100;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_TIME) {
		graph->priv->stop_x = (((biggest_x - smallest_x) / (10 * 60)) + 1) * (10 * 60) + smallest_x;
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_POWER) {
		graph->priv->stop_x = (((biggest_x - smallest_x) / 10000) + 2) * 10000 + smallest_x;
		if (graph->priv->stop_x < 10000) {
			graph->priv->stop_x = 10000 + smallest_x;
		}
	} else if (graph->priv->axis_type_x == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		graph->priv->stop_x = (((biggest_x - smallest_x) / 1000) + 2) * 1000 + smallest_x;
		if (graph->priv->stop_x < 1000) {
			graph->priv->stop_x = 1000 + smallest_x;
		}
	} else {
		graph->priv->start_x = 0;
		graph->priv->stop_x = biggest_x;
	}

	/* y */
	if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_PERCENTAGE) {
		graph->priv->start_y = 0;
		graph->priv->stop_y = 100;
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_TIME) {
		graph->priv->start_y = 0;
		graph->priv->stop_y = ((biggest_y / 60) + 2)* 60;
		if (graph->priv->stop_y > 60) {
			graph->priv->stop_y = ((biggest_y / (1 * 60)) + 2) * (1 * 60);
		}
		if (graph->priv->stop_y < 60) {
			graph->priv->stop_y = 60;
		}
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_POWER) {
		graph->priv->start_y = 0;
		graph->priv->stop_y = ((biggest_y / 10000) + 2) * 10000;
		if (graph->priv->stop_y < 10000) {
			graph->priv->stop_y = 10000;
		}
	} else if (graph->priv->axis_type_y == GPM_GRAPH_WIDGET_TYPE_VOLTAGE) {
		graph->priv->start_y = 0;
		graph->priv->stop_y = ((biggest_y / 1000) + 2) * 1000;
		if (graph->priv->stop_y < 1000) {
			graph->priv->stop_y = 1000;
		}
	} else {
		graph->priv->start_y = 0;
		graph->priv->stop_y = biggest_y;
	}
	gpm_debug ("Processed range is %i<x<%i, %i<y<%i",
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
	gpm_colour_to_rgb (colour, &r, &g, &b);
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
		gpm_warning ("shape %i not recognised!", shape);
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
		gpm_debug ("no data");
		return;
	}
	cairo_save (cr);

	/* do all the lines on the graphs */
	for (i=0; i<graph->priv->data_list->len; i++) {
		gpm_debug ("drawing line %i", i);
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
	guint previous_point = 0;
	gint prevpos = -1;


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
		/* try to position the point on the line, or at zero if there is no line */
		if (array == NULL) {
			dot = 0;
		} else {
			dot = gpm_array_interpolate (array, point->x);
		}
		gpm_graph_widget_get_pos_on_graph (graph, point->x, dot, &newx, &newy);

		/* don't overlay the points, stack vertically */
		if (abs (newx - prevpos) < 10) {
			previous_point++;
		} else {
			previous_point = 0;
		}
		newy -= (8 * previous_point);

		/* only do the event dot, if it's going to be valid on the graph */
		if (point->x > graph->priv->start_x && newy > graph->priv->box_y) {
			keyitem = gpm_graph_widget_key_find_id (graph, point->y);
			gpm_graph_widget_draw_dot (cr, newx, newy, keyitem->colour, keyitem->shape);
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
 * gpm_graph_widget_have_key_id:
 * Finds out if we have an event of type id.
 **/
static gboolean
gpm_graph_widget_have_key_id (GpmGraphWidget *graph, guint id)
{
	guint i;
	guint length;
	GpmArrayPoint *point;

	/* not set */
	if (graph->priv->events == NULL) {
		return FALSE;
	}

	length = gpm_array_get_size (graph->priv->events);
	for (i=0; i < length; i++) {
		point = gpm_array_get (graph->priv->events, i);
		if (point->y == id) {
			return TRUE;
		}
	}
	return FALSE;
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
	GpmGraphWidgetKeyItem *keyitem;

	gpm_graph_widget_draw_bounding_box (cr, x, y, width, height);
	y_count = y + 10;
	for (a=0; a<GPM_GRAPH_WIDGET_KEY_MAX; a++) {
		keyitem = gpm_graph_widget_key_find_id (graph, a);
		if (keyitem == NULL) {
			break;
		}
		/* only do the legend point if we have one of these dots */
		if (gpm_graph_widget_have_key_id (graph, keyitem->id) == TRUE) {
			gpm_graph_widget_draw_dot (cr, x + 8, y_count,
						   keyitem->colour, keyitem->shape);
			cairo_move_to (cr, x + 8 + 10, y_count + 3);
			cairo_show_text (cr, keyitem->name);
			y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
		}
	}

	/* add the line colours to the legend */
	gpm_graph_widget_draw_legend_line (cr, x + 8, y_count,
					   GPM_COLOUR_CHARGING);
	cairo_move_to (cr, x + 8 + 10, y_count + 3);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_show_text (cr, GPM_CHARGING_TEXT);
	y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
	gpm_graph_widget_draw_legend_line (cr, x + 8, y_count,
					   GPM_COLOUR_DISCHARGING);
	cairo_move_to (cr, x + 8 + 10, y_count + 3);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_show_text (cr, GPM_DISCHARGING_TEXT);
	y_count = y_count + GPM_GRAPH_WIDGET_LEGEND_SPACING;
	gpm_graph_widget_draw_legend_line (cr, x + 8, y_count,
					   GPM_COLOUR_CHARGED);
	cairo_move_to (cr, x + 8 + 10, y_count + 3);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_show_text (cr, GPM_CHARGED_TEXT);
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
	g_return_val_if_fail (graph != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_GRAPH_WIDGET (graph), FALSE);

	cairo_set_font_options (cr, graph->priv->options);

	guint a;
	GpmGraphWidgetKeyItem *keyitem;
	cairo_text_extents_t extents;

	/* set defaults */
	*width = 0;
	*height = 3 * GPM_GRAPH_WIDGET_LEGEND_SPACING;

	/* get the max size for the labels (may be different for non UK) */
	cairo_text_extents (cr, GPM_CHARGED_TEXT, &extents);
	if (*width < extents.width) {
		*width = extents.width;
	}
	cairo_text_extents (cr, GPM_CHARGING_TEXT, &extents);
	if (*width < extents.width) {
		*width = extents.width;
	}
	cairo_text_extents (cr, GPM_DISCHARGING_TEXT, &extents);
	if (*width < extents.width) {
		*width = extents.width;
	}

	/* find the width of the biggest key that is showing */
	for (a=0; a<GPM_GRAPH_WIDGET_KEY_MAX; a++) {
		keyitem = gpm_graph_widget_key_find_id (graph, a);
		if (keyitem == NULL) {
			break;
		}
		/* only do the legend point if we have one of these dots */
		if (gpm_graph_widget_have_key_id (graph, keyitem->id) == TRUE) {
			*height = *height + GPM_GRAPH_WIDGET_LEGEND_SPACING + 2;
			cairo_text_extents (cr, keyitem->name, &extents);
			if (*width < extents.width) {
				*width = extents.width;
			}
		}
	}

	/* add for borders */
	*width += 25;

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
	cairo_text_extents_t extents_axis_label_x;
	cairo_text_extents_t extents_axis_label_y;
	cairo_text_extents_t extents_title;

	GpmGraphWidget *graph = (GpmGraphWidget*) graph_widget;
	g_return_if_fail (graph != NULL);
	g_return_if_fail (GPM_IS_GRAPH_WIDGET (graph));

	gpm_graph_widget_legend_calculate_size (graph, cr, &legend_width, &legend_height);

	cairo_save (cr);

	graph->priv->box_x = 35;
	graph->priv->box_y = 5;

	if (graph->priv->title != NULL) {
		cairo_text_extents (cr, graph->priv->title, &extents_title);
		graph->priv->box_y += extents_title.height;
	}

	graph->priv->box_height = graph_widget->allocation.height - (20 + graph->priv->box_y);

	/* make size adjustment for axis labels */
	if (graph->priv->use_axis_labels) {
		/* get the size of the x axis text */
		cairo_save (cr);
		cairo_set_font_size (cr, 12);
		cairo_text_extents (cr, graph->priv->axis_label_x, &extents_axis_label_x);
		cairo_text_extents (cr, graph->priv->axis_label_y, &extents_axis_label_y);
		cairo_restore (cr);

		graph->priv->box_height -= (extents_axis_label_x.height + 2);
		graph->priv->box_x += (extents_axis_label_y.height + 2);
	}

	/* make size adjustment for legend */
	if (graph->priv->use_legend) {
		graph->priv->box_width = graph_widget->allocation.width -
					 (3 + legend_width + 5 + graph->priv->box_x);
		legend_x = graph->priv->box_x + graph->priv->box_width + 6;
		legend_y = graph->priv->box_y;
	} else {
		graph->priv->box_width = graph_widget->allocation.width -
					 (3 + graph->priv->box_x);
	}

	/* center title */
	if (graph->priv->title != NULL) {
		cairo_move_to (cr, graph->priv->box_x + (graph->priv->box_width-extents_title.width)/2, 9);
		cairo_show_text (cr, graph->priv->title);
	}

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

	if (graph->priv->use_axis_labels) {
		/* horizontal axis label */
		cairo_save (cr);
		cairo_set_font_size (cr, 12);
		cairo_move_to (cr, graph->priv->box_x + ((graph->priv->box_width - extents_axis_label_x.width)/2), graph_widget->allocation.height - 3);
		cairo_show_text (cr, graph->priv->axis_label_x);
		/* vertical axis label */
		cairo_move_to (cr, extents_axis_label_y.height, ((graph->priv->box_height + extents_axis_label_y.width) / 2) - graph->priv->box_y);
		cairo_rotate (cr, -3.1415927 / 2.0);
		cairo_show_text (cr, graph->priv->axis_label_y);
		cairo_restore (cr);
	}

	gpm_graph_widget_draw_line (graph, cr);

	if (graph->priv->use_events) {
		gpm_graph_widget_draw_event_dots (graph, cr);
	}

	if (graph->priv->use_legend) {
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
