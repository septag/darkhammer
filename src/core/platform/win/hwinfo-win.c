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

#if defined(_WIN_)

#include "dhcore/core.h"
#include "dhcore/win.h"
#include <intrin.h>

void query_meminfo(struct hwinfo* info)
{
    MEMORYSTATUSEX status;
    memset(&status, 0x00, sizeof(MEMORYSTATUS));
    status.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatusEx(&status);

    info->sys_mem = (size_t)status.ullTotalPhys;
    info->sys_memfree = (size_t)status.ullAvailPhys;
}

uint query_clockspeed(uint cpu_idx)
{
    uint clock_speed = 0;
    HKEY key_hdl;
    char key[128];
    uint len;

    sprintf(key, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\%d\\", cpu_idx);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE, &key_hdl) != ERROR_SUCCESS)   {
        return 0;
    }

    /* read the clock speed */
    if (RegQueryValueEx(key_hdl, "~MHz", NULL, NULL,
        (uint8*)&clock_speed, (LPDWORD)&len) != ERROR_SUCCESS)
    {
        RegCloseKey(key_hdl);
        return 0;
    }

    RegCloseKey(key_hdl);
    return clock_speed;
}

void query_cpuinfo(struct hwinfo* info)
{
    uint high_feat;
    uint high_featex;
    int buff[4];
    char man[13];

    /*  */
    __cpuid(buff, 0);
    high_feat = (uint)(buff[0]);
    *(int*)&man[0] = buff[1];
    *(int*)&man[4] = buff[3];
    *(int*)&man[8] = buff[2];
    man[12] = 0;

    if (str_isequal(man, "AuthenticAMD"))       info->cpu_type = HWINFO_CPU_AMD;
    else if (str_isequal(man, "GenuineIntel"))  info->cpu_type = HWINFO_CPU_INTEL;
    else                                        info->cpu_type = HWINFO_CPU_UNKNOWN;

    /* Cpu Hyper-Threading and Number of Cores */
    if (high_feat >= 1)        {
        __cpuid(buff, 1);
        info->cpu_core_cnt = (buff[1]>>16)&0xFF;
        /* this doesn't work !
        if (high_feat >= 4) {
            __cpuid(buff, 4);
            info->cpu_pcore_cnt = (buff[0]&0xFC000000)>>26;
        }
        info->cpu_pcore_cnt ++;
        */
        info->cpu_pcore_cnt = info->cpu_core_cnt;
    }

    /* Get Highest Extended Feature */
    __cpuid(buff, 0x80000000);
    high_featex = (uint)(buff[0]);
    if (high_featex > 0x80000004)    {
        char cpu_name[49];
        cpu_name[0] = 0;

        __cpuid((int*)&cpu_name[0], 0x80000002);
        __cpuid((int*)&cpu_name[16], 0x80000003);
        __cpuid((int*)&cpu_name[32], 0x80000004);
        cpu_name[48] = 0;

        /* remove Spaces from the end of the string */
        for (int i = (int)strlen(cpu_name) - 1; i >= 0; --i)        {
            if (cpu_name[i] == ' ')        cpu_name[i] = '\0';
            else                           break;
        }

        sprintf(info->cpu_name, "%s(%s)", cpu_name, man);
    }

    /* cpu features */
    info->cpu_feat[0] = 0;
    if (high_feat >= 1)        {
        __cpuid(buff, 1);
        if (buff[3] & 1<<23)    {
            strcat(info->cpu_feat, "MMX ");
        }
        if (buff[3] & 1<<25)        {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE);
            strcat(info->cpu_feat, "SSE ");
        }
        if (buff[3] & 1<<26)        {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE2);
            strcat(info->cpu_feat, "SSE2 ");
        }
        if (buff[2] & 0x1)        {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE3);
            strcat(info->cpu_feat, "SSE3 ");
        }
        if (buff[2] & 1<<19)        {
            BIT_ADD(info->cpu_caps, HWINFO_CPUEXT_SSE4);
            strcat(info->cpu_feat, "SSE4 ");
        }
    }

    /* cache size */
    if (high_featex >= 0x80000006)        {
        __cpuid(buff, 0x80000006);
        info->cpu_cachesize = (buff[2]>>16)&0xFFFF;
        info->cpu_cacheline = buff[2]&0xFF;
    }

    info->cpu_clock = query_clockspeed(0);
}

void query_osinfo(struct hwinfo* info)
{
    OSVERSIONINFOEX os_info;
    SYSTEM_INFO sys_info;

    memset(&sys_info, 0x00, sizeof(SYSTEM_INFO));
    memset(&os_info, 0x00, sizeof(OSVERSIONINFOEX));

    typedef void (WINAPI *pfn_GetNativeSystemInfo)(LPSYSTEM_INFO);
    pfn_GetNativeSystemInfo GetNativeSystemInfo = (pfn_GetNativeSystemInfo)GetProcAddress(
        GetModuleHandle("kernel32.dll"), "GetNativeSystemInfo");
    if (GetNativeSystemInfo != NULL)
        GetNativeSystemInfo(&sys_info);
    else
        GetSystemInfo(&sys_info);

    os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!GetVersionEx((OSVERSIONINFO*)&os_info))        {
        memset(&os_info, 0x00, sizeof(OSVERSIONINFOEX));
        os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx((OSVERSIONINFO*)&os_info);
    }

    /* win9x */
    if (os_info.dwPlatformId == 1)        {
        if ((os_info.dwMajorVersion == 4) && (os_info.dwMinorVersion == 0)) {
            strcat(info->os_name, "Windows95 ");
        }
        else if ((os_info.dwMajorVersion == 4) && (os_info.dwMinorVersion == 10))   {
            strcat(info->os_name, "Windows98 ");
        }
        else if ((os_info.dwMajorVersion == 4) && (os_info.dwMinorVersion == 90))   {
            strcat(info->os_name, "WindowsME ");
        }
        else    {
            strcat(info->os_name, "Windows9x ");
        }
        info->os_type = HWINFO_OS_WIN9X;
    }

    /* WinNT */
    if (os_info.dwPlatformId == 2)        {
        if ((os_info.dwMajorVersion == 4) && (os_info.dwMinorVersion == 0))            {
            strcat(info->os_name, "WindowsNT 4.0 ");
            info->os_type = HWINFO_OS_WINNT;
        }    else if ((os_info.dwMajorVersion == 5) && (os_info.dwMinorVersion == 0))        {
            strcat(info->os_name, "Windows2K ");
            info->os_type = HWINFO_OS_WIN2K;
        }    else if ((os_info.dwMajorVersion == 5) && (os_info.dwMinorVersion == 1))        {
            strcat(info->os_name, "WindowsXP ");
            info->os_type = HWINFO_OS_WINXP;
        }    else if ((os_info.dwMajorVersion == 6) && (os_info.dwMinorVersion == 0))        {
            strcat(info->os_name, "WindowsVista ");
            info->os_type = HWINFO_OS_WINVISTA;
        }    else if ((os_info.dwMajorVersion == 6) && (os_info.dwMinorVersion == 1))        {
            strcat(info->os_name, "Windows7 ");
            info->os_type = HWINFO_OS_WIN7;
        }   else if ((os_info.dwMajorVersion == 6) && (os_info.dwMinorVersion == 2))    {
            strcat(info->os_name, "Windows8 ");
            info->os_type = HWINFO_OS_WIN8;
        }
    }    else        {
        info->os_type = HWINFO_OS_UNKNOWN;
    }

    strcat(info->os_name, os_info.szCSDVersion);
    if (os_info.dwMajorVersion >= 6)    {
        if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            strcat(info->os_name, " - 64bit");
        else if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
            strcat(info->os_name, " - 32bit");
    }
}

#endif /* _WIN_ */

