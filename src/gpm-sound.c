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

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gst/gst.h>

#include "gpm-ac-adapter.h"
#include "gpm-common.h"
#include "gpm-control.h"
#include "egg-debug.h"
#include "gpm-conf.h"
#include "gpm-sound.h"

#define GPM_SOUND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SOUND, GpmSoundPrivate))

struct GpmSoundPrivate
{
	gboolean		 enable_beeping;
	GpmAcAdapter		*ac_adapter;
	GpmConf			*conf;
	GpmControl		*control;
	GstElement		*playbin;
};

G_DEFINE_TYPE (GpmSound, gpm_sound, G_TYPE_OBJECT)
static gpointer gpm_sound_object = NULL;

/**
 * gpm_sound_force:
 * @sound: This class instance
 **/
static gboolean
gpm_sound_play (GpmSound   *sound,
		const char *filename)
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
gpm_sound_force (GpmSound       *sound,
		 GpmSoundAction  action)
{
	const char *filename = NULL;

	if (action == GPM_SOUND_AC_UNPLUGGED) {
		filename = "gpm-unplugged.wav";
	} else if (action == GPM_SOUND_POWER_LOW) {
		filename = "gpm-critical-power.wav";
	} else if (action == GPM_SOUND_SUSPEND_FAILURE) {
		filename = "gpm-suspend-failure.wav";
	} else {
		g_error ("enum %i not known", action);
	}

	gpm_sound_play (sound, filename);
	return TRUE;
}

/**
 * gpm_sound_event:
 * @sound: This class instance
 **/
gboolean
gpm_sound_event (GpmSound       *sound,
		 GpmSoundAction  action)
{
	if (sound->priv->enable_beeping) {
		gpm_sound_force (sound, action);
	}
	return TRUE;
}

/**
 * conf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
conf_key_changed_cb (GpmConf     *conf,
		     const gchar *key,
		     GpmSound    *sound)
{
	if (strcmp (key, GPM_CONF_UI_ENABLE_BEEPING) == 0) {
		gpm_conf_get_bool (sound->priv->conf, GPM_CONF_UI_ENABLE_BEEPING,
				   &sound->priv->enable_beeping);
	}
}

/**
 * ac_adapter_changed_cb:
 * @ac_adapter: The ac_adapter class instance
 * @on_ac: if we are on AC ac_adapter
 * @sound: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
ac_adapter_changed_cb (GpmAcAdapter *ac_adapter,
		       gboolean      on_ac,
		       GpmSound     *sound)
{
	if (on_ac == FALSE) {
		gpm_sound_event (sound, GPM_SOUND_AC_UNPLUGGED);
	}
}

/**
 * control_sleep_failure_cb:
 *
 * Sleep failed for some reason, alert the user.
 **/
static void
control_sleep_failure_cb (GpmControl      *control,
			  GpmControlAction action,
			  GpmSound	  *sound)
{
	gpm_sound_event (sound, GPM_SOUND_SUSPEND_FAILURE);
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

	g_object_unref (sound->priv->conf);
	g_object_unref (sound->priv->ac_adapter);
	g_object_unref (sound->priv->control);

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

	sound->priv->conf = gpm_conf_new ();
	g_signal_connect (sound->priv->conf, "value-changed",
			  G_CALLBACK (conf_key_changed_cb), sound);

	sound->priv->control = gpm_control_new ();
	g_signal_connect (sound->priv->control, "sleep-failure",
			  G_CALLBACK (control_sleep_failure_cb), sound);

	/* we use ac_adapter so we can make the right sound */
	sound->priv->ac_adapter = gpm_ac_adapter_new ();
	g_signal_connect (sound->priv->ac_adapter, "ac-adapter-changed",
			  G_CALLBACK (ac_adapter_changed_cb), sound);

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

	/* do we beep? */
	gpm_conf_get_bool (sound->priv->conf, GPM_CONF_UI_ENABLE_BEEPING, &sound->priv->enable_beeping);
}

/**
 * gpm_sound_new:
 * Return value: A new sound class instance.
 **/
GpmSound *
gpm_sound_new (void)
{
	if (gpm_sound_object != NULL) {
		g_object_ref (gpm_sound_object);
	} else {
		gpm_sound_object = g_object_new (GPM_TYPE_SOUND, NULL);
		g_object_add_weak_pointer (gpm_sound_object, &gpm_sound_object);
	}
	return GPM_SOUND (gpm_sound_object);
}
