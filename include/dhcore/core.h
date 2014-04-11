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

#ifndef __CORE_H__
#define __CORE_H__

/**
 * @mainpage Core library API reference
 * Current version: 0.4.7
 */

 /**
 * @defgroup core
 * Core library essential headers and init/release\n
 * Include "core.h" and use core_init, core_release to initialize basic core functionality
 * Example: @code
 * #include "dhcore/core.h"
 * int main()
 * {
 *      core_init(TRUE);
 *      // continue with application and engine initialization and update
 *      core_release();
 * }
 * @endcode
 */

 /* essential headers */
#include <stdio.h>

#include "types.h"
#include "mem-mgr.h"
#include "err.h"
#include "log.h"
#include "core-api.h"
#include "numeric.h"
#include "str.h"
#include "allocator.h"
#include "util.h"

enum core_init_flags
{
    CORE_INIT_TRACEMEM = (1<<0),
    CORE_INIT_CRASHDUMP = (1<<1),
    CORE_INIT_LOGGER = (1<<2),
    CORE_INIT_ERRORS = (1<<3),
    CORE_INIT_JSON = (1<<4),
    CORE_INIT_FILEIO = (1<<5),
    CORE_INIT_TIMER = (1<<6),
    CORE_INIT_ALL = 0xffffffff
};

/**
 * Initializes/releases main @e core library components\n
 * It initializes following core library sub-systems:
 * - Memory manager
 * - Logger
 * - Error handling
 * - Random seed
 * - JSON parser
 * - File manager
 * - Timers
 * @ingroup core
 */
CORE_API result_t core_init(uint flags);

/**
 * Release core components
 * @param report_leaks report memory leaks to the logger
 * @ingroup core
 */
CORE_API void core_release(int report_leaks);


#endif /* __CORE_H__ */
