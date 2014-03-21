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

#ifndef __TASKMGR_H__
#define __TASKMGR_H__

#include "types.h"
#include "allocator.h"
#include "core-api.h"

enum tsk_run_context
{
    TSK_CONTEXT_ALL, /* assign task to all threads, doesn't care if they have pending tasks or not */
    TSK_CONTEXT_FREE, /* assign task to free threads (no pending tasks) */
    TSK_CONTEXT_ALL_NO_MAIN, /* assign task to all threads, except the main one */
    TSK_CONTEXT_FREE_NO_MAIN /* assign task to free threads, except the main one */
};

#define TSK_THREADS_ALL INVALID_INDEX

/**
 * Callback for task run
 * @param params Custom params for task function, submitted by @e tsk_dispatch
 * @param result Result structure for task function, submitted by @e tsk_dispatch
 * @param thread_id Running thread ID
 * @param job_id Current running task ID
 * @param worker_idx Job index for task. For example, if task is submitted to 2 threads,
 * there would be (0, 1) indexes dispatched to each callback function
 */
typedef void (*pfn_tsk_run)(void* params, void* result, uint thread_id, uint job_id,
                            uint worker_idx);

CORE_API result_t tsk_initmgr(uint thread_cnt, size_t localmem_perthread_sz,
                              size_t tmpmem_perthread_sz, uint flags);
CORE_API void tsk_releasemgr();

/* dispatch must be called from main thread only */
CORE_API uint tsk_dispatch(pfn_tsk_run run_fn, enum tsk_run_context ctx, uint thread_cnt,
                             void* params, void* result);
CORE_API uint tsk_dispatch_exclusive(pfn_tsk_run run_fn, const uint* thread_idxs,
                                       uint thread_cnt, void* params, void* result);
CORE_API void tsk_destroy(uint job_id);
CORE_API void tsk_wait(uint job_id);
CORE_API bool_t tsk_check_finished(uint job_id);

CORE_API struct allocator* tsk_get_localalloc(uint thread_id);
CORE_API struct allocator* tsk_get_tmpalloc(uint thread_id);

CORE_API void* tsk_get_params(uint job_id);
CORE_API void* tsk_get_result(uint job_id);

#endif /* __TASKMGR_H__ */