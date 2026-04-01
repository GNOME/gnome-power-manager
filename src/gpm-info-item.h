/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#pragma once

#include <gdk/gdk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_INFO_ITEM_TYPE (gpm_info_item_get_type ())

G_DECLARE_FINAL_TYPE (GpmInfoItem, gpm_info_item, GPM, INFO_ITEM, GObject)

GpmInfoItem *
gpm_info_item_new (const gchar *attribute, const gchar *value);

const gchar *
gpm_info_item_get_attribute (GpmInfoItem *item);
const gchar *
gpm_info_item_get_value (GpmInfoItem *item);

G_END_DECLS
