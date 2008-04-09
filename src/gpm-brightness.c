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
#include "gpm-debug.h"
#include "gpm-marshal.h"

#define GPM_BRIGHTNESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS, GpmBrightnessPrivate))

struct GpmBrightnessPrivate
{
	gboolean		 use_xrandr;
	gboolean		 use_hal;
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
 * gpm_brightness_set:
 * @brightness: This brightness class instance
 * @percentage: The percentage brightness
 **/
gboolean
gpm_brightness_set (GpmBrightness *brightness, guint percentage)
{
	gboolean ret = FALSE;
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_set (brightness->priv->xrandr, percentage);
		return ret;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_set (brightness->priv->hal, percentage);
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
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_get (brightness->priv->xrandr, percentage);
		return ret;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_get (brightness->priv->hal, percentage);
	}	
	return ret;
}

/**
 * gpm_brightness_up:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
gboolean
gpm_brightness_up (GpmBrightness *brightness)
{
	gboolean ret = FALSE;
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_up (brightness->priv->xrandr);
		return ret;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_up (brightness->priv->hal);
	}	
	return ret;
}

/**
 * gpm_brightness_down:
 * @brightness: This brightness class instance
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
gboolean
gpm_brightness_down (GpmBrightness *brightness)
{
	gboolean ret = FALSE;
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	if (brightness->priv->use_xrandr) {
		ret = gpm_brightness_xrandr_down (brightness->priv->xrandr);
		return ret;
	}
	if (brightness->priv->use_hal) {
		ret = gpm_brightness_hal_down (brightness->priv->hal);
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
 * gpm_brightness_changed_cb:
 * This callback is called when the brightness value changes.
 **/
static void
gpm_brightness_changed_cb (gpointer caller, guint percentage, GpmBrightness *brightness)
{
	gpm_debug ("emitting brightness-changed (%i)", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);
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

	brightness->priv->xrandr = gpm_brightness_xrandr_new ();
	if (gpm_brightness_xrandr_has_hw (brightness->priv->xrandr)) {
		brightness->priv->use_xrandr = TRUE;
	}
	brightness->priv->hal = gpm_brightness_hal_new ();
	if (gpm_brightness_hal_has_hw (brightness->priv->hal)) {
		brightness->priv->use_hal = TRUE;
	}
	g_signal_connect (brightness->priv->hal, "brightness-changed",
			  G_CALLBACK (gpm_brightness_changed_cb), brightness);
	g_signal_connect (brightness->priv->xrandr, "brightness-changed",
			  G_CALLBACK (gpm_brightness_changed_cb), brightness);
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

