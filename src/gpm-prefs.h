/*! @file	gpm-prefs.h
 *  @brief	GNOME Power Preferences
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

#ifndef _GPMPREFS_H
#define _GPMPREFS_H

/** The policy tipe, required because the sliders all do different things
 *
 */
typedef enum {
	POLICY_PERCENT,	/**< Policy is of type percent 0..100	*/
	POLICY_LCD,	/**< Policy is of type LCD 0..x		*/
	POLICY_TIME	/**< Policy is of type time 0..120	*/
} PolicyType;

#endif	/* _GPMPREFS_H */
