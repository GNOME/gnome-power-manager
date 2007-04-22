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

#include <glib.h>
#include "gpm-st-main.h"

#include "../src/gpm-array-float.h"
#include "../src/gpm-debug.h"

void
gpm_st_array_float (GpmSelfTest *test)
{
#if 0
	/************************************************************/
	gpm_st_title (test, "make sure we get a non null array");
	array = gpm_array_new ();
	if (array != NULL) {
		gpm_st_success (test, "got GpmArray");
	} else {
		gpm_st_failed (test, "could not get GpmArray");
	}
#endif


#if 0
	guint length_data;
	guint length_gaus;
	gint half_length;
	const float gaussian_width = 4.0;
	GpmArrayPoint *point;
	gfloat div;
	gfloat data[12];
	gfloat *dataptr;
	gfloat *dataptr_gaus;
	gfloat *dataptr_data;
	gfloat *dataptr_result;

	length_gaus = 7;

	length_data = gpm_array_get_size (array);

	GArray *array_kernel;
	GArray *array_data;
	GArray *array_result;

	array_kernel = gpm_array_float_compute_gaussian (length_gaus, 1.0);

	gpm_debug ("array gaus");
	gpm_array_float_print (array_kernel);

	array_data = gpm_array_float_convert (array);

	gpm_debug ("array data");
	gpm_array_float_print (array_data);

	array_result = gpm_array_float_convolve (array_data, array_kernel);

	gpm_debug ("array result");
	gpm_array_float_print (array_result);

	g_array_free (array_kernel, TRUE);
	g_array_free (array_data, TRUE);
	g_array_free (array_result, TRUE);
#endif

}

