/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
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


#if defined(_WIN_)
#include "win.h"
#endif

#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "mt.h"
#include "util.h"

#define LINE_COUNT_FLUSH    20

/* fwd declarations */
void log_outputtext(enum log_type type, const char* text);

/* types */
struct log
{
    struct log_stats    stats;
    uint              outputs;
    char                log_filepath[DH_PATH_MAX];
    FILE*               log_file;
    pfn_log_handler     log_fn;
    void*               fn_param;
};

enum output_mode
{
    OUTPUT_CONSOLE = (1<<0),
    OUTPUT_DEBUGGER = (1<<1),
    OUTPUT_FILE = (1<<2),
    OUTPUT_CUSTOM = (1<<3)
};

/* globals */
static struct log g_log;
static int g_log_init = FALSE;

result_t log_init()
{
    memset(&g_log, 0x00, sizeof(g_log));
    g_log_init = TRUE;
    return RET_OK;
}

void log_release()
{
    if (!g_log_init)
        return;

    if (g_log.log_file != NULL)     {
        fclose(g_log.log_file);
    }
    memset(&g_log, 0x00, sizeof(g_log));
}

result_t log_outputconsole(int enable)
{
    if (enable)
    	BIT_ADD(g_log.outputs, OUTPUT_CONSOLE);
    else
    	BIT_REMOVE(g_log.outputs, OUTPUT_CONSOLE);

    return RET_OK;
}

result_t log_outputfile(int enable, const char* log_filepath)
{
    BIT_ADD(g_log.outputs, OUTPUT_FILE);

    /* if logfile is previously opened, close it */
    if (g_log.log_file != NULL)     {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
        BIT_REMOVE(g_log.outputs, OUTPUT_FILE);
    }

    if (enable)     {
        g_log.log_file = fopen(log_filepath, "wt");
        if (g_log.log_file == NULL)
            return RET_FILE_ERROR;

        BIT_ADD(g_log.outputs, OUTPUT_FILE);
        strcpy(g_log.log_filepath, log_filepath);
    }

    return RET_OK;
}

result_t log_outputdebugger(int enable)
{
    if (enable)
    	BIT_ADD(g_log.outputs, OUTPUT_DEBUGGER);
    else
    	BIT_REMOVE(g_log.outputs, OUTPUT_DEBUGGER);

    return RET_OK;
}

result_t log_outputfunc(int enable, pfn_log_handler log_fn, void* param)
{
    if (enable)     {
        BIT_ADD(g_log.outputs, OUTPUT_CUSTOM);
        g_log.log_fn = log_fn;
        g_log.fn_param = param;
    }    else       {
        BIT_REMOVE(g_log.outputs, OUTPUT_CUSTOM);
        g_log.log_fn = NULL;
        g_log.fn_param = NULL;
    }

    return RET_OK;
}

int log_isconsole()
{
    return BIT_CHECK(g_log.outputs, OUTPUT_CONSOLE);
}

int log_isfile()
{
    return BIT_CHECK(g_log.outputs, OUTPUT_FILE);
}

int log_isdebugger()
{
    return BIT_CHECK(g_log.outputs, OUTPUT_DEBUGGER);
}

int log_isoutputfunc()
{
    return BIT_CHECK(g_log.outputs, OUTPUT_CUSTOM);
}

void log_print(enum log_type type, const char* text)
{
    log_outputtext(type, text);
}

void log_printf(enum log_type type, const char* fmt, ...)
{
    char text[2048];
    text[0] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    log_outputtext(type, text);
}

void log_getstats(struct log_stats* stats)
{
    memcpy(stats, &g_log.stats, sizeof(g_log.stats));
}

void log_outputtext(enum log_type type, const char* text)
{
    const char* prefix;
    char msg[2048];

    switch (type)   {
        case LOG_ERROR:
            prefix = "Error: ";
            MT_ATOMIC_INCR(g_log.stats.errors_cnt);
            break;

        case LOG_WARNING:
            prefix = "Warning: ";
            MT_ATOMIC_INCR(g_log.stats.warnings_cnt);
            break;

        case LOG_LOAD:
            prefix = "Load: ";
            break;

        default:
            prefix = "";
            break;
    }

    MT_ATOMIC_INCR(g_log.stats.msgs_cnt);
    strcpy(msg, prefix);
    strcat(msg, text);

    /* message is ready, dispatch it to outputs */
    if (BIT_CHECK(g_log.outputs, OUTPUT_CONSOLE))   {
#if !defined(_WIN_)        
        char msg2[2060];
        const char* color;
        switch (type)   {
            case LOG_ERROR:
            color = TERM_RED;
            break;
            case LOG_WARNING:
            color = TERM_YELLOW;
            break;
            case LOG_INFO:
            color = TERM_DIM;
            break;
            case LOG_LOAD:
            color = TERM_DIMCYAN;
            break;
            case LOG_TEXT:
            color = TERM_RESET;
            break;
        }

        strcpy(msg2, color);
        strcat(msg2, msg);
        strcat(msg2, TERM_RESET);
        puts(msg2);
#else
        puts(msg);
#endif
    }

    strcat(msg, "\n");
    if (BIT_CHECK(g_log.outputs, OUTPUT_FILE))  {
        fputs(msg, g_log.log_file);
        fflush(g_log.log_file);
    }

#if defined(_MSVC_) && defined(_DEBUG_)
    if (BIT_CHECK(g_log.outputs, OUTPUT_DEBUGGER))  {
        OutputDebugString(msg);
    }
#endif

    if (BIT_CHECK(g_log.outputs, OUTPUT_CUSTOM))    {
        g_log.log_fn(type, msg, g_log.fn_param);
    }
}
