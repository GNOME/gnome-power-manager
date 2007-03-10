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

#include "../src/gpm-common.h"

void
gpm_st_common (GpmSelfTest *test)
{
	guint8 r, g, b;
	guint32 colour;
	test->type = "GpmCommon        ";

	/************************************************************/
	gpm_st_title (test, "get red");
	gpm_colour_to_rgb (0xff0000, &r, &g, &b);
	if (r == 255 && g == 0 && b == 0) {
		gpm_st_success (test, "got red");
	} else {
		gpm_st_failed (test, "could not get red (%i, %i, %i)", r, g, b);
	}

	/************************************************************/
	gpm_st_title (test, "get green");
	gpm_colour_to_rgb (0x00ff00, &r, &g, &b);
	if (r == 0 && g == 255 && b == 0) {
		gpm_st_success (test, "got green");
	} else {
		gpm_st_failed (test, "could not get green (%i, %i, %i)", r, g, b);
	}

	/************************************************************/
	gpm_st_title (test, "get blue");
	gpm_colour_to_rgb (0x0000ff, &r, &g, &b);
	if (r == 0 && g == 0 && b == 255) {
		gpm_st_success (test, "got blue");
	} else {
		gpm_st_failed (test, "could not get blue (%i, %i, %i)", r, g, b);
	}

	/************************************************************/
	gpm_st_title (test, "get black");
	gpm_colour_to_rgb (0x000000, &r, &g, &b);
	if (r == 0 && g == 0 && b == 0) {
		gpm_st_success (test, "got black");
	} else {
		gpm_st_failed (test, "could not get black (%i, %i, %i)", r, g, b);
	}

	/************************************************************/
	gpm_st_title (test, "get white");
	gpm_colour_to_rgb (0xffffff, &r, &g, &b);
	if (r == 255 && g == 255 && b == 255) {
		gpm_st_success (test, "got white");
	} else {
		gpm_st_failed (test, "could not get white (%i, %i, %i)", r, g, b);
	}

	/************************************************************/
	gpm_st_title (test, "set red");
	colour = gpm_rgb_to_colour (0xff, 0x00, 0x00);
	if (colour == 0xff0000) {
		gpm_st_success (test, "set red");
	} else {
		gpm_st_failed (test, "could not set red (%i)", colour);
	}

	/************************************************************/
	gpm_st_title (test, "set green");
	colour = gpm_rgb_to_colour (0x00, 0xff, 0x00);
	if (colour == 0x00ff00) {
		gpm_st_success (test, "set green");
	} else {
		gpm_st_failed (test, "could not set green (%i)", colour);
	}

	/************************************************************/
	gpm_st_title (test, "set blue");
	colour = gpm_rgb_to_colour (0x00, 0x00, 0xff);
	if (colour == 0x0000ff) {
		gpm_st_success (test, "set blue");
	} else {
		gpm_st_failed (test, "could not set blue (%i)", colour);
	}

	/************************************************************/
	gpm_st_title (test, "set white");
	colour = gpm_rgb_to_colour (0xff, 0xff, 0xff);
	if (colour == 0xffffff) {
		gpm_st_success (test, "set white");
	} else {
		gpm_st_failed (test, "could not set white (%i)", colour);
	}
}

