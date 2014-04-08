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
#include "dhcore/pool-alloc.h"
#include "dhcore/timer.h"

void test_mempool()
{
    const uint item_cnt = 1000;
    void* ptrs[item_cnt];
    struct pool_alloc pool;
    struct allocator alloc;

    uint s = rand_geti(64, 1024);
    mem_pool_create(mem_heap(), &pool, s, 100, 0);
    mem_pool_bindalloc(&pool, &alloc);

    uint64 t1 = timer_querytick();

    log_printf(LOG_TEXT, "allocating %d items from pool...", item_cnt);
    for (uint i = 0; i < item_cnt; i++)   {
        ptrs[i] = A_ALLOC(&alloc, s, 0);
        ASSERT(ptrs[i]);
    }

    for (uint i = 0; i < item_cnt; i++)   {
        if (rand_flipcoin(50))  {
            A_FREE(&alloc, ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    /* report leaks */
    uint leaks_cnt = mem_pool_getleaks(&pool);
    if (leaks_cnt > 0)  {
        log_printf(LOG_TEXT, "%d leaks found", leaks_cnt);
    }

    log_print(LOG_TEXT, "done.");
    log_printf(LOG_TEXT, "took %f ms.", timer_calctm(t1, timer_querytick())*1000.0f);

    mem_pool_destroy(&pool);
}
