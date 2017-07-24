/*
 * src/ioasync.c
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <include/utils.h>
#include <include/poller.h>
#include <include/queue.h>
#include <include/ioasync.h>
#include <include/workqueue.h>


struct iopacket {
    struct packet packet;
    struct sockaddr addr;
};


struct ioasync {
    struct poller poller;
    bool initialized;

    mempool_t *pkt_pool;
    pack_buf_pool_t *buf_pool;

    /* list of active iohandler objects */
    struct list_head active_list;

    /* list of closing struct iohandler objects.
     * these are waiting to push their
     * queued packets to the fd before
     * freeing themselves.
     */
    struct list_head closing_list;
    pthread_mutex_t lock;
};


struct handle_ops {
    void (*post)(void *priv, struct iopacket *pkt);
    void (*accept)(void *priv, int acceptfd);
    void (*handle)(void *priv, uint8_t *data, int len);
    void (*handlefrom)(void *priv, uint8_t *data, int len, void *from);
    void (*close)(void *priv);
};

enum iohandler_type {
    HANDLER_TYPE_NORMAL,
    HANDLER_TYPE_TCP_ACCEPT,
    HANDLER_TYPE_TCP,
    HANDLER_TYPE_UDP,
};


struct iohandler {
    int fd;
    int type;
    int flags;
    int closing;

    struct handle_ops h_ops;
    void *priv_data;

    struct workqueue_struct *wq;
    struct work_struct work;
    struct queue *q_in;
    struct queue *q_out;

    struct list_head entry;
    pthread_mutex_t lock;
    struct ioasync *owner;
};

static struct iopacket *iohandler_pack_alloc(iohandler_t *ioh, int allocbuf)
{
    struct iopacket *pkt;
    ioasync_t *aio = ioh->owner;

    pkt = (struct iopacket *)mempool_alloc(aio->pkt_pool);
    if (allocbuf) {
        pkt->packet.buf = (pack_buf_t *)pack_buf_alloc(aio->buf_pool);
    }

    return pkt;
}

static void iohandler_pack_free(iohandler_t *ioh, struct iopacket *pkt,
                                int freebuf)
{
    ioasync_t *aio = ioh->owner;

    if (freebuf) {
        pack_buf_free(pkt->packet.buf);
    }
    mempool_free(aio->pkt_pool, pkt);
}


pack_buf_t *iohandler_pack_buf_alloc(iohandler_t *ioh)
{
    pack_buf_t *pkb;
    ioasync_t *aio = ioh->owner;

    pkb = (pack_buf_t *)pack_buf_alloc(aio->buf_pool);
    return pkb;
}

void iohandler_pack_buf_free(pack_buf_t *pkb)
{
    pack_buf_free(pkb);
}

void iohandler_pack_submit(iohandler_t *ioh, struct iopacket *pack)
{
    int empty;

    pthread_mutex_lock(&ioh->lock);

    empty = !queue_count(ioh->q_out);
    if (empty) {
        ioasync_t *aio = ioh->owner;
        poller_event_enable(&aio->poller, ioh->fd, EV_WRITE);
    }
    queue_in(ioh->q_out, (struct packet *)pack);

    pthread_mutex_unlock(&ioh->lock);
}

void iohandler_pkt_send(iohandler_t *ioh, pack_buf_t *pkb)
{
    struct iopacket *pack;

    pack = iohandler_pack_alloc(ioh, 0);
    pack->packet.buf = pkb;

    iohandler_pack_submit(ioh, pack);
}

void iohandler_pkt_sendto(iohandler_t *ioh, pack_buf_t *pkb,
                          struct sockaddr *to)
{
    struct iopacket *pack;

    pack = iohandler_pack_alloc(ioh, 0);
    pack->packet.buf = pkb;
    pack->addr = *to;

    iohandler_pack_submit(ioh, pack);
}

void iohandler_send(iohandler_t *ioh, const uint8_t *data, int len)
{
    pack_buf_t *pkb;

    pkb = iohandler_pack_buf_alloc(ioh);
    memcpy(pkb->data, data, len);
    pkb->len = len;

    iohandler_pkt_send(ioh, pkb);
}

void iohandler_sendto(iohandler_t *ioh, const uint8_t *data, int len,
                      struct sockaddr *to)
{
    pack_buf_t *pkb;

    pkb = iohandler_pack_buf_alloc(ioh);
    memcpy(pkb->data, data, len);
    pkb->len = len;

    iohandler_pkt_sendto(ioh, pkb, to);
}


static void iohandler_close(iohandler_t *ioh)
{
    ioasync_t *aio = ioh->owner;

    if (ioh->h_ops.close)
        ioh->h_ops.close(ioh->priv_data);

    pthread_mutex_lock(&aio->lock);
    list_del(&ioh->entry);
    pthread_mutex_unlock(&aio->lock);

    queue_release(ioh->q_in);
    queue_release(ioh->q_out);

    if (ioh->fd > 0) {
        poller_event_del(&aio->poller, ioh->fd);
    }

    free(ioh);
}

void iohandler_shutdown(iohandler_t *ioh)
{
    int q_empty;
    ioasync_t *aio = ioh->owner;

    ioh->h_ops.close = NULL;

    q_empty = !!queue_count(ioh->q_out);

    if (q_empty) {
        iohandler_close(ioh);
    } else if (!q_empty && !ioh->closing) {
        ioh->closing = 1;

        pthread_mutex_lock(&aio->lock);
        list_del(&ioh->entry);
        list_add(&ioh->entry, &aio->closing_list);
        pthread_mutex_unlock(&aio->lock);

        return;
    }
}


static void iohandler_in_handle_work(struct work_struct *work)
{
    struct iopacket *pack;
    iohandler_t *ioh;

    ioh = container_of(work, struct iohandler, work);

    logv("iohandler handle work.\n");

    while (queue_count(ioh->q_in) > 0) {
        pack = (struct iopacket *)queue_out(ioh->q_in);
        if (!pack)
            return;

        if (ioh->h_ops.post)
            ioh->h_ops.post(ioh, pack);

        iohandler_pack_free(ioh, pack, 1);
    }
}

static void iohandler_in_pack_queue(iohandler_t *ioh, struct iopacket *pack)
{
    logv("iohandler receive data. packet queue.\n");
    queue_in(ioh->q_in, (struct packet *)pack);

    queue_work(ioh->wq, &ioh->work);
}

static int iohandler_read(iohandler_t *ioh)
{
    struct iopacket *pack;
    pack_buf_t  *pkb;

    pack = iohandler_pack_alloc(ioh, 1);
    pkb = pack->packet.buf;

    switch (ioh->type) {
        case HANDLER_TYPE_NORMAL:
        case HANDLER_TYPE_TCP: {
            pkb->len = xread(ioh->fd, pkb->data, PACKET_MAX_PAYLOAD);
            break;
        }
        case HANDLER_TYPE_UDP: {
            socklen_t addrlen = sizeof(struct sockaddr_in);
            bzero(&pack->addr, sizeof(pack->addr));
            pkb->len = recvfrom(ioh->fd, &pkb->data, PACKET_MAX_PAYLOAD,
                                0, &pack->addr, &addrlen);
            break;
        }
        case HANDLER_TYPE_TCP_ACCEPT: {
            int channel;
            channel = xaccept(ioh->fd);
            memcpy(pkb->data, &channel, sizeof(int));
            pkb->len = sizeof(int);
            break;
        }
        default:
            pkb->len = -1;
            break;
    }

    if (pkb->len < 0) {
        goto fail;
    } else if ((pkb->len == 0) &&
               (ioh->type && HANDLER_TYPE_TCP)) {
        iohandler_close(ioh);
        iohandler_pack_free(ioh, pack, 1);
        return 0;
    }

    iohandler_in_pack_queue(ioh, pack);
    return 0;

fail:
    loge("iohandler read data failed.\n");
    iohandler_pack_free(ioh, pack, 1);
    return -EINVAL;
}

static int iohandler_write_packet(iohandler_t *ioh, struct iopacket *pkt)
{
    int len = -EINVAL;

    switch (ioh->type) {
        case HANDLER_TYPE_NORMAL:
        case HANDLER_TYPE_TCP: {
            int out_pos = 0;
            int avail = 0;
            pack_buf_t *pkb = pkt->packet.buf;

            while (out_pos < pkb->len) {
                avail = pkb->len - out_pos;

                len = xwrite(ioh->fd, (&pkb->data) + out_pos, avail);
                if (len < 0)
                    goto fail;
                out_pos += len;
            }
            break;
        }
        case HANDLER_TYPE_UDP: {
            pack_buf_t *pkb = pkt->packet.buf;
            len = sendto(ioh->fd, &pkb->data, pkb->len, 0,
                         &pkt->addr, sizeof(struct sockaddr));
            if (len < 0)
                goto fail;
            break;
        }
        case HANDLER_TYPE_TCP_ACCEPT:
            BUG();
        default:
            goto fail;
    }

    return 0;

fail:
    loge("send data fail, ret=%d, droped.\n", len);
    return -EINVAL;
}


static int iohandler_write(iohandler_t *ioh)
{
    int ret;
    struct iopacket *pack;
    ioasync_t *aio = ioh->owner;

    if (queue_count(ioh->q_out) == 0)
        return 0;

    pack = (struct iopacket *)queue_out(ioh->q_out);
    if (!pack)
        return 0;

    logv("iohandler send data.\n");

    pthread_mutex_lock(&ioh->lock);
    if (queue_count(ioh->q_out) == 0)
        poller_event_disable(&aio->poller, ioh->fd, EV_WRITE);

    pthread_mutex_unlock(&ioh->lock);

    ret = iohandler_write_packet(ioh, pack);

    iohandler_pack_free(ioh, pack, 1);

    return ret;
}


/* iohandler file descriptor event callback for read/write ops */
static void iohandler_event(void *data, int events)
{
    iohandler_t *ioh = (iohandler_t *)data;

    /* in certain cases, it's possible to have both EPOLLIN and
     * EPOLLHUP at the same time. This indicates that there is incoming
     * data to read, but that the connection was nonetheless closed
     * by the sender. Be sure to read the data before closing
     * the receiver to avoid packet loss.
     */
    if (events & EV_READ) {
        iohandler_read(ioh);
    }

    if (events & EV_WRITE) {
        iohandler_write(ioh);
    }

    if (events & (EV_HUP | EV_ERROR)) {
        /* disconnection */
        loge("iohandler disconnect on fd %d", ioh->fd);
        iohandler_close(ioh);
        return;
    }
}

static iohandler_t *ioasync_create_context(ioasync_t *aio, int fd, int type)
{
    iohandler_t *ioh;

    ioh = malloc(sizeof(*ioh));
    if (!ioh)
        return NULL;

    ioh->fd = fd;
    ioh->type = type;
    ioh->flags = 0;
    ioh->closing = 0;

    /*XXX*/
    ioh->wq = alloc_workqueue(0, WQ_CPU_INTENSIVE);

    ioh->q_in = queue_init(0);
    ioh->q_out = queue_init(0);
    ioh->owner = aio;
    pthread_mutex_init(&ioh->lock, NULL);
    INIT_WORK(&ioh->work, iohandler_in_handle_work);

    /*Add to active list*/
    pthread_mutex_lock(&aio->lock);
    list_add(&ioh->entry, &aio->active_list);
    pthread_mutex_unlock(&aio->lock);

    poller_event_add(&aio->poller, fd, iohandler_event, ioh);
    poller_event_enable(&aio->poller, fd, EV_READ);
    return ioh;
}


static void iohandler_normal_post(void *priv, struct iopacket *pkt)
{
    iohandler_t *ioh = (iohandler_t *)priv;
    pack_buf_t *pkb = pkt->packet.buf;

    if (!pkb)
        return;

    if (ioh->h_ops.handle)
        ioh->h_ops.handle(ioh->priv_data, pkb->data, pkb->len);
}

iohandler_t *iohandler_create(ioasync_t *aio, int fd,
                              void (*handle)(void *, uint8_t *, int), void (*close)(void *), void *priv)
{
    iohandler_t *ioh;

    ioh = ioasync_create_context(aio, fd, HANDLER_TYPE_NORMAL);

    ioh->h_ops.post = iohandler_normal_post;
    ioh->h_ops.handle = handle;
    ioh->h_ops.close = close;

    ioh->priv_data = priv;

    return ioh;
}


static void iohandler_accept_post(void *priv, struct iopacket *pkt)
{
    iohandler_t *ioh = (iohandler_t *)priv;
    pack_buf_t *pkb = pkt->packet.buf;

    if (!pkb)
        return;

    if (ioh->h_ops.accept) {
        int channel;
        channel = ((int *)pkb->data)[0];
        ioh->h_ops.accept(ioh->priv_data, channel);
    }
}

iohandler_t *iohandler_accept_create(ioasync_t *aio, int fd,
                                     void (*accept)(void *, int), void (*close)(void *), void *priv)
{
    iohandler_t *ioh;

    ioh = ioasync_create_context(aio, fd, HANDLER_TYPE_TCP_ACCEPT);

    ioh->h_ops.post = iohandler_accept_post;
    ioh->h_ops.accept = accept;
    ioh->h_ops.close = close;

    ioh->priv_data = priv;

    listen(fd, 50);

    return ioh;
}


iohandler_t *iohandler_tcp_create(ioasync_t *aio, int fd,
                                  void (*handle)(void *, uint8_t *, int), void (*close)(void *), void *priv)
{
    iohandler_t *ioh;

    ioh = ioasync_create_context(aio, fd, HANDLER_TYPE_TCP);

    ioh->h_ops.post = iohandler_normal_post;
    ioh->h_ops.handle = handle;
    ioh->h_ops.close = close;

    ioh->priv_data = priv;

    return ioh;
}


static void iohandler_udp_post(void *priv, struct iopacket *pkt)
{
    iohandler_t *ioh = (iohandler_t *)priv;
    pack_buf_t *pkb = pkt->packet.buf;

    if (!pkb)
        return;

    if (ioh->h_ops.handlefrom)
        ioh->h_ops.handlefrom(ioh->priv_data, pkb->data, pkb->len, &pkt->addr);
}

iohandler_t *iohandler_udp_create(ioasync_t *aio, int fd,
                                  void (*handlefrom)(void *, uint8_t *, int, void *),
                                  void (*close)(void *), void *priv)
{
    iohandler_t *ioh;

    ioh = ioasync_create_context(aio, fd, HANDLER_TYPE_UDP);

    ioh->h_ops.post = iohandler_udp_post;
    ioh->h_ops.handlefrom = handlefrom;
    ioh->h_ops.close = close;

    ioh->priv_data = priv;

    return ioh;
}

static void *ioasync_handle(void *args)
{
    ioasync_t *aio = (ioasync_t *)args;

    poller_loop(&aio->poller);
    return 0;
}

ioasync_t *ioasync_init(void)
{
    int ret;
    pthread_t thread;
    ioasync_t *aio;

    aio = malloc(sizeof(*aio));
    if (!aio)
        return NULL;

    poller_init(&aio->poller);

    aio->pkt_pool = mempool_create(sizeof(struct iopacket), 128, 0);
    aio->buf_pool = create_pack_buf_pool(PACKET_MAX_PAYLOAD, 128);

    INIT_LIST_HEAD(&aio->active_list);
    INIT_LIST_HEAD(&aio->closing_list);

    pthread_mutex_init(&aio->lock, NULL);

    ret = pthread_create(&thread, NULL, ioasync_handle, aio);
    if (ret) {
        ret = -EINVAL;
        goto fail;
    }

    aio->initialized = 1;
    return aio;

fail:
    poller_done(&aio->poller);

    free_pack_buf_pool(aio->buf_pool);
    mempool_release(aio->pkt_pool);

    free(aio);
    return NULL;
}

#if 0
void ioasync_loop(ioasync_t *aio)
{
    poller_loop(&aio->poller);
}
#endif

void ioasync_release(ioasync_t *aio)
{
    aio->initialized = 0;

    poller_done(&aio->poller);

    free_pack_buf_pool(aio->buf_pool);
    mempool_release(aio->pkt_pool);

    free(aio);
}


/*****************************************************/

static ioasync_t *g_ioasync;

ioasync_t *get_global_ioasync(void)
{
    return g_ioasync;
}

void global_ioasync_init(void)
{
    g_ioasync = ioasync_init();
}

void global_ioasync_release(void)
{
    ioasync_release(g_ioasync);
}


