/*
 * src/sock_dgram.c
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
#include <pthread.h>
#include <arpa/inet.h>

#include <include/log.h>
#include <include/netsock.h>

struct sock_dgram {
    int sock;
    /* listen address */
    struct sockaddr_in recv_addr;
    /* send address */
    struct sockaddr_in send_addr;

    int recv_running;
};

static void run_recv_process(struct netsock *nsock);


/**
* @brief   dgram_init
*
* initialize the udp trans module.
* @author hoyleeson
* @date 2012-06-26
* @param[in] arg1:input infomation structure.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
static int dgram_init(struct netsock *nsock)
{
    int ret = 0;
    int optval;
    struct sock_dgram *dgram;

    dgram = (struct sock_dgram *)malloc(sizeof(struct sock_dgram));
    if (!dgram)
        fatal("alloc sock dgram failed.\n");

    memset(dgram, 0, sizeof(*dgram));
    if ((dgram->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        loge("create dgram socket failed. ret is %d\n", dgram->sock);
        ret = -EINVAL;
        goto failed;
    }

    optval = 1;
    if (setsockopt(dgram->sock, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval, sizeof(int)) < 0) {
        loge("dgram sock setsockopt failed.\n");
        ret = -EINVAL;
        goto failed;
    }

    memset(&dgram->recv_addr, 0, sizeof(struct sockaddr_in));
    dgram->recv_addr.sin_family = AF_INET;
    dgram->recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dgram->recv_addr.sin_port = htons(nsock->args.listen_port);

    /*bind the socket*/
    if (bind(dgram->sock, (struct sockaddr *)&dgram->recv_addr,
             sizeof (dgram->recv_addr)) < 0) {
        loge("recv_sock udp bind failed.\n");
        ret = -EINVAL;
        goto failed;
    }

    memset(&dgram->send_addr, 0, sizeof(struct sockaddr_in));
    dgram->send_addr.sin_family = AF_INET;
    dgram->send_addr.sin_addr.s_addr = nsock->args.dest_ip;
    dgram->send_addr.sin_port = htons(nsock->args.dest_port);

    nsock->private_data = dgram;

    if (nsock->args.recv_cb) {
        dgram->recv_running = 1;
        run_recv_process(
            nsock);	//start the receive thread when use callback receive the data.
    } else if (nsock->args.is_server) {
        logw("WARNING:receive data used callback is recommended.\n");
    }

    logi("udp init success.\n");
    return 0;

failed:
    free(dgram);
    return ret;
}


/*udp library receive data function*/
/**
* @brief   dgram_recv
*
* udp receive data function.
* @author hoyleeson
* @date 2012-06-26
* @param[in] arg1:input infomation structure.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
static int dgram_recv(struct netsock *nsock, void *data, int len)
{
    int ret;
    struct sock_dgram *dgram = nsock->private_data;
    socklen_t cli_len = sizeof(struct sockaddr_in);

    if (nsock->args.is_server) {
        loge("server recv data using the callback instead of this.\n.");
        return -EINVAL;
    }

    ret = recvfrom(dgram->sock, data, len, 0, (struct sockaddr *)&dgram->recv_addr,
                   &cli_len);

    dgram->send_addr.sin_addr.s_addr = dgram->recv_addr.sin_addr.s_addr;//temp

    return ret;
}


/*udp library receive data function*/
/**
* @brief   dgram_recv_timeout
*
* udp receive data function.
* @author hoyleeson
* @date 2012-06-26
* @param[in] arg1:input infomation structure.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
static int dgram_recv_timeout(struct netsock *nsock, void *data, int len,
                              unsigned long ms)
{
    int nready;
    fd_set rset;
    struct timeval timeout;
    int ret = -EINVAL;
    struct sock_dgram *dgram = nsock->private_data;
    socklen_t cli_len = sizeof(struct sockaddr_in);

    if (nsock->args.is_server) {
        loge("server recv data using the callback instead of this.\n.");
        return -EINVAL;
    }

    ret = -1;

    FD_ZERO(&rset);
    FD_SET(dgram->sock, &rset);

    timeout.tv_sec = ms / 1000;
    timeout.tv_usec = (ms % 1000) * 1000;

    nready = select(dgram->sock + 1, &rset, NULL, NULL, &timeout);
    if (nready < 0) {
        return -EINVAL;
    } else if (nready == 0) {
        return 0;
    } else {
        ret = recvfrom(dgram->sock, data, len, 0, (struct sockaddr *)&dgram->recv_addr,
                       &cli_len);
        dgram->send_addr.sin_addr.s_addr = dgram->recv_addr.sin_addr.s_addr;//temp
    }

    return ret;
}


/**
* @brief   dgram_recv_thread
*
* udp receive thread.
* @author hoyleeson
* @date 2012-06-26
* @param[in] arg1:input infomation structure.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
static void *dgram_recv_thread(void *arg)
{
    int nready;
    int err = 0;
    socklen_t cli_len = sizeof(struct sockaddr_in);
    struct net_packet pack;
    int buf_len;
    struct timeval timeout = {5, 0};
    struct netsock *nsock = (struct netsock *)arg;
    struct sock_dgram *dgram = nsock->private_data;

    fd_set rset;
    pack.data = malloc(nsock->args.buf_size);

    while (dgram->recv_running) {
        FD_ZERO(&rset);
        FD_SET(dgram->sock, &rset);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        nready = select(dgram->sock + 1, &rset, NULL, NULL, &timeout);
        if (nready < 0) {
            if (nsock->args.err_cb)
                nsock->args.err_cb(E_SOCKSELECT, nsock->args.priv_data);
            break;
        } else if (nready == 0) {
            err++;
            if (err > 5 && nsock->args.err_cb) {
                nsock->args.err_cb(E_SOCKTIMEOUT, nsock->args.priv_data);
                break;
            }
            continue;
        } else if (FD_ISSET(dgram->sock, &rset)) {
            memset(pack.data, 0, sizeof(nsock->args.buf_size));
            if ((buf_len = recvfrom(dgram->sock, pack.data, nsock->args.buf_size, 0,
                                    (struct sockaddr *)&dgram->recv_addr, &cli_len)) < 0) {
                if (nsock->args.err_cb)
                    nsock->args.err_cb(E_SOCKRECV, nsock->args.priv_data);
                break;
            } else {	//recv data success.
                logd("receive udp data, size=(%d)\n", buf_len);

                pack.datalen = buf_len;
                pack.conn.sock_addr = dgram->recv_addr;

                if (nsock->args.recv_cb)
                    nsock->args.recv_cb(&pack, nsock->args.priv_data);	//call the callback function.
            }
        }
    }

    free(pack.data);
    return 0;
}


/*
*start the receive data thread.
*/
static void run_recv_process(struct netsock *nsock)
{
    int ret;
    pthread_t recv_th;

    ret = pthread_create(&recv_th, NULL, dgram_recv_thread, nsock);
    if (ret) {
        loge("create the recv thread error!\n");
        exit(EXIT_FAILURE);
    }
}


/**
* @brief   dgram_send
*
* dgram_send
* @author hoyleeson
* @date 2012-06-26
* @param[in] arg1: send data.
* @param[in] arg1: send lenght.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
static int dgram_send(struct netsock *nsock, void *data, int len)
{
    struct sock_dgram *dgram = nsock->private_data;

    if (sendto(dgram->sock, data, len, 0, (struct sockaddr *)&dgram->send_addr,
               sizeof(struct sockaddr_in)) < 0)
        return -EINVAL;

    return 0;
}


/**
* @brief   dgram_send_by_session
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
int dgram_send_by_session(struct netsock *nsock, void *session, void *buf,
                          int len)
{
    struct sock_dgram *dgram = nsock->private_data;
    struct connection *conn = (struct connection *)session;

    if (sendto(dgram->sock, buf, len, 0, (struct sockaddr *)&conn->sock_addr,
               sizeof(struct sockaddr_in)) < 0)
        return -EINVAL;

    return 0;
}


/**
* @brief   dgram_release
*
* call this function destory the resource.
* @author hoyleeson
* @date 2012-06-26
* @param[in] arg1:input infomation structure.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
static void dgram_release(struct netsock *nsock)
{
    struct sock_dgram *dgram = nsock->private_data;

    close(dgram->sock);
    free(dgram);
}


struct netsock_operations dgram_ops = {
    .init       = dgram_init,
    .release    = dgram_release,
    .send       = dgram_send,
    .send_by_session  = dgram_send_by_session,
    .recv       = dgram_recv,
    .recv_timeout = dgram_recv_timeout,
};


