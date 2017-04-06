/*
 * src/dump_stack.c
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

#include <dlfcn.h>
#include <include/log.h>

#ifdef ANDROID

//#include <corkscrew/backtrace.h>

#define MAX_DEPTH               31
#define MAX_LINE_LENGTH         800
#define PATH                    "/system/lib/libcorkscrew.so"

/*
 *  * Describes a single frame of a backtrace.
 *   */
typedef struct {
    uintptr_t absolute_pc;     /* absolute PC offset */
    uintptr_t stack_top;       /* top of stack for this frame */
    size_t stack_size;         /* size of this stack frame */
} backtrace_frame_t;

/*
 *  * Describes the symbols associated with a backtrace frame.
 *   */
typedef struct {
    uintptr_t relative_pc;       /* relative frame PC offset from the start of the library,
                                    or the absolute PC if the library is unknown */
    uintptr_t relative_symbol_addr; /* relative offset of the symbol from the start of the
                                       library or 0 if the library is unknown */
    char* map_name;              /* executable or library name, or NULL if unknown */
    char* symbol_name;           /* symbol name, or NULL if unknown */
    char* demangled_name;        /* demangled symbol name, or NULL if unknown */
} backtrace_symbol_t;


typedef ssize_t (*unwindFn)(backtrace_frame_t*, size_t, size_t);
typedef void (*unwindSymbFn)(const backtrace_frame_t*, size_t, backtrace_symbol_t*);
typedef void (*unwindSymbFreeFn)(backtrace_symbol_t*, size_t);

static void *g_handle = NULL;

void dump_stack(void)
{
    ssize_t i = 0;
    ssize_t count;
    backtrace_frame_t stack[MAX_DEPTH];
    backtrace_symbol_t symbols[MAX_DEPTH];

    unwindFn unwind_backtrace = NULL;
    unwindSymbFn get_backtrace_symbols = NULL;
    unwindSymbFreeFn free_backtrace_symbols = NULL;

    // open the so.
    if(g_handle == NULL) 
        g_handle = dlopen(PATH, RTLD_NOW);

    // get the interface for unwind and symbol analyse
    if(g_handle != NULL) {
        unwind_backtrace = (unwindFn)dlsym(g_handle, "unwind_backtrace");
        get_backtrace_symbols = (unwindSymbFn)dlsym(g_handle, "get_backtrace_symbols");
        free_backtrace_symbols = (unwindSymbFreeFn)dlsym(g_handle, "free_backtrace_symbols");
    }

    if(!g_handle ||!unwind_backtrace ||
            !get_backtrace_symbols || !free_backtrace_symbols  ) {
        LOGE("Error! cannot get unwind info: handle:%p %p %p %p",
                g_handle, unwind_backtrace, get_backtrace_symbols, free_backtrace_symbols);
        return;
    }

    count= unwind_backtrace(stack, 1, MAX_DEPTH);
    get_backtrace_symbols(stack, count, symbols);

    for (i = 0; i < count; i++) {
        char line[MAX_LINE_LENGTH];

        const char* mapName = symbols[i].map_name ? symbols[i].map_name : "<unknown>";
        const char* symbolName =symbols[i].demangled_name ? symbols[i].demangled_name : symbols[i].symbol_name;
        size_t fieldWidth = (MAX_LINE_LENGTH - 80) / 2;

        if (symbolName) {
            uint32_t pc_offset = symbols[i].relative_pc - symbols[i].relative_symbol_addr;
            if (pc_offset) {
                snprintf(line, MAX_LINE_LENGTH, "#%02d  pc %08x  %.*s (%.*s+%u)",
                        i, symbols[i].relative_pc, fieldWidth, mapName,
                        fieldWidth, symbolName, pc_offset);
            } else {
                snprintf(line, MAX_LINE_LENGTH, "#%02d  pc %08x  %.*s (%.*s)",
                        i, symbols[i].relative_pc, fieldWidth, mapName,
                        fieldWidth, symbolName);
            }
        } else {
            snprintf(line, MAX_LINE_LENGTH, "#%02d  pc %08x  %.*s",
                    i, symbols[i].relative_pc, fieldWidth, mapName);
        }

        LOGD("%s", line);
    }

    free_backtrace_symbols(symbols, count);
}

#else

#include <execinfo.h>

#define DUMP_STACK_SIZE      (1024)

void dump_stack(void)
{
    int i;
    int size;
    void *buffer[DUMP_STACK_SIZE];
    char **funcs;

    size = backtrace(buffer, DUMP_STACK_SIZE);

    funcs = backtrace_symbols(buffer, size);
    if(funcs == NULL) {
        logi("Empty stack.\n");
        return;
    }

    logi("Stack trace:\n");
    for(i=0; i < size; i++) {
        logi("[%d] %s\n", i, funcs[i]);
    }
    free(funcs);
}


#endif

