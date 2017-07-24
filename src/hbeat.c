/*
 * src/hbeat.c
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdio.h>

#include <include/timer.h>
#include <include/log.h>
#include <include/hbeat.h>
#include <include/list.h>


void user_heartbeat(hbeat_node_t *hbeat)
{
    hbeat->count = HBEAT_INIT;
    hbeat->online = 1;
}

void hbeat_add_to_god(hbeat_god_t *god, hbeat_node_t *hbeat)
{
    hbeat->count = HBEAT_INIT;
    hbeat->online = 1;

    pthread_mutex_lock(&god->lock);
    list_add_tail(&hbeat->node, &god->list);
    pthread_mutex_unlock(&god->lock);
}

void hbeat_rm_from_god(hbeat_god_t *god, hbeat_node_t *hbeat)
{
    pthread_mutex_lock(&god->lock);
    list_del(&hbeat->node);
    pthread_mutex_unlock(&god->lock);
}

void hbeat_god_handle(unsigned long data)
{
    hbeat_node_t *hbeat, *tmp;
    hbeat_god_t *god = (hbeat_god_t *)data;

    list_for_each_entry_safe(hbeat, tmp, &god->list, node) {
        hbeat->count--;

        if (hbeat->count <= 0) {
            hbeat->online = 0;
            god->dead(hbeat);
        }
    }

    mod_timer(&god->timer, curr_time_ms() + HBEAD_DEAD_LINE);
}

void hbeat_god_init(hbeat_god_t *god, void (*dead)(hbeat_node_t *))
{
    INIT_LIST_HEAD(&god->list);

    god->dead = dead;
    init_timer(&god->timer);
    setup_timer(&god->timer, hbeat_god_handle, (unsigned long)god);
    pthread_mutex_init(&god->lock, NULL);

    mod_timer(&god->timer, curr_time_ms() + HBEAD_DEAD_LINE);
}


