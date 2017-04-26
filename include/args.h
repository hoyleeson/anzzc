/*
 * include/args.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * A generic implementation of binary search
 *
 */

#ifndef _ANZZC_ARGS_H
#define _ANZZC_ARGS_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


enum args_error_type {
  ARGS_ERROR_OK,
  ARGS_ERROR_UKNOWN_OPT,
  ARGS_ERROR_TOO_MANY,
  ARGS_ERROR_REQUIRED_INTEGER_ARG,
  ARGS_ERROR_REQUIRED_STRING_ARG,
  ARGS_ERROR_UNEXPECTED_ARG,
};


enum args_option_type {
  // special
  ARGS_OPT_END,
  ARGS_OPT_GROUP,
  // options with no arguments
  ARGS_OPT_BOOLEAN,
  // options with arguments (optional or required)
  ARGS_OPT_INTEGER,
  ARGS_OPT_STRING,
};


typedef struct _args_option {
  int type;
  const char short_name;
  const char *long_name;
  void *value;
  int max_count;
  const char *help;
  const char *type_help;
  int count;
} args_option_t;


#define OPT_BOOLEAN(short_name, long_name, value, ...) \
    { ARGS_OPT_BOOLEAN, short_name, long_name, value, 1, __VA_ARGS__ }

#define OPT_INTEGER(short_name, long_name, value, ...) \
    { ARGS_OPT_INTEGER, short_name, long_name, value, 1, __VA_ARGS__ }

#define OPT_STRING_MULTI(short_name, long_name, value, max_count, ...) \
    { ARGS_OPT_STRING, short_name, long_name, value, max_count, __VA_ARGS__ }

#define OPT_STRING(short_name, long_name, value, ...) \
    OPT_STRING_MULTI(short_name, long_name, value, 1, __VA_ARGS__)

#define OPT_END() { ARGS_OPT_END, 0 }


int args_parse(args_option_t *options, int argc, const char **argv);
void args_print_usage(args_option_t *options, int aligment);


#ifdef __cplusplus
}
#endif

#endif
