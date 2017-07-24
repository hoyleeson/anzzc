/*
 * include/packet.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_PACKET_H
#define _ANZZC_PACKET_H

#include <stdint.h>

#include "list.h"
#include "fake_atomic.h"
#include "mempool.h"
#include "core.h"

#define PACKET_MAX_PAYLOAD      (2000)

typedef struct _pack_buf pack_buf_t;

struct packet {
    struct list_head node;
    pack_buf_t *buf;
};


typedef struct _pack_buf_pool {
    mempool_t *pool;
} pack_buf_pool_t;

struct _pack_buf {
    pack_buf_pool_t *owner;
    fake_atomic_t refcount;

    int len;
    uint8_t data[0];
};

#define node_to_item(node, container, member) \
    (container *) (((char*) (node)) - offsetof(container, member))

#define data_to_pack_buf(ptr) \
    node_to_item(ptr, pack_buf_t, data)

#ifdef __cplusplus
extern "C" {
#endif


pack_buf_pool_t *create_pack_buf_pool(int esize, int ecount);
void free_pack_buf_pool(pack_buf_pool_t *pool);

pack_buf_t *pack_buf_alloc(pack_buf_pool_t *pool);
pack_buf_t *pack_buf_get(pack_buf_t *pkb);
void pack_buf_free(pack_buf_t *pkb);

#ifdef __cplusplus
}
#endif


#endif

