/** @file	glibhal-extras.h
 *  @brief	GLIB replacement for libhal, the extra stuff
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-02
 */
/*
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/**
 * @addtogroup	glibhal
 * @{
 */

#ifndef _GLIBHALEXTRAS_H
#define _GLIBHALEXTRAS_H

gboolean hal_pm_check (void);
gboolean hal_pm_can_suspend (void);
gboolean hal_pm_can_hibernate (void);
gboolean hal_is_laptop (void);
gboolean hal_get_brightness_steps (gint *steps);
gboolean hal_set_brightness (gint brightness);
gboolean hal_set_brightness_dim (gint brightness);
gboolean hal_set_brightness_up (void);
gboolean hal_set_brightness_down (void);
gboolean hal_suspend (gint wakeup);
gboolean hal_hibernate (void);
gboolean hal_setlowpowermode (gboolean set);

#endif	/* _GLIBHALEXTRAS_H */
/** @} */
