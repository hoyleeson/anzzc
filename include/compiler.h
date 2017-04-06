/*
 * include/compiler.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_COMPILER_H
#define _ANZZC_COMPILER_H

#ifdef __GNUC__
#include "compiler-gcc.h"
#endif


#ifndef __always_inline
#define __always_inline inline
#endif

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#ifndef barrier
# define barrier() __memory_barrier()
#endif


#endif

