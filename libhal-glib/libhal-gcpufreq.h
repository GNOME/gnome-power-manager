/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __LIBHAL_GCPUFREQ_H
#define __LIBHAL_GCPUFREQ_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LIBHAL_TYPE_CPUFREQ		(hal_gcpufreq_get_type ())
#define LIBHAL_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), LIBHAL_TYPE_CPUFREQ, HalGCpufreq))
#define LIBHAL_CPUFREQ_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), LIBHAL_TYPE_CPUFREQ, HalGCpufreqClass))
#define LIBHAL_IS_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), LIBHAL_TYPE_CPUFREQ))
#define LIBHAL_IS_CPUFREQ_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), LIBHAL_TYPE_CPUFREQ))
#define LIBHAL_CPUFREQ_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), LIBHAL_TYPE_CPUFREQ, HalGCpufreqClass))

typedef struct HalGCpufreqPrivate HalGCpufreqPrivate;

typedef struct
{
	GObject			 parent;
	HalGCpufreqPrivate	*priv;
} HalGCpufreq;


typedef struct
{
	GObjectClass	parent_class;
} HalGCpufreqClass;

/* types of governor */
typedef enum {
	LIBHAL_CPUFREQ_UNKNOWN = 0,
	LIBHAL_CPUFREQ_ONDEMAND = 1,
	LIBHAL_CPUFREQ_CONSERVATIVE = 2,
	LIBHAL_CPUFREQ_POWERSAVE = 4,
	LIBHAL_CPUFREQ_USERSPACE = 8,
	LIBHAL_CPUFREQ_PERFORMANCE = 16,
	LIBHAL_CPUFREQ_NOTHING = 32,
} HalGCpufreqType;

#define CODE_CPUFREQ_ONDEMAND		"ondemand"
#define CODE_CPUFREQ_CONSERVATIVE	"conservative"
#define CODE_CPUFREQ_POWERSAVE		"powersave"
#define CODE_CPUFREQ_USERSPACE		"userspace"
#define CODE_CPUFREQ_PERFORMANCE	"performance"
#define CODE_CPUFREQ_NOTHING		"nothing"

GType		 hal_gcpufreq_get_type			(void);
HalGCpufreq	*hal_gcpufreq_new			(void);
gboolean	 hal_gcpufreq_has_hw			(void);

const gchar	*hal_gcpufreq_enum_to_string		(HalGCpufreqType  cpufreq_type);
HalGCpufreqType	 hal_gcpufreq_string_to_enum		(const gchar	*governor);
gboolean	 hal_gcpufreq_get_governors		(HalGCpufreq	*cpufreq,
							 HalGCpufreqType *cpufreq_type);
gboolean	 hal_gcpufreq_get_governor		(HalGCpufreq	*cpufreq,
							 HalGCpufreqType *cpufreq_type);
gboolean	 hal_gcpufreq_set_governor		(HalGCpufreq	*cpufreq,
							 HalGCpufreqType  governor_enum);
gboolean	 hal_gcpufreq_get_consider_nice		(HalGCpufreq	*cpufreq,
							 gboolean	*consider_nice);
gboolean	 hal_gcpufreq_set_consider_nice		(HalGCpufreq	*cpufreq,
							 gboolean	 consider_nice);
gboolean	 hal_gcpufreq_get_performance		(HalGCpufreq	*cpufreq,
							 guint		*performance);
gboolean	 hal_gcpufreq_set_performance		(HalGCpufreq	*cpufreq,
							 guint		 performance);
guint		 hal_gcpufreq_get_number_governors	(HalGCpufreq	*cpufreq,
							 gboolean	 use_cache);

G_END_DECLS

#endif	/* __LIBHAL_GCPUFREQ_H */
