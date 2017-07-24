/*
 * include/netsock.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_NETSOCK_H
#define _ANZZC_NETSOCK_H

#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>

#include "types.h"


#ifdef __cplusplus
extern "C" {
#endif

/*socket connection type*/
enum netsock_type {
    NETSOCK_STREAM,		/* socket type is tcp */
    NETSOCK_DGRAM,		/* socket type is udp */
    NETSOCK_MAX,
};

/*connection session unit*/
struct connection {
    union {
        int sock;       /* used for tcp */
        struct sockaddr_in sock_addr;   /* used for udp */
    };
};


/*use for the error callback function*/
enum err_code {
    E_SOCKRECV,
    E_SOCKSELECT,
    E_SOCKTIMEOUT,
    E_SOCKLISTEN,
    E_SOCKCONNECT,
    E_UNKNOWN,
};

/*
 * when use the callback function get receive data,
* the recv_callback() return this structure.
*/
struct net_packet {
    void *data;		//data buf
    int datalen;	//data lenght
    struct connection conn;		//Identification data is of who sent
};

typedef int (*recv_callback)(struct net_packet *pack,
                             void *priv);	//receive data callback
typedef void (*err_callback)(int err_code, void *priv);		//error callback.


struct netsock_args {
    enum netsock_type type;	/*socket type, tcp or udp*/
    uint32_t is_server;		//1:server;	0:client
    uint32_t dest_ip;	//if client, this is server ip, otherwise into empty string
    uint32_t dest_port;		//destination port
    uint32_t listen_port;	//listen port

    uint32_t buf_size;	//receive buffer size.
    recv_callback recv_cb;
    err_callback err_cb;
    void *priv_data;
};

#define SOCKET_ARGS_INILIALIZER(_type, _is_serv, _dest_ip, _dest_port, \
								_listen_port, _buf_size, _recv_cb, _err_cb, _priv)	{	\
    .type        = _type,		\
    .is_server   = _is_serv,	\
    .dest_ip     = _dest_ip,	\
    .dest_port   = _dest_port,	\
    .listen_port = _listen_port,	\
    .buf_size    = _buf_size,	\
    .recv_cb     = _recv_cb,	\
    .err_cb      = _err_cb,	    \
    .priv_data   = _priv        \
}

#define DECLARE_SOCKET_ARGS(_name, _type, _is_serv, _dest_ip, _dest_port, \
							_listen_port, _buf_size, _recv_cb, _err_cb, _priv)	\
				struct netsock_args _name = \
					SOCKET_ARGS_INILIALIZER(_type, _is_serv, _dest_ip, _dest_port, _listen_port, _buf_size, recv_cb, _err_cb, _priv)


/*
* network library interface data structure.
*/
struct netsock {
    struct netsock_args args;
    struct netsock_operations *netsock_ops;
    void *private_data;

    pthread_mutex_t s_lock;
    pthread_mutex_t r_lock;
};

/*
* network library function interface.
*/
struct netsock_operations {
    int (*init)(struct netsock *nsock);
    void (*release)(struct netsock *nsock);
    int (*send)(struct netsock *nsock, void *buf, int len);
    int (*send_by_session)(struct netsock *nsock, void *session, void *buf,
                           int len);
    int (*recv)(struct netsock *nsock, _out void *buf, int len);
    int (*recv_timeout)(struct netsock *nsock, _out void *buf, int len,
                        unsigned long timeout);
};


/*
*description: initilaize the socket library
*
*@arg1: the socket initilaize input param structure.
*
*return: indentify the handle to the library
*/
void *netsock_init(struct netsock_args *args);


/*
*description:receive network data function.
*
*@arg1:indentify the handle to the library
*	,netsock_init()function return pointer.
*@arg2:(out)data buffer pointer.
*@arg3: need receive data lenght
*
*return: receive data len;<0: error.
*/
int netsock_recv(void *handle, _out void *buf, int len);

/*
*description:receive network data function.
*
*@arg1:indentify the handle to the library
*	,netsock_init()function return pointer.
*@arg2:(out)data buffer pointer.
*@arg3: need receive data lenght
*@arg4: timeout, units: millisecond.
*
*return: receive data len;=0:timeout;<0: error.
*/
int netsock_recv_timeout(void *handle, _out void *buf, int len,
                         unsigned long timeout);


/*
*description:call this function to send data to network.
*
*@arg1:indentify the handle to the library
*	,netsock_init()function return pointer.
*@arg2:data buffer pointer.
*@arg3:data lenght
*
*return: 0:send data success, -1: send data error.
*/
int netsock_send(void *handle, void *buf, int len);

/*
*description:reinitialize the resource function.
*
*@arg1:indentify the handle to the library,
*	,netsock_init()function return pointer.
*
*return:int
*/
int netsock_reinit(void *handle, struct netsock_args *args);

/*
*description:release the resource function.
*
*@arg1:indentify the handle to the library,
*	,netsock_init()function return pointer.
*
*return:void
*/
void netsock_release(void *handle);


extern struct netsock_operations stream_ops;
extern struct netsock_operations dgram_ops;

#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif
