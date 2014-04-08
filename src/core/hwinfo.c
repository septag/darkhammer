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

#include <stdio.h>

#include "hwinfo.h"
#include "log.h"

/* fwd (implemented in platform sources - see platform/${PLATFORM} */
void query_meminfo(struct hwinfo* info);
void query_cpuinfo(struct hwinfo* info);
void query_osinfo(struct hwinfo* info);
uint query_clockspeed(uint cpu_idx);

/*  */
void hw_getinfo(struct hwinfo* info, uint flags)
{
    memset(info, 0x00, sizeof(struct hwinfo));

    if (BIT_CHECK(flags, HWINFO_MEMORY))
        query_meminfo(info);

    if (BIT_CHECK(flags, HWINFO_CPU))
        query_cpuinfo(info);

    if (BIT_CHECK(flags, HWINFO_OS))
        query_osinfo(info);
}

void hw_printinfo(const struct hwinfo* info, uint flags)
{
    if (BIT_CHECK(flags, HWINFO_CPU))        {
    	log_print(LOG_INFO, "  cpu:");
        log_printf(LOG_INFO, "\tcpu vendor: %s", info->cpu_name);
        log_printf(LOG_INFO, "\tcpu clock-speed: %d(MHz)", info->cpu_clock);
        log_printf(LOG_INFO, "\tcpu features: %s", info->cpu_feat);
        log_printf(LOG_INFO, "\tcpu L2 cache: %d(kb)", info->cpu_cachesize*2);
        log_printf(LOG_INFO, "\tcpu physical cores: %d", info->cpu_pcore_cnt);
        log_printf(LOG_INFO, "\tcpu logical cores: %d", info->cpu_core_cnt);
    }

    if (BIT_CHECK(flags, HWINFO_MEMORY))        {
    	log_print(LOG_INFO, "  memory:");
        log_printf(LOG_INFO, "\tsystem memory: %d(mb)", info->sys_mem/1024);
        log_printf(LOG_INFO, "\tfree memory: %d(mb)", info->sys_memfree/1024);
    }

    if (BIT_CHECK(flags, HWINFO_OS))        {
    	log_print(LOG_INFO, "  system:");
        log_printf(LOG_INFO, "\tos: %s", info->os_name);
    }
}
