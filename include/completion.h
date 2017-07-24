/*
 * include/completion.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * Atomic wait-for-completion handler data structures.
 *
 */

#ifndef _ANZZC_COMPLETION_H
#define _ANZZC_COMPLETION_H

#include <pthread.h>

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * struct completion - structure used to maintain state for a "completion"
 *
 * This is the opaque structure used to maintain the state for a "completion".
 * Completions currently use a FIFO to queue threads that have to wait for
 * the "completion" event.
 *
 * See also:  complete(), wait_for_completion() (and friends _timeout,
 * _interruptible, _interruptible_timeout, and _killable), init_completion(),
 * and macros DECLARE_COMPLETION(), DECLARE_COMPLETION_ONSTACK(), and
 * INIT_COMPLETION().
 */
struct completion {
    unsigned int done;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

#define COMPLETION_INITIALIZER(work) { 	\
	.done = 0, 	\
	.lock = PTHREAD_MUTEX_INITIALIZER, \
	.cond = PTHREAD_COND_INITIALIZER }

#define COMPLETION_INITIALIZER_ONSTACK(work) \
	({ init_completion(&work); work; })

/**
 * DECLARE_COMPLETION - declare and initialize a completion structure
 * @work:  identifier for the completion structure
 *
 * This macro declares and initializes a completion structure. Generally used
 * for static declarations. You should use the _ONSTACK variant for automatic
 * variables.
 */
#define DECLARE_COMPLETION(work) \
	struct completion work = COMPLETION_INITIALIZER(work)

/**
 * init_completion - Initialize a dynamically allocated completion
 * @x:  completion structure that is to be initialized
 *
 * This inline function will initialize a dynamically created completion
 * structure.
 */
static inline void init_completion(struct completion *x)
{
    x->done = 0;
    pthread_mutex_init(&x->lock, NULL);
    pthread_cond_init(&x->cond, NULL);
}

extern void wait_for_completion(struct completion *);
extern unsigned long wait_for_completion_timeout(struct completion *x,
        unsigned long timeout);

extern bool try_wait_for_completion(struct completion *x);
extern bool completion_done(struct completion *x);

extern void complete(struct completion *);
extern void complete_all(struct completion *);

/**
 * INIT_COMPLETION - reinitialize a completion structure
 * @x:  completion structure to be reinitialized
 *
 * This macro should be used to reinitialize a completion structure so it can
 * be reused. This is especially important after complete_all() is used.
 */
#define INIT_COMPLETION(x)	((x).done = 0)

#ifdef __cplusplus
}
#endif


#endif
