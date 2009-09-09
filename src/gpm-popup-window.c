/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "egg-debug.h"

#include "gpm-popup-window.h"

#define GPM_POPUP_WINDOW_DIALOG_TIMEOUT		2000	/* dialog timeout in ms */
#define GPM_POPUP_WINDOW_DIALOG_FADE_TIMEOUT	1500	/* timeout before fade starts */
#define GPM_POPUP_WINDOW_FADE_TIMEOUT		10	/* timeout in ms between each frame of the fade */

#define GPM_POPUP_WINDOW_BG_ALPHA		0.50
#define GPM_POPUP_WINDOW_FG_ALPHA		1.00

#define GPM_POPUP_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POPUP_WINDOW, GpmPopupWindowPrivate))

struct GpmPopupWindowPrivate
{
	gboolean		 is_composited;
	guint			 hide_timeout_id;
	guint			 fade_timeout_id;
	gdouble			 fade_out_alpha;
	gfloat			 value;
	gchar			*icon_name;
	GtkImage		*image;
	GtkWidget		*progress;
	GtkWidget		*frame;
};

G_DEFINE_TYPE (GpmPopupWindow, gpm_popup_window, GTK_TYPE_WINDOW)

/**
 * gpm_popup_window_fade_timeout_cb:
 **/
static gboolean
gpm_popup_window_fade_timeout_cb (GpmPopupWindow *popup)
{
	if (popup->priv->fade_out_alpha <= 0.0) {
		gtk_widget_hide (GTK_WIDGET (popup));

		/* Reset it for the next time */
		popup->priv->fade_out_alpha = 1.0;
		popup->priv->fade_timeout_id = 0;

		return FALSE;
	} else {
		GdkRectangle rect;
		GtkWidget *widget = GTK_WIDGET (popup);
		GtkAllocation allocation;

		popup->priv->fade_out_alpha -= 0.10;

		rect.x = 0;
		rect.y = 0;
		gtk_widget_get_allocation (widget, &allocation);
		rect.width = allocation.width;
		rect.height = allocation.height;

		gdk_window_invalidate_rect (gtk_widget_get_window (widget), &rect, FALSE);
	}

	return TRUE;
}

/**
 * gpm_popup_window_hide_timeout_cb:
 **/
static gboolean
gpm_popup_window_hide_timeout_cb (GpmPopupWindow *popup)
{
	if (popup->priv->is_composited) {
		popup->priv->hide_timeout_id = 0;
		popup->priv->fade_timeout_id = g_timeout_add (GPM_POPUP_WINDOW_FADE_TIMEOUT,
							      (GSourceFunc) gpm_popup_window_fade_timeout_cb, popup);
	} else {
		gtk_widget_hide (GTK_WIDGET (popup));
	}

	return FALSE;
}

/**
 * gpm_popup_window_remove_hide_timeout:
 **/
static void
gpm_popup_window_remove_hide_timeout (GpmPopupWindow *popup)
{
	if (popup->priv->hide_timeout_id != 0) {
		g_source_remove (popup->priv->hide_timeout_id);
		popup->priv->hide_timeout_id = 0;
	}

	if (popup->priv->fade_timeout_id != 0) {
		g_source_remove (popup->priv->fade_timeout_id);
		popup->priv->fade_timeout_id = 0;
		popup->priv->fade_out_alpha = 1.0;
	}
}

/**
 * gpm_popup_window_add_hide_timeout:
 **/
static void
gpm_popup_window_add_hide_timeout (GpmPopupWindow *popup)
{
	guint timeout;
	if (popup->priv->is_composited) {
		timeout = GPM_POPUP_WINDOW_DIALOG_FADE_TIMEOUT;
	} else {
		timeout = GPM_POPUP_WINDOW_DIALOG_TIMEOUT;
	}
	popup->priv->hide_timeout_id = g_timeout_add (timeout, (GSourceFunc) gpm_popup_window_hide_timeout_cb, popup);
}

/**
 * gpm_popup_window_update_window:
 **/
static void
gpm_popup_window_update_window (GpmPopupWindow *popup)
{
	gpm_popup_window_remove_hide_timeout (popup);
	gpm_popup_window_add_hide_timeout (popup);

	if (popup->priv->is_composited)
		gtk_widget_queue_draw (GTK_WIDGET (popup));
}

/**
 * gpm_popup_window_set_icon_name:
 **/
void
gpm_popup_window_set_icon_name (GpmPopupWindow *popup, const gchar *icon_name)
{
	g_return_if_fail (GPM_IS_POPUP_WINDOW (popup));
	g_free (popup->priv->icon_name);
	popup->priv->icon_name = g_strdup (icon_name);
	if (popup->priv->image != NULL)
		gtk_image_set_from_icon_name (popup->priv->image, icon_name, GTK_ICON_SIZE_DIALOG);
}

/**
 * gpm_popup_window_set_value:
 **/
void
gpm_popup_window_set_value (GpmPopupWindow *popup, gfloat level)
{
	g_return_if_fail (GPM_IS_POPUP_WINDOW (popup));

	if (popup->priv->value != level) {
		popup->priv->value = level;
		gpm_popup_window_update_window (popup);

		/* set value straight away */
		if (!popup->priv->is_composited && popup->priv->progress != NULL)
			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (popup->priv->progress), popup->priv->value);
	}
}

/**
 * gpm_popup_window_curved_rectangle:
 **/
static void
gpm_popup_window_curved_rectangle (cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height, gdouble radius)
{
	gdouble xw;
	gdouble yh;

	xw = x + width;
	yh = y + height;

	if (!width || !height) {
		goto out;
	}

	if (width / 2 < radius) {
		if (height / 2 < radius) {
			cairo_move_to (cr, x, (y + yh) / 2);
			cairo_curve_to (cr, x ,y, x, y, (x + xw) / 2, y);
			cairo_curve_to (cr, xw, y, xw, y, xw, (y + yh) / 2);
			cairo_curve_to (cr, xw, yh, xw, yh, (xw + x) / 2, yh);
			cairo_curve_to (cr, x, yh, x, yh, x, (y + yh) / 2);
		} else {
			cairo_move_to (cr, x, y + radius);
			cairo_curve_to (cr, x, y, x, y, (x + xw) / 2, y);
			cairo_curve_to (cr, xw, y, xw, y, xw, y + radius);
			cairo_line_to (cr, xw, yh - radius);
			cairo_curve_to (cr, xw, yh, xw, yh, (xw + x) / 2, yh);
			cairo_curve_to (cr, x, yh, x, yh, x, yh - radius);
		}
	} else {
		if (height / 2 < radius) {
			cairo_move_to (cr, x, (y + yh) / 2);
			cairo_curve_to (cr, x, y, x , y, x + radius, y);
			cairo_line_to (cr, xw - radius, y);
			cairo_curve_to (cr, xw, y, xw, y, xw, (y + yh) / 2);
			cairo_curve_to (cr, xw, yh, xw, yh, xw - radius, yh);
			cairo_line_to (cr, x + radius, yh);
			cairo_curve_to (cr, x, yh, x, yh, x, (y + yh) / 2);
		} else {
			cairo_move_to (cr, x, y + radius);
			cairo_curve_to (cr, x , y, x , y, x + radius, y);
			cairo_line_to (cr, xw - radius, y);
			cairo_curve_to (cr, xw, y, xw, y, xw, y + radius);
			cairo_line_to (cr, xw, yh - radius);
			cairo_curve_to (cr, xw, yh, xw, yh, xw - radius, yh);
			cairo_line_to (cr, x + radius, yh);
			cairo_curve_to (cr, x, yh, x, yh, x, yh - radius);
		}
	}
out:
	cairo_close_path (cr);
}

/**
 * gpm_popup_window_load_pixbuf:
 **/
static GdkPixbuf *
gpm_popup_window_load_pixbuf (GpmPopupWindow *popup, const gchar *icon_name, gint icon_size)
{
	GtkIconTheme *theme;
	GdkPixbuf *pixbuf = NULL;
	guint width;

	if (icon_name == NULL)
		goto out;

	egg_debug ("rendering %s to a pixbuf", icon_name); 
	if (popup != NULL && gtk_widget_has_screen (GTK_WIDGET (popup))) {
		theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (popup)));
	} else {
		theme = gtk_icon_theme_get_default ();
	}

	/* make sure the pixbuf is close to the requested size
	 * this is necessary because GTK_ICON_LOOKUP_FORCE_SVG
	 * seems to be broken */
	pixbuf = gtk_icon_theme_load_icon (theme, icon_name, icon_size, GTK_ICON_LOOKUP_FORCE_SVG, NULL);
	if (pixbuf != NULL) {
		width = gdk_pixbuf_get_width (pixbuf);
		if (width < (float)icon_size * 0.75) {
			g_object_unref (pixbuf);
			pixbuf = NULL;
		}
	}
out:
	return pixbuf;
}

/**
 * gpm_popup_window_render_icon:
 **/
static gboolean
gpm_popup_window_render_icon (GpmPopupWindow *popup, cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height)
{
	GdkPixbuf *pixbuf;

	/* get pixbuf */
	pixbuf = gpm_popup_window_load_pixbuf (popup, popup->priv->icon_name, (gint) width);
	if (pixbuf == NULL)
		return FALSE;

	/* render */
	gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
	cairo_paint_with_alpha (cr, GPM_POPUP_WINDOW_FG_ALPHA);

	g_object_unref (pixbuf);
	return TRUE;
}

/**
 * gpm_popup_window_draw_progress_bar:
 **/
static void
gpm_popup_window_draw_progress_bar (GpmPopupWindow *popup, cairo_t *cr, gdouble percentage,
				    gdouble x, gdouble y, gdouble width, gdouble height)
{
	gdouble xw;
	GdkColor color;
	gdouble r, g, b;

	xw = width * percentage;

	/* bar background */
	color = gtk_widget_get_style (GTK_WIDGET (popup))->dark [GTK_STATE_NORMAL];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_rectangle (cr, x, y, width, height);
	cairo_set_source_rgba (cr, r, g, b, GPM_POPUP_WINDOW_FG_ALPHA);
	cairo_fill (cr);

	/* bar border */
	color = gtk_widget_get_style (GTK_WIDGET (popup))->dark [GTK_STATE_SELECTED];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_rectangle (cr, x, y, width, height);
	cairo_set_source_rgba (cr, r, g, b, GPM_POPUP_WINDOW_FG_ALPHA);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);

	/* bar progress */
	color = gtk_widget_get_style (GTK_WIDGET (popup))->bg [GTK_STATE_SELECTED];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_rectangle (cr, x, y, xw, height);
	cairo_set_source_rgba (cr, r, g, b, GPM_POPUP_WINDOW_FG_ALPHA);
	cairo_fill (cr);
}

/**
 * gpm_popup_window_draw_action:
 **/
static void
gpm_popup_window_draw_action (GpmPopupWindow *popup, cairo_t *cr)
{
	gint window_width;
	gint window_height;
	gdouble icon_box_width;
	gdouble icon_box_height;
	gdouble icon_box_x;
	gdouble icon_box_y;
	gdouble value_box_x;
	gdouble value_box_y;
	gdouble value_box_width;
	gdouble value_box_height;
	gboolean ret;

	gtk_window_get_size (GTK_WINDOW (popup), &window_width, &window_height);

	icon_box_width = window_width * 0.65;
	icon_box_height = window_height * 0.65;
	value_box_width = icon_box_width;
	value_box_height = window_height * 0.05;

	icon_box_x = (window_width - icon_box_width) / 2;
	icon_box_y = (window_height - icon_box_height - value_box_height) / 2;
	value_box_x = icon_box_x;
	value_box_y = icon_box_height + icon_box_y;

	ret = gpm_popup_window_render_icon (popup, cr, icon_box_x, icon_box_y, icon_box_width, icon_box_height);
	if (!ret)
		egg_warning ("failed to render");

	/* draw progress bar */
	gpm_popup_window_draw_progress_bar (popup, cr, popup->priv->value,
					    value_box_x, value_box_y,
					    value_box_width, value_box_height);
}

/**
 * gpm_popup_window_expose_event_cb:
 **/
static gboolean
gpm_popup_window_expose_event_cb (GtkWidget *widget, GdkEventExpose *event, GpmPopupWindow *popup)
{
	cairo_t *context;
	cairo_t *cr;
	cairo_surface_t *surface;
	gint width;
	gint height;
	GdkColor color;
	gdouble r, g, b;

	/* ignore for non-composite windows */
	if (!popup->priv->is_composited)
		goto out;

	context = gdk_cairo_create (gtk_widget_get_window (GTK_WIDGET (popup)));

	cairo_set_operator (context, CAIRO_OPERATOR_SOURCE);
	gtk_window_get_size (GTK_WINDOW (widget), &width, &height);

	surface = cairo_surface_create_similar (cairo_get_target (context),
						CAIRO_CONTENT_COLOR_ALPHA,
						width, height);

	if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
		goto done;

	cr = cairo_create (surface);
	if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
		goto done;

	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_paint (cr);

	/* draw a box */
	gpm_popup_window_curved_rectangle (cr, 0.5, 0.5, width-1, height-1, height / 10);
	color = gtk_widget_get_style (GTK_WIDGET (popup))->bg [GTK_STATE_NORMAL];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_set_source_rgba (cr, r, g, b, GPM_POPUP_WINDOW_BG_ALPHA);
	cairo_fill_preserve (cr);

	color = gtk_widget_get_style (GTK_WIDGET (popup))->fg [GTK_STATE_NORMAL];
	r = (float)color.red / 65535.0;
	g = (float)color.green / 65535.0;
	b = (float)color.blue / 65535.0;
	cairo_set_source_rgba (cr, r, g, b, GPM_POPUP_WINDOW_BG_ALPHA);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);

	/* draw action */
	gpm_popup_window_draw_action (popup, cr);

	cairo_destroy (cr);

	/* Make sure we have a transparent background */
	cairo_rectangle (context, 0, 0, width, height);
	cairo_set_source_rgba (context, 0.0, 0.0, 0.0, 0.0);
	cairo_fill (context);

	cairo_set_source_surface (context, surface, 0, 0);
	cairo_paint_with_alpha (context, popup->priv->fade_out_alpha);

done:
	if (surface != NULL)
		cairo_surface_destroy (surface);
	cairo_destroy (context);
out:
	return FALSE;
}

/**
 * gpm_popup_window_move_to_screen_base:
 **/
static void
gpm_popup_window_move_to_screen_base (GtkWidget *widget)
{
	gint orig_w, orig_h;
	gint screen_w, screen_h;
	gint x, y;
	gint pointer_x, pointer_y;
	GtkRequisition widget_req;
	GdkScreen *pointer_screen = NULL;
	GdkRectangle geometry;
	gint monitor = 0;
	GdkScreen *current_screen;

	current_screen = gdk_screen_get_default ();
	gtk_window_set_screen (GTK_WINDOW (widget), current_screen);

	/* get the window size - if the window hasn't been mapped, it doesn't
	 * necessarily know its true size, yet, so we need to jump through hoops */
	gtk_window_get_default_size (GTK_WINDOW (widget), &orig_w, &orig_h);
	gtk_widget_size_request (widget, &widget_req);

	if (widget_req.width > orig_w)
		orig_w = widget_req.width;
	if (widget_req.height > orig_h)
		orig_h = widget_req.height;

	gdk_display_get_pointer (gdk_screen_get_display (current_screen),
				 &pointer_screen, &pointer_x, &pointer_y, NULL);
	/* is the pointer is on the current screen */
	if (pointer_screen == current_screen)
		monitor = gdk_screen_get_monitor_at_point (current_screen, pointer_x, pointer_y);

	/* get monitor size */
	gdk_screen_get_monitor_geometry (current_screen, monitor, &geometry);
	screen_w = geometry.width;
	screen_h = geometry.height;

	/* put two thirds down */
	x = ((screen_w - orig_w) / 2) + geometry.x;
	y = geometry.y + (screen_h / 2) + (screen_h / 2 - orig_h) / 2;

	gtk_window_move (GTK_WINDOW (widget), x, y);
}

/**
 * gpm_popup_window_show:
 **/
static void
gpm_popup_window_show (GtkWidget *widget)
{
	GpmPopupWindow *popup;

	/* put two thirds down */
	gpm_popup_window_move_to_screen_base (widget);

	if (GTK_WIDGET_CLASS (gpm_popup_window_parent_class)->show)
		GTK_WIDGET_CLASS (gpm_popup_window_parent_class)->show (widget);

	popup = GPM_POPUP_WINDOW (widget);
	gpm_popup_window_remove_hide_timeout (popup);
	gpm_popup_window_add_hide_timeout (popup);
}

/**
 * gpm_popup_window_hide:
 **/
static void
gpm_popup_window_hide (GtkWidget *widget)
{
	GpmPopupWindow *popup;

	if (GTK_WIDGET_CLASS (gpm_popup_window_parent_class)->hide)
		GTK_WIDGET_CLASS (gpm_popup_window_parent_class)->hide (widget);

	popup = GPM_POPUP_WINDOW (widget);
	gpm_popup_window_remove_hide_timeout (popup);
}

/**
 * gpm_popup_window_realize:
 **/
static void
gpm_popup_window_realize (GtkWidget *widget)
{
	GtkAllocation allocation;
	GdkColormap *colormap;
	GdkBitmap *mask;
	cairo_t *cr;

	colormap = gdk_screen_get_rgba_colormap (gtk_widget_get_screen (widget));

	if (colormap != NULL)
		gtk_widget_set_colormap (widget, colormap);

	if (GTK_WIDGET_CLASS (gpm_popup_window_parent_class)->realize)
		GTK_WIDGET_CLASS (gpm_popup_window_parent_class)->realize (widget);

	gtk_widget_get_allocation (widget, &allocation);
	mask = gdk_pixmap_new (gtk_widget_get_window (widget),
			       allocation.width,
			       allocation.height, 1);
	cr = gdk_cairo_create (mask);

	cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.0f);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint (cr);

	/* make the whole window ignore events */
	gdk_window_input_shape_combine_mask (gtk_widget_get_window (widget), mask, 0, 0);
	g_object_unref (mask);
	cairo_destroy (cr);
}

/**
 * gpm_popup_window_finalize:
 **/
static void
gpm_popup_window_finalize (GObject *object)
{
	GpmPopupWindow *popup;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_POPUP_WINDOW (object));

	popup = GPM_POPUP_WINDOW (object);
	popup->priv = GPM_POPUP_WINDOW_GET_PRIVATE (popup);

	g_free (popup->priv->icon_name);
	if (popup->priv->hide_timeout_id != 0)
		g_source_remove (popup->priv->hide_timeout_id);
	if (popup->priv->fade_timeout_id != 0)
		g_source_remove (popup->priv->fade_timeout_id);

	G_OBJECT_CLASS (gpm_popup_window_parent_class)->finalize (object);
}

/**
 * gpm_popup_window_class_init:
 **/
static void
gpm_popup_window_class_init (GpmPopupWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = gpm_popup_window_finalize;

	widget_class->show = gpm_popup_window_show;
	widget_class->hide = gpm_popup_window_hide;
	widget_class->realize = gpm_popup_window_realize;

	g_type_class_add_private (klass, sizeof (GpmPopupWindowPrivate));
}

/**
 * gpm_popup_window_setup:
 **/
static void
gpm_popup_window_setup (GpmPopupWindow *popup)
{
	gdouble scalew, scaleh, scale;
	gint size;
	GdkScreen *screen;
	GtkBuilder *builder;
	gchar **objects;

	/* remove non-composited frame */
	if (popup->priv->frame != NULL) {
		gtk_container_remove (GTK_CONTAINER (popup), popup->priv->frame);
		popup->priv->frame = NULL;
		popup->priv->image = NULL;
		popup->priv->progress = NULL;
	}

	if (popup->priv->is_composited) {

		gtk_window_set_decorated (GTK_WINDOW (popup), FALSE);
		gtk_widget_set_app_paintable (GTK_WIDGET (popup), TRUE);

		/* assume 130xw30 on a 640x480 display and scale from there */
		screen = gtk_widget_get_screen (GTK_WIDGET (popup));
		scalew = gdk_screen_get_width (screen) / 640.0;
		scaleh = gdk_screen_get_height (screen) / 480.0;
		scale = MIN (scalew, scaleh);
		size = 130 * MAX (1, scale);

		gtk_widget_set_size_request (GTK_WIDGET (popup), size, size);
		gtk_window_set_default_size (GTK_WINDOW (popup), size, size);

	} else {
		builder = gtk_builder_new ();
		objects = g_strsplit ("frame_popup", ",", -1);
		gtk_builder_add_objects_from_file (builder, GPM_DATA "/gpm-feedback-widget.ui", objects, NULL);
		g_strfreev (objects);

		/* setup non-composited window as a fallback */
		popup->priv->image = GTK_IMAGE (gtk_builder_get_object (builder, "image_popup"));
		popup->priv->progress = GTK_WIDGET (gtk_builder_get_object (builder, "progressbar_popup"));
		popup->priv->frame = GTK_WIDGET (gtk_builder_get_object (builder, "frame_popup"));
		if (popup->priv->frame != NULL) {
			gtk_container_add (GTK_CONTAINER (popup), popup->priv->frame);
			gtk_widget_show_all (popup->priv->frame);
		}

		/* if we're going from composited to non-composited, set this now */
		if (popup->priv->icon_name != NULL)
			gtk_image_set_from_icon_name (popup->priv->image, popup->priv->icon_name, GTK_ICON_SIZE_DIALOG);

		gtk_widget_set_app_paintable (GTK_WIDGET (popup), FALSE);
		gtk_widget_set_size_request (GTK_WIDGET (popup), -1, -1);
		gtk_window_set_default_size (GTK_WINDOW (popup), -1, -1);

		/* stay alive until the window takes ownership of the frame (and its children) */
		g_object_unref (builder);
		gtk_widget_hide (GTK_WIDGET (popup));
	}
}

/**
 * gpm_popup_window_screen_changed_cb:
 **/
static void
gpm_popup_window_screen_changed_cb (GdkScreen *screen, GpmPopupWindow *popup)
{
	popup->priv->is_composited = gdk_screen_is_composited (screen);
	egg_debug ("is_composited=%i", popup->priv->is_composited);
	gpm_popup_window_setup (popup);
}

/**
 * gpm_popup_window_init:
 **/
static void
gpm_popup_window_init (GpmPopupWindow *popup)
{
	GdkScreen *screen;

	popup->priv = GPM_POPUP_WINDOW_GET_PRIVATE (popup);
	popup->priv->fade_out_alpha = 1.0;
	popup->priv->frame = NULL;

	screen = gtk_widget_get_screen (GTK_WIDGET (popup));

	popup->priv->is_composited = gdk_screen_is_composited (screen);
	g_signal_connect (screen, "size-changed", G_CALLBACK (gpm_popup_window_screen_changed_cb), popup);
	g_signal_connect (screen, "monitors-changed", G_CALLBACK (gpm_popup_window_screen_changed_cb), popup);
	g_signal_connect (screen, "composited-changed", G_CALLBACK (gpm_popup_window_screen_changed_cb), popup);

	/* needed for composite window */
	g_signal_connect (popup, "expose-event", G_CALLBACK (gpm_popup_window_expose_event_cb), popup);

	/* setup window */
	gpm_popup_window_setup (popup);
}

/**
 * gpm_popup_window_new:
 **/
GtkWidget *
gpm_popup_window_new (void)
{
	GObject *object;
	object = g_object_new (GPM_TYPE_POPUP_WINDOW,
			       "type", GTK_WINDOW_POPUP,
			       "type-hint", GDK_WINDOW_TYPE_HINT_NOTIFICATION,
			       "skip-taskbar-hint", TRUE,
			       "skip-pager-hint", TRUE,
			       "focus-on-map", FALSE,
			       NULL);
	return GTK_WIDGET (object);
}
