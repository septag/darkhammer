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

#ifndef __ZIP_H__
#define __ZIP_H__

/**
 * @defgroup zip Zip
 * Low-level buffer compression/decompression using miniz (INFLATE)\n
 * @ingroup zip
 */

#include "types.h"
#include "core-api.h"

/**
 * @ingroup zip
 */
enum compress_mode
{
    COMPRESS_NORMAL = 0,
    COMPRESS_FAST,
    COMPRESS_BEST,
    COMPRESS_NONE
};

/**
 * roughly estimate maximum size of the compressed buffer 
 * @param src_size Size (bytes) of uncompressed buffer
 * @return Estimated size of compressed target buffer
 * @ingroup zip
 */
CORE_API size_t zip_compressedsize(size_t src_size);

/**
 * compress buffer 
 * @param dest_buffer Destination buffer that will be filled with compressed data
 * @param dest_size Maximum size of destiniation buffer, usually fetched by @e zip_compressedsize
 * @return actual Size of compressed buffer, returns 0 if 
 * @ingroup zip
 */
CORE_API size_t zip_compress(void* dest_buffer, size_t dest_size, const void* buffer,
		size_t size, enum compress_mode mode);

/**
 * decompress buffer
 * @param dest_buffer Uncompressed destination buffer 
 * @param dest_size Uncompressed buffer size, this value should be saved when buffer is compressed
 * @return actual Size of uncompressed buffer
 * @ingroup zip
 */
CORE_API size_t zip_decompress(void* dest_buffer, size_t dest_size, 
		const void* buffer, size_t size);

#endif /* __ZIP_H__ */
