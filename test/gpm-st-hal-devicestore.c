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

#include <libhal-gdevice.h>
#include <libhal-gmanager.h>
#include <libhal-gdevicestore.h>

void
egg_test_hal_devicestore (GpmSelfTest *test)
{
	HalGDevicestore *devicestore;
	HalGDevice *device;
	HalGDevice *device2;
	HalGDevice *device3;
	gboolean ret;

	if (egg_test_start (test, "HalGDevicestore") == FALSE) {
		return;
	}

	/************************************************************/
	egg_test_title (test, "make sure we get a non null devicestore");
	devicestore = hal_gdevicestore_new ();
	if (devicestore != NULL) {
		egg_test_success (test, "got HalGDevicestore");
	} else {
		egg_test_failed (test, "could not get HalGDevicestore");
	}

	/************************************************************/
	egg_test_title (test, "make sure device not in devicestore");
	device = hal_gdevice_new ();
	hal_gdevice_set_udi (device, HAL_ROOT_COMPUTER);
	ret = hal_gdevicestore_present (devicestore, device);
	if (!ret) {
		egg_test_success (test, "could not get different device");
	} else {
		egg_test_failed (test, "got computer in empty store");
	}

	/************************************************************/
	egg_test_title (test, "insert device");
	ret = hal_gdevicestore_insert (devicestore, device);
	if (ret) {
		egg_test_success (test, "inserted device");
	} else {
		egg_test_failed (test, "could not insert device");
	}

	/************************************************************/
	egg_test_title (test, "insert duplicate device");
	ret = hal_gdevicestore_insert (devicestore, device);
	if (!ret) {
		egg_test_success (test, "cannot insert duplicate device");
	} else {
		egg_test_failed (test, "inserted duplicate device");
	}

	/************************************************************/
	egg_test_title (test, "make sure device in store");
	ret = hal_gdevicestore_present (devicestore, device);
	if (ret) {
		egg_test_success (test, "found device");
	} else {
		egg_test_failed (test, "could not find device");
	}

	/************************************************************/
	egg_test_title (test, "find device by UDI");
	device3 = hal_gdevicestore_find_udi (devicestore, HAL_ROOT_COMPUTER);
	if (device3 == device) {
		egg_test_success (test, "found device");
	} else {
		egg_test_failed (test, "could not find device");
	}

	/************************************************************/
	egg_test_title (test, "find missing device by UDI");
	device3 = hal_gdevicestore_find_udi (devicestore, "/foo");
	if (device3 == NULL) {
		egg_test_success (test, "not found invalid device");
	} else {
		egg_test_failed (test, "Found /foo device");
	}

	/************************************************************/
	egg_test_title (test, "make sure we can match on UDI");
	device2 = hal_gdevice_new ();
	hal_gdevice_set_udi (device2, HAL_ROOT_COMPUTER);
	ret = hal_gdevicestore_present (devicestore, device2);
	if (ret) {
		egg_test_success (test, "found device");
	} else {
		egg_test_failed (test, "could not find device");
	}

	/************************************************************/
	egg_test_title (test, "remove device");
	g_object_ref (device); /* so we can test it in a minute */
	ret = hal_gdevicestore_remove (devicestore, device);
	if (ret) {
		egg_test_success (test, "removed device");
	} else {
		egg_test_failed (test, "could not remove device");
	}

	/************************************************************/
	egg_test_title (test, "make sure device not in devicestore");
	ret = hal_gdevicestore_present (devicestore, device);
	if (!ret) {
		egg_test_success (test, "could not get device from empty devicestore");
	} else {
		egg_test_failed (test, "got computer in empty store");
	}

	g_object_unref (device);
	g_object_unref (device2);
	g_object_unref (devicestore);

	egg_test_end (test);
}

