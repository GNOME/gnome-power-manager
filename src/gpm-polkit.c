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
#include <dbus/dbus-glib.h>
#include <libdbus-proxy.h>

#include "gpm-polkit.h"
#include "gpm-debug.h"

static void     gpm_polkit_class_init (GpmPolkitClass *klass);
static void     gpm_polkit_init       (GpmPolkit      *polkit);
static void     gpm_polkit_finalize   (GObject		   *object);

#define GPM_POLKIT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POLKIT, GpmPolkitPrivate))

#define POLKITD_SERVICE			"org.freedesktop.PolicyKit"
#define POLKITD_MANAGER_PATH		"/org/freedesktop/PolicyKit/Manager"
#define POLKITD_MANAGER_INTERFACE	"org.freedesktop.PolicyKit.Manager"

struct GpmPolkitPrivate
{
	DbusProxy		*gproxy;
};

G_DEFINE_TYPE (GpmPolkit, gpm_polkit, G_TYPE_OBJECT)

/**
 * gpm_polkit_is_user_privileged:
 * @polkit: This polkit class instance
 * Return value: Success value.
 **/
gboolean
gpm_polkit_is_user_privileged (GpmPolkit   *polkit,
			       const gchar *privilege)
{
	GError *error = NULL;
	const gchar *user = g_get_user_name ();
	const gchar *bus_unique_name;
	const gchar *myresource = NULL;
	const gchar *but_restricted_to = NULL;
	gboolean out_is_allowed;
	gboolean out_is_temporary;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POLKIT (polkit), FALSE);

	proxy = dbus_proxy_get_proxy (polkit->priv->gproxy);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	bus_unique_name = dbus_g_proxy_get_bus_name (proxy);

	ret = dbus_g_proxy_call (proxy, "IsUserPrivileged", &error,
				 G_TYPE_STRING, bus_unique_name,
				 G_TYPE_STRING, user,
				 G_TYPE_STRING, privilege,
				 G_TYPE_STRING, myresource,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, &out_is_allowed,
				 G_TYPE_BOOLEAN, &out_is_temporary,
				 G_TYPE_STRING, but_restricted_to,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("IsUserPrivileged failed!");
		return FALSE;
	}
	return out_is_allowed;
}

/**
 * gpm_polkit_class_init:
 * @klass: This polkit class instance
 **/
static void
gpm_polkit_class_init (GpmPolkitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_polkit_finalize;
	g_type_class_add_private (klass, sizeof (GpmPolkitPrivate));
}

/**
 * gpm_polkit_init:
 * @polkit: This polkit class instance
 **/
static void
gpm_polkit_init (GpmPolkit *polkit)
{
	polkit->priv = GPM_POLKIT_GET_PRIVATE (polkit);

	polkit->priv->gproxy = dbus_proxy_new ();
	dbus_proxy_assign (polkit->priv->gproxy,
			  DBUS_PROXY_SYSTEM,
			  POLKITD_SERVICE,
			  POLKITD_MANAGER_PATH,
			  POLKITD_MANAGER_INTERFACE);
}

/**
 * gpm_polkit_finalize:
 * @object: This polkit class instance
 **/
static void
gpm_polkit_finalize (GObject *object)
{
	GpmPolkit *polkit;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_POLKIT (object));

	polkit = GPM_POLKIT (object);
	polkit->priv = GPM_POLKIT_GET_PRIVATE (polkit);
	g_object_unref (polkit->priv->gproxy);

	G_OBJECT_CLASS (gpm_polkit_parent_class)->finalize (object);
}

/**
 * gpm_polkit_new:
 * Return value: new GpmPolkit instance.
 **/
GpmPolkit *
gpm_polkit_new (void)
{
	GpmPolkit *polkit;
#ifdef HAVE_POLKIT
	polkit = g_object_new (GPM_TYPE_POLKIT, NULL);
#else
	polkit = NULL;
#endif
	return GPM_POLKIT (polkit);
}
