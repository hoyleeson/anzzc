/*
 * include/poller.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_POLLER_H
#define _ANZZC_POLLER_H

#include <pthread.h>

#include "wait.h"

/* A struct poller object is used to monitor activity on one or more
 * file descriptors (e.g sockets).
 *
 * - call poller_event_add() to register a function that will be
 *   called when events happen on the file descriptor.
 *
 * - call poller_event_enable() or poller_disable() to enable/disable
 *   the set of monitored events for a given file descriptor.
 *
 * - call poller_event_del() to unregister a file descriptor.
 *   this does *not* close the file descriptor.
 *
 * Note that you can only provide a single function to handle
 * all events related to a given file descriptor.

 * You can call poller_event_enable/_disable/_del within a function
 * callback.
 */

/* the current implementation uses Linux's epoll facility
 * the event mask we use are simply combinations of EPOLLIN
 * EPOLLOUT, EPOLLHUP and EPOLLERR
 */

/* the event handler function type, 'data' is a user-specific
 * opaque pointer passed to poller_add().
 */
typedef void (*event_func)(void*  data, int  events);

/* bit flags for the struct event_hook structure.
 *
 * HOOK_PENDING means that an event happened on the
 * corresponding file descriptor.
 *
 * HOOK_CLOSING is used to delay-close monitored
 * file descriptors.
 */
enum {
    HOOK_PENDING = (1 << 0),
    HOOK_CLOSING = (1 << 1),
};


#define EV_READ 	EPOLLIN
#define EV_WRITE 	EPOLLOUT		
#define EV_ERROR 	EPOLLERR		
#define EV_HUP      EPOLLHUP

/* A struct event_hook structure is used to monitor a given
 * file descriptor and record its event handler.
 */
struct event_hook {
    int fd;
    int wanted;  /* events we are monitoring */
    int events;  /* events that occured */
    int state;   /* see HOOK_XXX constants */
    void* data; /* user-provided handler parameter */
    event_func func; /* event handler callback */
};

/* struct poller is the main object modeling a poller object
 */
struct poller {
    int epoll_fd;
    int num_fds;
    int max_fds;

    struct epoll_event* events;
    struct event_hook* hooks;
    int ctl_socks[2];
    int running;

    wait_queue_head_t waitq;
    pthread_mutex_t lock;
};


#ifdef __cplusplus
extern "C" {
#endif


void poller_event_add(struct poller* l, int fd, event_func func, void* user);
void poller_event_del(struct poller* l, int fd);
void poller_event_enable(struct poller* l, int  fd, int  events);
void poller_event_disable(struct poller* l, int  fd, int  events);
void poller_event_signal(struct poller* l);

void poller_loop(struct poller* l);
void poller_done(struct poller* l);

int poller_init(struct poller *l);
void poller_release(struct poller*  l);

struct poller *poller_create(void);
void poller_release(struct poller*  l);

#ifdef __cplusplus
}
#endif


#endif

