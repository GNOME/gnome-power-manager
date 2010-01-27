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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib.h>

typedef struct
{
	guint		 total;
	guint		 succeeded;
	gboolean	 started;
	gchar		*type;
} EggTest;

typedef void (*EggTestFunc) (EggTest *test);

gboolean egg_test_start (EggTest *test, const gchar *name);
void egg_test_end (EggTest *test);
void egg_test_title (EggTest *test, const gchar *format, ...);
void egg_test_success (EggTest *test, const gchar *format, ...);
void egg_test_failed (EggTest *test, const gchar *format, ...);
void egg_test_warning (EggTest *test, const gchar *format, ...);

void gpm_common_test (EggTest *test);
void gpm_profile_test (EggTest *test);
void egg_test_webcam (EggTest *test);
void gpm_array_test (EggTest *test);
void gpm_idletime_test (EggTest *test);
void gpm_array_float_test (EggTest *test);
void gpm_cell_test (EggTest *test);
void gpm_cell_unit_test (EggTest *test);
void gpm_cell_test_array (EggTest *test);
void gpm_proxy_test (EggTest *test);
void gpm_phone_test (EggTest *test);
void gpm_inhibit_test (EggTest *test);
void gpm_device_test (EggTest *test);
void gpm_device_teststore (EggTest *test);
void gpm_hal_device_power_test (EggTest *test);
void gpm_hal_manager_test (EggTest *test);
void gpm_graph_widget_test (EggTest *test);

