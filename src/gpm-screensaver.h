/** @file	gpm-screensaver.h
 *  @brief	Functions to query and control GNOME Screensaver
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
 * @addtogroup	gs
 * @{
 */

#ifndef _GPMSCREENSAVER_H
#define _GPMSCREENSAVER_H

#define GS_LISTENER_SERVICE	"org.gnome.screensaver"
#define GS_LISTENER_PATH	"/org/gnome/screensaver"
#define GS_LISTENER_INTERFACE	"org.gnome.screensaver"

#define GS_GCONF_ROOT		"/apps/gnome-screensaver/"
#define GS_GCONF_ROOT_NO_SLASH	"/apps/gnome-screensaver"

gboolean gpm_screensaver_lock (void);
gboolean gpm_screensaver_lock_check (void);
gboolean gpm_screensaver_set_throttle (gboolean throttle);
gboolean gpm_screensaver_is_running (void);
gboolean gpm_screensaver_poke (void);
gboolean gpm_screensaver_get_idle (gint *time);
gboolean gpm_screensaver_set_dpms_timeout (gint timeout);
gboolean gpm_screensaver_set_dpms (gboolean state);

#endif	/* _GPMSCREENSAVER_H */
/** @} */
