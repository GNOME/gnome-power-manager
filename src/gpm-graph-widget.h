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

#ifndef __GPM_GRAPH_WIDGET_H__
#define __GPM_GRAPH_WIDGET_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GPM_TYPE_GRAPH_WIDGET		(gpm_graph_widget_get_type ())
#define GPM_GRAPH_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidget))
#define GPM_GRAPH_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GPM_GRAPH_WIDGET, GpmGraphWidgetClass))
#define GPM_IS_GRAPH_WIDGET(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GPM_TYPE_GRAPH_WIDGET))
#define GPM_IS_GRAPH_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_GRAPH_WIDGET))
#define GPM_GRAPH_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidgetClass))

#define GPM_GRAPH_WIDGET_COLOUR_CHARGING	GPM_GRAPH_WIDGET_COLOUR_DARK_BLUE
#define GPM_GRAPH_WIDGET_COLOUR_DISCHARGING	GPM_GRAPH_WIDGET_COLOUR_DARK_RED
#define GPM_GRAPH_WIDGET_COLOUR_CHARGED		GPM_GRAPH_WIDGET_COLOUR_GREEN
#define GPM_GRAPH_WIDGET_LEGEND_SPACING		19

typedef struct GpmGraphWidget		GpmGraphWidget;
typedef struct GpmGraphWidgetClass	GpmGraphWidgetClass;
typedef struct GpmGraphWidgetPrivate	GpmGraphWidgetPrivate;

typedef enum {
	GPM_GRAPH_WIDGET_COLOUR_DEFAULT,
	GPM_GRAPH_WIDGET_COLOUR_WHITE,
	GPM_GRAPH_WIDGET_COLOUR_BLACK,
	GPM_GRAPH_WIDGET_COLOUR_RED,
	GPM_GRAPH_WIDGET_COLOUR_BLUE,
	GPM_GRAPH_WIDGET_COLOUR_GREEN,
	GPM_GRAPH_WIDGET_COLOUR_MAGENTA,
	GPM_GRAPH_WIDGET_COLOUR_YELLOW,
	GPM_GRAPH_WIDGET_COLOUR_CYAN,
	GPM_GRAPH_WIDGET_COLOUR_GREY,
	GPM_GRAPH_WIDGET_COLOUR_DARK_BLUE,
	GPM_GRAPH_WIDGET_COLOUR_DARK_RED,
	GPM_GRAPH_WIDGET_COLOUR_DARK_MAGENTA,
	GPM_GRAPH_WIDGET_COLOUR_DARK_YELLOW,
	GPM_GRAPH_WIDGET_COLOUR_DARK_GREEN,
	GPM_GRAPH_WIDGET_COLOUR_DARK_CYAN,
	GPM_GRAPH_WIDGET_COLOUR_DARK_GREY,
	GPM_GRAPH_WIDGET_COLOUR_LAST
} GpmGraphWidgetColour;

typedef enum {
	GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
	GPM_GRAPH_WIDGET_SHAPE_SQUARE,
	GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
	GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
	GPM_GRAPH_WIDGET_SHAPE_LAST
} GpmGraphWidgetShape;

typedef enum {
	GPM_GRAPH_WIDGET_EVENT_ON_AC,
	GPM_GRAPH_WIDGET_EVENT_ON_BATTERY,
	GPM_GRAPH_WIDGET_EVENT_SCREEN_DIM,
	GPM_GRAPH_WIDGET_EVENT_SCREEN_RESUME,
	GPM_GRAPH_WIDGET_EVENT_SUSPEND,
	GPM_GRAPH_WIDGET_EVENT_HIBERNATE,
	GPM_GRAPH_WIDGET_EVENT_RESUME,
	GPM_GRAPH_WIDGET_EVENT_LID_CLOSED,
	GPM_GRAPH_WIDGET_EVENT_LID_OPENED,
	GPM_GRAPH_WIDGET_EVENT_NOTIFICATION,
	GPM_GRAPH_WIDGET_EVENT_LAST
} GpmGraphWidgetEvent;

typedef enum {
	GPM_GRAPH_WIDGET_TYPE_INVALID,
	GPM_GRAPH_WIDGET_TYPE_PERCENTAGE,
	GPM_GRAPH_WIDGET_TYPE_TIME,
	GPM_GRAPH_WIDGET_TYPE_POWER,
	GPM_GRAPH_WIDGET_TYPE_LAST
} GpmGraphWidgetAxisType;

struct GpmGraphWidget
{
	GtkDrawingArea	 parent;
	GpmGraphWidgetPrivate	*priv;
};

struct GpmGraphWidgetClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 gpm_graph_widget_get_type		(void);
GtkWidget	*gpm_graph_widget_new			(void);

void		 gpm_graph_widget_set_invert_x		(GpmGraphWidget	*graph,
							 gboolean	 inv);
void		 gpm_graph_widget_enable_legend		(GpmGraphWidget	*graph,
							 gboolean	 enable);
void		 gpm_graph_widget_enable_events		(GpmGraphWidget	*graph,
							 gboolean	 enable);
void		 gpm_graph_widget_set_invert_y		(GpmGraphWidget	*graph,
							 gboolean	 inv);
void		 gpm_graph_widget_set_data		(GpmGraphWidget	*graph,
							 GList		*list);
void		 gpm_graph_widget_set_events		(GpmGraphWidget	*graph,
							 GList		*list);
void		 gpm_graph_widget_set_axis_x		(GpmGraphWidget	*graph,
							 GpmGraphWidgetAxisType axis);
void		 gpm_graph_widget_set_axis_y		(GpmGraphWidget	*graph,
							 GpmGraphWidgetAxisType axis);
const gchar *	 gpm_graph_widget_event_description	(GpmGraphWidgetEvent event);
void		 gpm_graph_widget_get_event_visual		(GpmGraphWidgetEvent event,
							 GpmGraphWidgetColour *colour,
							 GpmGraphWidgetShape *shape);
GpmGraphWidgetAxisType	 gpm_graph_widget_string_to_axis_type (const gchar	*type);

G_END_DECLS

#endif
