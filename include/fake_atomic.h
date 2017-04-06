/*
 * include/fake_atomic.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_FAKE_ATOMIC_H
#define _ANZZC_FAKE_ATOMIC_H

#include <pthread.h>

#include "bitops.h"


typedef struct _fake_atomic {
    pthread_mutex_t lock;   /* It must be placed in front */
    int counter;
} fake_atomic_t;


typedef struct _fake_atomic_long {
    pthread_mutex_t lock; /* It must be placed in front */
    long counter;
} fake_atomic_long_t;

#define COUNTER_OFFSET  sizeof(pthread_mutex_t)

static inline unsigned long *counter_entry(unsigned long *addr)
{
    return addr + COUNTER_OFFSET;
}

static inline void fake_atomic_init(fake_atomic_t *v, int val)
{
    pthread_mutex_init(&v->lock, NULL);
    v->counter = val;
}

static inline void fake_atomic_set(fake_atomic_long_t *v, int i)
{
    pthread_mutex_lock(&v->lock);
    v->counter = i;
    pthread_mutex_unlock(&v->lock);
}

static inline int fake_atomic_get(fake_atomic_long_t *v)
{
    return v->counter;
}


static inline void fake_atomic_inc(fake_atomic_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter++;
    pthread_mutex_unlock(&v->lock);
}

static inline void fake_atomic_dec(fake_atomic_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter--;
    pthread_mutex_unlock(&v->lock);
}


static inline void fake_atomic_add(int i, fake_atomic_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter += i;
    pthread_mutex_unlock(&v->lock);
}


static inline void fake_atomic_sub(int i, fake_atomic_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter -= i;
    pthread_mutex_unlock(&v->lock);
}


static inline int fake_atomic_inc_and_test(fake_atomic_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = (++(v->counter) == 0);
    pthread_mutex_unlock(&v->lock);

    return val;
}

static inline int fake_atomic_dec_and_test(fake_atomic_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = (--(v->counter) == 0);
    pthread_mutex_unlock(&v->lock);

    return val;
}

static inline int fake_atomic_inc_return(fake_atomic_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = ++(v->counter);
    pthread_mutex_unlock(&v->lock);

    return val;

}

static inline int fake_atomic_dec_return(fake_atomic_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = --(v->counter);
    pthread_mutex_unlock(&v->lock);

    return val;
}


static inline int fake_atomic_add_return(int i, fake_atomic_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = (v->counter += i);
    pthread_mutex_unlock(&v->lock);

    return val;
}

static inline int fake_atomic_sub_return(int i, fake_atomic_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = (v->counter -= i);
    pthread_mutex_unlock(&v->lock);

    return val;
}

/***********************************************************/

static inline void fake_atomic_long_init(fake_atomic_long_t *v, long val)
{
    pthread_mutex_init(&v->lock, NULL);
    v->counter = val;
}


static inline void fake_atomic_long_set(fake_atomic_long_t *v, long i)
{
    pthread_mutex_lock(&v->lock);
    v->counter = i;
    pthread_mutex_unlock(&v->lock);
}

static inline long fake_atomic_long_get(fake_atomic_long_t *v)
{
    return v->counter;
}


static inline void fake_atomic_long_inc(fake_atomic_long_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter++;
    pthread_mutex_unlock(&v->lock);
}

static inline void fake_atomic_long_dec(fake_atomic_long_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter--;
    pthread_mutex_unlock(&v->lock);
}


static inline void fake_atomic_long_add(long i, fake_atomic_long_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter += i;
    pthread_mutex_unlock(&v->lock);
}


static inline void fake_atomic_long_sub(long i, fake_atomic_long_t *v)
{
    pthread_mutex_lock(&v->lock);
    v->counter -= i;
    pthread_mutex_unlock(&v->lock);
}


static inline int fake_atomic_long_inc_and_test(fake_atomic_long_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = (++(v->counter) == 0);
    pthread_mutex_unlock(&v->lock);

    return val;
}

static inline int fake_atomic_long_dec_and_test(fake_atomic_long_t *v)
{
    int val;

    pthread_mutex_lock(&v->lock);
    val = (--(v->counter) == 0);
    pthread_mutex_unlock(&v->lock);

    return val;
}

static inline long fake_atomic_long_inc_return(fake_atomic_long_t *v)
{
    long val;

    pthread_mutex_lock(&v->lock);
    val = ++(v->counter);
    pthread_mutex_unlock(&v->lock);

    return val;

}

static inline long fake_atomic_long_dec_return(fake_atomic_long_t *v)
{
    long val;

    pthread_mutex_lock(&v->lock);
    val = --(v->counter);
    pthread_mutex_unlock(&v->lock);

    return val;
}


static inline long fake_atomic_long_add_return(long i, fake_atomic_long_t *v)
{
    long val;

    pthread_mutex_lock(&v->lock);
    val = (v->counter += i);
    pthread_mutex_unlock(&v->lock);

    return val;
}

static inline long fake_atomic_long_sub_return(long i, fake_atomic_long_t *v)
{
    long val;

    pthread_mutex_lock(&v->lock);
    val = (v->counter -= i);
    pthread_mutex_unlock(&v->lock);

    return val;
}


/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 *
 * Note: there are no guarantees that this function will not be reordered
 * on non x86 architectures, so if you are writing portable code,
 * make sure not to rely on its reordering guarantees.
 *
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void fake_set_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)counter_entry(addr)) + BIT_WORD(nr);
    pthread_mutex_t *lock = (pthread_mutex_t *)addr;

    pthread_mutex_lock(lock);
	*p  |= mask;
    pthread_mutex_unlock(lock);
}

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_clear_bit() and/or smp_mb__after_clear_bit()
 * in order to ensure changes are visible on other processors.
 */
static inline void fake_clear_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)counter_entry(addr)) + BIT_WORD(nr);
    pthread_mutex_t *lock = (pthread_mutex_t *)addr;

    pthread_mutex_lock(lock);
	*p &= ~mask;
    pthread_mutex_unlock(lock);
}

/**
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered. It may be
 * reordered on other architectures than x86.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void fake_change_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)counter_entry(addr)) + BIT_WORD(nr);
    pthread_mutex_t *lock = (pthread_mutex_t *)addr;

    pthread_mutex_lock(lock);
	*p ^= mask;
    pthread_mutex_unlock(lock);
}

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It may be reordered on other architectures than x86.
 * It also implies a memory barrier.
 */
static inline int fake_test_and_set_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)counter_entry(addr)) + BIT_WORD(nr);
	unsigned long old;
    pthread_mutex_t *lock = (pthread_mutex_t *)addr;

    pthread_mutex_lock(lock);
	old = *p;
	*p = old | mask;
    pthread_mutex_unlock(lock);

	return (old & mask) != 0;
}

/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It can be reorderdered on other architectures other than x86.
 * It also implies a memory barrier.
 */
static inline int fake_test_and_clear_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)counter_entry(addr)) + BIT_WORD(nr);
	unsigned long old;
    pthread_mutex_t *lock = (pthread_mutex_t *)addr;

    pthread_mutex_lock(lock);
	old = *p;
	*p = old & ~mask;
    pthread_mutex_unlock(lock);

	return (old & mask) != 0;
}

/**
 * test_and_change_bit - Change a bit and return its old value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int fake_test_and_change_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)counter_entry(addr)) + BIT_WORD(nr);
	unsigned long old;
    pthread_mutex_t *lock = (pthread_mutex_t *)addr;

    pthread_mutex_lock(lock);
	old = *p;
	*p = old ^ mask;
    pthread_mutex_unlock(lock);

	return (old & mask) != 0;
}

#endif
