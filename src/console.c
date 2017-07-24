/*
 * src/console.c
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
#include <string.h>
#include <errno.h>

#include <include/log.h>
#include <include/cmds.h>
#include <include/console.h>

#define CFG_MAXARGS     (16)
#define CMD_MAX_LEN     (1024)

#define CONSOLE_MARK 	"anzz>"

static int read_cmds(char *cmd)
{
    char buf[CMD_MAX_LEN], *p;
    int len = 0;

    printf("%s", CONSOLE_MARK);

    if (fgets(buf, sizeof(buf), stdin)) {
        p = strchr(buf, '\n');
        len = p - buf;
        len = CMD_MAX_LEN > len ? len : CMD_MAX_LEN;

        strncpy(cmd, buf, len);
    }

    return len;
}

static int parse_cmds(char *line, char **argv)
{
    int nargs = 0;
    logv("parse line: \"%s\"\n", line);

    while (nargs < CFG_MAXARGS) {
        while ((*line == ' ') || (*line == '\t')) {
            ++line;
        }

        if (*line == '\0') {
            argv[nargs] = NULL;
            return nargs;
        }

        argv[nargs++] = line;

        while (*line && (*line != ' ') && (*line != '\t'))
            ++line;

        if (*line == '\0') {
            argv[nargs] = NULL;
            return nargs;
        }
        *line++ = '\0';
    }

    printf("**too many args (max. %d)**\n", CFG_MAXARGS);
    return nargs;
}

void console_loop(void)
{
    int ret;
    int len;
    char cmd[CMD_MAX_LEN];
    char *cmd_argv[CFG_MAXARGS];

    for ( ;; ) {
        memset(cmd, 0, CMD_MAX_LEN);

        len = read_cmds(cmd);
        if (len <= 0)
            continue;

        ret = parse_cmds(cmd, cmd_argv);
        if (ret <= 0) {
            logv("parse cmd failed.\n");
            continue;
        }

        ret = execute_cmds(ret, cmd_argv);
        if (ret) {
            logv("execute cmd failed.\n");
            continue;
        }
    }
}

