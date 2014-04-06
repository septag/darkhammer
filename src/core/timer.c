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

#include "timer.h"
#include "mem-mgr.h"
#include "err.h"

 /* types */
struct timer_mgr
{
    uint64 freq;
    uint64 prev_tick;
    float scale;
    struct pool_alloc timer_pool;
    struct linked_list* timers;
};

/*************************************************************************************************
 * globals
 */
static struct timer_mgr g_tmmgr;
static bool_t g_tm_zero = FALSE;

/*************************************************************************************************/
result_t timer_initmgr()
{
    memset(&g_tmmgr, 0x00, sizeof(struct timer_mgr));
    g_tm_zero = TRUE;

    g_tmmgr.scale = 1.0f;
    g_tmmgr.freq = timer_queryfreq();

    /* memory pool for timers */
    return mem_pool_create(mem_heap(), &g_tmmgr.timer_pool, sizeof(struct timer), 20, 0);
}

void timer_releasemgr()
{
    if (!g_tm_zero)
        return;

    mem_pool_destroy(&g_tmmgr.timer_pool);
    memset(&g_tmmgr, 0x00, sizeof(struct timer_mgr));
}

struct timer* timer_createinstance(bool_t start)
{
    struct timer* tm = (struct timer*)mem_pool_alloc(&g_tmmgr.timer_pool);
    ASSERT(tm != NULL);
    memset(tm, 0x00, sizeof(struct timer));

    list_add(&g_tmmgr.timers, &tm->node, tm);
    if (start)
        tm->rate = 1.0f;

    return tm;
}

void timer_destroyinstance(struct timer* tm)
{
    list_remove(&g_tmmgr.timers, &tm->node);
    memset(tm, 0x00, sizeof(struct timer));
    mem_pool_free(&g_tmmgr.timer_pool, tm);
}

void timer_update(uint64 tick)
{
    if (g_tmmgr.prev_tick == 0)
        g_tmmgr.prev_tick = tick;

    fl64 dt = ((fl64)(tick - g_tmmgr.prev_tick)) / ((fl64)g_tmmgr.freq);
    dt *= g_tmmgr.scale;
    g_tmmgr.prev_tick = tick;
    float dtf = (float)dt;

    /* move through the linked-list of timers and update them */
    struct linked_list* tm_node = g_tmmgr.timers;
    while (tm_node != NULL)     {
        struct timer* tm = (struct timer*)tm_node->data;
        tm->dt = dtf * tm->rate;
        tm->t += tm->dt;
        tm_node = tm_node->next;
    }
}

fl64 timer_calctm(uint64 tick1, uint64 tick2)
{
    fl64 freq = (fl64)g_tmmgr.freq;
    uint64 dt = tick2 - tick1;
    return (fl64)dt / freq;
}

void timer_pauseall()
{
    struct linked_list* tm_node = g_tmmgr.timers;
    while (tm_node != NULL)     {
        struct timer* tm = (struct timer*)tm_node->data;
        TIMER_PAUSE(tm);
        tm_node = tm_node->next;
    }
}

void timer_resumeall()
{
    struct linked_list* tm_node = g_tmmgr.timers;
    while (tm_node != NULL)     {
        struct timer* tm = (struct timer*)tm_node->data;
        TIMER_START(tm);
        tm_node = tm_node->next;
    }
}
