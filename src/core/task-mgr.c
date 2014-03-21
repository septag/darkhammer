/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
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

#include "core.h"
#include "mt.h"
#include "freelist-alloc.h"
#include "queue.h"
#include "stack.h"
#include "array.h"
#include "pool-alloc.h"
#include "hash-table.h"
#include "task-mgr.h"
#include "stack-alloc.h"

#define LOCAL_MEM_SIZE (1024*1024)
#define TEMP_MEM_SIZE (4*1024*1024)
#define FREE_JOBS_BLOCK_SIZE 64

/*************************************************************************************************
 * types
 */
struct tsk_worker
{
    uint thread_id;
    uint finish_signal_id;
    uint idx;
};

struct tsk_job
{
    uint id;
    pfn_tsk_run run_fn;
    mt_event finish_event;
    void* params;
    void* result;
    uint worker_cnt;
    struct tsk_worker* workers;
    struct hashtable_fixed worker_tbl;  /* key: thread_id, value: index of worker */
    struct queue qnode;
    long volatile finished_cnt; /* atomic finished counter (if == worker_cnt then it's all finished) */
};

struct tsk_thread
{
    mt_thread t;
    struct queue* job_queue;    /* item: tsk_job */
    mt_mutex job_queue_mtx;
    long volatile queue_isempty;
    long volatile quit;
};

struct tsk_mgr
{
    uint flags;
    uint thread_cnt;
    uint job_cnt;

    uint* thread_idxs;    /* tmp buffer, init count=thread_cnt+1 */
    struct tsk_thread* threads;
    struct array jobs;  /* item: tsk_job */

    struct stack* free_jobs;    /* data: uint (index to jobs) */
    struct pool_alloc free_jobs_pool;   /* item: struct stack */

    /* allocators for main thread */
    struct stack_alloc tmp_mem;
    struct allocator tmp_alloc;
    struct freelist_alloc main_mem;
    struct allocator main_alloc;
};

/*************************************************************************************************
 * Fwd declare
 */
result_t tsk_kernel_fn(mt_thread thread);

void tsk_job_destroy(struct tsk_job* job);
result_t tsk_thread_init(struct tsk_thread* thread, size_t localmem_perthread_sz,
                         size_t tmpmem_perthread_sz);
void tsk_thread_release(struct tsk_thread* thread);
uint tsk_job_create(pfn_tsk_run run_fn, void* params, void* result, const uint* thread_idxs,
                      uint thread_cnt);
void tsk_queuejob(uint job_id, const uint* thread_idxs, uint thread_cnt,
                  pfn_tsk_run run_fn, void* params, void* result);


/*************************************************************************************************
 * Globals
 */
static struct tsk_mgr g_tsk;
static bool_t g_tsk_zero = FALSE;

/*************************************************************************************************
 * Inlines
 */
INLINE struct tsk_job* tsk_job_get(uint job_id)
{
    ASSERT(job_id != 0);
    return &((struct tsk_job*)g_tsk.jobs.buffer)[job_id - 1];
}

/*************************************************************************************************/
result_t tsk_initmgr(uint thread_cnt, size_t localmem_perthread_sz, size_t tmpmem_perthread_sz,
                     uint flags)
{
    ASSERT(thread_cnt != 0);

    memset(&g_tsk, 0x00, sizeof(struct tsk_mgr));
    g_tsk_zero = TRUE;

    result_t r;
    g_tsk.flags = flags;

    /* worker threads */
    if (localmem_perthread_sz == 0)
        localmem_perthread_sz = LOCAL_MEM_SIZE;
    if (tmpmem_perthread_sz == 0)
        tmpmem_perthread_sz = TEMP_MEM_SIZE;

    g_tsk.threads = (struct tsk_thread*)ALLOC(sizeof(struct tsk_thread)*thread_cnt, 0);
    if (g_tsk.threads == NULL)  {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_FAIL;
    }

    for (uint i = 0; i < thread_cnt; i++) {
        if (IS_FAIL(tsk_thread_init(&g_tsk.threads[i], localmem_perthread_sz, tmpmem_perthread_sz)))
        {
            err_print(__FILE__, __LINE__, "task-mgr init failed: could not initialize threads");
            return RET_FAIL;
        }
    }

    g_tsk.thread_idxs = (uint*)ALLOC(sizeof(uint)*(thread_cnt+1), 0);
    if (g_tsk.thread_idxs == NULL)  {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_FAIL;
    }
    g_tsk.thread_cnt = thread_cnt;

    /* local/temp memory for main thread */
    r = mem_stack_create(mem_heap(), &g_tsk.tmp_mem, tmpmem_perthread_sz, 0);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_FAIL;
    }
    mem_stack_bindalloc(&g_tsk.tmp_mem, &g_tsk.tmp_alloc);

    r = mem_freelist_create(mem_heap(), &g_tsk.main_mem, localmem_perthread_sz, 0);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_FAIL;
    }
    mem_freelist_bindalloc(&g_tsk.main_mem, &g_tsk.main_alloc);

    /* jobs array */
    r = arr_create(mem_heap(), &g_tsk.jobs, sizeof(struct tsk_job), 64, 128, 0);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_FAIL;
    }

    r = mem_pool_create(mem_heap(), &g_tsk.free_jobs_pool, sizeof(struct tsk_job), 128, 0);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_FAIL;
    }

    return RET_OK;
}

result_t tsk_thread_init(struct tsk_thread* thread, size_t localmem_perthread_sz,
                         size_t tmpmem_perthread_sz)
{
    memset(thread, 0x00, sizeof(struct tsk_thread));

    mt_mutex_init(&thread->job_queue_mtx);

    thread->t =  mt_thread_create(tsk_kernel_fn, NULL, NULL,
        MT_THREAD_NORMAL, localmem_perthread_sz, tmpmem_perthread_sz, thread, NULL);
    if (thread->t == NULL)
        return RET_FAIL;

    return RET_OK;
}

void tsk_releasemgr()
{
    if (!g_tsk_zero)
        return;

    if (g_tsk.job_cnt > 0)
        log_printf(LOG_WARNING, "Destroying %d unfinished/unreleased tasks", g_tsk.job_cnt);

    for (uint i = 0; i < g_tsk.jobs.item_cnt; i++)
        tsk_job_destroy(&((struct tsk_job*)g_tsk.jobs.buffer)[i]);

    for (uint i = 0; i < g_tsk.thread_cnt; i++)   {
        MT_ATOMIC_SET(g_tsk.threads[i].quit, TRUE);
        tsk_thread_release(&g_tsk.threads[i]);
    }

    if (g_tsk.thread_idxs != NULL)
        FREE(g_tsk.thread_idxs);

    arr_destroy(&g_tsk.jobs);
    mem_pool_destroy(&g_tsk.free_jobs_pool);

    mem_freelist_destroy(&g_tsk.main_mem);
    mem_stack_destroy(&g_tsk.tmp_mem);

    if (g_tsk.threads != NULL)
        FREE(g_tsk.threads);
}

void tsk_thread_release(struct tsk_thread* thread)
{
    if (thread->t != NULL)
        mt_thread_destroy(thread->t);

    mt_mutex_release(&thread->job_queue_mtx);
}

void tsk_destroy(uint job_id)
{
    struct tsk_job* job = tsk_job_get(job_id);
    tsk_job_destroy(job);

    /* push to free items stack */
    struct stack* free_job_item = (struct stack*)mem_pool_alloc(&g_tsk.free_jobs_pool);
    ASSERT(free_job_item);
    uptr_t idx = job_id - 1;
    stack_push(&g_tsk.free_jobs, free_job_item, (void*)idx);
}

void tsk_job_destroy(struct tsk_job* job)
{
    if (job->id == 0)
        return;

    hashtable_fixed_destroy(&job->worker_tbl);
    if (job->workers != NULL)
        A_FREE(&g_tsk.main_alloc, job->workers);
    if (job->finish_event != NULL)
        mt_event_destroy(job->finish_event);

    job->id = 0;    /* zero ID means that job is invalid */
    g_tsk.job_cnt --;
}

/* must be called from main thread */
uint tsk_dispatch(pfn_tsk_run run_fn, enum tsk_run_context ctx, uint thread_cnt,
                    void* params, void* result)
{
    /* look for available threads based on specified context mode */
    uint tsk_thread_cnt = g_tsk.thread_cnt;
    thread_cnt = maxun(minun(thread_cnt, tsk_thread_cnt+1), 1);
    uint cnt = 0;
    uint* thread_idxs = g_tsk.thread_idxs;

    switch (ctx)    {
    case TSK_CONTEXT_ALL:
        thread_idxs[cnt++] = INVALID_INDEX;
    case TSK_CONTEXT_ALL_NO_MAIN:
        for (uint i = 0; i < tsk_thread_cnt && cnt < thread_cnt; i++)
            thread_idxs[cnt++] = i;
        break;
    case TSK_CONTEXT_FREE:
        thread_idxs[cnt++] = INVALID_INDEX;
    case TSK_CONTEXT_FREE_NO_MAIN:
        for (uint i = 0; i < tsk_thread_cnt && cnt < thread_cnt; i++)   {
            if (g_tsk.threads[i].queue_isempty)
                thread_idxs[cnt++] = i;
        }
        break;
    }

    /* only may occur in TSK_CONTEXT_FREE_NO_MAIN case */
    if (cnt == 0)
        return 0;

    /* setup task and it's workers */
    uint job_id = tsk_job_create(run_fn, params, result, thread_idxs, cnt);
    if (job_id == 0)
        return 0;

    tsk_queuejob(job_id, thread_idxs, cnt, run_fn, params, result);

    return job_id;
}

uint tsk_dispatch_exclusive(pfn_tsk_run run_fn, const uint* thread_idxs, uint thread_cnt,
                              void* params, void* result)
{
    thread_cnt = minun(thread_cnt, g_tsk.thread_cnt);
    uint job_id = tsk_job_create(run_fn, params, result, thread_idxs, thread_cnt);
    if (job_id == 0)
        return 0;

    tsk_queuejob(job_id, thread_idxs, thread_cnt, run_fn, params, result);
    return job_id;
}

uint tsk_job_create(pfn_tsk_run run_fn, void* params, void* result, const uint* thread_idxs,
                      uint thread_cnt)
{
    ASSERT(run_fn);

    struct stack* free_job_item;
    struct tsk_job* job;
    uint id = 0;

    if ((free_job_item = stack_pop(&g_tsk.free_jobs)) != NULL)  {
        struct tsk_job* pjobs = (struct tsk_job*)g_tsk.jobs.buffer;
        uptr_t free_idx = (uptr_t)free_job_item->data;
        job = &pjobs[free_idx];
        id = (uint)free_idx + 1;
        mem_pool_free(&g_tsk.free_jobs_pool, free_job_item);
    }   else    {
        job = (struct tsk_job*)arr_add(&g_tsk.jobs);
        if (job == NULL)
            return 0;
        id = g_tsk.jobs.item_cnt;
    }
    memset(job, 0x00, sizeof(struct tsk_job));

    job->id = id;
    if (thread_cnt > 1 || thread_idxs[0] != INVALID_INDEX)
        job->finish_event = mt_event_create(&g_tsk.main_alloc);
    job->run_fn = run_fn;
    job->params = params;
    job->result = result;
    job->workers = (struct tsk_worker*)A_ALLOC(&g_tsk.main_alloc,
        sizeof(struct tsk_worker)*thread_cnt, 0);
    if (job->workers == NULL ||
        IS_FAIL(hashtable_fixed_create(&g_tsk.main_alloc, &job->worker_tbl, thread_cnt, 0)))
    {
        tsk_destroy(id);
        return 0;
    }
    job->worker_cnt = thread_cnt;

    for (uint i = 0; i < thread_cnt; i++) {
        uint thread_id = (thread_idxs[i] != INVALID_INDEX) ?
            mt_thread_getid(g_tsk.threads[thread_idxs[i]].t) : 0;
        hashtable_fixed_add(&job->worker_tbl, thread_id, i);

        job->workers[i].thread_id = thread_id;
        job->workers[i].finish_signal_id = (thread_id != 0) ? mt_event_addsignal(job->finish_event) :
            0;
        job->workers[i].idx = i;
    }

    g_tsk.job_cnt ++;
    return id;
}

void tsk_queuejob(uint job_id, const uint* thread_idxs, uint thread_cnt,
                  pfn_tsk_run run_fn, void* params, void* result)
{
    /* dispatch them to thread queues */
    struct tsk_job* job = (struct tsk_job*)tsk_job_get(job_id);
    uint main_thread_work = INVALID_INDEX;

    for (uint i = 0; i < thread_cnt; i++)    {
        struct tsk_worker* worker = &job->workers[i];
        if (worker->thread_id == 0) {
            main_thread_work = i;
        }   else    {
            ASSERT(thread_idxs[i] != INVALID_INDEX);
            struct tsk_thread* tt = &g_tsk.threads[thread_idxs[i]];
            mt_mutex_lock(&tt->job_queue_mtx);
            bool_t first_node = (tt->job_queue == NULL);
            queue_push(&tt->job_queue, &job->qnode, job);
            mt_mutex_unlock(&tt->job_queue_mtx);
            /* we pushed a new job, resume thread */
            if (first_node) {
                MT_ATOMIC_SET(tt->queue_isempty, FALSE);
                mt_thread_resume(tt->t);
            }
        }
    }

    /* main thread, starts immediately in the caller thread */
    if (main_thread_work != INVALID_INDEX)  {
        run_fn(params, result, 0, job->id, main_thread_work);
        MT_ATOMIC_INCR(job->finished_cnt);
    }
}

void tsk_wait(uint job_id)
{
    struct tsk_job* job = tsk_job_get(job_id);
    mt_event_waitforall(job->finish_event, MT_TIMEOUT_INFINITE);
}

bool_t tsk_check_finished(uint job_id)
{
    struct tsk_job* job = tsk_job_get(job_id);
    return (job->finished_cnt == job->worker_cnt);
}

struct allocator* tsk_get_localalloc(uint thread_id)
{
    if (thread_id == 0)
        return &g_tsk.main_alloc;
    else    {
        for (uint i = 0; i < g_tsk.thread_cnt; i++)   {
            if (mt_thread_getid(g_tsk.threads[i].t) == thread_id)
                return mt_thread_getlocalalloc(g_tsk.threads[i].t);
        }
        ASSERT(0);
        return NULL;
    }
}

struct allocator* tsk_get_tmpalloc(uint thread_id)
{
    if (thread_id == 0)   {
        return &g_tsk.tmp_alloc;
    }   else    {
        for (uint i = 0; i < g_tsk.thread_cnt; i++)   {
            if (mt_thread_getid(g_tsk.threads[i].t) == thread_id)
                return mt_thread_gettmpalloc(g_tsk.threads[i].t);
        }

        ASSERT(0);
        return NULL;
    }
}

/* running in worker threads */
result_t tsk_kernel_fn(mt_thread thread)
{
    struct tsk_thread* tt = (struct tsk_thread*)mt_thread_getparam1(thread);

    if (tt->quit)
        return RET_ABORT;

    /* check thread queue for remaining jobs, if anything is poped, execute it
     * Pause the thread if no jobs found in the queue */
    mt_mutex_lock(&tt->job_queue_mtx);
    struct queue* job_item = queue_pop(&tt->job_queue);

    /* pause the thread if we have no jobs in the queue */
    if (job_item == NULL)   {
        mt_thread_pause(thread);
        MT_ATOMIC_SET(tt->queue_isempty, TRUE);
    }

    mt_mutex_unlock(&tt->job_queue_mtx);

    if (job_item != NULL) {
        /* reset temp allocator before executing any jobs */
        mt_thread_resettmpalloc(thread);

        struct tsk_job* job = (struct tsk_job*)job_item->data;
        struct hashtable_item* worker_item = hashtable_fixed_find(&job->worker_tbl,
            mt_thread_getid(thread));
        if (worker_item != NULL)    {
            struct tsk_worker* worker = &job->workers[worker_item->value];
            job->run_fn(job->params, job->result, worker->thread_id, job->id, worker->idx);
            mt_event_trigger(job->finish_event, worker->finish_signal_id);
            MT_ATOMIC_INCR(job->finished_cnt);
        }
    }

    return RET_OK;
}

void* tsk_get_params(uint job_id)
{
    return tsk_job_get(job_id)->params;
}

void* tsk_get_result(uint job_id)
{
    return tsk_job_get(job_id)->result;
}

