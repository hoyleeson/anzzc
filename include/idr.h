/*
 * include/idr.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * Small id to pointer translation service avoiding fixed sized
 * tables.
 *
 */

#ifndef _ANZZC_IDR_H
#define _ANZZC_IDR_H

#include <pthread.h>

#include "types.h"
#include "bitops.h"
#include "bitmap.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * We want shallower trees and thus more bits covered at each layer.  8
 * bits gives us large enough first layer for most use cases and maximum
 * tree depth of 4.  Each idr_layer is slightly larger than 2k on 64bit and
 * 1k on 32bit.
 */
#define IDR_BITS 8
#define IDR_SIZE (1 << IDR_BITS)
#define IDR_MASK ((1 << IDR_BITS)-1)

struct idr_layer {
    int			prefix;	/* the ID prefix of this idr_layer */
    int			layer;	/* distance from leaf */
    struct idr_layer *ary[1 << IDR_BITS];
    int			count;	/* When zero, we can release it */

    DECLARE_BITMAP(bitmap, IDR_SIZE);
};

struct idr {
    struct idr_layer *hint;	/* the last layer allocated from */
    struct idr_layer *top;
    int			layers;	/* only valid w/o concurrent changes */
    int			cur;	/* current pos for cyclic allocation */
    pthread_mutex_t lock;
    int			id_free_cnt;
    struct idr_layer	*id_free;
};

#define IDR_INIT(name)							\
{									\
	.lock			= __SPIN_LOCK_UNLOCKED(name.lock),	\
}
#define DEFINE_IDR(name)	struct idr name = IDR_INIT(name)


/*XXX*/
#define IDR_INIT_POINTER(p, v) do { \
    p = v;  \
} while(0)

#define idr_assign_pointer(p, v) do { \
    barrier();      \
    p = v;          \
} while(0)

#define idr_dereference_raw(p)  \
({          \
    p;      \
})


/**
 * DOC: idr sync
 * idr synchronization (stolen from radix-tree.h)
 *
 * idr_find() is able to be called locklessly, using RCU. The caller must
 * ensure calls to this function are made within rcu_read_lock() regions.
 * Other readers (lock-free or otherwise) and modifications may be running
 * concurrently.
 *
 * It is still required that the caller manage the synchronization and
 * lifetimes of the items. So if RCU lock-free lookups are used, typically
 * this would mean that the items have their own locks, or are amenable to
 * lock-free access; and that the items are freed by RCU (or only freed after
 * having been deleted from the idr tree *and* a synchronize_rcu() grace
 * period).
 */

/*
 * This is what we export.
 */

void *idr_find_slowpath(struct idr *idp, int id);
void idr_preload(void);
int idr_alloc(struct idr *idp, void *ptr, int start, int end);
int idr_alloc_cyclic(struct idr *idr, void *ptr, int start, int end);
int idr_for_each(struct idr *idp,
                 int (*fn)(int id, void *p, void *data), void *data);
void *idr_get_next(struct idr *idp, int *nextid);
void *idr_replace(struct idr *idp, void *ptr, int id);
void idr_remove(struct idr *idp, int id);
void idr_destroy(struct idr *idp);
void idr_init(struct idr *idp);
bool idr_is_empty(struct idr *idp);

/**
 * idr_preload_end - end preload section started with idr_preload()
 *
 * Each idr_preload() should be matched with an invocation of this
 * function.  See idr_preload() for details.
 */
static inline void idr_preload_end(void)
{
}

/**
 * idr_find - return pointer for given id
 * @idr: idr handle
 * @id: lookup key
 *
 * Return the pointer given the id it has been registered with.  A %NULL
 * return indicates that @id is not valid or you passed %NULL in
 * idr_get_new().
 *
 * This function can be called under rcu_read_lock(), given that the leaf
 * pointers lifetimes are correctly managed.
 */
static inline void *idr_find(struct idr *idr, int id)
{
    struct idr_layer *hint = idr_dereference_raw(idr->hint);

    if (hint && (id & ~IDR_MASK) == hint->prefix)
        return idr_dereference_raw(hint->ary[id & IDR_MASK]);

    return idr_find_slowpath(idr, id);
}

/**
 * idr_for_each_entry - iterate over an idr's elements of a given type
 * @idp:     idr handle
 * @entry:   the type * to use as cursor
 * @id:      id entry's key
 *
 * @entry and @id do not need to be initialized before the loop, and
 * after normal terminatinon @entry is left with the value NULL.  This
 * is convenient for a "not found" value.
 */
#define idr_for_each_entry(idp, entry, id)			\
	for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; ++id)

/*
 * IDA - IDR based id allocator, use when translation from id to
 * pointer isn't necessary.
 *
 * IDA_BITMAP_LONGS is calculated to be one less to accommodate
 * ida_bitmap->nr_busy so that the whole struct fits in 128 bytes.
 */
#define IDA_CHUNK_SIZE		128	/* 128 bytes per chunk */
#define IDA_BITMAP_LONGS	(IDA_CHUNK_SIZE / sizeof(long) - 1)
#define IDA_BITMAP_BITS 	(IDA_BITMAP_LONGS * sizeof(long) * 8)

struct ida_bitmap {
    long			nr_busy;
    unsigned long		bitmap[IDA_BITMAP_LONGS];
};

struct ida {
    struct idr		idr;
    struct ida_bitmap	*free_bitmap;
};

#define IDA_INIT(name)		{ .idr = IDR_INIT((name).idr), .free_bitmap = NULL, }
#define DEFINE_IDA(name)	struct ida name = IDA_INIT(name)

int ida_pre_get(struct ida *ida);
int ida_get_new_above(struct ida *ida, int starting_id, int *p_id);
void ida_remove(struct ida *ida, int id);
void ida_destroy(struct ida *ida);
void ida_init(struct ida *ida);

int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end);
void ida_simple_remove(struct ida *ida, unsigned int id);

/**
 * ida_get_new - allocate new ID
 * @ida:	idr handle
 * @p_id:	pointer to the allocated handle
 *
 * Simple wrapper around ida_get_new_above() w/ @starting_id of zero.
 */
static inline int ida_get_new(struct ida *ida, int *p_id)
{
    return ida_get_new_above(ida, 0, p_id);
}

void idr_init_cache(void);

#ifdef __cplusplus
}
#endif


#endif /* _ANZZC_IDR_H */
