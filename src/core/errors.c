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

#include "err.h"
#include "mem-mgr.h"
#include "log.h"
#include "mt.h"

#define ERROR_STACK_MAX    32

struct err_desc
{
    char    text[1024];
#if defined(_DEBUG_)
    char    src_filepath[DH_PATH_MAX];
    uint  line;
#endif
};

struct err_data
{
    struct err_desc* err_stack;      /* array of error stacks (see err_desc) */
    char* err_string;     /* whole error string that is created when needed */
    long volatile err_cnt;        /* number of error items in the stack */
    int init;           /* initialized ? */
    long volatile err_code;       /* last error code */
    mt_mutex mtx;
};

static struct err_data g_err;
static int g_err_zero = FALSE;

/* */
void err_reportassert(const char* expr, const char* source, uint line)
{
    char msg[512];
    sprintf(msg, "ASSERTION FAILURE: Expression '%s': %s(line: %d)\n", expr, source, line);

#if defined(_WIN_)
    MessageBox(NULL, msg, "ASSERT", MB_OK | MB_ICONWARNING);
#if defined(_MSVC_) && defined(_DEBUG_)
    OutputDebugString(msg);
#endif
#else
    puts(msg);
#endif
}

result_t err_init()
{
    memset(&g_err, 0x00, sizeof(g_err));
    g_err_zero = TRUE;

    g_err.err_stack = (struct err_desc*)ALLOC(sizeof(struct err_desc)*ERROR_STACK_MAX, 0);
    g_err.err_string = (char*)ALLOC(ERROR_STACK_MAX*1024, 0);
    if (g_err.err_stack == NULL || g_err.err_string == NULL)    {
        err_release();
        return RET_OUTOFMEMORY;
    }

    mt_mutex_init(&g_err.mtx);

    g_err.err_string[0] = 0;
    g_err.init = TRUE;
    return RET_OK;
}

void err_release()
{
    if (!g_err_zero)
        return;

    if (g_err.err_stack != NULL)    {
        FREE(g_err.err_stack);
        g_err.err_stack = NULL;
    }

    if (g_err.err_string != NULL)   {
        FREE(g_err.err_string);
        g_err.err_string = NULL;
    }

    mt_mutex_release(&g_err.mtx);
    g_err.init = FALSE;
}

void err_printf(const char* source, uint line, const char* fmt, ...)
{
    char text[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    err_print(source, line, text);
}

result_t err_printn(const char* source, uint line, uint err_code)
{
    const char* text;
    switch (err_code)   {
        case RET_OK:            text = "No errors!";    break;
        case RET_FAIL:          text = "Generic fatal error";   break;
        case RET_OUTOFMEMORY:   text = "Insufficient memory";   break;
        case RET_WARNING:       text = "Non-fatal error";   break;
        case RET_INVALIDARG:    text = "Invalid arguments"; break;
        case RET_FILE_ERROR:    text = "File open failed";  break;
        case RET_INVALIDCALL:	text = "Command not found";	break;
        case RET_NOT_IMPL:      text = "Not implemented";   break;
        default:                text = "Unknown error!";     break;
    }

    MT_ATOMIC_SET(g_err.err_code, (long)err_code);
    err_print(source, line, text);
    return err_code;
}


void err_print(const char* source, uint line, const char* text)
{
    if (g_err.err_cnt == ERROR_STACK_MAX)
        return;

    mt_mutex_lock(&g_err.mtx);
    uint idx = g_err.err_cnt;
    strcpy(g_err.err_stack[idx].text, text);

#if defined(_DEBUG_)
    strcpy(g_err.err_stack[idx].src_filepath, source);
    g_err.err_stack[idx].line = line;
#endif
    mt_mutex_unlock(&g_err.mtx);

    MT_ATOMIC_INCR(g_err.err_cnt);
}

void err_sendtolog(int as_warning)
{
    const char* text = err_getstring();
    if (as_warning)
        log_print(LOG_WARNING, text);
    else
        log_print(LOG_ERROR, text);
}

const char* err_getstring()
{
    ASSERT(g_err.err_string);

    char err_line[2048];

    mt_mutex_lock(&g_err.mtx);
    g_err.err_string[0] = '\n';
    g_err.err_string[1] = 0;

    if (g_err.err_cnt > 0)  {
        for (int i = 0; i < (int)g_err.err_cnt; i++)     {
            sprintf(err_line, "%d) %s\n", i, g_err.err_stack[i].text);
            strcat(g_err.err_string, err_line);
        }

        /* for debug releases, output the call stack too */
#if defined(_DEBUG_)
        strcat(g_err.err_string, "CALL STACK: \n");
        for (int i = 0; i < (int)g_err.err_cnt; i++)     {
            sprintf(err_line, "\t%d) %s (line: %d)\n",
                    i,
                    g_err.err_stack[i].src_filepath,
                    g_err.err_stack[i].line);
            strcat(g_err.err_string, err_line);
        }
#endif

        /* reset error count, so we can build another error stack later */
        MT_ATOMIC_SET(g_err.err_cnt, 0);
    }

    mt_mutex_unlock(&g_err.mtx);
    return g_err.err_string;
}

uint err_getcode()
{
    return g_err.err_code;
}

void err_clear()
{
    MT_ATOMIC_SET(g_err.err_cnt, 0);
}

int err_haserrors()
{
    return g_err.err_cnt != 0;
}

