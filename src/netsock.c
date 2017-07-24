/*
 * src/netsock.c
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <include/log.h>
#include <include/types.h>
#include <include/netsock.h>


struct netsock_operations *netsock_ops_list[NETSOCK_MAX] = {
    [NETSOCK_STREAM] = &stream_ops,
    [NETSOCK_DGRAM] = &dgram_ops,
};


/*
*description: initilaize the socket library
*
*@arg1: the socket initilaize input param structure.
*
*return: indentify the handle to the library
*/
void *netsock_init(struct netsock_args *args)
{
    struct netsock *nsock;

    if (!args) {
        loge("please input the correct parameters.\n");
        return NULL;
    }

    nsock = (struct netsock *)malloc(sizeof(struct netsock));
    if (!nsock)
        fatal("alloc the memory fail.\n");

    pthread_mutex_init(&nsock->r_lock, NULL);
    pthread_mutex_init(&nsock->s_lock, NULL);

    nsock->args = *args;
    nsock->netsock_ops = netsock_ops_list[args->type];

    nsock->netsock_ops->init(nsock);

    return nsock;
}

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
int netsock_recv(void *handle, _out void *buf, int len)
{
    int ret;
    struct netsock *nsock;

    if (!handle)
        return -EINVAL;

    nsock = (struct netsock *)handle;


    pthread_mutex_lock(&nsock->r_lock);

    ret = nsock->netsock_ops->recv(nsock, buf, len);

    pthread_mutex_unlock(&nsock->r_lock);

    return ret;
}


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
                         unsigned long timeout)
{
    int ret;
    struct netsock *nsock;

    if (!handle)
        return -EINVAL;

    nsock = (struct netsock *)handle;

    pthread_mutex_lock(&nsock->r_lock);

    ret = nsock->netsock_ops->recv_timeout(nsock, buf, len, timeout);

    pthread_mutex_unlock(&nsock->r_lock);

    return ret;
}



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
int netsock_send(void *handle, void *buf, int len)
{
    int ret;
    struct netsock *nsock;

    if (!handle)
        return -EINVAL;

    nsock = (struct netsock *)handle;

    pthread_mutex_lock(&nsock->s_lock);

    ret = nsock->netsock_ops->send(nsock, buf, len);

    pthread_mutex_unlock(&nsock->s_lock);

    return ret;
}


int netsock_send_by_session(void *handle, void *session, void *buf, int len)
{
    int ret;
    struct netsock *nsock;

    if ((!handle) || (!session))
        return -EINVAL;

    nsock = (struct netsock *)handle;

    pthread_mutex_lock(&nsock->s_lock);

    ret = nsock->netsock_ops->send_by_session(nsock, session, buf, len);

    pthread_mutex_unlock(&nsock->s_lock);
    return ret;
}

/*
*description:release the resource function.
*
*@arg1:indentify the handle to the library,
*	,netsock_init()function return pointer.
*
*return:void
*/
int netsock_reinit(void *handle, struct netsock_args *args)
{
    int ret;
    struct netsock *nsock;

    if (!handle || !args) {
        loge("please input the correct parameters.\n");
        return -EINVAL;
    }

    nsock = (struct netsock *)handle;

    pthread_mutex_lock(&nsock->s_lock);
    pthread_mutex_lock(&nsock->r_lock);

    nsock->netsock_ops->release(nsock);

    nsock->args = *args;
    nsock->netsock_ops = netsock_ops_list[args->type];

    ret = nsock->netsock_ops->init(nsock);

    pthread_mutex_unlock(&nsock->r_lock);
    pthread_mutex_unlock(&nsock->s_lock);

    return ret;
}


/*
*description:release the resource function.
*
*@arg1:indentify the handle to the library,
*	,netsock_init()function return pointer.
*
*return:void
*/
void netsock_release(void *handle)
{
    struct netsock *nsock;

    if (!handle)
        return;

    nsock = (struct netsock *)handle;

    pthread_mutex_lock(&nsock->s_lock);
    pthread_mutex_lock(&nsock->r_lock);

    nsock->netsock_ops->release(nsock);

    pthread_mutex_unlock(&nsock->r_lock);
    pthread_mutex_unlock(&nsock->s_lock);

    free(handle);
}


