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
	guint value;
	gfloat fvalue;

	if (gpm_st_start (test, "GpmCommon", CLASS_AUTO) == FALSE) {
		return;
	}

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

	/************************************************************/
	gpm_st_title (test, "limit precision down 0,10");
	value = gpm_precision_round_down (0, 10);
	if (value == 0) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 4,10");
	value = gpm_precision_round_down (4, 10);
	if (value == 0) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 11,10");
	value = gpm_precision_round_down (11, 10);
	if (value == 10) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 201,2");
	value = gpm_precision_round_down (201, 2);
	if (value == 200) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision down 100,10");
	value = gpm_precision_round_down (100, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 0,10");
	value = gpm_precision_round_up (0, 10);
	if (value == 10) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 4,10");
	value = gpm_precision_round_up (4, 10);
	if (value == 10) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 11,10");
	value = gpm_precision_round_up (11, 10);
	if (value == 20) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 201,2");
	value = gpm_precision_round_up (201, 2);
	if (value == 202) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "limit precision up 100,10");
	value = gpm_precision_round_up (100, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "precision incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 0/10 levels");
	value = gpm_discrete_to_percent (0, 10);
	if (value == 0) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "conversion incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 9/10 levels");
	value = gpm_discrete_to_percent (9, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "conversion incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 20/10 levels");
	value = gpm_discrete_to_percent (20, 10);
	if (value == 100) {
		gpm_st_success (test, "got %i", value);
	} else {
		gpm_st_failed (test, "conversion incorrect (%i)", value);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 0/10 levels");
	fvalue = gpm_discrete_to_fraction (0, 10);
	if (fvalue > -0.01 && fvalue < 0.01) {
		gpm_st_success (test, "got %f", fvalue);
	} else {
		gpm_st_failed (test, "conversion incorrect (%f)", fvalue);
	}

	/************************************************************/
	gpm_st_title (test, "convert discrete 9/10 levels");
	fvalue = gpm_discrete_to_fraction (9, 10);
	if (fvalue > -1.01 && fvalue < 1.01) {
		gpm_st_success (test, "got %f", fvalue);
	} else {
		gpm_st_failed (test, "conversion incorrect (%f)", fvalue);
	}

	gpm_st_end (test);
}

