/***********************************************************************************
 * Copyright (c) 2013, Davide Bacchet
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

#if defined(_OSX_)

#include <mach/mach_time.h>

uint64 timer_queryfreq()
{
    mach_timebase_info_data_t info;
    mach_timebase_info( &info );
    return 1e9*(double)info.numer/(double)info.denom;
}

uint64 timer_querytick()
{
    return mach_absolute_time();
}


#endif /* _OSX_ */
