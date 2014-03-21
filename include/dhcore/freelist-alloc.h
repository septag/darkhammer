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

#ifndef __FREELIST_ALLOC_H__
#define __FREELIST_ALLOC_H__

#include "types.h"
#include "allocator.h"
#include "linked-list.h"
#include "core-api.h"
#include "mt.h"

/**
 * freelist allocator: variable-sized small block memory allocator\n
 * more that 8k memory blocks will be allocated from heap
 * @ingroup alloc
 */
struct freelist_alloc
{
    uint8*              buffer;
    size_t              size;
    size_t              alloc_size;
    struct linked_list* free_chunks;
    struct linked_list* alloc_chunks;
    struct allocator*   alloc;
};

/**
 * freelist create/destroy
 * @param alloc allocator for internal freelist memory
 * @param size size (in bytes) for freelist buffer
 * @see mem_freelist_destroy    @ingroup alloc
 */
CORE_API result_t mem_freelist_create(struct allocator* alloc,
                                      struct freelist_alloc* freelist,
                                      size_t size, uint mem_id);
/**
 * destroy freelist
 * @ingroup alloc
 */
CORE_API void mem_freelist_destroy(struct freelist_alloc* freelist);

/**
 * allocate memory from freelist
 * @param size size (in bytes) of requested memory, if requested size is more than 8k -
 * see (freelist-alloc.c), memory will be allocated from heap instead of freelist
 * @return allocated memory block   @ingroup alloc
 */
CORE_API void* mem_freelist_alloc(struct freelist_alloc* freelist, size_t size, uint mem_id);

/**
 * Aligned allocation from freelist
 * @see mem_freelist_alloc
 * @ingroup alloc
 */
CORE_API void* mem_freelist_alignedalloc(struct freelist_alloc* freelist, size_t size,
                                         uint8 alignment, uint mem_id);
/**
 * @ingroup alloc
 */
CORE_API void mem_freelist_free(struct freelist_alloc* freelist, void* ptr);

/**
 * @ingroup alloc
 */
CORE_API void mem_freelist_alignedfree(struct freelist_alloc* freelist, void* ptr);

/**
 * get freelist memory leaks
 * @param pptrs array of pointers to the leaks, if =NULL function only returns number of leaks
 * @return number of leaks
 * @ingroup alloc
 */
CORE_API uint mem_freelist_getleaks(struct freelist_alloc* freelist, void** pptrs);

/**
 * get size of the allocated memory from freelist
 */
CORE_API size_t mem_freelist_getsize(struct freelist_alloc* freelist, void* ptr);

/**
 * bind freelist-alloc to generic allocator
 * @ingroup alloc
 */
CORE_API void mem_freelist_bindalloc(struct freelist_alloc* freelist, struct allocator* alloc);

/*************************************************************************************************/
/**
 * freelist allocator (thread-safe): variable-sized small block memory allocator\n
 * more that 8k memory blocks will be allocated from heap
 * @ingroup alloc
 */
struct freelist_alloc_ts
{
    struct freelist_alloc fl;
    mt_mutex lock;
};

/**
 * Freelist create/destroy (thread-safe)
 * @param alloc allocator for internal freelist memory
 * @param size size (in bytes) for freelist buffer
 * @see mem_freelist_destroy    @ingroup alloc
 */
CORE_API result_t mem_freelist_create_ts(struct allocator* alloc,
                                         struct freelist_alloc_ts* freelist,
                                         size_t size, uint mem_id);
/**
 * Destroy freelist (thread-safe)
 * @ingroup alloc
 */
CORE_API void mem_freelist_destroy_ts(struct freelist_alloc_ts* freelist);

/**
 * Allocate memory from freelist (thread-safe)
 * @param size size (in bytes) of requested memory, if requested size is more than 8k -
 * see (freelist-alloc.c), memory will be allocated from heap instead of freelist
 * @return allocated memory block   @ingroup alloc
 */
CORE_API void* mem_freelist_alloc_ts(struct freelist_alloc_ts* freelist, size_t size, uint mem_id);

/**
 * @ingroup alloc
 */
CORE_API void* mem_freelist_alignedalloc_ts(struct freelist_alloc_ts* freelist, size_t size,
                                            uint8 alignment, uint mem_id);

/**
 * @ingroup alloc
 */
CORE_API void mem_freelist_bindalloc_ts(struct freelist_alloc_ts* freelist, struct allocator* alloc);

/**
 * @ingroup alloc
 */
CORE_API void mem_freelist_free_ts(struct freelist_alloc_ts* freelist, void* ptr);

/**
 * @ingroup alloc
 */
CORE_API void mem_freelist_alignedfree_ts(struct freelist_alloc_ts* freelist, void* ptr);

/**
 * get freelist memory leaks (thread-safe)
 * @param pptrs array of pointers to the leaks, if =NULL function only returns number of leaks
 * @return number of leaks
 * @ingroup alloc
 */
CORE_API uint mem_freelist_getleaks_ts(struct freelist_alloc_ts* freelist, void** pptrs);

/**
 * get size of the allocated memory from freelist (thread-safe)
 */
CORE_API size_t mem_freelist_getsize_ts(struct freelist_alloc_ts* freelist, void* ptr);

#endif
