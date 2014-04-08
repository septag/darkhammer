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

#include "hwinfo.h"

#if defined(_LINUX_)

#include "dhcore/core.h"
#include <sys/utsname.h>
#include <unistd.h>

void query_meminfo(struct hwinfo* info)
{
    char* data = util_runcmd("cat /proc/meminfo");
    if (data != NULL)	{
        char* token = strtok(data, "\n");
        while (token != NULL)	{
            if (strstr(token, "MemTotal:"))	{
                str_trim(token, strlen(token), token, " \t");
                info->sys_mem = str_toint32(strchr(token, ':') + 1);
            }	else if (strstr(token, "Active:"))	{
                str_trim(token, strlen(token), token, " \t");
                info->sys_memfree = info->sys_mem - str_toint32(strchr(token, ':') + 1);
            }

            if (info->sys_mem != 0 && info->sys_memfree != 0)
                break;

            token = strtok(NULL, "\n");
        }
        FREE(data);
    }
}

void query_cpuinfo(struct hwinfo* info)
{
    char* data = util_runcmd("cat /proc/cpuinfo");
    if (data != NULL)	{
        char* token = strtok(data, "\n");
        while (token != NULL)	{
            /* vendor */
            if (strstr(token, "vendor_id"))		{
                if (strstr(token, "GenuineIntel"))
                    info->cpu_type = HWINFO_CPU_INTEL;
                else if (strstr(token, "AuthenticAMD"))
                    info->cpu_type = HWINFO_CPU_AMD;
                else
                    info->cpu_type = HWINFO_CPU_UNKNOWN;
            }

            /* cpu name */
            else if (strstr(token, "model name"))	{
                if (!str_isempty(info->cpu_name))
                    break;
                strcpy(info->cpu_name, strchr(token, ':') + 2);
            }

            /* cpu features */
            else if (strstr(token, "flags"))	{
                if (strstr(token, "mmx "))	{
                    BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_MMX);
                    strcat(info->cpu_feat, "MMX ");
                }
                if (strstr(token, "sse "))		{
                    BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE);
                    strcat(info->cpu_feat, "SSE ");
                }
                if (strstr(token, "sse2"))	{
                    BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE2);
                    strcat(info->cpu_feat, "SSE2 ");
                }
                if (strstr(token, "sse3"))		{
                    BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE3);
                    strcat(info->cpu_feat, "SSE3 ");
                }
                if (strstr(token, "sse4"))	{
                    BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE4);
                    strcat(info->cpu_feat, "SSE4 ");
                }
            }

            /* clock speed */
            else if (strstr(token, "cpu MHz"))	{
                info->cpu_clock = str_toint32(strchr(token, ':') + 2);
            }

            /* cache */
            else if (strstr(token, "cache size"))	{
                info->cpu_cachesize = str_toint32(strchr(token, ':') + 2);
            }

            /* cores */
            else if (strstr(token, "cpu cores"))	{
                info->cpu_core_cnt = str_toint32(strchr(token, ':') + 2);
                info->cpu_pcore_cnt = info->cpu_core_cnt;
            }

            /* cache line */
            else if (strstr(token, "cache_alignment"))	{
                info->cpu_cacheline = str_toint32(strchr(token, ':') + 2);
            }

            token = strtok(NULL, "\n");
        }
        FREE(data);
    }

    info->cpu_core_cnt = maxui(info->cpu_core_cnt, 1);
    info->cpu_pcore_cnt = maxui(info->cpu_pcore_cnt, 1);
}

void query_osinfo(struct hwinfo* info)
{
    struct utsname data;
    uname(&data);
    info->os_type = HWINFO_OS_LINUX;
    sprintf(info->os_name, "%s %s - %s", data.sysname, data.machine, data.release);
}

uint query_clockspeed(uint cpu_idx)
{
    return 0;
}

#endif /* _LINUX_ */
