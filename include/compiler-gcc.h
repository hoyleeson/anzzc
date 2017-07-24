/*
 * include/compiler-gcc.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_COMPILER_GCC_H
#define _ANZZC_COMPILER_GCC_H

/* defined in sys/cdefs.h __always_inline     */
#undef __always_inline
#define __always_inline     inline __attribute__((always_inline))

#define barrier() __asm__ __volatile__("": : :"memory")

#endif

