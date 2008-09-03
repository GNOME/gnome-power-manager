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

typedef struct
{
	guint		 total;
	guint		 succeeded;
	gboolean	 started;
	gchar		*type;
} GpmSelfTest;

typedef void (*GpmSelfTestFunc) (GpmSelfTest *test);

gboolean gpm_st_start (GpmSelfTest *test, const gchar *name);
void gpm_st_end (GpmSelfTest *test);
void gpm_st_title (GpmSelfTest *test, const gchar *format, ...);
void gpm_st_success (GpmSelfTest *test, const gchar *format, ...);
void gpm_st_failed (GpmSelfTest *test, const gchar *format, ...);
void gpm_st_warning (GpmSelfTest *test, const gchar *format, ...);

void gpm_st_common (GpmSelfTest *test);
void gpm_st_profile (GpmSelfTest *test);
void gpm_st_webcam (GpmSelfTest *test);
void gpm_st_array (GpmSelfTest *test);
void gpm_st_idletime (GpmSelfTest *test);
void gpm_st_array_float (GpmSelfTest *test);
void gpm_st_cell (GpmSelfTest *test);
void gpm_st_cell_unit (GpmSelfTest *test);
void gpm_st_cell_array (GpmSelfTest *test);
void gpm_st_proxy (GpmSelfTest *test);
void gpm_st_phone (GpmSelfTest *test);
void gpm_st_inhibit (GpmSelfTest *test);
void gpm_st_hal_device (GpmSelfTest *test);
void gpm_st_hal_devicestore (GpmSelfTest *test);
void gpm_st_hal_power (GpmSelfTest *test);
void gpm_st_hal_manager (GpmSelfTest *test);
void gpm_st_graph_widget (GpmSelfTest *test);

