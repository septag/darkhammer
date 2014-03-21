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
 * Note: we have to skip the whole engine memory management for python extension
 * because allocations/free can occur after core and engine is released
 */

#include <malloc.h>
#include "dhcore/types.h"

#define ALIGNMENT 16

void* py_alignedalloc(size_t s)
{
    size_t ns = s + ALIGNMENT;
    uptr_t raw_addr = (uptr_t)malloc(ns);
    if (raw_addr == 0)     return NULL;

    uptr_t misalign = raw_addr & (ALIGNMENT - 1);
    uint8 adjust = ALIGNMENT - (uint8)misalign;
    uptr_t aligned_addr = raw_addr + adjust;
    uint8* a = (uint8*)(aligned_addr - sizeof(uint8));
    *a = adjust;
    return (void*)aligned_addr;
}

void py_alignedfree(void* ptr)
{
    uptr_t aligned_addr = (uptr_t)ptr;
    uint8 adjust = *((uint8*)(aligned_addr - sizeof(uint8)));
    uptr_t raw_addr = aligned_addr - adjust;
    free((void*)raw_addr);
}
