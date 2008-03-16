/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_SOUND_H
#define __GPM_SOUND_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_SOUND		(gpm_sound_get_type ())
#define GPM_SOUND(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_SOUND, GpmSound))
#define GPM_SOUND_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_SOUND, GpmSoundClass))
#define GPM_IS_SOUND(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_SOUND))
#define GPM_IS_SOUND_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_SOUND))
#define GPM_SOUND_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_SOUND, GpmSoundClass))

typedef enum {
	GPM_SOUND_AC_UNPLUGGED,
	GPM_SOUND_POWER_LOW,
	GPM_SOUND_SUSPEND_FAILURE,
	GPM_SOUND_LAST
} GpmSoundAction;

typedef struct GpmSoundPrivate GpmSoundPrivate;

typedef struct
{
	GObject		      parent;
	GpmSoundPrivate *priv;
} GpmSound;

typedef struct
{
	GObjectClass	parent_class;
} GpmSoundClass;

GType		 gpm_sound_get_type		(void);
GpmSound	*gpm_sound_new			(void);

gboolean	 gpm_sound_force		(GpmSound	*sound,
						 GpmSoundAction	 action);
gboolean	 gpm_sound_event		(GpmSound	*sound,
						 GpmSoundAction	 action);
void		 gpm_sound_beep			(void);

G_END_DECLS

#endif /* __GPM_SOUND_H */
