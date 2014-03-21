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

#include "array.h"
#include "err.h"

result_t arr_create(struct allocator* alloc,
                    struct array* arr,
                    uint item_sz, uint init_item_cnt, uint expand_cnt,
                    uint mem_id)
{
    memset(arr, 0x00, sizeof(struct array));
    arr->buffer = A_ALIGNED_ALLOC(alloc, item_sz*init_item_cnt, mem_id);
    if (arr->buffer == NULL)
        return RET_OUTOFMEMORY;

    arr->alloc = alloc;
    arr->expand_sz = expand_cnt;
    arr->item_cnt = 0;
    arr->item_sz = item_sz;
    arr->max_cnt = init_item_cnt;
    arr->mem_id = mem_id;

    return RET_OK;
}

void arr_destroy(struct array* arr)
{
    ASSERT(arr != NULL);

    if (arr->buffer != NULL)    {
        ASSERT(arr->alloc != NULL);
        A_ALIGNED_FREE(arr->alloc, arr->buffer);
    }
}

void* arr_add(struct array* arr)
{
    result_t r = RET_OK;
    if (arr_needexpand(arr))
        r = arr_expand(arr);

    if (r == RET_OK)    {
        void* p = (uint8*)arr->buffer + arr->item_cnt*arr->item_sz;
        arr->item_cnt ++;
        return p;
    }   else    {
        return NULL;
    }
}


result_t arr_expand(struct array* arr)
{
    ASSERT(arr != NULL);
    ASSERT(arr->alloc != NULL);
    ASSERT(arr->buffer != NULL);

    /* reallocate */
    uint newsz = arr->max_cnt + arr->expand_sz;
    void* tmp = A_ALIGNED_ALLOC(arr->alloc, newsz*arr->item_sz, arr->mem_id);
    if (tmp == NULL)
        return RET_OUTOFMEMORY;

    /* we now have a bigger buffer in temp (realloc)
     * copy previous items into the temp buffer and assign temp buffer to the array */
    memcpy(tmp, arr->buffer, arr->item_cnt*arr->item_sz);
    A_ALIGNED_FREE(arr->alloc, arr->buffer);

    arr->buffer = tmp;
    arr->max_cnt = newsz;
    return RET_OK;
}


