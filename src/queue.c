/*
 * src/queue.c
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

#include <include/queue.h>

static int queue_empty(struct queue *q)
{
    return list_empty(&q->list);
}

void queue_in(struct queue *q, struct packet *p)
{
    pthread_mutex_lock(&q->lock);

    list_add_tail(&p->node, &q->list);

    if((q->count++) == 0 && (q->flags & QUEUE_F_BLOCK))
        pthread_cond_broadcast(&q->cond);

    pthread_mutex_unlock(&q->lock);
}

struct packet *queue_out(struct queue *q)
{
    struct packet *p;
    pthread_mutex_lock(&q->lock);

retry:
    if(queue_empty(q)) {
        if(q->flags & QUEUE_F_BLOCK) {
            pthread_cond_wait(&q->cond, &q->lock);
            goto retry;
        } else {
            pthread_mutex_unlock(&q->lock);
            return NULL;
        }
    }

    p = list_first_entry(&q->list, struct packet, node);
    list_del_init(q->list.next);
    q->count--;

    pthread_mutex_unlock(&q->lock);
    return p;
}

/**
 * queue_peek - get data from the fifo without removing
 */
struct packet *queue_peek(struct queue *q)
{
    struct packet *p;

    if(queue_empty(q)) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    p = list_entry(q->list.next, struct packet, node);
    return p;
}


size_t queue_count(struct queue *q) 
{
    return q->count;
}

void queue_clear(struct queue *q, void(*reclaim)(struct packet *p))
{
    struct packet *p, *tmp;
    pthread_mutex_lock(&q->lock);

    list_for_each_entry_safe(p, tmp, &q->list, node) {
        list_del(&p->node);

        if(reclaim != NULL)
            reclaim(p);
    }

    q->count = 0;
    pthread_mutex_unlock(&q->lock);
}

struct queue *queue_init(int block)
{
    struct queue *q;

    q = (struct queue *)malloc(sizeof(*q));
    if(!q)
        return NULL;

    INIT_LIST_HEAD(&q->list);
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);

    q->flags = (!!block) << QUEUE_F_BLOCK;
    q->count = 0;

    return q;
}

void queue_release(struct queue *q)
{
    if(q->count > 0)
        queue_clear(q, NULL);

    q->count = 0;
    free(q);
}


