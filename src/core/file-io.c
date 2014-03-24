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

#include "file-io.h"
#include "mem-mgr.h"
#include "err.h"
#include "numeric.h"
#include "str.h"
#include "pak-file.h"
#include "log.h"
#include "hash-table.h"
#include "mt.h"
#include "util.h"

#if defined(_FILEMON_)
/* You'll need 3rdparty EFSW library (forked): https://bitbucket.org/sepul/efsw */
#define EFSW_DYNAMIC
#include "efsw/efsw.h"
#undef EFSW_DYNAMIC
#endif

#define MEM_BLOCK_SIZE 4096
#define MON_BUFFER_SIZE (256*1024)
#define MON_ITEM_SIZE 200

/*************************************************************************************************
 * types
 */
/* callbacks and declerations for read/write to different file types */
typedef size_t (*pfnio_file_read)(file_t f, void* buffer, size_t item_size, size_t items_cnt);
typedef size_t (*pfnio_file_write)(file_t f, const void* buffer, size_t item_size, size_t items_cnt);

struct vdir
{
    bool_t monitor;
    char path[DH_PATH_MAX];

#if defined(_FILEMON_)
    efsw_watchid watchid;
    mt_mutex mtx;
    void* volatile backbuff;
    void* volatile frontbuff;
    struct array backarr;  /* item: char[DH_PATH_MAX] */
    struct array frontarr; /* item: char[DH_PATH_MAX] */
#endif
};

#if defined(_FILEMON_)
struct mon_item
{
    pfn_fio_modify fn;
    reshandle_t hdl;    /* possible resource handle */
    uptr_t param1;
    uptr_t param2;
};
#endif

/**
 * file manager that handles virtual-filesystems and pool allocators for file io\n
 * virtual-filesystems are either virtual-directories or 'pak' file archives
 */
struct file_mgr
{
    bool_t init;
    struct pool_alloc_ts diskfile_alloc;
    struct pool_alloc_ts memfile_alloc;
    struct array vdirs;   /* item: vdir */
    struct array paks;    /* item: pak_file */
    struct hashtable_open mon_table;    /* key: filepath(hashed), value: pointer to mon_item */
#if defined(_FILEMON_)
    efsw_watcher watcher;
#endif
};

struct file_header
{
    enum file_type type;
    char path[DH_PATH_MAX];
    size_t size;
    enum file_mode mode;
    pfnio_file_read read_fn;
    pfnio_file_write write_fn;
};

struct disk_file
{
    FILE* file;
};

struct mem_file
{
    struct allocator* alloc;
    uint8* buffer;
    size_t offset;
    size_t max_size;
    uint mem_id;
};

/*************************************************************************************************/
/* callbacks for directory monitoring */
#if defined(_FILEMON_)
result_t fio_mon_init(void* pt);
void fio_mon_release(void* pt);
result_t fio_mon_kernel(void* pt);
bool_t fio_vdir_initmon(struct vdir* vd);
void fio_vdir_releasemon(struct vdir* vd);
#endif

size_t fio_readdisk(file_t f, void* buffer, size_t item_size, size_t items_cnt);
size_t fio_writedisk(file_t f, const void* buffer, size_t item_size, size_t items_cnt);
size_t fio_readmem(file_t f, void* buffer, size_t item_size, size_t items_cnt);
size_t fio_writemem(file_t f, const void* buffer, size_t item_size, size_t items_cnt);

/* resolve and open a filepath from the disk */
FILE* open_resolvepath(const char* filepath);

/*************************************************************************************************
 * globals
 */
static struct file_mgr g_fio;
static bool_t g_fio_zero = FALSE;

/*************************************************************************************************/
result_t fio_initmgr()
{

    memset(&g_fio, 0x00, sizeof(struct file_mgr));
    g_fio_zero = TRUE;

    result_t r;

    r = mem_pool_create_ts(mem_heap(), &g_fio.diskfile_alloc,
                           sizeof(struct file_header) + sizeof(struct disk_file), 32, 0);
    if (IS_FAIL(r))   {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

    r = mem_pool_create_ts(mem_heap(), &g_fio.memfile_alloc,
                           sizeof(struct file_header) + sizeof(struct mem_file), 32, 0);
    if (IS_FAIL(r))   {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

    r = arr_create(mem_heap(), &g_fio.vdirs, sizeof(struct vdir), 5, 5, 0);
    if (IS_FAIL(r))     {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

    r = arr_create(mem_heap(), &g_fio.paks, sizeof(struct pak_file*), 5, 5, 0);
    if (IS_FAIL(r))     {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }

#if defined(_FILEMON_)
    r = hashtable_open_create(mem_heap(), &g_fio.mon_table, MON_ITEM_SIZE, MON_ITEM_SIZE, 0);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, r);
        return r;
    }
#endif

    g_fio.init = TRUE;
    return RET_OK;
}

void fio_releasemgr()
{
    if (!g_fio_zero)
        return;

#if defined(_FILEMON_)
    /* search for remaining registered monitor items and delete them */
    uint cnt = 0;
    for (uint i = 0; i < g_fio.mon_table.slots_cnt; i++) {
        struct hashtable_item* item = &g_fio.mon_table.slots[i];
        if (item->hash != 0)    {
            struct mon_item* mitem = (struct mon_item*)item->value;
            FREE(mitem);
            cnt ++;
        }
    }
    #if defined(_DEBUG_)
    if (cnt > 0)
        log_printf(LOG_INFO, "file-mgr: destroyed %d file monitors", cnt);
    #endif
#endif

    /* */
    fio_clearvdirs();
    g_fio.init = FALSE;

#if defined(_FILEMON_)
    if (g_fio.watcher != NULL)
        efsw_destroy(g_fio.watcher);
#endif

    hashtable_open_destroy(&g_fio.mon_table);
    arr_destroy(&g_fio.vdirs);
    arr_destroy(&g_fio.paks);
    mem_pool_destroy_ts(&g_fio.memfile_alloc);
    mem_pool_destroy_ts(&g_fio.diskfile_alloc);
    memset(&g_fio, 0x00, sizeof(struct file_mgr));
}

bool_t fio_addvdir(const char* directory, bool_t monitor)
{
    ASSERT(directory);
    ASSERT(directory[0] != 0);
    ASSERT(g_fio.init);

    char dir[DH_PATH_MAX];
    path_norm(dir, directory);

    if (!util_pathisdir(dir)) {
        err_printf(__FILE__, __LINE__, "directory '%s' is not valid", dir);
        return FALSE;
    }

    struct vdir* vd = (struct vdir*)arr_add(&g_fio.vdirs);
    if (vd == NULL)
        return FALSE;

    memset(vd, 0x00, sizeof(struct vdir));
    vd->monitor = monitor;
    path_norm(vd->path, dir);

#if defined(_FILEMON_)
    if (monitor)
        return fio_vdir_initmon(vd);
#else
    if (monitor)
        log_print(LOG_WARNING, "File monitoring not implemented to this build");
#endif
    return TRUE;
}

void fio_clearvdirs()
{
#if defined(_FILEMON_)
    for (uint i = 0; i < g_fio.vdirs.item_cnt; i++)  {
        struct vdir* vd = &((struct vdir*)g_fio.vdirs.buffer)[i];
        if (vd->monitor)
            fio_vdir_releasemon(vd);
    }
#endif
    arr_clear(&g_fio.vdirs);
}

void fio_addpak(struct pak_file* pak)
{
    ASSERT(pak);
    ASSERT(pak_isopen(pak));

    struct pak_file** ppaks = (struct pak_file**)arr_add(&g_fio.paks);
    ASSERT(ppaks);
    *ppaks = pak;
}

void fio_clearpaks()
{
    arr_clear(&g_fio.paks);
}

file_t fio_createmem(struct allocator* alloc, const char* name, uint mem_id)
{
    uint8* file_buf = (uint8*)mem_pool_alloc_ts(&g_fio.memfile_alloc);

    if (file_buf == NULL)
        return NULL;
    memset(file_buf, 0x00, g_fio.memfile_alloc.p.item_sz);

    struct file_header* header = (struct file_header*)file_buf;
    struct mem_file* f = (struct mem_file*)(file_buf + sizeof(struct file_header));

    /* header */
    header->type = FILE_TYPE_MEM;
    strcpy(header->path, name);
    header->size = 0;
    header->mode = FILE_MODE_WRITE;
    header->write_fn = fio_writemem;

    /* data */
    f->buffer = (uint8*)A_ALLOC(alloc, MEM_BLOCK_SIZE, mem_id);
    if (f->buffer == NULL)  {
        mem_pool_free_ts(&g_fio.memfile_alloc, file_buf);
        return NULL;
    }
    f->alloc = alloc;
    f->max_size = MEM_BLOCK_SIZE;
    f->offset = 0;
    f->mem_id = mem_id;

    return file_buf;
}

file_t fio_openmem(struct allocator* alloc, const char* filepath,
                       bool_t ignore_vfs, uint mem_id)
{
    /* if memory file is requested and we have pak files, first try loading from paks */
    if (!ignore_vfs && !arr_isempty(&g_fio.paks))    {
        struct pak_file** paks = (struct pak_file**)g_fio.paks.buffer;
        uint paks_cnt = g_fio.paks.item_cnt;
        for (uint i = 0; i < paks_cnt; i++)   {
            uint file_id = pak_findfile(paks[i], filepath);
            if (file_id != INVALID_INDEX)
                return pak_getfile(paks[i], alloc, mem_heap(), file_id, mem_id);
        }
    }

    /* continue opening a file from disk and load it into memory */
    uint8* file_buf = (uint8*)mem_pool_alloc_ts(&g_fio.memfile_alloc);
    if (file_buf == NULL)
    	return NULL;
    memset(file_buf, 0x00, g_fio.memfile_alloc.p.item_sz);

    struct file_header* header = (struct file_header*)file_buf;
    struct mem_file* f = (struct mem_file*)(file_buf + sizeof(struct file_header));

    FILE* ff = (!ignore_vfs && !arr_isempty(&g_fio.vdirs)) ? open_resolvepath(filepath) :
               fopen(filepath, "rb");
    if (ff == NULL)     {
        mem_pool_free_ts(&g_fio.memfile_alloc, file_buf);
        return NULL;
    }

    /* header */
    header->type = FILE_TYPE_MEM;
    strcpy(header->path, filepath);
    header->mode = FILE_MODE_READ;

    fseek(ff, 0, SEEK_END);
    header->size = ftell(ff);
    fseek(ff, 0, SEEK_SET);
    header->read_fn = fio_readmem;

    /* data */
    f->buffer = (uint8*)A_ALLOC(alloc, header->size, mem_id);
    if (f->buffer == NULL)  {
        fclose(ff);
        mem_pool_free_ts(&g_fio.memfile_alloc, file_buf);
        return NULL;
    }
    fread(f->buffer, header->size, 1, ff);
    f->alloc = alloc;
    f->max_size = header->size;
    f->offset = 0;
    f->mem_id = mem_id;

    fclose(ff);
    return file_buf;
}

file_t fio_attachmem(struct allocator* alloc, void* buffer,
                         size_t size, const char* name, uint mem_id)
{
    uint8* file_buf = (uint8*)mem_pool_alloc_ts(&g_fio.memfile_alloc);
    if (file_buf == NULL)
        return NULL;
    memset(file_buf, 0x00, g_fio.memfile_alloc.p.item_sz);

    struct file_header* header = (struct file_header*)file_buf;
    struct mem_file* f = (struct mem_file*)(file_buf + sizeof(struct file_header));

    /* header  */
    header->type = FILE_TYPE_MEM;
    strcpy(header->path, name);
    header->mode = FILE_MODE_READ;
    header->size = size;
    header->read_fn = fio_readmem;

    /* data */
    f->alloc = alloc;
    f->mem_id = mem_id;
    f->buffer = (uint8*)buffer;
    f->max_size = size;
    f->offset = 0;

    return file_buf;
}

void* fio_detachmem(file_t f, size_t* outsize, struct allocator** palloc)
{
    struct file_header* header = (struct file_header*)f;
    struct mem_file* fdata = (struct mem_file*)((uint8*)f + sizeof(struct file_header));

    if (fdata->buffer == NULL)
        return NULL;

    void* buffer = fdata->buffer;
    *outsize = header->size;

    if (palloc != NULL)
        *palloc = fdata->alloc;

    memset(fdata, 0x00, sizeof(struct mem_file));
    return buffer;
}

file_t fio_createdisk(const char* filepath)
{
    uint8* file_buf = (uint8*)mem_pool_alloc_ts(&g_fio.diskfile_alloc);

    if (file_buf == NULL)
        return NULL;
    memset(file_buf, 0x00, g_fio.diskfile_alloc.p.item_sz);

    struct file_header* header = (struct file_header*)file_buf;
    struct disk_file* f = (struct disk_file*)(file_buf + sizeof(struct file_header));

    /* header */
    header->type = FILE_TYPE_DSK;
    header->mode = FILE_MODE_WRITE;
    strcpy(header->path, filepath);
    header->size = 0;
    header->write_fn = fio_writedisk;

    /* data */
    f->file = fopen(filepath, "wb");
    if (f->file == NULL)    {
        mem_pool_free_ts(&g_fio.diskfile_alloc, file_buf);
        return NULL;
    }

    return file_buf;
}

file_t fio_opendisk(const char* filepath, bool_t ignore_vfs)
{
    uint8* file_buf = (uint8*)mem_pool_alloc_ts(&g_fio.diskfile_alloc);

    if (file_buf == NULL)
        return NULL;
    memset(file_buf, 0x00, g_fio.diskfile_alloc.p.item_sz);

    struct file_header* header = (struct file_header*)file_buf;
    struct disk_file* f = (struct disk_file*)(file_buf + sizeof(struct file_header));

    /* header */
    header->type = FILE_TYPE_DSK;
    header->mode = FILE_MODE_READ;
    strcpy(header->path, filepath);
    header->read_fn = fio_readdisk;

    /* data */
    f->file = (!ignore_vfs && !arr_isempty(&g_fio.vdirs)) ? open_resolvepath(filepath) :
        fopen(filepath, "rb");
    if (f->file == NULL)    {
        mem_pool_free_ts(&g_fio.diskfile_alloc, file_buf);
        return NULL;
    }

    /* size */
    fseek(f->file, 0, SEEK_END);
    header->size = ftell(f->file);
    fseek(f->file, 0, SEEK_SET);

    return file_buf;
}

FILE* open_resolvepath(const char* filepath)
{
    ASSERT(g_fio.init);
    struct vdir* vds = (struct vdir*)g_fio.vdirs.buffer;
    char testpath[DH_PATH_MAX];
    FILE* f = NULL;

    uint item_cnt = g_fio.vdirs.item_cnt;
    for (uint i = 0; i < item_cnt; i++)   {
        struct vdir* vd = &vds[i];
        path_join(testpath, vd->path, filepath, NULL);
        f = fopen(testpath, "rb");
        if (f != NULL)
            return f;
    }

    return NULL;
}

void fio_close(file_t f)
{
    ASSERT(f != NULL);

    struct file_header* header = (struct file_header*)f;

    if (header->type == FILE_TYPE_MEM)   {
        struct mem_file* fdata = (struct mem_file*)((uint8*)f + sizeof(struct file_header));
        if (fdata->buffer != NULL)  {
            A_FREE(fdata->alloc, fdata->buffer);
        }
        mem_pool_free_ts(&g_fio.memfile_alloc, f);
        memset(f, 0x00, g_fio.memfile_alloc.p.item_sz);
    }    else if (header->type == FILE_TYPE_DSK)    {
        struct disk_file* fdata = (struct disk_file*)((uint8*)f + sizeof(struct file_header));
        if (fdata->file != NULL)    {
            fclose(fdata->file);
        }
        mem_pool_free_ts(&g_fio.diskfile_alloc, f);
        memset(f, 0x00, g_fio.diskfile_alloc.p.item_sz);
    }
}


void fio_seek(file_t f, enum seek_mode seek, int offset)
{
    ASSERT(f != NULL);

    struct file_header* header = (struct file_header*)f;

    if (header->type == FILE_TYPE_MEM)   {
        struct mem_file* fdata = (struct mem_file*)((uint8*)f + sizeof(struct file_header));
        switch (seek)   {
            case SEEK_MODE_CUR:
                fdata->offset += offset;
                break;
            case SEEK_MODE_START:
                fdata->offset = offset;
                break;
            case SEEK_MODE_END:
                ASSERT(offset > 0);
                fdata->offset = header->size - offset;
                break;
        }
        fdata->offset = clampsz(fdata->offset, 0, header->size);
    }    else if (header->type == FILE_TYPE_DSK)    {
        struct disk_file* fdata = (struct disk_file*)((uint8*)f + sizeof(struct file_header));
        int seek_std;
        switch (seek)   {
            case SEEK_MODE_CUR:     seek_std = SEEK_CUR;    break;
            case SEEK_MODE_START:   seek_std = SEEK_SET;    break;
            case SEEK_MODE_END:     seek_std = SEEK_END;    break;
            default:				seek_std = SEEK_SET;	break;
        }
        fseek(fdata->file, offset, seek_std);
    }
}

size_t fio_read(file_t f, void* buffer, size_t item_size, size_t items_cnt)
{
    ASSERT(f != NULL);
    struct file_header* header = (struct file_header*)f;
    return header->read_fn(f, buffer, item_size, items_cnt);
}

size_t fio_write(file_t f, const void* buffer, size_t item_size, size_t items_cnt)
{
    ASSERT(f != NULL);
    struct file_header* header = (struct file_header*)f;
    return header->write_fn(f, buffer, item_size, items_cnt);
}

size_t fio_readdisk(file_t f, void* buffer, size_t item_size, size_t items_cnt)
{
    struct disk_file* fdata = (struct disk_file*)((uint8*)f + sizeof(struct file_header));
    return fread(buffer, item_size, items_cnt, fdata->file);
}

size_t fio_writedisk(file_t f, const void* buffer, size_t item_size, size_t items_cnt)
{
    struct disk_file* fdata = (struct disk_file*)((uint8*)f + sizeof(struct file_header));
    return fwrite(buffer, item_size, items_cnt, fdata->file);
}

size_t fio_readmem(file_t f, void* buffer, size_t item_size, size_t items_cnt)
{
    struct file_header* header = (struct file_header*)f;
    struct mem_file* fdata = (struct mem_file*)((uint8*)f + sizeof(struct file_header));
    size_t read_sz = item_size * items_cnt;
    if ((read_sz + fdata->offset) > header->size)   {
        read_sz = header->size - fdata->offset;
        read_sz -= (read_sz % item_size);
    }
    if (read_sz != 0)   {
        memcpy(buffer, fdata->buffer + fdata->offset, read_sz);
        fdata->offset += read_sz;
    }
    return (read_sz/item_size);
}

size_t fio_writemem(file_t f, const void* buffer, size_t item_size, size_t items_cnt)
{
    struct file_header* header = (struct file_header*)f;
    struct mem_file* fdata = (struct mem_file*)((uint8*)f + sizeof(struct file_header));

    size_t write_sz = item_size * items_cnt;
    size_t offset = fdata->offset;

    /* grow buffer if necessary */
    if ((write_sz + offset) > fdata->max_size)   {
        size_t grow_sz = write_sz + offset - fdata->max_size;
        size_t expand_sz = ((grow_sz/MEM_BLOCK_SIZE) + 1)*MEM_BLOCK_SIZE;

        void* tmp = A_ALLOC(fdata->alloc, expand_sz + fdata->max_size, fdata->mem_id);
        if (tmp == NULL)    return 0;
        memcpy(tmp, fdata->buffer, header->size);
        A_FREE(fdata->alloc, fdata->buffer);
        fdata->buffer = (uint8*)tmp;

        fdata->max_size += expand_sz;
    }

    if ((offset + write_sz) > header->size)  {
        header->size = offset + write_sz;
    }

    memcpy(fdata->buffer + offset, buffer, write_sz);
    fdata->offset += write_sz;
    return items_cnt;
}

size_t fio_getsize(file_t f)
{
    struct file_header* header = (struct file_header*)f;
    return header->size;
}

size_t fio_getpos(file_t f)
{
    struct file_header* header = (struct file_header*)f;
    if (header->type == FILE_TYPE_MEM)   {
        struct mem_file* fdata = (struct mem_file*)((uint8*)f + sizeof(struct file_header));
        return fdata->offset;
    }    else if (header->type == FILE_TYPE_DSK)    {
        struct disk_file* fdata = (struct disk_file*)((uint8*)f + sizeof(struct file_header));
        return ftell(fdata->file);
    }
    return 0;
}

const char* fio_getpath(file_t f)
{
    struct file_header* header = (struct file_header*)f;
    return header->path;
}

bool_t fio_isopen(file_t f)
{
    struct file_header* header = (struct file_header*)f;
    if (header->type == FILE_TYPE_MEM)   {
        struct mem_file* fdata = (struct mem_file*)((uint8*)f + sizeof(struct file_header));
        return (fdata->buffer != NULL);
    }    else if (header->type == FILE_TYPE_DSK)    {
        struct disk_file* fdata = (struct disk_file*)((uint8*)f + sizeof(struct file_header));
        return (fdata->file != NULL);
    }
    return FALSE;
}

enum file_type fio_gettype(file_t f)
{
    struct file_header* header = (struct file_header*)f;
    return header->type;
}

enum file_mode fio_getmode(file_t f)
{
    struct file_header* header = (struct file_header*)f;
    return header->mode;
}

/*************************************************************************************************
 * file change monitoring routines
 */
#if defined(_FILEMON_)
bool_t fio_find_file(const struct array* files, const char* filepath);

void fio_mon_callback(efsw_watcher watcher,
    efsw_watchid watchid, const char* dir, const char* filename,
    enum efsw_action action, const char* old_filename, void* param)
{
    if (action == EFSW_MODIFIED || action == EFSW_MOVED)    {
        struct vdir* vd = (struct vdir*)param;

        mt_mutex_lock(&vd->mtx);
        struct array* arr = (struct array*)vd->backbuff;
        char filepath_unix[DH_PATH_MAX];
        path_tounix(filepath_unix, filename);

        if (!fio_find_file(arr, filepath_unix))   {
            char* filepath = (char*)arr_add(arr);
            ASSERT(filepath);
            strcpy(filepath, filepath_unix);
        }

        mt_mutex_unlock(&vd->mtx);
    }
}

bool_t fio_vdir_initmon(struct vdir* vd)
{
    if (g_fio.watcher == NULL) {
        g_fio.watcher = efsw_create(FALSE);
        if (g_fio.watcher == NULL) {
            log_print(LOG_WARNING, "file-mgr: could not init file monitoring");
            return FALSE;
        }
    }

    mt_mutex_init(&vd->mtx);

    arr_create(mem_heap(), &vd->backarr, DH_PATH_MAX, 10, 10, 0);
    arr_create(mem_heap(), &vd->frontarr, DH_PATH_MAX, 10, 10, 0);
    vd->frontbuff = &vd->frontarr;
    vd->backbuff = &vd->backarr;

    vd->watchid = efsw_addwatch(g_fio.watcher, vd->path, fio_mon_callback, TRUE, vd);
    if (vd->watchid == -1)  {
        log_printf(LOG_WARNING, "file-mgr: %s", efsw_getlasterror());
        return FALSE;
    }

    efsw_watch(g_fio.watcher);
    return TRUE;
}

void fio_vdir_releasemon(struct vdir* vd)
{
    ASSERT(g_fio.watcher);
    efsw_removewatch_byid(g_fio.watcher, vd->watchid);

    mt_mutex_release(&vd->mtx);

    arr_destroy(&vd->backarr);
    arr_destroy(&vd->frontarr);

    vd->frontbuff = NULL;
    vd->backbuff = NULL;
}


bool_t fio_find_file(const struct array* files, const char* filepath)
{
    for (uint i = 0; i < files->item_cnt; i++)    {
        const char* f = ((char*)files->buffer) + i*DH_PATH_MAX;
        if (str_isequal(f, filepath))
            return TRUE;
    }
    return FALSE;
}

void fio_mon_update()
{
    /* move through vdirs and look for changes within their file-system */
    for (uint i = 0; i < g_fio.vdirs.item_cnt; i++)  {
        struct vdir* vd = &((struct vdir*)g_fio.vdirs.buffer)[i];
        if (!vd->monitor)
            continue;

        /* swap back_arr with front_arr
         * use mutex to ensure that we don't swap back/front in the middle of adding */
        mt_mutex_lock(&vd->mtx);
        swapptr((void**)&vd->backbuff, (void**)&vd->frontbuff);
        mt_mutex_unlock(&vd->mtx);

        /* check with registered file items and see we have a match */
        struct array* arr = (struct array*)vd->frontbuff;
        for (uint c = 0; c < arr->item_cnt; c++)  {
            const char* filepath = ((char*)arr->buffer) + c*DH_PATH_MAX;

            struct hashtable_item* item = hashtable_open_find(&g_fio.mon_table,
                hash_str(filepath));
            if (item != NULL)   {
                struct mon_item* mitem = (struct mon_item*)item->value;
                mitem->fn(filepath, mitem->hdl, mitem->param1, mitem->param2);
            }
        }

        arr_clear(arr);
    }
}


void fio_mon_reg(const char* filepath, pfn_fio_modify fn, reshandle_t hdl,
    uptr_t param1, uptr_t param2)
{
    struct mon_item* mitem = (struct mon_item*)ALLOC(sizeof(struct mon_item), 0);
    ASSERT(mitem);
    mitem->fn = fn;
    mitem->hdl = hdl;
    mitem->param1 = param1;
    mitem->param2 = param2;

    hashtable_open_add(&g_fio.mon_table, hash_str(filepath), (uint64)((uptr_t)mitem));
}

void fio_mon_unreg(const char* filepath)
{
    struct hashtable_item* item = hashtable_open_find(&g_fio.mon_table, hash_str(filepath));
    if (item != NULL)   {
        hashtable_open_remove(&g_fio.mon_table, item);
        struct mon_item* mitem = (struct mon_item*)item->value;
        FREE(mitem);
    }
}
#else
void fio_mon_reg(const char* filepath, pfn_fio_modify fn, reshandle_t hdl,
                 uptr_t param1, uptr_t param2)
{
    ASSERT(0);  /* Not built with _FILEMON_ preprocessor ! */
}

void fio_mon_unreg(const char* filepath)
{
    ASSERT(0);  /* Not built with _FILEMON_ preprocessor ! */
}

void fio_mon_update()
{
    ASSERT(0);  /* Not built with _FILEMON_ preprocessor ! */
}
#endif