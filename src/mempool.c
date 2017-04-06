/*
 * src/mempool.c
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
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <include/core.h>
#include <include/types.h>
#include <include/log.h>
#include <include/bsearch.h>
#include <include/compiler.h>
#include <include/mempool.h>

struct mempool {
    struct list_head free_list;
    struct list_head dynamic_free_list;
    pthread_mutex_t lock;
    uint8_t *buf;

    int bsize; /* block size */
    int init_count; /* initiailized count for blocks ever alloced */
    int count; /* total count for blocks ever alloced */
    int used; 	/* used block count */
    int dynamic_used; 	/* used block count */
    int limited; /* is resource limited to initial count? */
};


struct block
{
    struct list_head free;
};

#define block_data(b) ((void *)(b))

#define block_entry(buf)  ((struct block *)(buf))

mempool_t *mempool_create(int block_size, int init_count, int limited)
{
    mempool_t *pool = (mempool_t *)malloc(sizeof(mempool_t));

    INIT_LIST_HEAD(&pool->free_list);
    INIT_LIST_HEAD(&pool->dynamic_free_list);
    pthread_mutex_init(&pool->lock, NULL);
    pool->bsize = block_size;
    pool->init_count = pool->count = init_count;
    pool->used = pool->dynamic_used = 0;
    pool->limited = limited;

    if(init_count > 0) {
        pool->buf = calloc(init_count, block_size);
        if(!pool->buf)
            fatal("alloc memory fail.\n");

        struct block *b;
        int i;

        for(i=0 ; i<init_count ; i++) {
            b = (struct block *) (pool->buf + i * block_size);
            list_add(&b->free, &pool->free_list);
        }
    }

    return pool;
}

void mempool_release(mempool_t *pool)
{
    struct list_head *l, *tmp;
    struct block *b;

    list_for_each_safe(l, tmp, &pool->dynamic_free_list) {
        b = list_entry(l, struct block, free);
        list_del(l);

        free(b);
    }

    free(pool->buf);
    free(pool);
}

void *mempool_alloc(mempool_t *pool)
{
    struct block *b = NULL;
    struct list_head *l = NULL;

    pthread_mutex_lock(&pool->lock);

    if (!list_empty(&pool->free_list)) 
        l = pool->free_list.next;
    else if(!list_empty(&pool->dynamic_free_list)) {
        l = pool->dynamic_free_list.next;
        pool->dynamic_used++;
    }

    if(l) {
        list_del(l);
        b = list_entry(l, struct block, free);
    }

    if(unlikely(!b)) {
        if(pool->limited) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        } else {
            int c;
            b = (struct block *) malloc(pool->bsize);
            pool->dynamic_used++;
            c = ++pool->count;
            if( (c & ((1<<10)-1)) == 0 ) {
                logw("hitting %d blocks\n", c);
            }
        }
    }

    pool->used++;

    pthread_mutex_unlock(&pool->lock);

    return block_data(b);
}


void *mempool_zalloc(mempool_t *pool)
{
    void *ptr;

    ptr = mempool_alloc(pool);
    memset(ptr, 0, pool->bsize);
    return ptr;
}

static inline bool is_dynamic_mem(mempool_t *pool, void *buf) {
    return !(((buf - (void*)pool->buf) >= 0) &&
            ((((void*)pool->buf + pool->bsize*pool->init_count) - buf) > 0));
}

static bool mempool_needed_shrink(mempool_t *pool) 
{
    int free;
    int dynamic_free;

    free = pool->count - pool->used;
    dynamic_free = (pool->count - pool->init_count) - pool->dynamic_used;
    return !!((free > pool->init_count * 3) && 
            (dynamic_free > pool->init_count));
}

void mempool_shrink(mempool_t *pool) 
{
    struct list_head *l, *tmp;
    struct block *b;
    int shrink;
    int dynamic_free;

    pthread_mutex_lock(&pool->lock);

    shrink = (pool->count - pool->init_count) - pool->used;
    dynamic_free = (pool->count - pool->init_count) - pool->dynamic_used;
    shrink = min(shrink, dynamic_free);

    if(shrink <= 0) {
        pthread_mutex_unlock(&pool->lock);
        return;
    }

    list_for_each_safe(l, tmp, &pool->dynamic_free_list) {
        b = list_entry(l, struct block, free);
        list_del(l);

        free(b);
        pool->count--;

        if(--shrink == 0)
            break;
    }

    pthread_mutex_unlock(&pool->lock);
}

void mempool_free(mempool_t *pool, void *buf)
{
    struct block *b = block_entry(buf);

    pthread_mutex_lock(&pool->lock);

    if(is_dynamic_mem(pool, buf)) {
        list_add_tail(&b->free, &pool->dynamic_free_list);
        pool->dynamic_used--;
    } else {
        list_add_tail(&b->free, &pool->free_list);
    }

    pool->used--;

    pthread_mutex_unlock(&pool->lock);

    if(mempool_needed_shrink(pool)) 
        mempool_shrink(pool);
}


struct cache_sizes {
    size_t    	cs_size;
    int 		cs_count;
    mempool_t 	*cs_cachep;
};

static struct cache_sizes cachesizes[] = {
#define CACHE(x, n)  { .cs_size = (x), .cs_count = (n) },
#include <include/memsizes.h>
    CACHE(ULONG_MAX, 0)
#undef CACHE
};

int mem_cache_init(void)
{
    struct cache_sizes *sizes = cachesizes;

    while(sizes->cs_size != ULONG_MAX) {
        int size = sizeof(struct mem_head) + sizes->cs_size;
        sizes->cs_cachep = mempool_create(size, sizes->cs_count, IS_LIMIT);
        if(!sizes->cs_cachep) {
            goto mem_fail;
        }
        sizes++;
    }
    return 0;

mem_fail:
    do {
        sizes--;
        mempool_release(sizes->cs_cachep);
    } while(sizes != cachesizes);
    return -ENOMEM;
}

int size_cmp(const void *key, const void *elt) 
{
    const struct cache_sizes *a = key;
    const struct cache_sizes *b = elt;

    return a->cs_size - b->cs_size; 
}

int size_to_index(int size) {
    struct cache_sizes *node;
    struct cache_sizes key;

    key.cs_size = size;
    node = bsearch_edge(&key, cachesizes, ARRAY_SIZE(cachesizes),
            sizeof(struct cache_sizes), BSEARCH_MATCH_UP, size_cmp);
    if(node)
        return node->cs_size;
    return -EINVAL;
}

void *__mm_alloc(int size, int node) 
{
    mempool_t *pool;
    struct mem_item *item;

    if(node < 0) {
        node = size_to_index(size);
        if(node < 0 || node >= ARRAY_SIZE(cachesizes))
            return NULL;
    }

    pool = cachesizes[node].cs_cachep;
    item = (struct mem_item*)mempool_alloc(pool);
    item->head.size = size;
    return item->data;
}

void __mm_free(void *ptr, int node) 
{
    int size;
    struct mem_item *item;
    mempool_t *pool;

    item = mem_entry(ptr);
    size = item->head.size;

    if (!size)
        return;

    if(node < 0) {
        node = size_to_index(size);
        if(node < 0 || node >= ARRAY_SIZE(cachesizes))
            return;
    }

    pool = cachesizes[node].cs_cachep;
    mempool_free(pool, item);
}

