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


#ifndef __MEMMGR_H__
#define __MEMMGR_H__

#include "types.h"
#include "core-api.h"
#include "allocator.h"

/**
 * @defgroup mem Memory
 */

/**
 * memory statistics structure
 * @see mem_getstats
 * @ingroup mem
 */
struct mem_stats
{
    uint alloc_cnt;           /**< allocation count */
    size_t alloc_bytes;         /**< total allocated bytes */
    size_t limit_bytes;         /**< maximum allowed heap allocation size, =0 if it's not limited */
    size_t tracer_alloc_bytes;  /**< total allocated bytes by memory tracer */
};

/**
 * Intialize memory system
 * @param trace_mem Enable tracing memory calls, like leak detection
 * @ingroup mem
 */
CORE_API result_t mem_init(bool_t trace_mem);

/**
 * Release memory system
 * @ingroup mem
 */
CORE_API void mem_release();

/**
 * Checks is memory system is initialized
 * @ingroup mem
 */
CORE_API bool_t mem_isinit();

/**
 * Get memory statistics
 * @ingroup mem
 */
CORE_API void mem_getstats(struct mem_stats* stats);

/**
 * Print memory leaks to the logger
 * @ingroup mem
 */
CORE_API void mem_reportleaks();

/**
 * Allocate memory of requested size from the heap
 * @param size memory size (bytes)
 * @param source source file of memory allocation call
 * @param line line of memory allocation call
 * @param id custom memory id of allocated memory
 * @return allocated memory block
 * @ingroup mem
 */
CORE_API void* mem_alloc(size_t size, const char* source, uint line, uint id);
/**
 * free alloacted memory from heap
 * @ingroup mem
 */
CORE_API void mem_free(void* ptr);
/**
 * aligned heap allocation
 * @see mem_alloc   @ingroup mem
 */
CORE_API void* mem_alignedalloc(size_t size, uint8 alignment,
                                const char* source, uint line, uint id);
/**
 * Aligned heap free
 * @see mem_free
 * @ingroup mem
 */
CORE_API void mem_alignedfree(void* ptr);

/**
 * Set the maximum limit of heap memory allocation - overflow memory allocation calls will return NULL
 * @ingroup mem
 */
CORE_API void mem_setmaxlimit(size_t size);

/**
 * Checks if memory limit is passed
 * @see mem_setmaxlimit     @ingroup mem
 */
CORE_API bool_t mem_isoverrun();

/**
 * Gets allocation size of certain memory Id
 * @return allocated memory id size (bytes)
 * @ingroup mem
 */
CORE_API size_t mem_sizebyid(uint id);

/**
 * Gets allocated size of a memory block
 * @ingroup mem
 */
CORE_API size_t mem_size(void* ptr);
/**
 * Gets aligned allocated memory block size
 * @ingroup mem
 */
CORE_API size_t mem_alignedsize(void* ptr);

/**
 * Gets default global heap allocator
 * @ingroup alloc
 */
CORE_API struct allocator* mem_heap();

/**
 * Binds default heap allocator to specified allocator object
 * @ingroup alloc
 */
CORE_API void mem_heap_bindalloc(struct allocator* alloc);

/**
 * Heap allocate macro
 * @param size requested memory size in bytes
 * @param id ID of the memory block
 * @ingroup mem
 */
#define ALLOC(size, id) mem_alloc((size), __FILE__, __LINE__, (id))

/**
 * Aligned Heap allocate macro (16-byte)
 * @see ALLOC
 * @ingroup mem
 */
#define ALIGNED_ALLOC(size, id)	mem_alignedalloc((size), 16, __FILE__, __LINE__, (id))

/**
 * Free heap memory
 * @param ptr Pointer to allocated memory
 * @see ALLOC
 * @ingroup mem
 */
#define FREE(ptr)   mem_free((ptr))

/**
 * Free aligned memory from heap
 * @param size Requested memory size in bytes
 * @param id ID of the memory block
 * @see ALIGNED_ALLOC
 * @ingroup mem
 */
#define ALIGNED_FREE(ptr)   mem_alignedfree((ptr))

#if defined(_GNUC_)
#define ALIGN16        __attribute__((aligned(16)))
#elif defined(_MSVC_)
#define ALIGN16        __declspec(align(16))
#endif

#endif /* __MEMMGR_H__ */
