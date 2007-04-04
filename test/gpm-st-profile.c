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

#include "../src/gpm-profile.h"

void
gpm_st_profile (GpmSelfTest *test)
{
	test->type = "GpmProfile       ";
	GpmProfile *profile;
	gboolean ret;
	guint i;
	guint value;

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null profile");
	profile = gpm_profile_new ();
	if (profile != NULL) {
		gpm_st_success (test, "got GpmProfile");
	} else {
		gpm_st_failed (test, "could not get GpmProfile");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a zero accuracy when non-set");
	value = gpm_profile_get_accuracy (profile, 50);
	if (value == 0) {
		gpm_st_success (test, "got zero");
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a zero time when non-set");
	value = gpm_profile_get_time (profile, 50, TRUE);
	if (value == 0) {
		gpm_st_success (test, "got zero");
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "set config id");
	ret = gpm_profile_set_config_id (profile, "test123");
	if (ret == TRUE) {
		gpm_st_success (test, "set type");
	} else {
		gpm_st_failed (test, "could not set type");
	}

	/************************************************************/
	gpm_st_title (test, "delete old charging data");
	ret = gpm_profile_delete_data (profile, FALSE);
	if (ret == TRUE) {
		gpm_st_success (test, "deleted");
	} else {
		gpm_st_failed (test, "could not delete");
	}

	/************************************************************/
	gpm_st_title (test, "delete old discharging data");
	ret = gpm_profile_delete_data (profile, TRUE);
	if (ret == TRUE) {
		gpm_st_success (test, "deleted");
	} else {
		gpm_st_failed (test, "could not delete");
	}
g_error ("kkl");
	/************************************************************/
	gpm_st_title (test, "set config id (should create file)");
	ret = gpm_profile_set_config_id (profile, "test123");
	if (ret == TRUE) {
		gpm_st_success (test, "set type");
	} else {
		gpm_st_failed (test, "could not set type");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a zero accuracy with a new dataset");
	value = gpm_profile_get_accuracy (profile, 50);
	if (value == 0) {
		gpm_st_success (test, "got non zero");
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non-zero time when non-set");
	value = gpm_profile_get_time (profile, 50, TRUE);
	if (value != 0) {
		gpm_st_success (test, "got non zero");
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "register discharging");
	ret = gpm_profile_register_charging (profile, FALSE);
	if (ret == TRUE) {
		gpm_st_success (test, "set discharging");
	} else {
		gpm_st_failed (test, "could not set discharging");
	}

	/************************************************************/
	gpm_st_title (test, "ignore first point");
	ret = gpm_profile_register_percentage (profile, 99);
	if (ret == FALSE) {
		gpm_st_success (test, "ignored first");
	} else {
		gpm_st_failed (test, "ignored second");
	}

	/************************************************************/
	gpm_st_title (test, "make up discharging dataset");
	ret = TRUE;
	for (i=98; i>0; i--) {
		g_usleep (200*1000);
		if (gpm_profile_register_percentage (profile, i) == FALSE) {
			ret = FALSE;
			g_print ("FAILED (%i),", i);
		} else {
			g_print (".");
		}
	}
	if (ret == TRUE) {
		gpm_st_success (test, "put dataset");
	} else {
		gpm_st_failed (test, "could not put dataset");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non-zero time when set");
	value = gpm_profile_get_time (profile, 50, TRUE);
	if (value != 0) {
		gpm_st_success (test, "got non zero %i", value);
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non-zero accuracy when a complete dataset");
	value = gpm_profile_get_accuracy (profile, 50);
	if (value != 0) {
		gpm_st_success (test, "got non zero %i", value);
	} else {
		gpm_st_failed (test, "got %i", value);
	}

	g_object_unref (profile);
}

