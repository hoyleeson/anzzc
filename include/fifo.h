/*
 * include/fifo.h
 *
 * Copyright (C) 2013 Stefani Seibold <stefani@seibold.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * A generic kernel FIFO implementation
 *
 */

#ifndef _ANZZC_FIFO_H
#define _ANZZC_FIFO_H

#include <pthread.h>

#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * How to porting drivers to the new generic FIFO API:
 *
 * - Modify the declaration of the "struct fifo *" object into a
 *   in-place "struct fifo" object
 * - Init the in-place object with fifo_alloc() or fifo_init()
 *   Note: The address of the in-place "struct fifo" object must be
 *   passed as the first argument to this functions
 * - Replace the use of __fifo_put into fifo_in and __fifo_get
 *   into fifo_out
 * - Replace the use of fifo_put into fifo_in_mutex and fifo_get
 *   into fifo_out_mutex
 *   Note: the mutex lock pointer formerly passed to fifo_init/fifo_alloc
 *   must be passed now to the fifo_in_mutex and fifo_out_mutex
 *   as the last parameter
 * - The formerly __fifo_* functions are renamed into fifo_*
 */

/*
 * Note about locking : There is no locking required until only * one reader
 * and one writer is using the fifo and no fifo_reset() will be * called
 *  fifo_reset_out() can be safely used, until it will be only called
 * in the reader thread.
 *  For multiple writer and one reader there is only a need to lock the writer.
 * And vice versa for only one writer and multiple reader there is only a need
 * to lock the reader.
 */

struct __fifo {
    unsigned int	in;
    unsigned int	out;
    unsigned int	mask;
    unsigned int	esize;
    void		*data;
};

#define __STRUCT_FIFO_COMMON(datatype, recsize, ptrtype) \
	union { \
		struct __fifo	fifo; \
		datatype	*type; \
		char		(*rectype)[recsize]; \
		ptrtype		*ptr; \
		const ptrtype	*ptr_const; \
	}

#define __STRUCT_FIFO(type, size, recsize, ptrtype) \
{ \
	__STRUCT_FIFO_COMMON(type, recsize, ptrtype); \
	type		buf[((size < 2) || (size & (size - 1))) ? -1 : size]; \
}

#define STRUCT_FIFO(type, size) \
	struct __STRUCT_FIFO(type, size, 0, type)

#define __STRUCT_FIFO_PTR(type, recsize, ptrtype) \
{ \
	__STRUCT_FIFO_COMMON(type, recsize, ptrtype); \
	type		buf[0]; \
}

#define STRUCT_FIFO_PTR(type) \
	struct __STRUCT_FIFO_PTR(type, 0, type)

/*
 * define compatibility "struct fifo" for dynamic allocated fifos
 */
struct fifo __STRUCT_FIFO_PTR(unsigned char, 0, void);

#define STRUCT_FIFO_REC_1(size) \
	struct __STRUCT_FIFO(unsigned char, size, 1, void)

#define STRUCT_FIFO_REC_2(size) \
	struct __STRUCT_FIFO(unsigned char, size, 2, void)

/*
 * define fifo_rec types
 */
struct fifo_rec_ptr_1 __STRUCT_FIFO_PTR(unsigned char, 1, void);
struct fifo_rec_ptr_2 __STRUCT_FIFO_PTR(unsigned char, 2, void);

/*
 * helper macro to distinguish between real in place fifo where the fifo
 * array is a part of the structure and the fifo type where the array is
 * outside of the fifo structure.
 */
#define	__is_fifo_ptr(fifo)	(sizeof(*fifo) == sizeof(struct __fifo))

/**
 * DECLARE_FIFO_PTR - macro to declare a fifo pointer object
 * @fifo: name of the declared fifo
 * @type: type of the fifo elements
 */
#define DECLARE_FIFO_PTR(fifo, type)	STRUCT_FIFO_PTR(type) fifo

/**
 * DECLARE_FIFO - macro to declare a fifo object
 * @fifo: name of the declared fifo
 * @type: type of the fifo elements
 * @size: the number of elements in the fifo, this must be a power of 2
 */
#define DECLARE_FIFO(fifo, type, size)	STRUCT_FIFO(type, size) fifo

/**
 * INIT_FIFO - Initialize a fifo declared by DECLARE_FIFO
 * @fifo: name of the declared fifo datatype
 */
#define INIT_FIFO(fifo) \
(void)({ \
	typeof(&(fifo)) __tmp = &(fifo); \
	struct __fifo *__fifo = &__tmp->fifo; \
	__fifo->in = 0; \
	__fifo->out = 0; \
	__fifo->mask = __is_fifo_ptr(__tmp) ? 0 : ARRAY_SIZE(__tmp->buf) - 1;\
	__fifo->esize = sizeof(*__tmp->buf); \
	__fifo->data = __is_fifo_ptr(__tmp) ?  NULL : __tmp->buf; \
})

/**
 * DEFINE_FIFO - macro to define and initialize a fifo
 * @fifo: name of the declared fifo datatype
 * @type: type of the fifo elements
 * @size: the number of elements in the fifo, this must be a power of 2
 *
 * Note: the macro can be used for global and local fifo data type variables.
 */
#define DEFINE_FIFO(fifo, type, size) \
	DECLARE_FIFO(fifo, type, size) = \
	(typeof(fifo)) { \
		{ \
			{ \
			.in	= 0, \
			.out	= 0, \
			.mask	= __is_fifo_ptr(&(fifo)) ? \
				  0 : \
				  ARRAY_SIZE((fifo).buf) - 1, \
			.esize	= sizeof(*(fifo).buf), \
			.data	= __is_fifo_ptr(&(fifo)) ? \
				NULL : \
				(fifo).buf, \
			} \
		} \
	}


#define __must_check		__attribute__((warn_unused_result))

static inline unsigned int __must_check
__fifo_uint_must_check_helper(unsigned int val)
{
    return val;
}

static inline int __must_check
__fifo_int_must_check_helper(int val)
{
    return val;
}

/**
 * fifo_initialized - Check if the fifo is initialized
 * @fifo: address of the fifo to check
 *
 * Return %true if fifo is initialized, otherwise %false.
 * Assumes the fifo was 0 before.
 */
#define fifo_initialized(fifo) ((fifo)->fifo.mask)

/**
 * fifo_esize - returns the size of the element managed by the fifo
 * @fifo: address of the fifo to be used
 */
#define fifo_esize(fifo)	((fifo)->fifo.esize)

/**
 * fifo_recsize - returns the size of the record length field
 * @fifo: address of the fifo to be used
 */
#define fifo_recsize(fifo)	(sizeof(*(fifo)->rectype))

/**
 * fifo_size - returns the size of the fifo in elements
 * @fifo: address of the fifo to be used
 */
#define fifo_size(fifo)	((fifo)->fifo.mask + 1)

/**
 * fifo_reset - removes the entire fifo content
 * @fifo: address of the fifo to be used
 *
 * Note: usage of fifo_reset() is dangerous. It should be only called when the
 * fifo is exclusived locked or when it is secured that no other thread is
 * accessing the fifo.
 */
#define fifo_reset(fifo) \
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	__tmp->fifo.in = __tmp->fifo.out = 0; \
})

/**
 * fifo_reset_out - skip fifo content
 * @fifo: address of the fifo to be used
 *
 * Note: The usage of fifo_reset_out() is safe until it will be only called
 * from the reader thread and there is only one concurrent reader. Otherwise
 * it is dangerous and must be handled in the same way as fifo_reset().
 */
#define fifo_reset_out(fifo)	\
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	__tmp->fifo.out = __tmp->fifo.in; \
})

/**
 * fifo_len - returns the number of used elements in the fifo
 * @fifo: address of the fifo to be used
 */
#define fifo_len(fifo) \
({ \
	typeof((fifo) + 1) __tmpl = (fifo); \
	__tmpl->fifo.in - __tmpl->fifo.out; \
})

/**
 * fifo_is_empty - returns true if the fifo is empty
 * @fifo: address of the fifo to be used
 */
#define	fifo_is_empty(fifo) \
({ \
	typeof((fifo) + 1) __tmpq = (fifo); \
	__tmpq->fifo.in == __tmpq->fifo.out; \
})

/**
 * fifo_is_full - returns true if the fifo is full
 * @fifo: address of the fifo to be used
 */
#define	fifo_is_full(fifo) \
({ \
	typeof((fifo) + 1) __tmpq = (fifo); \
	fifo_len(__tmpq) > __tmpq->fifo.mask; \
})

/**
 * fifo_avail - returns the number of unused elements in the fifo
 * @fifo: address of the fifo to be used
 */
#define	fifo_avail(fifo) \
__fifo_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmpq = (fifo); \
	const size_t __recsize = sizeof(*__tmpq->rectype); \
	unsigned int __avail = fifo_size(__tmpq) - fifo_len(__tmpq); \
	(__recsize) ? ((__avail <= __recsize) ? 0 : \
	__fifo_max_r(__avail - __recsize, __recsize)) : \
	__avail; \
}) \
)

/**
 * fifo_skip - skip output data
 * @fifo: address of the fifo to be used
 */
#define	fifo_skip(fifo) \
(void)({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (__recsize) \
		__fifo_skip_r(__fifo, __recsize); \
	else \
		__fifo->out++; \
})

/**
 * fifo_peek_len - gets the size of the next fifo record
 * @fifo: address of the fifo to be used
 *
 * This function returns the size of the next fifo record in number of bytes.
 */
#define fifo_peek_len(fifo) \
__fifo_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	(!__recsize) ? fifo_len(__tmp) * sizeof(*__tmp->type) : \
	__fifo_len_r(__fifo, __recsize); \
}) \
)

/**
 * fifo_alloc - dynamically allocates a new fifo buffer
 * @fifo: pointer to the fifo
 * @size: the number of elements in the fifo, this must be a power of 2
 *
 * This macro dynamically allocates a new fifo buffer.
 *
 * The numer of elements will be rounded-up to a power of 2.
 * The fifo will be release with fifo_free().
 * Return 0 if no error, otherwise an error code.
 */
#define fifo_alloc(fifo, size) \
__fifo_int_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	struct __fifo *__fifo = &__tmp->fifo; \
	__is_fifo_ptr(__tmp) ? \
	__fifo_alloc(__fifo, size, sizeof(*__tmp->type)) : \
	-EINVAL; \
}) \
)

/**
 * fifo_free - frees the fifo
 * @fifo: the fifo to be freed
 */
#define fifo_free(fifo) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (__is_fifo_ptr(__tmp)) \
		__fifo_free(__fifo); \
})

/**
 * fifo_init - initialize a fifo using a preallocated buffer
 * @fifo: the fifo to assign the buffer
 * @buffer: the preallocated buffer to be used
 * @size: the size of the internal buffer, this have to be a power of 2
 *
 * This macro initialize a fifo using a preallocated buffer.
 *
 * The numer of elements will be rounded-up to a power of 2.
 * Return 0 if no error, otherwise an error code.
 */
#define fifo_init(fifo, buffer, size) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	struct __fifo *__fifo = &__tmp->fifo; \
	__is_fifo_ptr(__tmp) ? \
	__fifo_init(__fifo, buffer, size, sizeof(*__tmp->type)) : \
	-EINVAL; \
})

/**
 * fifo_put - put data into the fifo
 * @fifo: address of the fifo to be used
 * @val: the data to be added
 *
 * This macro copies the given value into the fifo.
 * It returns 0 if the fifo was full. Otherwise it returns the number
 * processed elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	fifo_put(fifo, val) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof((val) + 1) __val = (val); \
	unsigned int __ret; \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (0) { \
		typeof(__tmp->ptr_const) __dummy __attribute__ ((unused)); \
		__dummy = (typeof(__val))NULL; \
	} \
	if (__recsize) \
		__ret = __fifo_in_r(__fifo, __val, sizeof(*__val), \
			__recsize); \
	else { \
		__ret = !fifo_is_full(__tmp); \
		if (__ret) { \
			(__is_fifo_ptr(__tmp) ? \
			((typeof(__tmp->type))__fifo->data) : \
			(__tmp->buf) \
			)[__fifo->in & __tmp->fifo.mask] = \
				*(typeof(__tmp->type))__val; \
			__fifo->in++; \
		} \
	} \
	__ret; \
})

/**
 * fifo_get - get data from the fifo
 * @fifo: address of the fifo to be used
 * @val: the var where to store the data to be added
 *
 * This macro reads the data from the fifo.
 * It returns 0 if the fifo was empty. Otherwise it returns the number
 * processed elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	fifo_get(fifo, val) \
__fifo_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof((val) + 1) __val = (val); \
	unsigned int __ret; \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (0) \
		__val = (typeof(__tmp->ptr))0; \
	if (__recsize) \
		__ret = __fifo_out_r(__fifo, __val, sizeof(*__val), \
			__recsize); \
	else { \
		__ret = !fifo_is_empty(__tmp); \
		if (__ret) { \
			*(typeof(__tmp->type))__val = \
				(__is_fifo_ptr(__tmp) ? \
				((typeof(__tmp->type))__fifo->data) : \
				(__tmp->buf) \
				)[__fifo->out & __tmp->fifo.mask]; \
			__fifo->out++; \
		} \
	} \
	__ret; \
}) \
)

/**
 * fifo_peek - get data from the fifo without removing
 * @fifo: address of the fifo to be used
 * @val: the var where to store the data to be added
 *
 * This reads the data from the fifo without removing it from the fifo.
 * It returns 0 if the fifo was empty. Otherwise it returns the number
 * processed elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	fifo_peek(fifo, val) \
__fifo_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof((val) + 1) __val = (val); \
	unsigned int __ret; \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (0) \
		__val = (typeof(__tmp->ptr))NULL; \
	if (__recsize) \
		__ret = __fifo_out_peek_r(__fifo, __val, sizeof(*__val), \
			__recsize); \
	else { \
		__ret = !fifo_is_empty(__tmp); \
		if (__ret) { \
			*(typeof(__tmp->type))__val = \
				(__is_fifo_ptr(__tmp) ? \
				((typeof(__tmp->type))__fifo->data) : \
				(__tmp->buf) \
				)[__fifo->out & __tmp->fifo.mask]; \
		} \
	} \
	__ret; \
}) \
)

/**
 * fifo_in - put data into the fifo
 * @fifo: address of the fifo to be used
 * @buf: the data to be added
 * @n: number of elements to be added
 *
 * This macro copies the given buffer into the fifo and returns the
 * number of copied elements.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	fifo_in(fifo, buf, n) \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof((buf) + 1) __buf = (buf); \
	unsigned long __n = (n); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (0) { \
		typeof(__tmp->ptr_const) __dummy __attribute__ ((unused)); \
		__dummy = (typeof(__buf))NULL; \
	} \
	(__recsize) ?\
	__fifo_in_r(__fifo, __buf, __n, __recsize) : \
	__fifo_in(__fifo, __buf, __n); \
})

/**
 * fifo_in_mutex - put data into the fifo using a mutex lock for locking
 * @fifo: address of the fifo to be used
 * @buf: the data to be added
 * @n: number of elements to be added
 * @lock: pointer to the mutex lock to use for locking
 *
 * This macro copies the given values buffer into the fifo and returns the
 * number of copied elements.
 */
#define	fifo_in_mutex(fifo, buf, n, lock) \
({ \
	unsigned int __ret; \
	pthread_mutex_lock(lock); \
	__ret = fifo_in(fifo, buf, n); \
	pthread_mutex_unlock(lock); \
	__ret; \
})

/* alias for fifo_in_mutex, will be removed in a future release */
#define fifo_in_locked(fifo, buf, n, lock) \
		fifo_in_mutex(fifo, buf, n, lock)

/**
 * fifo_out - get data from the fifo
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 *
 * This macro get some data from the fifo and return the numbers of elements
 * copied.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	fifo_out(fifo, buf, n) \
__fifo_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof((buf) + 1) __buf = (buf); \
	unsigned long __n = (n); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (0) { \
		typeof(__tmp->ptr) __dummy = NULL; \
		__buf = __dummy; \
	} \
	(__recsize) ?\
	__fifo_out_r(__fifo, __buf, __n, __recsize) : \
	__fifo_out(__fifo, __buf, __n); \
}) \
)

/**
 * fifo_out_mutex - get data from the fifo using a mutex lock for locking
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 * @lock: pointer to the mutex lock to use for locking
 *
 * This macro get the data from the fifo and return the numbers of elements
 * copied.
 */
#define	fifo_out_mutex(fifo, buf, n, lock) \
__fifo_uint_must_check_helper( \
({ \
	unsigned int __ret; \
	pthread_mutex_lock(lock); \
	__ret = fifo_out(fifo, buf, n); \
	pthread_mutex_unlock(lock); \
	__ret; \
}) \
)

/* alias for fifo_out_mutex, will be removed in a future release */
#define fifo_out_locked(fifo, buf, n, lock) \
		fifo_out_mutex(fifo, buf, n, lock)

/**
 * fifo_out_peek - gets some data from the fifo
 * @fifo: address of the fifo to be used
 * @buf: pointer to the storage buffer
 * @n: max. number of elements to get
 *
 * This macro get the data from the fifo and return the numbers of elements
 * copied. The data is not removed from the fifo.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	fifo_out_peek(fifo, buf, n) \
__fifo_uint_must_check_helper( \
({ \
	typeof((fifo) + 1) __tmp = (fifo); \
	typeof((buf) + 1) __buf = (buf); \
	unsigned long __n = (n); \
	const size_t __recsize = sizeof(*__tmp->rectype); \
	struct __fifo *__fifo = &__tmp->fifo; \
	if (0) { \
		typeof(__tmp->ptr) __dummy __attribute__ ((unused)) = NULL; \
		__buf = __dummy; \
	} \
	(__recsize) ? \
	__fifo_out_peek_r(__fifo, __buf, __n, __recsize) : \
	__fifo_out_peek(__fifo, __buf, __n); \
}) \
)

extern int __fifo_alloc(struct __fifo *fifo, unsigned int size, size_t esize);

extern void __fifo_free(struct __fifo *fifo);

extern int __fifo_init(struct __fifo *fifo, void *buffer,
                       unsigned int size, size_t esize);

extern unsigned int __fifo_in(struct __fifo *fifo,
                              const void *buf, unsigned int len);

extern unsigned int __fifo_out(struct __fifo *fifo,
                               void *buf, unsigned int len);

extern unsigned int __fifo_out_peek(struct __fifo *fifo,
                                    void *buf, unsigned int len);

extern unsigned int __fifo_in_r(struct __fifo *fifo,
                                const void *buf, unsigned int len, size_t recsize);

extern unsigned int __fifo_out_r(struct __fifo *fifo,
                                 void *buf, unsigned int len, size_t recsize);

extern unsigned int __fifo_len_r(struct __fifo *fifo, size_t recsize);

extern void __fifo_skip_r(struct __fifo *fifo, size_t recsize);

extern unsigned int __fifo_out_peek_r(struct __fifo *fifo,
                                      void *buf, unsigned int len, size_t recsize);

extern unsigned int __fifo_max_r(unsigned int len, size_t recsize);

#ifdef __cplusplus
}
#endif


#endif
