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

#if defined(_POSIXLIB_)

#include <stdio.h>
#include <errno.h>

#include "mem-mgr.h"
#include "err.h"
#include "freelist-alloc.h"
#include "stack-alloc.h"
#include "array.h"

/* thread's own callback */
void* thread_callback(void* param);

/*************************************************************************************************
 * Types
 */

struct mt_event_signal
{
    int signal;
    mt_mutex signal_mtx;
    pthread_cond_t cond;
};

struct mt_event_data
{
    struct array signals;   /* item: mt_event_signal */
    struct allocator* alloc;
};

enum mt_thread_state
{
    MT_THREADSTATE_RUNNING = 0,
    MT_THREADSTATE_PAUSE,
    MT_THREADSTATE_STOP
};

struct mt_thread_data
{
    pthread_t t;
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

    pthread_attr_t attr;
    mt_mutex state_mtx;
    pthread_cond_t state_event;
    enum mt_thread_state state;
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

    result_t r = arr_create(alloc, &e->signals, sizeof(struct mt_event_signal), 8, 16, 0);
    if (IS_FAIL(r)) {
        A_FREE(alloc, e);
        return NULL;
    }

    return e;
}

void mt_event_destroy(mt_event e)
{
    for (uint i = 0; i < e->signals.item_cnt; i++) {
        struct mt_event_signal* signal = &((struct mt_event_signal*)e->signals.buffer)[i];
        pthread_cond_destroy(&signal->cond);
        mt_mutex_release(&signal->signal_mtx);
    }
    arr_destroy(&e->signals);
    A_FREE(e->alloc, e);
}

uint mt_event_addsignal(mt_event e)
{
    struct mt_event_signal* signal = (struct mt_event_signal*)arr_add(&e->signals);
    if (signal == NULL)
        return 0;

    signal->signal = FALSE;
    pthread_cond_init(&signal->cond, NULL);
    mt_mutex_init(&signal->signal_mtx);

    return e->signals.item_cnt;
}

enum mt_event_response mt_event_wait(mt_event e, uint signal_id, uint timeout)
{
    ASSERT(signal_id != 0);
    ASSERT(signal_id <= e->signals.item_cnt);

    int r = 0;
    struct mt_event_signal* signal =
		&((struct mt_event_signal*)e->signals.buffer)[signal_id-1];
    mt_mutex_lock(&signal->signal_mtx);
    if (!signal->signal) {
        if (timeout == MT_TIMEOUT_INFINITE)    {
            r = pthread_cond_wait(&signal->cond, &signal->signal_mtx);
        }   else    {
            struct timespec tmspec;
            tmspec.tv_sec = timeout/1000;
            tmspec.tv_nsec = (timeout % 1000)*1000000;
            r = pthread_cond_timedwait(&signal->cond, &signal->signal_mtx, &tmspec);
        }
    }
    signal->signal = FALSE;
    mt_mutex_unlock(&signal->signal_mtx);

    if (r == 0)
        return MT_EVENT_OK;
    else if (r == ETIMEDOUT)
        return MT_EVENT_TIMEOUT;
    else
        return MT_EVENT_ERROR;
}

enum mt_event_response mt_event_waitforall(mt_event e, uint timeout)
{
    int r = 0;
    for (uint i = 0; i < e->signals.item_cnt; i++)    {
        struct mt_event_signal* signal = &((struct mt_event_signal*)e->signals.buffer)[i];
        mt_mutex_lock(&signal->signal_mtx);
        if (!signal->signal) {
            if (timeout == MT_TIMEOUT_INFINITE)    {
                r = pthread_cond_wait(&signal->cond, &signal->signal_mtx);
            }   else    {
                struct timespec tmspec;
                tmspec.tv_sec = timeout/1000;
                tmspec.tv_nsec = (timeout % 1000)*1000000;
                r = pthread_cond_timedwait(&signal->cond, &signal->signal_mtx, &tmspec);
            }
        }
        signal->signal = FALSE;
        mt_mutex_unlock(&signal->signal_mtx);
    }

    if (r == 0)
        return MT_EVENT_OK;
    else if (r == ETIMEDOUT)
        return MT_EVENT_TIMEOUT;
    else
        return MT_EVENT_ERROR;
}

void mt_event_trigger(mt_event e, uint signal_id)
{
    ASSERT(signal_id != 0);
    ASSERT(signal_id <= e->signals.item_cnt);
    struct mt_event_signal* signal =
		&((struct mt_event_signal*)e->signals.buffer)[signal_id-1];

    mt_mutex_lock(&signal->signal_mtx);
    signal->signal = TRUE;
    pthread_cond_signal(&signal->cond);
    mt_mutex_unlock(&signal->signal_mtx);
}

/*************************************************************************************************
 * Threads
 */
mt_thread mt_thread_create(pfn_mt_thread_kernel kernel_fn,
    pfn_mt_thread_init init_fn,
    pfn_mt_thread_release release_fn,
    enum mt_thread_priority level, size_t local_mem_sz, size_t tmp_mem_sz,
    void* param1, void* param2)
{
    static uint thread_id = 1;

    result_t r;
    mt_thread thread = ALLOC(sizeof(struct mt_thread_data), 0);
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
    thread->pr = level;
    thread->param1 = param1;
    thread->param2 = param2;

    /* create thread and it's conditiion/mutex variables */
    mt_mutex_init(&thread->state_mtx);
    pthread_cond_init(&thread->state_event, NULL);

    pthread_attr_init(&thread->attr);
    pthread_attr_setdetachstate(&thread->attr, PTHREAD_CREATE_JOINABLE);
    int r2 = pthread_create(&thread->t, &thread->attr, thread_callback, thread);
    if (r2 != 0)     {
        mt_thread_destroy(thread);
        return NULL;
    }

    thread->id = thread_id++;
    return thread;
}

void mt_thread_destroy(mt_thread thread)
{
    if (thread->t != 0)  {
        /* stop the thread */
        mt_thread_stop(thread);

        /* wait for thread exit */
        pthread_join(thread->t, NULL);
    }

    pthread_attr_destroy(&thread->attr);
    mt_mutex_release(&thread->state_mtx);
    pthread_cond_destroy(&thread->state_event);

    mem_freelist_destroy(&thread->local_mem);
    mem_stack_destroy(&thread->tmp_mem);
    FREE(thread);
}

void mt_thread_pause(mt_thread thread)
{
    mt_mutex_lock(&thread->state_mtx);
    if (thread->state != MT_THREADSTATE_STOP)
        thread->state = MT_THREADSTATE_PAUSE;
    mt_mutex_unlock(&thread->state_mtx);
}

void mt_thread_resume(mt_thread thread)
{
    mt_mutex_lock(&thread->state_mtx);
    if (thread->state != MT_THREADSTATE_STOP)   {
        thread->state = MT_THREADSTATE_RUNNING;
        pthread_cond_signal(&thread->state_event);
    }
    mt_mutex_unlock(&thread->state_mtx);
}

void mt_thread_stop(mt_thread thread)
{
    mt_mutex_lock(&thread->state_mtx);
    thread->state = MT_THREADSTATE_STOP;
    pthread_cond_signal(&thread->state_event);
    mt_mutex_unlock(&thread->state_mtx);
}

void* thread_callback(void* param)
{
    result_t r;
    mt_thread thread = param;

    ASSERT(thread->kernel_fn != NULL);

    /* init */
    if (thread->init_fn != NULL)   {
        r = thread->init_fn(thread);
        if (IS_FAIL(r))     {
            goto cleanup;
        }
    }

    /* kernel */
    while (TRUE)  {
        r = thread->kernel_fn(thread);
        if (r == RET_ABORT)  {
            goto cleanup;
        }

        mt_mutex_lock(&thread->state_mtx);
        switch (thread->state)  {
            case MT_THREADSTATE_PAUSE:
            pthread_cond_wait(&thread->state_event, &thread->state_mtx);
            break;
            case MT_THREADSTATE_STOP:
            mt_mutex_unlock(&thread->state_mtx);
            goto cleanup;
            default:
            break;
        }
        mt_mutex_unlock(&thread->state_mtx);
    }

cleanup:
    /* release */
    if (thread->release_fn != NULL)
        thread->release_fn(thread);
    pthread_exit(NULL);
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

#endif /* _POSIX_ */
