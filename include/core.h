/*
 * include/core.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_CORE_H
#define _ANZZC_CORE_H

#include "bug.h"

#ifndef USHRT_MAX
#define USHRT_MAX	((u16)(~0U))
#endif
#ifndef SHRT_MAX
#define SHRT_MAX	((s16)(USHRT_MAX>>1))
#endif
#ifndef SHRT_MIN
#define SHRT_MIN	((s16)(-SHRT_MAX - 1))
#endif
#ifndef INT_MAX
#define INT_MAX		((int)(~0U>>1))
#endif
#ifndef INT_MIN
#define INT_MIN		(-INT_MAX - 1)
#endif
#ifndef UINT_MAX
#define UINT_MAX	(~0U)
#endif
#ifndef LONG_MAX
#define LONG_MAX	((long)(~0UL>>1))
#endif
#ifndef LONG_MIN
#define LONG_MIN	(-LONG_MAX - 1)
#endif
#ifndef ULONG_MAX
#define ULONG_MAX	(~0UL)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX	((long long)(~0ULL>>1))
#endif
#ifndef LLONG_MIN
#define LLONG_MIN	(-LLONG_MAX - 1)
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX	(~0ULL)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 	(4*1024)
#endif

#define __ALIGN_COMMON(x, a)		__ALIGN_COMMON_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_COMMON_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#define ALIGN(x, a)		__ALIGN_COMMON((x), (a))
#define __ALIGN_MASK(x, mask)	__ALIGN_COMMON_MASK((x), (mask))
#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))
#define IS_ALIGNED(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)


#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
#define __must_be_array(a) BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define DIV_ROUND_UP_ULL(ll,d) \
    ({ unsigned long long _tmp = (ll)+(d)-1; do_div(_tmp, d); _tmp; })

/*
 * swap - swap value of @a and @b
 */
#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

/*
 * abs() handles unsigned and signed longs, ints, shorts and chars.  For all
 * input types abs() returns a signed long.
 * abs() should not be used for 64-bit types (s64, u64, long long) - use abs64()
 * for those.
 */
#define abs(x) ({						\
		long ret;					\
		if (sizeof(x) == sizeof(long)) {		\
			long __x = (x);				\
			ret = (__x < 0) ? -__x : __x;		\
		} else {					\
			int __x = (x);				\
			ret = (__x < 0) ? -__x : __x;		\
		}						\
		ret;						\
	})

#define abs64(x) ({				\
		s64 __x = (x);			\
		(__x < 0) ? -__x : __x;		\
	})


#define min(x, y) ({                \
		typeof(x) _min1 = (x);          \
		typeof(y) _min2 = (y);          \
		(void) (&_min1 == &_min2);      \
		_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({                \
		typeof(x) _max1 = (x);          \
		typeof(y) _max2 = (y);          \
		(void) (&_max1 == &_max2);      \
		_max1 > _max2 ? _max1 : _max2; })

#define min3(x, y, z) ({            \
		typeof(x) _min1 = (x);          \
		typeof(y) _min2 = (y);          \
		typeof(z) _min3 = (z);          \
		(void) (&_min1 == &_min2);      \
		(void) (&_min1 == &_min3);      \
		_min1 < _min2 ? (_min1 < _min3 ? _min1 : _min3) : \
		(_min2 < _min3 ? _min2 : _min3); })

#define max3(x, y, z) ({            \
		typeof(x) _max1 = (x);          \
		typeof(y) _max2 = (y);          \
		typeof(z) _max3 = (z);          \
		(void) (&_max1 == &_max2);      \
		(void) (&_max1 == &_max3);      \
		_max1 > _max2 ? (_max1 > _max3 ? _max1 : _max3) : \
		(_max2 > _max3 ? _max2 : _max3); })

/**
 * min_not_zero - return the minimum that is _not_ zero, unless both are zero
 * @x: value1
 * @y: value2
 */
#define min_not_zero(x, y) ({           \
		typeof(x) __x = (x);            \
		typeof(y) __y = (y);            \
		__x == 0 ? __y : ((__y == 0) ? __x : min(__x, __y)); })

/**
 * clamp - return a value clamped to a given range with strict typechecking
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * This macro does strict typechecking of min/max to make sure they are of the
 * same type as val.  See the unnecessary pointer comparisons.
 */
#define clamp(val, min, max) ({         \
		typeof(val) __val = (val);      \
		typeof(min) __min = (min);      \
		typeof(max) __max = (max);      \
		(void) (&__val == &__min);      \
		(void) (&__val == &__max);      \
		__val = __val < __min ? __min: __val;   \
		__val > __max ? __max: __val; })


/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use min/max/clamp at all, of course.
 */
#define min_t(type, x, y) ({            \
        type __min1 = (x);          \
        type __min2 = (y);          \
        __min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({            \
        type __max1 = (x);          \
        type __max2 = (y);          \
        __max1 > __max2 ? __max1: __max2; })

/*
 * clamp_t - return a value clamped to a given range using a given type
 * @type: the type of variable to use
 * @val: current value
 * @lo: minimum allowable value
 * @hi: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of type
 * 'type' to make all the comparisons.
 */
#define clamp_t(type, val, lo, hi) min_t(type, max_t(type, val, lo), hi)


#endif
