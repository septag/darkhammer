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

/**
 * Author: Davide Bacchet
 */

#include "hwinfo.h"

#if defined(_OSX_)

#include <sys/types.h>
#include <sys/sysctl.h>

#include "dhcore/core.h"

int get_sys_string(const char *name, char *buf, unsigned int buflen)
{
    size_t size = buflen;
    if (sysctlbyname(name, buf, &size, NULL, 0) < 0)
        return 0;
    return 1;
}

int get_sys_int64(const char *name, int64_t *val)
{
    size_t size = sizeof(int64_t);
    if (sysctlbyname(name, val, &size, NULL, 0) < 0)
        return 0;
    return 1;
}

void query_meminfo(struct hwinfo* info)
{
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    u_int namelen = sizeof(mib) / sizeof(mib[0]);
    uint64_t mem_size;
    size_t len = sizeof(mem_size);

    if (sysctl(mib, namelen, &mem_size, &len, NULL, 0) < 0)    {
        perror("sysctl");
    }    else    {
        printf("HW.HW_MEMSIZE = %llu bytes\n", mem_size);
    }
    info->sys_mem = mem_size;
    info->sys_memfree = 0;      ///< \todo implement free memory count on OSX
}

void query_cpuinfo(struct hwinfo* info)
{
    char tmpstr[1024];
    int64_t tmpint64;
    /* vendor */
    if (get_sys_string("machdep.cpu.vendor", tmpstr, 1024)) {
        if (strstr(tmpstr, "GenuineIntel"))         info->cpu_type = HWINFO_CPU_INTEL;
        else if (strstr(tmpstr, "AuthenticAMD"))    info->cpu_type = HWINFO_CPU_AMD;
        else                                       info->cpu_type = HWINFO_CPU_UNKNOWN;
    }
    /* cpu name */
    if (get_sys_string("machdep.cpu.brand_string", tmpstr, 1024))
        strcpy(info->cpu_name, tmpstr);
    /* cpu features */
    if (get_sys_string("machdep.cpu.features", tmpstr, 1024)) {
        if (strstr(tmpstr, "MMX "))  {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_MMX);
            strcat(info->cpu_feat, "MMX ");
        }
        if (strstr(tmpstr, "SSE "))      {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE);
            strcat(info->cpu_feat, "SSE ");
        }
        if (strstr(tmpstr, "SSE2"))  {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE2);
            strcat(info->cpu_feat, "SSE2 ");
        }
        if (strstr(tmpstr, "SSE3"))      {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE3);
            strcat(info->cpu_feat, "SSE3 ");
        }
        if (strstr(tmpstr, "SSE4"))  {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE4);
            strcat(info->cpu_feat, "SSE4 ");
        }
    }
    /* clock speed MHz */
    if (get_sys_int64("hw.cpufrequency", &tmpint64))
        info->cpu_clock = (int)((double)tmpint64/1E6);
    /* cache size Kb */
    int64_t cachesize = -1;
    if (get_sys_int64("hw.l1icachesize", &tmpint64))
        if (tmpint64>cachesize) cachesize = tmpint64;
    if (get_sys_int64("hw.l2cachesize", &tmpint64))
        if (tmpint64>cachesize) cachesize = tmpint64;
    if (get_sys_int64("hw.l3cachesize", &tmpint64))
        if (tmpint64>cachesize) cachesize = tmpint64;
    info->cpu_cachesize = (uint)(cachesize/1024.0);
    /* cores */
    if (get_sys_int64("machdep.cpu.core_count", &tmpint64))
        info->cpu_pcore_cnt = (uint)(tmpint64);
    if (get_sys_int64("machdep.cpu.thread_count", &tmpint64))
        info->cpu_core_cnt = (uint)(tmpint64);
    /* cache line */
    if (get_sys_int64("hw.cachelinesize", &tmpint64))
        info->cpu_cacheline = (uint)(tmpint64);
}

void query_osinfo(struct hwinfo* info)
{
    info->os_type = HWINFO_OS_OSX;
    char data_sysname[128];
    char data_sysversion[128];
    get_sys_string("kern.ostype", data_sysname, 128);
    get_sys_string("kern.osrelease", data_sysversion, 128);
    sprintf(info->os_name, "%s - %s", data_sysname, data_sysversion);
}

uint query_clockspeed(uint cpu_idx)
{
    return 0;
}


#endif /* _OSX_ */
