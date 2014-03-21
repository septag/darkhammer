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
#include "dhcore/json.h"
#include "dhcore/timer.h"

void test_heap()
{
    const uint item_cnt = 1000;
    void* ptrs[item_cnt];

    uint64 t1 = timer_querytick();

    log_printf(LOG_TEXT, "allocating %d items from heap...", item_cnt);
    for (uint i = 0; i < item_cnt; i++)    {
        ptrs[i] = A_ALLOC(mem_heap(), rand_getn(16, 1024), 0);
        ASSERT(ptrs[i]);
    }

    for (uint i = 0; i < item_cnt; i++)   {
        if (rand_flipcoin(20))  {
            A_FREE(mem_heap(), ptrs[i]);
        }
    }

    log_printf(LOG_TEXT, "took %f ms.",
        timer_calctm(t1, timer_querytick())*1000.0f);

    util_getch();
}
