/*
 * include/bsearch.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * A generic implementation of binary search
 *
 */

#ifndef _ANZZC_BSEARCH_H
#define _ANZZC_BSEARCH_H

#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


void *bsearch(const void *key, const void *base, size_t num, size_t size,
        int (*cmp)(const void *key, const void *elt));

#define BSEARCH_MATCH_UP 		(0)
#define BSEARCH_MATCH_DOWN 		(1)

void *bsearch_edge(const void *key, const void *base, size_t num, size_t size, int edge,
        int (*cmp)(const void *key, const void *elt));

#ifdef __cplusplus
}
#endif


#endif

