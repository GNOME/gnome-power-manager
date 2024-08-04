/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2024 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GPM_TYPE_ROTATED_WIDGET (gpm_rotated_widget_get_type())

G_DECLARE_FINAL_TYPE (GpmRotatedWidget, gpm_rotated_widget, GPM, ROTATED_WIDGET, GtkWidget)

G_END_DECLS
