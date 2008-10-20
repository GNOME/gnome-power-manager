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
#include <egg-dbus-proxy.h>
#include "gpm-st-main.h"
#include "../src/gpm-common.h"

void
egg_test_proxy (GpmSelfTest *test)
{
	EggDbusProxy *gproxy = NULL;
	DBusGProxy *proxy = NULL;
	DBusGConnection *connection;

	if (egg_test_start (test, "EggDbusProxy") == FALSE) {
		return;
	}

	/************************************************************/
	egg_test_title (test, "make sure we can get a new gproxy");
	gproxy = egg_dbus_proxy_new ();
	if (gproxy != NULL) {
		egg_test_success (test, "got gproxy");
	} else {
		egg_test_failed (test, "could not get gproxy");
	}

	/************************************************************/
	egg_test_title (test, "make sure proxy if NULL when no assign");
	proxy = egg_dbus_proxy_get_proxy (gproxy);
	if (proxy == NULL) {
		egg_test_success (test, "got NULL proxy");
	} else {
		egg_test_failed (test, "did not get NULL proxy");
	}

	/************************************************************/
	egg_test_title (test, "make sure we can assign and connect");
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
	proxy = egg_dbus_proxy_assign (gproxy, connection, GPM_DBUS_SERVICE,
				       GPM_DBUS_PATH_INHIBIT, GPM_DBUS_INTERFACE_INHIBIT);
	if (proxy != NULL) {
		egg_test_success (test, "got proxy (init)");
	} else {
		egg_test_failed (test, "could not get proxy (init)");
	}

	/************************************************************/
	egg_test_title (test, "make sure proxy non NULL when assigned");
	proxy = egg_dbus_proxy_get_proxy (gproxy);
	if (proxy != NULL) {
		egg_test_success (test, "got valid proxy");
	} else {
		egg_test_failed (test, "did not get valid proxy");
	}

	g_object_unref (gproxy);

	egg_test_end (test);
}

