/*
 * include/workqueue.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_WORK_QUEUE_H
#define _ANZZC_WORK_QUEUE_H

#include "list.h"
#include "timer.h"
#include "fake_atomic.h"

struct workqueue_struct;

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

/*
 * The first word is the work queue pointer and the flags rolled into
 * one
 */
#define work_data_bits(work) ((unsigned long *)(&(work)->data))


enum {
	WORK_STRUCT_PENDING_BIT = 0,    /* work item is pending execution */
	WORK_STRUCT_DELAYED_BIT = 1,    /* work item is delayed */

    WORK_STRUCT_PENDING = 1 << WORK_STRUCT_PENDING_BIT,
    WORK_STRUCT_DELAYED = 1 << WORK_STRUCT_DELAYED_BIT,

    WORK_STRUCT_FLAG_BITS = 2,
    WORK_STRUCT_FLAG_MASK = (1UL << WORK_STRUCT_FLAG_BITS) - 1,
    WORK_STRUCT_WQ_DATA_MASK = ~WORK_STRUCT_FLAG_MASK,

    /* bit mask for work_busy() return values */
    WORK_BUSY_PENDING   = 1 << 0,
    WORK_BUSY_RUNNING   = 1 << 1,
};

struct work_struct {
	struct list_head entry;
	work_func_t func;
	fake_atomic_long_t data;
};

struct delayed_work {
	struct work_struct work;
	struct timer_list timer;
};

static inline struct delayed_work *to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

#define __WORK_INITIALIZER(n, f) {              \
	.data = 0,            \
	.entry  = { &(n).entry, &(n).entry },           \
	.func = (f),                        \
}

#define __DELAYED_WORK_INITIALIZER(n, f) {          \
	.work = __WORK_INITIALIZER((n).work, (f)),      \
	.timer = TIMER_INITIALIZER((n).timer, NULL, 0, 0),         \
}


#define DECLARE_WORK(n, f)                  \
	struct work_struct n = __WORK_INITIALIZER(n, f)

#define DECLARE_DELAYED_WORK(n, f)              \
	struct delayed_work n = __DELAYED_WORK_INITIALIZER(n, f)


/*
 * initialize a work item's function pointer
 */
#define PREPARE_WORK(_work, _func)              \
    do {                            \
        (_work)->func = (_func);            \
    } while (0)

#define PREPARE_DELAYED_WORK(_work, _func)          \
    PREPARE_WORK(&(_work)->work, (_func))


#define INIT_WORK(_work, _func)                 \
    do {                                \
        fake_atomic_long_init(&(_work)->data, 0);   \
        INIT_LIST_HEAD(&(_work)->entry);            \
        PREPARE_WORK((_work), (_func));             \
    } while (0)

#define INIT_DELAYED_WORK(_work, _func)             \
    do {                            \
        INIT_WORK(&(_work)->work, (_func));     \
        init_timer(&(_work)->timer);            \
    } while (0)


/*
 * work_pending - Find out whether a work item is currently pending
 * @work: The work item in question
 */
#define work_pending(work) \
    (!!(fake_atomic_long_get(&(work)->data) & WORK_STRUCT_PENDING_BIT)) 

/**
 * delayed_work_pending - Find out whether a delayable work item is currently
 * pending
 * @work: The work item in question
 */
#define delayed_work_pending(w) \
    work_pending(&(w)->work)


/**
 * work_clear_pending - for internal use only, mark a work item as not pending
 * @work: The work item in question
 */
#define work_clear_pending(work) \
    fake_clear_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))


struct workqueue_struct *alloc_workqueue(int max_active, unsigned int flags);

#define create_workqueue()	    \
    alloc_workqueue(1, 0)

/*
 * Workqueue flags and constants.  For details, please refer to
 * Documentation/workqueue.txt.
 */
enum {
    WQ_NON_REENTRANT    = 1 << 0, /* guarantee non-reentrance */
    WQ_UNBOUND      = 1 << 1, /* not bound to any cpu */
    WQ_FREEZABLE        = 1 << 2, /* freeze during suspend */
    WQ_MEM_RECLAIM      = 1 << 3, /* may be used for memory reclaim */
    WQ_HIGHPRI      = 1 << 4, /* high priority */
    WQ_CPU_INTENSIVE    = 1 << 5, /* cpu instensive workqueue */

    WQ_DRAINING     = 1 << 6, /* internal: workqueue is draining */
    WQ_RESCUER      = 1 << 7, /* internal: workqueue has rescuer */

    WQ_MAX_ACTIVE       = 512,    /* I like 512, better ideas? */
    WQ_DFL_ACTIVE       = WQ_MAX_ACTIVE / 2,
};


extern struct workqueue_struct *global_wq;

#ifdef __cplusplus
extern "C" {
#endif

int queue_work(struct workqueue_struct *wq, struct work_struct *work);
int queue_delayed_work(struct workqueue_struct *wq,
             struct delayed_work *work, unsigned long delay);


void flush_workqueue(struct workqueue_struct *wq);
void drain_workqueue(struct workqueue_struct *wq);
void flush_scheduled_work(void);

int schedule_work(struct work_struct *work);
int schedule_delayed_work(struct delayed_work *work, unsigned long delay);


bool flush_work(struct work_struct *work);
bool flush_work_sync(struct work_struct *work);
bool cancel_work_sync(struct work_struct *work);

bool flush_delayed_work(struct delayed_work *dwork);
bool flush_delayed_work_sync(struct delayed_work *work);
bool cancel_delayed_work_sync(struct delayed_work *dwork);

void workqueue_set_max_active(struct workqueue_struct *wq,
                      int max_active);
bool workqueue_congested(unsigned int cpu, struct workqueue_struct *wq);
unsigned int work_busy(struct work_struct *work);

int init_workqueues(void);

#ifdef __cplusplus
}
#endif


#endif
