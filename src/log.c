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

static FILE *logfp = NULL;  /* Only use for LOG_MODE_FILE */
static void (*log_cbprint)(int, const char *) = NULL;

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
#ifdef ANDROID
	[LOG_MODE_ANDROID] = "android",
#endif
};


__attribute__((weak)) const char *get_log_path(void)
{
	return LOG_FILE;
}

void default_cbprint(int level, const char *log)
{
	fputc(level_tags[level], stdout);
	fputc(':', stdout);
	fputs(log, stdout);
	fputc('\n', stdout);
	fflush(stdout);
}

void log_set_logpath(const char *path)
{
	if(logfp)
		fclose(logfp);

	logfp = fopen(path, "a+");
	if(!logfp) {
		log_mode = LOG_MODE_STDOUT;
	}
	logv("log file:%s", path);
}

void log_set_callback(void (*cb)(int, const char *))
{
	log_cbprint = cb;
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
		/* FIXME: */
	}  else if(log_mode == LOG_MODE_CALLBACK) {
		if(log_cbprint)
			log_cbprint(level, buf + len);
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

	if(log_mode == LOG_MODE_FILE && !logfp) {
		logfp = fopen(get_log_path(), "a+");
		if(!logfp) {
			log_mode = LOG_MODE_STDOUT;
		}
	} else if(log_mode == LOG_MODE_CALLBACK && !log_cbprint) {
		log_cbprint = default_cbprint;
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

