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
#include "../src/gpm-webcam.h"

void
gpm_st_webcam (GpmSelfTest *test)
{
	GpmWebcam *webcam;
	gfloat brightness;
	gboolean ret;
	test->type = "GpmWebcam        ";

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null webcam");
	webcam = gpm_webcam_new ();
	if (webcam != NULL) {
		gpm_st_success (test, "got GpmWebcam");
	} else {
		gpm_st_failed (test, "could not get GpmWebcam");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a valid sample");
	ret = gpm_webcam_get_brightness (webcam, &brightness);
	if (ret == TRUE) {
		gpm_st_success (test, "got sample %f", brightness);
	} else {
		gpm_st_failed (test, "did not get sample");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get valid data");
	if (brightness > 0 && brightness < 1) {
		gpm_st_success (test, "got correct sample %f", brightness);
	} else {
		gpm_st_failed (test, "did not get correct sample");
	}

#if 0
	do {
		/************************************************************/
		gpm_st_title (test, "make sure we get a valid sample");
		ret = gpm_webcam_get_brightness (webcam, &brightness);
		if (ret == TRUE) {
			gpm_st_success (test, "got sample %f", brightness);
		} else {
			gpm_st_failed (test, "did not get sample");
		}
	} while (TRUE);
#endif
	g_object_unref (webcam);
}

