/*
 * include/timer.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_TIMER_H
#define _ANZZC_TIMER_H

#include <stdint.h>
#include <sys/timerfd.h>

#include "types.h"
#include "rbtree.h"
#include "list.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif


#define MSEC_PER_SEC            (1000LL)
#define NSEC_PER_MSEC           (1000000LL)
#define NSEC_PER_SEC 			(1000000000LL)

struct timer_list {
    struct rb_node entry;
    /* list need be used when several timers have the same expires*/
    struct list_head list;
    uint64_t expires;
    struct timer_base *base;

    void (*function)(unsigned long);
    unsigned long data;

    int state;
};


extern struct timer_base _timers;

#define TIMER_INITIALIZER(_name, _function, _expires, _data) {\
	.entry = RB_NODE_INITIALIZER(_name.entry),	\
	.list = LIST_HEAD_INIT(_name.list),	\
	.expires = (_expires),				\
	.base = &_timers, 					\
	.function = (_function),			\
	.data = (_data),				\
	.state = -1,					\
}

#define DEFINE_TIMER(_name, _function, _expires, _data)\
	struct timer_list _name =\
		TIMER_INITIALIZER(_name, _function, _expires, _data)

#define time_after(a,b)		\
	(typecheck(uint64_t, a) && \
	 typecheck(uint64_t, b) && \
	 ((int64_t)(b) - (int64_t)(a) < 0))
#define time_before(a,b)	time_after(b,a)

#define time_after_eq(a,b)\
	(typecheck(uint64_t, a) && \
	 typecheck(uint64_t, b) && \
	 ((int64_t)(a) - (int64_t)(b) >= 0))

#define time_before_eq(a,b)	time_after_eq(b,a)

/*register timer notifier to tick, get beat the clock. */
int init_timers(void);

static inline void setup_timer(struct timer_list *timer,
                               void (*function)(unsigned long), unsigned long data)
{
    timer->function = function;
    timer->data = data;
}

void init_timer(struct timer_list *timer);
int add_timer(struct timer_list *timer);
int del_timer(struct timer_list *timer);
int mod_timer(struct timer_list *timer, unsigned long expires);


/* current time in milliseconds */
static inline uint64_t curr_time_ms(void)
{
    struct timespec tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);
    return tm.tv_sec * MSEC_PER_SEC + (tm.tv_nsec / NSEC_PER_MSEC);
}

/**
 * timer_pending - is a timer pending?
 * @timer: the timer in question
 *
 * timer_pending will tell whether a given timer is currently pending,
 * or not. Callers must ensure serialization wrt. other operations done
 * to this timer, eg. interrupt contexts, or other CPUs on SMP.
 *
 * return value: 1 if the timer is pending, 0 if not.
 */
static inline int timer_pending(const struct timer_list *timer)
{
    return !RB_EMPTY_NODE(&timer->entry) || !list_empty(&timer->list);
}

#ifdef __cplusplus
}
#endif


#endif

