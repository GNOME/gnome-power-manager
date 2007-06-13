/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_ARRAY_FLOAT_H
#define __GPM_ARRAY_FLOAT_H

#include <glib.h>
#include "gpm-array.h"

G_BEGIN_DECLS

GArray		*gpm_array_float_new			(guint		 length);
void		 gpm_array_float_free			(GArray		*array);
gfloat		 gpm_array_float_guassian_value		(gfloat		 x,
							 gfloat		 sigma);
gfloat		 gpm_array_float_sum			(GArray		*array);
gboolean	 gpm_array_float_from_array_y		(GArray		*arrayfloat,
							 GpmArray	*array);
gboolean	 gpm_array_float_from_array_z		(GArray		*arrayfloat,
							 GpmArray	*array);
gboolean	 gpm_array_float_to_array_y		(GArray		*arrayfloat,
							 GpmArray	*array);
gboolean	 gpm_array_float_to_array_z		(GArray		*arrayfloat,
							 GpmArray	*array);
GArray		*gpm_array_float_compute_gaussian	(guint		 length,
							 gfloat		 sigma);
gfloat		 gpm_array_float_compute_integral	(GArray		*array,
							 guint		 x1,
							 guint		 x2);
gfloat		 gpm_array_float_get_average		(GArray		*array);
gboolean	 gpm_array_float_print			(GArray		*array);
GArray		*gpm_array_float_convolve		(GArray		*data,
							 GArray		*kernel);
inline gfloat	 gpm_array_float_get			(GArray		*array,
							 guint		 i);
inline void	 gpm_array_float_set			(GArray		*array,
							 guint		 i,
							 gfloat		 value);

G_END_DECLS

#endif /* __GPM_ARRAY_FLOAT_H */
