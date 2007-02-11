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

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <X11/X.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "gpm-common.h"
#include "gpm-button.h"
#include "gpm-debug.h"
#include "gpm-hal.h"
#include "gpm-marshal.h"

static void     gpm_button_class_init (GpmButtonClass *klass);
static void     gpm_button_init       (GpmButton      *button);
static void     gpm_button_finalize   (GObject	      *object);

#define GPM_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BUTTON, GpmButtonPrivate))

struct GpmButtonPrivate
{
	GdkScreen		*screen;
	GdkWindow		*window;
	GHashTable		*hash_to_hal;
	gboolean		 lid_is_closed;
	GpmHal			*hal; /* remove when iput events is in the kernel */
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
			g_signal_emit (button, signals [BUTTON_PRESSED], 0, key);
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
gpm_button_button_key (GpmButton   *button,
		       const gchar *keystr,
		       const gchar *hal_key)
{
	char *key = NULL;
	gboolean ret;

	/* is the key string already in our DB? */
	key = g_hash_table_lookup (button->priv->hash_to_hal, keystr);
	if (key != NULL) {
		gpm_warning ("Already buttoning %s", keystr);
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
			      gpm_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * gpm_button_is_lid_closed:
 **/
gboolean
gpm_button_is_lid_closed (GpmButton *button)
{
	g_return_val_if_fail (button != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BUTTON (button), FALSE);
	return button->priv->lid_is_closed;
}

/**
 * emit_button_pressed:
 *
 * @udi: The HAL UDI
 * @details: The event details, or "" for unknown or invalid
 *				NOTE: details cannot be NULL
 *
 * Use when we want to emit a ButtonPressed event and we know the udi.
 * We can get two different types of ButtonPressed condition
 *   1. The old acpi hardware buttons
 *      udi="acpi_foo", details="";
 *      button.type="power"
 *   2. The new keyboard buttons
 *      udi="foo_Kbd_Port_logicaldev_input", details="sleep"
 *      button.type=""
 */
static void
emit_button_pressed (GpmButton *button,
		     const gchar   *udi,
		     const gchar   *details)
{
	gchar *type = NULL;
	gboolean state;
	const char *atype;

	g_return_if_fail (udi != NULL);
	g_return_if_fail (details != NULL);

	if (strcmp (details, "") == 0) {
		/* no details about the event, so we get more info
		   for type 1 buttons */
		gpm_hal_device_get_string (button->priv->hal, udi, "button.type", &type, NULL);
		/* hal may no longer be there */
		g_return_if_fail (type != NULL);
	} else {
		type = g_strdup (details);
	}
	atype = type;

	/* Buttons without state should default to true. */
	state = TRUE;
	/* we need to get the button state for lid buttons */
	if (strcmp (type, "lid") == 0) {
		gpm_hal_device_get_bool (button->priv->hal, udi, "button.state.value", &state, NULL);
	}

	/* abstact away that HAL has an extra parameter */
	if (strcmp (type, GPM_BUTTON_LID_DEP) == 0 && state == FALSE) {
		atype = GPM_BUTTON_LID_OPEN;
	} else if (strcmp (type, GPM_BUTTON_LID_DEP) == 0 && state == TRUE) {
		atype = GPM_BUTTON_LID_CLOSED;
	}

	/* filter out duplicate lid events */
	if (strcmp (atype, GPM_BUTTON_LID_CLOSED) == 0) {
		if (button->priv->lid_is_closed == TRUE) {
			gpm_debug ("ignoring duplicate lid event");
			return;
		}
		button->priv->lid_is_closed = TRUE;
	}
	if (strcmp (atype, GPM_BUTTON_LID_OPEN) == 0) {
		if (button->priv->lid_is_closed == FALSE) {
			gpm_debug ("ignoring duplicate lid event");
			return;
		}
		button->priv->lid_is_closed = FALSE;
	}

	/* the names changed in 0.5.8 */
	if (strcmp (type, GPM_BUTTON_BRIGHT_UP_DEP) == 0) {
		atype = GPM_BUTTON_BRIGHT_UP;
	} else if (strcmp (type, GPM_BUTTON_BRIGHT_DOWN_DEP) == 0) {
		atype = GPM_BUTTON_BRIGHT_DOWN;
	}

	/* we now emit all buttons, even the ones we don't know */
	gpm_debug ("emitting button-pressed : %s", atype);
	g_signal_emit (button, signals [BUTTON_PRESSED], 0, atype);

	g_free (type);
}

/**
 * hal_device_property_modified_cb:
 *
 * @udi: The HAL UDI
 * @key: Property key
 * @is_added: If the key was added
 * @is_removed: If the key was removed
 *
 * Invoked when a property of a device in the Global Device List is
 * changed, and we have we have subscribed to changes for that device.
 */
static void
hal_device_property_modified_cb (GpmHal        *hal,
				 const gchar   *udi,
				 const gchar   *key,
				 gboolean       is_added,
				 gboolean       is_removed,
				 gboolean       finally,
				 GpmButton *button)
{
	gpm_debug ("udi=%s, key=%s, added=%i, removed=%i, finally=%i",
		   udi, key, is_added, is_removed, finally);

	/* do not process keys that have been removed */
	if (is_removed) {
		return;
	}

	/* only match button* values */
	if (strncmp (key, "button", 6) == 0) {
		gpm_debug ("state of a button has changed : %s, %s", udi, key);
		emit_button_pressed (button, udi, "");
	}
}

/**
 * hal_device_condition_cb:
 *
 * @udi: Univerisal Device Id
 * @name: Name of condition
 * @details: D-BUS message with parameters
 *
 * Invoked when a property of a device in the Global Device List is
 * changed, and we have we have subscribed to changes for that device.
 */
static void
hal_device_condition_cb (GpmHal        *hal,
			 const gchar   *udi,
			 const gchar   *condition,
			 const gchar   *details,
			 GpmButton *button)
{
	gpm_debug ("udi=%s, condition=%s, details=%s", udi, condition, details);

	if (strcmp (condition, "ButtonPressed") == 0) {
		emit_button_pressed (button, udi, details);
	}
}

/**
 * watch_add_button:
 *
 * @udi: The HAL UDI
 */
static void
watch_add_button (GpmButton *button,
		  const gchar   *udi)
{
	gpm_hal_device_watch_condition (button->priv->hal, udi, FALSE);
	gpm_hal_device_watch_propery_modified (button->priv->hal, udi, FALSE);
}

/**
 * gpm_button_init:
 * @button: This class instance
 **/
static void
gpm_button_init (GpmButton *button)
{
	gboolean have_xevents = FALSE;
	int    i;
	char **device_names = NULL;
	gboolean ret;
	GError *error;

	button->priv = GPM_BUTTON_GET_PRIVATE (button);

	button->priv->screen = gdk_screen_get_default ();
	button->priv->window = gdk_screen_get_root_window (button->priv->screen);

	button->priv->hash_to_hal = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	button->priv->lid_is_closed = FALSE;

#ifdef HAVE_XEVENTS
	have_xevents = TRUE;
#endif

	if (have_xevents == TRUE) {
		/* register the brightness keys */
		gpm_button_button_key (button, "XF86XK_Execute", GPM_BUTTON_POWER);
		gpm_button_button_key (button, "XF86XK_PowerOff", GPM_BUTTON_POWER);
		gpm_button_button_key (button, "XF86XK_Suspend", GPM_BUTTON_SUSPEND);
		gpm_button_button_key (button, "XF86XK_Sleep", GPM_BUTTON_SUSPEND); /* should be configurable */
		gpm_button_button_key (button, "XF86XK_Hibernate", GPM_BUTTON_HIBERNATE);
		gpm_button_button_key (button, "XF86BrightnessUp", GPM_BUTTON_BRIGHT_UP);
		gpm_button_button_key (button, "XF86BrightnessDown", GPM_BUTTON_BRIGHT_DOWN);
		gpm_button_button_key (button, "XF86XK_ScreenSaver", GPM_BUTTON_LOCK);
		gpm_button_button_key (button, "XF86XK_Battery", GPM_BUTTON_BATTERY);
		gpm_button_button_key (button, "XF86KeyboardLightUp", GPM_BUTTON_KBD_BRIGHT_UP);
		gpm_button_button_key (button, "XF86KeyboardLightDown", GPM_BUTTON_KBD_BRIGHT_DOWN);
		gpm_button_button_key (button, "XF86KeyboardLightOnOff", GPM_BUTTON_KBD_BRIGHT_TOGGLE);

		/* use global filter */
		gdk_window_add_filter (button->priv->window,
				       gpm_button_filter_x_events, (gpointer) button);
	}

	/* remove when button support is out of HAL */
	button->priv->hal = gpm_hal_new ();

	/* devices of type button */
	error = NULL;
	ret = gpm_hal_device_find_capability (button->priv->hal, "button", &device_names, &error);
	if (ret == FALSE) {
		gpm_warning ("Couldn't obtain list of buttons: %s", error->message);
		g_error_free (error);
		return;
	}
	if (device_names[0] != NULL) {
		/* we have found buttons */
		for (i = 0; device_names[i]; i++) {
			watch_add_button (button, device_names[i]);
		}
		g_signal_connect (button->priv->hal, "property-modified",
				  G_CALLBACK (hal_device_property_modified_cb), button);
		g_signal_connect (button->priv->hal, "device-condition",
				  G_CALLBACK (hal_device_condition_cb), button);
	} else {
		gpm_debug ("Couldn't obtain list of buttons");
	}

	gpm_hal_free_capability (button->priv->hal, device_names);
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

	if (button->priv->hal != NULL) {
		g_object_unref (button->priv->hal);
	}

	g_hash_table_unref (button->priv->hash_to_hal);
}

/**
 * gpm_button_new:
 * Return value: new class instance.
 **/
GpmButton *
gpm_button_new (void)
{
	static GpmButton *button = NULL;
	if (button != NULL) {
		g_object_ref (button);
	} else {
		button = g_object_new (GPM_TYPE_BUTTON, NULL);
	}
	return GPM_BUTTON (button);
}
