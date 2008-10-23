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

#include <hal-device.h>
#include <hal-manager.h>
#include <hal-device-store.h>

void
egg_test_hal_device_store (GpmSelfTest *test)
{
	HalDeviceStore *device_store;
	HalDevice *device;
	HalDevice *device2;
	HalDevice *device3;
	gboolean ret;

	if (egg_test_start (test, "HalDeviceStore") == FALSE) {
		return;
	}

	/************************************************************/
	egg_test_title (test, "make sure we get a non null device_store");
	device_store = hal_device_store_new ();
	if (device_store != NULL) {
		egg_test_success (test, "got HalDeviceStore");
	} else {
		egg_test_failed (test, "could not get HalDeviceStore");
	}

	/************************************************************/
	egg_test_title (test, "make sure device not in device_store");
	device = hal_device_new ();
	hal_device_set_udi (device, HAL_ROOT_COMPUTER);
	ret = hal_device_store_present (device_store, device);
	if (!ret) {
		egg_test_success (test, "could not get different device");
	} else {
		egg_test_failed (test, "got computer in empty store");
	}

	/************************************************************/
	egg_test_title (test, "insert device");
	ret = hal_device_store_insert (device_store, device);
	if (ret) {
		egg_test_success (test, "inserted device");
	} else {
		egg_test_failed (test, "could not insert device");
	}

	/************************************************************/
	egg_test_title (test, "insert duplicate device");
	ret = hal_device_store_insert (device_store, device);
	if (!ret) {
		egg_test_success (test, "cannot insert duplicate device");
	} else {
		egg_test_failed (test, "inserted duplicate device");
	}

	/************************************************************/
	egg_test_title (test, "make sure device in store");
	ret = hal_device_store_present (device_store, device);
	if (ret) {
		egg_test_success (test, "found device");
	} else {
		egg_test_failed (test, "could not find device");
	}

	/************************************************************/
	egg_test_title (test, "find device by UDI");
	device3 = hal_device_store_find_udi (device_store, HAL_ROOT_COMPUTER);
	if (device3 == device) {
		egg_test_success (test, "found device");
	} else {
		egg_test_failed (test, "could not find device");
	}

	/************************************************************/
	egg_test_title (test, "find missing device by UDI");
	device3 = hal_device_store_find_udi (device_store, "/foo");
	if (device3 == NULL) {
		egg_test_success (test, "not found invalid device");
	} else {
		egg_test_failed (test, "Found /foo device");
	}

	/************************************************************/
	egg_test_title (test, "make sure we can match on UDI");
	device2 = hal_device_new ();
	hal_device_set_udi (device2, HAL_ROOT_COMPUTER);
	ret = hal_device_store_present (device_store, device2);
	if (ret) {
		egg_test_success (test, "found device");
	} else {
		egg_test_failed (test, "could not find device");
	}

	/************************************************************/
	egg_test_title (test, "remove device");
	g_object_ref (device); /* so we can test it in a minute */
	ret = hal_device_store_remove (device_store, device);
	if (ret) {
		egg_test_success (test, "removed device");
	} else {
		egg_test_failed (test, "could not remove device");
	}

	/************************************************************/
	egg_test_title (test, "make sure device not in device_store");
	ret = hal_device_store_present (device_store, device);
	if (!ret) {
		egg_test_success (test, "could not get device from empty device_store");
	} else {
		egg_test_failed (test, "got computer in empty store");
	}

	g_object_unref (device);
	g_object_unref (device2);
	g_object_unref (device_store);

	egg_test_end (test);
}

