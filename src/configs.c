/*
 * src/configs.c
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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <include/list.h>
#include <include/utils.h>
#include <include/log.h>

#include "parser.h"

#define SECTION 0x01
#define COMMAND 0x02
#define OPTION  0x04


struct config {
    struct list_head node;

    const char *key;
    const char *value;
};

#define DAEMON_DISABLED    0x01  /* do not autostart with class */
#define DAEMON_ONESHOT     0x02  /* do not restart on exit */
#define DAEMON_RUNNING     0x04  /* currently active */
#define DAEMON_RESTARTING  0x08  /* waiting to restart */
#define DAEMON_RESTART     0x10 /* Use to safely restart (stop, wait, start) a service */
#define DAEMON_RESET       0x40  /* Use when stopping a process, but not disabling
                                    so it can be restarted with its class */

struct daemon {
    struct list_head node;

    const char *name;
    pid_t pid;

    unsigned flags;
    int nr_crashed;         /* number of times crashed within window */
    time_t time_started;    /* time of last start */

    int nargs;
    /* "MUST BE AT THE END OF THE STRUCT" */
    char *args[1];
};

struct command {
    struct list_head node;

    const char *name;
    int (*func)(int nargs, char **args);
    int nargs;
    char *args[1];
};

struct import {
    struct list_head node;

    const char *filename;
};


struct configs_module {
    struct list_head configs_list;
    struct list_head daemons_list;
    struct list_head commands_list;

    void *data;
};

struct configs_module _configs;


static void parse_line_no_op(struct parse_state *state, int nargs, char **args);
static void parse_line_config(struct parse_state* state, int nargs, char **args);
static void parse_line_command(struct parse_state* state, int nargs, char **args);
static void parse_line_daemon(struct parse_state* state, int nargs, char **args);
static void parse_line_import(struct parse_state *state, int nargs, char **args);

#include "keywords.h"

#define KEYWORD(symbol, flags, nargs, fn) \
    [ K_##symbol ] = {K_##symbol, #symbol, .func = fn, nargs + 1, flags },

struct {
    int key;
    const char *name;
    union {
        int (*func)(int nargs, char **args);
        void (*parse)(struct parse_state* state, int nargs, char **args);
    };
    unsigned char nargs;
    unsigned char flags;
} keyword_info[KEYWORD_COUNT] = {
    [K_UNKNOWN] = {K_UNKNOWN, "unknown", .parse = parse_line_no_op, 0, 0 },

    [K_config] = { K_config, "[configs]", .parse = parse_line_config, 0, SECTION },
    [K_command] = { K_command, "[commands]", .parse = parse_line_command, 0, SECTION },
    [K_daemon] = { K_daemon, "[daemons]", .parse = parse_line_daemon, 0, SECTION },
    [K_import] = { K_import, "[imports]", .parse = parse_line_import, 0, SECTION },

#include "keywords.h"
};
#undef KEYWORD

#define kw_is(kw, type) (keyword_info[kw].flags & (type))
#define kw_name(kw) (keyword_info[kw].name)
#define kw_func(kw) (keyword_info[kw].func)
#define kw_parse(kw) (keyword_info[kw].parse)
#define kw_nargs(kw) (keyword_info[kw].nargs)

#define INIT_PARSER_MAXARGS 64

int lookup_keyword(const char *s)
{
    int i;
    for(i=0; i<KEYWORD_COUNT; i++) {
        if(!strcmp(s, keyword_info[i].name)) {
            return keyword_info[i].key;
        }
    }

    return K_UNKNOWN;
}

static int valid_name(const char *name)
{
    if (strlen(name) > 16) {
        return 0;
    }
    while (*name) {
        if (!isalnum(*name) && (*name != '_') && (*name != '-')) {
            return 0;
        }
        name++;
    }
    return 1;
}

static struct config *config_find_by_key(const char *key) 
{
    struct config *conf;
    struct configs_module *configs = &_configs;

    list_for_each_entry(conf, &configs->configs_list, node) {
        if(!strcmp(conf->key, key))
            return conf;
    }
    return NULL;
}

static struct daemon *daemon_find_by_name(const char *name) 
{
    struct daemon *dm;
    struct configs_module *configs = &_configs;

    list_for_each_entry(dm, &configs->daemons_list, node) {
        if(!strcmp(dm->name, name))
            return dm;
    }
    return NULL;
}


static void parse_line_no_op(struct parse_state *state, int nargs, char **args)
{
}

static void parse_line_config(struct parse_state* state, int nargs, char **args)
{
    struct config *conf;
    struct configs_module *configs = state->context;

    if (nargs < 2) {
        parse_error(state, "config must have a name and value\n");
        return;
    }

    conf = malloc(sizeof(*conf));
    conf->key = args[0];
    conf->value = args[1];

    list_add_tail(&conf->node, &configs->configs_list);
}


static void parse_line_daemon(struct parse_state* state, int nargs, char **args)
{
    struct daemon *dm;
    struct configs_module *configs = state->context;

    if (nargs < 2) {
        parse_error(state, "daemon must have a name and a program\n");
        return;
    }
    if (!valid_name(args[0])) {
        parse_error(state, "invalid daemon name '%s'\n", args[0]);
        return;
    }

    dm = daemon_find_by_name(args[0]);
    if (dm) {
        parse_error(state, "ignored duplicate definition of daemon '%s'\n", args[0]);
        return;
    }

    nargs -= 1;
    dm = calloc(1, sizeof(*dm) + sizeof(char*) * nargs);
    if (!dm) {
        parse_error(state, "out of memory\n");
        return;
    }
    dm->name = args[0];
    memcpy(dm->args, args + 1, sizeof(char*) * nargs);
    dm->args[nargs] = 0;
    dm->nargs = nargs;
    list_add_tail(&dm->node, &configs->daemons_list);
}

static void parse_line_command(struct parse_state* state, int nargs, char **args)
{
    struct command *cmd;
    struct configs_module *configs = state->context;
    int kw, n;

    if (nargs == 0) {
        return;
    }

    kw = lookup_keyword(args[0]);
    if (!kw_is(kw, COMMAND)) {
        parse_error(state, "invalid command '%s'\n", args[0]);
        return;
    }

    n = kw_nargs(kw);
    if (nargs < n) {
        parse_error(state, "%s requires %d %s\n", args[0], n - 1,
                n > 2 ? "arguments" : "argument");
        return;
    }

    cmd = malloc(sizeof(*cmd) + sizeof(char*) * nargs);
    cmd->name = args[0];
    cmd->func = kw_func(kw);
    cmd->nargs = nargs;
    memcpy(cmd->args, args, sizeof(char*) * nargs);
    list_add_tail(&cmd->node, &configs->commands_list);
}

static void parse_line_import(struct parse_state *state, int nargs, char **args)
{
    struct list_head *imports_list = state->priv;
    struct import *import;

    if (nargs != 1) {
        loge("single argument needed for import\n");
        return;
    }

    import = calloc(1, sizeof(struct import));
    import->filename = args[0];
    list_add_tail(&import->node, imports_list);
    logi("found import '%s', adding to import list\n", import->filename);
}

static int init_parse_config_file(struct configs_module *configs, const char *fname);

static void parse_configs(struct configs_module *configs, const char *fn, char *s)
{
    struct import *import, *tmp;
    struct parse_state state;
    LIST_HEAD(import_list);
    char *args[INIT_PARSER_MAXARGS];
    int nargs;

    nargs = 0;
    state.filename = fn;
    state.line = 0;
    state.ptr = s;
    state.nexttoken = 0;
    state.parse_line = parse_line_no_op;
    state.context = configs;

    state.priv = &import_list;

    for (;;) {
        switch (next_token(&state)) {
            case T_EOF:
                //  state.parse_line(&state, 0, 0);
                goto parser_done;
            case T_NEWLINE:
                state.line++;
                if (nargs) {
                    int kw = lookup_keyword(args[0]);
                    if (kw_is(kw, SECTION)) {
                        //state.parse_line(&state, 0, 0);
                        state.parse_line = kw_parse(kw);
                    } else {
                        state.parse_line(&state, nargs, args);
                    }
                    nargs = 0;
                }
                break;
            case T_TEXT:
                if (nargs < INIT_PARSER_MAXARGS) {
                    args[nargs++] = state.text;
                }
                break;
        }
    }

parser_done:
    list_for_each_entry_safe(import, tmp, &import_list, node) {
        int ret;

        logi("importing '%s'\n", import->filename);
        ret = init_parse_config_file(configs, import->filename);
        if (ret)
            loge("could not import file '%s' from '%s'\n", import->filename, fn);
        list_del(&import->node);
        free(import);
    }
}

static int init_parse_config_file(struct configs_module *configs, const char *fname)
{
    char *data;

    data = read_file(fname, 0);
    if (!data)
        return -1;

    parse_configs(configs, fname, data);
    return 0;
}


static void daemon_start(struct daemon *dm) 
{
    struct stat s;
    pid_t pid;

    /* starting a service removes it from the disabled or reset
     * state and immediately takes it out of the restarting
     * state if it was in there
     */
    dm->flags &= (~(DAEMON_DISABLED|DAEMON_RESTARTING|DAEMON_RESET|DAEMON_RESTART));
    dm->time_started = 0;

    /* running processes require no additional work -- if
     * they're in the process of exiting, we've ensured
     * that they will immediately restart on exit, unless
     * they are ONESHOT
     */
    if (dm->flags & DAEMON_RUNNING) {
        return;
    }

    if (stat(dm->args[0], &s) != 0) {
        loge("cannot find '%s', disabling '%s'\n", dm->args[0], dm->name);
        dm->flags |= DAEMON_DISABLED;
        return;
    }

    pid = fork();

    if (pid == 0) {
        char *environ[] = { NULL };
        if(execve(dm->args[0], (char**) dm->args, (char**) environ) < 0) {
            loge("cannot execve('%s'): %s\n", dm->args[0], strerror(errno));
        }

        _exit(127);
    } else if (pid < 0) {
        loge("failed to start '%s'\n", dm->name);
        dm->pid = 0;
        return;
    }

    dm->time_started = gettime();
    dm->pid = pid;
    dm->flags |= DAEMON_RUNNING;
}

void exec_daemons(void) 
{
    struct daemon *dm;
    struct configs_module *configs = &_configs;

    list_for_each_entry(dm, &configs->daemons_list, node) {
        daemon_start(dm);
    }
}

void exec_commands(void) 
{
    struct command *cmd;
    struct configs_module *configs = &_configs;

    list_for_each_entry(cmd, &configs->commands_list, node) {
        if(cmd->func)
            cmd->func(cmd->nargs, cmd->args);
    }
}

static void dump(void);

int init_configs(const char *fname)
{
    struct configs_module *configs = &_configs;

    INIT_LIST_HEAD(&configs->configs_list);
    INIT_LIST_HEAD(&configs->daemons_list);
    INIT_LIST_HEAD(&configs->commands_list);

    init_parse_config_file(configs, fname);

    dump();
    return 0;
}

const char *config_val_find_by_key(const char *key) 
{
    struct config *conf;

    conf = config_find_by_key(key);
    if(!conf)
        return NULL;
    return conf->value;
}

static void dump(void) 
{
    struct config *config;
    struct daemon *daemon;
    struct command *command;
    struct configs_module *configs = &_configs;

    printf("\n====================config============================\n");
    list_for_each_entry(config, &configs->configs_list, node) {
        printf("key:%s value:%s\n", config->key, config->value);
    }

    printf("\n====================daemon============================\n");
    list_for_each_entry(daemon, &configs->daemons_list, node) {
        int i;
        printf("name:%s args:", daemon->name);
        for(i=0; i<daemon->nargs; i++) {
            printf("%s ", daemon->args[i]);
        }
        printf("\n");
    }

    printf("\n====================command===========================\n");
    list_for_each_entry(command, &configs->commands_list, node) {
        int i;
        printf("name:%s args:", command->name);
        for(i=0; i<command->nargs; i++) {
            printf("%s ", command->args[i]);
        }
        printf("\n");
    }
}


