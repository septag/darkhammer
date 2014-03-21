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
#include "mem-mgr.h"
#include "stack-alloc.h"
#include "err.h"
#include "log.h"

/*************************************************************************************************/
/* functions for binding allocators to stack-alloc */
void* s_alloc(size_t size, const char* source, uint line, uint mem_id, void* param)
{
    return mem_stack_alloc((struct stack_alloc*)param, size, mem_id);
}

void s_free(void* p, void* param)
{
    mem_stack_free((struct stack_alloc*)param, p);
}

void* s_alignedalloc(size_t size, uint8 alignment, const char* source,
                     uint line, uint mem_id, void* param)
{
    return mem_stack_alignedalloc((struct stack_alloc*)param, size, alignment, mem_id);
}

void s_alignedfree(void* p, void* param)
{
    mem_stack_alignedfree((struct stack_alloc*)param, p);
}

void s_save(void* param)
{
    mem_stack_save((struct stack_alloc*)param);
}

void s_load(void* param)
{
    mem_stack_load((struct stack_alloc*)param);
}

/* */
result_t mem_stack_create(struct allocator* alloc, struct stack_alloc* stack,
                          size_t size, uint mem_id)
{
    memset(stack, 0x00, sizeof(struct stack_alloc));
    stack->buffer = (uint8*)A_ALIGNED_ALLOC(alloc, size, mem_id);
    if (stack->buffer == NULL)
        return RET_OUTOFMEMORY;

    stack->size = size;
    stack->alloc = alloc;

    for (uint i = 0; i < STACKALLOC_SAVES_MAX; i++)
        stack->save_ptrs[STACKALLOC_SAVES_MAX-i-1] = &stack->save_nodes[i];
    stack->save_iter = STACKALLOC_SAVES_MAX;

    return RET_OK;
}

void mem_stack_destroy(struct stack_alloc* stack)
{
    ASSERT(stack != NULL);

    if (stack->buffer != NULL)  {
        A_ALIGNED_FREE(stack->alloc, stack->buffer);
    }

    memset(stack, 0x00, sizeof(struct stack_alloc));
}

void* mem_stack_alignedalloc(struct stack_alloc* stack, size_t size,
                             uint8 alignment, uint mem_id)
{
    size_t ns = size + alignment;
    uptr_t raw_addr = (uptr_t)mem_stack_alloc(stack, ns, mem_id);
    if (raw_addr == 0)
        return NULL;

    uptr_t misalign = raw_addr & (alignment - 1);
    uint8 adjust = alignment - (uint8)misalign;
    uptr_t aligned_addr = raw_addr + adjust;
    uint8* a = (uint8*)(aligned_addr - sizeof(uint8));
    *a = adjust;
    return (void*)aligned_addr;
}

void* mem_stack_alloc(struct stack_alloc* stack, size_t size, uint mem_id)
{
    ASSERT(stack->buffer != NULL);

    if ((stack->offset + size) > stack->size)   {
#if defined(_DEBUG_)
        printf("Warning: (Performance) stack allocator '%p' (req-size: %d, id: %d) is overloaded."
            "Allocating from heap.\n", stack, (uint)size, mem_id);
#endif
        return ALLOC(size, mem_id);
    }

    void* ptr = stack->buffer + stack->offset;
    stack->offset += size;

    /* save maximum allocated size */
    if (stack->offset > stack->alloc_max)
        stack->alloc_max = stack->offset;

    return ptr;
}

void mem_stack_alignedfree(struct stack_alloc* stack, void* ptr)
{
    uptr_t aligned_addr = (uptr_t)ptr;
    uint8 adjust = *((uint8*)(aligned_addr - sizeof(uint8)));
    uptr_t raw_addr = aligned_addr - adjust;
    mem_stack_free(stack, (void*)raw_addr);
}

void mem_stack_free(struct stack_alloc* stack, void* ptr)
{
    uptr_t nptr = (uptr_t)ptr;
    uptr_t nbuff = (uptr_t)stack->buffer;
    if (nptr < nbuff || nptr >= (nbuff + stack->size))
        FREE(ptr);
}

void mem_stack_bindalloc(struct stack_alloc* stack, struct allocator* alloc)
{
    alloc->param = stack;
    alloc->alloc_fn = s_alloc;
    alloc->alignedalloc_fn = s_alignedalloc;
    alloc->free_fn = s_free;
    alloc->alignedfree_fn = s_alignedfree;
    alloc->save_fn = s_save;
    alloc->load_fn = s_load;
}

void mem_stack_save(struct stack_alloc* stack)
{
    if (stack->save_iter == 0)  {
        ASSERT(0);  /* Too much saves without load,
                     * increase STACKALLOC_SAVES_MAX
                     * or check your code for A_SAVE without proceeding A_LOAD */
        return;
    }

    struct stack* snode = stack->save_ptrs[--stack->save_iter];
    stack_push(&stack->save_stack, snode, (void*)stack->offset);
}

void mem_stack_load(struct stack_alloc* stack)
{
    struct stack* snode = stack_pop(&stack->save_stack);
    size_t save_offset = (size_t)snode->data;
    ASSERT(save_offset <= stack->offset);

    if (save_offset != stack->offset)
        memset(stack->buffer + save_offset, 0x00, stack->offset - save_offset);

    stack->offset = save_offset;
    stack->save_ptrs[stack->save_iter++] = snode;
    ASSERT(stack->save_iter <= STACKALLOC_SAVES_MAX);   /* In case of error,
                                                         * It's likely that you didn't call each
                                                         * A_LOAD with proceeding A_SAVE */
}

void mem_stack_reset(struct stack_alloc* stack)
{
    for (uint i = 0; i < STACKALLOC_SAVES_MAX; i++)
        stack->save_ptrs[STACKALLOC_SAVES_MAX-i-1] = &stack->save_nodes[i];
    stack->save_iter = STACKALLOC_SAVES_MAX;
    stack->offset = 0;
    stack->save_stack = NULL;
    memset(stack->buffer, 0x00, stack->size);
}

/*************************************************************************************************
 * stack allocator: thread-safe
 */
void* s_alloc_ts(size_t size, const char* source, uint line, uint mem_id, void* param)
{
    return mem_stack_alloc_ts((struct stack_alloc_ts*)param, size, mem_id);
}

void s_free_ts(void* p, void* param)
{
    mem_stack_free_ts((struct stack_alloc_ts*)param, p);
}

void* s_alignedalloc_ts(size_t size, uint8 alignment, const char* source,
                     uint line, uint mem_id, void* param)
{
    return mem_stack_alignedalloc_ts((struct stack_alloc_ts*)param, size, alignment, mem_id);
}

void s_alignedfree_ts(void* p, void* param)
{
    mem_stack_alignedfree_ts((struct stack_alloc_ts*)param, p);
}

void s_save_ts(void* param)
{
    mem_stack_save_ts((struct stack_alloc_ts*)param);
}

void s_load_ts(void* param)
{
    mem_stack_load_ts((struct stack_alloc_ts*)param);
}

/* */
result_t mem_stack_create_ts(struct allocator* alloc, struct stack_alloc_ts* stack,
                             size_t size, uint mem_id)
{
    memset(stack, 0x00, sizeof(struct stack_alloc_ts));
    stack->buffer = (uint8*)A_ALIGNED_ALLOC(alloc, size, mem_id);
    if (stack->buffer == NULL)
        return RET_OUTOFMEMORY;

    stack->size = size;
    stack->alloc = alloc;
    stack->alloc_max = 0;

    return RET_OK;
}

void mem_stack_destroy_ts(struct stack_alloc_ts* stack)
{
    ASSERT(stack != NULL);

    if (stack->buffer != NULL)  {
        A_ALIGNED_FREE(stack->alloc, stack->buffer);
    }

    memset(stack, 0x00, sizeof(struct stack_alloc));
}

void* mem_stack_alignedalloc_ts(struct stack_alloc_ts* stack, size_t size,
                                uint8 alignment, uint mem_id)
{
    size_t ns = size + alignment;
    uptr_t raw_addr = (uptr_t)mem_stack_alloc_ts(stack, ns, mem_id);
    if (raw_addr == 0)
        return NULL;

    uptr_t misalign = raw_addr & (alignment - 1);
    uint8 adjust = alignment - (uint8)misalign;
    uptr_t aligned_addr = raw_addr + adjust;
    uint8* a = (uint8*)(aligned_addr - sizeof(uint8));
    *a = adjust;
    return (void*)aligned_addr;
}

void* mem_stack_alloc_ts(struct stack_alloc_ts* stack, size_t size, uint mem_id)
{
    ASSERT(stack->buffer != NULL);

    void* ptr;
    while (TRUE)    {
        size_t cur_offset = stack->offset;
        if ((cur_offset + size) > stack->size)  {
            log_printf(LOG_WARNING, "stack allocator '%p' (req-size: %d, id: %d) is overloaded\n",
                stack, size, mem_id);
            return ALLOC(size, mem_id);
        }

        ptr = stack->buffer + cur_offset;
        size_t new_offset = cur_offset + size;

        /* set maximum bytes fetched from stack alloc, not really important in multi-threading */
#if defined(_X86_) || defined(_ARM_)
        if (new_offset > stack->alloc_max)
            MT_ATOMIC_SET(stack->alloc_max, new_offset);
#elif defined(_X64_)
        if (new_offset > stack->alloc_max)
            MT_ATOMIC_SET64(stack->alloc_max, new_offset);
#endif

        /* commit changes */
#if defined(_X86_)
        if (MT_ATOMIC_CAS(stack->offset, cur_offset, new_offset) == cur_offset)
            return ptr;
#elif defined(_X64_)
        if (MT_ATOMIC_CAS64(stack->offset, cur_offset, new_offset) == cur_offset)
            return ptr;
#endif
    }

    return ptr;
}

void mem_stack_alignedfree_ts(struct stack_alloc_ts* stack, void* ptr)
{
    uptr_t aligned_addr = (uptr_t)ptr;
    uint8 adjust = *((uint8*)(aligned_addr - sizeof(uint8)));
    uptr_t raw_addr = aligned_addr - adjust;
    mem_stack_free_ts(stack, (void*)raw_addr);
}

void mem_stack_free_ts(struct stack_alloc_ts* stack, void* ptr)
{
    uptr_t nptr = (uptr_t)ptr;
    uptr_t nbuff = (uptr_t)stack->buffer;
    if (nptr < nbuff || nptr >= (nbuff + stack->size))
        FREE(ptr);
}

void mem_stack_bindalloc_ts(struct stack_alloc_ts* stack, struct allocator* alloc)
{
    alloc->param = stack;
    alloc->alloc_fn = s_alloc_ts;
    alloc->alignedalloc_fn = s_alignedalloc_ts;
    alloc->free_fn = s_free_ts;
    alloc->alignedfree_fn = s_alignedfree_ts;
    alloc->save_fn = s_save_ts;
    alloc->load_fn = s_load_ts;
}

void mem_stack_save_ts(struct stack_alloc_ts* stack)
{
    stack->save_offset = stack->offset;

#if defined(_X86_)
    MT_ATOMIC_SET(stack->save_offset, stack->offset);
#elif defined(_X64_)
    MT_ATOMIC_SET64(stack->save_offset, stack->offset);
#endif
}

void mem_stack_load_ts(struct stack_alloc_ts* stack)
{
    ASSERT(stack->save_offset <= stack->offset);

#if defined(_X86_)
    MT_ATOMIC_SET(stack->offset, stack->save_offset);
#elif defined(_X64_)
    MT_ATOMIC_SET64(stack->offset, stack->save_offset);
#endif
}

void mem_stack_reset_ts(struct stack_alloc_ts* stack)
{
    MT_ATOMIC_SET(stack->offset, 0);
    MT_ATOMIC_SET(stack->save_offset, 0);
}
