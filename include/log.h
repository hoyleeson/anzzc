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

#ifndef ANDROID

#define LOG_FATAL 		(0)
#define LOG_ERROR 		(1)
#define LOG_WARNING 	(2)
#define LOG_INFO 		(3)
#define LOG_DEBUG 		(4)
#define LOG_VERBOSE 	(5)
#define LOG_LEVEL_MAX  	LOG_VERBOSE

#define LOG_DEFAULT_LEVEL 	LOG_VERBOSE

#define LOG_BUF_SIZE 	(1024)

void log_printf(int level, const char *tag, const char *fmt, ...);

#define LOG_PRINT(l, ...) 	log_printf(l, LOG_TAG, __VA_ARGS__)

#define LOGV(...)   LOG_PRINT(LOG_VERBOSE, __VA_ARGS__)
#define LOGD(...)   LOG_PRINT(LOG_DEBUG, __VA_ARGS__)
#define LOGI(...)   LOG_PRINT(LOG_INFO, __VA_ARGS__)
#define LOGW(...)   LOG_PRINT(LOG_WARNING, __VA_ARGS__)
#define LOGE(...)   LOG_PRINT(LOG_ERROR, __VA_ARGS__)

#define logprint(...)      printf(__VA_ARGS__)

#else /* #else ANDROID */

#include "jni.h"
#include <android/log.h>

#define LOGV(...)   __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...)   __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGI(...)   __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...)   __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...)   __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define logprint(...)      LOGI(__VA_ARGS__)

#endif  /* #endif ANDROID */


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

#define logi(...) 		LOGI(__VA_ARGS__)
#define logw(...) 		LOGW(__VA_ARGS__)
#define loge(...) 		LOGE(__VA_ARGS__)

#define fatal(...) 		do { loge(__VA_ARGS__); exit(-1); } while(0)

#define panic(...) 		fatal(__VA_ARGS__);

void dump_stack(void);

void log_init(int level);

#ifdef __cplusplus
}
#endif


/*XXX*/
#define hhh    logi("--------------%s:%d------------\n", __func__, __LINE__);

#endif

