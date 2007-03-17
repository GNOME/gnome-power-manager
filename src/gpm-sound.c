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

#include "gpm-ac-adapter.h"
#include "gpm-common.h"
#include "gpm-control.h"
#include "gpm-debug.h"
#include "gpm-conf.h"
#include "gpm-sound.h"

#define GPM_SOUND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_SOUND, GpmSoundPrivate))

struct GpmSoundPrivate
{
	gboolean		 enable_beeping;
	GpmAcAdapter		*ac_adapter;
	GpmConf			*conf;
	GpmControl		*control;
};

G_DEFINE_TYPE (GpmSound, gpm_sound, G_TYPE_OBJECT)
static gpointer gpm_sound_object = NULL;

/**
 * gpm_sound_force:
 * @sound: This class instance
 **/
gboolean
gpm_sound_force (GpmSound       *sound,
		 GpmSoundAction  action)
{
	char *command;
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

	command = g_strdup_printf ("gst-launch filesrc location=%s%s ! "
				   "decodebin ! audioconvert ! gconfaudiosink",
				   GPM_DATA, filename);

	if (! g_spawn_command_line_async (command, NULL)) {
		gpm_warning ("Couldn't execute command: %s", command);
	}

	g_free (command);
	return TRUE;
}

/**
 * gpm_sound_force:
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
	if (strcmp (key, GPM_CONF_ENABLE_BEEPING) == 0) {
		gpm_conf_get_bool (sound->priv->conf, GPM_CONF_ENABLE_BEEPING,
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
		          GpmSound        *sound)
{
	gpm_sound_event (sound, GPM_SOUND_SUSPEND_FAILURE);
}

/**
 * gpm_sound_constructor:
 **/
static GObject *
gpm_sound_constructor (GType type,
		       guint n_construct_properties,
		       GObjectConstructParam *construct_properties)
{
	GpmSound      *sound;
	GpmSoundClass *klass;
	klass = GPM_SOUND_CLASS (g_type_class_peek (GPM_TYPE_SOUND));
	sound = GPM_SOUND (G_OBJECT_CLASS (gpm_sound_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (sound);
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
	object_class->constructor  = gpm_sound_constructor;

	g_type_class_add_private (klass, sizeof (GpmSoundPrivate));
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

	/* do we beep? */
	gpm_conf_get_bool (sound->priv->conf, GPM_CONF_ENABLE_BEEPING, &sound->priv->enable_beeping);
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
