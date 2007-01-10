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

#include "gpm-powermanager.h"
#include "gpm-debug.h"
#include "gpm-proxy.h"

static void     gpm_powermanager_class_init (GpmPowermanagerClass *klass);
static void     gpm_powermanager_init       (GpmPowermanager      *powermanager);
static void     gpm_powermanager_finalize   (GObject		  *object);

#define GPM_POWERMANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_POWERMANAGER, GpmPowermanagerPrivate))

struct GpmPowermanagerPrivate
{
	GpmProxy		*gproxy_brightness;
	GpmProxy		*gproxy_inhibit;
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

	proxy = gpm_proxy_get_proxy (powermanager->priv->gproxy_brightness);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "GetBrightness", &error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &policy_brightness,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == TRUE) {
		*brightness = policy_brightness;
	} else {
		/* abort as the DBUS method failed */
		gpm_warning ("GetBrightness failed!");
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

	proxy = gpm_proxy_get_proxy (powermanager->priv->gproxy_brightness);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "SetBrightness", &error,
				 G_TYPE_UINT, brightness,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("SetBrightness failed!");
	}

	return ret;
}

/** cookie is returned as an unsigned integer */
gboolean
gpm_powermanager_inhibit (GpmPowermanager *powermanager,
			  const gchar     *appname,
		          const gchar     *reason,
		          guint	          *cookie)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);
	g_return_val_if_fail (cookie != NULL, FALSE);

	proxy = gpm_proxy_get_proxy (powermanager->priv->gproxy_inhibit);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "RequestCookie", &error,
				 G_TYPE_STRING, appname,
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
		*cookie = 0;
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("RequestCookie failed!");
	}

	return ret;
}

gboolean
gpm_powermanager_uninhibit (GpmPowermanager *powermanager,
			    guint            cookie)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);

	proxy = gpm_proxy_get_proxy (powermanager->priv->gproxy_inhibit);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "ClearCookie", &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("ClearCookie failed!");
	}

	return ret;
}

gboolean
gpm_powermanager_is_valid (GpmPowermanager *powermanager,
			   gboolean        *valid)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (GPM_IS_POWERMANAGER (powermanager), FALSE);

	proxy = gpm_proxy_get_proxy (powermanager->priv->gproxy_inhibit);
	if (proxy == NULL) {
		gpm_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (proxy, "IsValid", &error,
				 G_TYPE_INVALID,
				 G_TYPE_BOOLEAN, valid,
				 G_TYPE_INVALID);
	if (error) {
		gpm_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		gpm_warning ("IsValid failed!");
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

	powermanager->priv->gproxy_brightness = gpm_proxy_new ();
	proxy = gpm_proxy_assign (powermanager->priv->gproxy_brightness,
				  GPM_PROXY_SESSION,
				  GPM_DBUS_SERVICE,
				  GPM_DBUS_PATH_BRIGHT_LCD,
				  GPM_DBUS_INTERFACE_BRIGHT_LCD);
	powermanager->priv->gproxy_inhibit = gpm_proxy_new ();
	proxy = gpm_proxy_assign (powermanager->priv->gproxy_inhibit,
				  GPM_PROXY_SESSION,
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
