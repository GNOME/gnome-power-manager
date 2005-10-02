/*! @file	gpm-idle.c
 *  @brief	Idle calculation routines
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *
 * This module handles all the idle and load checking for g-p-m. It
 * uses the functionality of gnome-screensaver, and it's own load
 * calculator to call a user specified callback on user idle.
 *
 *  @note	gpm_idle_set_callback (x) and gpm_idle_set_timeout (x)
 *		must be called before a g_timeout is added to the main loop.
 *    		e.g. g_timeout_add (POLL_FREQ * 1000, gpm_idle_update, NULL);
 */
/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <sys/time.h>

#include <glib.h>
#include "gpm-idle.h"
#include "gpm-screensaver.h"

/* global in this file */
cpudata		old;
gboolean	init;
double		loadpercentage;
gint		time_idle_callback = 0;
IdleCallback	callbackfunction;

/** Gets the raw CPU values from /proc/stat
 *
 *  @param	data		An empty, pre-allocated CPU object
 *  @return			If we can read the /proc/stat file
 *
 * @note	user	- Time spent in user space (for all processes)
 * 		nice	- Time spent in niced tasks (tasks with +ve nice value)
 * 		system	- Time spent in Kernel space
 * 		idle	- Time the processor was not busy. 
 */
static gboolean
cpudata_get_values (cpudata *data)
{
	char tmp[5];
	char str[80];
	FILE *fd;

	/* assertion checks */
	g_assert (data);

	fd = fopen("/proc/stat", "r");
	if (!fd)
		return FALSE;
	fgets(str, 80, fd);
	sscanf(str, "%s %lu %lu %lu %lu", tmp, &data->user, &data->nice, &data->system, &data->idle);
	fclose(fd);
	/* 
	 * Summing up all these times gives you the system uptime in jiffies.
	 * This is what the uptime command does.
	 */
	data->total = data->user + data->nice + data->system + data->idle;
	return TRUE;
}

/** Diff two CPU structures
 *
 *  @param	data1		The newer CPU data struct
 *  @param	data2		The older CPU data struct
 *  @param	diff		The difference CPU data struct
 */
static void
cpudata_diff (cpudata *data2, cpudata *data1, cpudata *diff)
{
	/* assertion checks */
	g_assert (data1);
	g_assert (data2);
	g_assert (diff);

	diff->user = data1->user - data2->user;
	diff->nice = data1->nice - data2->nice;
	diff->system = data1->system - data2->system;
	diff->idle = data1->idle - data2->idle;
	diff->total = data1->total - data2->total;
}

/** Diff two CPU structures
 *
 *  @param	to		The new CPU data struct
 *  @param	from		The old CPU data struct
 */
static void
cpudata_copy (cpudata *to, cpudata *from)
{
	/* assertion checks */
	g_assert (to);
	g_assert (from);

	to->user = from->user;
	to->nice = from->nice;
	to->system = from->system;
	to->idle = from->idle;
	to->total = from->total;
}

#if 0
/** Normalise a cpudata structure to Hz.
 *
 *  @param	data		The CPU data struct
 *
 *  @note	This precision is required because 100 / POLL_FREQUENCY may
 *		be non-integer.
 * 		Also, we cannot assume 100 / POLL_FREQUENCY == total, as we
 *		may have taken longer to do the read than we had planned.
 */
static void
cpudata_normalize (cpudata *data)
{
	/* assertion checks */
	g_assert (data);

	double factor = 100.0 / (double) data->total;
	data->user = (double) data->user * factor;
	data->nice = (double) data->nice * factor;
	data->system = (double) data->system * factor;
	data->idle = (double) data->idle * factor;
}
#endif

/** Returns the CPU's load
 *
 *  @return			The CPU load
 */
static gint
cpu_update_data (void)
{
	cpudata new;
	cpudata diff;

	/* fill "old" value manually */
	if (!init) {
		init = TRUE;
		cpudata_get_values (&old);
		return 0;
	}

	/* work out the differences */
	cpudata_get_values (&new);
	cpudata_diff (&old, &new, &diff);

	/* Copy into old */
	cpudata_copy (&old, &new);

	/* If we divide the total time by idle time we get the load. */
	if (diff.idle > 0)
		loadpercentage = (double) diff.total / (double) diff.idle;
	else
		loadpercentage = 100;
	return loadpercentage;
}

/** Sets the idle timeout
 *
 *  @param	timeout		The idle timeout to set before calling our
 *				assigned Callback function.
 *  @return			If timeout was valid
 */
gboolean
gpm_idle_set_timeout (gint timeout)
{
	if (timeout <= 0 || timeout > 10 * 60 * 60) {
		g_warning ("gpm_idle_set_timeout was called with value = %i", timeout);
		return FALSE;
	}
	g_debug ("gpm_idle_set_timeout = %i", timeout);
	time_idle_callback = timeout;
	return TRUE;
}

/** Sets the idle callback
 *
 *  @param	callback	The assigned Callback function.
 *  @return			Success.
 */
gboolean
gpm_idle_set_callback (IdleCallback callback)
{
	callbackfunction = callback;
	return TRUE;
}

/** Find out if we should call out callback.
 *
 *  @param	data		Unused
 *  @return			If we should continue to poll
 */
gboolean
gpm_idle_update (gpointer data)
{
	gint gstime = 0;
	gint load;

	if (!callbackfunction) {
		g_warning ("gpm_idle_set_callback has not been called so no function set!");
		/* will stop polling */
		return FALSE;
	}

	if (time_idle_callback == 0) {
		g_debug ("gpm_idle_set_timeout has not been called so no idle processing done");
		/*
		 * Will not stop polling, as user could increase value by changing
		 * g-conf value using g-p-p
		 */
		return TRUE;
	}

	/* get our computed load value */
	load = cpu_update_data ();

	/* get idle time from g-s */
	if (!gscreensaver_get_idle (&gstime)) {
		g_warning ("getIdleTime polling disabled");
		return FALSE; /* will stop polling */
	}
	g_debug ("gpm_idle_update: gstime = %i, load = %i", gstime, load);

	/*
	 * The load has to be less than IDLE_LIMIT so we do not suspend or 
	 * shutdown halfway thru a heavy CPU utility, e.g. disk update, 
	 * rpm transaction, or kernel compile.
	 */
	if (gstime > time_idle_callback && load < IDLE_LIMIT) {
		g_debug ("running callback function");
		callbackfunction (gstime);
	}
	return TRUE;
}
