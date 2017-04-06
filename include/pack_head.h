/*
 * include/pack_head.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_PACK_HEAD_H
#define _ANZZC_PACK_HEAD_H

#include <stdint.h>

#define PROTOS_MAGIC        (0x2016)	
#define PROTOS_VERSION      (1)

typedef struct _pack_header {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t seqnum;
    uint8_t chsum;
    uint8_t _reserved1;
    uint32_t datalen;
    uint8_t data[0];
}__attribute__((packed)) pack_head_t;


#define pack_head_len() 	sizeof(pack_head_t)

#ifdef __cplusplus
extern "C" {
#endif


pack_head_t *create_pack(uint8_t type, uint32_t len);
void init_pack(pack_head_t *pack, uint8_t type, uint32_t len);
void free_pack(pack_head_t *pack);


#ifdef __cplusplus
}
#endif


#endif

