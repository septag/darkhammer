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

#include "mt.h"

#if defined(_WIN_)

#include <process.h>
#include <stdio.h>

#include "mem-mgr.h"
#include "err.h"
#include "stack-alloc.h"
#include "freelist-alloc.h"
#include "array.h"

#define EVENT_STOP      0
#define EVENT_RESUME    1

/* thread callback */
DWORD WINAPI thread_callback(void* param);

/*************************************************************************************************
 * Types
 */
struct mt_event_data
{
    struct array signals;    /* item: HANDLE for OS event */
    struct allocator* alloc;
};

struct mt_thread_data
{
    HANDLE t;   /* OS thread */
    enum mt_thread_priority pr; /* priority */
    struct freelist_alloc local_mem; /* local dynamic memory besides thread's own stack */
    struct allocator local_alloc; /* allocator for local memory */
    struct stack_alloc tmp_mem; /* temp memory stack */
    struct allocator tmp_alloc; /* temp allocator */
    pfn_mt_thread_kernel kernel_fn; /* kernel function, runs in loop unless RET_ABORT is returned */
    pfn_mt_thread_init init_fn; /* init function (happens in thread process) */
    pfn_mt_thread_release release_fn; /* release function (happens in thread process) */
    void* param1; /* custom param1 */
    void* param2; /* custom param2 */

    HANDLE events[2];  /* 0=stop, 1=resume */
    uint id;
};


/*************************************************************************************************
 * Events
 */
mt_event mt_event_create(struct allocator* alloc)
{
    mt_event e = (mt_event)A_ALLOC(alloc, sizeof(struct mt_event_data), 0);
    if (e == NULL)
        return NULL;
    memset(e, 0x00, sizeof(struct mt_event_data));
    e->alloc = alloc;

    result_t r = arr_create(alloc, &e->signals, sizeof(HANDLE), 8, 16, 0);
    if (IS_FAIL(r)) {
        A_FREE(alloc, e);
        return NULL;
    }

    return e;
}

void mt_event_destroy(mt_event e)
{
    for (uint i = 0; i < e->signals.item_cnt; i++) {
        HANDLE ehdl = ((HANDLE*)e->signals.buffer)[i];
        if (ehdl != NULL)
            CloseHandle(ehdl);
    }
    arr_destroy(&e->signals);
    A_FREE(e->alloc, e);
}

uint mt_event_addsignal(mt_event e)
{
    HANDLE ehdl = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (ehdl == NULL)
        return 0;

    HANDLE* phdl = (HANDLE*)arr_add(&e->signals);
    if (phdl == NULL)   {
        CloseHandle(ehdl);
        return 0;
    }

    *phdl = ehdl;
    return e->signals.item_cnt;
}

enum mt_event_response mt_event_wait(mt_event e, uint signal_id, uint timeout)
{
    ASSERT(signal_id != 0);
    ASSERT(signal_id <= e->signals.item_cnt);

    HANDLE ehdl = ((HANDLE*)e->signals.buffer)[signal_id-1];
    ASSERT(ehdl != NULL);
    DWORD r = WaitForSingleObject(ehdl, timeout);
    if (r == WAIT_TIMEOUT)
        return MT_EVENT_TIMEOUT;
    else if (r == WAIT_FAILED)
        return MT_EVENT_ERROR;
    else
        return MT_EVENT_OK;
}

enum mt_event_response mt_event_waitforall(mt_event e, uint timeout)
{
    const HANDLE* hdls = (const HANDLE*)e->signals.buffer;
    DWORD r = WaitForMultipleObjects(e->signals.item_cnt, hdls, TRUE, timeout);
    if (r == WAIT_TIMEOUT)
        return MT_EVENT_TIMEOUT;
    else if (r == WAIT_FAILED)
        return MT_EVENT_ERROR;
    else
        return MT_EVENT_OK;
}

void mt_event_trigger(mt_event e, uint signal_id)
{
    ASSERT(signal_id != 0);
    ASSERT(signal_id <= e->signals.item_cnt);

    HANDLE ehdl = ((HANDLE*)e->signals.buffer)[signal_id-1];
    ASSERT(ehdl != NULL);
    SetEvent(ehdl);
}

/*************************************************************************************************
 * Threads
 */
mt_thread mt_thread_create(
    pfn_mt_thread_kernel kernel_fn, pfn_mt_thread_init init_fn, pfn_mt_thread_release release_fn,
    enum mt_thread_priority pr, size_t local_mem_sz, size_t tmp_mem_sz, void* param1, void* param2)
{
    static uint count = 0;
    result_t r;
    mt_thread thread = (mt_thread)ALLOC(sizeof(struct mt_thread_data), 0);
    if (thread == NULL)
        return NULL;
    memset(thread, 0x00, sizeof(struct mt_thread_data));

    if (local_mem_sz > 0)   {
        r = mem_freelist_create(mem_heap(), &thread->local_mem, local_mem_sz, 0);
        if (IS_FAIL(r)) {
            FREE(thread);
            return NULL;
        }
        mem_freelist_bindalloc(&thread->local_mem, &thread->local_alloc);
    }

    if (tmp_mem_sz > 0) {
        r = mem_stack_create(mem_heap(), &thread->tmp_mem, tmp_mem_sz, 0);
        if (IS_FAIL(r)) {
            FREE(thread);
            return NULL;
        }
        mem_stack_bindalloc(&thread->tmp_mem, &thread->tmp_alloc);
    }

    thread->kernel_fn = kernel_fn;
    thread->init_fn = init_fn;
    thread->release_fn = release_fn;
    thread->param1 = param1;
    thread->param2 = param2;
    thread->pr = pr;

    char e1name[32];
    char e2name[32];
    sprintf(e1name, "stop:t(%d)", count);
    sprintf(e2name, "resume:t(%d)", count);
    count++;

    thread->events[EVENT_STOP] = CreateEvent(NULL, TRUE, FALSE, e1name);
    thread->events[EVENT_RESUME] = CreateEvent(NULL, TRUE, TRUE, e2name);

    if (thread->events[EVENT_STOP] == NULL || thread->events[EVENT_RESUME] == NULL)
        return NULL;

    HANDLE t = CreateThread(NULL, 0, thread_callback, thread, 0, (DWORD*)&thread->id);
    if (t == NULL) {
        mt_thread_destroy(thread);
        return NULL;
    }

    thread->t = t;

    switch (pr)   {
    case MT_THREAD_NORMAL:    SetThreadPriority(thread->t, THREAD_PRIORITY_NORMAL);    break;
    case MT_THREAD_HIGH:      SetThreadPriority(thread->t, THREAD_PRIORITY_HIGHEST);   break;
    case MT_THREAD_LOW:       SetThreadPriority(thread->t, THREAD_PRIORITY_LOWEST);    break;
    }

    return thread;
}

void mt_thread_destroy(mt_thread thread)
{
    /* reset events */
    if (thread->events[EVENT_STOP] != NULL)
        SetEvent(thread->events[EVENT_STOP]);
    if (thread->events[EVENT_RESUME] != NULL)
        ResetEvent(thread->events[EVENT_RESUME]);

    /* wait for thread to finish */
    if (thread->t != NULL)  {
        WaitForSingleObject(thread->t, INFINITE);
        CloseHandle(thread->t);
    }

    /* destroy events */
    if (thread->events[EVENT_RESUME] != NULL)
        CloseHandle(thread->events[EVENT_RESUME]);
    if (thread->events[EVENT_STOP] != NULL)
        CloseHandle(thread->events[EVENT_STOP]);

    mem_freelist_destroy(&thread->local_mem);
    mem_stack_destroy(&thread->tmp_mem);
    FREE(thread);
}

void mt_thread_pause(mt_thread thread)
{
    ResetEvent(thread->events[EVENT_RESUME]);
}

void mt_thread_resume(mt_thread thread)
{
    SetEvent(thread->events[EVENT_RESUME]);
}

void mt_thread_stop(mt_thread thread)
{
    SetEvent(thread->events[EVENT_STOP]);
}

DWORD WINAPI thread_callback(void* param)
{
    result_t r;
    mt_thread thread = (mt_thread)param;

    if (thread->init_fn != NULL)   {
        r = thread->init_fn((mt_thread)param);
        if (IS_FAIL(r)) {
            _endthreadex(-1);
            return -1;
        }
    }

    while (WaitForMultipleObjects(2, thread->events, FALSE, INFINITE) !=
        (WAIT_OBJECT_0 + EVENT_STOP))
    {
        r = thread->kernel_fn((mt_thread)param);
        if (IS_FAIL(r))
            break;
    }

    if (thread->release_fn != NULL)
        thread->release_fn((mt_thread)param);

    ExitThread(0);
    return 0;
}

uint mt_thread_getid(mt_thread thread)
{
    return thread->id;
}

void* mt_thread_getparam1(mt_thread thread)
{
    return thread->param1;
}

void* mt_thread_getparam2(mt_thread thread)
{
    return thread->param2;
}

struct allocator* mt_thread_getlocalalloc(mt_thread thread)
{
    return &thread->local_alloc;
}

struct allocator* mt_thread_gettmpalloc(mt_thread thread)
{
    return &thread->tmp_alloc;
}

void mt_thread_resettmpalloc(mt_thread thread)
{
    mem_stack_reset(&thread->tmp_mem);
}

#endif  /* _WIN_ */
