/*
 * src/sock_stream.c
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
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <include/log.h>
#include <include/netsock.h>

#define CONNECT_TIMEOUT	    (5)
#define MAXPENDING			(1)

/*tcp data infomation structure.*/
struct sock_stream {
    int sock;
    /* listen address */
    struct sockaddr_in sock_addr; 
};

struct process_param {
    int sock;
    struct netsock* owner;
};


static int stream_connect(struct netsock* nsock);
static int stream_listen(struct netsock* nsock);
static void run_recv_process(struct netsock* nsock, int sock);
static void* stream_listen_thread(void* args);

/**
 * @brief   stream_init
 * 
 * initialize the tcp trans module. 
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static int stream_init(struct netsock* nsock)
{
    int ret = 0;
    int optval = 0;

    struct sock_stream* stream;

    stream = (struct sock_stream*)malloc(sizeof(struct sock_stream));	
    if(!stream)
        fatal("alloc tcp info struct fail.\n");

    memset(stream, 0, sizeof(*stream));
    stream->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (stream->sock == -1) {
		loge("create stream socket failed. ret is %d\n", stream->sock);
        ret = -EINVAL;
        goto failed;
    }

    bzero(&stream->sock_addr, sizeof(struct sockaddr_in));
    stream->sock_addr.sin_family = AF_INET;

    if((optval = fcntl(stream->sock, F_GETFL, 0)) < 0) {
        ret = -EINVAL;
        goto failed;
    }

    if(fcntl(stream->sock, F_SETFL, optval & (~O_NONBLOCK))< 0) {
        ret = -EINVAL;
        goto failed;
    }

    if(nsock->args.is_server) {
        stream->sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        stream->sock_addr.sin_port = htons(nsock->args.listen_port);	
        stream_listen(nsock);	//listen port

        if(nsock->args.recv_cb) {
            logw("WARNING:receive data used callback is recommended.\n");
        }
    } else {
        stream->sock_addr.sin_addr.s_addr = nsock->args.dest_ip;
        stream->sock_addr.sin_port = htons(nsock->args.dest_port);	

        stream_connect(nsock);	//connection the server.
    }

    nsock->private_data = stream;

    logi("tcp init success.\n");
    return 0;

failed:
    free(stream);
    return ret;
}


/**
 * @brief   stream_listen
 * 
 * Create stream_listen thread. 
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static int stream_listen(struct netsock* nsock)
{
    pthread_t thread;

    return pthread_create(&thread, NULL, stream_listen_thread, nsock);
}


/**
 * @brief   stream_connect
 * 
 * client call this function connect the server 
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static int stream_connect(struct netsock* nsock)
{
    int ret;
    int flags;
    int result;
    struct timeval tm;
    fd_set c_fds;
    struct sock_stream* stream = nsock->private_data;

    ret = -1;	

    if((flags = fcntl(stream->sock, F_GETFL, 0)) < 0)
        return -EINVAL;

    if(fcntl(stream->sock, F_SETFL, flags | O_NONBLOCK)< 0)
        return -EINVAL;

    result = connect(stream->sock, (struct sockaddr*)(&stream->sock_addr),
            sizeof(stream->sock_addr));

    if(result == 0) {
        logi("connect server success.\n");
        ret = 0;
    } else {
        if(errno == EINPROGRESS) {
            tm.tv_sec  = CONNECT_TIMEOUT;
            tm.tv_usec = 0;
            FD_ZERO(&c_fds);
            FD_SET(stream->sock, &c_fds);

            int n = select(stream->sock+1, 0, &c_fds, 0, &tm);

            if(n == -1 && errno!= EINTR) {
                logw("server select error\n");			
            } else if(n == 0) {
                logw("connect server timeout\n");
            } else if(n > 0) {               
                int optval;
                int optlen = 4;
                if(getsockopt(stream->sock, SOL_SOCKET, SO_ERROR, (void*)&optval, (socklen_t*)&optlen) < 0) 
                    loge("tcp getsockopt fail!\n");

                if(optval == 0) {
                    ret=0;
                    logi("server success select\n");
                } else {
                    logw("failed select %d:%s in the server\n", optval, strerror(optval));
                }
            } 
        } else {
            close(stream->sock);
            logw("connect server fail\n");
        }
    }

    if(fcntl(stream->sock, F_SETFL, flags)< 0)
        return -EINVAL;

    /* receive data use for callback. */
    if(nsock->args.recv_cb) {
        run_recv_process(nsock, stream->sock);
    }

    return ret;
}


/**
 * @brief   stream_recv
 * 
 * tcp receive data function.
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static int stream_recv(struct netsock* nsock, void* data, int len)
{
    int ret;
    struct sock_stream* stream = nsock->private_data;

    if(nsock->args.is_server) {
        loge("server recv data using the callback instead of this.\n.");
        return -EINVAL;
    }

    ret = recv(stream->sock, data, len, 0);

    /* If the other party closed socket, will also own a socket closed */
    if(ret <= 0) {
        close(stream->sock);
    }

    return ret;
}


/**
 * @brief   stream_recv_timeout
 * 
 * tcp receive data function.
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static int stream_recv_timeout(struct netsock* nsock, void* data, int len, unsigned long ms)
{
    int nready;
    fd_set rset;
    struct timeval timeout;
    int ret = -EINVAL;
    struct sock_stream* stream = nsock->private_data;

    if(nsock->args.is_server) {
        loge("server recv data using the callback instead of this.\n.");
        return -EINVAL;
    }

    FD_ZERO(&rset);
    FD_SET(stream->sock, &rset);

    timeout.tv_sec = ms / 1000;
    timeout.tv_usec = ms % 1000;

    nready = select(stream->sock + 1, &rset, NULL, NULL, &timeout);
    if(nready < 0) {
        return -1;
    } else if(nready == 0) {
        return 0;
    } else {
        ret = recv(stream->sock, data, len, 0);

        /* If the other party closed socket, will also own a socket closed */
        if(ret <= 0) {
            close(stream->sock);
        }
    }
    return ret;
}


/**
 * @brief   stream_send
 * 
 * stream_send
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1: send data.
 * @param[in] arg1: send lenght.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static int stream_send(struct netsock* nsock, void* data, int len)
{
    int ret;
    struct sock_stream* stream = nsock->private_data;

    ret = send(stream->sock, data, len, 0);

    return (ret<0) ? -EINVAL : 0;
}


/**
 * @brief   stream_send_by_session
 * 
 * when the library to be call by the server, 
 * use this function send data. reserved method.
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1: send data.
 * @param[in] arg1: send lenght.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static int stream_send_by_session(struct netsock *nsock, void *session, void *buf, int len)
{
    int ret;
    struct connection* conn = (struct connection*)session;

    ret = send(conn->sock, buf, len, 0);

    return (ret<0) ? -EINVAL : 0;
}


/**
 * @brief   stream_release
 * 
 * call this function destory the resource.
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static void stream_release(struct netsock* nsock)
{
    struct sock_stream* stream = nsock->private_data;

    close(stream->sock);

    free(stream);
}


/**
 * @brief   process_connection
 * 
 * process_connection.
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static void* process_connection(void* arg)
{
    int running = 1;
    int nready;
    struct process_param* param;
    fd_set fds;

    struct timeval timeout;
    struct net_packet pack;
    struct netsock *nsock;

    param = (struct process_param*)arg;
    nsock = param->owner;

    pack.data = malloc(nsock->args.buf_size);
    logd("process connection running. recv socket is %d.\n", param->sock);

    while(running) {
        FD_ZERO(&fds);
        FD_SET(param->sock, &fds); 
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        nready = select(param->sock+1, &fds, NULL, NULL, &timeout);

        if(nready<=0)  {
            usleep(1000);
            continue;
        } else {
            if ((pack.datalen = recv(param->sock, pack.data, nsock->args.buf_size, 0)) <= 0) {
                close(param->sock);
                FD_CLR(param->sock, &fds);
                param->sock = -1;
                break;
            }
            pack.conn.sock = param->sock;

            if(nsock->args.recv_cb)
                nsock->args.recv_cb(&pack, nsock->args.priv_data);
        }
    }

    logd("tcp socket %d close.\n", param->sock);
    if(param->sock > 0)
        close(param->sock);

    return 0;
}


static void run_recv_process(struct netsock* nsock, int sock)
{
    int ret;
    pthread_t recv_thd;
    struct process_param *p;

    p = (struct process_param*)malloc(sizeof(struct process_param));
    p->sock = sock;
    p->owner = nsock;

    ret = pthread_create(&recv_thd, NULL, process_connection, p);
	if(ret) {
		loge("create the recv thread error!\n");
	}
}


/**
 * @brief   stream_listen_thread
 * 
 * stream_listen_thread.
 * @author hoyleeson
 * @date 2012-06-26
 * @param[in] arg1:input infomation structure.
 * @return int return success or failed
 * @retval returns zero on success
 * @retval return a non-zero error code if failed
 */
static void* stream_listen_thread(void* args)
{
    int ret;
    int cli_sock;
    struct sockaddr_in cli_addr;
    socklen_t cli_len;
    struct netsock* nsock;
    struct sock_stream* stream;

    nsock = (struct netsock*)args;
    stream = (struct sock_stream*)nsock->private_data;


    stream->sock_addr.sin_addr.s_addr =htonl(INADDR_ANY); /* Any incoming interface */

    ret = bind(stream->sock, (struct sockaddr* )&stream->sock_addr, sizeof cli_addr);
    if(ret < 0)
        return 0;

    ret = listen(stream->sock, MAXPENDING);
    if(ret < 0)
        return 0;

    for( ; ; )
    {
        cli_len = sizeof cli_addr;

        cli_sock = accept(stream->sock, (struct sockaddr*) &cli_addr, &cli_len);

        logd("accept new client. fd = %d\n", cli_sock);
        run_recv_process(nsock, cli_sock);
    }

    return 0;
}


struct netsock_operations stream_ops = {
    .init       =  stream_init,
    .release    =  stream_release,
    .send       =  stream_send,
    .send_by_session  =  stream_send_by_session,
    .recv       =  stream_recv,
    .recv_timeout = stream_recv_timeout,
};


