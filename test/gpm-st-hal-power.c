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

#include <hal-device-power.h>

void
egg_test_hal_device_power (GpmSelfTest *test)
{
	HalDevicePower *power;
	gboolean ret;

	if (egg_test_start (test, "HalDevicePower") == FALSE) {
		return;
	}

	/************************************************************/
	egg_test_title (test, "make sure we get a non null device");
	power = hal_device_power_new ();
	if (power != NULL) {
		egg_test_success (test, "got HalDevicePower");
	} else {
		egg_test_failed (test, "could not get HalDevicePower");
	}

	/************************************************************/
	egg_test_title (test, "make sure we have pm support");
	ret = hal_device_power_has_support (power);
	if (ret) {
		egg_test_success (test, "has pm support");
	} else {
		egg_test_failed (test, "does not have pm support");
	}

	g_object_unref (power);

	egg_test_end (test);
}

