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

#ifndef __ARRAY_H__
#define __ARRAY_H__

#include "types.h"
#include "core-api.h"
#include "allocator.h"

/**
 * @defgroup array Array
 * Expanding array\n
 * holds a buffer and it's properties for expansion on demand\n
 * Usage example:\n
 * @code
 * struct array myarr;
 * arr_create(mem_heap(), &myarr, sizeof(uint), 100, 200, 0);
 * printf("adding numbers\n");
 * for (uint i = 0; i < 10; i++)  {
 *      uint* pval = arr_add(&myarr);
 *      *paval = i;
 * }
 * printf("listing numbers ...\n");
 * for (uint i = 0; i < 10; i++)  {
 *      printf("%d\n", ((uint*)myarr.buffer)[i]);
 * }
 * arr_destroy(&myarr);
 * @endcode
 * @ingroup array
 */
struct array
{
    struct allocator*   alloc;      /**< allocator */
    void*               buffer;     /**< array buffer, can be casted to any pointer type for use */
    uint              item_cnt;   /**< current item count in the array */
    uint              max_cnt;    /**< maximum item count */
    uint              item_sz;    /**< item size in bytes */
    uint              mem_id;     /**< memory id */
    uint              expand_sz;  /**< in number of items */
};

/**
 * creates an array
 * @param alloc internal memory allocator for array data
 * @param arr array to be created
 * @param item_sz each array element item size (in bytes)
 * @param init_item_cnt initial item count
 * @param expand_cnt number of items to expand if needed
 * @see arr_expand
 * @ingroup cnt
 */
CORE_API result_t arr_create(struct allocator* alloc,
                             struct array* arr,
                             uint item_sz, uint init_item_cnt, uint expand_cnt,
                             uint mem_id);
/**
 * destroys array
 * @ingroup array
 */
CORE_API void arr_destroy(struct array* arr);

/**
 * expand the array buffer by the size defined in arr_create\n
 * recommended method is to check to array expansion before adding new items\n
 * example: if (arr_needexpand(a))  arr_expand(a)\n
 *          a.buffer[a.item_cnt++] = item
 * @see arr_needexpand
 * @see arr_create
 * @ingroup array
 */
CORE_API result_t arr_expand(struct array* arr);

/**
 * helper array function for expanding buffer, useful for adding objects
 * expands if needed and returns newly created buffer, and adds item count to array
 * @return newly created pointer to last item in the buffer, =NULL if out of memory
 */
CORE_API void* arr_add(struct array* arr);

/**
 * checks if array needs expansion
 * @see arr_expand
 * @ingroup array
 */
INLINE bool_t arr_needexpand(const struct array* arr)
{
    return (arr->max_cnt == arr->item_cnt);
}

/**
 * checks if array is empty
 * @ingroup array
 */
INLINE bool_t arr_isempty(const struct array* arr)
{
    return (arr->item_cnt == 0);
}

/**
 * clear array items, but does not free or resize it's buffer
 * @ingroup array
 */
INLINE void arr_clear(struct array* arr)
{
    arr->item_cnt = 0;
}

#endif /*__ARRAY_H__*/
