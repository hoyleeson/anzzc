/*
 * include/ioasync.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_IOASYNC_H
#define _ANZZC_IOASYNC_H

#include <sys/socket.h>

#include "types.h"
#include "packet.h"
#include "poller.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct iohandler iohandler_t;
typedef struct ioasync ioasync_t;

#if 0
typedef void (*handle_func) (void *priv, uint8_t *data, int len);
typedef void (*handlefrom_func) (void *priv, uint8_t *data, int len,
                                 void *from);
typedef void (*accept_func) (void *priv, int acceptfd);
typedef void (*close_func) (void  *priv);
#endif

pack_buf_t *iohandler_pack_buf_alloc(iohandler_t *ioh);
void iohandler_pack_buf_free(pack_buf_t *pkb);


void iohandler_send(iohandler_t *ioh, const uint8_t *data, int len);
void iohandler_sendto(iohandler_t *ioh, const uint8_t *data, int len,
                      struct sockaddr *to);

void iohandler_pkt_send(iohandler_t *ioh, pack_buf_t *pkb);
void iohandler_pkt_sendto(iohandler_t *ioh, pack_buf_t *pkb,
                          struct sockaddr *to);

iohandler_t *iohandler_create(ioasync_t *aio, int fd,
                              void (*handle)(void *, uint8_t *, int), void (*close)(void *), void *priv);

iohandler_t *iohandler_accept_create(ioasync_t *aio, int fd,
                                     void (*accept)(void *, int), void (*close)(void *), void *priv);

iohandler_t *iohandler_udp_create(ioasync_t *aio, int fd,
                                  void (*handlefrom)(void *, uint8_t *, int, void *),
                                  void (*close)(void *), void *priv);

void iohandler_shutdown(iohandler_t *ioh);

ioasync_t *ioasync_init(void);
//void ioasync_loop(ioasync_t *aio);
void ioasync_release(ioasync_t *aio);

void global_ioasync_init(void);
void global_ioasync_release(void);
ioasync_t *get_global_ioasync(void);

#ifdef __cplusplus
}
#endif


#endif

