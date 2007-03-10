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

#include <libhal-gmanager.h>

void
gpm_st_hal_manager (GpmSelfTest *test)
{
	HalGManager *manager;
	gboolean ret;
	test->type = "HalGManager      ";

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null device");
	manager = hal_gmanager_new ();
	if (manager != NULL) {
		gpm_st_success (test, "got HalGManager");
	} else {
		gpm_st_failed (test, "could not get HalGManager");
	}

	/************************************************************/
	gpm_st_title (test, "check if we are a laptop");
	ret = hal_gmanager_is_laptop (manager);
	if (ret == TRUE) {
		gpm_st_success (test, "identified as a laptop");
	} else {
		gpm_st_failed (test, "not identified as a laptop");
	}


	/************************************************************/
	gchar **value;
	gpm_st_title (test, "search for battery devices");
	ret = hal_gmanager_find_capability (manager, "battery", &value, NULL);
	if (ret == TRUE && value != NULL && value[0] != NULL) {
		gpm_st_success (test, "found battery device");
	} else {
		gpm_st_failed (test, "did not find battery device");
	}
	hal_gmanager_free_capability (value);

	g_object_unref (manager);
}

