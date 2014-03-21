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

#if defined(_LINUX_)

#include <time.h>

 #define BILLION 1000000000L

uint64 timer_queryfreq()
{
    struct timespec t;
    clock_getres(CLOCK_MONOTONIC, &t);
    return (uint64)t.tv_nsec*BILLION;
}

uint64 timer_querytick()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64)t.tv_sec*BILLION + (uint64)t.tv_nsec;
}

#endif /* _LINUX_ */
