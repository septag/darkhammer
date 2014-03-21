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

 
#ifndef __STACK_H__
#define __STACK_H__

/**
 * @defgroup stack Stack
 */

#include "types.h"

/**
 * FILO (first-in-last-out) stack structure\n
 * usage :\n
 * keep stack* pointer in your root structure (first)\n
 * keep stack structure in each structure you wish be stack (node)\n
 * provide the node owner as 'data' in push\n
 * @see stack_push
 * @see stack_pop
 * @ingroup stack
 */
struct stack
{
    struct stack*   prev;
    void*           data;
};

/**
 * push item into stack
 * @param pstack pointer to root stack item (can be NULL)
 * @param item stack item to be pushed
 * @param data custom data to keep in stack item, mostly owner of the current stack item
 * @ingroup stack
 */
INLINE void stack_push(struct stack** pstack, struct stack* item, void* data)
{
    item->prev = *pstack;
    *pstack = item;
    item->data = data;
}

/**
 * pop item from stack
 * @param pstack pointer to root stack item, will be NULL if last item is popped
 * @return popped stack item, look in ->data member variable for owner data
 * @ingroup stack
 */
INLINE struct stack* stack_pop(struct stack** pstack)
{
    struct stack* item = *pstack;
    if (*pstack != NULL)    {
        *pstack = (*pstack)->prev;
        item->prev = NULL;
    }
    return item;
}

#endif /* __STACK_H__ */
