/*
 * src/data_frag.c
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
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include <include/timer.h>
#include <include/log.h>
#include <include/hash.h>
#include <include/list.h>
#include <include/data_frag.h>


#define FRAG_HASH_SHIFT     (8)
#define FRAG_HASH_SZ    (1<<FRAG_HASH_SHIFT)

#define FRAG_HASH_FN(idx, bits) \
    (hash_long((idx), bits))

#define frag_hash_key(id) \
    (FRAG_HASH_FN((id), FRAG_HASH_SHIFT))



#define FRAGS_HASH_CAPACITY     (256)

#define DATA_MAX_LEN        (1024*1024*1024)

typedef struct _frag_node {
    uint16_t id;
    uint8_t mf;
    uint32_t frag_ofs;    /* max data len: 1MB */
    uint32_t datalen;     /* packet len */
    void *data;
    struct list_head node;
    void *pkt;
} frag_node_t;

struct data_frags {
    int fraglen;
    int nextseq;
    int stat_timeout;
    struct hlist_head hlist[FRAG_HASH_SZ];

    void (*input)(void *opaque, void *data, int len);
    void (*output)(void *opaque, data_vec_t *v);
    void (*free)(void *opaque, void *frag_pkt);
    void *data;
    pthread_mutex_t lock;
};

typedef struct _frag_queue {
    int id;
    int total_len;
    int recv_len;
    struct list_head list;
    struct hlist_node entry;
    pthread_mutex_t lock;
    struct timer_list timer;
    struct data_frags *owner;
} frag_queue_t;


static inline int alloc_frag_seq(data_frags_t *frags)
{
    int seq;

    pthread_mutex_lock(&frags->lock);
    seq = frags->nextseq++;
    pthread_mutex_unlock(&frags->lock);
    return seq;
}

int data_frag(data_frags_t *frags, void *data, int len)
{
    int i = 0;
    int ofs = 0;
    data_vec_t v;
    int seq = alloc_frag_seq(frags);

    while (ofs < len) {
        v.ofs = ofs;
        v.data = data + ofs;
        v.len = (len - ofs) > frags->fraglen ? frags->fraglen : len - ofs;
        v.seq = seq;

        ofs += v.len;
        if (ofs == len)
            v.mf = 1;
        else
            v.mf = 0;

        frags->output(frags->data, &v);
        i++;
    }

    return i;
}


static frag_node_t *frag_node_create(data_vec_t *v, void *frag_pkt)
{
    frag_node_t *frag;

    frag = (frag_node_t *)malloc(sizeof(*frag));
    if (!frag)
        return NULL;

    frag->id = v->seq;
    frag->mf = v->mf;
    frag->frag_ofs = v->ofs;
    frag->data = v->data;
    frag->datalen = v->len;
    frag->pkt = frag_pkt;

    return frag;
}

static void frag_node_free(frag_node_t *frag)
{
    free(frag);
}

static void frag_queue_free(frag_queue_t *fq)
{
    frag_node_t *frag, *p;
    data_frags_t *frags = fq->owner;

    pthread_mutex_lock(&fq->lock);
    list_for_each_entry_safe(frag, p, &fq->list, node) {
        list_del(&frag->node);
        if (frags->free && frag->pkt)
            frags->free(frags->data, frag->pkt);

        frag_node_free(frag);
    }
    pthread_mutex_unlock(&fq->lock);

    free(fq);
}


static void rm_frag_queue(data_frags_t *frags, frag_queue_t *fq)
{
    pthread_mutex_lock(&frags->lock);

    hlist_del(&fq->entry);
    del_timer(&fq->timer);
    frag_queue_free(fq);

    pthread_mutex_unlock(&frags->lock);
}

static void __attribute__ ((unused)) dump_frag_queue(frag_queue_t *fq)
{
    int nextofs = 0;
    frag_node_t *frag;

    list_for_each_entry_reverse(frag, &fq->list, node) {
        logi("[%d].\n", frag->frag_ofs);
    }

    list_for_each_entry_reverse(frag, &fq->list, node) {
        if (nextofs != frag->frag_ofs) {
            logi("frag [%d %d] lost.\n", nextofs, frag->frag_ofs);
            nextofs = frag->frag_ofs + frag->datalen;
        } else
            nextofs += frag->datalen;
    }
}

static void defrag_timeout_handle(unsigned long data)
{
    data_frags_t *frags;
    frag_queue_t *fq = (frag_queue_t *)data;

    frags = fq->owner;

    logw("defrag timout, seq:%d, (%d times)\n", fq->id, frags->stat_timeout);

#ifdef VDEBUG
    dump_frag_queue(fq);
#endif
    rm_frag_queue(fq->owner, fq);

    pthread_mutex_lock(&frags->lock);
    frags->stat_timeout++;
    pthread_mutex_unlock(&frags->lock);
}

static frag_queue_t *frag_queue_create(data_frags_t *frags, int id)
{
    frag_queue_t *fq;

    fq = (frag_queue_t *)malloc(sizeof(*fq));

    fq->id = id;
    fq->total_len = 0;
    fq->recv_len = 0;
    fq->owner = frags;

    INIT_LIST_HEAD(&fq->list);
    pthread_mutex_init(&fq->lock, NULL);

    init_timer(&fq->timer);
    setup_timer(&fq->timer, defrag_timeout_handle, (unsigned long)fq);
    mod_timer(&fq->timer, curr_time_ms() + DEFRAG_TIMEOUT);

    return fq;
}

static frag_queue_t *find_frag_queue(data_frags_t *frags, frag_node_t *frag)
{
    int key;
    struct hlist_node *pos;
    frag_queue_t *fq = NULL;

    key = frag_hash_key(frag->id);

    pthread_mutex_lock(&frags->lock);

    hlist_for_each_entry(fq, pos, &frags->hlist[key], entry) {
        if (fq->id == frag->id)
            break;
    }

    if (!fq) {
        fq = frag_queue_create(frags, frag->id);
        hlist_add_head(&fq->entry, &frags->hlist[key]);
    }
    pthread_mutex_unlock(&frags->lock);

    return fq;
}


static int data_frag_insert(frag_queue_t *fq, frag_node_t *frag)
{
    frag_node_t *p;

    list_for_each_entry(p, &fq->list, node) {
        if (p->frag_ofs < frag->frag_ofs) {
            list_add(&frag->node, p->node.prev);
            return 0;
        } else if (p->frag_ofs == frag->frag_ofs) {
            return -EEXIST;
        }
    }

    list_add_tail(&frag->node, &p->node);

    return 0;
}

static int __attribute__((warn_unused_result)) check_defrag(frag_queue_t *fq)
{
    int ret = 0;
    int nextofs = 0;
    frag_node_t *frag;

    list_for_each_entry_reverse(frag, &fq->list, node) {
        if (nextofs != frag->frag_ofs) {
            ret = -EINVAL;
            goto out;
        }
        nextofs += frag->datalen;
    }

out:
    return ret;
}

static int data_frag_queue(frag_queue_t *fq, frag_node_t *frag)
{
    int ret;

    pthread_mutex_lock(&fq->lock);
    ret = data_frag_insert(fq, frag);
    if (ret == -EEXIST) {
        pthread_mutex_unlock(&fq->lock);
        return ret;
    }

    fq->recv_len += frag->datalen;
    if (frag->mf) {
        fq->total_len = frag->frag_ofs + frag->datalen;
    }
    pthread_mutex_unlock(&fq->lock);

    if (!fq->total_len ||
        fq->total_len != fq->recv_len)
        return 0;

    if (check_defrag(fq)) {
        logw("check defrag fail.\n");
        return -EINVAL;
    }

    return fq->total_len;
}

int data_frag_reasm(frag_queue_t *fq, void *data)
{
    int ret = 0;
    frag_node_t *frag;

    list_for_each_entry_reverse(frag, &fq->list, node) {
        memcpy(data + frag->frag_ofs, frag->data, frag->datalen);
        ret += frag->datalen;
    }
    return ret;
}


int data_defrag(data_frags_t *frags, data_vec_t *v, void *frag_pkt)
{
    int ret;
    void *data;
    int len;
    frag_queue_t *fq;
    frag_node_t *frag;

    frag = frag_node_create(v, frag_pkt);
    fq = find_frag_queue(frags, frag);

    if (!fq) {
        ret = -EINVAL;
        goto fail;
    }

    ret = data_frag_queue(fq, frag);
    if (ret <= 0) {
        return ret;
    }

    /* All data have been successfully received, submit now. */
    len = fq->total_len;
    if (len > DATA_MAX_LEN) {
        ret = -EINVAL;
        goto fail;
    }

    data = malloc(len);
    if (!data) {
        ret = -ENOMEM;
        goto fail;
    }

    ret = data_frag_reasm(fq, data);

    frags->input(frags->data, data, len);
    free(data);

    rm_frag_queue(frags, fq);
    return 0;

fail:
    frag_node_free(frag);
    return ret;
}

data_frags_t *data_frag_init(int fraglen,
                             void (*input)(void *, void *, int),
                             void (*output)(void *, data_vec_t *v),
                             void (*free_pkt)(void *opaque, void *frag_pkt),
                             void *opaque)
{
    int i;
    data_frags_t *frags;

    frags = (data_frags_t *)malloc(sizeof(*frags));
    if (!frags)
        return NULL;

    frags->fraglen = fraglen;
    frags->stat_timeout = 0;
    frags->input = input;
    frags->output = output;
    frags->free = free_pkt;
    frags->data = opaque;
    frags->nextseq = 0;

    for (i = 0; i < FRAG_HASH_SZ; i++) {
        INIT_HLIST_HEAD(&frags->hlist[i]);
    }

    pthread_mutex_init(&frags->lock, NULL);

    return frags;
}

void data_frag_release(data_frags_t *frags)
{
    int i;

    for (i = 0; i < FRAG_HASH_SZ; i++) {
        frag_queue_t *fq;
        struct hlist_node *pos;

        hlist_for_each_entry(fq, pos, &frags->hlist[i], entry) {
            rm_frag_queue(frags, fq);
        }
    }

    free(frags);
}


