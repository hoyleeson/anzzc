/*
 * src/parser.h
 *
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _COMMON_PARSER_H_
#define _COMMON_PARSER_H_

#define T_EOF 0
#define T_TEXT 1
#define T_NEWLINE 2

struct parse_state {
    char *ptr;
    char *text;
    int line;
    int nexttoken;
    void *context;
    void (*parse_line)(struct parse_state *state, int nargs, char **args);
    const char *filename;
    void *priv;
};

int lookup_keyword(const char *s);
int next_token(struct parse_state *state);
void parse_error(struct parse_state *state, const char *fmt, ...);

#endif /* PARSER_H_ */
