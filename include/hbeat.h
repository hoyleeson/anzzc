/*
 * include/hbeat.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */


#ifndef _ANZZC_HBEAT_H
#define _ANZZC_HBEAT_H

#include <pthread.h>

#include "list.h"
#include "timer.h"

#define HBEAT_INIT 		    (3)
#define HBEAD_DEAD_LINE     (10 * MSEC_PER_SEC)

#ifdef __cplusplus
extern "C" {
#endif


typedef struct hbeat_node {
    int count;
    int online;
    struct list_head node;
} hbeat_node_t;

typedef struct hbeat_god {
    struct list_head list;
    struct timer_list timer;
    void (*dead)(hbeat_node_t *hbeat);
    pthread_mutex_t lock;
} hbeat_god_t;


void user_heartbeat(hbeat_node_t *hbeat);

void hbeat_add_to_god(hbeat_god_t *god, hbeat_node_t *hbeat);
void hbeat_rm_from_god(hbeat_god_t *god, hbeat_node_t *hbeat);

void hbeat_god_init(hbeat_god_t *god, void (*dead)(hbeat_node_t *));

#ifdef __cplusplus
}
#endif


#endif
