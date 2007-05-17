/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include "gpm-st-main.h"
#include <libdbus-proxy.h>

#include "../src/gpm-common.h"
#include "../src/gpm-inhibit.h"

/** cookie is returned as an unsigned integer */
static gboolean
inhibit (DbusProxy *gproxy,
			  const gchar     *appname,
		          const gchar     *reason,
		          guint           *cookie)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	g_return_val_if_fail (cookie != NULL, FALSE);

	proxy = dbus_proxy_get_proxy (gproxy);
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

static gboolean
uninhibit (DbusProxy *gproxy,
			    guint            cookie)
{
	GError *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	proxy = dbus_proxy_get_proxy (gproxy);
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

static gboolean
has_inhibit (DbusProxy *gproxy,
			      gboolean        *has_inhibit)
{
	GError  *error = NULL;
	gboolean ret;
	DBusGProxy *proxy;

	proxy = dbus_proxy_get_proxy (gproxy);
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

void
gpm_st_inhibit (GpmSelfTest *test)
{
	gboolean ret;
	gboolean valid;
	guint cookie1 = 0;
	guint cookie2 = 0;
	DbusProxy *gproxy;

	test->type = "GpmInhibit       ";
	gproxy = dbus_proxy_new ();
	dbus_proxy_assign (gproxy, DBUS_PROXY_SESSION,
				   GPM_DBUS_SERVICE,
				   GPM_DBUS_PATH_INHIBIT,
				   GPM_DBUS_INTERFACE_INHIBIT);

	if (gproxy == NULL) {
		g_warning ("Unable to get connection to power manager");
		return;
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are not inhibited");
	ret = has_inhibit (gproxy, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == TRUE) {
		gpm_st_failed (test, "Already inhibited");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "clear an invalid cookie");
	ret = uninhibit (gproxy, 123456);
	if (ret == FALSE) {
		gpm_st_success (test, "invalid cookie failed as expected");
	} else {
		gpm_st_failed (test, "should have rejected invalid cookie");
	}

	/************************************************************/
	gpm_st_title (test, "get auto cookie 1");
	ret = inhibit (gproxy,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie1);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to inhibit");
	} else if (cookie1 == 0) {
		gpm_st_failed (test, "Cookie invalid (cookie: %u)", cookie1);
	} else {
		gpm_st_success (test, "cookie: %u", cookie1);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are auto inhibited");
	ret = has_inhibit (gproxy, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == TRUE) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "get cookie 2");
	ret = inhibit (gproxy,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie2);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to inhibit");
	} else if (cookie2 == 0) {
		gpm_st_failed (test, "Cookie invalid (cookie: %u)", cookie2);
	} else {
		gpm_st_success (test, "cookie: %u", cookie2);
	}

	/************************************************************/
	gpm_st_title (test, "clear cookie 1");
	ret = uninhibit (gproxy, cookie1);
	if (ret == FALSE) {
		gpm_st_failed (test, "cookie failed to clear");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are still inhibited");
	ret = has_inhibit (gproxy, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == TRUE) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "clear cookie 2");
	ret = uninhibit (gproxy, cookie2);
	if (ret == FALSE) {
		gpm_st_failed (test, "cookie failed to clear");
	} else {
		gpm_st_success (test, NULL);
	}

	g_object_unref (gproxy);
}

