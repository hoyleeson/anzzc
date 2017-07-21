/*
 * include/log.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_LOG_H
#define _ANZZC_LOG_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_TAG
#define LOG_TAG 	"anzzc"
#endif

#define LOG_PATH 		"anzzc.log"

#define DEFAULT_LOG_LEVEL 		LOG_INFO
#define DEFAULT_LOG_MODE 		LOG_MODE_STDOUT

enum logger_mode {
	LOG_MODE_QUIET = 0,
	LOG_MODE_STDOUT,
	LOG_MODE_FILE,
	LOG_MODE_CLOUD,
	LOG_MODE_CALLBACK, 
	LOG_MODE_MAX,
};

enum logger_level {
	LOG_FATAL = 0,
	LOG_ERROR,
	LOG_WARNING,
	LOG_INFO,
	LOG_DEBUG,
	LOG_VERBOSE,
	LOG_LEVEL_MAX,
};

#define LOG_DEFAULT_LEVEL 	LOG_INFO
#define LOG_BUF_SIZE 		(4096)
#define LOG_DEFAULT_ROTATE_LIMIT    (8*1024*1024)

#define LOGV(...)   log_print(LOG_VERBOSE, LOG_TAG, __func__, __LINE__, __VA_ARGS__)
#define LOGD(...)   log_print(LOG_DEBUG, LOG_TAG, __func__, __LINE__, __VA_ARGS__)
#define LOGI(...)   log_print(LOG_INFO, LOG_TAG, __func__, __LINE__, __VA_ARGS__)
#define LOGW(...)   log_print(LOG_WARNING, LOG_TAG, __func__, __LINE__, __VA_ARGS__)
#define LOGE(...)   log_print(LOG_ERROR, LOG_TAG, __func__, __LINE__, __VA_ARGS__)


#define logprint(...)      printf(__VA_ARGS__)

#define logi(...) 		LOGI(__VA_ARGS__)
#define logw(...) 		LOGW(__VA_ARGS__)
#define loge(...) 		LOGE(__VA_ARGS__)

#define fatal(...) 		do { loge(__VA_ARGS__); exit(-1); } while(0)

#define panic(...) 		fatal(__VA_ARGS__);

void dump_stack(void);

void log_set_loglevel(int level);
/* only use for LOG_MODE_FILE mode */
void log_set_logpath(const char *path);
/* only use for LOG_MODE_FILE mode */
void log_set_rotate_limit(int len);
/* only use for LOG_MODE_CALLBACK mode */
void log_set_callback(void (*cb)(int, const char *));

void log_print(int level, const char *tag, const char* func, int line, const char *fmt, ...);
int log_init(enum logger_mode mode, enum logger_level level);
void log_release(void);


#ifdef VDEBUG
#define logv(...) 		LOGV(__VA_ARGS__)

static inline void dump_data(const char *desc, void *data, int len) 
{
    int i;
    uint8_t *p = (uint8_t *)data;

    logprint("[%s]dump data(%d):\n", desc, len);
    for(i=0; i<len; i++) {
        if((i % 16) == 0)
            logprint("\n");
        logprint("%02x ", *(p + i));
    }

    logprint("\n");
}

#ifndef DDEBUG
#define DDEBUG
#endif

#else
#define logv(...)

static inline void dump_data(const char *desc, void *data, int len) 
{
}

#endif

#ifdef DEBUG
//#define DDEBUG
#endif

#ifdef DDEBUG
#define logd(...) 		LOGD(__VA_ARGS__)
#else
#define logd(...)
#endif


#ifdef __cplusplus
}
#endif


/*XXX*/
#define xxxx    logi("--------------%s:%d------------", __func__, __LINE__);

#endif

