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
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/XF86keysym.h>

#include <libhal-gmanager.h>
#include <libhal-gdevice.h>
#include <libhal-gdevicestore.h>

#include "gpm-common.h"
#include "gpm-button.h"
#include "egg-debug.h"
#include "gpm-marshal.h"

static void     gpm_button_class_init (GpmButtonClass *klass);
static void     gpm_button_init       (GpmButton      *button);
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
	HalGManager		*hal_manager; /* remove when input events is in the kernel */
	HalGDevicestore		*hal_devicestore;
};

enum {
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_button_object = NULL;

G_DEFINE_TYPE (GpmButton, gpm_button, G_TYPE_OBJECT)

#define GPM_BUTTON_DUPLICATE_TIMEOUT	0.25f

/**
 * gpm_button_emit_type:
 **/
gboolean
gpm_button_emit_type (GpmButton *button, const gchar *type)
{
	g_return_val_if_fail (button != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_BUTTON (button), FALSE);

	/* did we just have this button before the timeout? */
	if (g_strcmp0 (type, button->priv->last_button) == 0 &&
	    g_timer_elapsed (button->priv->timer, NULL) < GPM_BUTTON_DUPLICATE_TIMEOUT) {
		egg_debug ("ignoring duplicate button %s", type);
		return FALSE;
	}

	egg_debug ("emitting button-pressed : %s", type);
	g_signal_emit (button, signals [BUTTON_PRESSED], 0, type);

	/* save type and last size */
	g_free (button->priv->last_button);
	button->priv->last_button = g_strdup (type);
	g_timer_reset (button->priv->timer);

	return TRUE;
}

/**
 * gpm_button_filter_x_events:
 **/
static GdkFilterReturn
gpm_button_filter_x_events (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	GpmButton *button = (GpmButton *) data;
	XEvent *xev = (XEvent *) xevent;
	guint keycode;
	const gchar *key;
	gchar *keycode_str;

	if (xev->type != KeyPress)
		return GDK_FILTER_CONTINUE;

	keycode = xev->xkey.keycode;

	/* is the key string already in our DB? */
	keycode_str = g_strdup_printf ("0x%x", keycode);
	key = g_hash_table_lookup (button->priv->keysym_to_name_hash, (gpointer) keycode_str);
	g_free (keycode_str);

	/* found anything? */
	if (key == NULL) {
		egg_debug ("Key %i not found in hash", keycode);
		/* pass normal keypresses on, which might help with accessibility access */
		return GDK_FILTER_CONTINUE;
	}

	egg_debug ("Key %i mapped to key %s", keycode, key);
	gpm_button_emit_type (button, key);

	return GDK_FILTER_REMOVE;
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
gpm_button_grab_keystring (GpmButton *button, guint64 keycode)
{
	guint modmask = 0;
	Display *display;
	gint ret;

	/* get the current X Display */
	display = GDK_DISPLAY ();

	/* don't abort on error */
	gdk_error_trap_push ();

	/* grab the key if possible */
	ret = XGrabKey (display, keycode, modmask,
			GDK_WINDOW_XID (button->priv->window), True,
			GrabModeAsync, GrabModeAsync);
	if (ret == BadAccess) {
		egg_warning ("Failed to grab modmask=%u, keycode=%li",
			     modmask, (long int) keycode);
		return FALSE;
	}

	/* grab the lock key if possible */
	ret = XGrabKey (display, keycode, LockMask | modmask,
			GDK_WINDOW_XID (button->priv->window), True,
			GrabModeAsync, GrabModeAsync);
	if (ret == BadAccess) {
		egg_warning ("Failed to grab modmask=%u, keycode=%li",
			     LockMask | modmask, (long int) keycode);
		return FALSE;
	}

	/* we are not processing the error */
	gdk_flush ();
	gdk_error_trap_pop ();

	egg_debug ("Grabbed modmask=%x, keycode=%li", modmask, (long int) keycode);
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
gpm_button_xevent_key (GpmButton *button, guint keysym, const gchar *hal_key)
{
	gchar *key = NULL;
	gboolean ret;
	gchar *keycode_str;
	guint keycode;

	/* convert from keysym to keycode */
	keycode = XKeysymToKeycode (GDK_DISPLAY (), keysym);
	if (keycode == 0) {
		egg_warning ("could not map keysym %x to keycode", keysym);
		return FALSE;
	}

	/* is the key string already in our DB? */
	keycode_str = g_strdup_printf ("0x%x", keycode);
	key = g_hash_table_lookup (button->priv->keysym_to_name_hash, (gpointer) keycode_str);
	if (key != NULL) {
		egg_warning ("found in hash %i", keycode);
		g_free (keycode_str);
		return FALSE;
	}

	/* try to register X event */
	ret = gpm_button_grab_keystring (button, keycode);
	if (!ret) {
		egg_warning ("Failed to grab %i", keycode);
		g_free (keycode_str);
		return FALSE;
	}

	/* add to hash table */
	g_hash_table_insert (button->priv->keysym_to_name_hash, (gpointer) keycode_str, (gpointer) g_strdup (hal_key));

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
			      NULL, NULL,
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
emit_button_pressed (GpmButton *button, HalGDevice *device, const gchar *details)
{
	gchar *type = NULL;
	gboolean state;
	const gchar *atype;

	g_return_if_fail (device != NULL);
	g_return_if_fail (details != NULL);

	if (strcmp (details, "") == 0) {
		/* no details about the event, so we get more info for type 1 buttons */
		hal_gdevice_get_string (device, "button.type", &type, NULL);
		/* hal may no longer be there */
		if (type == NULL) {
			egg_warning ("cannot get button type for %s", hal_gdevice_get_udi (device));
			goto out;
		}
	} else {
		type = g_strdup (details);
	}
	atype = type;

	/* Buttons without state should default to true. */
	state = TRUE;
	/* we need to get the button state for lid buttons */
	if (strcmp (type, "lid") == 0)
		hal_gdevice_get_bool (device, "button.state.value", &state, NULL);

	/* abstact away that HAL has an extra parameter */
	if (strcmp (type, GPM_BUTTON_LID_DEP) == 0 && state == FALSE)
		atype = GPM_BUTTON_LID_OPEN;
	else if (strcmp (type, GPM_BUTTON_LID_DEP) == 0 && state)
		atype = GPM_BUTTON_LID_CLOSED;

	/* filter out duplicate lid events */
	if (strcmp (atype, GPM_BUTTON_LID_CLOSED) == 0) {
		if (button->priv->lid_is_closed) {
			egg_debug ("ignoring duplicate lid event");
			return;
		}
		button->priv->lid_is_closed = TRUE;
	}
	if (strcmp (atype, GPM_BUTTON_LID_OPEN) == 0) {
		if (button->priv->lid_is_closed == FALSE) {
			egg_debug ("ignoring duplicate lid event");
			return;
		}
		button->priv->lid_is_closed = FALSE;
	}

	/* we emit all buttons, even the ones we don't know */
	gpm_button_emit_type (button, atype);
out:
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
hal_device_property_modified_cb (HalGDevice    *device,
				 const gchar   *key,
				 gboolean       is_added,
				 gboolean       is_removed,
				 gboolean       finally,
				 GpmButton     *button)
{
	egg_debug ("key=%s, added=%i, removed=%i, finally=%i",
		   key, is_added, is_removed, finally);

	/* do not process keys that have been removed */
	if (is_removed)
		return;

	/* only match button* values */
	if (strncmp (key, "button", 6) == 0) {
		egg_debug ("state of a button has changed : %s, %s", hal_gdevice_get_udi (device), key);
		emit_button_pressed (button, device, "");
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
hal_device_condition_cb (HalGDevice    *device,
			 const gchar   *condition,
			 const gchar   *details,
			 GpmButton     *button)
{
	egg_debug ("condition=%s, details=%s", condition, details);

	if (strcmp (condition, "ButtonPressed") == 0)
		emit_button_pressed (button, device, details);
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
	HalGDevice *device;

	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, udi);
	hal_gdevice_watch_condition (device);
	hal_gdevice_watch_property_modified (device);

	g_signal_connect (device, "property-modified",
			  G_CALLBACK (hal_device_property_modified_cb), button);
	g_signal_connect (device, "device-condition",
			  G_CALLBACK (hal_device_condition_cb), button);

	/* when added to the devicestore, devices are automatically unref'ed */
	hal_gdevicestore_insert (button->priv->hal_devicestore, device);
}

/**
 * coldplug_buttons:
 **/
static void
coldplug_buttons (GpmButton *button)
{
	int i;
	char **device_names = NULL;
	gboolean ret;
	GError *error;

	/* devices of type button */
	error = NULL;
	ret = hal_gmanager_find_capability (button->priv->hal_manager, "button", &device_names, &error);
	if (!ret) {
		egg_warning ("Couldn't obtain list of buttons: %s", error->message);
		g_error_free (error);
		return;
	}
	if (device_names[0] != NULL) {
		/* we have found buttons */
		for (i = 0; device_names[i]; i++) {
			watch_add_button (button, device_names[i]);
			egg_debug ("Watching %s", device_names[i]);
		}
	} else {
		egg_debug ("Couldn't obtain list of buttons");
	}

	hal_gmanager_free_capability (device_names);
}

/**
 * hal_daemon_start_cb:
 **/
static void
hal_daemon_start_cb (HalGManager *hal_manager,
		     GpmButton   *button)
{
	/* get new devices, hal has come back up */
	if (button->priv->hal_devicestore == NULL) {
		button->priv->hal_devicestore = hal_gdevicestore_new ();
		coldplug_buttons (button);
	}
}

/**
 * hal_daemon_stop_cb:
 **/
static void
hal_daemon_stop_cb (HalGManager *hal_manager,
		    GpmButton   *button)
{
	/* clear devices, HAL is going down */
	if (button->priv->hal_devicestore != NULL) {
		g_object_unref (button->priv->hal_devicestore);
		button->priv->hal_devicestore = NULL;
	}
}

/**
 * gpm_button_init:
 * @button: This class instance
 **/
static void
gpm_button_init (GpmButton *button)
{
	gboolean have_xevents = FALSE;

	button->priv = GPM_BUTTON_GET_PRIVATE (button);

	button->priv->screen = gdk_screen_get_default ();
	button->priv->window = gdk_screen_get_root_window (button->priv->screen);

	button->priv->keysym_to_name_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	button->priv->last_button = NULL;
	button->priv->timer = g_timer_new ();

	button->priv->lid_is_closed = FALSE;

#ifdef HAVE_XEVENTS
	have_xevents = TRUE;
#endif

	if (have_xevents) {
		/* register the brightness keys */
		gpm_button_xevent_key (button, XF86XK_PowerOff, GPM_BUTTON_POWER);
//		gpm_button_xevent_key (button, XF86XK_Suspend, GPM_BUTTON_SUSPEND);
		gpm_button_xevent_key (button, XF86XK_Sleep, GPM_BUTTON_SUSPEND); /* should be configurable */
//		gpm_button_xevent_key (button, XF86XK_Hibernate, GPM_BUTTON_HIBERNATE);
		gpm_button_xevent_key (button, XF86XK_MonBrightnessUp, GPM_BUTTON_BRIGHT_UP);
		gpm_button_xevent_key (button, XF86XK_MonBrightnessDown, GPM_BUTTON_BRIGHT_DOWN);
		gpm_button_xevent_key (button, XF86XK_ScreenSaver, GPM_BUTTON_LOCK);
//		gpm_button_xevent_key (button, XF86XK_Battery, GPM_BUTTON_BATTERY);
		gpm_button_xevent_key (button, XF86XK_KbdBrightnessUp, GPM_BUTTON_KBD_BRIGHT_UP);
		gpm_button_xevent_key (button, XF86XK_KbdBrightnessDown, GPM_BUTTON_KBD_BRIGHT_DOWN);
		gpm_button_xevent_key (button, XF86XK_KbdLightOnOff, GPM_BUTTON_KBD_BRIGHT_TOGGLE);

		/* use global filter */
		gdk_window_add_filter (button->priv->window,
				       gpm_button_filter_x_events, (gpointer) button);
	}

	/* remove when button support is out of HAL */
	button->priv->hal_manager = hal_gmanager_new ();
	g_signal_connect (button->priv->hal_manager, "daemon-start",
			  G_CALLBACK (hal_daemon_start_cb), button);
	g_signal_connect (button->priv->hal_manager, "daemon-stop",
			  G_CALLBACK (hal_daemon_stop_cb), button);

	button->priv->hal_devicestore = hal_gdevicestore_new ();

	coldplug_buttons (button);
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

	g_object_unref (button->priv->hal_manager);
	g_object_unref (button->priv->hal_devicestore);
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
