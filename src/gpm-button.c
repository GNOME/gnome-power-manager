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

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <X11/X.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "gpm-button.h"
#include "gpm-debug.h"
#include "gpm-marshal.h"

static void     gpm_button_class_init (GpmButtonClass *klass);
static void     gpm_button_init       (GpmButton      *button);
static void     gpm_button_finalize   (GObject	      *object);

#define GPM_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BUTTON, GpmButtonPrivate))

struct GpmButtonPrivate
{
	GdkScreen	*screen;
	GdkWindow	*window;
	GHashTable	*hash_to_hal;
};

enum {
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GpmButton, gpm_button, G_TYPE_OBJECT)

static GdkFilterReturn
gpm_button_filter_x_events (GdkXEvent *xevent,
			    GdkEvent  *event,
			    gpointer   data)
{
	GpmButton *button = (GpmButton *) data;
	XEvent *xev = (XEvent *) xevent;
	guint keycode, state;
	gchar *hashkey;
	gchar *key;

	keycode = xev->xkey.keycode;
	state = xev->xkey.state;
	
	if (xev->type != KeyPress) {
		hashkey = g_strdup_printf ("key_%x_%x", state, keycode);

		/* is the key string already in our DB? */
		key = g_hash_table_lookup (button->priv->hash_to_hal, hashkey);
		if (key == NULL) {
			gpm_warning ("Key '%s' not found in hash!", hashkey);
		} else {
			gpm_debug ("Key '%s' mapped to HAL key %s", hashkey, key);
			/* FIXME: we need to use the state for lid, in the current plan,
			 * or maybe better to use lid-open and lid-closed for clarity */
			g_signal_emit (button, signals [BUTTON_PRESSED], 0, key, TRUE);
		}

		g_free (hashkey);
		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}

/**
 * gpm_button_grab_keystring:
 * @button: This button class instance
 * @keystr: The key string, e.g. "<Control><Alt>F11"
 * @hashkey: A unique key made up from the modmask and keycode suitable for
 *           referencing in a hashtable.
 *           You must free this string, or specify NULL to ignore.
 *
 * Grab the key specified in the key string.
 *
 * Return value: TRUE if we parsed and grabbed okay
 **/
static gboolean
gpm_button_grab_keystring (GpmButton   *button,
			   const gchar *keystr,
			   gchar      **hashkey)
{
	guint modmask = 0;
	KeySym keysym = 0;
	KeyCode keycode = 0;
	Display *display;
	gint ret;

	/* get the current X Display */
	display = GDK_DISPLAY ();

	keysym = XStringToKeysym (keystr);

	/* no mask string, lets try find a keysym */
	if (keysym == NoSymbol) {
		gpm_debug ("'%s' not in XStringToKeysym", keystr);
		return FALSE;
	}

	keycode = XKeysymToKeycode (display, keysym);
	/* check we got a valid keycode */
	if (keycode == 0) {
		return FALSE;
	}

	/* don't abort on error */
	gdk_error_trap_push ();

	/* grab the key if possible */
	ret = XGrabKey (display, keycode, modmask,
			GDK_WINDOW_XID (button->priv->window), True,
			GrabModeAsync, GrabModeAsync);
	if (ret == BadAccess) {
		gpm_warning ("Failed to grab modmask=%u, keycode=%i",
			     modmask, keycode);
		return FALSE;
	}

	/* grab the lock key if possible */
	ret = XGrabKey (display, keycode, LockMask | modmask,
			GDK_WINDOW_XID (button->priv->window), True,
			GrabModeAsync, GrabModeAsync);
	if (ret == BadAccess) {
		gpm_warning ("Failed to grab modmask=%u, keycode=%i",
			     LockMask | modmask, keycode);
		return FALSE;
	}

	/* we are not processing the error */
	gdk_flush ();
	gdk_error_trap_pop ();

	/* generate the unique hash */
	if (hashkey) {
		*hashkey = g_strdup_printf ("key_%x_%x", modmask, keycode);
	}

	gpm_debug ("Grabbed %s (+lock) modmask=%x, keycode=%x", keystr, modmask, keycode);
	return TRUE;
}

/**
 * gpm_button_grab_keystring:
 * @button: This button class instance
 * @keystr: The key string, e.g. "<Control><Alt>F11"
 * @hashkey: A unique key made up from the modmask and keycode suitable for
 *           referencing in a hashtable.
 *           You must free this string, or specify NULL to ignore.
 *
 * Grab the key specified in the key string.
 *
 * Return value: TRUE if we parsed and grabbed okay
 **/
static gboolean
gpm_button_monitor_key (GpmButton   *button,
			   const gchar *keystr,
			   const gchar *hal_key)
{
	char *key = NULL;
	gboolean ret;

	/* is the key string already in our DB? */
	key = g_hash_table_lookup (button->priv->hash_to_hal, keystr);
	if (key != NULL) {
		gpm_warning ("Already monitoring %s", keystr);
		return FALSE;
	}

	/* try to register X event */
	ret = gpm_button_grab_keystring (button, keystr, &key);
	if (ret == FALSE) {
		gpm_warning ("Failed to grab %s", keystr);
		return FALSE;
	}	

	/* add to hash table */
	g_hash_table_insert (button->priv->hash_to_hal, key, (gpointer) hal_key);

	/* the key is freed in the hash function unref */
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
			      NULL,
			      NULL,
			      gpm_marshal_VOID__STRING_BOOLEAN,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);
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

	button->priv->hash_to_hal = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* register the brightness keys */
	gpm_button_monitor_key (button, "XF86XK_Execute", GPM_BUTTON_POWER);
	gpm_button_monitor_key (button, "XF86XK_PowerOff", GPM_BUTTON_POWER);
	gpm_button_monitor_key (button, "XF86XK_Suspend", GPM_BUTTON_SUSPEND);
	gpm_button_monitor_key (button, "XF86XK_Sleep", GPM_BUTTON_SUSPEND); /* should be configurable */
	gpm_button_monitor_key (button, "XF86XK_Hibernate", GPM_BUTTON_HIBERNATE);
	gpm_button_monitor_key (button, "XF86BrightnessUp", GPM_BUTTON_BRIGHT_UP);
	gpm_button_monitor_key (button, "XF86BrightnessDown", GPM_BUTTON_BRIGHT_DOWN);
	gpm_button_monitor_key (button, "XF86XK_ScreenSaver", GPM_BUTTON_LOCK);
	gpm_button_monitor_key (button, "XF86XK_Battery", GPM_BUTTON_BATTERY);
	gpm_button_monitor_key (button, "XF86KeyboardLightUp", GPM_BUTTON_KBD_BRIGHT_UP);
	gpm_button_monitor_key (button, "XF86KeyboardLightDown", GPM_BUTTON_KBD_BRIGHT_DOWN);
	gpm_button_monitor_key (button, "XF86KeyboardLightOnOff", GPM_BUTTON_KBD_BRIGHT_TOGGLE);

	gdk_window_add_filter (button->priv->window,
			       gpm_button_filter_x_events, (gpointer) button);
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
	
	g_hash_table_unref (button->priv->hash_to_hal);
}

/**
 * gpm_button_new:
 * Return value: new class instance.
 **/
GpmButton *
gpm_button_new (void)
{
#ifdef HAVE_XEVENTS
	GpmButton *button;
	button = g_object_new (GPM_TYPE_BUTTON, NULL);
	return GPM_BUTTON (button);
#else
	return NULL;
#endif
}
