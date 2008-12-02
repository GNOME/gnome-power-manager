/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#include "gpm-brightness.h"
#include "gpm-brightness-hal.h"
#include "gpm-brightness-xrandr.h"
#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-marshal.h"

#define GPM_BRIGHTNESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS, GpmBrightnessPrivate))
#define GPM_SOLE_SETTER_USE_CACHE	TRUE	/* this may be insanity */

struct GpmBrightnessPrivate
{
	gboolean		 use_xrandr;
	gboolean		 use_hal;
	gboolean		 has_changed_events;
	gboolean		 cache_trusted;
	guint			 cache_percentage;
	GpmBrightnessHal	*hal;
	GpmBrightnessXRandR	*xrandr;
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (GpmBrightness, gpm_brightness, G_TYPE_OBJECT)
static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_brightness_object = NULL;

/**
 * gpm_brightness_get_step:
 * @levels: The number of levels supported
 * Return value: the amount of hardware steps to do on each increment or decrement
 **/
guint
gpm_brightness_get_step (guint levels)
{
	if (levels > 20) {
		/* macbook pro has a bazzillion brightness levels, do in 5% steps */
		return levels / 20;
	}
	return 1;
}

/**
 * gpm_brightness_trust_cache:
 * @brightness: This brightness class instance
 * Return value: if we can trust the cache
 **/
static gboolean
gpm_brightness_trust_cache (GpmBrightness *brightness)
{
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	/* only return the cached value if the cache is trusted and we have change events */
	if (brightness->priv->cache_trusted && brightness->priv->has_changed_events) {
		egg_debug ("using cache for value %u (okay)", brightness->priv->cache_percentage);
		return TRUE;
	}

	/* can we trust that if we set a value 5 minutes ago, will it still be valid now?
	 * if we have multiple things setting policy on the workstation, e.g. fast user switching
	 * or kpowersave, then this will be invalid -- this logic may be insane */
	if (GPM_SOLE_SETTER_USE_CACHE && brightness->priv->cache_trusted) {
		egg_warning ("using cache for value %u (probably okay)", brightness->priv->cache_percentage);
		return TRUE;
	}
	return FALSE;
}

/**
 * gpm_brightness_set:
 * @brightness: This brightness class instance
 * @percentage: The percentage brightness
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 **/
gboolean
gpm_brightness_set (GpmBrightness *brightness, guint percentage, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gboolean trust_cache;
	gboolean hw_changed_local = FALSE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* can we check the new value with the cache? */
	trust_cache = gpm_brightness_trust_cache (brightness);
	if (trust_cache && percentage == brightness->priv->cache_percentage) {
		egg_debug ("not setting the same value %i", percentage);
		return TRUE;
	}

	/* set the hardware */
	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_set (brightness->priv->xrandr, percentage, &hw_changed_local);
		if (ret)
			goto out;
		brightness->priv->use_xrandr = FALSE;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_set (brightness->priv->hal, percentage, &hw_changed_local);
		goto out;
	}
	egg_debug ("no hardware support");
	return FALSE;
out:
	/* we did something to the hardware, so untrusted */
	if (ret) {
		brightness->priv->cache_trusted = FALSE;
	}
	/* is the caller interested? */
	if (ret && hw_changed != NULL) {
		*hw_changed = hw_changed_local;
	}
	return ret;
}

/**
 * gpm_brightness_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_brightness_get (GpmBrightness *brightness, guint *percentage)
{
	gboolean ret = FALSE;
	gboolean trust_cache;
	guint percentage_local;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	g_return_val_if_fail (percentage != NULL, FALSE);

	/* can we use the cache? */
	trust_cache = gpm_brightness_trust_cache (brightness);
	if (trust_cache) {
		*percentage = brightness->priv->cache_percentage;
		return TRUE;
	}

	/* get the brightness from hardware -- slow */
	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_get (brightness->priv->xrandr, &percentage_local);
		if (ret)
			goto out;
		brightness->priv->use_xrandr = FALSE;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_get (brightness->priv->hal, &percentage_local);
		goto out;
	}
	egg_debug ("no hardware support");
	return FALSE;
out:
	/* valid? */
	if (percentage_local > 100) {
		egg_warning ("percentage value of %i will be ignored", percentage_local);
		ret = FALSE;
	}
	/* a new value is always trusted if the method and checks succeed */
	if (ret) {
		brightness->priv->cache_percentage = percentage_local;
		brightness->priv->cache_trusted = TRUE;
		*percentage = percentage_local;
	} else {
		brightness->priv->cache_trusted = FALSE;
	}
	return ret;
}

/**
 * gpm_brightness_up:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
gboolean
gpm_brightness_up (GpmBrightness *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gboolean hw_changed_local = FALSE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_up (brightness->priv->xrandr, &hw_changed_local);
		if (ret)
			goto out;
		brightness->priv->use_xrandr = FALSE;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_up (brightness->priv->hal, &hw_changed_local);
		goto out;
	}
	egg_debug ("no hardware support");
	return FALSE;
out:
	/* we did something to the hardware, so untrusted */
	if (ret) {
		brightness->priv->cache_trusted = FALSE;
	}
	/* is the caller interested? */
	if (ret && hw_changed != NULL) {
		*hw_changed = hw_changed_local;
	}
	return ret;
}

/**
 * gpm_brightness_down:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
gboolean
gpm_brightness_down (GpmBrightness *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gboolean hw_changed_local = FALSE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_down (brightness->priv->xrandr, &hw_changed_local);
		if (ret)
			goto out;
		brightness->priv->use_xrandr = FALSE;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_down (brightness->priv->hal, &hw_changed_local);
		goto out;
	}
	egg_debug ("no hardware support");
	return FALSE;
out:
	/* we did something to the hardware, so untrusted */
	if (ret) {
		brightness->priv->cache_trusted = FALSE;
	}
	/* is the caller interested? */
	if (ret && hw_changed != NULL) {
		*hw_changed = hw_changed_local;
	}
	return ret;
}

/**
 * gpm_brightness_has_hw:
 **/
gboolean
gpm_brightness_has_hw (GpmBrightness *brightness)
{
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	return (brightness->priv->use_xrandr || brightness->priv->use_hal);
}

/**
 * gpm_brightness_finalize:
 **/
static void
gpm_brightness_finalize (GObject *object)
{
	GpmBrightness *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS (object));
	brightness = GPM_BRIGHTNESS (object);
	g_object_unref (brightness->priv->hal);
	g_object_unref (brightness->priv->xrandr);
	G_OBJECT_CLASS (gpm_brightness_parent_class)->finalize (object);
}

/**
 * gpm_brightness_class_init:
 **/
static void
gpm_brightness_class_init (GpmBrightnessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_brightness_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBrightnessClass, brightness_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmBrightnessPrivate));
}

/**
 * gpm_brightness_changed:
 * This callback is called when the brightness value changes.
 **/
static void
gpm_brightness_changed (GpmBrightness *brightness, guint percentage)
{
	g_return_if_fail (GPM_IS_BRIGHTNESS (brightness));
	brightness->priv->cache_trusted = TRUE;

	/* valid? */
	if (percentage > 100) {
		egg_warning ("percentage value of %i will be ignored", percentage);
		/* no longer trust the cache */
		brightness->priv->has_changed_events = FALSE;
		brightness->priv->cache_trusted = FALSE;
		return;
	}

	brightness->priv->has_changed_events = TRUE;
	brightness->priv->cache_trusted = TRUE;
	brightness->priv->cache_percentage = percentage;
	/* ONLY EMIT THIS SIGNAL WHEN SOMETHING _ELSE_ HAS CHANGED THE BACKLIGHT */
	egg_debug ("emitting brightness-changed (%i)", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);
}

/**
 * gpm_brightness_xrandr_changed_cb:
 * This callback is called when the brightness value changes.
 **/
static void
gpm_brightness_xrandr_changed_cb (GpmBrightnessXRandR *xrandr, guint percentage, GpmBrightness *brightness)
{
	g_return_if_fail (GPM_IS_BRIGHTNESS (brightness));
	if (brightness->priv->use_xrandr) {
		gpm_brightness_changed (brightness, percentage);
	}
}

/**
 * gpm_brightness_hal_changed_cb:
 * This callback is called when the brightness value changes.
 **/
static void
gpm_brightness_hal_changed_cb (GpmBrightnessHal *hal, guint percentage, GpmBrightness *brightness)
{
	g_return_if_fail (GPM_IS_BRIGHTNESS (brightness));
	if (brightness->priv->use_hal) {
		gpm_brightness_changed (brightness, percentage);
	}
}

/**
 * gpm_brightness_init:
 * @brightness: This brightness class instance
 **/
static void
gpm_brightness_init (GpmBrightness *brightness)
{
	brightness->priv = GPM_BRIGHTNESS_GET_PRIVATE (brightness);

	brightness->priv->use_xrandr = FALSE;
	brightness->priv->use_hal = FALSE;
	brightness->priv->cache_trusted = FALSE;
	brightness->priv->has_changed_events = FALSE;
	brightness->priv->cache_percentage = 0;

	brightness->priv->xrandr = gpm_brightness_xrandr_new ();
	if (gpm_brightness_xrandr_has_hw (brightness->priv->xrandr)) {
		brightness->priv->use_xrandr = TRUE;
	}
	brightness->priv->hal = gpm_brightness_hal_new ();
	if (gpm_brightness_hal_has_hw (brightness->priv->hal)) {
		brightness->priv->use_hal = TRUE;
	}
	g_signal_connect (brightness->priv->hal, "brightness-changed",
			  G_CALLBACK (gpm_brightness_hal_changed_cb), brightness);
	g_signal_connect (brightness->priv->xrandr, "brightness-changed",
			  G_CALLBACK (gpm_brightness_xrandr_changed_cb), brightness);
}

/**
 * gpm_brightness_new:
 * Return value: A new brightness class instance.
 * Can return NULL if no suitable hardware is found.
 **/
GpmBrightness *
gpm_brightness_new (void)
{
	if (gpm_brightness_object != NULL) {
		g_object_ref (gpm_brightness_object);
	} else {
		gpm_brightness_object = g_object_new (GPM_TYPE_BRIGHTNESS, NULL);
		g_object_add_weak_pointer (gpm_brightness_object, &gpm_brightness_object);
	}
	return GPM_BRIGHTNESS (gpm_brightness_object);
}

