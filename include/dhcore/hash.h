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

/*******************************************************************
*   Hashing Routines
*   Murmur3 Hash: http://code.google.com/p/smhasher/
*   Note: Dissed other hashes, because murmur seems to beat them all
*   Seed parameter in murmur hash functions are for variation
*   you should always provide fixed seed values to get same result
********************************************************************/

#ifndef __HASH_H__
#define __HASH_H__

#include "types.h"
#include "core-api.h"

/**
 * @defgroup hash Hashing functions
 */

#if defined(_X64_)
typedef struct hash_s
{
    uint64  h[2];
} hash_t;
#else
typedef struct hash_s
{
    uint  h[4];
} hash_t;
#endif

/**
 * Incremental hash structure
 * @see hash_murmurincr_begin @ingroup hash
 */
struct hash_incr
{
	uint hash;
	uint tail;
	uint cnt;
	size_t size;
};

/**
 * Test for 128bit hash equality
 * @ingroup hash
 */
INLINE int hash_isequal(hash_t h1, hash_t h2);

#if defined(_X64_)
INLINE int hash_isequal(hash_t h1, hash_t h2)
{
    return (h1.h[0] == h2.h[0] && h1.h[1] == h2.h[1]);
}

INLINE void hash_set(hash_t* dest, hash_t src)
{
	dest->h[0] = src.h[0];
	dest->h[1] = src.h[1];
}

INLINE void hash_zero(hash_t* h)
{
	h->h[0] = 0;
	h->h[1] = 0;
}

#else
INLINE int hash_isequal(hash_t h1, hash_t h2)
{
    return (h1.h[0] == h2.h[0] && h1.h[1] == h2.h[1] && h1.h[2] == h2.h[2] && h1.h[3] == h2.h[3]);
}

INLINE void hash_set(hash_t* dest, hash_t src)
{
	dest->h[0] = src.h[0];
	dest->h[1] = src.h[1];
	dest->h[2] = src.h[2];
	dest->h[3] = src.h[3];
}

INLINE void hash_zero(hash_t* h)
{
	h->h[0] = 0;		h->h[1] = 0;
	h->h[2] = 0;		h->h[3] = 0;
}
#endif


/**
 * murmur 32bit hash
 * @param key buffer containing data to be hashed
 * @param size_bytes size of buffer (bytes)
 * @param seed random seed value (must be same between hashes in order to compare
 * @return 32bit hash value
 * @ingroup hash
 */
CORE_API uint hash_murmur32(const void* key, size_t size_bytes, uint seed);

/**
 * murmur 128bit hash
 * @param key buffer containing data to be hashed
 * @param size_bytes size of buffer (bytes)
 * @param seed random seed value (must be same between hashes in order to compare
 * @return 128bit hash value
 * @ingroup hash
 */
CORE_API hash_t hash_murmur128(const void* key, size_t size_bytes, uint seed);

/**
 * hash 64bit value to 32bit
 * @param n 32bit value to be hashed
 * @return 32bit hash value
 * @ingroup hash
 */
CORE_API uint hash_u64(uint64 n);

/**
 * incremental hashing, based on murmur2A\n
 * begins incremental hashing, user must call _add and _end functions after _begin\n
 * @see hash_murmurincr_add @see hash_murmurincr_end
 * @ingroup hash
 */
CORE_API void hash_murmurincr_begin(struct hash_incr* h, uint seed);

/**
 * incremental hash addition
 * @see hash_murmurincr_begin
 * @ingroup hash
 */
CORE_API void hash_murmurincr_add(struct hash_incr* h, const void* data, size_t size);

/**
 * incremental hash end
 * @return 32bit hash value
 * @see hash_murmurincr_begin
 * @ingroup hash
 */
CORE_API uint hash_murmurincr_end(struct hash_incr* h);

/**
 * Hashes a null-terminated string to 32-bit integer
 * @ingroup hash
 */
CORE_API uint hash_str(const char* str);


#endif
