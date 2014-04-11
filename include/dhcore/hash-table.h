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

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "types.h"
#include "linked-list.h"
#include "array.h"
#include "allocator.h"
#include "core-api.h"
#include "hash.h"

/**
 * @defgroup htable Hash-table
 */


 /**
 * hash table item, used in open/fixed hash tables
 * @ingroup htable
 */
struct hashtable_item
{
    uint hash;   /**< hash */
    uptr_t value;  /**< saved user value */
};

 /**
 * hash table item, used in chained hash tables
 * @ingroup htable
 */
struct hashtable_item_chained
{
    uint hash;   /**< hash */
    uptr_t value;  /**< saved user value */
    struct linked_list node;    /* for chaining hash collisions */
};

/**
 * chained hash table
 * cjaomed hash tables can hold unlimited number of items\n
 * but needs dynamic allocator and may turn into a simple linked-list in worst case
 * @ingroup htable
 */
struct hashtable_chained
{
    struct allocator* item_alloc;
    struct allocator* alloc;
    uint mem_id;
    struct linked_list** pslots;
    uint slots_cnt;
    uint items_cnt;
};

/* chained hash table functions
 **
 * create: creates hash table data
 * @param alloc allocator for hash table main buffers which is created immediately after call
 * @param item_alloc hash item allocator (hashtable_item_chained), recommended allocator is mem-pool
 * @param slots_cnt number of slots in hash table, bigger number creates faster hash table
 * @ingroup htable
 */
CORE_API result_t hashtable_chained_create(struct allocator* alloc, struct allocator* item_alloc,
                                       struct hashtable_chained* table,
                                       uint slots_cnt, uint mem_id);

/**
 * destroy hash table
 * @ingroup htable
 */
CORE_API void hashtable_chained_destroy(struct hashtable_chained* table);
/**
 * checks if hash table is empty
 * @ingroup htable
 */
CORE_API int hashtable_chained_isempty(struct hashtable_chained* table);
/**
 * add item to hash table, constains key/value pair
 * @param hash_key hash key, can be created with hash functions
 * @param Integer value, can be casted to pointer
 * @see hash
 * @ingroup htable
 */
CORE_API result_t hashtable_chained_add(struct hashtable_chained* table, uint hash_key,
    uptr_t value);
/**
 * removes hash item from the hash table
 * @ingroup htable
 */
CORE_API void hashtable_chained_remove(struct hashtable_chained* table,
    struct hashtable_item_chained* item);
 /**
  * finds hash table by key
  * @return found item, NULL if not found
  * @see hashtable_item_chained
  * @ingroup htable
  */
CORE_API struct hashtable_item_chained* hashtable_chained_find(
    const struct hashtable_chained* table,
    uint hash_key);
/**
 * clears hash table items
 * @ingroup htable
 */
CORE_API void hashtable_chained_clear(struct hashtable_chained* table);

/**
 * closed hash table\n
 * closed tables holds a limited amount of items, but does not need dynamic allocator\n
 * 'prime number' maximum items are prefered : \n
 * http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
 * @ingroup htable
 */
struct hashtable_fixed
{
    struct allocator* alloc;
    struct hashtable_item* slots;
    uint slots_cnt;
    uint items_cnt;
};

/* closed hash table functions
 **
 * create: creates hash table data
 * @param alloc allocator for hash table main buffers which is created immediately after call
 * @param slots_cnt number of slots in hash table, prime numbers are pefered for optimized hash-table
 * @ingroup htable
 */
CORE_API result_t hashtable_fixed_create(struct allocator* alloc,
                                         struct hashtable_fixed* table,
                                         uint slots_cnt, uint mem_id);

/**
 * destroy hash table
 * @ingroup htable
 */
CORE_API void hashtable_fixed_destroy(struct hashtable_fixed* table);
/**
 * checks if hash table is empty
 * @ingroup htable
 */
CORE_API int hashtable_fixed_isempty(struct hashtable_fixed* table);
/**
 * add item to hash table, constains key/value pair
 * @param hash_key hash key, can be created with hash functions
 * @param Integer value, can be casted to pointer
 * @see hash
 * @ingroup htable
 */
CORE_API result_t hashtable_fixed_add(struct hashtable_fixed* table,
                                      uint hash_key, uptr_t value);
/**
 * removes hash item from the hash table
 * @ingroup htable
 */
CORE_API void hashtable_fixed_remove(struct hashtable_fixed* table, struct hashtable_item* item);
 /**
  * finds hash table by key
  * @return found item, NULL if not found
  * @ingroup htable
  */
CORE_API struct hashtable_item* hashtable_fixed_find(const struct hashtable_fixed* table,
                                                      uint hash_key);
/**
 * clears hash table items
 * @ingroup htable
 */
CORE_API void hashtable_fixed_clear(struct hashtable_fixed* table);

/**
 * @ingroup htable
 */
CORE_API size_t hashtable_fixed_estimate_size(uint slots_cnt);

/**
 * open hash table : same as closed hash table, but grows itself upon extra item additions
 */
struct hashtable_open
{
    struct allocator* alloc;
    struct hashtable_item* slots;
    uint slots_cnt;
    uint items_cnt;
    uint slots_grow;
    uint mem_id;
};

/* open hash table functions
 **
 * create: creates hash table data
 * @param alloc allocator for hash table main buffers which is created immediately after call
 * @param slots_cnt number of slots in hash table, prime numbers are pefered for optimized hash-table
 * @ingroup htable
 */
CORE_API result_t hashtable_open_create(struct allocator* alloc,
                                        struct hashtable_open* table,
                                        uint slots_cnt, uint grow_cnt,
                                        uint mem_id);

/**
 * destroy hash table
 * @ingroup htable
 */
CORE_API void hashtable_open_destroy(struct hashtable_open* table);
/**
 * checks if hash table is empty
 * @ingroup htable
 */
CORE_API int hashtable_open_isempty(struct hashtable_open* table);
/**
 * add item to hash table, constains key/value pair
 * @param hash_key hash key, can be created with hash functions
 * @param Integer value, can be casted to pointer
 * @see hash
 * @ingroup htable
 */
CORE_API result_t hashtable_open_add(struct hashtable_open* table, uint hash_key, uptr_t value);
/**
 * removes hash item from the hash table
 * @ingroup htable
 */
CORE_API void hashtable_open_remove(struct hashtable_open* table, struct hashtable_item* item);
 /**
  * finds hash table by key
  * @return found item, NULL if not found
  * @ingroup htable
  */
CORE_API struct hashtable_item* hashtable_open_find(const struct hashtable_open* table,
                                                      uint hash_key);
/**
 * clears hash table items
 * @ingroup htable
 */
CORE_API void hashtable_open_clear(struct hashtable_open* table);

#endif /* __HASHTABLE_H__ */
