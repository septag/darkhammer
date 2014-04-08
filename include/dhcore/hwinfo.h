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

#ifndef __HWINFO_H__
#define __HWINFO_H__

#include "types.h"
#include "core-api.h"

enum hwinfo_flags
{
    HWINFO_MEMORY = (1<<0),
    HWINFO_CPU = (1<<2),
    HWINFO_OS = (1<<3),
    HWINFO_ALL = 0xFFFFFFFF
};

/**
 * CPU Type
 * @ingroup eng
 */
enum hwinfo_cpu_type
{
    HWINFO_CPU_UNKNOWN,
    HWINFO_CPU_INTEL,
    HWINFO_CPU_AMD
};

/**
 * OS Type
 * @ingroup eng
 */
enum hwinfo_os_type
{
    HWINFO_OS_UNKNOWN,
    HWINFO_OS_WIN9X,
    HWINFO_OS_WINNT,
    HWINFO_OS_LINUX,
    HWINFO_OS_WIN2K,
    HWINFO_OS_WINXP,
    HWINFO_OS_WINVISTA,
    HWINFO_OS_WIN7,
    HWINFO_OS_WIN8,
    HWINFO_OS_OSX
};

/**
 * CPU Caps
 * @ingroup eng
 */
enum hwinfo_cpu_ext
{
    HWINFO_CPUEXT_MMX = (1<<0), /**< MMX Instructions support */
    HWINFO_CPUEXT_SSE = (1<<1), /**< SSE Instructions support */
    HWINFO_CPUEXT_SSE2 = (1<<2),    /**< SSE2 Instructions support */
    HWINFO_CPUEXT_SSE3 = (1<<3),    /**< SSE3 Instructions support */
    HWINFO_CPUEXT_SSE4 = (1<<4) /**< SSE4 Instructions support */
};

/**
 * Hardware information structure
 * @ingroup eng
 */
struct hwinfo
{
    char cpu_name[128]; /**< CPU vendor name */
    char cpu_feat[128]; /**< CPU Features string, each feature is separated with space */
    char os_name[128];  /**< Running OS Name */
    size_t sys_mem;	/**< Available total system memory (in Kb) */
    size_t sys_memfree; /**< Available free system memory (in Kb) */
    uint cpu_clock; /**< CPU clock rate (in MHZ) */
    uint cpu_cachesize; /**< CPU Cache size */
    uint cpu_cacheline; /**< CPU Cache line size */
    uint cpu_core_cnt; /**< Total count of cpu cores (logical) */
    uint cpu_pcore_cnt; /**< Total count of physical cpu cores */
    uint cpu_caps; /**< Combination of known CPU Caps (@see hwinfo_cpu_ext) */
    enum hwinfo_cpu_type cpu_type; /**< CPU Type (@see hwinfo_cpu_type) */
    enum hwinfo_os_type os_type;    /**< OS Type (@see hwinfo_os_type) */
};

CORE_API void hw_getinfo(struct hwinfo* info, uint flags);
CORE_API void hw_printinfo(const struct hwinfo* info, uint flags);

#endif /* __HWINFO_H__ */
