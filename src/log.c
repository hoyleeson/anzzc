/*
 * src/log.c
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
#include <stdint.h>
#include <stdarg.h>

#include <include/log.h>


#ifndef ANDROID
static uint8_t curr_level = LOG_DEFAULT_LEVEL;

void log_init(int level)
{
    if(level > LOG_LEVEL_MAX || level < 0)
        return;

    curr_level = level;
}

char level_tags[LOG_LEVEL_MAX + 1] = {
    'F', 'E', 'W', 'I', 'D', 'V',
};

void log_printf(int level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    int len;
    char buf[LOG_BUF_SIZE];

    if(level > curr_level || level < 0)
        return;

    len = snprintf(buf, LOG_BUF_SIZE, "%c/[%s] ", level_tags[level], tag);
    va_start(ap, fmt);
    vsnprintf(buf + len, LOG_BUF_SIZE - len, fmt, ap);
    va_end(ap);

    printf("%s", buf);
    fflush(stdout);
}

#else

void log_init(int level)
{
}

#endif
