/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <devkit-power-gobject/devicekit-power.h>

#include "egg-debug.h"
#include "egg-discrete.h"

#include "gpm-brightness.h"
#include "gpm-brightness-dkp.h"
#include "gpm-common.h"
#include "gpm-marshal.h"

#define GPM_BRIGHTNESS_DKP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_BRIGHTNESS_DKP, GpmBrightnessDkpPrivate))

struct GpmBrightnessDkpPrivate
{
#if DKP_CHECK_VERSION(0x009)
	DkpBacklight		*backlight;
#endif
	guint			 last_set_hw;
	guint			 maximum;
	gboolean		 hw_changed;
 	gboolean		 action_in_hardware;
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (GpmBrightnessDkp, gpm_brightness_dkp, G_TYPE_OBJECT)
static guint signals [LAST_SIGNAL] = { 0 };

/**
 * gpm_brightness_dkp_set_hw:
 * @brightness: This brightness class instance
 * @value_hw: The hardware level in raw units
 *
 * Sets the hardware value to a new number.
 *
 * Return value: Success.
 **/
static gboolean
gpm_brightness_dkp_set_hw (GpmBrightnessDkp *brightness, guint value_hw)
{
	gboolean ret = FALSE;
#if DKP_CHECK_VERSION(0x009)
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);

	egg_debug ("Setting %i of %i", value_hw, brightness->priv->maximum);
	ret = dkp_backlight_set_brightness (brightness->priv->backlight, value_hw, &error);
	if (!ret) {
		egg_debug ("failed to set brightness: %s", error->message);
		g_error_free (error);
	}

	/* we changed the hardware */
	if (ret)
		brightness->priv->hw_changed = TRUE;

	brightness->priv->last_set_hw = value_hw;
#endif
	return ret;
}

/**
 * gpm_brightness_dkp_dim_hw_step:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 *
 * Just do the step up and down, after knowing the step interval
 **/
static gboolean
gpm_brightness_dkp_dim_hw_step (GpmBrightnessDkp *brightness, guint new_level_hw, guint step_interval)
{
	guint last_set_hw;
	gint a;
	gboolean ret = FALSE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);

	last_set_hw = brightness->priv->last_set_hw;
	egg_debug ("new_level_hw=%i, last_set_hw=%i", new_level_hw, last_set_hw);

	/* we do the step interval as we can have insane levels of brightness */
	if (new_level_hw == last_set_hw)
		return FALSE;

	if (new_level_hw > last_set_hw) {
		/* going up */
		for (a=last_set_hw; a <= (gint) new_level_hw; a+=step_interval) {
			ret = gpm_brightness_dkp_set_hw (brightness, a);
			/* we failed the last brightness set command, don't keep trying */
			if (!ret) {
				break;
			}
			g_usleep (1000 * GPM_BRIGHTNESS_DIM_INTERVAL);
		}
	} else {
		/* going down */
		for (a=last_set_hw; (gint) (a + 1) > (gint) new_level_hw; a-=step_interval) {
			ret = gpm_brightness_dkp_set_hw (brightness, a);
			/* we failed the last brightness set command, don't keep trying */
			if (!ret) {
				break;
			}
			g_usleep (1000 * GPM_BRIGHTNESS_DIM_INTERVAL);
		}
	}
	return ret;
}

/**
 * gpm_brightness_dkp_dim_hw:
 * @brightness: This brightness class instance
 * @new_level_hw: The new hardware level
 **/
static gboolean
gpm_brightness_dkp_dim_hw (GpmBrightnessDkp *brightness, guint new_level_hw)
{
	guint step;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);

	egg_debug ("new_level_hw=%i", new_level_hw);

	/* macbook pro has a bazzillion brightness levels, be a bit clever */
	step = gpm_brightness_get_step (brightness->priv->maximum);
	return gpm_brightness_dkp_dim_hw_step (brightness, new_level_hw, step);
}

/**
 * gpm_brightness_dkp_set:
 * @brightness: This brightness class instance
 * @percentage: The percentage brightness
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 **/
gboolean
gpm_brightness_dkp_set (GpmBrightnessDkp *brightness, guint percentage, gboolean *hw_changed)
{
	guint level_hw;
	gboolean ret;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);
	g_return_val_if_fail (hw_changed != NULL, FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

	level_hw = egg_discrete_from_percent (percentage, brightness->priv->maximum + 1);

	/* update */
	ret = gpm_brightness_dkp_dim_hw (brightness, level_hw);

	/* did the hardware have to be modified? */
	*hw_changed = brightness->priv->hw_changed;
	return ret;
}

/**
 * gpm_brightness_dkp_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no DKP inquiry is done.
 **/
gboolean
gpm_brightness_dkp_get (GpmBrightnessDkp *brightness, guint *percentage)
{
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);

	*percentage = egg_discrete_to_percent (brightness->priv->last_set_hw,
					       brightness->priv->maximum + 1);
	return TRUE;
}

/**
 * gpm_brightness_dkp_up:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
gboolean
gpm_brightness_dkp_up (GpmBrightnessDkp *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gint step;
	guint current_hw = 0;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);
	g_return_val_if_fail (hw_changed != NULL, FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

	/* check to see if the panel has changed */
#if DKP_CHECK_VERSION(0x009)
	g_object_get (brightness->priv->backlight,
		      "actual", &current_hw,
		      NULL);
#endif
	egg_debug ("brightness level: %i", current_hw);

	/* the panel has been updated in firmware */
	if (current_hw != brightness->priv->last_set_hw || 
            brightness->priv->action_in_hardware) {
		brightness->priv->last_set_hw = current_hw;
		ret = TRUE;
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_get_step (brightness->priv->maximum);
		/* don't overflow */
		if (brightness->priv->last_set_hw + step > brightness->priv->maximum) {
			step = brightness->priv->maximum - brightness->priv->last_set_hw;
		}
		ret = gpm_brightness_dkp_set_hw (brightness, brightness->priv->last_set_hw + step);
	}

	/* did the hardware have to be modified? */
	*hw_changed = brightness->priv->hw_changed;
	return ret;
}

/**
 * gpm_brightness_dkp_down:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
gboolean
gpm_brightness_dkp_down (GpmBrightnessDkp *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gint step;
	guint current_hw = 0;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);
	g_return_val_if_fail (hw_changed != NULL, FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

	/* check to see if the panel has changed */
#if DKP_CHECK_VERSION(0x009)
	g_object_get (brightness->priv->backlight,
		      "actual", &current_hw,
		      NULL);
#endif
	egg_debug ("brightness level: %i", current_hw);

	/* the panel has been updated in firmware */
 	if (current_hw != brightness->priv->last_set_hw ||
              brightness->priv->action_in_hardware) {
		brightness->priv->last_set_hw = current_hw;
		ret = TRUE;
	} else {
		/* macbook pro has a bazzillion brightness levels, be a bit clever */
		step = gpm_brightness_get_step (brightness->priv->maximum);
		/* don't underflow */
		if ((gint) brightness->priv->last_set_hw < step) {
			step = brightness->priv->last_set_hw;
		}
		ret = gpm_brightness_dkp_set_hw (brightness, brightness->priv->last_set_hw - step);
	}


	/* did the hardware have to be modified? */
	*hw_changed = brightness->priv->hw_changed;
	return ret;
}

/**
 * gpm_brightness_dkp_has_hw:
 **/
gboolean
gpm_brightness_dkp_has_hw (GpmBrightnessDkp *brightness)
{
	g_return_val_if_fail (GPM_IS_BRIGHTNESS_DKP (brightness), FALSE);
	return (brightness->priv->maximum != 0);
}

/**
 * gpm_brightness_dkp_finalize:
 **/
static void
gpm_brightness_dkp_finalize (GObject *object)
{
	GpmBrightnessDkp *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS_DKP (object));
	brightness = GPM_BRIGHTNESS_DKP (object);

#if DKP_CHECK_VERSION(0x009)
	g_object_unref (brightness->priv->backlight);
#endif

	G_OBJECT_CLASS (gpm_brightness_dkp_parent_class)->finalize (object);
}

/**
 * gpm_brightness_dkp_class_init:
 **/
static void
gpm_brightness_dkp_class_init (GpmBrightnessDkpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_brightness_dkp_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBrightnessDkpClass, brightness_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (GpmBrightnessDkpPrivate));
}

/**
 * gpm_brightness_dkp_init:
 * @brightness: This brightness class instance
 *
 * initialises the brightness class. NOTE: We expect laptop_panel objects
 * to *NOT* be removed or added during the session.
 * We only control the first laptop_panel object if there are more than one.
 **/
static void
gpm_brightness_dkp_init (GpmBrightnessDkp *brightness)
{
	brightness->priv = GPM_BRIGHTNESS_DKP_GET_PRIVATE (brightness);
#if DKP_CHECK_VERSION(0x009)
	brightness->priv->backlight = dkp_backlight_new ();
#endif
	brightness->priv->hw_changed = FALSE;
	brightness->priv->maximum = 0;

	/* this changes under our feet */
#if DKP_CHECK_VERSION(0x009)
	g_object_get (brightness->priv->backlight,
		      "actual", &brightness->priv->last_set_hw,
		      "maximum", &brightness->priv->maximum,
		      "action-in-hardware", &brightness->priv->action_in_hardware,
		      NULL);
#endif

	egg_debug ("Starting: (%i of %i)", brightness->priv->last_set_hw,
		   brightness->priv->maximum);
}

/**
 * gpm_brightness_dkp_new:
 * Return value: A new brightness class instance.
 * Can return NULL if no suitable hardware is found.
 **/
GpmBrightnessDkp *
gpm_brightness_dkp_new (void)
{
	GpmBrightnessDkp *brightness;
	brightness = g_object_new (GPM_TYPE_BRIGHTNESS_DKP, NULL);
	return GPM_BRIGHTNESS_DKP (brightness);
}

