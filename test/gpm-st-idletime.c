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

#include <libidletime.h>
#include <gdk/gdk.h>

static void
gpm_st_idletime_wait (guint time_ms)
{
	GTimer *ltimer = g_timer_new ();
	gfloat goal = time_ms / (gfloat) 1000.0f;
	do {
		g_main_context_iteration (NULL, FALSE);
	} while (g_timer_elapsed (ltimer, NULL) < goal);
	g_timer_destroy (ltimer);
}

static guint last_alarm = 0;
static guint event_time;
GTimer *timer;

static void
gpm_alarm_expired_cb (LibIdletime *idletime, guint alarm, gpointer data)
{
	last_alarm = alarm;
	event_time = g_timer_elapsed (timer, NULL) * (gfloat) 1000.0f;
//	g_print ("[evt %i in %ims]\n", alarm, event_time);
}

static void
wait_until_alarm (void)
{
	g_print ("*****************************\n");
	g_print ("*** DO NOT MOVE THE MOUSE ***\n");
	g_print ("*****************************\n");
	while (last_alarm == 0)
		g_main_context_iteration (NULL, FALSE);
}

static void
wait_until_reset (void)
{
	if (last_alarm == 0)
		return;
	g_print ("*****************************\n");
	g_print ("***     MOVE THE MOUSE    ***\n");
	g_print ("*****************************\n");
	while (last_alarm != 0)
		g_main_context_iteration (NULL, FALSE);
	gpm_st_idletime_wait (1000);
}

void
gpm_st_idletime (GpmSelfTest *test)
{
	LibIdletime *idletime;
	gboolean ret;
	guint alarm;
	guint i;

	if (gpm_st_start (test, "LibIdletime", CLASS_AUTO) == FALSE) {
		return;
	}

	timer = g_timer_new ();
	gdk_init (NULL, NULL);

	/* warn */

	g_timer_start (timer);
	/************************************************************/
	gpm_st_title (test, "check to see if delay works as expected");
	gpm_st_idletime_wait (2000);
	event_time = g_timer_elapsed (timer, NULL) * (gfloat) 1000.0f;
	if (event_time > 1800 && event_time < 2200) {
		gpm_st_success (test, "time %i~=%i", 2000, event_time);
	} else {
		gpm_st_failed (test, "time not the same! %i != %i", event_time, 2000);
	}

	/************************************************************/
	gpm_st_title (test, "make sure we get a non null device");
	idletime = idletime_new ();
	if (idletime != NULL) {
		gpm_st_success (test, "got LibIdletime");
	} else {
		gpm_st_failed (test, "could not get LibIdletime");
	}
	g_signal_connect (idletime, "alarm-expired",
			  G_CALLBACK (gpm_alarm_expired_cb), NULL);

	/************************************************************/
	gpm_st_title (test, "check if we are alarm zero with no alarms");
	alarm = idletime_alarm_get (idletime);
	if (alarm == 0) {
		gpm_st_success (test, NULL);
	} else {
		gpm_st_failed (test, "alarm %i set!", alarm);
	}

	/************************************************************/
	gpm_st_title (test, "check if we can set an reset alarm");
	ret = idletime_alarm_set (idletime, 0, 100);
	if (ret == FALSE) {
		gpm_st_success (test, "ignored reset alarm");
	} else {
		gpm_st_failed (test, "did not ignore reset alarm");
	}

	/************************************************************/
	gpm_st_title (test, "check if we can set an alarm timeout of zero");
	ret = idletime_alarm_set (idletime, 999, 0);
	if (ret == FALSE) {
		gpm_st_success (test, "ignored invalid alarm");
	} else {
		gpm_st_failed (test, "did not ignore invalid alarm");
	}

	/************************************************************/
	g_timer_start (timer);
	gpm_st_title (test, "check if we can set an alarm");
	ret = idletime_alarm_set (idletime, 101, 5000);
	if (ret == TRUE) {
		gpm_st_success (test, "set alarm okay");
	} else {
		gpm_st_failed (test, "could not set alarm");
	}

	idletime_alarm_set (idletime, 101, 5000);
	wait_until_alarm ();

	/* loop this two times */
	for (i=0; i<2; i++) {
		/* just let it time out, and wait for human input */
		wait_until_reset ();
		g_timer_start (timer);

		/************************************************************/
		g_timer_start (timer);
		gpm_st_title (test, "check if we can set an alarm");
		ret = idletime_alarm_set (idletime, 101, 5000);
		if (ret == TRUE) {
			gpm_st_success (test, "set alarm 5000ms okay");
		} else {
			gpm_st_failed (test, "could not set alarm 5000ms");
		}

		/* wait for alarm to go off */
		wait_until_alarm ();
		g_timer_start (timer);

		/************************************************************/
		gpm_st_title (test, "check if correct alarm has gone off");
		alarm = idletime_alarm_get (idletime);
		if (alarm == 101) {
			gpm_st_success (test, "correct alarm");
		} else {
			gpm_st_failed (test, "alarm %i set!", alarm);
		}

		/************************************************************/
		gpm_st_title (test, "check if alarm has gone off in correct time");
		alarm = idletime_alarm_get (idletime);
		if (event_time > 3000 && event_time < 6000) {
			gpm_st_success (test, "correct, timeout ideally %ims (we did after %ims)", 5000, event_time);
		} else {
			gpm_st_failed (test, "alarm %i did not timeout correctly !", alarm);
		}
	}

	/* just let it time out, and wait for human input */
	wait_until_reset ();
	g_timer_start (timer);

	/************************************************************/
	g_timer_start (timer);
	gpm_st_title (test, "check if we can set an existing alarm");
	ret = idletime_alarm_set (idletime, 101, 10000);
	if (ret == TRUE) {
		gpm_st_success (test, "set alarm 10000ms okay");
	} else {
		gpm_st_failed (test, "could not set alarm 10000ms");
	}

	/* wait for alarm to go off */
	wait_until_alarm ();
	g_timer_start (timer);

	/************************************************************/
	gpm_st_title (test, "check if alarm has gone off in the old time");
	alarm = idletime_alarm_get (idletime);
	if (event_time > 5000) {
		gpm_st_success (test, "last timeout value used");
	} else {
		gpm_st_failed (test, "incorrect timeout used %ims", event_time);
	}

	/************************************************************/
	gpm_st_title (test, "check if we can remove an invalid alarm");
	ret = idletime_alarm_free_id (idletime, 202);
	if (ret == FALSE) {
		gpm_st_success (test, "ignored invalid alarm");
	} else {
		gpm_st_failed (test, "removed invalid alarm");
	}

	/************************************************************/
	gpm_st_title (test, "check if we can remove an valid alarm");
	ret = idletime_alarm_free_id (idletime, 101);
	if (ret == TRUE) {
		gpm_st_success (test, "removed valid alarm");
	} else {
		gpm_st_failed (test, "failed to remove valid alarm");
	}

	g_timer_destroy (timer);
	g_object_unref (idletime);

	gpm_st_end (test);
}

