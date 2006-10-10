/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPMHAL_CPUFREQ_H
#define __GPMHAL_CPUFREQ_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_HAL_CPUFREQ		(gpm_hal_cpufreq_get_type ())
#define GPM_HAL_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_HAL_CPUFREQ, GpmHalCpuFreq))
#define GPM_HAL_CPUFREQ_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_HAL_CPUFREQ, GpmHalCpuFreqClass))
#define GPM_IS_HAL_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_HAL_CPUFREQ))
#define GPM_IS_HAL_CPUFREQ_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_HAL_CPUFREQ))
#define GPM_HAL_CPUFREQ_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_HAL_CPUFREQ, GpmHalCpuFreqClass))

typedef struct GpmHalCpuFreqPrivate GpmHalCpuFreqPrivate;

typedef struct
{
	GObject			 parent;
	GpmHalCpuFreqPrivate	*priv;
} GpmHalCpuFreq;


typedef struct
{
	GObjectClass	parent_class;
} GpmHalCpuFreqClass;

/* types of governor */
typedef enum {
	GPM_CPUFREQ_UNKNOWN = 0,
	GPM_CPUFREQ_ONDEMAND = 1,
	GPM_CPUFREQ_CONSERVATIVE = 2,
	GPM_CPUFREQ_POWERSAVE = 4,
	GPM_CPUFREQ_USERSPACE = 8,
	GPM_CPUFREQ_PERFORMANCE = 16,
	GPM_CPUFREQ_NOTHING = 32,
} GpmHalCpuFreqEnum;

#define CODE_CPUFREQ_ONDEMAND		"ondemand"
#define CODE_CPUFREQ_CONSERVATIVE	"conservative"
#define CODE_CPUFREQ_POWERSAVE		"powersave"
#define CODE_CPUFREQ_USERSPACE		"userspace"
#define CODE_CPUFREQ_PERFORMANCE	"performance"
#define CODE_CPUFREQ_NOTHING		"nothing"

GType		 gpm_hal_cpufreq_get_type		(void);
GpmHalCpuFreq	*gpm_hal_cpufreq_new			(void);

const gchar	*gpm_hal_cpufreq_enum_to_string		(GpmHalCpuFreqEnum cpufreq_type);
GpmHalCpuFreqEnum gpm_hal_cpufreq_string_to_enum	(const gchar *governor);
gboolean	 gpm_hal_cpufreq_get_governors		(GpmHalCpuFreq	*cpufreq,
							 GpmHalCpuFreqEnum *cpufreq_type);
gboolean	 gpm_hal_cpufreq_get_governor		(GpmHalCpuFreq	*cpufreq,
							 GpmHalCpuFreqEnum *cpufreq_type);
gboolean	 gpm_hal_cpufreq_set_governor		(GpmHalCpuFreq	*cpufreq,
							 GpmHalCpuFreqEnum  governor_enum);
gboolean	 gpm_hal_cpufreq_get_consider_nice	(GpmHalCpuFreq	*cpufreq,
							 gboolean	*consider_nice);
gboolean	 gpm_hal_cpufreq_set_consider_nice	(GpmHalCpuFreq	*cpufreq,
							 gboolean	 consider_nice);
gboolean	 gpm_hal_cpufreq_get_performance	(GpmHalCpuFreq	*cpufreq,
							 guint		*performance);
gboolean	 gpm_hal_cpufreq_set_performance	(GpmHalCpuFreq	*cpufreq,
							 guint		 performance);
guint		 gpm_hal_cpufreq_get_number_governors	(GpmHalCpuFreq	*cpufreq,
							 gboolean	 use_cache);

G_END_DECLS

#endif	/* __GPMHAL_CPUFREQ_H */
