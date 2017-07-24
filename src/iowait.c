/*
 * src/iowait.c
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <include/iowait.h>
#include <include/hash.h>

#define RES_SLOT_SHIFT          (6)
#define RES_SLOT_CAPACITY       (1 << RES_SLOT_SHIFT)
#define RES_SLOT_MASK           (RES_SLOT_CAPACITY - 1)

int iowait_init(iowait_t *wait)
{
    int i;

    pthread_mutex_init(&wait->lock, NULL);
    wait->slots = malloc(sizeof(struct hlist_head) * RES_SLOT_CAPACITY);

    for (i = 0; i < RES_SLOT_CAPACITY; i++)
        INIT_HLIST_HEAD(&wait->slots[i]);
    return 0;
}

static struct hlist_head *watcher_slot_head(iowait_t *wait, int type, int seq)
{
    unsigned long key;

    key = type << 16 | seq;
    key = hash_long(key, RES_SLOT_SHIFT);
    return &wait->slots[key];
}

void iowait_watcher_init(iowait_watcher_t *watcher,
                         int type, int seq, void *result, int count)
{
    watcher->type = type;
    watcher->seq = seq;
    watcher->res = result;
    watcher->count = count;

    init_completion(&watcher->done);
}

int iowait_register_watcher(iowait_t *wait, iowait_watcher_t *watcher)
{
    struct hlist_head *rsh;

    rsh = watcher_slot_head(wait, watcher->type, watcher->seq);

    pthread_mutex_lock(&wait->lock);
    hlist_add_head(&watcher->hentry, rsh);
    pthread_mutex_unlock(&wait->lock);

    return 0;
}

int wait_for_response_data(iowait_t *wait, iowait_watcher_t *watcher, int *res)
{
    int ret;

    ret = wait_for_completion_timeout(&watcher->done, WAIT_RES_DEAD_LINE);

    if (res != NULL)
        *res = watcher->count;

    pthread_mutex_lock(&wait->lock);
    hlist_del_init(&watcher->hentry);
    pthread_mutex_unlock(&wait->lock);
    return ret;
}


int post_response_data(iowait_t *wait, int type, int seq,
                       void *result, int count)
{
    iowait_watcher_t *watcher = NULL;
    struct hlist_head *rsh;
    struct hlist_node *tmp;

    rsh = watcher_slot_head(wait, type, seq);

    pthread_mutex_lock(&wait->lock);
    hlist_for_each_entry(watcher, tmp, rsh, hentry) {
        if (watcher->type == type && watcher->seq == seq) {
            pthread_mutex_unlock(&wait->lock);
            goto found;
        }
    }
    pthread_mutex_unlock(&wait->lock);
    return -EINVAL;

found:
    if ((watcher->count == 0) ||
        (watcher->count != 0 && watcher->count > count))
        watcher->count = count;

    memcpy(watcher->res, result, watcher->count);

    complete(&watcher->done);
    return 0;
}


int wait_for_response(iowait_t *wait, iowait_watcher_t *watcher)
{
    return wait_for_response_data(wait, watcher, NULL);
}


int post_response(iowait_t *wait, int type, int seq, void *result,
                  void (*fn)(void *dst, void *src))
{
    struct hlist_head *rsh;
    struct hlist_node *r;
    iowait_watcher_t *watcher;

    rsh = watcher_slot_head(wait, type, seq);

    pthread_mutex_lock(&wait->lock);
    hlist_for_each_entry(watcher, r, rsh, hentry) {
        if (watcher->type == type && watcher->seq == seq) {
            pthread_mutex_unlock(&wait->lock);
            goto found;
        }
    }
    pthread_mutex_unlock(&wait->lock);
    return -EINVAL;

found:
    fn(watcher->res, result);

    complete(&watcher->done);
    return 0;
}

