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

#include <libhal-gpower.h>

void
gpm_st_hal_power (GpmSelfTest *test)
{
	HalGPower *power;
	gboolean ret;
	test->type = "HalGPower        ";

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null device");
	power = hal_gpower_new ();
	if (power != NULL) {
		gpm_st_success (test, "got HalGPower");
	} else {
		gpm_st_failed (test, "could not get HalGPower");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we have pm support");
	ret = hal_gpower_has_support (power);
	if (ret == TRUE) {
		gpm_st_success (test, "has pm support");
	} else {
		gpm_st_failed (test, "does not have pm support");
	}

	g_object_unref (power);
}

