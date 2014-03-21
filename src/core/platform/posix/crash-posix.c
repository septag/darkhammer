/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <execinfo.h>
#include <errno.h>
#include <unistd.h>

#include "crash.h"
#include "log.h"

/*************************************************************************************************/
int detect_gdb(void)
{
    int rc = 0;
    FILE *fd = fopen("/tmp", "r");

    if (fileno(fd) > 5)
    {
        rc = 1;
    }

    fclose(fd);
    return rc;
}

void crash_print(const char* text)
{
    char* str = (char*)malloc(strlen(text)+5);
    strcpy(str+4, text);
    str[0] = 32;
    str[1] = 32;
    str[2] = 32;
    str[3] = 32;

    if (!log_isconsole())
        puts(text);
    log_print(LOG_INFO, str);

    free(str);
}

void crash_print_callstack(uint max_frames)
{
    /* storage array for stack trace address data */
    void* addrlist[max_frames+1];

    /* retrieve current stack addresses */
    uint addrlen = backtrace( addrlist, sizeof( addrlist ) / sizeof( void* ));

    if (addrlen == 0) {
        crash_print("");
        return;
    }

    /* create readable strings to each frame. */
    char** symbollist = backtrace_symbols( addrlist, addrlen );

    /* print the stack trace. */
    for (uint i = 4; i < addrlen; i++)
        crash_print(symbollist[i]);

    free(symbollist);
}

void crash_handler(int signum)
{
    const char* name;
    switch (signum) {
    case SIGABRT:
        name = "SIGABRT (Program abort)";
        break;
    case SIGSEGV:
        name = "SIGSEGV (Memory access)";
        break;
    case SIGILL:
        name = "SIGILL (Illegal call)";
        break;
    case SIGFPE:
        name = "SIGFPE (Illegal FPU call)";
        break;
    default:
        name = "[unknown]";
    }

    if (!log_isconsole())   {
        printf("Fatal Error: %s\n", name);
        puts("Callstack:");
    }

    log_printf(LOG_ERROR, "Fatal error: %s", name);
    log_print(LOG_TEXT, "Callstack:");

    crash_print_callstack(63);

    exit(signum);
}

result_t crash_init()
{
    if (!detect_gdb())   {
#if defined(_DEBUG_)
        puts("Activating crash handler ...");
#endif
        signal(SIGABRT, crash_handler);
        signal(SIGSEGV, crash_handler);
        signal(SIGILL, crash_handler);
        signal(SIGFPE, crash_handler);
    }
    return RET_OK;
}
