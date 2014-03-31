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

/**
 * @mainpage Engine library API reference
 * Current version: 0.4.7
 */

/**
 * @defgroup eng Engine
 */

#ifndef __ENGINE_H__
#define __ENGINE_H__

#include "dhcore/types.h"
#include "engine-api.h"
#include "dhcore/allocator.h"
#include "init-params.h"

/* fwd */
struct file_mgr;
struct timer_mgr;
struct hwinfo;

struct frame_stats
{
    uint64 start_tick;
    uint frame;
    float ft;
    uint fps;
};

struct eng_mem_stats
{
    size_t data_max;
    size_t data_size;
    size_t lsr_max;
    size_t lsr_size;
    size_t tmp0_total;
    size_t tmp0_max;    /* maximum memory allocated by main temp allocator */
    uint tmp0_max_frameid;
};

void eng_zero();

/**
 * Initializes the engine using init parameters\n
 * @param params Init params structure, should be constructed by user or parsed from json file
 * @see eng_load_params
 * @see init_params
 * @see eng_release
 * @ingroup eng
 */
ENGINE_API result_t eng_init(const struct init_params* params);

/**
 * Releases engine and all it's sub-systems, must be called before @e app_release
 * @see app_release
 * @see eng_init
 * @ingroup eng
 */
ENGINE_API void eng_release();

/**
 * Progress and renders the frame, all that is needed to be done in one frame is done by this function\n
 * Updates components, renders scene, updates physics, etc.
 * @ingroup eng
 */
ENGINE_API void eng_update();

/**
 * Returns load-stay-resident memory allocator\n
 * Should be used for allocating globally resident objects and structures at engine initialization
 * @ingroup eng
 */
ENGINE_API struct allocator* eng_get_lsralloc();

/**
 * Returns main data allocator, for allocating main scene objects
 * @ingroup eng
 */
ENGINE_API struct allocator* eng_get_dataalloc();

/**
 * Sends crucial keyboard messages to the UI for resposive in-game console\n
 * @param c Is the last input character, in windows, it is mainly the result of WM_CHAR's 'wparam' argument
 * @param vkey Virtual-key code of the last key pressed. (os-dependent) \n
 * in windows, it is mainly the result of WM_KEYDOWN's 'wparam' argument
 * @ingroup eng
 */
ENGINE_API void eng_send_guimsgs(char c, uint vkey);

/**
 * Pauses time and thus all simulations
 * @ingroup eng
 */
ENGINE_API void eng_pause();

/**
 * Resumes time and all simulations
 * @ingroup eng
 */
ENGINE_API void eng_resume();

/**
 * Returns hwinfo structure, containing collected info about present hardware
 * @ingroup eng
 */
ENGINE_API const struct hwinfo* eng_get_hwinfo();

/**
 * Returns shared root path
 * @ingroup eng
 */
ENGINE_API const char* eng_get_sharedir();

/* internal */
_EXTERN_BEGIN_

ENGINE_API const struct frame_stats* eng_get_framestats();
ENGINE_API float eng_get_frametime();
const struct init_params* eng_get_params();
void eng_get_memstats(struct eng_mem_stats* stats);

_EXTERN_END_

#endif /* __ENGINE_H__ */
