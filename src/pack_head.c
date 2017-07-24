/*
 * src/pack_head.c
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

#include <include/log.h>
#include <include/pack_head.h>


pack_head_t *create_pack(uint8_t type, uint32_t len)
{
    pack_head_t *pack;
    pack = (pack_head_t *)malloc(sizeof(*pack) + len);
    if (!pack)
        return NULL;

    pack->magic = PROTOS_MAGIC;
    pack->version = PROTOS_VERSION;

    pack->type = type;
    pack->datalen = len;
    return pack;
}

void init_pack(pack_head_t *pack, uint8_t type, uint32_t len)
{
    pack->magic = PROTOS_MAGIC;
    pack->version = PROTOS_VERSION;

    pack->type = type;
    pack->datalen = len;
}


void free_pack(pack_head_t *pack)
{
    free(pack);
}

