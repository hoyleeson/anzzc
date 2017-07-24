/*
 * src/completion.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#include <include/core.h>
#include <include/completion.h>


/**
 * wait_for_completion: - waits for completion of a task
 * @x:  holds the state of this particular completion
 *
 * This waits to be signaled for completion of a specific task. It is NOT
 * interruptible and there is no timeout.
 *
 * See also similar routines (i.e. wait_for_completion_timeout()) with timeout
 * and interrupt capability. Also see complete().
 */
void wait_for_completion(struct completion *x)
{
    pthread_mutex_lock(&x->lock);
    while (!x->done) {
        pthread_cond_wait(&x->cond, &x->lock);
    }
    x->done--;
    pthread_mutex_unlock(&x->lock);
}


/**
 * wait_for_completion_timeout: - waits for completion of a task (w/timeout)
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be signaled or for a
 * specified timeout to expire. The timeout is in jiffies. It is not
 * interruptible.
 */
unsigned long wait_for_completion_timeout(struct completion *x,
        unsigned long ms)
{
    int ret;
    struct timeval now;
    struct timespec timeout;

#define ms2ns(ms) ((ms)*1000*1000)

    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + ms / 1000;
    timeout.tv_nsec = now.tv_usec * 1000 + ms2ns(ms % 1000);

    pthread_mutex_lock(&x->lock);

    ret = pthread_cond_timedwait(&x->cond, &x->lock, &timeout);

    if (ret == ETIMEDOUT) {
        logi("wait for completion timeout!\n");
    } else if (ret == 0) {
        x->done--;
        logi("receive the completion.\n");
    } else {
        ret = -1;
        loge("wait for completion error!\n");
    }

    pthread_mutex_unlock(&x->lock);
    return ret;
}


/**
 *	try_wait_for_completion - try to decrement a completion without blocking
 *	@x:	completion structure
 *
 *	Returns: 0 if a decrement cannot be done without blocking
 *		 1 if a decrement succeeded.
 *
 *	If a completion is being used as a counting completion,
 *	attempt to decrement the counter without blocking. This
 *	enables us to avoid waiting if the resource the completion
 *	is protecting is not available.
 */
bool try_wait_for_completion(struct completion *x)
{
    int ret = 1;

    pthread_mutex_lock(&x->lock);
    if (!x->done)
        ret = 0;
    else
        x->done--;
    pthread_mutex_unlock(&x->lock);
    return ret;
}


/**
 *	completion_done - Test to see if a completion has any waiters
 *	@x:	completion structure
 *
 *	Returns: 0 if there are waiters (wait_for_completion() in progress)
 *		 1 if there are no waiters.
 *
 */
bool completion_done(struct completion *x)
{
    int ret = 1;
    pthread_mutex_lock(&x->lock);
    if (!x->done)
        ret = 0;
    pthread_mutex_unlock(&x->lock);
    return ret;
}

/**
 * complete: - signals a single thread waiting on this completion
 * @x:  holds the state of this particular completion
 *
 * This will wake up a single thread waiting on this completion. Threads will be
 * awakened in the same order in which they were queued.
 *
 * See also complete_all(), wait_for_completion() and related routines.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void complete(struct completion *x)
{
    pthread_mutex_lock(&x->lock);
    x->done++;
    pthread_cond_signal(&x->cond);
    pthread_mutex_unlock(&x->lock);
}

/**
 * complete_all: - signals all threads waiting on this completion
 * @x:  holds the state of this particular completion
 *
 * This will wake up all threads waiting on this particular completion event.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void complete_all(struct completion *x)
{
    pthread_mutex_lock(&x->lock);
    x->done += UINT_MAX / 2;
    pthread_cond_broadcast(&x->cond);
    pthread_mutex_unlock(&x->lock);
}


