/*
 * src/daemon.c
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <include/log.h>

int enter_daemon(void)
{
    int pid;
    int fd;

    switch(pid = fork()) {
        case -1:
            /* error */
            loge("fork failed.\n");
            return -EINVAL;
        case 0:
            /* child, success */
            break;
        default:
            /* parent, success */
            logi("forked to background, child pid %d.\n", pid);
            exit(0);
            break;
    }

    if(setsid() == -1) {
        loge("setsid failed.\n");
        return -EINVAL;
    }

    umask(0);

    fd = open("/dev/null", 0);
    if (fd == -1) {
        loge("can't open /dev/null.\n");
        return -EINVAL;
    }

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    if (fd > STDERR_FILENO) {
        close(fd);
    }

    return 0;
}
