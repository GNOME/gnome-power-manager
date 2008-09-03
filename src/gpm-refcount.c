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

#include "gpm-refcount.h"
#include "egg-debug.h"

static void     gpm_refcount_class_init (GpmRefcountClass *klass);
static void     gpm_refcount_init       (GpmRefcount      *refcount);
static void     gpm_refcount_finalize   (GObject	  *object);

#define GPM_REFCOUNT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_REFCOUNT, GpmRefcountPrivate))

/* this is a managed refcounter */

struct GpmRefcountPrivate
{
	guint		 timeout; /* ms */
	guint		 refcount;
};

enum {
	REFCOUNT_ADDED,
	REFCOUNT_ZERO,
	LAST_SIGNAL
};

static guint	     signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmRefcount, gpm_refcount, G_TYPE_OBJECT)

/**
 * gpm_refcount_auto_decrement:
 * @data: gpointer to this class instance
 *
 * Called automatically to clear one of the refcounts
 **/
static gboolean
gpm_refcount_auto_decrement (gpointer data)
{
	GpmRefcount *refcount = (GpmRefcount*) data;

	if (refcount->priv->refcount == 0) {
		egg_warning ("no refcount to remove");
		return FALSE;
	}

	refcount->priv->refcount--;

	if (refcount->priv->refcount == 0) {
		egg_debug ("zero, so sending REFCOUNT_ZERO");
		g_signal_emit (refcount, signals [REFCOUNT_ZERO], 0);
	} else {
		egg_debug ("refcount now: %i", refcount->priv->refcount);
	}

	return FALSE;
}

/**
 * gpm_refcount_add:
 * @refcount: This class instance
 * Return value: success
 **/
gboolean
gpm_refcount_add (GpmRefcount *refcount)
{
	g_return_val_if_fail (refcount != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_REFCOUNT (refcount), FALSE);

	if (refcount->priv->timeout == 0) {
		egg_warning ("no timeout has been set");
		return FALSE;
	}

	refcount->priv->refcount++;
	egg_debug ("refcount now: %i", refcount->priv->refcount);
	egg_debug ("non zero, so sending REFCOUNT_ADDED");
	g_signal_emit (refcount, signals [REFCOUNT_ADDED], 0);

	/* remove the last timeout */
	g_idle_remove_by_data (refcount);

	/* add ONE automatic timeout */
	g_timeout_add (refcount->priv->timeout, gpm_refcount_auto_decrement, refcount);

	return TRUE;
}

/**
 * gpm_refcount_remove:
 * @refcount: This class instance
 * Return value: success
 *
 * Not normally required, but removes a refcount manually.
 **/
gboolean
gpm_refcount_remove (GpmRefcount *refcount)
{
	g_return_val_if_fail (refcount != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_REFCOUNT (refcount), FALSE);

	if (refcount->priv->refcount == 0) {
		egg_warning ("no refcount to remove");
		return FALSE;
	}

	/* BUG? we should clear the timeout also */
	return gpm_refcount_auto_decrement (refcount);
}

/**
 * gpm_refcount_set_timeout:
 * @refcount: This class instance
 * Return value: success
 **/
gboolean
gpm_refcount_set_timeout (GpmRefcount *refcount, guint timeout)
{
	g_return_val_if_fail (refcount != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_REFCOUNT (refcount), FALSE);

	if (timeout > 100000) {
		egg_warning ("refcount is not designed for long timeouts");
		return FALSE;
	}
	if (timeout == 0) {
		egg_warning ("refcount cannot be zero");
		timeout = 1000;
	}

	refcount->priv->timeout = timeout;
	return TRUE;
}

/**
 * gpm_refcount_class_init:
 * @refcount: This class instance
 **/
static void
gpm_refcount_class_init (GpmRefcountClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_refcount_finalize;
	g_type_class_add_private (klass, sizeof (GpmRefcountPrivate));

	signals [REFCOUNT_ADDED] =
		g_signal_new ("refcount-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmRefcountClass, refcount_added),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [REFCOUNT_ZERO] =
		g_signal_new ("refcount-zero",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmRefcountClass, refcount_zero),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * gpm_refcount_init:
 * @refcount: This class instance
 **/
static void
gpm_refcount_init (GpmRefcount *refcount)
{
	refcount->priv = GPM_REFCOUNT_GET_PRIVATE (refcount);

	refcount->priv->refcount = 0;
	refcount->priv->timeout = 0;
}

/**
 * gpm_refcount_finalize:
 * @object: This class instance
 **/
static void
gpm_refcount_finalize (GObject *object)
{
	GpmRefcount *refcount;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_REFCOUNT (object));

	refcount = GPM_REFCOUNT (object);
	refcount->priv = GPM_REFCOUNT_GET_PRIVATE (refcount);
	/* emit signal ? */
	G_OBJECT_CLASS (gpm_refcount_parent_class)->finalize (object);
}

/**
 * gpm_refcount_new:
 * Return value: new class instance.
 **/
GpmRefcount *
gpm_refcount_new (void)
{
	GpmRefcount *refcount;
	refcount = g_object_new (GPM_TYPE_REFCOUNT, NULL);
	return GPM_REFCOUNT (refcount);
}
