/*
 * src/wait.c
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdlib.h>

#include <include/wait.h>

void init_waitqueue_head(wait_queue_head_t *q)
{
    pthread_mutex_init(&q->lock, NULL);
    INIT_LIST_HEAD(&q->task_list);
}

void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
    wait->flags &= ~WQ_FLAG_EXCLUSIVE;

    pthread_mutex_lock(&q->lock);
    __add_wait_queue(q, wait);
    pthread_mutex_unlock(&q->lock);
}

void add_wait_queue_exclusive(wait_queue_head_t *q, wait_queue_t *wait)
{
    wait->flags |= WQ_FLAG_EXCLUSIVE;

    pthread_mutex_lock(&q->lock);
    __add_wait_queue_tail(q, wait);
    pthread_mutex_unlock(&q->lock);
}


void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
    pthread_mutex_lock(&q->lock);
    __remove_wait_queue(q, wait);
    pthread_mutex_unlock(&q->lock);
}

int autoremove_wake_function(wait_queue_t *wait, int sync)
{
    int ret = default_wake_function(wait, sync);

    if (ret)
        list_del_init(&wait->task_list);
    return ret;
}

int default_wake_function(wait_queue_t *curr, int wake_flags)
{
    complete(&curr->done);
    return 0;
}

static void __wake_up_common(wait_queue_head_t *q, int nr_exclusive,
                             int wake_flags)
{
    wait_queue_t *curr, *next;

    list_for_each_entry_safe(curr, next, &q->task_list, task_list) {
        unsigned flags = curr->flags;

        if (curr->func(curr, wake_flags) &&
            (flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive)
            break;
    }
}


/**
 * __wake_up - wake up threads blocked on a waitqueue.
 * @q: the waitqueue
 * @nr_exclusive: how many wake-one or wake-many threads to wake up
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void __wake_up(wait_queue_head_t *q, int nr_exclusive)
{
    pthread_mutex_lock(&q->lock);
    __wake_up_common(q, nr_exclusive, 0);
    pthread_mutex_unlock(&q->lock);
}

/*
 * Same as __wake_up but called with the spinlock in wait_queue_head_t held.
 */
void __wake_up_locked(wait_queue_head_t *q)
{
    __wake_up_common(q, 1, 0);
}

void prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
    wait->flags &= ~WQ_FLAG_EXCLUSIVE;
    pthread_mutex_lock(&q->lock);

    if (list_empty(&wait->task_list))
        __add_wait_queue(q, wait);

    pthread_mutex_unlock(&q->lock);
}

void prepare_to_wait_exclusive(wait_queue_head_t *q, wait_queue_t *wait)
{
    wait->flags |= WQ_FLAG_EXCLUSIVE;
    pthread_mutex_lock(&q->lock);
    if (list_empty(&wait->task_list))
        __add_wait_queue(q, wait);

    pthread_mutex_unlock(&q->lock);
}


void finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
    pthread_mutex_lock(&q->lock);
    if (!list_empty(&wait->task_list))
        list_del_init(&wait->task_list);

    pthread_mutex_unlock(&q->lock);
}

