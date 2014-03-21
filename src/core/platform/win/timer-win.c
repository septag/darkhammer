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

#if defined(_WIN_)

#include "win.h"

uint64 timer_queryfreq()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (uint64)freq.QuadPart;
}

uint64 timer_querytick()
{
    LARGE_INTEGER tick;
    QueryPerformanceCounter(&tick);
    return (uint64)(tick.QuadPart);
}

#endif /* _WIN_ */
