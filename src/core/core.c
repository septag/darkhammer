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

#include "core.h"
#include "json.h"
#include "file-io.h"
#include "timer.h"
#include "crash.h"

result_t core_init(uint flags)
{
    if (BIT_CHECK(flags, CORE_INIT_CRASHDUMP))  {
        if (IS_FAIL(crash_init()))
            return RET_FAIL;
    }

    if (IS_FAIL(mem_init(BIT_CHECK(flags, CORE_INIT_TRACEMEM))))
        return RET_FAIL;

    if (IS_FAIL(log_init()))
        return RET_FAIL;

    if (BIT_CHECK(flags, CORE_INIT_ERRORS)) {
        if (IS_FAIL(err_init()))
            return RET_FAIL;
    }

    rand_seed();

    if (BIT_CHECK(flags, CORE_INIT_JSON))   {
        if (IS_FAIL(json_init()))
            return RET_FAIL;
    }

    if (BIT_CHECK(flags, CORE_INIT_FILEIO)) {
        if (IS_FAIL(fio_initmgr()))
            return RET_FAIL;
    }

    if (BIT_CHECK(flags, CORE_INIT_TIMER)) {
        if (IS_FAIL(timer_initmgr()))
            return RET_FAIL;
    }

    return RET_OK;
}

void core_release(bool_t report_leaks)
{
	timer_releasemgr();

    fio_releasemgr();

    json_release();

    err_release();

    /* dump memory leaks before releasing memory manager and log
     * because memory leak report dumps leakage data to logger */
    if (report_leaks)
        mem_reportleaks();

    log_release();
    mem_release();
}
