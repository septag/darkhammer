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

#ifndef __MAIN_H__
#define __MAIN_H__

#include "dhcore/numeric.h"
#include <stdio.h>

#if defined(_WIN_)
#define DATA_DIR    "d:/sepul/dev/hmrengine2/test-data/"
#else
#define DATA_DIR	"/home/sepul/dev/projects/dark-hammer/test-data/"
#endif

void test_json();
void test_heap();
void test_freelist();
void test_mempool();
void test_thread();
void test_efsw();
void test_taskmgr();

INLINE void fill_buffer(void* buffer, size_t size)
{
    uint int_cnt = (uint)(size/sizeof(int));
    int* ibuf = (int*)buffer;

    for (uint i = 0; i < int_cnt; i++)    {
        ibuf[i] = rand_geti(1, 100);
    }
}


#endif /* __MAIN_H__ */
