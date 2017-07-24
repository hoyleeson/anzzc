/*
 * src/cmds.c
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <config.h>
#include <include/log.h>
#include <include/cmds.h>

cmd_tbl_t *get_static_cmd_list(void);

static int do_help(int argc, char **argv)
{
    cmd_tbl_t *cmdp = get_static_cmd_list();

    for (; cmdp->name != 0; cmdp++) {
        printf("[%s]\n\tusage:\n", cmdp->name);
        printf("\t%s\n", cmdp->usage);
    }
    return 0;
}

static int do_version(int argc, char **argv)
{
    printf("program:  %s\n", PACKAGE_NAME);
    printf("version:  V%s\n", VERSION);
    printf("compilation date: %s,time: %s\n", __DATE__, __TIME__);
    return 0;
}

static int do_exit(int argc, char **argv)
{
    char ch;
    int opt;

    while ((opt = getopt(argc, argv, "f")) != -1) {
        switch (opt) {
            case 'f':
                exit(0);
                break;
            default:
                printf("please use help for more infomation.\n");
                break;
        }
    }

    printf("exit program?(y|n)");
    ch = getchar();
    if (ch == 'y') {
        exit(0);
    }
    return 0;
}

static int do_quit(int argc, char **argv)
{
    return do_exit(argc, argv);
}


int do_loglevel(int argc, char **argv)
{
    return 0;
}

/*******************************************************/

#define CONSOLE_CMD_END() \
    { 0, 0, 0 }

static cmd_tbl_t cmd_tbl_list[] = {
    CONSOLE_CMD(help,       do_help,        "Show help info."),
    CONSOLE_CMD(version,    do_version,     "Show version info."),
    CONSOLE_CMD(exit,       do_exit,        "Exit program.\n\t-f:exit program force."),
    CONSOLE_CMD(quit,       do_quit,        "Exit program.\n\t-f:exit program force."),
    CONSOLE_CMD(loglevel,   do_loglevel,    "Setting log print level."),
    CONSOLE_CMD_END(),
};

static LIST_HEAD(dynamic_cmd_list);
static pthread_mutex_t cmd_lock = PTHREAD_MUTEX_INITIALIZER;

cmd_tbl_t *get_static_cmd_list(void)
{
    return cmd_tbl_list;
}

void register_cmd(cmd_tbl_t *cmdp)
{
    pthread_mutex_lock(&cmd_lock);
    list_add_tail(&cmdp->entry, &dynamic_cmd_list);
    pthread_mutex_unlock(&cmd_lock);
}

void unregister_cmd(cmd_tbl_t *cmdp)
{
    pthread_mutex_lock(&cmd_lock);
    list_del(&cmdp->entry);
    pthread_mutex_unlock(&cmd_lock);
}


static cmd_tbl_t *find_cmd(const char *cmd)
{
    int n_found = 0;
    unsigned int len;
    cmd_tbl_t *cmd_list;
    cmd_tbl_t *cmdp, *cmdp_temp = NULL;

    /* Stage 1: Static commands. */
    cmd_list = get_static_cmd_list();

    if (!cmd)
        return NULL;

    len = strlen(cmd);
    for (cmdp = cmd_list; cmdp->name != 0; cmdp++) {
        if (strncmp(cmdp->name, cmd, len) == 0) {
            if (len == strlen(cmdp->name))
                return cmdp;

            cmdp_temp = cmdp;
            n_found++;
        }
    }

    /* Stage 2: Dynamic commands. */
    pthread_mutex_lock(&cmd_lock);
    list_for_each_entry(cmdp, &dynamic_cmd_list, entry) {
        if (strncmp(cmdp->name, cmd, len) == 0) {
            if (len == strlen(cmdp->name)) {
                pthread_mutex_unlock(&cmd_lock);
                return cmdp;
            }

            cmdp_temp = cmdp;
            n_found++;
        }
    }
    pthread_mutex_unlock(&cmd_lock);

    if (n_found) {
        return cmdp_temp;
    }
    return NULL;
}

int execute_cmds(int argc, char **argv)
{
    int ret = -EINVAL;
    cmd_tbl_t *cmdtp;

    cmdtp = find_cmd(argv[0]);
    if (!cmdtp)
        return ret;

    ret = cmdtp->cmd(argc, argv);
    printf("\n");

    return ret;
}

