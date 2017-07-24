/*
 * include/wait.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_WAIT_H
#define _ANZZC_WAIT_H

#include <pthread.h>

#include "completion.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct __wait_queue wait_queue_t;
typedef int (*wait_queue_func_t)(wait_queue_t *wait, int flags);
int default_wake_function(wait_queue_t *wait, int flags);


struct __wait_queue {
    unsigned int flags;
#define WQ_FLAG_EXCLUSIVE	0x01
    struct completion done;
    wait_queue_func_t func;
    struct list_head task_list;
};

struct __wait_queue_head {
    pthread_mutex_t lock;
    struct list_head task_list;
};

typedef struct __wait_queue_head wait_queue_head_t;

#define __WAITQUEUE_INITIALIZER(name) {				\
	.done 		= COMPLETION_INITIALIZER((name).done), 	\
	.func		= default_wake_function,	\
	.task_list	= { NULL, NULL } }

#define DECLARE_WAITQUEUE(name)					\
	wait_queue_t name = __WAITQUEUE_INITIALIZER(name)

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) {				\
	.lock		= PTHREAD_MUTEX_INITIALIZER,		\
	.task_list	= { &(name).task_list, &(name).task_list } }

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	wait_queue_head_t name = __WAIT_QUEUE_HEAD_INITIALIZER(name)

#define __WAIT_BIT_KEY_INITIALIZER(word, bit)				\
	{ .flags = word, .bit_nr = bit, }


static inline int waitqueue_active(wait_queue_head_t *q)
{
    return !list_empty(&q->task_list);
}

void init_waitqueue_head(wait_queue_head_t *q);

void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);
void add_wait_queue_exclusive(wait_queue_head_t *q, wait_queue_t *wait);
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);


static inline void __add_wait_queue(wait_queue_head_t *head, wait_queue_t *new)
{
    list_add(&new->task_list, &head->task_list);
}

/*
 * Used for wake-one threads:
 */
static inline void __add_wait_queue_exclusive(wait_queue_head_t *q,
        wait_queue_t *wait)
{
    wait->flags |= WQ_FLAG_EXCLUSIVE;
    __add_wait_queue(q, wait);
}

static inline void __add_wait_queue_tail(wait_queue_head_t *head,
        wait_queue_t *new)
{
    list_add_tail(&new->task_list, &head->task_list);
}

static inline void __add_wait_queue_tail_exclusive(wait_queue_head_t *q,
        wait_queue_t *wait)
{
    wait->flags |= WQ_FLAG_EXCLUSIVE;
    __add_wait_queue_tail(q, wait);
}

static inline void __remove_wait_queue(wait_queue_head_t *head,
                                       wait_queue_t *old)
{
    list_del(&old->task_list);
}

void __wake_up(wait_queue_head_t *q, int nr);

#define wake_up(x)			__wake_up(x, 1)
#define wake_up_nr(x, nr)		__wake_up(x, nr)
#define wake_up_all(x)			__wake_up(x, 0)
#define wake_up_locked(x)		__wake_up_locked((x))


#define __wait_event(wq, condition) 	\
do {									\
	DEFINE_WAIT(__wait);				\
									\
	for (;;) {							\
		prepare_to_wait(&wq, &__wait);	\
		if (condition)					\
			break;						\
		wait_for_completion(&__wait.done);			\
	}								\
	finish_wait(&wq, &__wait);			\
} while (0)

/**
 * wait_event - sleep until a condition gets true
 * @wq: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 *
 * The process is put to sleep (TASK_UNINTERRUPTIBLE) until the
 * @condition evaluates to true. The @condition is checked each time
 * the waitqueue @wq is woken up.
 *
 * wake_up() has to be called after changing any variable that could
 * change the result of the wait condition.
 */
#define wait_event(wq, condition) 					\
do {									\
	if (condition)	 						\
		break;							\
	__wait_event(wq, condition);					\
} while (0)

#define __wait_event_timeout(wq, condition, ret)			\
do {									\
	DEFINE_WAIT(__wait);						\
									\
	for (;;) {							\
		prepare_to_wait(&wq, &__wait);	\
		if (condition)						\
			break;						\
		ret = wait_for_completion_timeout(&__wait->done, ret); 	\
		if (ret >= 0)						\
			break;						\
	}								\
	finish_wait(&wq, &__wait);					\
} while (0)

/**
 * wait_event_timeout - sleep until a condition gets true or a timeout elapses
 * @wq: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 * @timeout: timeout, in jiffies
 *
 * The process is put to sleep (TASK_UNINTERRUPTIBLE) until the
 * @condition evaluates to true. The @condition is checked each time
 * the waitqueue @wq is woken up.
 *
 * wake_up() has to be called after changing any variable that could
 * change the result of the wait condition.
 *
 * The function returns 0 if the @timeout elapsed, and the remaining
 * jiffies if the condition evaluated to true before the timeout elapsed.
 */
#define wait_event_timeout(wq, condition, timeout)			\
({									\
	long __ret = timeout;						\
	if (!(condition)) 						\
		__wait_event_timeout(wq, condition, __ret);		\
	__ret;								\
})

#define __wait_event_exclusive(wq, condition) 	\
do {									\
	DEFINE_WAIT(__wait);				\
									\
	for (;;) {							\
		prepare_to_wait_exclusive(&wq, &__wait);	\
		if (condition)					\
			break;						\
		wait_for_completion(&__wait.done);			\
	}								\
	finish_wait(&wq, &__wait);			\
} while (0)

#define wait_event_exclusive(wq, condition) 	\
do {									\
	if (condition)	 						\
		break;							\
	__wait_event_exclusive(wq, condition);		\
} while (0)



/*
 * Waitqueues which are removed from the waitqueue_head at wakeup time
 */
void prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait);
void prepare_to_wait_exclusive(wait_queue_head_t *q, wait_queue_t *wait);
void finish_wait(wait_queue_head_t *q, wait_queue_t *wait);
int autoremove_wake_function(wait_queue_t *wait, int sync);

#define DEFINE_WAIT_FUNC(name, function)				\
	wait_queue_t name = {						\
		.done 		= COMPLETION_INITIALIZER((name).done), 	\
		.func		= function,				\
		.task_list	= LIST_HEAD_INIT((name).task_list),	\
	}

#define DEFINE_WAIT(name) DEFINE_WAIT_FUNC(name, autoremove_wake_function)

#ifdef __cplusplus
}
#endif


#endif
