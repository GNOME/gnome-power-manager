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

#include <glib.h>

#include <glade/glade.h>
#include <libgnomeui/gnome-help.h>
#include <gtk/gtk.h>

#include "gpm-feedback-widget.h"
#include "gpm-stock-icons.h"
#include "gpm-debug.h"

static void     gpm_feedback_class_init (GpmFeedbackClass *klass);
static void     gpm_feedback_init       (GpmFeedback      *feedback);
static void     gpm_feedback_finalize   (GObject	  *object);

#define GPM_FEEDBACK_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_FEEDBACK, GpmFeedbackPrivate))

#define GPM_FEEDBACK_TIMOUT		2	/* seconds */

struct GpmFeedbackPrivate
{
	GladeXML		*xml;
	GtkWidget		*main_window;
	GtkWidget		*progress;
	gint			 refcount;
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
 * gpm_info_log_do_poll:
 * @data: gpointer to this info class instance
 *
 * This is the callback to get the log data every timeout period, where we have
 * to add points to the database and also update the graphs.
 **/
static gboolean
gpm_feedback_auto_close (gpointer data)
{
	GpmFeedback *feedback = (GpmFeedback*) data;
	feedback->priv->refcount--;
	if (feedback->priv->refcount == 0) {
		gpm_debug ("Auto-closing feedback widget");
		gtk_widget_hide (feedback->priv->main_window);
	}
	return FALSE;
}

gboolean
gpm_feedback_display_value (GpmFeedback *feedback, gfloat value)
{
	g_return_val_if_fail (feedback != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_FEEDBACK (feedback), FALSE);

	gpm_debug ("Displaying %f on feedback widget", value);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (feedback->priv->progress), value);
	gtk_widget_show_all (feedback->priv->main_window);
	/* set up the timer auto-close thing */
	g_idle_remove_by_data (feedback);
	g_timeout_add (GPM_FEEDBACK_TIMOUT * 1000, gpm_feedback_auto_close, feedback);
	feedback->priv->refcount++;
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

	gpm_debug ("Using icon name '%s'", icon_name);
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

	/* initialise the window */
	feedback->priv->xml = glade_xml_new (GPM_DATA "/gpm-feedback-widget.glade", NULL, NULL);
	if (! feedback->priv->xml) {
		gpm_critical_error ("Can't find gpm-feedback-widget.glade");
	}
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

	if (feedback->priv->icon_name != NULL) {
		g_free (feedback->priv->icon_name);
	}

	/* FIXME: we should unref some stuff */

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
