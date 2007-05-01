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

#ifndef __GPM_GRAPH_WIDGET_H__
#define __GPM_GRAPH_WIDGET_H__

#include <gtk/gtk.h>
#include "gpm-array.h"

G_BEGIN_DECLS

#define GPM_TYPE_GRAPH_WIDGET		(gpm_graph_widget_get_type ())
#define GPM_GRAPH_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidget))
#define GPM_GRAPH_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GPM_GRAPH_WIDGET, GpmGraphWidgetClass))
#define GPM_IS_GRAPH_WIDGET(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GPM_TYPE_GRAPH_WIDGET))
#define GPM_IS_GRAPH_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_GRAPH_WIDGET))
#define GPM_GRAPH_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GPM_TYPE_GRAPH_WIDGET, GpmGraphWidgetClass))

#define GPM_COLOUR_CHARGING			GPM_COLOUR_BLUE
#define GPM_COLOUR_DISCHARGING			GPM_COLOUR_DARK_RED
#define GPM_COLOUR_CHARGED			GPM_COLOUR_GREEN
#define GPM_GRAPH_WIDGET_LEGEND_SPACING		19

typedef struct GpmGraphWidget		GpmGraphWidget;
typedef struct GpmGraphWidgetClass	GpmGraphWidgetClass;
typedef struct GpmGraphWidgetPrivate	GpmGraphWidgetPrivate;

typedef enum {
	GPM_GRAPH_WIDGET_SHAPE_CIRCLE,
	GPM_GRAPH_WIDGET_SHAPE_SQUARE,
	GPM_GRAPH_WIDGET_SHAPE_DIAMOND,
	GPM_GRAPH_WIDGET_SHAPE_TRIANGLE,
	GPM_GRAPH_WIDGET_SHAPE_LAST
} GpmGraphWidgetShape;

typedef enum {
	GPM_GRAPH_WIDGET_TYPE_INVALID,
	GPM_GRAPH_WIDGET_TYPE_PERCENTAGE,
	GPM_GRAPH_WIDGET_TYPE_TIME,
	GPM_GRAPH_WIDGET_TYPE_POWER,
	GPM_GRAPH_WIDGET_TYPE_VOLTAGE,
	GPM_GRAPH_WIDGET_TYPE_LAST
} GpmGraphWidgetAxisType;

/* the different kinds of dots in the key */
typedef struct {
	guint			 id;
	const gchar		*name;
	guint32			 colour;
	GpmGraphWidgetShape	 shape;
} GpmGraphWidgetKeyItem;

/* the different kinds of lines in the key */
typedef struct {
	guint32			 colour;
	const gchar		*desc;
} GpmGraphWidgetKeyData;

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
							 GpmArray	*array,
							 guint		 id);
void		 gpm_graph_widget_set_events		(GpmGraphWidget	*graph,
							 GpmArray	*array);
void		 gpm_graph_widget_set_title		(GpmGraphWidget	*graph,
							 const gchar	*title);
void		 gpm_graph_widget_set_axis_type_x	(GpmGraphWidget	*graph,
							 GpmGraphWidgetAxisType axis);
void		 gpm_graph_widget_set_axis_type_y	(GpmGraphWidget	*graph,
							 GpmGraphWidgetAxisType axis);
gboolean	 gpm_graph_widget_key_data_clear	(GpmGraphWidget	*graph);
gboolean	 gpm_graph_widget_key_data_add		(GpmGraphWidget	*graph,
							 guint		 colour,
							 const gchar	*desc);
gboolean	 gpm_graph_widget_key_add /* _event */	(GpmGraphWidget	*graph,
							 const gchar	*name,
							 guint		 id,
							 guint32	 colour,
							 GpmGraphWidgetShape shape);

GpmGraphWidgetAxisType	 gpm_graph_widget_string_to_axis_type (const gchar	*type);

G_END_DECLS

#endif
