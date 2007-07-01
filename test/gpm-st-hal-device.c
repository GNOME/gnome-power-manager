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
#include <string.h>
#include "gpm-st-main.h"

#include <libhal-gmanager.h>
#include <libhal-gdevice.h>

void
gpm_st_hal_device (GpmSelfTest *test)
{
	HalGDevice *device;
	const char *udi;
	char *retstr;
	gboolean ret;

	if (gpm_st_start (test, "HalGDevice", CLASS_AUTO) == FALSE) {
		return;
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null device");
	device = hal_gdevice_new ();
	if (device != NULL) {
		gpm_st_success (test, "got HalGDevice");
	} else {
		gpm_st_failed (test, "could not get HalGDevice");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a null UDI");
	udi = hal_gdevice_get_udi (device);
	if (udi == NULL) {
		gpm_st_success (test, "got NULL UDI");
	} else {
		gpm_st_failed (test, "got non-null UDI: %s", udi);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we can set a UDI");
	ret = hal_gdevice_set_udi (device, HAL_ROOT_COMPUTER);
	if (ret == TRUE) {
		gpm_st_success (test, "set UDI");
	} else {
		gpm_st_failed (test, "could not set UDI");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we can get the UDI");
	udi = hal_gdevice_get_udi (device);
	if (udi && strcmp (udi, HAL_ROOT_COMPUTER) == 0) {
		gpm_st_success (test, "got correct UDI: %s", udi);
	} else {
		gpm_st_failed (test, "got incorrect UDI: %s", udi);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we cannot set another UDI");
	ret = hal_gdevice_set_udi (device, "foo");
	if (ret == FALSE) {
		gpm_st_success (test, "Cannot set another UDI");
	} else {
		gpm_st_failed (test, "Able to overwrite UDI");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we can get a string key");
	ret = hal_gdevice_get_string (device, "info.product", &retstr, NULL);
	if (ret == TRUE && retstr && strcmp (retstr, "Computer") == 0) {
		gpm_st_success (test, "got correct key");
	} else {
		gpm_st_failed (test, "got invalid key");
	}
	g_free (retstr);

	/************************************************************/
	gpm_st_title (test, "try to get property modified events");
	ret = hal_gdevice_watch_property_modified (device);
	if (ret == TRUE) {
		gpm_st_success (test, "got notification");
	} else {
		gpm_st_failed (test, "could not get notification");
	}

	/************************************************************/
	gpm_st_title (test, "try to get duplicate property modified events");
	ret = hal_gdevice_watch_property_modified (device);
	if (ret == FALSE) {
		gpm_st_success (test, "duplicate notification refused");
	} else {
		gpm_st_failed (test, "got duplicate notification");
	}

	/************************************************************/
	gpm_st_title (test, "try to cancel property modified events");
	ret = hal_gdevice_remove_property_modified (device);
	if (ret == TRUE) {
		gpm_st_success (test, "cancel notification");
	} else {
		gpm_st_failed (test, "could not cancel notification");
	}

	/************************************************************/
	gpm_st_title (test, "try to get duplicate property modified cancel");
	ret = hal_gdevice_remove_property_modified (device);
	if (ret == FALSE) {
		gpm_st_success (test, "duplicate cancel refused");
	} else {
		gpm_st_failed (test, "did duplicate cancel");
	}

	g_object_unref (device);

	gpm_st_end (test);
}

