/*
 * include/queue.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_QUEUE_H
#define _ANZZC_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#include "list.h"
#include "packet.h"

#define QUEUE_F_BLOCK 		(1 << 0)

#define QUEUE_NONBLOCK 		(0)
#define QUEUE_BLOCK 		(1)

struct queue {
	struct list_head 	list;
	pthread_mutex_t 	lock;
	pthread_cond_t 		cond;
	size_t 	count;
	int 	flags;
};

#ifdef __cplusplus
extern "C" {
#endif


struct queue *queue_init();
void queue_release(struct queue *q);

void queue_in(struct queue *q, struct packet *p);
struct packet *queue_out(struct queue *q);
struct packet *queue_peek(struct queue *q);

size_t queue_count(struct queue *q);
void queue_clear(struct queue *q, void(*reclaim)(struct packet *p));

#ifdef __cplusplus
}
#endif


#endif
