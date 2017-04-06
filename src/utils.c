/*
 * src/utils.c
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <include/log.h>


/** UTILITIES
 **/

void* xalloc(size_t sz)
{
    void* p;

    if(sz == 0)
        return NULL;

    p = malloc(sz);
    if(p == NULL)
        fatal("not enough memory");

    return p;
}

void* xzalloc(size_t sz)
{
    void* p = xalloc(sz);
    memset(p, 0, sz);
    return p;
}

void* xrealloc(void* block, size_t size)
{
    void* p = realloc(block, size);

    if(p == NULL && size > 0)
        fatal("not enough memory");

    return p;
}

int hexdigit(int c)
{
    unsigned d;

    d = (unsigned)(c - '0');
    if (d < 10) return d;

    d = (unsigned)(c - 'a');
    if (d < 6) return d+10;

    d = (unsigned)(c - 'A');
    if (d < 6) return d+10;

    return -1;
}

int hex2int(const uint8_t* data, int len)
{
    int  result = 0;
    while (len > 0) {
        int c = *data++;
        unsigned  d;

        result <<= 4;
        do {
            d = (unsigned)(c - '0');
            if (d < 10)
                break;

            d = (unsigned)(c - 'a');
            if (d < 6) {
                d += 10;
                break;
            }

            d = (unsigned)(c - 'A');
            if (d < 6) {
                d += 10;
                break;
            }

            return -1;
        }
        while(0);

        result |= d;
        len    -= 1;
    }
    return result;
}

void int2hex(int value, uint8_t* to, int width)
{
    int  nn = 0;
    static const char hexchars[16] = "0123456789abcdef";

    for (--width; width >= 0; width--, nn++) {
        to[nn] = hexchars[(value >> (width*4)) & 15];
    }
}

int xread(int fd, void* to, int len)
{
    int ret;

    do {
        ret = read(fd, to, len);
    } while(ret < 0 && errno == EINTR);

    return ret;
}

int xwrite(int fd, const void* from, int len)
{
    int ret;

    do {
        ret = write(fd, from, len);
    } while(ret < 0 && errno == EINTR);

    return ret;
}

void setnonblock(int fd)
{
    int ret;
    int flags;

    do {
        flags = fcntl(fd, F_GETFL);
    } while (flags < 0 && errno == EINTR);

    if (flags < 0) {
        fatal("%s: could not get flags for fd %d: %s",
                __func__, fd, strerror(errno));
    }

    do {
        ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        fatal("%s: could not set fd %d to non-blocking: %s",
                __func__, fd, strerror(errno));
    }
}


int xaccept(int fd)
{
    int ret;
    struct sockaddr from;
    socklen_t fromlen = sizeof(from);

    do {
        ret = accept(fd, &from, &fromlen);
    } while(ret < 0 && errno == EINTR);

    return ret;
}




/*
 * gettime() - returns the time in seconds of the system's monotonic clock or
 * zero on error.
 */
time_t gettime(void)
{
#if 0
    struct timespec ts;
    int ret;

    ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret < 0) {
        loge("clock_gettime(CLOCK_MONOTONIC) failed: %s\n", strerror(errno));
        return 0;
    }

    return ts.tv_sec;
#else
    return time(NULL);
#endif
}


/* reads a file, making sure it is terminated with \n \0 */
void *read_file(const char *fname, unsigned *_sz)
{
    char *data;
    int sz;
    int fd;
    struct stat sb;

    data = 0;
    fd = open(fname, O_RDONLY);
    if(fd < 0)
        return 0;

    // for security reasons, disallow world-writable
    // or group-writable files
    if (fstat(fd, &sb) < 0) {
        loge("fstat failed for '%s'\n", fname);
        goto oops;
    }
    if ((sb.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        loge("skipping insecure file '%s'\n", fname);
        goto oops;
    }

    sz = lseek(fd, 0, SEEK_END);
    if(sz < 0)
        goto oops;

    if(lseek(fd, 0, SEEK_SET) != 0) 
        goto oops;

    data = (char*) malloc(sz + 2);
    if(data == 0)
        goto oops;

    if(read(fd, data, sz) != sz) 
        goto oops;

    close(fd);
    data[sz] = '\n';
    data[sz+1] = 0;
    if(_sz)
        *_sz = sz;

    return data;

oops:
    close(fd);
    if(data != 0)
        free(data);
    return 0;
}

