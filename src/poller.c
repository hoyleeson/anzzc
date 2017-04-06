/*
 * src/poller.c
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
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <include/sizes.h>
#include <include/poller.h>
#include <include/utils.h>
#include <include/log.h>

enum loop_ev_opt {
    EV_POLLER_ADD,
    EV_POLLER_DEL,
    EV_POLLER_ENABLE,
    EV_POLLER_DISABLE,
    EV_POLLER_SIGNAL,
};

typedef struct {
    int opt;
    int fd;

    union {
        struct {
            void* ev_user;
            event_func ev_func;
        } ev; /* used for looper add */
        int events; 		/* used for looper enable / disable */
    };
} poller_ctl_t;


static inline void poller_ctl_submit(struct poller* l, void *data, int len)
{
    int ret;

    pthread_mutex_lock(&l->lock);
    ret = xwrite(l->ctl_socks[0], data, len);
    if(ret < 0)
        loge("poller ctl command submit failed(%d).\n", ret);
    pthread_mutex_unlock(&l->lock);
}

/* register a file descriptor and its event handler.
 * no event mask will be enabled
 */
void poller_event_add(struct poller* l, int fd, event_func func, void* user)
{
    poller_ctl_t ctl;

    ctl.opt = EV_POLLER_ADD;
    ctl.fd = fd;

    ctl.ev.ev_user = user;
    ctl.ev.ev_func = func;

    poller_ctl_submit(l, &ctl, sizeof(ctl));
}

/*
 * unregister a file descriptor and its event handler
 */
void poller_event_del(struct poller* l, int fd)
{
    poller_ctl_t ctl;

    ctl.opt = EV_POLLER_DEL;
    ctl.fd = fd;

    poller_ctl_submit(l, &ctl, sizeof(ctl));
}

/* enable monitoring of certain events for a file
 * descriptor. This adds 'events' to the current
 * event mask
 */
void poller_event_enable(struct poller* l, int  fd, int  events)
{
    poller_ctl_t ctl;

    ctl.opt = EV_POLLER_ENABLE;
    ctl.fd = fd;
    ctl.events = events;

    poller_ctl_submit(l, &ctl, sizeof(ctl));
}

/* disable monitoring of certain events for a file
 * descriptor. This ignores events that are not
 * currently enabled.
 */
void poller_event_disable(struct poller* l, int  fd, int  events)
{
    poller_ctl_t ctl;

    ctl.opt = EV_POLLER_DISABLE;
    ctl.fd = fd;
    ctl.events = events;

    poller_ctl_submit(l, &ctl, sizeof(ctl));
}

/* 
 * 
 */
void poller_event_signal(struct poller* l)
{
    poller_ctl_t ctl;

    ctl.opt = EV_POLLER_SIGNAL;
    poller_ctl_submit(l, &ctl, sizeof(ctl));
}


/* return the struct event_hook corresponding to a given
 * monitored file descriptor, or NULL if not found
 */
static struct event_hook* poller_find(struct poller*  l, int  fd)
{
    struct event_hook*  hook = l->hooks;
    struct event_hook*  end  = hook + l->num_fds;

    for (; hook < end; hook++) {
        if (hook->fd == fd)
            return hook;
    }
    return NULL;
}

/* grow the arrays in the poller object */
static void poller_grow(struct poller*  l)
{
    int  old_max = l->max_fds;
    int  new_max = old_max + (old_max >> 1) + 4;
    int  n;

    xrenew(l->events, new_max);
    xrenew(l->hooks,  new_max);
    l->max_fds = new_max;

    /* now change the handles to all events */
    for (n = 0; n < l->num_fds; n++) {
        struct epoll_event ev;
        struct event_hook* hook = l->hooks + n;

        ev.events   = hook->wanted;
        ev.data.ptr = hook;
        epoll_ctl(l->epoll_fd, EPOLL_CTL_MOD, hook->fd, &ev);
    }
}

/* register a file descriptor and its event handler.
 * no event mask will be enabled
 */
static void poller_add(struct poller* l, int fd, event_func  func, void*  data)
{
    struct epoll_event  ev;
    struct event_hook*           hook;

    if (l->num_fds >= l->max_fds)
        poller_grow(l);

    hook = l->hooks + l->num_fds;

    hook->fd      = fd;
    hook->data = data;
    hook->func = func;
    hook->state   = 0;
    hook->wanted  = 0;
    hook->events  = 0;

    setnonblock(fd);

    ev.events   = 0;
    ev.data.ptr = hook;
    epoll_ctl(l->epoll_fd, EPOLL_CTL_ADD, fd, &ev);

    if(!l->num_fds++)
        wake_up(&l->waitq);
}

/* unregister a file descriptor and its event handler
*/
static void poller_del(struct poller*  l, int  fd)
{
    struct event_hook*  hook = poller_find(l, fd);

    if (!hook) {
        loge("%s: invalid fd: %d", __func__, fd);
        return;
    }
    /* don't remove the hook yet */
    hook->state |= HOOK_CLOSING;

    epoll_ctl(l->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

/* enable monitoring of certain events for a file
 * descriptor. This adds 'events' to the current
 * event mask
 */
static void poller_enable(struct poller*  l, int  fd, int  events)
{
    struct event_hook*  hook = poller_find(l, fd);

    if (!hook) {
        loge("%s: invalid fd: %d", __func__, fd);
        return;
    }

    if (events & ~hook->wanted) {
        struct epoll_event  ev;

        hook->wanted |= events;
        ev.events   = hook->wanted;
        ev.data.ptr = hook;

        epoll_ctl(l->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    }
}

/* disable monitoring of certain events for a file
 * descriptor. This ignores events that are not
 * currently enabled.
 */
static void poller_disable(struct poller*  l, int  fd, int  events)
{
    struct event_hook*  hook = poller_find(l, fd);

    if (!hook) {
        loge("%s: invalid fd: %d", __func__, fd);
        return;
    }

    if (events & hook->wanted) {
        struct epoll_event  ev;

        hook->wanted &= ~events;
        ev.events   = hook->wanted;
        ev.data.ptr = hook;

        epoll_ctl(l->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    }
}


static void poller_ctl_event(struct poller *l, int events)
{
    poller_ctl_t ctl;
    int len;

    if(!(events & EPOLLIN)) {
        return;
    }

    len = xread(l->ctl_socks[1], &ctl, sizeof(poller_ctl_t));
    if(len < sizeof(poller_ctl_t))
        return;

    switch(ctl.opt) {
        case EV_POLLER_ADD:
            poller_add(l, ctl.fd, ctl.ev.ev_func, ctl.ev.ev_user);
            break;
        case EV_POLLER_DEL:
            poller_del(l, ctl.fd);
            break;
        case EV_POLLER_ENABLE:
            poller_enable(l, ctl.fd, ctl.events);
            break;
        case EV_POLLER_DISABLE:
            poller_disable(l, ctl.fd, ctl.events);
            break;
        default:
            break;
    }
}


static int poller_exec(struct poller* l) {
    int  n, count;
    struct event_hook* hook;

    wait_event(l->waitq, l->num_fds != 0);

    do {
        count = epoll_wait(l->epoll_fd, l->events, l->num_fds, -1);
    } while (count < 0 && errno == EINTR);

    if (count < 0) {
        loge("%s: error: %s", __func__, strerror(errno));
        return -EINVAL;
    }

    if (count == 0) {
        loge("poller huh ? epoll returned count=0");
        return 0;
    }

    /* mark all pending hooks */
    for (n = 0; n < count; n++) {
        hook = l->events[n].data.ptr;
        hook->state  = HOOK_PENDING;
        hook->events = l->events[n].events;
    }

#define FIRST_DYNAMIC_SLOT     (1)
    /* execute hook callbacks. this may change the 'hooks'
     * and 'events' array, as well as l->num_fds, so be careful */
    for (n = FIRST_DYNAMIC_SLOT; n < l->num_fds; n++) {
        hook = l->hooks + n;
        if (hook->state & HOOK_PENDING) {
            hook->state &= ~HOOK_PENDING;
            hook->func(hook->data, hook->events);
        }
    }

    /* now remove all the hooks that were closed by
     * the callbacks */
    for (n = FIRST_DYNAMIC_SLOT; n < l->num_fds;) {
        struct epoll_event ev;
        hook = l->hooks + n;

        if (!(hook->state & HOOK_CLOSING)) {
            n++;
            continue;
        }

        hook[0]     = l->hooks[l->num_fds-1];
        l->num_fds--;
        ev.events   = hook->wanted;
        ev.data.ptr = hook;
        epoll_ctl(l->epoll_fd, EPOLL_CTL_MOD, hook->fd, &ev);
    }

    /* slot 0: manage hook. */
    hook = l->hooks;
    if (hook->state & HOOK_PENDING) {
        hook->state &= ~HOOK_PENDING;
        hook->func(hook->data, hook->events);
    }

    return 0;
}

/* wait until an event occurs on one of the registered file
 * descriptors. Only returns in case of error !!
 */
void poller_loop(struct poller* l)
{
    int ret;
    for (;;) {
        if(!l->running)
            break;

        ret = poller_exec(l);
        if(ret)
            break;
    }
}

void poller_done(struct poller* l)
{
    l->running = 0;
    poller_event_signal(l);
}


/* initialize a poller object */
int poller_init(struct poller *l) 
{
    int ret;
    int size = SZ_32K;

    l->epoll_fd = epoll_create(1);
    l->num_fds  = 0;
    l->max_fds  = 0;
    l->events   = NULL;
    l->hooks    = NULL;

    pthread_mutex_init(&l->lock, NULL);
    init_waitqueue_head(&l->waitq);
    ret = socketpair(AF_UNIX, SOCK_DGRAM, 0, l->ctl_socks);
    if (ret < 0) {
        loge("error in socketpair(). errno:%d.\n", errno);
        return -EINVAL;
    }

    logd("create poller ctl event pipe, sockpair:%d:%d.\n",
            l->ctl_socks[0], l->ctl_socks[1]);

    setsockopt(l->ctl_socks[0], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    setsockopt(l->ctl_socks[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    setsockopt(l->ctl_socks[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    setsockopt(l->ctl_socks[1], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    fcntl(l->ctl_socks[0], F_SETFL, O_NONBLOCK);
    fcntl(l->ctl_socks[1], F_SETFL, O_NONBLOCK);

    poller_add(l, l->ctl_socks[1], (event_func)poller_ctl_event, l);
    poller_enable(l, l->ctl_socks[1], EPOLLIN);
    l->running = 1;

    return 0;
}

/* finalize a poller object */
void poller_release(struct poller*  l)
{
    xfree(l->events);
    xfree(l->hooks);
    l->max_fds = 0;
    l->num_fds = 0;

    close(l->epoll_fd);
    l->epoll_fd  = -1;
}

struct poller *poller_create(void) 
{
    struct poller* l;

    xnew(l);
    return l;
}

/* finalize a poller object */
void poller_free(struct poller*  l)
{
    xfree(l);
}

