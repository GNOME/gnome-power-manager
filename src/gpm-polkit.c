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

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "gpm-polkit.h"
#include "gpm-debug.h"
#include "gpm-dbus-system-monitor.h"

static void     gpm_polkit_class_init (GpmPolkitClass *klass);
static void     gpm_polkit_init       (GpmPolkit      *polkit);
static void     gpm_polkit_finalize   (GObject		   *object);

#define GPM_POLKIT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POLKIT, GpmPolkitPrivate))

#define POLKITD_SERVICE			"org.freedesktop.PolicyKit"
#define POLKITD_MANAGER_PATH		"/org/freedesktop/PolicyKit/Manager"
#define POLKITD_MANAGER_INTERFACE	"org.freedesktop.PolicyKit.Manager"

struct GpmPolkitPrivate
{
	GpmDbusSystemMonitor	*dbus_system;
	DBusGConnection		*system_connection;
	DBusGProxy		*polkit_proxy;
};

G_DEFINE_TYPE (GpmPolkit, gpm_polkit, G_TYPE_OBJECT)

/**
 * gpm_polkit_connect:
 * @polkit: This polkit class instance
 **/
static void
gpm_polkit_connect (GpmPolkit *polkit)
{
	polkit->priv->polkit_proxy = dbus_g_proxy_new_for_name (polkit->priv->system_connection,
								POLKITD_SERVICE,
								POLKITD_MANAGER_PATH,
								POLKITD_MANAGER_INTERFACE);
	gpm_debug ("gnome-polkit connected to the session DBUS");
}

/**
 * gpm_polkit_disconnect:
 * @polkit: This polkit class instance
 **/
static void
gpm_polkit_disconnect (GpmPolkit *polkit)
{
	if (polkit->priv->polkit_proxy) {
		g_object_unref (G_OBJECT (polkit->priv->polkit_proxy));
		polkit->priv->polkit_proxy = NULL;
	}

	gpm_debug ("gnome-polkit disconnected from the session DBUS");
}

/**
 * gpm_polkit_is_user_privileged:
 * @polkit: This polkit class instance
 * Return value: Success value.
 **/
gboolean
gpm_polkit_is_user_privileged (GpmPolkit *polkit, const char *privilege)
{
	GError *error = NULL;
	gboolean boolret = TRUE;

	const char *user = g_get_user_name ();
	const char *bus_unique_name = dbus_g_proxy_get_bus_name (polkit->priv->polkit_proxy);
	const char *myresource = NULL;
	const char *but_restricted_to = NULL;
	gboolean out_is_allowed;
	gboolean out_is_temporary;

	if (!dbus_g_proxy_call (polkit->priv->polkit_proxy, "IsUserPrivileged", &error,
				G_TYPE_STRING, bus_unique_name, 
				G_TYPE_STRING, user, 
				G_TYPE_STRING, privilege,
				G_TYPE_STRING, myresource,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &out_is_allowed,
				G_TYPE_BOOLEAN, &out_is_temporary,
				G_TYPE_STRING, but_restricted_to,
				G_TYPE_INVALID)) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_debug ("polkitd service is not running.");
		boolret = FALSE;
	}
	if (!boolret) {
		gpm_debug ("IsUserPrivileged failed");
		return FALSE;
	}
	return out_is_allowed;
}

/**
 * dbus_name_owner_changed_system_cb:
 * @power: The power class instance
 * @name: The DBUS name, e.g. hal.freedesktop.org
 * @prev: The previous name, e.g. :0.13
 * @new: The new name, e.g. :0.14
 * @manager: This manager class instance
 *
 * The name-owner-changed session DBUS callback.
 **/
static void
dbus_name_owner_changed_system_cb (GpmDbusSystemMonitor *dbus_monitor,
				    const char	   *name,
				    const char     *prev,
				    const char     *new,
				    GpmPolkit *polkit)
{
	if (strcmp (name, POLKITD_SERVICE) == 0) {
		if (strlen (prev) != 0 && strlen (new) == 0 ) {
			gpm_polkit_disconnect (polkit);
		}
		if (strlen (prev) == 0 && strlen (new) != 0 ) {
			gpm_polkit_connect (polkit);
		}
	}
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
	GError *error = NULL;
	polkit->priv = GPM_POLKIT_GET_PRIVATE (polkit);

	polkit->priv->dbus_system = gpm_dbus_system_monitor_new ();
	g_signal_connect (polkit->priv->dbus_system, "name-owner-changed",
			  G_CALLBACK (dbus_name_owner_changed_system_cb), polkit);

	polkit->priv->polkit_proxy = NULL;

	polkit->priv->system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (! polkit->priv->system_connection) {
		if (error) {
			gpm_warning ("%s", error->message);
			g_error_free (error);
		}
		gpm_critical_error ("Cannot connect to DBUS Session Daemon");
	}

	/* blindly try to connect */
	gpm_polkit_connect (polkit);
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

	gpm_polkit_disconnect (polkit);
	g_object_unref (polkit->priv->dbus_system);
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
	polkit = g_object_new (GPM_TYPE_POLKIT, NULL);
	return GPM_POLKIT (polkit);
}
