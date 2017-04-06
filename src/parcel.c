/*
 * src/parcel.c
 * 
 * 2016-07-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <include/parcel.h>
#include <include/bug.h>
#include <include/core.h>

/* initilize the parcel. */
void __parcel_init(struct parcel *par)
{
    par->error = 0;
    par->data = NULL;
    par->data_size = 0;
    par->data_capacity = 0;
    par->data_pos = 0;
}


int parcel_init(struct parcel *par)
{
	__parcel_init(par);
	return 0;
}

void parcel_release(struct parcel *par)
{
	if (par->data)
		free((void*)par->data);
}

/* reinitialize the parcel. */
void parcel_clear(struct parcel *par)
{
	__parcel_init(par);
}

/* get parcel data. */
const uint8_t* parcel_data(struct parcel *par)
{
	return par->data;
}

/* get parcel data size. */
size_t parcel_datasize(struct parcel *par)
{
	return (par->data_size > par->data_pos ? par->data_size : par->data_pos);
}

/* get parcel data position. */
size_t parcel_data_position(struct parcel *par)
{
	return par->data_pos;
}

/* get the parcel data capacity. */
size_t parcel_data_capacity(struct parcel *par)
{
	return par->data_capacity;
}

static int parcel_restart_write(struct parcel *par, size_t desired)
{
    uint8_t* data = (uint8_t*)realloc(par->data, desired);

    if(!data && desired > par->data_capacity) {
        par->error = -ENOMEM;
        return -ENOMEM;
    }

    if(data) {
        par->data = data;
        par->data_capacity = desired;
    }

    par->data_size = par->data_pos = 0;

    return 0;
}

/* if the data memory not enough, realloc memory. */
static int parcel_continue_write(struct parcel *par, size_t desired)
{
    if(par->data) {
        // We own the data, so we can just do a realloc().
        if(desired > par->data_capacity) {
            uint8_t* data = (uint8_t*)realloc(par->data, desired);

            if(data) {
                par->data = data;
                par->data_capacity = desired;
            } else if(desired > par->data_capacity) {
                par->error = -ENOMEM;
                return -ENOMEM;
            }

        } else {
            par->data_size = desired;

            if(par->data_pos > desired) {
                par->data_pos = desired;
            }
        }
    } else {
        // This is the first data.  Easy!
        uint8_t* data = (uint8_t*)malloc(desired);

        if (!data) {
            par->error = -ENOMEM;
            return -ENOMEM;
        }

        par->data = data;
        par->data_size = par->data_pos = 0;

        par->data_capacity = desired;
    }

    return 0;
}

static int parcel_finish_write(struct parcel *par, size_t len)
{
    par->data_pos += len;

    if (par->data_pos > par->data_size) {
        par->data_size = par->data_pos;
    }

    return 0;
}

/* 
   if the data memory not enough, realloc memory.
   call continue_write() finish the work
   */
static int parcel_grow_data(struct parcel *par, size_t len)
{
    size_t new_size = ((par->data_size + len)*3)/2;
    return (new_size <= par->data_size)
        ?  -ENOMEM : parcel_continue_write(par, new_size);
}


/* set the parcel data size. */
int parcel_set_data_size(struct parcel *par, size_t size)
{
	int err;
	if(size == 0 && par->data_size!= 0) {
        __parcel_init(par);
		return 0;
	}

	err = parcel_continue_write(par, size);
	if(err == 0) {
		par->data_size = size;
	}
	return err;
}

/* set the parcel data position. */
void parcel_set_data_pos(struct parcel *par, size_t pos)
{
	par->data_pos = pos;
}

/* set the parcel data capacity. */
int parcel_set_data_capacity(struct parcel *par, size_t size)
{
	if(size > par->data_size)
		return parcel_continue_write(par, size);

	return 0;
}

/* set the parcel data. */
int parcel_set_data(struct parcel *par, const uint8_t* buffer, size_t len)
{
	if(len == 0) {
        __parcel_init(par);
		return 0;
	}

    int err = parcel_restart_write(par, len);
    if (err == 0) {
        memcpy(par->data, buffer, len);
        par->data_size = len;
    }
    return err;
}

void* parcel_write_inplace(struct parcel *par, size_t len)
{
    int err;
	const size_t padded = ALIGN(len, 4);

	if(padded + par->data_pos < par->data_pos) {
		return NULL;
	}

	if(padded + par->data_pos <= par->data_capacity) {
        uint8_t* data;
restart_write:
        data = par->data + par->data_pos;
		if(padded != len) {
#if BYTE_ORDER == BIG_ENDIAN
            static uint32_t mask[4] = {
                0x00000000, 0xffffff00, 0xffff0000, 0xff000000
            };
#elif BYTE_ORDER == LITTLE_ENDIAN
            static uint32_t mask[4] = {
                0x00000000, 0x00ffffff, 0x0000ffff, 0x000000ff
            };
#endif
            *(uint32_t*)(data+padded-4) &= mask[padded-len];
        }
		
		parcel_finish_write(par, padded);
		return data;
	}

	err = parcel_grow_data(par, padded);

	if(err == 0)
		goto restart_write;

	return NULL;
}

#define parcel_write_aligned(par, val)      \
({                                          \
    int __ret = 0;                          \
    typeof(&(val)) __val;                   \
    static_assert(ALIGN(sizeof(val), 4) == sizeof(val));    \
                                    \
    while((par->data_pos + sizeof(val)) > par->data_capacity) {    \
        __ret = parcel_grow_data(par, sizeof(val));       \
        if (__ret != 0)             \
		    break;                  \
    }                               \
                                    \
    if(!__ret) {                    \
        __val = (typeof(&(val)))(par->data + par->data_pos); \
        *__val = val;                       \
        __ret = parcel_finish_write(par, sizeof(val));    \
    }           \
    __ret;      \
})



#define parcel_read_aligned(par, val)  \
({                          \
    static_assert(ALIGN(sizeof(val), 4) == sizeof(val));   \
    int __ret = -EINVAL;    \
    if ((par->data_pos + sizeof(val)) <= par->data_size) { \
        void* data = par->data + par->data_pos;         \
        par->data_pos += sizeof(val);                   \
        val = *(typeof(val) *)(data);                   \
                            \
        __ret = 0;          \
    }                       \
    __ret;                  \
})

#define DEFINE_PARCEL_RW(type)     \
int parcel_write_##type(struct parcel *par, type val) \
{       \
    return parcel_write_aligned(par, val);          \
}       \
        \
type parcel_read_##type(struct parcel *par)   \
{       \
    type result;        \
        \
    if (parcel_read_aligned(par, result) != 0) {    \
        result = 0;     \
    }   \
    return result;      \
}

int parcel_write(struct parcel *par, const void* data, size_t len)
{
    void* d = parcel_write_inplace(par, len);
    if(d) {
        memcpy(d, data, len);
        return 0;
    }
    return par->error;
}

int parcel_read(struct parcel *par, _out void* data, size_t len)
{
    if ((par->data_pos + ALIGN(len, 4)) >= par->data_pos && 
            (par->data_pos + ALIGN(len, 4)) <= par->data_size) {
        memcpy(data, par->data + par->data_pos, len);
        par->data_pos += ALIGN(len, 4);

        return 0;
    }
    return -EINVAL;
}

int parcel_write_string(struct parcel *par, const char* str)
{
    return parcel_write(par, str, strlen(str)+1);
}

const char* parcel_read_string(struct parcel *par)
{
    size_t avail = par->data_size - par->data_pos;

    if (avail > 0) {
        char* str = (char *)(par->data + par->data_pos);
        /* is the string's trailing NUL within the parcel's valid bounds? */
        char* eos = (char *)(memchr(str, 0, avail));
        if (eos) {
            size_t len = eos - str;
            par->data_pos += ALIGN(len+1, 4);
            return str;
        }
    }
    return NULL;
}

int parcel_write_uint8(struct parcel *par, uint8_t val)
{
    return parcel_write(par, &val, 1);
}

int parcel_write_uint16(struct parcel *par, uint16_t val)
{
    return parcel_write(par, &val, 2);
}

int parcel_write_uint32(struct parcel *par, uint32_t val)
{
    return parcel_write_aligned(par, val);
}

int parcel_write_uint64(struct parcel *par, uint64_t val)
{
    return parcel_write_aligned(par, val);
}


int parcel_write_int8(struct parcel *par, int8_t val)
{
    return parcel_write(par, &val, 1);
}

int parcel_write_int16(struct parcel *par, int16_t val)
{
    return parcel_write(par, &val, 2);
}

int parcel_write_int32(struct parcel *par, int32_t val)
{
    return parcel_write_aligned(par, val);
}

int parcel_write_int64(struct parcel *par, int64_t val)
{
    return parcel_write_aligned(par, val);
}


int parcel_write_float(struct parcel *par, float val)
{
    return parcel_write_aligned(par, val);
}

int parcel_write_double(struct parcel *par, double val)
{
    return parcel_write_aligned(par, val);
}


int parcel_write_intptr(struct parcel *par, intptr_t val)
{
    return parcel_write_aligned(par, val);
}

int parcel_write_uintptr(struct parcel *par, uintptr_t val)
{
    return parcel_write_aligned(par, val);
}


uint8_t parcel_read_uint8(struct parcel *par)
{
    uint8_t val;

    par->error = parcel_read(par, &val, 1);
    return val;
}

uint16_t parcel_read_uint16(struct parcel *par)
{
    uint16_t val;

    par->error = parcel_read(par, &val, 2);
    return val;
}

uint32_t parcel_read_uint32(struct parcel *par)
{
    uint32_t result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}

uint64_t parcel_read_uint64(struct parcel *par)
{
    uint64_t result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}


int8_t parcel_read_int8(struct parcel *par)
{
    int8_t val;

    par->error = parcel_read(par, &val, 1);
    return val;
}

int16_t parcel_read_int16(struct parcel *par)
{
    int16_t val;

    par->error = parcel_read(par, &val, 2);
    return val;
}

int32_t parcel_read_int32(struct parcel *par)
{
    int32_t result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}

int64_t parcel_read_int64(struct parcel *par)
{
    int64_t result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}


float parcel_read_float(struct parcel *par)
{
    float result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}

double parcel_read_double(struct parcel *par)
{
    double result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}


intptr_t parcel_read_intptr(struct parcel *par)
{
    intptr_t result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}

uintptr_t parcel_read_uintptr(struct parcel *par)
{
    uintptr_t result;

    if (parcel_read_aligned(par, result) != 0) {
        result = 0;
    }
    return result;
}



