/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2008 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib.h>
#include <gst/gst.h>
#include <gconf/gconf-client.h>

#include "gpm-conf.h"
#include "gpm-common.h"
#include "gpm-sound.h"
#include "egg-debug.h"

#define GPM_SOUND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SOUND, GpmSoundPrivate))

struct GpmSoundPrivate
{
	GConfClient		*gconf_client;
	GstElement		*playbin;
};

G_DEFINE_TYPE (GpmSound, gpm_sound, G_TYPE_OBJECT)

/**
 * gpm_sound_force:
 * @sound: This class instance
 **/
static gboolean
gpm_sound_play (GpmSound *sound, const char *filename)
{
	char *uri, *fname;

	/* Build the URI from the filename */
	fname = g_build_filename (GPM_DATA, filename, NULL);
	if (fname == NULL)
		return FALSE;
	uri = g_filename_to_uri (fname, NULL, NULL);
	g_free (fname);
	if (uri == NULL)
		return FALSE;

	/* set up new playbin source */
	gst_element_set_state (sound->priv->playbin, GST_STATE_NULL);
	g_object_set (G_OBJECT (sound->priv->playbin), "uri", uri, NULL);
	gst_element_set_state (sound->priv->playbin, GST_STATE_PLAYING);

	g_free (uri);
	return TRUE;
}

/**
 * gpm_sound_force:
 * @sound: This class instance
 **/
gboolean
gpm_sound_force (GpmSound *sound, GpmSoundAction action)
{
	const char *filename = NULL;

	if (action == GPM_SOUND_AC_UNPLUGGED)
		filename = "gpm-unplugged.wav";
	else if (action == GPM_SOUND_POWER_LOW)
		filename = "gpm-critical-power.wav";
	else if (action == GPM_SOUND_SUSPEND_FAILURE)
		filename = "gpm-suspend-failure.wav";
	else
		egg_error ("enum %i not known", action);

	gpm_sound_play (sound, filename);
	return TRUE;
}

/**
 * gpm_sound_event:
 * @sound: This class instance
 **/
gboolean
gpm_sound_event (GpmSound *sound, GpmSoundAction action)
{
	gboolean ret;
	ret = gconf_client_get_bool (sound->priv->gconf_client, GPM_CONF_UI_ENABLE_BEEPING, NULL);
	if (ret)
		gpm_sound_force (sound, action);
	return ret;
}

/**
 * gpm_sound_finalize:
 **/
static void
gpm_sound_finalize (GObject *object)
{
	GpmSound *sound;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SOUND (object));
	sound = GPM_SOUND (object);

	g_object_unref (sound->priv->gconf_client);

	/* stop and close */
	gst_element_set_state (sound->priv->playbin, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (sound->priv->playbin));

	G_OBJECT_CLASS (gpm_sound_parent_class)->finalize (object);
}

/**
 * gpm_sound_class_init:
 **/
static void
gpm_sound_class_init (GpmSoundClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_sound_finalize;

	g_type_class_add_private (klass, sizeof (GpmSoundPrivate));
}

/**
 * gpm_sound_class_init:
 *
 * Needed to change state of the playbin back to NULL to avoid lockups
 **/
static void
gpm_sound_gst_bus_cb (GstBus *bus, GstMessage *message, GpmSound *sound)
{
	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_ERROR: {
		GError	*error;
		gchar	*debug;
		
		gst_element_set_state (sound->priv->playbin, GST_STATE_NULL);

		gst_message_parse_error (message, &error, &debug);
		egg_warning ("%s (%s)", error->message, debug);

		g_error_free (error);
		g_free (debug);
		break;
	}
	case GST_MESSAGE_EOS: {
		gst_element_set_state (sound->priv->playbin, GST_STATE_NULL);
		break;
	}
	default:
		break;
	}
}

/**
 * gpm_sound_init:
 * @sound: This class instance
 *
 * initialises the sound class. NOTE: We expect sound objects
 * to *NOT* be removed or added during the session.
 * We only control the first sound object if there are more than one.
 **/
static void
gpm_sound_init (GpmSound *sound)
{
	GstElement *audio_sink;
	GstBus *bus;

	sound->priv = GPM_SOUND_GET_PRIVATE (sound);

	sound->priv->gconf_client = gconf_client_get_default ();

	/* Instatiate the audio sink ourselves, and set the profile
	 * so the right output is used */
	audio_sink = gst_element_factory_make ("gconfaudiosink", "audio-sink");
	if (audio_sink == NULL)
		audio_sink = gst_element_factory_make ("autoaudiosink", "audio-sink");
	if (audio_sink != NULL && g_object_class_find_property (G_OBJECT_GET_CLASS (audio_sink), "profile"))
		g_object_set (G_OBJECT (audio_sink), "profile", 0, NULL);
	/* we keep this alive for speed */
	sound->priv->playbin = gst_element_factory_make ("playbin", "play");
	if (audio_sink != NULL)
	g_object_set (sound->priv->playbin, "audio-sink", audio_sink, NULL);

	bus = gst_element_get_bus (GST_ELEMENT (sound->priv->playbin));
	gst_bus_add_signal_watch (bus);
	g_signal_connect (bus, "message", G_CALLBACK (gpm_sound_gst_bus_cb), sound);
	gst_object_unref (bus);
}

/**
 * gpm_sound_new:
 * Return value: A new sound class instance.
 **/
GpmSound *
gpm_sound_new (void)
{
	GpmSound *sound;
	sound = g_object_new (GPM_TYPE_SOUND, NULL);
	return GPM_SOUND (sound);
}
