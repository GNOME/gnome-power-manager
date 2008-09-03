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
#include "egg-debug.h"
#include "gpm-cell-unit.h"
#include "gpm-warnings.h"

#define GPM_WARNINGS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_WARNINGS, GpmWarningsPrivate))

struct GpmWarningsPrivate
{
	GpmConf			*conf;
	gboolean		 use_time_primary;

	guint			 low_percentage;
	guint			 critical_percentage;
	guint			 action_percentage;

	guint			 low_time;
	guint			 critical_time;
	guint			 action_time;
};

G_DEFINE_TYPE (GpmWarnings, gpm_warnings, G_TYPE_OBJECT)

static gpointer gpm_warnings_object = NULL;

static GpmWarningsState
gpm_warnings_get_state_csr (GpmWarnings  *warnings,
		           GpmCellUnit *unit)
{
	if (unit->charge_current == 2) {
		return GPM_WARNINGS_LOW;
	} else if (unit->charge_current == 1) {
		return GPM_WARNINGS_CRITICAL;
	}
	return GPM_WARNINGS_NONE;
}

static GpmWarningsState
gpm_warnings_get_state_time (GpmWarnings  *warnings,
		            GpmCellUnit *unit)
{
	if (unit->time_discharge == 0) {
		/* this is probably an error condition */
		egg_warning ("time zero, something's gone wrong");
		return GPM_WARNINGS_NONE;
	}
	if (unit->time_discharge <= warnings->priv->action_time) {
		return GPM_WARNINGS_ACTION;
	} else if (unit->time_discharge <= warnings->priv->critical_time) {
		return GPM_WARNINGS_CRITICAL;
	} else if (unit->time_discharge <= warnings->priv->low_time) {
		return GPM_WARNINGS_LOW;
	}
	return GPM_WARNINGS_NONE;
}

static GpmWarningsState
gpm_warnings_get_state_percentage (GpmWarnings  *warnings,
		                  GpmCellUnit *unit)
{
	if (unit->percentage == 0) {
		/* this is probably an error condition */
		egg_warning ("percentage zero, something's gone wrong");
		return GPM_WARNINGS_NONE;
	}
	if (unit->percentage <= warnings->priv->action_percentage) {
		return GPM_WARNINGS_ACTION;
	} else if (unit->percentage <= warnings->priv->critical_percentage) {
		return GPM_WARNINGS_CRITICAL;
	} else if (unit->percentage <= warnings->priv->low_percentage) {
		return GPM_WARNINGS_LOW;
	}
	return GPM_WARNINGS_NONE;
}

/**
 * gpm_warnings_get_state:
 * @warnings: This class instance
 * @battery_status: The battery status information
 * @policy: If we should use a per-time or per-percent policy
 *
 * This gets the possible warnings state for the device according to the
 * policy, which could be per-percent, or per-time.
 *
 * Return value: A GpmWarnings state, e.g. GPM_WARNINGS_VERY_LOW
 **/
GpmWarningsState
gpm_warnings_get_state (GpmWarnings  *warnings,
		       GpmCellUnit *unit)
{
	GpmWarningsState type;

	g_return_val_if_fail (GPM_IS_WARNINGS (warnings), GPM_WARNINGS_NONE);

	/* default to no warnings */
	type = GPM_WARNINGS_NONE;

	if (unit->kind == GPM_CELL_UNIT_KIND_MOUSE ||
	    unit->kind == GPM_CELL_UNIT_KIND_KEYBOARD) {

		type = gpm_warnings_get_state_csr (warnings, unit);

	} else if (unit->kind == GPM_CELL_UNIT_KIND_UPS ||
		   unit->kind == GPM_CELL_UNIT_KIND_PDA) {

		type = gpm_warnings_get_state_percentage (warnings, unit);

	} else if (unit->kind == GPM_CELL_UNIT_KIND_PHONE) {

		type = gpm_warnings_get_state_percentage (warnings, unit);

	} else if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY &&
		   warnings->priv->use_time_primary) {

		type = gpm_warnings_get_state_time (warnings, unit);

	} else if (unit->kind == GPM_CELL_UNIT_KIND_PRIMARY &&
		   warnings->priv->use_time_primary == FALSE) {

		type = gpm_warnings_get_state_percentage (warnings, unit);
	}

	/* If we have no important warningss, we should test for discharging */
	if (type == GPM_WARNINGS_NONE) {
		if (unit->is_discharging) {
			type = GPM_WARNINGS_DISCHARGING;
		}
	}
	return type;
}

/**
 * gpm_warnings_finalize:
 **/
static void
gpm_warnings_finalize (GObject *object)
{
	GpmWarnings *warnings;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_WARNINGS (object));
	warnings = GPM_WARNINGS (object);

	g_object_unref (warnings->priv->conf);

	G_OBJECT_CLASS (gpm_warnings_parent_class)->finalize (object);
}

/**
 * gconf_key_changed_cb:
 **/
static void
gconf_key_changed_cb (GpmConf     *conf,
		      const gchar *key,
		      GpmWarnings  *warnings)
{
	g_return_if_fail (GPM_IS_WARNINGS (warnings));

	if (strcmp (key, GPM_CONF_USE_TIME_POLICY) == 0) {
		gpm_conf_get_bool (warnings->priv->conf,
				   GPM_CONF_USE_TIME_POLICY,
				   &warnings->priv->use_time_primary);
	}
}

/**
 * gpm_warnings_class_init:
 **/
static void
gpm_warnings_class_init (GpmWarningsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_warnings_finalize;

	g_type_class_add_private (klass, sizeof (GpmWarningsPrivate));
}

/**
 * gpm_warnings_init:
 * @warnings: This warnings class instance
 *
 * initialises the warnings class. NOTE: We expect warnings objects
 * to *NOT* be removed or added during the session.
 * We only control the first warnings object if there are more than one.
 **/
static void
gpm_warnings_init (GpmWarnings *warnings)
{
	warnings->priv = GPM_WARNINGS_GET_PRIVATE (warnings);

	warnings->priv->conf = gpm_conf_new ();
	g_signal_connect (warnings->priv->conf, "value-changed",
			  G_CALLBACK (gconf_key_changed_cb), warnings);

	/* get percentage policy */
	gpm_conf_get_uint (warnings->priv->conf, GPM_CONF_THRESH_PERCENTAGE_LOW, &warnings->priv->low_percentage);
	gpm_conf_get_uint (warnings->priv->conf, GPM_CONF_THRESH_PERCENTAGE_CRITICAL, &warnings->priv->critical_percentage);
	gpm_conf_get_uint (warnings->priv->conf, GPM_CONF_THRESH_PERCENTAGE_ACTION, &warnings->priv->action_percentage);

	/* get time policy */
	gpm_conf_get_uint (warnings->priv->conf, GPM_CONF_THRESH_TIME_LOW, &warnings->priv->low_time);
	gpm_conf_get_uint (warnings->priv->conf, GPM_CONF_THRESH_TIME_CRITICAL, &warnings->priv->critical_time);
	gpm_conf_get_uint (warnings->priv->conf, GPM_CONF_THRESH_TIME_ACTION, &warnings->priv->action_time);

	/* We can disable this if the ACPI BIOS is broken, and the
	   time_remaining is therefore inaccurate or just plain wrong. */
	gpm_conf_get_bool (warnings->priv->conf, GPM_CONF_USE_TIME_POLICY, &warnings->priv->use_time_primary);
	if (warnings->priv->use_time_primary) {
		egg_debug ("Using per-time notification policy");
	} else {
		egg_debug ("Using percentage notification policy");
	}
}

/**
 * gpm_warnings_new:
 * Return value: A new warnings class instance.
 **/
GpmWarnings *
gpm_warnings_new (void)
{
	if (gpm_warnings_object != NULL) {
		g_object_ref (gpm_warnings_object);
	} else {
		gpm_warnings_object = g_object_new (GPM_TYPE_WARNINGS, NULL);
		g_object_add_weak_pointer (gpm_warnings_object, &gpm_warnings_object);
	}
	return GPM_WARNINGS (gpm_warnings_object);
}

