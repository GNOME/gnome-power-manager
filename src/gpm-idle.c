/***************************************************************************
 *
 * gpm-idle.c : Idle calculation routines
 *
 * Copyright (C) 2005 Richard Hughes, <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

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

/* 
 * The values are:
 * user    -  Time spent in user space (for all processes)
 * nice    -  Time spent in niced tasks (tasks with positive nice value)
 * system  -  Time spent in Kernel space
 * idle    -  Time the processor was not busy with any running process. 
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

/*
 * Diff two CPU structures
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

/*
 * Copy a CPU structures
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
/*
 * Normalise a cpudata structure to Hz.
 * This precision is required because 100 / POLL_FREQUENCY may be non-integer.
 * Also, we cannot assume 100 / POLL_FREQUENCY == total, as we may have taken
 * longer to do the read than we had planned.
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

gboolean
gpm_idle_set_callback (IdleCallback callback)
{
	callbackfunction = callback;
	return TRUE;
}

gboolean
gpm_idle_update (gpointer data)
{
	gint gstime = 0;
	gint load;

	if (!callbackfunction) {
		g_warning ("gpm_idle_set_callback has not been called so no function set!");
		return FALSE; /* will stop polling */
	}

	if (time_idle_callback == 0) {
		g_warning ("gpm_idle_set_timeout has not been called so no timeout set!");
		return FALSE; /* will stop polling */
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
