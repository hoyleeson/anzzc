/*
 * src/packet.c
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
#include <pthread.h>

#include <include/packet.h>
#include <include/log.h>


pack_buf_pool_t *create_pack_buf_pool(int esize, int ecount)
{
    pack_buf_pool_t *pool;

    pool = malloc(sizeof(*pool));

    /*Create unlimited memory pools. */
    pool->pool = mempool_create(esize + sizeof(pack_buf_t), ecount, 0);

    return pool;
}

void free_pack_buf_pool(pack_buf_pool_t *pool)
{
    mempool_release(pool->pool);
    free(pool);
}


pack_buf_t *pack_buf_alloc(pack_buf_pool_t *pool)
{
    pack_buf_t *pkb;
    pkb = mempool_alloc(pool->pool);

    pkb->owner = pool;
    fake_atomic_init(&pkb->refcount, 1); 

    return pkb;
}

pack_buf_t *pack_buf_get(pack_buf_t *pkb)
{
    fake_atomic_inc(&pkb->refcount); 
    return pkb;
}

void pack_buf_free(pack_buf_t *pkb)
{
    if(fake_atomic_dec_and_test(&pkb->refcount)) {
        pack_buf_pool_t *pool = pkb->owner;
        mempool_free(pool->pool, pkb); 
    }
}



