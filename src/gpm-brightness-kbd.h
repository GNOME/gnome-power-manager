/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2006 David Zeuthen <davidz@redhat.com>
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

#ifndef __GPM_BRIGHTNESS_KBD_H
#define __GPM_BRIGHTNESS_KBD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_BRIGHTNESS_KBD		(gpm_brightness_kbd_get_type ())
#define GPM_BRIGHTNESS_KBD(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_BRIGHTNESS_KBD, GpmBrightnessKbd))
#define GPM_BRIGHTNESS_KBD_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_BRIGHTNESS_KBD, GpmBrightnessKbdClass))
#define GPM_IS_BRIGHTNESS_KBD(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_BRIGHTNESS_KBD))
#define GPM_IS_BRIGHTNESS_KBD_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_BRIGHTNESS_KBD))
#define GPM_BRIGHTNESS_KBD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_BRIGHTNESS_KBD, GpmBrightnessKbdClass))

typedef struct GpmBrightnessKbdPrivate GpmBrightnessKbdPrivate;

typedef struct
{
	GObject		         parent;
	GpmBrightnessKbdPrivate *priv;
} GpmBrightnessKbd;

typedef struct
{
	GObjectClass	parent_class;
	void		(* brightness_changed)	(GpmBrightnessKbd	*brightness,
						 guint			 percentage);
} GpmBrightnessKbdClass;

GType		 gpm_brightness_kbd_get_type	(void);
GpmBrightnessKbd *gpm_brightness_kbd_new	(void);
gboolean	 gpm_brightness_kbd_has_hw	(void);

gboolean	 gpm_brightness_kbd_up		(GpmBrightnessKbd	*brightness);
gboolean	 gpm_brightness_kbd_down	(GpmBrightnessKbd	*brightness);
gboolean	 gpm_brightness_kbd_get		(GpmBrightnessKbd	*brightness,
						 guint			*brightness_level);
gboolean	 gpm_brightness_kbd_set_dim	(GpmBrightnessKbd	*brightness,
						 guint			 brightness_level);
gboolean	 gpm_brightness_kbd_set_std	(GpmBrightnessKbd	*brightness,
						 guint			 brightness_level);
gboolean	 gpm_brightness_kbd_dim		(GpmBrightnessKbd	*brightness);
gboolean	 gpm_brightness_kbd_undim	(GpmBrightnessKbd	*brightness);
gboolean	 gpm_brightness_kbd_toggle	(GpmBrightnessKbd	*brightness);

G_END_DECLS

#endif /* __GPM_BRIGHTNESS_KBD_H */
