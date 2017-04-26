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

#include <include/log.h>

#ifdef ANDROID
#include "jni.h"
#include <android/log.h>
#endif

FILE *logfp = NULL;  /* Only use for LOG_MODE_FILE */

static uint8_t log_level = LOG_DEFAULT_LEVEL;
static enum logger_mode log_mode = LOG_MODE_QUIET;

static char level_tags[LOG_LEVEL_MAX + 1] = {
    'F', 'E', 'W', 'I', 'D', 'V',
};

static const char *log_mode_str[LOG_MODE_MAX + 1] = {
	[LOG_MODE_QUIET] = "quiet",
	[LOG_MODE_STDERR] = "stderr",
	[LOG_MODE_FILE] = "file",
	[LOG_MODE_CLOUD] = "cloud",
};


__attribute__((weak)) const char *get_log_path(void)
{
	return LOG_FILE;
}

void log_print(int level, const char *tag, const char *fmt, ...)
{
    va_list ap;
	int len;
	time_t now;
	char timestr[32];
    char buf[LOG_BUF_SIZE];

    if(level > log_level || level < 0)
        return;

	time(&now);
	strftime(timestr, 32, "%Y-%m-%d %H:%M:%S", localtime(&now));

    len = snprintf(buf, LOG_BUF_SIZE, "%s (%s)/[%c] ",
		   	timestr, tag, level_tags[level]);
    va_start(ap, fmt);
    vsnprintf(buf + len, LOG_BUF_SIZE - len, fmt, ap);
    va_end(ap);

	if (log_mode == LOG_MODE_FILE) {
		fputs(buf, logfp);
		fflush(logfp);
	} else if (log_mode == LOG_MODE_CLOUD) {
	}
#ifdef ANDROID
   	else if (log_mode == LOG_MODE_ANDROID) {
		print_android_log(ANDROID_LOG_INFO, tag, buf + len);
	}
#endif
   	else {
		fputs(buf, stdout);
		fflush(stdout);
	}
}

void log_init(enum logger_mode mode, enum logger_level level)
{
	log_mode = mode;
	log_level = level;

	if (log_level > LOG_LEVEL_MAX || log_level < 0)
		log_level = DEFAULT_LOG_LEVEL;

	if(log_mode >= LOG_MODE_MAX || log_mode < 0)
		log_mode = DEFAULT_LOG_LEVEL;

	if(log_mode == LOG_MODE_FILE) {
		logfp = fopen(get_log_path(), "a+");
		if(!logfp) {
			log_mode = LOG_MODE_STDERR;
		}
	}
	logi("log init. mode:%d, level:%d\n",
		   	log_mode_str[mode], level_tags[log_level]);
}

void log_release(void)
{
	if(log_mode == LOG_MODE_FILE) {
		fclose(logfp);
	}
}

