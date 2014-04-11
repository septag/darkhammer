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


#include <stdio.h>
#include "pak-file.h"
#include "err.h"
#include "pak-file-fmt.h"
#include "str.h"
#include "numeric.h"

#define ITEM_BLOCK_SIZE     100
#define PAK_MAJOR_VERSION   1
#define PAK_MINOR_VERSION   0
#define HSEED           8263

void finalize(struct pak_file* pak)
{
    ASSERT(pak->f != NULL);
    ASSERT(pak->init_create);

    /* re-write header and item table into the file */
    struct pak_header header;
    memset(&header, 0x00, sizeof(header));

    /* current position is assumed to be the end of the files data */
    strcpy(header.sig, PAK_SIGN);
    VERSION_MAKE(header.v, PAK_MAJOR_VERSION, PAK_MINOR_VERSION);
    header.items_cnt = pak->items.item_cnt;
    header.items_offset = ftell(pak->f);
    header.compress_mode = (uint)pak->compress_mode;

    fseek(pak->f, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, pak->f);

    fseek(pak->f, (long)header.items_offset, SEEK_SET);
    fwrite(pak->items.buffer, pak->items.item_sz, pak->items.item_cnt, pak->f);
}

result_t pak_create(struct pak_file* pak, struct allocator* alloc,
                    const char* pakfilepath, enum compress_mode mode, uint mem_id)
{
    result_t r;

    memset(pak, 0x00, sizeof(struct pak_file));

    /* open pak file */
    pak->f = fopen(pakfilepath, "wb");
    if (pak->f == NULL)     {
        err_printf(__FILE__, __LINE__, "creating pak-file failed: could not open '%s' for write",
                   pakfilepath);
        return RET_FILE_ERROR;
    }

    /* init internal data for arbiatary writing */
    r = arr_create(alloc, &pak->items, sizeof(struct pak_item),
                   ITEM_BLOCK_SIZE, ITEM_BLOCK_SIZE, mem_id);
    if (IS_FAIL(r))     {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

    r = hashtable_open_create(alloc, &pak->table, ITEM_BLOCK_SIZE, ITEM_BLOCK_SIZE, mem_id);
    if (IS_FAIL(r))     {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

    /* reserve size for the header */
    fseek(pak->f, sizeof(struct pak_header), SEEK_SET);
    pak->compress_mode = mode;
    pak->init_create = TRUE;

    return RET_OK;
}


result_t pak_open(struct pak_file* pak, struct allocator* alloc,
                  const char* pakfilepath, uint mem_id)
{
    result_t r;
    memset(pak, 0x00, sizeof(struct pak_file));

    pak->f = fopen(pakfilepath, "rb");
    if (pak->f == NULL)     {
        err_printf(__FILE__, __LINE__, "opening pak-file failed: could not open '%s'",
                   pakfilepath);
        return RET_FILE_ERROR;
    }

    /* read header */
    struct pak_header header;
    fread(&header, sizeof(header), 1, pak->f);
    if (!str_isequal(header.sig, PAK_SIGN) ||
        !VERSION_CHECK(header.v, PAK_MAJOR_VERSION, PAK_MINOR_VERSION) ||
        header.items_cnt == 0)
    {
        err_printf(__FILE__, __LINE__, "opening pak-file failed: file '%s' is an invalid pak",
                   pakfilepath);
        return RET_FAIL;
    }

    /* init internal data for reading */
    r = arr_create(alloc, &pak->items, sizeof(struct pak_item),
                   (uint)header.items_cnt, ITEM_BLOCK_SIZE, mem_id);
    if (IS_FAIL(r))     {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

    r = hashtable_open_create(alloc, &pak->table, (uint)header.items_cnt, ITEM_BLOCK_SIZE, mem_id);
    if (IS_FAIL(r))     {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

    /* load items */
    fseek(pak->f, (long)header.items_offset, SEEK_SET);
    fread(pak->items.buffer, sizeof(struct pak_item), (size_t)header.items_cnt, pak->f);
    pak->items.item_cnt = (uint)header.items_cnt;

    struct pak_item* items = (struct pak_item*)pak->items.buffer;
    for (uint i = 0; i < header.items_cnt; i++)   {
        struct pak_item* item = &items[i];
        hashtable_open_add(&pak->table, hash_str(item->filepath), i);
    }

    return RET_OK;
}

void pak_close(struct pak_file* pak)
{
    if (pak->init_create)
        finalize(pak);

    if (pak->f != NULL)
        fclose(pak->f);

    hashtable_open_destroy(&pak->table);
    arr_destroy(&pak->items);

    memset(pak, 0x00, sizeof(struct pak_file));
}

int pak_isopen(struct pak_file* pak)
{
    return (pak->f != NULL);
}


result_t pak_putfile(struct pak_file* pak,
                     struct allocator* tmp_alloc, file_t src_file, const char* dest_path)
{
    ASSERT(fio_isopen(src_file));

    size_t size = fio_getsize(src_file);
    size_t compress_size;

    if (size == 0)      {
        return RET_OK;
    }

    if (size > UINT32_MAX)  {
        err_printf(__FILE__, __LINE__, "put file into pak failed: file '%s' is more than 4gb",
                   fio_getpath(src_file));
        return RET_FAIL;
    }

    /* compress/copy the file data into the pak file */
    void* file_buffer = A_ALLOC(tmp_alloc, size, 0);
    if (file_buffer == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }
    fio_read(src_file, file_buffer, size, 1);
    hash_t file_hash = hash_murmur128(file_buffer, size, HSEED);

    if (pak->compress_mode != COMPRESS_NONE)    {
        /* compress the buffer, then write it into the pak-file */
        compress_size = zip_compressedsize(size);
        void* compress_buffer = A_ALLOC(tmp_alloc, compress_size, 0);
        if (compress_buffer == NULL)    {
            A_FREE(tmp_alloc, file_buffer);
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            return RET_OUTOFMEMORY;
        }

        compress_size = zip_compress(compress_buffer, compress_size, file_buffer, size,
                                     pak->compress_mode);
        fwrite(compress_buffer, compress_size, 1, pak->f);
        A_FREE(tmp_alloc, compress_buffer);
    }    else    {
        /* just copy the buffer into target */
        fwrite(file_buffer, size, 1, pak->f);
        compress_size = size;
    }
    A_FREE(tmp_alloc, file_buffer);

    /* add file item description */
    if (arr_needexpand(&pak->items))    {
        arr_expand(&pak->items);
    }
    struct pak_item* items = (struct pak_item*)pak->items.buffer;
    struct pak_item* item = &items[pak->items.item_cnt];
    strcpy(item->filepath, (dest_path[0] == '/') ? (dest_path + 1) : (dest_path));
    item->offset = ftell(pak->f) - compress_size;
    item->size = (uint)compress_size;
    item->unzip_size = (uint)size;
    hash_set(&item->hash, file_hash);
    /* add to hash-table */
    hashtable_open_add(&pak->table, hash_str(dest_path), pak->items.item_cnt);
    pak->items.item_cnt ++;

    return RET_OK;
}

uint pak_findfile(struct pak_file* pak, const char* filepath)
{
    /* if path starts with '/' ignore the first char */
    const char* rpath = (filepath[0] == '/') ? (filepath + 1) : filepath;

    struct hashtable_item* titem = hashtable_open_find(&pak->table, hash_str(rpath));
    if (titem != NULL)     return (uint)titem->value;
    else                   return INVALID_INDEX;
}

file_t pak_getfile(struct pak_file* pak, struct allocator* alloc, struct allocator* tmp_alloc,
                   uint file_id, uint mem_id)
{
    ASSERT(file_id != INVALID_INDEX);
    ASSERT(file_id < pak->items.item_cnt);

    struct pak_item* items = (struct pak_item*)pak->items.buffer;
    struct pak_item* item = &items[file_id];

    void* file_buffer = A_ALLOC(tmp_alloc, item->size, 0);
    void* unzip_buffer = A_ALLOC(alloc, item->unzip_size, 0);

    if (file_buffer == NULL || unzip_buffer == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }

    fseek(pak->f, (long)item->offset, SEEK_SET);
    fread(file_buffer, item->size, 1, pak->f);
    if (pak->compress_mode != COMPRESS_NONE)    {
        zip_decompress(unzip_buffer, item->unzip_size, file_buffer, item->size);
    }    else    {
        memcpy(unzip_buffer, file_buffer, item->unzip_size);
    }

    A_FREE(tmp_alloc, file_buffer);

    /* check hash validity */
    hash_t h = hash_murmur128(unzip_buffer, item->unzip_size, HSEED);
    if (!hash_isequal(h, item->hash))   {
        err_printf(__FILE__, __LINE__, "pak get-file failed: data validity error for '%s'",
                   item->filepath);
        A_FREE(alloc, unzip_buffer);
        return RET_FAIL;
    }

    /* attach it to a file and return */
    return fio_attachmem(alloc, unzip_buffer, item->unzip_size, item->filepath, mem_id);
}

char* pak_createfilelist(struct pak_file* pak, struct allocator* alloc, OUT uint* pcnt)
{
	ASSERT(pcnt);
	if (arr_isempty(&pak->items))	{
		*pcnt = 0;
		return NULL;
	}

	char* filelist = (char*)A_ALLOC(alloc, DH_PATH_MAX*pak->items.item_cnt, 0);
	if (filelist == NULL)	{
		*pcnt = 0;
		return NULL;
	}
	memset(filelist, 0x00, DH_PATH_MAX*pak->items.item_cnt);

	*pcnt = pak->items.item_cnt;
	for (uint i = 0; i < *pcnt; i++)
		strcpy(filelist + i*DH_PATH_MAX, ((struct pak_item*)pak->items.buffer)[i].filepath);
	return filelist;
}

