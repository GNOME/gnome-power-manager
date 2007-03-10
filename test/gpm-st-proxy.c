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
#include <dbus/dbus-glib.h>
#include "gpm-st-main.h"

#include "../src/gpm-proxy.h"
#include "../src/gpm-powermanager.h"

void
gpm_st_proxy (GpmSelfTest *test)
{
	GpmProxy *gproxy = NULL;
	DBusGProxy *proxy = NULL;

	test->type = "GpmProxy         ";

	/************************************************************/
	gpm_st_title (test, "make sure we can get a new gproxy");
	gproxy = gpm_proxy_new ();
	if (gproxy != NULL) {
		gpm_st_success (test, "got gproxy");
	} else {
		gpm_st_failed (test, "could not get gproxy");
	}

	/************************************************************/
	gpm_st_title (test, "make sure proxy if NULL when no assign");
	proxy = gpm_proxy_get_proxy (gproxy);
	if (proxy == NULL) {
		gpm_st_success (test, "got NULL proxy");
	} else {
		gpm_st_failed (test, "did not get NULL proxy");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we can assign and connect");
	proxy = gpm_proxy_assign (gproxy,
				  GPM_PROXY_SESSION,
				  GPM_DBUS_SERVICE,
				  GPM_DBUS_PATH_INHIBIT,
				  GPM_DBUS_INTERFACE_INHIBIT);
	if (proxy != NULL) {
		gpm_st_success (test, "got proxy (init)");
	} else {
		gpm_st_failed (test, "could not get proxy (init)");
	}

	/************************************************************/
	gpm_st_title (test, "make sure proxy non NULL when assigned");
	proxy = gpm_proxy_get_proxy (gproxy);
	if (proxy != NULL) {
		gpm_st_success (test, "got valid proxy");
	} else {
		gpm_st_failed (test, "did not get valid proxy");
	}

	g_object_unref (gproxy);
}

