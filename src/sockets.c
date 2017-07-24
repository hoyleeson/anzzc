/*
 * src/sockets.c
 *
 * Copyright 2006, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <netdb.h>

#include <include/sockets.h>

#ifndef HAVE_WINSOCK
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define LISTEN_BACKLOG 4

/* open listen() port on any interface */
int socket_inaddr_any_server(int port, int type)
{
    struct sockaddr_in addr;
    int s, n;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, type, 0);
    if (s < 0) return -1;

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }

    if (type == SOCK_STREAM) {
        int ret;

        ret = listen(s, LISTEN_BACKLOG);

        if (ret < 0) {
            close(s);
            return -1;
        }
    }

    return s;
}


/* Connect to port on the IP interface. type is
 * SOCK_STREAM or SOCK_DGRAM.
 * return is a file descriptor or -1 on error
 */
int socket_network_client(const char *host, int port, int type)
{
    struct hostent *hp;
    struct sockaddr_in addr;
    int s;

    hp = gethostbyname(host);
    if (hp == 0) {
        if (-1 == (s = socket(AF_INET, SOCK_STREAM, 0))) {
            return -1;
        }
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(host);
        memset(&addr.sin_zero, 0, 8);

        if (-1 == connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr))) {
            close(s);
            return -1;
        }
        return s;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);

    s = socket(hp->h_addrtype, type, 0);
    if (s < 0) return -1;

    if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }

    return s;

}

