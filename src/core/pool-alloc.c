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


#include "types.h"
#include "pool-alloc.h"
#include "err.h"
#include "mem-mgr.h"

struct ALIGN16 mem_pool_block
{
    struct linked_list  node;       /* linked-list node */
    uint8*              buffer;     /* memory buffer that holds all objects */
    void**              ptrs;       /* pointer references to the buffer */
    uint              iter;       /* iterator for current buffer position */
};


/* fwd declarations */
struct mem_pool_block* create_singleblock(struct pool_alloc* pool,
                                           uint item_size, uint block_size);
void destroy_singleblock(struct pool_alloc* pool, struct mem_pool_block* block);

/* callback functions for binding pool-alloc to generic allocator */
void* p_alloc(size_t size, const char* source, uint line, uint mem_id, void* param)
{
    ASSERT(((struct pool_alloc*)param)->item_sz == size);
    return mem_pool_alloc((struct pool_alloc*)param);
}

void p_free(void* p, void* param)
{
    mem_pool_free((struct pool_alloc*)param, p);
}

void* p_alignedalloc(size_t size, uint8 alignment, const char* source,
                     uint line, uint mem_id, void* param)
{
    ASSERT(((struct pool_alloc*)param)->item_sz == size);
    return mem_pool_alloc((struct pool_alloc*)param);
}

void p_alignedfree(void* p, void* param)
{
    mem_pool_free((struct pool_alloc*)param, p);
}

/* */
result_t mem_pool_create(struct allocator* alloc,
                         struct pool_alloc* pool,
                         uint item_size, uint block_size, uint mem_id)
{
    struct mem_pool_block* block;

    memset(pool, 0x00, sizeof(struct pool_alloc));
    pool->item_sz = item_size;
    pool->items_max = block_size;
    pool->mem_id = mem_id;
    pool->alloc = alloc;

    /* create the first block */
    block = create_singleblock(pool, item_size, block_size);
    if (block == NULL)  {
        mem_pool_destroy(pool);
        return RET_OUTOFMEMORY;
    }

    return RET_OK;
}

void mem_pool_destroy(struct pool_alloc* pool)
{
    /* destroy all blocks of memory pool */
    struct linked_list* node = pool->blocks;
    while (node != NULL)    {
        struct linked_list* next = node->next;
        destroy_singleblock(pool, (struct mem_pool_block*)node->data);
        node = next;
    }
}

struct mem_pool_block* create_singleblock(struct pool_alloc* pool,
                                          uint item_size, uint block_size)
{
    uint i;
    size_t total_sz =
        sizeof(struct mem_pool_block) +
        item_size*block_size +
        sizeof(void*)*block_size;
    uint8* buff = (uint8*)A_ALIGNED_ALLOC(pool->alloc, total_sz, pool->mem_id);
    if (buff == NULL)
        return NULL;
    memset(buff, 0x00, total_sz);

    struct mem_pool_block* block = (struct mem_pool_block*)buff;
    buff += sizeof(struct mem_pool_block);
    block->buffer = buff;
    buff += item_size*block_size;
    block->ptrs = (void**)buff;

    /* assign pointer references to buffer */
    for (i = 0; i < block_size; i++)
        block->ptrs[block_size-i-1] = block->buffer + i*item_size;
    block->iter = block_size;

    /* add to linked-list of the pool */
    list_addlast(&pool->blocks, &block->node, block);
    pool->blocks_cnt++;
    return block;
}

void destroy_singleblock(struct pool_alloc* pool, struct mem_pool_block* block)
{
    list_remove(&pool->blocks, &block->node);
    A_ALIGNED_FREE(pool->alloc, block);
    pool->blocks_cnt--;
}

void* mem_pool_alloc(struct pool_alloc* pool)
{
    struct mem_pool_block* block;
    struct linked_list* node = pool->blocks;

    while (node != NULL)   {
        block = (struct mem_pool_block*)node->data;
        if (block->iter > 0)
            return block->ptrs[--block->iter];

        node = node->next;
    }

    /* couldn't find a free block, create a new one */
    block = create_singleblock(pool, pool->item_sz, pool->items_max);
    if (block == NULL)
        return NULL;

    return block->ptrs[--block->iter];
}


void mem_pool_free(struct pool_alloc* pool, void* ptr)
{
    /* find the block that pointer belongs to */
    struct linked_list* node = pool->blocks;
    struct mem_pool_block* block;
    uint buffer_sz = pool->items_max * pool->item_sz;
    uint8* u8ptr = (uint8*)ptr;

    while (node != NULL)   {
        block = (struct mem_pool_block*)node->data;
        if (u8ptr >= block->buffer && u8ptr < (block->buffer + buffer_sz))  {
            ASSERT(block->iter != pool->items_max);
            block->ptrs[block->iter++] = ptr;
            return;
        }
        node = node->next;
    }

    /* memory block does not belong to the pool?! */
    ASSERT(0);
}

void mem_pool_clear(struct pool_alloc* pool)
{
    uint item_size = pool->item_sz;
    uint block_size = pool->items_max;

    struct linked_list* node = pool->blocks;
    while (node != NULL)    {
        struct mem_pool_block* block = (struct mem_pool_block*)node->data;

        /* only re-assign pointer references to buffer */
        for (uint i = 0; i < block_size; i++)
            block->ptrs[block_size-i-1] = block->buffer + i*item_size;
        block->iter = block_size;

        node = node->next;
    }
}

void mem_pool_bindalloc(struct pool_alloc* pool, struct allocator* alloc)
{
    alloc->param = pool;
    alloc->alloc_fn = p_alloc;
    alloc->alignedalloc_fn = p_alignedalloc;
    alloc->alignedfree_fn = p_alignedfree;
    alloc->free_fn = p_free;
    alloc->save_fn = NULL;
    alloc->load_fn = NULL;
}

uint mem_pool_getleaks(struct pool_alloc* pool)
{
    uint count = 0;
    struct linked_list* node = pool->blocks;
    struct mem_pool_block* block;

    while (node != NULL)    {
        block = (struct mem_pool_block*)node->data;
        count += (pool->items_max - block->iter);
        node = node->next;
    }
    return count;
}

/*************************************************************************************************
 * Thread-safe
 */
/* callback functions for binding pool-alloc to generic allocator */
void* p_alloc_ts(size_t size, const char* source, uint line, uint mem_id, void* param)
{
    ASSERT(((struct pool_alloc_ts*)param)->p.item_sz == size);
    return mem_pool_alloc_ts((struct pool_alloc_ts*)param);
}

void p_free_ts(void* p, void* param)
{
    mem_pool_free_ts((struct pool_alloc_ts*)param, p);
}

void* p_alignedalloc_ts(size_t size, uint8 alignment, const char* source,
                        uint line, uint mem_id, void* param)
{
    ASSERT(((struct pool_alloc_ts*)param)->p.item_sz == size);
    return mem_pool_alloc_ts((struct pool_alloc_ts*)param);
}

void p_alignedfree_ts(void* p, void* param)
{
    mem_pool_free_ts((struct pool_alloc_ts*)param, p);
}

/* */
result_t mem_pool_create_ts(struct allocator* alloc, struct pool_alloc_ts* pool,
                            uint item_size, uint block_size, uint mem_id)
{
    memset(pool, 0x00, sizeof(struct pool_alloc_ts));
    mt_mutex_init(&pool->lock);
    return mem_pool_create(alloc, &pool->p, item_size, block_size, mem_id);
}

void mem_pool_destroy_ts(struct pool_alloc_ts* pool)
{
    mt_mutex_release(&pool->lock);
    mem_pool_destroy(&pool->p);
}

void* mem_pool_alloc_ts(struct pool_alloc_ts* pool)
{
    mt_mutex_lock(&pool->lock);
    void* ptr = mem_pool_alloc(&pool->p);
    mt_mutex_unlock(&pool->lock);
    return ptr;
}

void mem_pool_free_ts(struct pool_alloc_ts* pool, void* ptr)
{
    mt_mutex_lock(&pool->lock);
    mem_pool_free(&pool->p, ptr);
    mt_mutex_unlock(&pool->lock);
}

uint mem_pool_getleaks_ts(struct pool_alloc_ts* pool)
{
    return mem_pool_getleaks(&pool->p);
}

void mem_pool_clear_ts(struct pool_alloc_ts* pool)
{
    mem_pool_clear(&pool->p);
}

void mem_pool_bindalloc_ts(struct pool_alloc_ts* pool, struct allocator* alloc)
{
    alloc->param = pool;
    alloc->alloc_fn = p_alloc_ts;
    alloc->free_fn = p_free_ts;
    alloc->alignedalloc_fn = p_alignedalloc_ts;
    alloc->alignedfree_fn = p_alignedfree_ts;
    alloc->save_fn = NULL;
    alloc->load_fn = NULL;
}
