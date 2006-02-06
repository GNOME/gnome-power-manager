/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_SCREENSAVER_H
#define __GPM_SCREENSAVER_H

#include <glib.h>

G_BEGIN_DECLS

#define GS_PREF_DIR		"/apps/gnome-screensaver"
#define GS_PREF_LOCK_ENABLED	GS_PREF_DIR "/lock_enabled"

gboolean gpm_screensaver_lock (void);
gboolean gpm_screensaver_lock_set (gboolean lock);
gboolean gpm_screensaver_lock_enabled (void);
gboolean gpm_screensaver_enable_throttle (gboolean enable);
gboolean gpm_screensaver_is_running (void);
gboolean gpm_screensaver_poke (void);
gboolean gpm_screensaver_get_idle (gint *time);

G_END_DECLS

#endif	/* _GPMSCREENSAVER_H */
