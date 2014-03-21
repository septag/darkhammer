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

#include "zip.h"
#include "miniz/miniz.h"

/* */
size_t zip_compressedsize(size_t src_size)
{
    return (size_t)compressBound((mz_ulong)src_size);
}

size_t zip_compress(void* dest_buffer, size_t dest_size,
                    const void* buffer, size_t size, enum compress_mode mode)
{
    int c_level;
    switch (mode)   {
        case COMPRESS_NORMAL:   c_level = Z_DEFAULT_COMPRESSION;    break;
        case COMPRESS_FAST:     c_level = Z_BEST_SPEED;             break;
        case COMPRESS_BEST:     c_level = Z_BEST_COMPRESSION;       break;
        case COMPRESS_NONE:     c_level = Z_NO_COMPRESSION;         break;
        default:				c_level = Z_DEFAULT_COMPRESSION;	break;
    }

    uLongf dsize;
    int r = compress2((Bytef*)dest_buffer, &dsize, (const Bytef*)buffer, (uLongf)size, c_level);
    return (r == Z_OK) ? (size_t)dsize : 0;
}

size_t zip_decompress(void* dest_buffer, size_t dest_size, const void* buffer, size_t size)
{
    uLongf dsize = (uLongf)dest_size;
    int r = uncompress((Bytef*)dest_buffer, &dsize, (Bytef*)buffer, (uLongf)size);
    return (r == Z_OK) ? (size_t)dsize : 0;
}
