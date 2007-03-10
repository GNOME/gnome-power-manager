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

#include "../src/gpm-inhibit.h"
#include "../src/gpm-powermanager.h"

void
gpm_st_inhibit (GpmSelfTest *test)
{
	gboolean ret;
	gboolean valid;
	guint cookie1 = 0;
	guint cookie2 = 0;
	GpmPowermanager *powermanager;

	test->type = "GpmInhibit       ";

	powermanager = gpm_powermanager_new ();
	if (powermanager == NULL) {
		g_warning ("Unable to get connection to power manager");
		return;
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are not inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == FALSE) {
		gpm_st_failed (test, "Already inhibited");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "clear an invalid cookie");
	ret = gpm_powermanager_uninhibit (powermanager, 123456);
	if (ret == FALSE) {
		gpm_st_success (test, "invalid cookie failed as expected");
	} else {
		gpm_st_failed (test, "should have rejected invalid cookie");
	}

	/************************************************************/
	gpm_st_title (test, "get auto cookie 1");
	ret = gpm_powermanager_inhibit_auto (powermanager,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie1);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to inhibit");
	} else if (cookie1 == 0) {
		gpm_st_failed (test, "Cookie invalid (cookie: %u)", cookie1);
	} else {
		gpm_st_success (test, "cookie: %u", cookie1);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are auto inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == FALSE) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are not manual inhibited");
	ret = gpm_powermanager_is_valid (powermanager, TRUE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == TRUE) {
		gpm_st_success (test, "not inhibited for manual action");
	} else {
		gpm_st_failed (test, "inhibited auto but manual not valid");
	}

	/************************************************************/
	gpm_st_title (test, "get cookie 2");
	ret = gpm_powermanager_inhibit_auto (powermanager,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie2);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to inhibit");
	} else if (cookie2 == 0) {
		gpm_st_failed (test, "Cookie invalid (cookie: %u)", cookie2);
	} else {
		gpm_st_success (test, "cookie: %u", cookie2);
	}

	/************************************************************/
	gpm_st_title (test, "clear cookie 1");
	ret = gpm_powermanager_uninhibit (powermanager, cookie1);
	if (ret == FALSE) {
		gpm_st_failed (test, "cookie failed to clear");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are still inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == FALSE) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "clear cookie 2");
	ret = gpm_powermanager_uninhibit (powermanager, cookie2);
	if (ret == FALSE) {
		gpm_st_failed (test, "cookie failed to clear");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are not auto inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == FALSE) {
		gpm_st_failed (test, "Auto inhibited when we shouldn't be");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are not manual inhibited");
	ret = gpm_powermanager_is_valid (powermanager, TRUE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == FALSE) {
		gpm_st_failed (test, "Manual inhibited when we shouldn't be");
	} else {
		gpm_st_success (test, NULL);
	}

	/************************************************************/
	gpm_st_title (test, "get manual cookie 1");
	ret = gpm_powermanager_inhibit_manual (powermanager,
				  "gnome-power-self-test",
				  "test inhibit",
				  &cookie1);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to manual inhibit");
	} else if (cookie1 == 0) {
		gpm_st_failed (test, "Cookie invalid (cookie: %u)", cookie1);
	} else {
		gpm_st_success (test, "cookie: %u", cookie1);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are auto inhibited");
	ret = gpm_powermanager_is_valid (powermanager, FALSE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == FALSE) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "make sure we are manual inhibited");
	ret = gpm_powermanager_is_valid (powermanager, TRUE, &valid);
	if (ret == FALSE) {
		gpm_st_failed (test, "Unable to test validity");
	} else if (valid == FALSE) {
		gpm_st_success (test, "inhibited");
	} else {
		gpm_st_failed (test, "inhibit failed");
	}

	/************************************************************/
	gpm_st_title (test, "clear cookie 1");
	ret = gpm_powermanager_uninhibit (powermanager, cookie1);
	if (ret == FALSE) {
		gpm_st_failed (test, "cookie failed to clear");
	} else {
		gpm_st_success (test, NULL);
	}
	g_object_unref (powermanager);
}

