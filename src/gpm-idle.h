/** @file	gpm-idle.h
 *  @brief	Idle calculation routines
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
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
/**
 * @addtogroup	idle
 * @{
 */

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

/** The cached cpu statistics used to work out the difference
 *
 */
typedef struct {
	long unsigned user;	/**< The CPU user time			*/
	long unsigned nice;	/**< The CPU nice time			*/
	long unsigned system;	/**< The CPU system time		*/
	long unsigned idle;	/**< The CPU idle time			*/
	long unsigned total;	/**< The CPU total time (uptime)	*/
} cpudata;

typedef void (*IdleCallback) (const gint timeout);

gboolean gpm_idle_set_callback (IdleCallback callback);
gboolean gpm_idle_update (gpointer data);
gboolean gpm_idle_set_timeout (gint timeout);

#endif	/* _GPMIDLE_H */
/** @} */
