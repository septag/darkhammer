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

#include "dhcore/core.h"
#include "dhcore/mt.h"
#include <stdio.h>

#if defined(_WIN_)
#define sleep(x) Sleep(x*1000)
#else
#include <unistd.h>
#endif

result_t kernel(mt_thread param)
{
    printf("kernel...\n");
    sleep(1);
    return RET_OK;
}

result_t init(mt_thread param)
{
    printf("thread init\n");
    return RET_OK;
}

void release(mt_thread param)
{
    printf("thread release\n");
}

void test_thread()
{
    log_print(LOG_TEXT, "thread test ...");
    mt_thread t = mt_thread_create(kernel, init, release, MT_THREAD_NORMAL, 0, 0, NULL, NULL);
    log_print(LOG_TEXT, "waiting for thread work ...");
    sleep(5);
    log_print(LOG_TEXT, "destroying thread");
    mt_thread_destroy(t);
    log_print(LOG_TEXT, "thread destroyed");
}
