/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_DEBUG_H
#define __GPM_DEBUG_H

#include <stdarg.h>
#include <glib.h>

G_BEGIN_DECLS

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define gpm_debug(...) gpm_debug_real (__func__, __FILE__, __LINE__, __VA_ARGS__)
#define gpm_warning(...) gpm_warning_real (__func__, __FILE__, __LINE__, __VA_ARGS__)
#define gpm_error(...) gpm_error_real (__func__, __FILE__, __LINE__, __VA_ARGS__)
#define gpm_assert_run_once(x) static guint (x) = 0; if ((x)++ != 0) gpm_error ("%s, run multi!", __func__); gpm_warning ("%s, run multi!", __func__);
#elif defined(__GNUC__) && __GNUC__ >= 3
#define gpm_debug(...) gpm_debug_real (__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#define gpm_warning(...) gpm_warning_real (__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#define gpm_error(...) gpm_error_real (__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#define gpm_assert_run_once(x) static guint (x) = 0; if ((x)++ != 0) gpm_error ("%s, run multi!", __FUNCTION__); gpm_warning ("%s, run multi!", __FUNCTION__);
#else
#define gpm_debug
#define gpm_warning
#define gpm_error
#define gpm_assert_run_once(x)
#endif

void		gpm_debug_init			(gboolean	 debug);
void		gpm_debug_shutdown		(void);
void		gpm_debug_real			(const gchar	*func,
						 const gchar	*file,
						 int		 line,
						 const gchar	*format, ...);
void		gpm_warning_real		(const gchar	*func,
						 const gchar	*file,
						 int		 line,
						 const gchar	*format, ...);
void		gpm_error_real			(const gchar	*func,
						 const gchar	*file,
						 int		 line,
						 const gchar	*format, ...);
void		gpm_syslog			(const gchar	*format, ...);
void		gpm_debug_add_option		(const gchar	*option);

G_END_DECLS

#endif /* __GPM_DEBUG_H */
