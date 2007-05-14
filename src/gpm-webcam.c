/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <gst/gst.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-conf.h"
#include "gpm-webcam.h"

#define GPM_WEBCAM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_WEBCAM, GpmWebcamPrivate))

struct GpmWebcamPrivate
{
	gboolean		 enable_beeping;
	GpmConf			*conf;
};

G_DEFINE_TYPE (GpmWebcam, gpm_webcam, G_TYPE_OBJECT)
static gpointer gpm_webcam_object = NULL;

/**
 * gpm_webcam_get_image:
 * @webcam: This class instance
 * @filename: filename to save to
 *
 * effectivly does:
 * gst-launch-0.10 v4lsrc autoprobe-fps=false device=/dev/video0 \
 *         ! ffmpegcolorspace ! pngenc ! filesink location=foo.png
 *
 **/
static gboolean
gpm_webcam_get_image (GpmWebcam *webcam, const gchar *filename)
{
	gboolean ret;
	gboolean did_we_get_an_image = TRUE;
	GstElement *pipeline;
	GstElement *source;
	GstElement *pngenc;
	GstElement *colorspace;
	GstElement *sink;
	GstStateChangeReturn retval;

	/* initialize GStreamer */
//	gst_init (&argc, &argv);
	gst_init (NULL, NULL);

	/* create elements */
	pipeline = gst_pipeline_new ("webcam");

	gpm_debug ("Creating source");
	source = gst_element_factory_make ("v4lsrc", NULL);
	// USE HAL TO GET DEFAULT DEVICE!
	g_object_set (G_OBJECT (source), "device", "/dev/video0", NULL);
	g_object_set (G_OBJECT (source), "autoprobe-fps", FALSE, NULL);

	colorspace = gst_element_factory_make ("ffmpegcolorspace", NULL);
	pngenc = gst_element_factory_make ("pngenc", NULL);
	sink = gst_element_factory_make ("filesink", NULL);
	g_object_set (G_OBJECT (sink), "location", filename, NULL);

	/* add items to bin */
	gst_bin_add_many (GST_BIN (pipeline), source, colorspace, pngenc, sink, NULL);

	gpm_debug ("Link together");
	ret = gst_element_link_many (source, colorspace, pngenc, sink, NULL);
	if (ret == FALSE) {
		gpm_warning ("could not link");
		did_we_get_an_image = FALSE;
		goto Cleanup;
	}

	gpm_debug ("Set playing");
	retval = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (retval == GST_STATE_NULL) {
		gpm_warning ("error");
		did_we_get_an_image = FALSE;
		goto Cleanup;
	}
	/* assume async */

	retval = gst_element_get_state  (pipeline, NULL, NULL, 1000*1000*1000);
	if (retval == GST_STATE_CHANGE_SUCCESS) {
		gpm_debug ("finished!");
		did_we_get_an_image = TRUE;
		goto Cleanup;
	}
	if (retval == GST_STATE_NULL) {
		gpm_warning ("error");
		did_we_get_an_image = FALSE;
		goto Cleanup;
	}

	/* timeout */
	did_we_get_an_image = FALSE;

Cleanup:
	gpm_debug ("Cleanup");
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (pipeline));

	return did_we_get_an_image;
}

/**
 * gpm_webcam_get_average_brightness_of_pixbuf:
 * @webcam: This class instance
 * @brightness: average brightness retval
 *
 **/
gboolean
gpm_webcam_get_average_brightness_of_pixbuf (GdkPixbuf *pixbuf, gfloat *brightness)
{
	guint width;
	guint height;
	guint index;
	guchar *data;
	guint x, y;
	gfloat stride;
	gfloat pixel;
	gfloat average = 0;

	/* parse the data */
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	data = gdk_pixbuf_get_pixels (pixbuf);

	gpm_debug ("parsing %ix%i", width, height);
	/* calculate the average brightness, do width inside height to maximise cache hit */
	for (y=0; y<height; y++) {
		stride = 0;
		for (x=0; x<width; x++) {
			index = ((y*3)*width) + (x*3);
			pixel = (data[index+0] + data[index+1] + data[index+2]) / 3;
			stride += pixel;
		}
		/* average per stride */
		stride /= width;
		average += stride;
		gpm_debug ("stride = %f", stride);
	}
	average /= height;
	/* average per pixel */
	gpm_debug ("average = %f", average);

	if (average < 0 || average > 255) {
		gpm_warning ("brightness invalid!");
		*brightness = 0.0;
		return FALSE;
	}

	*brightness = average / 255;
	return TRUE;
}

/**
 * gpm_webcam_get_brightness:
 * @webcam: This class instance
 * @brightness: 0..1 floating point, not normalised in any way
 **/
gboolean
gpm_webcam_get_brightness (GpmWebcam *webcam, gfloat *brightness)
{
	gboolean ret;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	const gchar *filename = "/tmp/woot.png";

	/* try to get a fresh image */
	ret = gpm_webcam_get_image (webcam, filename);
	if (ret == FALSE) {
		*brightness = 0.0;
		return FALSE;
	}

	/* open the file we just took */
	pixbuf = gdk_pixbuf_new_from_file (filename, &error);
	if (error != NULL) {
		gpm_error ("error set");
	}

	ret = gpm_webcam_get_average_brightness_of_pixbuf (pixbuf, brightness);

	/* delete the file */
	g_unlink (filename);

	return ret;
}

/**
 * gpm_webcam_has_hardware:
 * @webcam: This class instance
 **/
gboolean
gpm_webcam_has_hardware (void)
{
	return TRUE;
}

/**
 * gpm_webcam_finalize:
 **/
static void
gpm_webcam_finalize (GObject *object)
{
	GpmWebcam *webcam;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_WEBCAM (object));
	webcam = GPM_WEBCAM (object);

	g_object_unref (webcam->priv->conf);

	G_OBJECT_CLASS (gpm_webcam_parent_class)->finalize (object);
}

/**
 * gpm_webcam_class_init:
 **/
static void
gpm_webcam_class_init (GpmWebcamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_webcam_finalize;
	g_type_class_add_private (klass, sizeof (GpmWebcamPrivate));
}

/**
 * gpm_webcam_init:
 * @webcam: This class instance
 *
 * initialises the webcam class. NOTE: We expect webcam objects
 * to *NOT* be removed or added during the session.
 * We only control the first webcam object if there are more than one.
 **/
static void
gpm_webcam_init (GpmWebcam *webcam)
{
	webcam->priv = GPM_WEBCAM_GET_PRIVATE (webcam);

	webcam->priv->conf = gpm_conf_new ();

	/* do we beep? */
	gpm_conf_get_bool (webcam->priv->conf, GPM_CONF_ENABLE_BEEPING, &webcam->priv->enable_beeping);
}

/**
 * gpm_webcam_new:
 * Return value: A new webcam class instance.
 **/
GpmWebcam *
gpm_webcam_new (void)
{
	if (gpm_webcam_object != NULL) {
		g_object_ref (gpm_webcam_object);
	} else {
		gpm_webcam_object = g_object_new (GPM_TYPE_WEBCAM, NULL);
		g_object_add_weak_pointer (gpm_webcam_object, &gpm_webcam_object);
	}
	return GPM_WEBCAM (gpm_webcam_object);
}
