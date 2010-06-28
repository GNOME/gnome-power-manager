/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __EGG_ARRAY_FLOAT_H
#define __EGG_ARRAY_FLOAT_H

#include <glib.h>

G_BEGIN_DECLS

/* at the moment just use a GArray as it's quick */
typedef GArray GpmArrayFloat;

GpmArrayFloat	*gpm_array_float_new			(guint		 length);
void		 gpm_array_float_free			(GpmArrayFloat	*array);
gfloat		 gpm_array_float_sum			(GpmArrayFloat	*array);
GpmArrayFloat	*gpm_array_float_compute_gaussian	(guint		 length,
							 gfloat		 sigma);
gfloat		 gpm_array_float_compute_integral	(GpmArrayFloat	*array,
							 guint		 x1,
							 guint		 x2);
gfloat		 gpm_array_float_get_average		(GpmArrayFloat	*array);
gboolean	 gpm_array_float_print			(GpmArrayFloat	*array);
GpmArrayFloat	*gpm_array_float_convolve		(GpmArrayFloat	*data,
							 GpmArrayFloat	*kernel);
gfloat		 gpm_array_float_get			(GpmArrayFloat	*array,
							 guint		 i);
void		 gpm_array_float_set			(GpmArrayFloat	*array,
							 guint		 i,
							 gfloat		 value);
GpmArrayFloat	*gpm_array_float_remove_outliers	(GpmArrayFloat *data, guint length, gfloat sigma);
gfloat		 gpm_array_float_guassian_value		(gfloat		 x,
							 gfloat		 sigma);

G_END_DECLS

#endif /* __EGG_ARRAY_FLOAT_H */
