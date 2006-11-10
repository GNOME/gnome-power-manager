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

#include "gpm-conf.h"
#include "gpm-common.h"
#include "gpm-debug.h"
#include "gpm-hal.h"
#include "gpm-policy.h"
#include "gpm-polkit.h"

#define GPM_POLICY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POLICY, GpmPolicyPrivate))

struct GpmPolicyPrivate
{
	GpmConf			*conf;
	GpmHal			*hal;
	GpmPolkit		*polkit;
};

G_DEFINE_TYPE (GpmPolicy, gpm_policy, G_TYPE_OBJECT)

/**
 * gpm_policy_allowed_suspend:
 * @policy: This class instance
 * @can: If we can suspend
 *
 * Checks the HAL key power_management.can_suspend_to_ram and also
 * checks gconf to see if we are allowed to suspend this computer.
 **/
gboolean
gpm_policy_allowed_suspend (GpmPolicy *policy,
			    gboolean  *can)
{
	gboolean conf_ok;
	gboolean polkit_ok = TRUE;
	gboolean hal_ok = FALSE;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;
	gpm_conf_get_bool (policy->priv->conf, GPM_CONF_CAN_SUSPEND, &conf_ok);
	hal_ok = gpm_hal_can_suspend (policy->priv->hal);
	if (policy->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (policy->priv->polkit, "hal-power-suspend");
	}
	if ( conf_ok && hal_ok && polkit_ok ) {
		*can = TRUE;
	}

	return TRUE;
}

/**
 * gpm_policy_allowed_hibernate:
 * @policy: This class instance
 * @can: If we can hibernate
 *
 * Checks the HAL key power_management.can_suspend_to_disk and also
 * checks gconf to see if we are allowed to hibernate this computer.
 **/
gboolean
gpm_policy_allowed_hibernate (GpmPolicy *policy,
			      gboolean  *can)
{
	gboolean conf_ok;
	gboolean polkit_ok = TRUE;
	gboolean hal_ok = FALSE;
	g_return_val_if_fail (can, FALSE);

	*can = FALSE;
	gpm_conf_get_bool (policy->priv->conf, GPM_CONF_CAN_HIBERNATE, &conf_ok);
	hal_ok = gpm_hal_can_hibernate (policy->priv->hal);
	if (policy->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (policy->priv->polkit, "hal-power-hibernate");
	}
	if ( conf_ok && hal_ok && polkit_ok ) {
		*can = TRUE;
	}
	return TRUE;
}

/**
 * gpm_policy_allowed_shutdown:
 * @policy: This class instance
 * @can: If we can shutdown
 *
 **/
gboolean
gpm_policy_allowed_shutdown (GpmPolicy *policy,
			     gboolean  *can)
{
	gboolean polkit_ok = TRUE;
	g_return_val_if_fail (can, FALSE);
	*can = FALSE;
	if (policy->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (policy->priv->polkit, "hal-power-shutdown");
	}
	if (polkit_ok == TRUE) {
		*can = TRUE;
	}
	return TRUE;
}

/**
 * gpm_policy_allowed_reboot:
 * @policy: This class instance
 * @can: If we can reboot
 *
 * Stub function -- TODO.
 **/
gboolean
gpm_policy_allowed_reboot (GpmPolicy *policy,
			   gboolean  *can)
{
	gboolean polkit_ok = TRUE;
	g_return_val_if_fail (can, FALSE);
	*can = FALSE;
	if (policy->priv->polkit) {
		polkit_ok = gpm_polkit_is_user_privileged (policy->priv->polkit, "hal-power-reboot");
	}
	if (polkit_ok == TRUE) {
		*can = TRUE;
	}
	return TRUE;
}

/**
 * gpm_policy_constructor:
 **/
static GObject *
gpm_policy_constructor (GType		  type,
			      guint		  n_construct_properties,
			      GObjectConstructParam *construct_properties)
{
	GpmPolicy      *policy;
	GpmPolicyClass *klass;
	klass = GPM_POLICY_CLASS (g_type_class_peek (GPM_TYPE_POLICY));
	policy = GPM_POLICY (G_OBJECT_CLASS (gpm_policy_parent_class)->constructor
			      		     (type, n_construct_properties, construct_properties));
	return G_OBJECT (policy);
}

/**
 * gpm_policy_finalize:
 **/
static void
gpm_policy_finalize (GObject *object)
{
	GpmPolicy *policy;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_POLICY (object));
	policy = GPM_POLICY (object);

	if (policy->priv->conf) {
		g_object_unref (policy->priv->conf);
	}
	if (policy->priv->hal) {
		g_object_unref (policy->priv->hal);
	}
	if (policy->priv->polkit) {
		g_object_unref (policy->priv->polkit);
	}

	g_return_if_fail (policy->priv != NULL);
	G_OBJECT_CLASS (gpm_policy_parent_class)->finalize (object);
}

/**
 * gpm_policy_class_init:
 **/
static void
gpm_policy_class_init (GpmPolicyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize	   = gpm_policy_finalize;
	object_class->constructor  = gpm_policy_constructor;

	g_type_class_add_private (klass, sizeof (GpmPolicyPrivate));
}

/**
 * gpm_policy_init:
 * @policy: This policy class instance
 *
 * initialises the policy class. NOTE: We expect policy objects
 * to *NOT* be removed or added during the session.
 * We only control the first policy object if there are more than one.
 **/
static void
gpm_policy_init (GpmPolicy *policy)
{
	policy->priv = GPM_POLICY_GET_PRIVATE (policy);
	policy->priv->hal = gpm_hal_new ();

	/* this will be NULL if we don't compile in support */
	policy->priv->polkit = gpm_polkit_new ();

	policy->priv->conf = gpm_conf_new ();
}

/**
 * gpm_policy_new:
 * Return value: A new policy class instance.
 **/
GpmPolicy *
gpm_policy_new (void)
{
	static GpmPolicy *policy = NULL;

	if (policy != NULL) {
		g_object_ref (policy);
		return policy;
	}

	policy = g_object_new (GPM_TYPE_POLICY, NULL);
	return policy;
}
