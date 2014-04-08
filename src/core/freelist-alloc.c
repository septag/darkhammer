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
#include "freelist-alloc.h"
#include "err.h"
#include "log.h"

/* this threshold value is for custom allocators
 * more than this amount of memory request is allocated from heap instead
 */
#define HEAP_ALLOC_THRESHOLD    8192

/*************************************************************************************************
 * types
 */
enum chunk_state
{
    CHUNK_NULL = 0,
    CHUNK_FREE,
    CHUNK_ALLOC,
};

struct freelist_chunk
{
    enum chunk_state        state;
    size_t                  size;
    uint                  mem_id;
    struct freelist_chunk*  prev_chunk;
    struct linked_list      node;
};

/*************************************************************************************************
 * inlines
 */
INLINE struct freelist_chunk* freelist_getnext(struct freelist_chunk* ch)
{
    return (struct freelist_chunk*)((uint8*)ch + sizeof(struct freelist_chunk) + ch->size);
}

INLINE struct freelist_chunk* freelist_getchunk(void* ptr)
{
    return (struct freelist_chunk*)((uint8*)ptr - sizeof(struct freelist_chunk));
}

INLINE void* freelist_getptr(struct freelist_chunk* ch)
{
    return (uint8*)ch + sizeof(struct freelist_chunk);
}

/*************************************************************************************************
 * fwd
 */
struct freelist_chunk* freelist_createchunk(struct freelist_alloc* freelist, void* buff,
    size_t size, uint mem_id, void** pmem);
struct freelist_chunk* freelist_divide(struct freelist_alloc* freelist, struct freelist_chunk* ch,
    size_t divide_offset, uint mem_id);
void freelist_chunkalloc(struct freelist_alloc* freelist, struct freelist_chunk* ch);
void freelist_chunkfree(struct freelist_alloc* freelist, struct freelist_chunk* ch);

/*************************************************************************************************/
/* bindings for generic allocator */
void* fl_alloc(size_t size, const char* source, uint line, uint mem_id, void* param)
{
    return mem_freelist_alloc((struct freelist_alloc*)param, size, mem_id);
}

void fl_free(void* p, void* param)
{
    mem_freelist_free((struct freelist_alloc*)param, p);
}

void* fl_alignedalloc(size_t size, uint8 alignment, const char* source,
                      uint line, uint mem_id, void* param)
{
    return mem_freelist_alignedalloc((struct freelist_alloc*)param, size, alignment, mem_id);
}

void fl_alignedfree(void* p, void* param)
{
    mem_freelist_alignedfree((struct freelist_alloc*)param, p);
}

/*************************************************************************************************/
/* default state of the chunk is FREE */
result_t mem_freelist_create(struct allocator* alloc,
                             struct freelist_alloc* freelist,
                             size_t size, uint mem_id)
{
    memset(freelist, 0x00, sizeof(struct freelist_alloc));

    freelist->buffer = (uint8*)A_ALIGNED_ALLOC(alloc, size, mem_id);
    if (freelist->buffer == NULL)
        return RET_OUTOFMEMORY;
    memset(freelist->buffer, 0x00, size);

    freelist->size = size;
    freelist->alloc = alloc;

    /* at the beginning, we have a very big chunk in the freelist */
    /* keep space for another dummy chunk */
    struct freelist_chunk* ch = freelist_createchunk(freelist, freelist->buffer,
        size - 2*sizeof(struct freelist_chunk), mem_id, NULL);

    /* last chunk in the buffer is dummy chunk */
    struct freelist_chunk* dummy = freelist_createchunk(freelist,
        freelist->buffer + size - sizeof(struct freelist_chunk), 0, mem_id, NULL);
    dummy->state = CHUNK_NULL;
    dummy->prev_chunk = ch;
    list_remove(&freelist->free_chunks, &dummy->node);

    return RET_OK;
}

void mem_freelist_destroy(struct freelist_alloc* freelist)
{
    if (freelist->buffer != NULL)   {
        ASSERT(freelist->alloc != NULL);
        A_ALIGNED_FREE(freelist->alloc, freelist->buffer);
    }

    memset(freelist, 0x00, sizeof(struct freelist_alloc));
}

void* mem_freelist_alignedalloc(struct freelist_alloc* freelist, size_t size,
                                uint8 alignment, uint mem_id)
{
    size_t ns = size + alignment;
    uptr_t raw_addr = (uptr_t)mem_freelist_alloc(freelist, ns, mem_id);
    if (raw_addr == 0)     return NULL;

    uptr_t misalign = raw_addr & (alignment - 1);
    uint8 adjust = alignment - (uint8)misalign;
    uptr_t aligned_addr = raw_addr + adjust;
    uint8* a = (uint8*)(aligned_addr - sizeof(uint8));
    *a = adjust;
    return (void*)aligned_addr;
}

void* mem_freelist_alloc(struct freelist_alloc* freelist, size_t size, uint mem_id)
{
    if (size >= HEAP_ALLOC_THRESHOLD)
        return ALLOC(size, mem_id);

    struct linked_list* node = freelist->free_chunks;
    while (node != NULL)    {
        struct freelist_chunk* ch = (struct freelist_chunk*)node->data;
        if (ch->size >= size)   {
            /* check if we can divide the current chunk */
            if ((ch->size - size) > sizeof(struct freelist_chunk))
                freelist_divide(freelist, ch, size, mem_id);

            /* it's gonna be allocated, remove from free-list and add it to alloc-list */
            freelist_chunkalloc(freelist, ch);
            freelist->alloc_size += size;
            return freelist_getptr(ch);
        }
        node = node->next;
    }

    /* no valid chunk found: throw a warning in debug mode and allocate from heap */
#if defined(_DEBUG_)
    printf("Warning: (Performance) freelist allocator '%p' (req-size: %d, id: %d) is overloaded."
        "Allocating from heap\n", freelist, (uint)size, mem_id);
#endif
    return ALLOC(size, mem_id);
}

void mem_freelist_alignedfree(struct freelist_alloc* freelist, void* ptr)
{
    uptr_t aligned_addr = (uptr_t)ptr;
    uint8 adjust = *((uint8*)(aligned_addr - sizeof(uint8)));
    uptr_t raw_addr = aligned_addr - adjust;
    mem_freelist_free(freelist, (void*)raw_addr);
}

void mem_freelist_free(struct freelist_alloc* freelist, void* ptr)
{
    ASSERT(ptr != NULL);

    /* check if pointer belongs to freelist buffer
     * if it doesn't belong to freelist buffer (maybe allocated from heap), then use heap */
    uptr_t pn = (uptr_t)ptr;
    uptr_t bufn = (uptr_t)freelist->buffer;
    if (pn < bufn || pn >= (bufn + freelist->size))   {
        FREE(ptr);
        return;
    }

    struct freelist_chunk* ch = freelist_getchunk(ptr);
    ASSERT(ch->state == CHUNK_ALLOC);
    freelist->alloc_size -= ch->size;

    /* check the next memory chunk. if it's FREE, then unite it with current and remove it */
    struct freelist_chunk* nextch = freelist_getnext(ch);
    if (nextch->state == CHUNK_FREE)    {
        struct freelist_chunk* nnch = freelist_getnext(nextch);
        nnch->prev_chunk = ch;

        ch->size += (nextch->size + sizeof(struct freelist_chunk));

        list_remove(&freelist->free_chunks, &nextch->node);
        memset(nextch, 0x00, sizeof(struct freelist_chunk));
    }

    /* check the previous memory chunk. if it's FREE, then unite it with current and remove cur */
    struct freelist_chunk* prevch = ch->prev_chunk;
    if (prevch != NULL && prevch->state == CHUNK_FREE)  {
        struct freelist_chunk* nextch = freelist_getnext(ch);
        nextch->prev_chunk = prevch;

        prevch->size += (ch->size + sizeof(struct freelist_chunk));

        /* current node must be deleted, so it resides in allocated list, remove it */
        list_remove(&freelist->alloc_chunks, &ch->node);
        memset(ch, 0x00, sizeof(struct freelist_chunk));

        return;
    }

    /* remove current chunk from alloc-list and add it to free-list */
    freelist_chunkfree(freelist, ch);
}

struct freelist_chunk* freelist_divide(struct freelist_alloc* freelist, struct freelist_chunk* ch,
    size_t divide_offset, uint mem_id)
{
    struct freelist_chunk* nch = freelist_createchunk(freelist,
        (uint8*)ch + sizeof(struct freelist_chunk) + divide_offset,
        ch->size - divide_offset - sizeof(struct freelist_chunk), mem_id, NULL);
    nch->prev_chunk = ch;
    nch->mem_id = ch->mem_id;

    struct freelist_chunk* nextch = freelist_getnext(nch);
    nextch->prev_chunk = nch;

    /* shrink the size of current chunk */
    ch->size = divide_offset;
    ch->mem_id = mem_id;

    return nch;
}

struct freelist_chunk* freelist_createchunk(struct freelist_alloc* freelist, void* buff,
    size_t size, uint mem_id, void** pmem)
{
    struct freelist_chunk* ch = (struct freelist_chunk*)buff;
    memset(ch, 0x00, sizeof(struct freelist_chunk));
    ch->state = CHUNK_FREE;
    ch->size = size;
    ch->mem_id = mem_id;

    if (pmem != NULL)
        *pmem = (uint8*)buff + sizeof(struct freelist_chunk);

    /* it is a free chunk, so add it to free-list */
    list_add(&freelist->free_chunks, &ch->node, ch);
    return (struct freelist_chunk*)buff;
}

void freelist_chunkalloc(struct freelist_alloc* freelist, struct freelist_chunk* ch)
{
    ch->state = CHUNK_ALLOC;
    list_remove(&freelist->free_chunks, &ch->node);
    list_add(&freelist->alloc_chunks, &ch->node, ch);
}

void freelist_chunkfree(struct freelist_alloc* freelist, struct freelist_chunk* ch)
{
    ch->state = CHUNK_FREE;
    list_remove(&freelist->alloc_chunks, &ch->node);
    list_add(&freelist->free_chunks, &ch->node, ch);
}

size_t mem_freelist_getsize(struct freelist_alloc* freelist, void* ptr)
{
    /* check if pointer belongs to freelist buffer
     * if it doesn't belong to freelist buffer (maybe allocated from heap), then use heap */
    uptr_t pn = (uptr_t)ptr;
    uptr_t bufn = (uptr_t)freelist->buffer;
    if (pn < bufn || pn >= (bufn + freelist->size))
        return mem_size(ptr);

    struct freelist_chunk* chunk = freelist_getchunk(ptr);
    ASSERT(chunk->state == CHUNK_ALLOC);
    return chunk->size;
}

void mem_freelist_bindalloc(struct freelist_alloc* freelist, struct allocator* alloc)
{
    alloc->param = freelist;
    alloc->alloc_fn = fl_alloc;
    alloc->alignedalloc_fn = fl_alignedalloc;
    alloc->free_fn = fl_free;
    alloc->alignedfree_fn = fl_alignedfree;
    alloc->save_fn = NULL;
    alloc->load_fn = NULL;
}

uint mem_freelist_getleaks(struct freelist_alloc* freelist, void** pptrs)
{
    uint count = 0;
    struct linked_list* node = freelist->alloc_chunks;
    while (node != NULL)   {
        if (pptrs != NULL)   {
            struct freelist_chunk* chunk = (struct freelist_chunk*)node->data;
            pptrs[count] = (uint8*)chunk + sizeof(struct freelist_chunk);
        }

        count++;
        node = node->next;
    }
    return count;
}

/*************************************************************************************************/
void* fl_alloc_ts(size_t size, const char* source, uint line, uint mem_id, void* param)
{
    return mem_freelist_alloc_ts((struct freelist_alloc_ts*)param, size, mem_id);
}

void fl_free_ts(void* p, void* param)
{
    mem_freelist_free_ts((struct freelist_alloc_ts*)param, p);
}

void* fl_alignedalloc_ts(size_t size, uint8 alignment, const char* source,
                         uint line, uint mem_id, void* param)
{
    return mem_freelist_alignedalloc_ts((struct freelist_alloc_ts*)param, size, alignment, mem_id);
}

void fl_alignedfree_ts(void* p, void* param)
{
    mem_freelist_alignedfree_ts((struct freelist_alloc_ts*)param, p);
}

/* */
result_t mem_freelist_create_ts(struct allocator* alloc, struct freelist_alloc_ts* freelist,
                                size_t size, uint mem_id)
{
    memset(freelist, 0x00, sizeof(struct freelist_alloc_ts));
    mt_mutex_init(&freelist->lock);
    return mem_freelist_create(alloc, &freelist->fl, size, mem_id);
}

void mem_freelist_destroy_ts(struct freelist_alloc_ts* freelist)
{
    mt_mutex_release(&freelist->lock);
    mem_freelist_destroy(&freelist->fl);
}

void* mem_freelist_alloc_ts(struct freelist_alloc_ts* freelist, size_t size, uint mem_id)
{
    mt_mutex_lock(&freelist->lock);
    void* ptr = mem_freelist_alloc(&freelist->fl, size, mem_id);
    mt_mutex_unlock(&freelist->lock);
    return ptr;
}

void* mem_freelist_alignedalloc_ts(struct freelist_alloc_ts* freelist, size_t size,
                                   uint8 alignment, uint mem_id)
{
    size_t ns = size + alignment;
    uptr_t raw_addr = (uptr_t)mem_freelist_alloc_ts(freelist, ns, mem_id);
    if (raw_addr == 0)
        return NULL;

    uptr_t misalign = raw_addr & (alignment - 1);
    uint8 adjust = alignment - (uint8)misalign;
    uptr_t aligned_addr = raw_addr + adjust;
    uint8* a = (uint8*)(aligned_addr - sizeof(uint8));
    *a = adjust;
    return (void*)aligned_addr;
}

void mem_freelist_bindalloc_ts(struct freelist_alloc_ts* freelist, struct allocator* alloc)
{
    alloc->param = freelist;
    alloc->alloc_fn = fl_alloc_ts;
    alloc->alignedalloc_fn = fl_alignedalloc_ts;
    alloc->free_fn = fl_free_ts;
    alloc->alignedfree_fn = fl_alignedfree_ts;
    alloc->save_fn = NULL;
    alloc->load_fn = NULL;
}

void mem_freelist_free_ts(struct freelist_alloc_ts* freelist, void* ptr)
{
    mt_mutex_lock(&freelist->lock);
    mem_freelist_free(&freelist->fl, ptr);
    mt_mutex_unlock(&freelist->lock);
}

void mem_freelist_alignedfree_ts(struct freelist_alloc_ts* freelist, void* ptr)
{
    uptr_t aligned_addr = (uptr_t)ptr;
    uint8 adjust = *((uint8*)(aligned_addr - sizeof(uint8)));
    uptr_t raw_addr = aligned_addr - adjust;

    mem_freelist_free_ts(freelist, (void*)raw_addr);
}

uint mem_freelist_getleaks_ts(struct freelist_alloc_ts* freelist, void** pptrs)
{
    return mem_freelist_getleaks(&freelist->fl, pptrs);
}

size_t mem_freelist_getsize_ts(struct freelist_alloc_ts* freelist, void* ptr)
{
    return mem_freelist_getsize(&freelist->fl, ptr);
}