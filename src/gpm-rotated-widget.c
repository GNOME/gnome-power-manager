/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2024 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gpm-rotated-widget.h"

struct _GpmRotatedWidget{
	GtkBox		parent_instance;
};

G_DEFINE_FINAL_TYPE (GpmRotatedWidget, gpm_rotated_widget, GTK_TYPE_WIDGET);

static void
allocate (GtkWidget           *widget,
	  int                  width,
	  int                  height,
	  int                  baseline)
{
	GtkWidget *child = gtk_widget_get_first_child (widget);
	GskTransform *transform;
	bool rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
	float angle = rtl ? 90.0 : 270.0;
	graphene_point_t translation = GRAPHENE_POINT_INIT (round (rtl ?
								   height / 2.0 - width :
								   -height / 2.0),
							    (rtl ? -width : 0));

	transform = gsk_transform_translate (gsk_transform_rotate (NULL, angle), &translation);

	gtk_widget_allocate (child, width, height, baseline, transform);
}

static void
measure (GtkWidget      *widget,
	 GtkOrientation  orientation,
	 int             for_size,
	 int            *minimum,
	 int            *natural,
	 int            *minimum_baseline,
	 int            *natural_baseline)
{
	if (orientation == GTK_ORIENTATION_VERTICAL) {
		orientation = GTK_ORIENTATION_HORIZONTAL;
	} else {
		orientation = GTK_ORIENTATION_VERTICAL;
	}

	gtk_widget_measure (gtk_widget_get_first_child (widget), orientation,
			    for_size, minimum, natural, NULL, NULL);
}

static void
gpm_rotated_widget_class_init (GpmRotatedWidgetClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	widget_class->size_allocate = allocate;
	widget_class->measure = measure;
}

static void
gpm_rotated_widget_init (GpmRotatedWidget *self)
{

}
