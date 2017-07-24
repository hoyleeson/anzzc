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
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#include <include/log.h>

static unsigned long rotate_limit_len = 0;

static FILE *log_stream = NULL;  /* Only use for LOG_MODE_FILE */
static char log_path[PATH_MAX] = {0};  /* Only use for LOG_MODE_FILE */
static unsigned long log_length = 0;
static void (*log_cbprint)(int, const char *) = NULL;
static pthread_mutex_t log_rotate_lock = PTHREAD_MUTEX_INITIALIZER;

static uint8_t log_level = LOG_DEFAULT_LEVEL;
static enum logger_mode log_mode = LOG_MODE_QUIET;


static char level_tags[LOG_LEVEL_MAX + 1] = {
    'F', 'E', 'W', 'I', 'D', 'V',
};

static const char *log_mode_str[LOG_MODE_MAX + 1] = {
    [LOG_MODE_QUIET] = "quiet",
    [LOG_MODE_STDOUT] = "stdout",
    [LOG_MODE_FILE] = "file",
    [LOG_MODE_CLOUD] = "cloud",
    [LOG_MODE_CALLBACK] = "callback",
};


__attribute__((weak)) const char *get_log_path(void)
{
    return LOG_PATH;
}

void default_cbprint(int level, const char *log)
{
    fputc(level_tags[level], stdout);
    fputc(':', stdout);
    fputs(log, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

void log_set_loglevel(int level)
{
    if (log_level != level)
        log_level = level;
}

void log_set_logpath(const char *path)
{
    int len = 0;
    if (log_stream)
        fclose(log_stream);

    log_stream = fopen(path, "a+");
    if (!log_stream) {
        log_mode = LOG_MODE_STDOUT;
        return;
    }
    len = snprintf(log_path, PATH_MAX - 1, "%s", path);
    log_path[len] = 0;
    logv("log file:%s", log_path);
}

void log_set_rotate_limit(int len)
{
    rotate_limit_len = len;
}

void log_set_callback(void (*cb)(int, const char *))
{
    log_cbprint = cb;
}

static void log_rotate(void)
{
    FILE *log_stream_old;
    char path[PATH_MAX] = {0};

    pthread_mutex_lock(&log_rotate_lock);
    if (log_length < rotate_limit_len) {
        pthread_mutex_unlock(&log_rotate_lock);
        return;
    }
    log_stream_old = log_stream;

    snprintf(path, PATH_MAX - 1, "%s.old", log_path);
    if (rename(log_path, path) < 0) {
        pthread_mutex_unlock(&log_rotate_lock);
        return;
    }

    log_stream = fopen(log_path, "a+");
    if (!log_stream) {
        log_stream = log_stream_old;
        pthread_mutex_unlock(&log_rotate_lock);
        return;
    }
    log_length = 0;
    fclose(log_stream_old);

    pthread_mutex_unlock(&log_rotate_lock);
}

static void increase_log_len(int len)
{
    if (!rotate_limit_len)
        return;

    log_length += len;

    if (log_length > rotate_limit_len) {
        log_rotate();
    }
}

void log_print(int level, const char *tag,
               const char *func, int line, const char *fmt, ...)
{
    va_list ap;
    int len;
    time_t now;
    char timestr[32];
    char buf[LOG_BUF_SIZE];
    int loglen = 0;

    if (level > log_level || level < 0)
        return;

    time(&now);
    strftime(timestr, 32, "%Y-%m-%d %H:%M:%S", localtime(&now));

    len = snprintf(buf, LOG_BUF_SIZE, "%s (%s)/[%c] <%s:%d> ",
                   timestr, tag, level_tags[level], func, line);
    loglen += len;
    va_start(ap, fmt);
    loglen += vsnprintf(buf + len, LOG_BUF_SIZE - len, fmt, ap);
    va_end(ap);

    if (log_mode == LOG_MODE_FILE) {
        increase_log_len(loglen);
        fputs(buf, log_stream);
        fflush(log_stream);
    } else if (log_mode == LOG_MODE_CLOUD) {
        /* TODO: */
    }  else if (log_mode == LOG_MODE_CALLBACK) {
        if (log_cbprint)
            log_cbprint(level, buf + len);
    } else {
        fputs(buf, stdout);
        fflush(stdout);
    }
}

int log_init(enum logger_mode mode, enum logger_level level)
{
    log_mode = mode;
    log_level = level;

    log_length = 0;
    if (log_level > LOG_LEVEL_MAX || log_level < 0)
        log_level = DEFAULT_LOG_LEVEL;

    if (log_mode >= LOG_MODE_MAX || log_mode < 0)
        log_mode = DEFAULT_LOG_MODE;

    if (log_mode == LOG_MODE_FILE && !log_stream) {
        int l;
        l = snprintf(log_path, PATH_MAX - 1, "%s", get_log_path());
        log_path[l] = '\0';
        log_stream = fopen(log_path, "a+");
        if (!log_stream) {
            log_mode = LOG_MODE_STDOUT;
        }

        if (fseek(log_stream, 0, SEEK_END) == -1) {
            /* Nothing */
        }
        log_length = ftell(log_stream);

        rotate_limit_len = LOG_DEFAULT_ROTATE_LIMIT;
    } else if (log_mode == LOG_MODE_CALLBACK && !log_cbprint) {
        log_cbprint = default_cbprint;
    }

    logi("log init. mode:%d, level:%d\n",
         log_mode_str[mode], level_tags[log_level]);
    return 0;
}

void log_release(void)
{
    if (log_mode == LOG_MODE_FILE) {
        fclose(log_stream);
    }
}

