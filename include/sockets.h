/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ANZZC_SOCKETS_H
#define _ANZZC_SOCKETS_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <config.h>

#ifdef HAVE_WINSOCK
#include <winsock2.h>
typedef int  socklen_t;
#elif HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int socket_network_client(const char *host, int port, int type);
int socket_inaddr_any_server(int port, int type);

#ifdef __cplusplus
}
#endif

#endif /* __CUTILS_SOCKETS_H */
