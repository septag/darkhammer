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

#include "dheng/res-mgr.h"

#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/stack.h"
#include "dhcore/pool-alloc.h"
#include "dhcore/hash-table.h"
#include "dhcore/file-io.h"
#include "dhcore/hash.h"
#include "dhcore/mt.h"
#include "dhcore/linked-list.h"
#include "dhcore/task-mgr.h"

#include "share/mem-ids.h"

#include "dheng/engine.h"
#include "dheng/gfx-device.h"
#include "dheng/gfx-texture.h"
#include "dheng/gfx-model.h"
#include "dheng/script.h"
#include "dheng/anim.h"
#include "dheng/components/cmp-model.h"
#include "dheng/components/cmp-rbody.h"
#include "dheng/components/cmp-anim.h"
#include "dheng/components/cmp-animchar.h"
#include "dheng/phx-prefab.h"

/*************************************************************************************************
 * defines
 */
#define GET_INDEX(hdl)        ((hdl)>>32)
#define GET_ID(hdl)           ((hdl)&0xffffffff)
#define MAKE_HANDLE(idx, id)  ((((uint64)(idx))<<32) | (((uint64)(id))&0xffffffff))
#define DICT_BLOCK_SIZE 4096

/*************************************************************************************************
 * types
 */
typedef void (*pfn_unload_res)(void* res);

struct rs_resource
{
    reshandle_t hdl;
    uint ref_cnt;
    void* ptr;
    char filepath[128];
    pfn_unload_res unload_func;
    struct stack node;
};

struct rs_freeslot_item
{
    uint idx;
    struct stack node;
};

enum rs_resource_type
{
    RS_RESOURCE_UNKNOWN = 0,
    RS_RESOURCE_TEXTURE,
    RS_RESOURCE_MODEL,
    RS_RESOURCE_PHXPREFAB,
    RS_RESOURCE_ANIMREEL,
    RS_RESOURCE_ANIMCTRL,
    RS_RESOURCE_SCRIPT
};

struct rs_load_data
{
    char filepath[128];
    enum rs_resource_type type;
    reshandle_t hdl;
    int reload;
    union   {
        struct {
            uint first_mipidx;
            int srgb;
        } tex;
    }   params;
    struct linked_list lnode;
};

struct rs_load_job_params
{
    uint cnt;
    struct rs_load_data** load_items;
};

struct rs_load_job_result
{
    void** ptrs;    /* loaded pointer objects (count = job_params->cnt) */
};

struct rs_unload_item
{
    reshandle_t hdl;
    struct linked_list lnode;
};

struct rs_mgr
{
    int init;
    uint flags;
    struct array ress; /* item = struct rs_resource */
    struct stack* freeslots; /* item = struct rs_freeslot_item */
    struct pool_alloc freeslot_pool;
    struct hashtable_chained dict;
    struct pool_alloc dict_itempool;
    struct allocator dict_itemalloc;
    struct allocator* alloc;    /* data allocator */

    struct linked_list* load_list; /* data: rs_load_data */
    struct pool_alloc load_data_pool;   /* item: rs_load_data */
    gfx_texture blank_tex;  /* blank texture for multi-threaded loading */
    uint load_threads_max;
    uint load_jobid;
    struct rs_load_job_params job_params;
    struct rs_load_job_result job_result;
    uint job_unload_cnt;  /* count for job_unloads */
    struct linked_list* unload_items; /* data: rs_unload_item, working jobs that needs to be unloaded */
};

/*************************************************************************************************
 * globals
 */
static struct rs_mgr g_rs;

/*************************************************************************************************
 * forward declarations
 */
reshandle_t rs_add_resource(const char* filepath, void* ptr, reshandle_t override_hdl,
                         pfn_unload_res unload_funcs);
reshandle_t rs_add_todb(const char* filepath, void* ptr, pfn_unload_res unload_funcs);
void rs_remove_fromdb(reshandle_t hdl);

void rs_texture_unload(void* tex);
void rs_model_unload(void* model);
void rs_animreel_unload(void* anim);
void rs_animctrl_unload(void* anim);
void rs_phxprefab_unload(void* prefab);
void rs_script_unload(void* script);

/* callback for reloading textures */
void rs_texture_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2);
void rs_animreel_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2);
void rs_model_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2);
void rs_phxprefab_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2);
void rs_script_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2);
void rs_animctrl_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2);

reshandle_t rs_animreel_queueload(const char* reel_filepath, reshandle_t override_hdl);
reshandle_t rs_texture_queueload(const char* tex_filepath, uint first_mipidx, int srgb,
                                 reshandle_t override_hdl);
reshandle_t rs_animctrl_queueload(const char* ctrl_filepath, reshandle_t override_hdl);
reshandle_t rs_model_queueload(const char* model_filepath, reshandle_t override_hdl);
reshandle_t rs_phxprefab_queueload(const char* phx_filepath, reshandle_t override_hdl);
reshandle_t rs_script_queueload(const char* lua_filepath, reshandle_t override_hdl);
int rs_search_in_unloads(reshandle_t hdl);

/*************************************************************************************************
 * Inlines
 */
INLINE struct rs_resource* rs_resource_get(reshandle_t hdl) {
    uint idx = GET_INDEX(hdl);
    ASSERT(idx < (uint)g_rs.ress.item_cnt);

    struct rs_resource* r = &((struct rs_resource*)g_rs.ress.buffer)[idx];
    ASSERT(r->hdl != INVALID_HANDLE);
    ASSERT(GET_ID(r->hdl) == GET_ID(hdl));
    return r;
}

/* if we have an override handle, check in existing queue and see if it's already exists */
INLINE struct linked_list* rs_loadqueue_search(reshandle_t hdl)
{
    struct linked_list* lnode = g_rs.load_list;
    while (lnode != nullptr)   {
        struct rs_load_data* ldata = (struct rs_load_data*)lnode->data;
        if (ldata->hdl == hdl)
            return lnode;
        lnode = lnode->next;
    }
    return nullptr;
}

INLINE void rs_register_hotload(const struct rs_load_data* ldata)
{
    switch (ldata->type)   {
    case RS_RESOURCE_TEXTURE:
        fio_mon_reg(ldata->filepath, rs_texture_reload, ldata->hdl, ldata->params.tex.first_mipidx,
            ldata->params.tex.srgb);
        break;
    case RS_RESOURCE_MODEL:
        fio_mon_reg(ldata->filepath, rs_model_reload, ldata->hdl, 0, 0);
        break;
    case RS_RESOURCE_ANIMREEL:
        fio_mon_reg(ldata->filepath, rs_animreel_reload, ldata->hdl, 0, 0);
        break;
    case RS_RESOURCE_PHXPREFAB:
        fio_mon_reg(ldata->filepath, rs_phxprefab_reload, ldata->hdl, 0, 0);
        break;
    case RS_RESOURCE_SCRIPT:
        fio_mon_reg(ldata->filepath, rs_script_reload, ldata->hdl, 0, 0);
        break;
    case RS_RESOURCE_ANIMCTRL:
        fio_mon_reg(ldata->filepath, rs_animctrl_reload, ldata->hdl, 0, 0);
        break;
    default:
        ASSERT(0);
    }
}

INLINE void rs_resource_manualreload(const struct rs_load_data* ldata)
{
    switch (ldata->type)    {
    case RS_RESOURCE_TEXTURE:
        break;
    case RS_RESOURCE_MODEL:
        cmp_model_reload(ldata->filepath, ldata->hdl, TRUE);
        break;
    case RS_RESOURCE_ANIMREEL:
        cmp_anim_reload(ldata->filepath, ldata->hdl, TRUE);
        cmp_animchar_reelchanged(ldata->hdl);
        break;
    case RS_RESOURCE_PHXPREFAB:
        cmp_rbody_reload(ldata->filepath, ldata->hdl, TRUE);
        break;
    case RS_RESOURCE_SCRIPT:
        sct_reload(ldata->filepath, ldata->hdl, TRUE);
        break;
    case RS_RESOURCE_ANIMCTRL:
        cmp_animchar_reload(ldata->filepath, ldata->hdl, TRUE);
        break;
    default:
        ASSERT(0);
    }
}

/*************************************************************************************************/
void rs_zero()
{
    memset(&g_rs, 0x00, sizeof(g_rs));
}

result_t rs_initmgr(uint flags, uint load_thread_cnt)
{
    result_t r;

    memset(&g_rs, 0x00, sizeof(g_rs));
    log_print(LOG_TEXT, "init res-mgr ...");

    r = arr_create(mem_heap(), &g_rs.ress, sizeof(struct rs_resource), 128, 256, MID_RES);
    r |= mem_pool_create(mem_heap(), &g_rs.freeslot_pool, sizeof(struct rs_freeslot_item),
        100, MID_RES);
    r |= mem_pool_create(mem_heap(), &g_rs.dict_itempool, sizeof(struct hashtable_item_chained),
        DICT_BLOCK_SIZE, MID_RES);
    mem_pool_bindalloc(&g_rs.dict_itempool, &g_rs.dict_itemalloc);
    r |= hashtable_chained_create(mem_heap(), &g_rs.dict_itemalloc, &g_rs.dict, 65521, MID_RES);
    r |= mem_pool_create(mem_heap(), &g_rs.load_data_pool, sizeof(struct rs_load_data), 256, MID_RES);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return r;
    }

    /* get maximum number of threads available for threaded loading */
    if (BIT_CHECK(flags, RS_FLAG_PREPARE_BGLOAD))   {
        g_rs.load_threads_max = maxui(load_thread_cnt, 1);
        g_rs.job_params.load_items = (struct rs_load_data**)ALLOC(
            sizeof(struct rs_load_data*)*load_thread_cnt, MID_RES);
        g_rs.job_result.ptrs = (void**)ALLOC(sizeof(void*)*load_thread_cnt, MID_RES);
        if (g_rs.job_params.load_items == nullptr || g_rs.job_result.ptrs == nullptr)    {
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            return RET_OUTOFMEMORY;
        }
    }

    g_rs.alloc = mem_heap();    /* default allocator is heap */
    g_rs.flags = flags;
    g_rs.init = TRUE;
    return RET_OK;
}

result_t rs_init_resources()
{
    /* blank texture */
    g_rs.blank_tex = gfx_texture_loaddds("textures/white1x1.dds", 0, FALSE, 0);
    if (g_rs.blank_tex == nullptr) {
        err_print(__FILE__, __LINE__, "res-mgr init resources failed: could not load blank texture");
        return RET_FAIL;
    }

    return RET_OK;
}

void rs_releasemgr()
{
    struct linked_list* unload_item = g_rs.unload_items;
    while (unload_item != nullptr) {
        struct linked_list* nitem = unload_item->next;
        FREE(unload_item->data);
        unload_item = nitem;
    }

    hashtable_chained_destroy(&g_rs.dict);
    mem_pool_destroy(&g_rs.dict_itempool);
    mem_pool_destroy(&g_rs.freeslot_pool);
    arr_destroy(&g_rs.ress);
    mem_pool_destroy(&g_rs.load_data_pool);

    if (g_rs.job_params.load_items != nullptr)
        FREE(g_rs.job_params.load_items);
    if (g_rs.job_result.ptrs != nullptr)
        FREE(g_rs.job_result.ptrs);

    log_print(LOG_TEXT, "res-mgr released.");
}

void rs_release_resources()
{
    gfx_delayed_release();

    /* wait for load task to finish */
    if (g_rs.load_jobid != 0)   {
        tsk_wait(g_rs.load_jobid);
        tsk_destroy(g_rs.load_jobid);
    }

    /* unload everything in queued loads */
    for (uint i = 0; i < g_rs.job_params.cnt; i++)    {
        reshandle_t hdl = g_rs.job_params.load_items[i]->hdl;
        void* ptr = g_rs.job_result.ptrs[i];
        if (ptr != nullptr)    {
            struct rs_resource* r = rs_resource_get(hdl);
            r->unload_func(ptr);
        }
    }

    if (g_rs.blank_tex != nullptr) {
        gfx_destroy_texture(g_rs.blank_tex);
        g_rs.blank_tex = nullptr;
    }
}

/* Runs in task threads */
void rs_threaded_load_fn(void* params, void* result, uint thread_id, uint job_id, int worker_idx)
{
    struct rs_load_job_params* lparams = (struct rs_load_job_params*)params;
    struct rs_load_job_result* lresult = (struct rs_load_job_result*)result;

    if (worker_idx >= (int)lparams->cnt)
        return;

    util_sleep(100);

    void* ptr = nullptr;
    struct rs_load_data* ldata = lparams->load_items[worker_idx];
    switch (ldata->type)    {
    case RS_RESOURCE_TEXTURE:
        ptr = gfx_texture_loaddds(ldata->filepath, ldata->params.tex.first_mipidx,
            ldata->params.tex.srgb, thread_id);
        if (ptr != nullptr)
            log_printf(LOG_LOAD, "(texture) \"%s\" - id: %d", ldata->filepath, GET_ID(ldata->hdl));
        break;
    case RS_RESOURCE_MODEL:
        ptr = gfx_model_load(g_rs.alloc, ldata->filepath, thread_id);
        if (ptr != nullptr)
            log_printf(LOG_LOAD, "(model) \"%s\" - id: %d", ldata->filepath, GET_ID(ldata->hdl));
        break;

    case RS_RESOURCE_ANIMREEL:
        ptr = anim_reel_loadf(ldata->filepath, g_rs.alloc, thread_id);
        if (ptr != nullptr)
            log_printf(LOG_LOAD, "(anim-reel) \"%s\" - id:%d", ldata->filepath, GET_ID(ldata->hdl));
        break;

    case RS_RESOURCE_PHXPREFAB:
        ptr = phx_prefab_load(ldata->filepath, g_rs.alloc, thread_id);
        if (ptr != nullptr)
            log_printf(LOG_LOAD, "(physics) \"%s\" - id: %d", ldata->filepath, GET_ID(ldata->hdl));
        break;

    case RS_RESOURCE_SCRIPT:
        ptr = sct_load(ldata->filepath, thread_id);
        if (ptr != nullptr)
            log_printf(LOG_LOAD, "(script) \"%s\" - id: %d", ldata->filepath, GET_ID(ldata->hdl));
        break;

    case RS_RESOURCE_ANIMCTRL:
        ptr = anim_charctrl_loadj(ldata->filepath, g_rs.alloc, thread_id);
        if (ptr != nullptr)
            log_printf(LOG_LOAD, "(anim-ctrl) \"%s\" - id: %d", ldata->filepath, GET_ID(ldata->hdl));
        break;

    default:
        ASSERT(0);
    }

    if (ptr == nullptr)   {
        log_printf(LOG_WARNING, "res-mgr: loading resource '%s' failed", ldata->filepath);
        err_clear();
    }

    lresult->ptrs[worker_idx] = ptr;
}

/* Runs in main thread, syncs resources */
void rs_update()
{
    uint job_id = g_rs.load_jobid;
    if (job_id != 0)    {
        if (!tsk_check_finished(job_id))    {
            gfx_delayed_createobjects();
            return;
        }   else    {
            gfx_delayed_finalizeobjects();
        }
    }

    struct rs_load_job_params* params = &g_rs.job_params;
    struct rs_load_job_result* result = &g_rs.job_result;

    /* if we already have a job_id, it means that the job is finished
     * update the database and destroy the task */
    if (job_id != 0)    {
        struct rs_load_job_params* params = (struct rs_load_job_params*)tsk_get_params(job_id);
        struct rs_load_job_result* result = (struct rs_load_job_result*)tsk_get_result(job_id);

        for (uint i = 0; i < params->cnt; i++)    {
            struct rs_load_data* ldata = params->load_items[i];
            reshandle_t hdl = ldata->hdl;

            /* check the handle in unload list and unload immediately */
            int must_unload = rs_search_in_unloads(hdl);

            struct rs_resource* r = rs_resource_get(hdl);
            if (result->ptrs[i] != nullptr)    {
                r->ptr = result->ptrs[i];

                if (!must_unload)    {
                    /* register hot-loading */
                    if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING) && !ldata->reload)
                        rs_register_hotload(ldata);

                    /* apply reload funcs */
                    rs_resource_manualreload(ldata);
                }   else    {
                    rs_remove_fromdb(hdl);
                }
            }   else if (must_unload)   {
                rs_remove_fromdb(hdl);
            }

            mem_pool_free(&g_rs.load_data_pool, params->load_items[i]);
        }

        /* cleanup */
        tsk_destroy(job_id);
    }

    /* fill new params and dispatch the job */
    struct linked_list* lnode = g_rs.load_list;
    uint qcnt = 0;
    while (qcnt < g_rs.load_threads_max && lnode != nullptr)    {
        params->load_items[qcnt++] = (struct rs_load_data*)lnode->data;
        list_remove(&g_rs.load_list, lnode);
        lnode = g_rs.load_list;
    }
    params->cnt = qcnt;

    /* dispatch to first thread only (exclusive mode) */
    if (qcnt > 0)   {
        int thread_cnt = (int)g_rs.load_threads_max;
        struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
        int* thread_idxs = (int*)A_ALLOC(tmp_alloc, sizeof(uint)*thread_cnt, MID_RES);
        ASSERT(thread_idxs);
        for (int i = 0; i < thread_cnt; i++)
            thread_idxs[i] = i;
        g_rs.load_jobid = tsk_dispatch_exclusive(rs_threaded_load_fn, thread_idxs, thread_cnt,
            params, result);
        A_FREE(tmp_alloc, thread_idxs);
    }   else    {
        g_rs.load_jobid = 0;
    }
}

int rs_search_in_unloads(reshandle_t hdl)
{
    struct linked_list* unode = g_rs.unload_items;
    while (unode != nullptr)   {
        struct linked_list* n_unode = unode->next;
        struct rs_unload_item* uitem = (struct rs_unload_item*)unode->data;
        if (uitem->hdl == hdl)  {
            list_remove(&g_rs.unload_items, unode);
            FREE(uitem);
            return TRUE;
        }
        unode = n_unode;
    }
    return FALSE;
}

void rs_texture_reload(const char* filepath, uint64 hdl, uptr_t param1, uptr_t param2)
{
    if (!g_rs.init)
        return;

    /* reload texture */
    rs_load_texture(filepath, (uint)param1, (uint)param2, RS_LOAD_REFRESH);
}

void rs_model_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2)
{
    if (!g_rs.init)
        return;

    cmp_model_reload(filepath, hdl, FALSE);
}

/* apply reload procedure to every component that is using it
 * currently it's raw animation (cmp-anim)
 * character animation controller (cmp-animchar)
 */
void rs_animreel_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2)
{
    cmp_anim_reload(filepath, hdl, FALSE);
    cmp_animchar_reelchanged(hdl);
}

void rs_phxprefab_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2)
{
    cmp_rbody_reload(filepath, hdl, FALSE);
}

void rs_script_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2)
{
    sct_reload(filepath, hdl, FALSE);
}

void rs_animctrl_reload(const char* filepath, reshandle_t hdl, uptr_t param1, uptr_t param2)
{
    cmp_animchar_reload(filepath, hdl, FALSE);
}

reshandle_t rs_load_texture(const char* tex_filepath, uint first_mipidx,
		int srgb, uint flags)
{
    reshandle_t res_hdl = INVALID_HANDLE;
    reshandle_t override_hdl = res_hdl;

    if (!g_rs.init)
        return INVALID_HANDLE;

    struct hashtable_item_chained* item = hashtable_chained_find(&g_rs.dict,
        hash_str(tex_filepath));
    if (item != nullptr)   {
        uint idx = (uint)item->value;
        struct rs_resource* res = (struct rs_resource*)g_rs.ress.buffer + idx;
        res_hdl = res->hdl;
    }

    if (res_hdl != INVALID_HANDLE && BIT_CHECK(flags, RS_LOAD_REFRESH))    {
        /* rs_resource already loaded, but refresh flag is set, so we reload it */
        if (BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))   {
            res_hdl = rs_texture_queueload(tex_filepath, first_mipidx, srgb, res_hdl);
        }   else    {
            override_hdl =  res_hdl;
            res_hdl = INVALID_HANDLE;
        }
    }   else if (res_hdl != INVALID_HANDLE) {
        /* add ref count */
        struct rs_resource* ress = (struct rs_resource*)g_rs.ress.buffer;
        ress[GET_INDEX(res_hdl)].ref_cnt ++;
    }

    /* rs_resource is not loaded before, so we just have to load it for the first time */
    if (res_hdl == INVALID_HANDLE)  {
        if(BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))  {
            res_hdl = rs_texture_queueload(tex_filepath, first_mipidx, srgb, INVALID_HANDLE);
        }   else    {
        	/* determine file extension, then load the texture based on that */
        	char ext[128];
        	path_getfileext(ext, tex_filepath);
        	gfx_texture tex = nullptr;

        	if (str_isequal_nocase(ext, "dds"))
        		tex  = gfx_texture_loaddds(tex_filepath, first_mipidx, srgb, 0);

            if (tex == nullptr) {
                log_printf(LOG_WARNING, "res-mgr: loading rs_resource '%s' failed:"
                    " could not load texture", tex_filepath);
                err_clear();
                if (override_hdl != INVALID_HANDLE)
                    rs_remove_fromdb(override_hdl);
                return INVALID_HANDLE;
            }

            res_hdl = rs_add_resource(tex_filepath, tex, override_hdl, rs_texture_unload);

            /* add to hot-loading files */
            if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING) && !BIT_CHECK(flags, RS_LOAD_REFRESH))   {
                fio_mon_reg(tex_filepath, rs_texture_reload, res_hdl, (uptr_t)first_mipidx,
                    (uptr_t)srgb);
            }

            log_printf(LOG_LOAD, "(texture) \"%s\" - id: %d", tex_filepath, GET_ID(res_hdl));
        }
    }

    return res_hdl;
}

reshandle_t rs_texture_queueload(const char* tex_filepath, uint first_mipidx, int srgb,
                                 reshandle_t override_hdl)
{
    /* if we have an override handle, check in existing queue and see if it's already exists */
    if (override_hdl != INVALID_HANDLE && rs_loadqueue_search(override_hdl) != nullptr)
        return override_hdl;

    reshandle_t hdl = rs_add_resource(tex_filepath, g_rs.blank_tex, override_hdl, rs_texture_unload);
    if (hdl == INVALID_HANDLE)
        return hdl;

    struct rs_load_data* ldata = (struct rs_load_data*)mem_pool_alloc(&g_rs.load_data_pool);
    ASSERT(ldata);
    memset(ldata, 0x00, sizeof(struct rs_load_data));

    str_safecpy(ldata->filepath, sizeof(ldata->filepath), tex_filepath);
    ldata->hdl = hdl;
    ldata->type = RS_RESOURCE_TEXTURE;
    ldata->params.tex.first_mipidx = first_mipidx;
    ldata->params.tex.srgb = srgb;
    ldata->reload = (override_hdl != INVALID_HANDLE);

    /* push to load list */
    list_addlast(&g_rs.load_list, &ldata->lnode, ldata);

    return hdl;
}


void rs_texture_unload(void* tex)
{
    gfx_texture gtex = (gfx_texture)tex;
    if (gtex != g_rs.blank_tex)
        gfx_destroy_texture((gfx_texture)tex);
}

reshandle_t rs_add_resource(const char* filepath, void* ptr, reshandle_t override_hdl,
    pfn_unload_res unload_func)
{
    reshandle_t res_hdl = override_hdl;

    /* if rs_resource handle is overrided, release previous rs_resource and put new one into
     * current rs_resource slot
     */
    if (res_hdl != INVALID_HANDLE)  {
        struct rs_resource* rss = (struct rs_resource*)g_rs.ress.buffer;
        struct rs_resource* rs = &rss[GET_INDEX(res_hdl)];
        if (rs->hdl != INVALID_HANDLE)
            rs->unload_func(rs->ptr);

        rs->ptr = ptr;
        rs->hdl = res_hdl;
        rs->unload_func = unload_func;
        ASSERT(str_isequal(rs->filepath, filepath));
    }   else    {
        /* just add it to rs_resource database */
        res_hdl = rs_add_todb(filepath, ptr, unload_func);
        if (res_hdl == INVALID_HANDLE)
            return INVALID_HANDLE;

        hashtable_chained_add(&g_rs.dict, hash_str(filepath), GET_INDEX(res_hdl));
    }

    return res_hdl;
}

reshandle_t rs_add_todb(const char* filepath, void* ptr, pfn_unload_res unload_func)
{
    static uint global_id = 0;
    global_id ++;

    reshandle_t res_hdl;
    struct rs_resource* rs;

    /* if we have freeslots in free-stack, pop it and re-update it */
    struct stack* sitem = stack_pop(&g_rs.freeslots);
    if (sitem != nullptr)  {
        uptr_t idx = (uptr_t)sitem->data;
        res_hdl = MAKE_HANDLE(idx, global_id);
        rs = &((struct rs_resource*)g_rs.ress.buffer)[idx];
    }   else    {
        res_hdl = MAKE_HANDLE(g_rs.ress.item_cnt, global_id);
        rs = (struct rs_resource*)arr_add(&g_rs.ress);
        ASSERT(rs);
    }

    rs->hdl = res_hdl;
    rs->ptr = ptr;
    rs->ref_cnt = 1;
    rs->unload_func = unload_func;
    ASSERT(strlen(filepath) < 128);
    str_safecpy(rs->filepath, sizeof(rs->filepath), filepath);

    return res_hdl;
}

void rs_unload(reshandle_t hdl)
{
    ASSERT(hdl != INVALID_HANDLE);

    if (!g_rs.init)
        return;

    /* make sure handle is binded properly and is not invalid */
    uptr_t idx = GET_INDEX(hdl);
    ASSERT(idx < g_rs.ress.item_cnt);
    struct rs_resource* rs = &((struct rs_resource*)g_rs.ress.buffer)[idx];
    ASSERT(rs->hdl != INVALID_HANDLE);

    /* decr reference count, and see if have to release it */
    rs->ref_cnt --;
    if (rs->ref_cnt == 0)   {
        /* check in pending threaded loads
         * If exists, it means that there is a loading job queued inside thread that
         * needs to be unloaded after it's done (see rs_update) */
        for (uint i = 0, cnt = g_rs.job_params.cnt; i < cnt; i++) {
            if (g_rs.job_params.load_items[i]->hdl == hdl)  {
                struct rs_unload_item* uitem = (struct rs_unload_item*)ALLOC(
                    sizeof(struct rs_unload_item), MID_RES);
                ASSERT(uitem);
                uitem->hdl = hdl;
                list_add(&g_rs.unload_items, &uitem->lnode, uitem);
                return;
            }
        }

        /* also search in queued loads and remove from list immediately */
        struct linked_list* lnode = rs_loadqueue_search(hdl);
        if (lnode != nullptr)  {
            list_remove(&g_rs.load_list, lnode);
            mem_pool_free(&g_rs.load_data_pool, lnode->data);
        }

        /* remove from hot-loading list */
        if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING))
            fio_mon_unreg(rs->filepath);

        rs_remove_fromdb(hdl);
    }
}

/* removes from resoouce data base, and unloads rs_resource data */
void rs_remove_fromdb(reshandle_t hdl)
{
    /* make sure handle is binded properly and is not invalid */
    uptr_t idx = GET_INDEX(hdl);
    ASSERT(idx < g_rs.ress.item_cnt);
    struct rs_resource* rs = &((struct rs_resource*)g_rs.ress.buffer)[idx];
    ASSERT(rs->hdl != INVALID_HANDLE);

    struct hashtable_item_chained* item = hashtable_chained_find(&g_rs.dict,
        hash_str(rs->filepath));
    ASSERT(item);
    hashtable_chained_remove(&g_rs.dict, item);

    /* add item to free slots */
    stack_push(&g_rs.freeslots, &rs->node, (void*)idx);

    rs->unload_func(rs->ptr);
    rs->hdl = INVALID_HANDLE;
}

reshandle_t rs_load_model(const char* model_filepath, uint flags)
{
    reshandle_t res_hdl = INVALID_HANDLE;
    reshandle_t override_hdl = res_hdl;

    if (!g_rs.init)
        return INVALID_HANDLE;

    struct hashtable_item_chained* item = hashtable_chained_find(&g_rs.dict,
        hash_str(model_filepath));
    if (item != nullptr)   {
        uint idx = (uint)item->value;
        struct rs_resource* res = (struct rs_resource*)g_rs.ress.buffer + idx;
        res_hdl = res->hdl;
    }

    if (res_hdl != INVALID_HANDLE && BIT_CHECK(flags, RS_LOAD_REFRESH))    {
        /* rs_resource already loaded, but refresh flag is set, so we reload it */
        if (BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))   {
            res_hdl = rs_model_queueload(model_filepath, res_hdl);
        }   else    {
            override_hdl =  res_hdl;
            res_hdl = INVALID_HANDLE;
        }
    }   else if (res_hdl != INVALID_HANDLE) {
        /* add ref count */
        struct rs_resource* ress = (struct rs_resource*)g_rs.ress.buffer;
        ress[GET_INDEX(res_hdl)].ref_cnt ++;
    }

    /* rs_resource is not loaded before, so we just have to load it for the first time */
    if (res_hdl == INVALID_HANDLE)  {
        if(BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))  {
            res_hdl = rs_model_queueload(model_filepath, INVALID_HANDLE);
        }   else    {
        	/* determine file extension, then load the texture based on that */
        	char ext[128];
        	path_getfileext(ext, model_filepath);
        	struct gfx_model* model = nullptr;

        	/* model files should be 'h3dm' extension */
        	if (str_isequal_nocase(ext, "h3dm"))
        		model = gfx_model_load(g_rs.alloc, model_filepath, 0);

            if (model == nullptr) {
                log_printf(LOG_WARNING, "res-mgr: loading resource '%s' failed:"
                    " could not load model", model_filepath);
                err_clear();
                if (override_hdl != INVALID_HANDLE)
                    rs_remove_fromdb(override_hdl);
                return INVALID_HANDLE;
            }

            res_hdl = rs_add_resource(model_filepath, model, override_hdl, rs_model_unload);

            /* add to hot-loading files */
            if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING) && !BIT_CHECK(flags, RS_LOAD_REFRESH))
                fio_mon_reg(model_filepath, rs_model_reload, res_hdl, 0, 0);

            log_printf(LOG_LOAD, "(model) \"%s\" - id: %d", model_filepath, GET_ID(res_hdl));
        }
    }

    return res_hdl;
}

reshandle_t rs_model_queueload(const char* model_filepath, reshandle_t override_hdl)
{
    if (override_hdl != INVALID_HANDLE && rs_loadqueue_search(override_hdl) != nullptr)
        return override_hdl;

    reshandle_t hdl = rs_add_resource(model_filepath, nullptr, override_hdl, rs_model_unload);
    if (hdl == INVALID_HANDLE)
        return hdl;

    struct rs_load_data* ldata = (struct rs_load_data*)mem_pool_alloc(&g_rs.load_data_pool);
    ASSERT(ldata);
    memset(ldata, 0x00, sizeof(struct rs_load_data));

    str_safecpy(ldata->filepath, sizeof(ldata->filepath), model_filepath);
    ldata->hdl = hdl;
    ldata->type = RS_RESOURCE_MODEL;
    ldata->reload = (override_hdl != INVALID_HANDLE);

    /* push to load list */
    list_addlast(&g_rs.load_list, &ldata->lnode, ldata);

    return hdl;
}


void rs_model_unload(void* model)
{
    if (model != nullptr)
	    gfx_model_unload((struct gfx_model*)model);
}

reshandle_t rs_load_animreel(const char* reel_filepath, uint flags)
{
    reshandle_t res_hdl = INVALID_HANDLE;
    reshandle_t override_hdl = res_hdl;

    if (!g_rs.init)
        return INVALID_HANDLE;

    struct hashtable_item_chained* item = hashtable_chained_find(&g_rs.dict,
        hash_str(reel_filepath));
    if (item != nullptr)   {
        uint idx = (uint)item->value;
        struct rs_resource* res = (struct rs_resource*)g_rs.ress.buffer + idx;
        res_hdl = res->hdl;
    }

    if (res_hdl != INVALID_HANDLE && BIT_CHECK(flags, RS_LOAD_REFRESH))    {
        /* rs_resource already loaded, but refresh flag is set, so we reload it */
        if (BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))   {
            res_hdl = rs_animreel_queueload(reel_filepath, res_hdl);
        }   else    {
            override_hdl =  res_hdl;
            res_hdl = INVALID_HANDLE;
        }
    }   else if (res_hdl != INVALID_HANDLE) {
        /* add ref count */
        struct rs_resource* ress = (struct rs_resource*)g_rs.ress.buffer;
        ress[GET_INDEX(res_hdl)].ref_cnt ++;
    }

    /* rs_resource is not loaded before, so we just have to load it for the first time */
    if (res_hdl == INVALID_HANDLE)  {
        if(BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))  {
            res_hdl = rs_animreel_queueload(reel_filepath, INVALID_HANDLE);
        }   else    {
            /* determine file extension, then load the texture based on that */
            char ext[128];
            path_getfileext(ext, reel_filepath);
            animReel *reel = nullptr;

            /* model files should be valid extension */
            if (str_isequal_nocase(ext, "h3da"))
                reel = anim_reel_loadf(reel_filepath, g_rs.alloc, 0);

            if (reel == nullptr) {
                log_printf(LOG_WARNING, "res-mgr: loading rs_resource '%s' failed:"
                    " could not load anim-reel", reel_filepath);
                err_clear();
                if (override_hdl != INVALID_HANDLE)
                    rs_remove_fromdb(override_hdl);
                return INVALID_HANDLE;
            }

            res_hdl = rs_add_resource(reel_filepath, reel, override_hdl, rs_animreel_unload);

            /* add to hot-loading files */
            if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING) && !BIT_CHECK(flags, RS_LOAD_REFRESH))
                fio_mon_reg(reel_filepath, rs_animreel_reload, res_hdl, 0, 0);

            log_printf(LOG_LOAD, "(anim-reel) \"%s\" - id: %d", reel_filepath, GET_ID(res_hdl));
        }
    }

    return res_hdl;
}

reshandle_t rs_animreel_queueload(const char* reel_filepath, reshandle_t override_hdl)
{
    if (override_hdl != INVALID_HANDLE && rs_loadqueue_search(override_hdl) != nullptr)
        return override_hdl;

    reshandle_t hdl = rs_add_resource(reel_filepath, nullptr, override_hdl, rs_animreel_unload);
    if (hdl == INVALID_HANDLE)
        return hdl;

    struct rs_load_data* ldata = (struct rs_load_data*)mem_pool_alloc(&g_rs.load_data_pool);
    ASSERT(ldata);
    memset(ldata, 0x00, sizeof(struct rs_load_data));

    str_safecpy(ldata->filepath, sizeof(ldata->filepath), reel_filepath);
    ldata->hdl = hdl;
    ldata->type = RS_RESOURCE_ANIMREEL;
    ldata->reload = (override_hdl != INVALID_HANDLE);

    /* push to load list */
    list_addlast(&g_rs.load_list, &ldata->lnode, ldata);

    return hdl;
}

void rs_animreel_unload(void* anim)
{
    if (anim)
        ((animReel*)anim)->destroy();
}

reshandle_t rs_load_animctrl(const char* ctrl_filepath, uint flags)
{
    reshandle_t res_hdl = INVALID_HANDLE;
    reshandle_t override_hdl = res_hdl;

    if (!g_rs.init)
        return INVALID_HANDLE;

    struct hashtable_item_chained* item = hashtable_chained_find(&g_rs.dict,
        hash_str(ctrl_filepath));
    if (item != nullptr)   {
        uint idx = (uint)item->value;
        struct rs_resource* res = (struct rs_resource*)g_rs.ress.buffer + idx;
        res_hdl = res->hdl;
    }

    if (res_hdl != INVALID_HANDLE && BIT_CHECK(flags, RS_LOAD_REFRESH))    {
        /* rs_resource already loaded, but refresh flag is set, so we reload it */
        if (BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))   {
            res_hdl = rs_animctrl_queueload(ctrl_filepath, res_hdl);
        }   else    {
            override_hdl =  res_hdl;
            res_hdl = INVALID_HANDLE;
        }
    }   else if (res_hdl != INVALID_HANDLE) {
        /* add ref count */
        struct rs_resource* ress = (struct rs_resource*)g_rs.ress.buffer;
        ress[GET_INDEX(res_hdl)].ref_cnt ++;
    }

    /* rs_resource is not loaded before, so we just have to load it for the first time */
    if (res_hdl == INVALID_HANDLE)  {
        if(BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))  {
            res_hdl = rs_animctrl_queueload(ctrl_filepath, INVALID_HANDLE);
        }   else    {
            /* determine file extension, then load the texture based on that */
            char ext[128];
            path_getfileext(ext, ctrl_filepath);
            animCharController *ctrl = nullptr;

            /* model files should be valid extension */
            if (str_isequal_nocase(ext, "json"))
                ctrl = anim_charctrl_loadj(ctrl_filepath, g_rs.alloc, 0);

            if (ctrl == nullptr) {
                log_printf(LOG_WARNING, "res-mgr: loading rs_resource '%s' failed:"
                    " could not load anim-ctrl", ctrl_filepath);
                err_clear();
                if (override_hdl != INVALID_HANDLE)
                    rs_remove_fromdb(override_hdl);
                return INVALID_HANDLE;
            }

            res_hdl = rs_add_resource(ctrl_filepath, ctrl, override_hdl, rs_animctrl_unload);

            /* add to hot-loading files */
            if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING) && !BIT_CHECK(flags, RS_LOAD_REFRESH))
                fio_mon_reg(ctrl_filepath, rs_animctrl_reload, res_hdl, 0, 0);

            log_printf(LOG_LOAD, "(anim-ctrl) \"%s\" - id: %d", ctrl_filepath, GET_ID(res_hdl));
        }
    }

    return res_hdl;
}

reshandle_t rs_animctrl_queueload(const char* ctrl_filepath, reshandle_t override_hdl)
{
    /* if we have an override handle, check in existing queue and see if it's already exists */
    if (override_hdl != INVALID_HANDLE && rs_loadqueue_search(override_hdl) != nullptr)
        return override_hdl;

    reshandle_t hdl = rs_add_resource(ctrl_filepath, nullptr, override_hdl, rs_animctrl_unload);
    if (hdl == INVALID_HANDLE)
        return hdl;

    struct rs_load_data* ldata = (struct rs_load_data*)mem_pool_alloc(&g_rs.load_data_pool);
    ASSERT(ldata);
    memset(ldata, 0x00, sizeof(struct rs_load_data));

    str_safecpy(ldata->filepath, sizeof(ldata->filepath), ctrl_filepath);
    ldata->hdl = hdl;
    ldata->type = RS_RESOURCE_ANIMCTRL;
    ldata->reload = (override_hdl != INVALID_HANDLE);

    /* push to load list */
    list_addlast(&g_rs.load_list, &ldata->lnode, ldata);

    return hdl;
}

void rs_animctrl_unload(void* animctrl)
{
    if (animctrl)
        ((animCharController*)animctrl)->destroy();
}

reshandle_t rs_load_script(const char* lua_filepath, uint flags)
{
    reshandle_t res_hdl = INVALID_HANDLE;
    reshandle_t override_hdl = res_hdl;

    if (!g_rs.init)
        return INVALID_HANDLE;

    struct hashtable_item_chained* item = hashtable_chained_find(&g_rs.dict,
        hash_str(lua_filepath));
    if (item != nullptr)   {
        uint idx = (uint)item->value;
        struct rs_resource* res = (struct rs_resource*)g_rs.ress.buffer + idx;
        res_hdl = res->hdl;
    }

    if (res_hdl != INVALID_HANDLE && BIT_CHECK(flags, RS_LOAD_REFRESH))    {
        /* rs_resource already loaded, but refresh flag is set, so we reload it */
        if (BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))   {
            res_hdl = rs_script_queueload(lua_filepath, res_hdl);
        }   else    {
            override_hdl =  res_hdl;
            res_hdl = INVALID_HANDLE;
        }
    }   else if (res_hdl != INVALID_HANDLE) {
        /* add ref count */
        struct rs_resource* ress = (struct rs_resource*)g_rs.ress.buffer;
        ress[GET_INDEX(res_hdl)].ref_cnt ++;
    }

    /* rs_resource is not loaded before, so we just have to load it for the first time */
    if (res_hdl == INVALID_HANDLE)  {
        if(BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))  {
            res_hdl = rs_script_queueload(lua_filepath, INVALID_HANDLE);
        }   else    {
            /* determine file extension, then load the texture based on that */
            char ext[128];
            path_getfileext(ext, lua_filepath);
            sct_t script = nullptr;

            /* model files should be valid extension */
            if (str_isequal_nocase(ext, "lua"))
                script = sct_load(lua_filepath, 0);

            if (script == nullptr) {
                err_sendtolog(TRUE);
                log_printf(LOG_WARNING, "res-mgr: loading rs_resource '%s' failed:"
                    " could not load script", lua_filepath);
                if (override_hdl != INVALID_HANDLE)
                    rs_remove_fromdb(override_hdl);
                return INVALID_HANDLE;
            }

            res_hdl = rs_add_resource(lua_filepath, script, override_hdl, rs_script_unload);

            /* add to hot-loading files */
            if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING) && !BIT_CHECK(flags, RS_LOAD_REFRESH))
                fio_mon_reg(lua_filepath, rs_script_reload, res_hdl, 0, 0);

            log_printf(LOG_LOAD, "(script) \"%s\" - id: %d", lua_filepath, GET_ID(res_hdl));
        }
    }

    return res_hdl;
}

reshandle_t rs_script_queueload(const char* lua_filepath, reshandle_t override_hdl)
{
    /* if we have an override handle, check in existing queue and see if it's already exists */
    if (override_hdl != INVALID_HANDLE && rs_loadqueue_search(override_hdl) != nullptr)
        return override_hdl;

    reshandle_t hdl = rs_add_resource(lua_filepath, nullptr, override_hdl, rs_script_unload);
    if (hdl == INVALID_HANDLE)
        return hdl;

    struct rs_load_data* ldata = (struct rs_load_data*)mem_pool_alloc(&g_rs.load_data_pool);
    ASSERT(ldata);
    memset(ldata, 0x00, sizeof(struct rs_load_data));

    str_safecpy(ldata->filepath, sizeof(ldata->filepath), lua_filepath);
    ldata->hdl = hdl;
    ldata->type = RS_RESOURCE_SCRIPT;
    ldata->reload = (override_hdl != INVALID_HANDLE);

    /* push to load list */
    list_addlast(&g_rs.load_list, &ldata->lnode, ldata);

    return hdl;
}

void rs_script_unload(void* script)
{
    if (script != nullptr)
        sct_unload((sct_t)script);
}

reshandle_t rs_load_phxprefab(const char* phx_filepath, uint flags)
{
    reshandle_t res_hdl = INVALID_HANDLE;
    reshandle_t override_hdl = res_hdl;

    if (!g_rs.init)
        return INVALID_HANDLE;

    struct hashtable_item_chained* item = hashtable_chained_find(&g_rs.dict,
        hash_str(phx_filepath));
    if (item != nullptr)   {
        uint idx = (uint)item->value;
        struct rs_resource* res = (struct rs_resource*)g_rs.ress.buffer + idx;
        res_hdl = res->hdl;
    }

    if (res_hdl != INVALID_HANDLE && BIT_CHECK(flags, RS_LOAD_REFRESH))    {
        /* rs_resource already loaded, but refresh flag is set, so we reload it */
        if (BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))   {
            res_hdl = rs_phxprefab_queueload(phx_filepath, res_hdl);
        }   else    {
            override_hdl =  res_hdl;
            res_hdl = INVALID_HANDLE;
        }
    }   else if (res_hdl != INVALID_HANDLE) {
        /* add ref count */
        struct rs_resource* ress = (struct rs_resource*)g_rs.ress.buffer;
        ress[GET_INDEX(res_hdl)].ref_cnt ++;
    }

    /* rs_resource is not loaded before, so we just have to load it for the first time */
    if (res_hdl == INVALID_HANDLE)  {
        if(BIT_CHECK(g_rs.flags, RS_FLAG_BGLOADING))  {
            res_hdl = rs_phxprefab_queueload(phx_filepath, INVALID_HANDLE);
        }   else    {
            /* determine file extension, then load the texture based on that */
            char ext[128];
            path_getfileext(ext, phx_filepath);
            phx_prefab prefab = nullptr;

            /* model files should be valid extension */
            if (str_isequal_nocase(ext, "h3dp"))
                prefab = phx_prefab_load(phx_filepath, g_rs.alloc, 0);

            if (prefab == nullptr) {
                err_sendtolog(TRUE);
                log_printf(LOG_WARNING, "res-mgr: loading rs_resource '%s' failed:"
                    " could not load phx-prefab", phx_filepath);
                if (override_hdl != INVALID_HANDLE)
                    rs_remove_fromdb(override_hdl);
                return INVALID_HANDLE;
            }

            res_hdl = rs_add_resource(phx_filepath, prefab, override_hdl, rs_phxprefab_unload);

            /* add to hot-loading files */
            if (BIT_CHECK(g_rs.flags, RS_FLAG_HOTLOADING) && !BIT_CHECK(flags, RS_LOAD_REFRESH))
                fio_mon_reg(phx_filepath, rs_phxprefab_reload, res_hdl, 0, 0);

            log_printf(LOG_LOAD, "(physics) \"%s\" - id: %d", phx_filepath, GET_ID(res_hdl));
        }
    }

    return res_hdl;
}

reshandle_t rs_phxprefab_queueload(const char* phx_filepath, reshandle_t override_hdl)
{
    /* if we have an override handle, check in existing queue and see if it's already exists */
    if (override_hdl != INVALID_HANDLE && rs_loadqueue_search(override_hdl) != nullptr)
        return override_hdl;

    reshandle_t hdl = rs_add_resource(phx_filepath, nullptr, override_hdl, rs_phxprefab_unload);
    if (hdl == INVALID_HANDLE)
        return hdl;

    struct rs_load_data* ldata = (struct rs_load_data*)mem_pool_alloc(&g_rs.load_data_pool);
    ASSERT(ldata);
    memset(ldata, 0x00, sizeof(struct rs_load_data));

    str_safecpy(ldata->filepath, sizeof(ldata->filepath), phx_filepath);
    ldata->hdl = hdl;
    ldata->type = RS_RESOURCE_PHXPREFAB;
    ldata->reload = (override_hdl != INVALID_HANDLE);

    /* push to load list */
    list_addlast(&g_rs.load_list, &ldata->lnode, ldata);

    return hdl;
}

void rs_phxprefab_unload(void* prefab)
{
    if (prefab != nullptr)
        phx_prefab_unload((phx_prefab)prefab);
}

void rs_reportleaks()
{
    struct rs_resource* rss = (struct rs_resource*)g_rs.ress.buffer;
    for (int i = 0; i < g_rs.ress.item_cnt; i++)   {
        const struct rs_resource* r = &rss[i];
        if (r->hdl != INVALID_HANDLE && r->ref_cnt > 0) {
            log_printf(LOG_WARNING, "res-mgr: unreleased \"%s\" (ref_cnt = %d, id = %d)",
                r->filepath, r->ref_cnt, GET_ID(r->hdl));
        }
    }
}

sct_s rs_get_script(reshandle_t script_hdl)
{
    ASSERT(g_rs.init);
    return (sct_s)rs_resource_get(script_hdl)->ptr;
}

gfx_texture rs_get_texture(reshandle_t tex_hdl)
{
    ASSERT(g_rs.init);
    return (gfx_texture)rs_resource_get(tex_hdl)->ptr;
}

phx_prefab rs_get_phxprefab(reshandle_t prefab_hdl)
{
    ASSERT(g_rs.init);
    return (phx_prefab)rs_resource_get(prefab_hdl)->ptr;
}

struct gfx_model* rs_get_model(reshandle_t mdl_hdl)
{
    ASSERT(g_rs.init);
    return (struct gfx_model*)rs_resource_get(mdl_hdl)->ptr;
}

animReel* rs_get_animreel(reshandle_t anim_hdl)
{
    ASSERT(g_rs.init);
    return (animReel*)rs_resource_get(anim_hdl)->ptr;
}

animCharController* rs_get_animctrl(reshandle_t ctrl_hdl)
{
    ASSERT(g_rs.init);
    return (animCharController*)rs_resource_get(ctrl_hdl)->ptr;
}

void rs_set_dataalloc(struct allocator* alloc)
{
    g_rs.alloc = alloc;
}

int rs_isinit()
{
    return g_rs.init;
}

const char* rs_get_filepath(reshandle_t hdl)
{
    if (!g_rs.init)
        return "";
    return rs_resource_get(hdl)->filepath;
}

void rs_add_flags(uint flags)
{
    BIT_REMOVE(flags, RS_FLAG_PREPARE_BGLOAD);

    if (!BIT_CHECK(g_rs.flags, RS_FLAG_PREPARE_BGLOAD))
        BIT_REMOVE(flags, RS_FLAG_BGLOADING);

    BIT_ADD(g_rs.flags, flags);
}

void rs_remove_flags(uint flags)
{
    BIT_REMOVE(flags, RS_FLAG_PREPARE_BGLOAD);
    BIT_REMOVE(g_rs.flags, flags);
}


uint rs_get_flags()
{
    return g_rs.flags;
}

void rs_unloadptr(reshandle_t hdl)
{
    struct rs_resource* r = rs_resource_get(hdl);
    r->unload_func(r->ptr);
    r->ptr = nullptr;
}
