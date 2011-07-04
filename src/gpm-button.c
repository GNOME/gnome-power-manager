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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <X11/X.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/XF86keysym.h>
#include <libupower-glib/upower.h>

#include "gpm-common.h"
#include "gpm-button.h"

static void     gpm_button_finalize   (GObject	      *object);

#define GPM_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BUTTON, GpmButtonPrivate))

struct GpmButtonPrivate
{
	GdkScreen		*screen;
	GdkWindow		*window;
	GHashTable		*keysym_to_name_hash;
	gchar			*last_button;
	GTimer			*timer;
	gboolean		 lid_is_closed;
	UpClient		*client;
};

enum {
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_button_object = NULL;

G_DEFINE_TYPE (GpmButton, gpm_button, G_TYPE_OBJECT)

#define GPM_BUTTON_DUPLICATE_TIMEOUT	0.125f

/**
 * gpm_button_emit_type:
 **/
static gboolean
gpm_button_emit_type (GpmButton *button, const gchar *type)
{
	g_return_val_if_fail (GPM_IS_BUTTON (button), FALSE);

	/* did we just have this button before the timeout? */
	if (g_strcmp0 (type, button->priv->last_button) == 0 &&
	    g_timer_elapsed (button->priv->timer, NULL) < GPM_BUTTON_DUPLICATE_TIMEOUT) {
		g_debug ("ignoring duplicate button %s", type);
		return FALSE;
	}

	g_debug ("emitting button-pressed : %s", type);
	g_signal_emit (button, signals [BUTTON_PRESSED], 0, type);

	/* save type and last size */
	g_free (button->priv->last_button);
	button->priv->last_button = g_strdup (type);
	g_timer_reset (button->priv->timer);

	return TRUE;
}

/**
 * gpm_button_class_init:
 * @button: This class instance
 **/
static void
gpm_button_class_init (GpmButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_button_finalize;
	g_type_class_add_private (klass, sizeof (GpmButtonPrivate));

	signals [BUTTON_PRESSED] =
		g_signal_new ("button-pressed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmButtonClass, button_pressed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * gpm_button_is_lid_closed:
 **/
gboolean
gpm_button_is_lid_closed (GpmButton *button)
{
	g_return_val_if_fail (GPM_IS_BUTTON (button), FALSE);
	return 	up_client_get_lid_is_closed (button->priv->client);
}

/**
 * gpm_button_reset_time:
 *
 * We have to refresh the event time on resume to handle duplicate buttons
 * properly when the time is significant when we suspend.
 **/
gboolean
gpm_button_reset_time (GpmButton *button)
{
	g_return_val_if_fail (GPM_IS_BUTTON (button), FALSE);
	g_timer_reset (button->priv->timer);
	return TRUE;
}

/**
 * gpm_button_client_changed_cb
 **/
static void
gpm_button_client_changed_cb (UpClient *client, GpmButton *button)
{
	gboolean lid_is_closed;

	/* get new state */
	lid_is_closed = up_client_get_lid_is_closed (button->priv->client);

	/* same state */
	if (button->priv->lid_is_closed == lid_is_closed)
		return;

	/* save state */
	button->priv->lid_is_closed = lid_is_closed;

	/* sent correct event */
	if (lid_is_closed)
		gpm_button_emit_type (button, GPM_BUTTON_LID_CLOSED);
	else
		gpm_button_emit_type (button, GPM_BUTTON_LID_OPEN);
}

/**
 * gpm_button_init:
 * @button: This class instance
 **/
static void
gpm_button_init (GpmButton *button)
{
	button->priv = GPM_BUTTON_GET_PRIVATE (button);

	button->priv->screen = gdk_screen_get_default ();
	button->priv->window = gdk_screen_get_root_window (button->priv->screen);

	button->priv->keysym_to_name_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	button->priv->last_button = NULL;
	button->priv->timer = g_timer_new ();

	button->priv->client = up_client_new ();
	button->priv->lid_is_closed = up_client_get_lid_is_closed (button->priv->client);
	g_signal_connect (button->priv->client, "changed",
			  G_CALLBACK (gpm_button_client_changed_cb), button);
}

/**
 * gpm_button_finalize:
 * @object: This class instance
 **/
static void
gpm_button_finalize (GObject *object)
{
	GpmButton *button;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BUTTON (object));

	button = GPM_BUTTON (object);
	button->priv = GPM_BUTTON_GET_PRIVATE (button);

	g_object_unref (button->priv->client);
	g_free (button->priv->last_button);
	g_timer_destroy (button->priv->timer);

	g_hash_table_unref (button->priv->keysym_to_name_hash);

	G_OBJECT_CLASS (gpm_button_parent_class)->finalize (object);
}

/**
 * gpm_button_new:
 * Return value: new class instance.
 **/
GpmButton *
gpm_button_new (void)
{
	if (gpm_button_object != NULL) {
		g_object_ref (gpm_button_object);
	} else {
		gpm_button_object = g_object_new (GPM_TYPE_BUTTON, NULL);
		g_object_add_weak_pointer (gpm_button_object, &gpm_button_object);
	}
	return GPM_BUTTON (gpm_button_object);
}
