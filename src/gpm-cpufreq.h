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

#ifndef __GPMCPUFREQ_H
#define __GPMCPUFREQ_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_CPUFREQ		(gpm_cpufreq_get_type ())
#define GPM_CPUFREQ(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_CPUFREQ, GpmCpuFreq))
#define GPM_CPUFREQ_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_CPUFREQ, GpmCpuFreqClass))
#define GPM_IS_CPUFREQ(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_CPUFREQ))
#define GPM_IS_CPUFREQ_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_CPUFREQ))
#define GPM_CPUFREQ_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_CPUFREQ, GpmCpuFreqClass))

typedef struct GpmCpuFreqPrivate GpmCpuFreqPrivate;

typedef struct
{
	GObject			 parent;
	GpmCpuFreqPrivate	*priv;
} GpmCpuFreq;


typedef struct
{
	GObjectClass	parent_class;
} GpmCpuFreqClass;

/* types of governor */
typedef enum {
	GPM_CPUFREQ_UNKNOWN = 0,
	GPM_CPUFREQ_ONDEMAND = 1,
	GPM_CPUFREQ_CONSERVATIVE = 2,
	GPM_CPUFREQ_POWERSAVE = 4,
	GPM_CPUFREQ_USERSPACE = 8,
	GPM_CPUFREQ_PERFORMANCE = 16,
	GPM_CPUFREQ_NOTHING = 32,
} GpmCpuFreqEnum;

#define CODE_CPUFREQ_ONDEMAND		"ondemand"
#define CODE_CPUFREQ_CONSERVATIVE	"conservative"
#define CODE_CPUFREQ_POWERSAVE		"powersave"
#define CODE_CPUFREQ_USERSPACE		"userspace"
#define CODE_CPUFREQ_PERFORMANCE	"performance"
#define CODE_CPUFREQ_NOTHING		"nothing"

GType		 gpm_cpufreq_get_type			(void);
GpmCpuFreq	*gpm_cpufreq_new			(void);
gboolean	 gpm_cpufreq_has_hw			(void);

const gchar	*gpm_cpufreq_enum_to_string		(GpmCpuFreqEnum  cpufreq_type);
GpmCpuFreqEnum	 gpm_cpufreq_string_to_enum		(const gchar	*governor);
gboolean	 gpm_cpufreq_get_governors		(GpmCpuFreq	*cpufreq,
							 GpmCpuFreqEnum *cpufreq_type);
gboolean	 gpm_cpufreq_get_governor		(GpmCpuFreq	*cpufreq,
							 GpmCpuFreqEnum *cpufreq_type);
gboolean	 gpm_cpufreq_set_governor		(GpmCpuFreq	*cpufreq,
							 GpmCpuFreqEnum  governor_enum);
gboolean	 gpm_cpufreq_get_consider_nice		(GpmCpuFreq	*cpufreq,
							 gboolean	*consider_nice);
gboolean	 gpm_cpufreq_set_consider_nice		(GpmCpuFreq	*cpufreq,
							 gboolean	 consider_nice);
gboolean	 gpm_cpufreq_get_performance		(GpmCpuFreq	*cpufreq,
							 guint		*performance);
gboolean	 gpm_cpufreq_set_performance		(GpmCpuFreq	*cpufreq,
							 guint		 performance);
guint		 gpm_cpufreq_get_number_governors	(GpmCpuFreq	*cpufreq,
							 gboolean	 use_cache);

G_END_DECLS

#endif	/* __GPMCPUFREQ_H */
