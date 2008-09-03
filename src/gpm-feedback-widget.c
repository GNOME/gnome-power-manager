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

#include <glib.h>

#include <glade/glade.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "gpm-feedback-widget.h"
#include "gpm-stock-icons.h"
#include "gpm-refcount.h"
#include "egg-debug.h"

static void     gpm_feedback_class_init (GpmFeedbackClass *klass);
static void     gpm_feedback_init       (GpmFeedback      *feedback);
static void     gpm_feedback_finalize   (GObject	  *object);
static void	gpm_feedback_show	(GtkWidget 	  *widget);

#define GPM_FEEDBACK_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_FEEDBACK, GpmFeedbackPrivate))

#define GPM_FEEDBACK_TIMOUT		2000	/* ms */

struct GpmFeedbackPrivate
{
	GladeXML		*xml;
	GtkWidget		*main_window;
	GtkWidget		*progress;
	GpmRefcount		*refcount;
	gchar			*icon_name;
};

G_DEFINE_TYPE (GpmFeedback, gpm_feedback, G_TYPE_OBJECT)

/**
 * gpm_feedback_class_init:
 * @klass: This feedback class instance
 **/
static void
gpm_feedback_class_init (GpmFeedbackClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_feedback_finalize;
	g_type_class_add_private (klass, sizeof (GpmFeedbackPrivate));
}

/**
 * gpm_feedback_show:
 **/
static void
gpm_feedback_show (GtkWidget *widget)
{
	int orig_w, orig_h;
	int screen_w, screen_h;
	int x, y;
	int pointer_x, pointer_y;
	GtkRequisition win_req;
	GdkScreen *pointer_screen;
	GdkRectangle geometry;
	int monitor;
	GdkScreen *current_screen;
	
	current_screen = gdk_screen_get_default();

	gtk_window_set_screen (GTK_WINDOW (widget), current_screen);

	/* get the window size - if the window hasn't been mapped, it doesn't
	 * necessarily know its true size, yet, so we need to jump through hoops */
	gtk_window_get_default_size (GTK_WINDOW (widget), &orig_w, &orig_h);
	gtk_widget_size_request (widget, &win_req);

	if (win_req.width > orig_w)
		orig_w = win_req.width;
	if (win_req.height > orig_h)
		orig_h = win_req.height;

	pointer_screen = NULL;
	gdk_display_get_pointer (gdk_screen_get_display (current_screen),
				 &pointer_screen, &pointer_x, &pointer_y, NULL);
	if (pointer_screen != current_screen) {
		/* The pointer isn't on the current screen, so just
		 * assume the default monitor */
		monitor = 0;
	} else {
		monitor = gdk_screen_get_monitor_at_point (current_screen, pointer_x, pointer_y);
	}

	gdk_screen_get_monitor_geometry (current_screen, monitor, &geometry);

	screen_w = geometry.width;
	screen_h = geometry.height;

	x = ((screen_w - orig_w) / 2) + geometry.x;
	y = geometry.y + (screen_h / 2) + (screen_h / 2 - orig_h) / 2;

	gtk_window_move (GTK_WINDOW (widget), x, y);

	gtk_widget_show (widget);
	gdk_display_sync (gdk_screen_get_display (current_screen));
}

/**
 * gpm_feedback_close_window:
 * @data: gpointer to this class instance
 **/
static void
gpm_feedback_close_window (GpmRefcount *refcount,
			   GpmFeedback *feedback)
{
	egg_debug ("Closing feedback widget");
	gtk_widget_hide (feedback->priv->main_window);
}

gboolean
gpm_feedback_display_value (GpmFeedback *feedback, gfloat value)
{
	g_return_val_if_fail (feedback != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_FEEDBACK (feedback), FALSE);

	egg_debug ("Displaying %f on feedback widget", value);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (feedback->priv->progress), value);
	gpm_feedback_show (feedback->priv->main_window);

	/* set up the window auto-close */
	gpm_refcount_add (feedback->priv->refcount);

	return TRUE;
}

gboolean
gpm_feedback_set_icon_name (GpmFeedback *feedback, const gchar *icon_name)
{
	GtkWidget *image;

	g_return_val_if_fail (feedback != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_FEEDBACK (feedback), FALSE);
	g_return_val_if_fail (icon_name != NULL, FALSE);

	/* if name already set then free */
	if (feedback->priv->icon_name != NULL) {
		g_free (feedback->priv->icon_name);
	}

	egg_debug ("Using icon name '%s'", icon_name);
	feedback->priv->icon_name = g_strdup (icon_name);

	image = glade_xml_get_widget (feedback->priv->xml, "image");
	gtk_image_set_from_icon_name  (GTK_IMAGE (image),
				       feedback->priv->icon_name,
				       GTK_ICON_SIZE_DIALOG);

	return TRUE;
}

/**
 * gpm_feedback_init:
 * @feedback: This feedback class instance
 **/
static void
gpm_feedback_init (GpmFeedback *feedback)
{
	feedback->priv = GPM_FEEDBACK_GET_PRIVATE (feedback);
	feedback->priv->refcount = 0;
	feedback->priv->icon_name = NULL;

	feedback->priv->refcount = gpm_refcount_new ();
	g_signal_connect (feedback->priv->refcount, "refcount-zero",
			  G_CALLBACK (gpm_feedback_close_window), feedback);
	gpm_refcount_set_timeout (feedback->priv->refcount, GPM_FEEDBACK_TIMOUT);

	/* initialise the window */
	feedback->priv->xml = glade_xml_new (GPM_DATA "/gpm-feedback-widget.glade", NULL, NULL);
	if (feedback->priv->xml == NULL)
		egg_error ("Can't find gpm-feedback-widget.glade");
	feedback->priv->main_window = glade_xml_get_widget (feedback->priv->xml, "main_window");

	feedback->priv->progress = glade_xml_get_widget (feedback->priv->xml, "progressbar");
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (feedback->priv->progress), 0.0f);
	gtk_widget_set_sensitive (feedback->priv->progress, FALSE);

	/* hide until we get a request */
	gtk_widget_hide (feedback->priv->main_window);
}

/**
 * gpm_feedback_finalize:
 * @object: This feedback class instance
 **/
static void
gpm_feedback_finalize (GObject *object)
{
	GpmFeedback *feedback;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_FEEDBACK (object));

	feedback = GPM_FEEDBACK (object);
	feedback->priv = GPM_FEEDBACK_GET_PRIVATE (feedback);

	g_free (feedback->priv->icon_name);
	if (feedback->priv->refcount != NULL)
		g_object_unref (feedback->priv->refcount);

	G_OBJECT_CLASS (gpm_feedback_parent_class)->finalize (object);
}

/**
 * gpm_feedback_new:
 * Return value: new GpmFeedback instance.
 **/
GpmFeedback *
gpm_feedback_new (void)
{
	GpmFeedback *feedback;
	feedback = g_object_new (GPM_TYPE_FEEDBACK, NULL);
	return GPM_FEEDBACK (feedback);
}

