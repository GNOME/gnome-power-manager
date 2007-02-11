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

#include "gpm-common.h"
#include "gpm-conf.h"
#include "gpm-debug.h"
#include "gpm-power.h"
#include "gpm-warning.h"

#define GPM_WARNING_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_WARNING, GpmWarningPrivate))

struct GpmWarningPrivate
{
	GpmConf			*conf;
	gboolean		 use_time;

	guint			 low_percentage;
	guint			 very_low_percentage;
	guint			 critical_percentage;
	guint			 action_percentage;

	guint			 low_time;
	guint			 very_low_time;
	guint			 critical_time;
	guint			 action_time;
};

G_DEFINE_TYPE (GpmWarning, gpm_warning, G_TYPE_OBJECT)

/**
 * gpm_warning_get_state:
 * @warning: This class instance
 * @battery_status: The battery status information
 * @policy: If we should use a per-time or per-percent policy
 *
 * This gets the possible warning state for the device according to the
 * policy, which could be per-percent, or per-time.
 *
 * Return value: A GpmWarning state, e.g. GPM_WARNING_VERY_LOW
 **/
GpmWarningState
gpm_warning_get_state (GpmWarning       *warning,
		      GpmPowerStatus   *status,
		      GpmWarningPolicy  policy)
{
	GpmWarningState type;

	g_return_val_if_fail (GPM_IS_WARNING (warning), GPM_WARNING_NONE);

	/* default to no warning */
	type = GPM_WARNING_NONE;

	/* get from gconf */
	if (policy == GPM_WARNING_AUTO) {
		policy = warning->priv->use_time;
	}

	/* this is a CSR mouse */
	if (status->design_charge == 7) {
		if (status->current_charge == 2) {
			type = GPM_WARNING_LOW;
		} else if (status->current_charge == 1) {
			type = GPM_WARNING_VERY_LOW;
		}
	} else if (policy == GPM_WARNING_TIME) {
		if (status->remaining_time <= 0) {
			type = GPM_WARNING_NONE;
		} else if (status->remaining_time <= warning->priv->action_time) {
			type = GPM_WARNING_ACTION;
		} else if (status->remaining_time <= warning->priv->critical_time) {
			type = GPM_WARNING_CRITICAL;
		} else if (status->remaining_time <= warning->priv->very_low_time) {
			type = GPM_WARNING_VERY_LOW;
		} else if (status->remaining_time <= warning->priv->low_time) {
			type = GPM_WARNING_LOW;
		}
	} else {
		if (status->percentage_charge <= 0) {
			type = GPM_WARNING_NONE;
			gpm_warning ("Your hardware is reporting a percentage "
				     "charge of %i, which is impossible. "
				     "WARNING_ACTION will *not* be reported.",
				     status->percentage_charge);
		} else if (status->percentage_charge <= warning->priv->action_percentage) {
			type = GPM_WARNING_ACTION;
		} else if (status->percentage_charge <= warning->priv->critical_percentage) {
			type = GPM_WARNING_CRITICAL;
		} else if (status->percentage_charge <= warning->priv->very_low_percentage) {
			type = GPM_WARNING_VERY_LOW;
		} else if (status->percentage_charge <= warning->priv->low_percentage) {
			type = GPM_WARNING_LOW;
		}
	}

	/* If we have no important warnings, we should test for discharging */
	if (type == GPM_WARNING_NONE) {
		if (status->is_discharging) {
			type = GPM_WARNING_DISCHARGING;
		}
	}
	return type;
}

/**
 * gpm_warning_get_title:
 * @warning_type: The warning type, e.g. GPM_WARNING_VERY_LOW
 * Return value: the title text according to the warning type.
 **/
const gchar *
gpm_warning_get_title (GpmWarningState warning_type)
{
	char *title = NULL;

	if (warning_type == GPM_WARNING_ACTION ||
	    warning_type == GPM_WARNING_CRITICAL) {
		title = _("Power Critically Low");
	} else if (warning_type == GPM_WARNING_VERY_LOW) {
		title = _("Power Very Low");
	} else if (warning_type == GPM_WARNING_LOW) {
		title = _("Power Low");
	} else if (warning_type == GPM_WARNING_DISCHARGING) {
		title = _("Power Information");
	}

	return title;
}

/**
 * gpm_warning_constructor:
 **/
static GObject *
gpm_warning_constructor (GType		  type,
			      guint		  n_construct_properties,
			      GObjectConstructParam *construct_properties)
{
	GpmWarning      *warning;
	GpmWarningClass *klass;
	klass = GPM_WARNING_CLASS (g_type_class_peek (GPM_TYPE_WARNING));
	warning = GPM_WARNING (G_OBJECT_CLASS (gpm_warning_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (warning);
}

/**
 * gpm_warning_finalize:
 **/
static void
gpm_warning_finalize (GObject *object)
{
	GpmWarning *warning;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_WARNING (object));
	warning = GPM_WARNING (object);

	g_object_unref (warning->priv->conf);

	G_OBJECT_CLASS (gpm_warning_parent_class)->finalize (object);
}

/**
 * gconf_key_changed_cb:
 **/
static void
gconf_key_changed_cb (GpmConf     *conf,
		      const gchar *key,
		      GpmWarning  *warning)
{
	g_return_if_fail (GPM_IS_WARNING (warning));

	if (strcmp (key, GPM_CONF_USE_TIME_POLICY) == 0) {
		gpm_conf_get_bool (warning->priv->conf,
				   GPM_CONF_USE_TIME_POLICY,
				   &warning->priv->use_time);
	}
}

/**
 * gpm_warning_class_init:
 **/
static void
gpm_warning_class_init (GpmWarningClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_warning_finalize;
	object_class->constructor  = gpm_warning_constructor;

	g_type_class_add_private (klass, sizeof (GpmWarningPrivate));
}

/**
 * gpm_warning_init:
 * @warning: This warning class instance
 *
 * initialises the warning class. NOTE: We expect warning objects
 * to *NOT* be removed or added during the session.
 * We only control the first warning object if there are more than one.
 **/
static void
gpm_warning_init (GpmWarning *warning)
{
	warning->priv = GPM_WARNING_GET_PRIVATE (warning);

	warning->priv->conf = gpm_conf_new ();
	g_signal_connect (warning->priv->conf, "value-changed",
			  G_CALLBACK (gconf_key_changed_cb), warning);

	/* get percentage policy */
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_LOW_PERCENTAGE, &warning->priv->low_percentage);
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_VERY_LOW_PERCENTAGE, &warning->priv->very_low_percentage);
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_CRITICAL_PERCENTAGE, &warning->priv->critical_percentage);
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_ACTION_PERCENTAGE, &warning->priv->action_percentage);

	/* get time policy */
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_LOW_TIME, &warning->priv->low_time);
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_VERY_LOW_TIME, &warning->priv->very_low_time);
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_CRITICAL_TIME, &warning->priv->critical_time);
	gpm_conf_get_uint (warning->priv->conf, GPM_CONF_ACTION_TIME, &warning->priv->action_time);

	/* We can disable this if the ACPI BIOS is broken, and the
	   time_remaining is therefore inaccurate or just plain wrong. */
	gpm_conf_get_bool (warning->priv->conf, GPM_CONF_USE_TIME_POLICY, &warning->priv->use_time);
	if (warning->priv->use_time) {
		gpm_debug ("Using per-time notification policy");
	} else {
		gpm_debug ("Using percentage notification policy");
	}
}

/**
 * gpm_warning_new:
 * Return value: A new warning class instance.
 **/
GpmWarning *
gpm_warning_new (void)
{
	static GpmWarning *warning = NULL;

	if (warning != NULL) {
		g_object_ref (warning);
		return warning;
	}

	warning = g_object_new (GPM_TYPE_WARNING, NULL);
	return warning;
}
