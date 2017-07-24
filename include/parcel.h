/*
 * include/parcel.h
 *
 * 2016-07-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */


#ifndef _ANZZC_PARCEL_H
#define _ANZZC_PARCEL_H

#include <stdint.h>
#include <stddef.h>

#include "types.h"

struct parcel {
    int error;
    uint8_t *data;
    size_t data_size;
    size_t data_pos;

    size_t data_capacity;

};

#ifdef __cplusplus
extern "C" {
#endif

int parcel_init(struct parcel *p);
void parcel_release(struct parcel *p);

/*reinitialize the parcel.*/
void parcel_clear(struct parcel *par);

/*get parcel data.*/
const uint8_t *parcel_data(struct parcel *par);

/*get parcel data size.*/
size_t parcel_datasize(struct parcel *par);

/*get parcel data position.*/
size_t parcel_data_position(struct parcel *par);

/*get the parcel data capacity.*/
size_t parcel_data_capacity(struct parcel *par);

/*set the parcel data size.*/
int parcel_set_data_size(struct parcel *par, size_t size);

/*set the parcel data position.*/
void parcel_set_data_pos(struct parcel *par, size_t pos);

/*set the parcel data capacity.*/
int parcel_set_data_capacity(struct parcel *par, size_t size);

/*set the parcel data.*/
int parcel_set_data(struct parcel *par, const uint8_t *buffer, size_t len);

void *parcel_write_inplace(struct parcel *par, size_t len);

int parcel_write(struct parcel *par, const void *data, size_t len);
int parcel_read(struct parcel *par, _out void *data, size_t len);

int parcel_write_string(struct parcel *par, const char *str);
const char *parcel_read_string(struct parcel *par);

int parcel_write_uint8(struct parcel *par, uint8_t val);
int parcel_write_uint16(struct parcel *par, uint16_t val);
int parcel_write_uint32(struct parcel *par, uint32_t val);
int parcel_write_uint64(struct parcel *par, uint64_t val);

int parcel_write_int8(struct parcel *par, int8_t val);
int parcel_write_int16(struct parcel *par, int16_t val);
int parcel_write_int32(struct parcel *par, int32_t val);
int parcel_write_int64(struct parcel *par, int64_t val);

int parcel_write_float(struct parcel *par, float val);
int parcel_write_double(struct parcel *par, double val);
int parcel_write_intptr(struct parcel *par, intptr_t val);
int parcel_write_uintptr(struct parcel *par, uintptr_t val);

uint8_t parcel_read_uint8(struct parcel *par);
uint16_t parcel_read_uint16(struct parcel *par);
uint32_t parcel_read_uint32(struct parcel *par);
uint64_t parcel_read_uint64(struct parcel *par);

int8_t parcel_read_int8(struct parcel *par);
int16_t parcel_read_int16(struct parcel *par);
int32_t parcel_read_int32(struct parcel *par);
int64_t parcel_read_int64(struct parcel *par);

float parcel_read_float(struct parcel *par);
double parcel_read_double(struct parcel *par);
intptr_t parcel_read_intptr(struct parcel *par);
uintptr_t parcel_read_uintptr(struct parcel *par);

#define DECLARE_PARCEL_RW(type)     \
int parcel_write_##type(struct parcel *par, type val); \
type parcel_read_##type(struct parcel *par);

#ifdef __cplusplus
}
#endif

#endif
