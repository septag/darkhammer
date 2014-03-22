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

#ifndef __STACKALLOC_H__
#define __STACKALLOC_H__

#include "types.h"
#include "allocator.h"
#include "core-api.h"
#include "mt.h"
#include "stack.h"

#define STACKALLOC_SAVES_MAX    8

/**
 * Stack allocator: variable-size sequential stack allocator, total size is fixed\n
 * It is the fastest allocator, but it's sequential and does not support dynamic free\n
 * Normal stack allocator, can save and load it's memory offset up to STACKALLOC_SAVES_MAX times,
 * using A_LOAD and A_SAVE macros\n
 * Stack size is fixed, so when user request larger memory than the stack contains, it will throw
 * a warning and allocate the block from heap instead
 * @ingroup alloc
 */
struct stack_alloc
{
    uint8* buffer;
    size_t offset;     /* in bytes */
    size_t size;       /* in bytes */
    size_t alloc_max;
    struct allocator* alloc;
    struct stack* save_stack;   /* save stack, data: (size_t) offset to last save */
    uint save_iter;
    struct stack save_nodes[STACKALLOC_SAVES_MAX];
    struct stack* save_ptrs[STACKALLOC_SAVES_MAX];
};

/**
 * Create stack allocator
 * @param alloc allocator for internal stack allocator buffer
 * @param size size of stack allocator buffer (bytes)
 * @ingroup alloc
 */
CORE_API result_t mem_stack_create(struct allocator* alloc,
                                   struct stack_alloc* stack, size_t size, uint mem_id);

/**
 * Destroy stack allocator
 * @ingroup alloc
 */
CORE_API void mem_stack_destroy(struct stack_alloc* stack);

/**
 * Allocate memory from stack allocator
 * @see mem_stack_bindalloc @ingroup alloc
 */
CORE_API void* mem_stack_alloc(struct stack_alloc* stack, size_t size, uint mem_id);

/**
 * Allocate aligned memory from stack allocator
 * @see mem_stack_bindalloc
 * @ingroup alloc
 */
CORE_API void* mem_stack_alignedalloc(struct stack_alloc* stack, size_t size,
                                      uint8 alignment, uint mem_id);

/**
 * save stack allocator state in order to load it later
 * @see mem_stack_load
 * @ingroup alloc
 */
CORE_API void mem_stack_save(struct stack_alloc* stack);

/**
 * Load previously saved stack allocator state.\n
 * memory after saved state is discarded after 'load'
 * @see mem_stack_save
 * @ingroup alloc
 */
CORE_API void mem_stack_load(struct stack_alloc* stack);

/**
 * Reset stack allocator state, discarding any memory that is allocated
 * @ingroup alloc
 */
CORE_API void mem_stack_reset(struct stack_alloc* stack);

/**
 * Free memory from stack, this actually frees only out-of-bound memory block that is allocated
 * from heap instead
 * @ingroup alloc
 */
void mem_stack_free(struct stack_alloc* stack, void* ptr);

/**
 * Free aligned memory from stack, this actually frees only out-of-bound memory block that is allocated
 * from heap instead
 * @ingroup alloc
 */
void mem_stack_alignedfree(struct stack_alloc* stack, void* ptr);

/**
 * bind stack-alloc to generic allocator
 * @ingroup alloc
 */
CORE_API void mem_stack_bindalloc(struct stack_alloc* stack, struct allocator* alloc);

/**
 * Stack allocator (thread-safe): variable-size sequential stack allocator, total size is fixed\n
 * it is the fastest allocator, but it's sequential and does not support dynamic free\n
 * This kind of stack allocator is thread-safe\n
 * So It's memory should be allocated and freed from one thread, and it's data can be allocated from
 * different threads\n
 * Stack size is fixed, so when user request larger memory than the stack contains, it will throw
 * a warning and allocate the block from heap instead
 * @ingroup alloc
 */
struct stack_alloc_ts
{
    uint8* buffer;
    atom_t offset;     /* in bytes */
    atom_t alloc_max;
    atom_t save_offset;
    size_t size;       /* in bytes */
    struct allocator* alloc;
};

/**
 * create stack allocator (thread-safe)
 * @param alloc allocator for internal stack allocator buffer
 * @param size size of stack allocator buffer (bytes)
 * @ingroup alloc
 */
CORE_API result_t mem_stack_create_ts(struct allocator* alloc,
                                      struct stack_alloc_ts* stack, size_t size, uint mem_id);

/**
 * destroy stack allocator (thread-safe)
 * @ingroup alloc
 */
CORE_API void mem_stack_destroy_ts(struct stack_alloc_ts* stack);

/**
 * stack alloc (thread-safe)
 * @see mem_stack_bindalloc @ingroup alloc
 */
CORE_API void* mem_stack_alloc_ts(struct stack_alloc_ts* stack, size_t size, uint mem_id);

/**
 * stack aligned alloc (thread-safe)
 * @see mem_stack_bindalloc @ingroup alloc
 */
CORE_API void* mem_stack_alignedalloc_ts(struct stack_alloc_ts* stack, size_t size,
                                         uint8 alignment, uint mem_id);

/**
 * save stack allocator state in order to load it later (thread-safe)
 * @see mem_stack_load
 * @ingroup alloc
 */
CORE_API void mem_stack_save_ts(struct stack_alloc_ts* stack);

/**
 * load previously saved stack allocator state. (thread-safe)\n
 * memory after saved state is discarded after 'load'
 * @see mem_stack_save
 * @ingroup alloc
 */
CORE_API void mem_stack_load_ts(struct stack_alloc_ts* stack);

/**
 * reset stack allocator state, discarding any memory that is allocated (thread-safe)
 * @ingroup alloc
 */
CORE_API void mem_stack_reset_ts(struct stack_alloc_ts* stack);

/**
 * Free memory from stack, this actually frees only out-of-bound memory block that is allocated
 * from heap instead of stack (thread-safe)
 * @ingroup alloc
 */
void mem_stack_alignedfree_ts(struct stack_alloc_ts* stack, void* ptr);

/**
 * Free aligned memory from stack, this actually frees only out-of-bound memory block that is allocated
 * from heap instead of stack (thread-safe)
 * @ingroup alloc
 */
void mem_stack_free_ts(struct stack_alloc_ts* stack, void* ptr);

/**
 * bind stack-alloc to generic allocator (thread-safe)
 * @ingroup alloc
 */
CORE_API void mem_stack_bindalloc_ts(struct stack_alloc_ts* stack, struct allocator* alloc);


#endif /*__STACKALLOC_H__*/
