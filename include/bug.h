/*
 * include/bug.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_BUG_H
#define _ANZZC_BUG_H

#include "log.h"

#define BUG() do { \
    loge("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
    panic("BUG!"); \
} while (0)

#define BUG_ON(condition) do { if (unlikely(condition)) BUG(); } while (0)

#define BUILD_BUG_ON(condition)					\
    do {							\
        ((void)sizeof(char[1 - 2*!!(condition)]));	\
        if (condition) __build_bug_on_failed = 1;	\
    } while(0)

/**
 * BUILD_BUG - break compile if used.
 *
 * If you have some code that you expect the compiler to eliminate at
 * build time, you should use BUILD_BUG to detect if it is
 * unexpectedly used.
 */
#define BUILD_BUG()						\
    do {							\
        extern void __build_bug_failed(void)		\
           __linktime_error("BUILD_BUG failed");	\
        __build_bug_failed();				\
    } while (0)



/* Force a compilation error if a constant expression is not a power of 2 */
#define BUILD_BUG_ON_NOT_POWER_OF_2(n)			\
    BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))

/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_NULL(e) ((void *)sizeof(struct { int:-!!(e); }))

#define static_assert(expr) do { int _array[(expr) ? 1 : -1]; (void) _array[0]; } while (0)

#endif
