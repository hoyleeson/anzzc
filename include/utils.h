/*
 * include/utils.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_UTILS_H
#define _ANZZC_UTILS_H

#include <stdint.h>
#include <time.h>

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void* xalloc(size_t   sz);
void* xzalloc(size_t  sz);
void* xrealloc(void*  block, size_t  size);

int hexdigit( int  c );
int hex2int(const uint8_t*  data, int  len);
void int2hex(int  value, uint8_t*  to, int  width);
int xread(int  fd, void*  to, int  len);
int xwrite(int  fd, const void*  from, int  len);
void setnonblock(int  fd);
int xaccept(int  fd);

void *read_file(const char *fn, unsigned *_sz);
time_t gettime(void);

#define  xnew(p)   do { (p) = xalloc(sizeof(*(p))); } while(0)
#define  xznew(p)   do { (p) = xzalloc(sizeof(*(p))); } while(0)
#define  xfree(p)    do { (free((p)), (p) = NULL); } while(0)
#define  xrenew(p,count)  do { (p) = xrealloc((p),sizeof(*(p))*(count)); } while(0)

#ifdef __cplusplus
}
#endif


#endif

