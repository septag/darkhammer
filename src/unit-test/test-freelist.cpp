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

#include "unit-test-main.h"
#include "dhcore/core.h"
#include "dhcore/freelist-alloc.h"
#include "dhcore/timer.h"
#include "dhcore/hash.h"

void test_freelist()
{
    const uint item_cnt = 100000;
    const uint max_size = item_cnt * 1024;
    void** ptrs = (void**)ALLOC(item_cnt*sizeof(void*), 0);
    uint* h = (uint*)ALLOC(item_cnt*sizeof(uint), 0);
    size_t* sizes = (size_t*)ALLOC(item_cnt*sizeof(size_t), 0);

    uint free_cnt = 0;

    struct freelist_alloc freelist;
    struct allocator alloc;
    mem_freelist_create(mem_heap(), &freelist, max_size, 0);
    mem_freelist_bindalloc(&freelist, &alloc);

    uint64 t1 = timer_querytick();

    log_printf(LOG_TEXT, "allocating %d items from freelist (with hash validation)...", item_cnt);
    for (uint i = 0; i < item_cnt; i++)    {
        int s = rand_getn(8, 1024);
        ASSERT(s <= 1024);
        ptrs[i] = A_ALLOC(&alloc, s, 6);
        ASSERT(ptrs[i]);

        if (i > 0 && rand_flipcoin(50))  {
            uint idx_tofree = rand_getn(0, i-1);
            if (ptrs[idx_tofree] != NULL)   {
                A_FREE(&alloc, ptrs[idx_tofree]);
                ptrs[idx_tofree] = NULL;
            }
        }

        // random fill the buffer
        memset(ptrs[i], 0x00, s);

        h[i] = hash_murmur32(ptrs[i], s, 100);
        sizes[i] = s;
    }

    // check if the remaining buffers are untouched
    for (uint i = 0; i < item_cnt; i++)   {
        if (ptrs[i] != NULL)    {
#if defined(_DEBUG_)
            uint hh = hash_murmur32(ptrs[i], sizes[i], 100);
            ASSERT(h[i] == hh);
#endif
        }
    }

    for (uint i = 0; i < item_cnt; i++)   {
        //if (rand_flipcoin(50))  {
        if (ptrs[i] != NULL)    {
            A_FREE(&alloc, ptrs[i]);
            free_cnt ++;
            ptrs[i] = NULL;
        }
        //}
    }

    /* report leaks */
    uint leaks_cnt = mem_freelist_getleaks(&freelist, NULL);
    if (leaks_cnt > 0)
        log_printf(LOG_TEXT, "%d leaks found", leaks_cnt);

    mem_freelist_destroy(&freelist);
    log_print(LOG_TEXT, "done.");
    log_printf(LOG_TEXT, "took %f ms.",
        timer_calctm(t1, timer_querytick())*1000.0f);

    FREE(ptrs);
    FREE(h);
    FREE(sizes);

    util_getch();
}

