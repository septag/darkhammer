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

#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "types.h"
/**
 * @defgroup queue Queue
 */
 
/**
 * FIFO (first-in-first-out) queue structure \n
 * usage: \n
 * keep a queue* pointer in your root structure (first)\n
 * keep queue structure in each structure you wish to be queued (node)\n
 * provide the node owner as 'data' in push\n
 * @see queue_push
 * @see queue_pop
 * @ingroup queue
 */
struct queue
{
    struct queue* next;
    void*         data;
};

/**
 * Pushesa an item into queue
 * @param pqueue pointer to the root queue (can be NULL)
 * @param item queue item to push
 * @param data pointer to owner of the 'item'
 * @ingroup queue
 */
INLINE void queue_push(struct queue** pqueue, struct queue* item, void* data)
{
    if (*pqueue != NULL)    {
        struct queue* last = *pqueue;
        while (last->next != NULL)  
            last = last->next;
        last->next = item;
    }    else    {
        *pqueue = item;
    }
    item->next = NULL;
    item->data = data;
}

/**
 * Pops an item from queue
 * @param pqueue pointer to the root queue item, if the last item is removed *plist will be NULL
 * @return popped queue item, check it's ->data for owner data
 * @ingroup queue
 */
INLINE struct queue* queue_pop(struct queue** pqueue)
{
    struct queue* item = *pqueue;
    if (*pqueue != NULL)    {
        *pqueue = (*pqueue)->next;
        item->next = NULL;
    }
    return item;
}

#endif /* __QUEUE_H__ */
