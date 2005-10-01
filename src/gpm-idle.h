/***************************************************************************
 *
 * gpm-idle.h : Idle calculation routines
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 **************************************************************************/

#ifndef _GPMIDLE_H
#define _GPMIDLE_H

/*
 * How many seconds between polling?
 */
#define POLL_FREQUENCY	5
/*
 * Sets the idle percent limit, i.e. how hard the computer can work 
 * while considered "at idle"
 */
#define IDLE_LIMIT	5

typedef struct {
	long unsigned user;
	long unsigned nice;
	long unsigned system;
	long unsigned idle;
	long unsigned total;
} cpudata;

typedef void (*IdleCallback) (const gint timeout);

gboolean gpm_idle_set_callback (IdleCallback callback);
gboolean gpm_idle_update (gpointer data);
gboolean gpm_idle_set_timeout (gint timeout);

#endif	/* _GPMIDLE_H */
