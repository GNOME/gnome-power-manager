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

#define GPM_TYPE_DEVICE_ITEM (gpm_device_item_get_type ())

G_DECLARE_FINAL_TYPE (GpmDeviceItem, gpm_device_item, GPM, DEVICE_ITEM, GObject)

GpmDeviceItem *
gpm_device_item_new (GIcon *icon, const gchar *text, const gchar *id);

GIcon *
gpm_device_item_get_icon (GpmDeviceItem *item);
const gchar *
gpm_device_item_get_text (GpmDeviceItem *item);
const gchar *
gpm_device_item_get_id (GpmDeviceItem *item);

G_END_DECLS
