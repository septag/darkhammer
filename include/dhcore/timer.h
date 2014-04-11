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


#ifndef __TIMER_H__
#define __TIMER_H__

#include "types.h"
#include "linked-list.h"
#include "core-api.h"
#include "pool-alloc.h"

/**
 * @defgroup timer Timers
 */

/**
 * basic timer structure, holds data for each timer instance
 * @ingroup timer
 */
struct timer
{
    float t; /**< elapsed time */
    float dt; /**< delta time */
    float rate; /**< playrate, =0.0 stopped, =1.0 normal */
    struct linked_list node; /**< linked-list node */
};

#define TIMER_PAUSE(tm) tm->rate = 0;
#define TIMER_START(tm) tm->rate = 1.0f;
#define TIMER_SCALE(tm, s) tm->rate *= s;
#define TIMER_STOP(tm) tm->rate = 0;    tm->t = 0.0f;   tm->dt = 0.0f;

/**
 * initialize timer manager
 * @ingroup timer
 */
result_t timer_initmgr();

/**
 * release timer manager
 * @ingroup timer
 */
void timer_releasemgr();

/**
 * add timers to the timer_mgr \n
 * added timers will be updated after each call to timer_update
 * @param start defines if timer should be started immediately after added to the manager
 * @see timer_update
 * @see timer_destroyinstance
 * @ingroup timer
 */
CORE_API struct timer* timer_createinstance(int start);

/**
 * remove timer from timer_mgr\n
 * removed timer will no longer be updated
 * @ingroup timer
 */
CORE_API void timer_destroyinstance(struct timer* tm);

/**
 * update all added timers in timer_mgr
 * @param tick timer tick of current point in time
 * @see timer_querytick
 * @ingroup timer
 */
CORE_API void timer_update(uint64 tick);

/**
 * query cpu tick time
 * @ingroup timer
 */
CORE_API uint64 timer_querytick();

/**
 * update frequency value of timer_mgr
 * @ingroup timer
 */
CORE_API uint64 timer_queryfreq();

/**
 * calculates the time between two ticks
 * @ingroup timer
 */
CORE_API fl64 timer_calctm(uint64 tick1, uint64 tick2);

/**
 * Pause all timers
 * @ingroup timer
 */
CORE_API void timer_pauseall();

/**
 * Resume all timers
 * @ingroup timer
 */
CORE_API void timer_resumeall();

#endif /*__TIMER_H__*/
