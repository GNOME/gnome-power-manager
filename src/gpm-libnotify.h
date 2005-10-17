/** @file	gpm-libnotify.h
 *  @brief	LibNotify shared code and fallback code
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 *  @note	This code can still be used without libnotify compiled in,
 *    		as it fall backs to a standard modal messsagebox.
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
 * @addtogroup	libnotify
 * @{
 */

#ifndef _GPMLIBNOTIFY_H
#define _GPMLIBNOTIFY_H

/** Set the timeout of the libnotify notifications */
#define NOTIFY_TIMEOUT			5


/** The libnotify urgency type */
typedef enum {
	LIBNOTIFY_URGENCY_CRITICAL = 1,	/**< Critical warning!	*/
	LIBNOTIFY_URGENCY_NORMAL = 2,	/**< Normal message	*/
	LIBNOTIFY_URGENCY_LOW = 3	/**< Low urgency	*/
} LibNotifyEventType;

gboolean libnotify_init (const gchar *nicename);
gboolean libnotify_event (const gchar *content, const LibNotifyEventType urgency, GtkWidget *point);

#endif	/* _GPMLIBNOTIFY_H */
/** @} */
