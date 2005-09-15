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

#define POLL_FREQUENCY	2

typedef struct {
	long unsigned user;
	long unsigned nice;
	long unsigned system;
	long unsigned idle;
	long unsigned total;
} cpudata;

gboolean update_idle_function (gpointer data);

gboolean set_cpu_idle_limit (const int percentage);
gboolean get_is_cpu_idle ();

#endif	/* _GPMIDLE_H */
