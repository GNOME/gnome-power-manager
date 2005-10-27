/** @file	compiler.h
 *  @brief	Common compiler stuff
 *  @author	Richard Hughes <richard@hughsie.com>
 *  @date	2005-10-04
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
#ifndef _COMPILER_H
#define _COMPILER_H

/** define GCC macros to let us do clever things */
#if __GNUC__ >= 3
# define inline		inline __attribute__ ((always_inline))
# define __pure		__attribute__ ((pure))
# define __noreturn	__attribute__ ((noreturn))
# define __malloc	__attribute__ ((malloc))
# define __must_check	__attribute__ ((warn_unused_result))
# define __deprecated	__attribute__ ((deprecated))
# define __used		__attribute__ ((used))
# define __unused	__attribute__ ((unused))
# define __packed	__attribute__ ((packed))
# define likely(x)	__builtin_expect (!!(x), 1)
# define unlikely(x)	__builtin_expect (!!(x), 0)
#else
# define inline		/* no inline */
# define __pure		/* no pure */
# define __noreturn	/* no noreturn */
# define __malloc	/* no malloc */
# define __must_check	/* no warn_unused_result */
# define __deprecated	/* no deprecated */
# define __used		/* no used */
# define __unused	/* no unused */
# define __packed	/* no packed */
# define likely(x)	(x)
# define unlikely(x)	(x)
#endif

#endif	/* _COMPILER_H */
