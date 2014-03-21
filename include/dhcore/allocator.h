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


#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include "types.h"

 /**
 * Callback function for custom allocation
 * @param size size (in bytes) to allocate
 * @param source source file of allocation call
 * @param line line number of the allocation call
 * @param id memory id
 * @param param void pointer to allocator structure (or any other custom parameter)
 * @return returns pointer to allocated block
 * @ingroup mem
 */
typedef void* (*pfn_alloc)(size_t size, const char* source,
                           uint line, uint id, void* param);
/**
 * Callback function for custom free
 * @ingroup mem
 */
typedef void  (*pfn_free)(void* ptr, void* param);
/**
 * callback function for aligned allocation
 * @see pfn_alloc
 * @ingroup mem
 */
typedef void* (*pfn_alignedalloc)(size_t size, uint8 alignment,
                                  const char* source, uint line, uint id,
                                  void* param);
/**
 * Callback function for aligned free
 * @see pfn_free
 * @ingroup mem
 */
typedef void  (*pfn_alignedfree)(void* ptr, void* param);

/**
 * Callback loading and saving allocator state, can easily be implemented in stack allocators
 * @see pfn_free
 * @ingroup mem
 */
typedef void (*pfn_alloc_saveload)(void* param);

/**
 * Allocator structure that holds all callback functions\n
 * each custom allocator, has a function for binding allocator structure to it's routines
 * @see mem_stack_bindalloc
 * @ingroup mem
 */
struct allocator
{
    pfn_alloc alloc_fn;
    pfn_free free_fn;
    pfn_alignedalloc alignedalloc_fn;
    pfn_alignedfree alignedfree_fn;
    pfn_alloc_saveload save_fn;
    pfn_alloc_saveload load_fn;
    void* param;            /* likely will be allocator source object */
};

/**
 * Allocation with custom allocator
 * @param alloc custom allocator pointer
 * @param size size of the requested memory block
 * @param ID of the memory block
 * @ingroup mem
 */
#define A_ALLOC(alloc, size, id)   \
    (alloc)->alloc_fn((size), __FILE__, __LINE__, (id), (alloc)->param)
/**
 * Free memory with custom allocator
 * @param alloc custom allocator pointer
 * @param ptr pointer to the allocated memory @see A_ALLOC
 * @ingroup mem
 */
#define A_FREE(alloc, ptr)  (alloc)->free_fn((ptr), (alloc)->param)
/**
 * Aligned memory allocator (16-byte)
 * @see A_ALLOC
 * @ingroup mem
 */
#define A_ALIGNED_ALLOC(alloc, size, id)    \
    (alloc)->alignedalloc_fn((size), 16, __FILE__, __LINE__, (id), (alloc)->param)
/**
 * Aligned memory free
 * @see A_ALIGNED_ALLOC
 * @ingroup mem
 */
#define A_ALIGNED_FREE(alloc, ptr)  (alloc)->alignedfree_fn((ptr), (alloc)->param)

/**
 * Save allocator state
 * @ingroup mem
 */
#define A_SAVE(alloc)   if ((alloc)->save_fn != NULL)   (alloc)->save_fn((alloc)->param);

/**
 * Load allocator state
 * @ingroup mem
 */
#define A_LOAD(alloc)   if ((alloc)->load_fn != NULL)   (alloc)->load_fn((alloc)->param);


#endif /*__ALLOCATOR_H__*/
