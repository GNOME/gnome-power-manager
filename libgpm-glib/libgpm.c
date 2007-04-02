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

#include "libgpm.h"

static void     gpm_powermanager_class_init (GpmPowermanagerClass *klass);
static void     gpm_powermanager_init       (GpmPowermanager      *powermanager);
static void     gpm_powermanager_finalize   (GObject		  *object);

#define GPM_POWERMANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POWERMANAGER, GpmPowermanagerPrivate))

struct GpmPowermanagerPrivate
{
	DbusProxy		*gproxy_brightness;
	DbusProxy		*gproxy_inhibit;
};

G_DEFINE_TYPE (GpmPowermanager, gpm_powermanager, G_TYPE_OBJECT)

/**
 * gpm_powermanager_get_brightness_lcd:
 * Return value: Success value, or zero for failure
 **/
gboolean
gpm_powermanager_get_brightness_lcd (GpmPowermanager *powermanager,
				     guint	     *brightness)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;
	guint policy_brightness;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);
	g_return_val_if_fail (brightness != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (powermanager->priv->gproxy_brightness);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &policy_brightness,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == TRUE) {
		*brightness = policy_brightness;
	} else {
		/* abort as the DBUS method failed */
		g_warning ("GetBrightness failed!");
	}

	return ret;
}

/**
 * gpm_powermanager_set_brightness_lcd:
 * Return value: Success value, or zero for failure
 **/
gboolean
gpm_powermanager_set_brightness_lcd (GpmPowermanager *powermanager,
				     guint	      brightness)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);

	proxy = dbus_proxy_get_proxy (powermanager->priv->gproxy_brightness);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "SetBrightness", &error,
				 G_TYPE_UINT, brightness,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		g_warning ("SetBrightness failed!");
	}

	return ret;
}

/** cookie is returned as an unsigned integer */
gboolean
gpm_powermanager_inhibit (GpmPowermanager *powermanager,
			  const gchar     *appname,
		          const gchar     *reason,
		          guint           *cookie)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);
	g_return_val_if_fail (cookie != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (powermanager->priv->gproxy_inhibit);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "Inhibit", &error,
				 G_TYPE_STRING, appname,
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
		*cookie = 0;
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		g_warning ("Inhibit failed!");
	}

	return ret;
}

gboolean
gpm_powermanager_uninhibit (GpmPowermanager *powermanager,
			    guint            cookie)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);

	proxy = dbus_proxy_get_proxy (powermanager->priv->gproxy_inhibit);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "UnInhibit", &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		g_warning ("UnInhibit failed!");
	}

	return ret;
}

gboolean
gpm_powermanager_has_inhibit (GpmPowermanager *powermanager,
			      gboolean        *has_inhibit)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);

	proxy = dbus_proxy_get_proxy (powermanager->priv->gproxy_inhibit);
	if (proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "HasInhibit", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, has_inhibit,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		g_warning ("HasInhibit failed!");
	}

	return ret;
}

/**
 * gpm_powermanager_class_init:
 * @klass: This class instance
 **/
static void
gpm_powermanager_class_init (GpmPowermanagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_powermanager_finalize;
	g_type_class_add_private (klass, sizeof (GpmPowermanagerPrivate));
}

/**
 * gpm_powermanager_init:
 * @powermanager: This class instance
 **/
static void
gpm_powermanager_init (GpmPowermanager *powermanager)
{
	DBusGProxy *proxy;

	powermanager->priv = GPM_POWERMANAGER_GET_PRIVATE (powermanager);

	powermanager->priv->gproxy_brightness = dbus_proxy_new ();
	proxy = dbus_proxy_assign (powermanager->priv->gproxy_brightness,
				   DBUS_PROXY_SESSION,
				   GPM_DBUS_SERVICE,
				   GPM_DBUS_PATH_BACKLIGHT,
				   GPM_DBUS_INTERFACE_BACKLIGHT);
	powermanager->priv->gproxy_inhibit = dbus_proxy_new ();
	proxy = dbus_proxy_assign (powermanager->priv->gproxy_inhibit,
				   DBUS_PROXY_SESSION,
				   GPM_DBUS_SERVICE,
				   GPM_DBUS_PATH_INHIBIT,
				   GPM_DBUS_INTERFACE_INHIBIT);
}

/**
 * gpm_powermanager_finalize:
 * @object: This class instance
 **/
static void
gpm_powermanager_finalize (GObject *object)
{
	GpmPowermanager *powermanager;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_POWERMANAGER (object));

	powermanager = GPM_POWERMANAGER (object);
	powermanager->priv = GPM_POWERMANAGER_GET_PRIVATE (powermanager);

	g_object_unref (powermanager->priv->gproxy_brightness);
	g_object_unref (powermanager->priv->gproxy_inhibit);

	G_OBJECT_CLASS (gpm_powermanager_parent_class)->finalize (object);
}

/**
 * gpm_powermanager_new:
 * Return value: new GpmPowermanager instance.
 **/
GpmPowermanager *
gpm_powermanager_new (void)
{
	GpmPowermanager *powermanager;
	powermanager = g_object_new (GPM_TYPE_POWERMANAGER, NULL);
	return GPM_POWERMANAGER (powermanager);
}
