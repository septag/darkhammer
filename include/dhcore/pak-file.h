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


#ifndef __PAKFILE_H__
#define __PAKFILE_H__

#include <stdio.h>
#include "types.h"
#include "core-api.h"
#include "allocator.h"
#include "hash-table.h"
#include "array.h"
#include "pool-alloc.h"
#include "zip.h"
#include "file-io.h"

/**
 * @defgroup pak Pak files
 */

/* fwd declarations */
struct file_mgr;

/**
 * pak file - contains zipped archive of multiple files\n
 * used in file-mgr for compression and fast extraction of files\n
 * @see fileio
 *
 */
struct pak_file
{
    FILE* f;
    struct hashtable_open table; /* hash-table for referencing pak files */
    struct array items; /* file items in the pak (see pak-file.c) */
    enum compress_mode compress_mode; /* compression mode (see zip.h) */
    int init_create;
    struct allocator table_alloc;
};

/**
 * create pak file on disk and get it ready for putting files in it
 * @param alloc memory allocator for internal pak_file data
 * @param pakfilepath pak file which will be created on disk (absolute path)
 * @param mode zip compression mode
 * @see zip
 * @ingroup pak
 */
CORE_API result_t pak_create(struct pak_file* pak, struct allocator* alloc,
                             const char* pakfilepath, enum compress_mode mode,
                             uint mem_id);

/**
 * open pak file from disk, get it ready to fetch files from it
 * @param pakfilepath file path of the pak file on disk (absolute path)
 * @param alloc allocator for internal pak_file data
 * @ingroup pak
 */
CORE_API result_t pak_open(struct pak_file* pak, struct allocator* alloc,
                           const char* pakfilepath, uint mem_id);
/**
 * closes pak file and release internal data
 * @ingroup pak
 */
CORE_API void pak_close(struct pak_file* pak);
/**
 * checks if pak file is opened
 * @ingroup pak
 */
CORE_API int pak_isopen(struct pak_file* pak);

/**
 * compress and put an opened file into pak
 * @param alloc temp-allocator for decompressing buffers inside the routine
 * @param src_file source file which must be already opened
 * @param dest_path destination filepath (alias) which will be saved in pak file_id
 * @ingroup pak
 */
CORE_API result_t pak_putfile(struct pak_file* pak,
                              struct allocator* tmp_alloc, file_t src_file, const char* dest_path);

/**
 * find a file in pak
 * @param filepath filepath (case sensitive) of dest_path provided in 'pak_putfile' when -
 *                  archive was created.
 * @return id of the file. INVALID_INDEX if file is not found.
 * @ingroup pak
 */
CORE_API uint pak_findfile(struct pak_file* pak, const char* filepath);

/**
 * decompress and get a file from pak
 * @param alloc memory allocator for creating memory file
 * @param tmp_alloc temp-allocator for internal memory allocation
 * @param file_id file-id of the file in the pak, must be fetched from 'pak_findfile'
 * @return handle to the opened file in memory, ready to read
 * @ingroup pak
 */
CORE_API file_t pak_getfile(struct pak_file* pak, struct allocator* alloc,
                            struct allocator* tmp_alloc,
                            uint file_id, uint mem_id);

/**
 * creates/allocates list of files inside pak-file
 * @param alloc memory allocator for the list
 * @param pcnt outputs file count
 * @return array of char* which contains list of files (should be freed if not needed by caller)\n
 * for striding the char** array you should use DH_PATH_MAX strides (char* + index*DH_PATH_MAX)
 * @ingroup pak
 */
CORE_API char* pak_createfilelist(struct pak_file* pak, struct allocator* alloc, OUT uint* pcnt);

#endif /*__PAKFILE_H__*/
