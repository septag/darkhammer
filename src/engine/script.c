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

/**
 * notice: script executation is not currently thread-safe
 * but loading in another thread and memory management IS thread-safe */

#include "dhcore/core.h"
#include "dhcore/file-io.h"
#include "dhcore/pool-alloc.h"
#include "dhcore/array.h"
#include "dhcore/mt.h"
#include "dhcore/timer.h"
#include "dhcore/json.h"
#include "dhcore/task-mgr.h"

#include "script.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"


#ifdef __cplusplus
}
#endif

#include "mem-ids.h"
#include "console.h"
#include "debug-hud.h"
#include "res-mgr.h"
#include "prf-mgr.h"
#include "init-params.h"

#include "components/cmp-trigger.h"

#define GC_TIMEOUT 0.5f
#define GC_THRESHOLD 1024
#define GC_INTERATIONS 1
#define DEFAULT_MEM_SIZE (8*1024*1024)

/* we create multiple allocators for different sizes,
 * Each pool allocator has an index defined below */
#define SCT_ALLOC_32    0
#define SCT_ALLOC_64    1
#define SCT_ALLOC_128   2
#define SCT_ALLOC_256   3
#define SCT_ALLOC_512   4
#define SCT_ALLOC_1K    5
#define SCT_ALLOC_2K    6
#define SCT_ALLOC_CNT   7

/*************************************************************************************************
 * types
 */
enum sct_arg_type
{
    SCT_ARG_INT,
    SCT_ARG_FLOAT,
    SCT_ARG_STRING,
    SCT_ARG_PTR
};

struct sct_timer_event
{
    uint id;
    char funcname[32];
    reshandle_t hdl;
    float dt; /* in seconds */
    float timeout;   /* in seconds */
    bool_t single_shot;
    struct linked_list lnode;
};

struct sct_trigger_event
{
    char funcname[32];
    reshandle_t hdl;
    cmphandle_t cmp_hdl;    /* trigger component */
    struct linked_list lnode;
};

struct sct_gcitem
{
    float t;
    lua_State* ls;
    int init_sz;  /* initial mem size after first run */
    int last_sz;
    int threshold;
    struct linked_list lnode;
};

struct sct_mgr
{
    struct sct_params params;
    lua_State* cur_ls;  /* current running lua script (=NULL if nothing's running) */
    reshandle_t cur_hdl;    /* current running script handle */
    struct timer* tm;
    struct linked_list* tm_events;  /* linked-list of running timer events (data: sct_timer_event)*/
    struct linked_list* trigger_events; /* linked-list of running trigger events (data: sct_trigger_event) */
    struct array garbage_scripts;   /* scripts that were not unloaded immediately (have tm_events)
                                       but now are garbage */
    bool_t monitor;
    struct linked_list* gcs;    /* scripts that needs to be garbage-collected periodically */
    struct pool_alloc_ts buffs[SCT_ALLOC_CNT];
};

/*************************************************************************************************
 * fwd declerations
 */
/* callback for triggers */
void sct_trigger_callback(struct cmp_obj* trigger_obj, struct cmp_obj* other_obj,
    enum phx_trigger_state state, void* param);

/* callback for lua_State allocation
 * see (lua_Alloc): http://www.lua.org/manual/5.1/manual.html */
void* sct_lua_alloc_callback(void* ud, void* ptr, size_t osize, size_t nsize);

/* callback for lua_State panic */
int sct_lua_panic_callback(lua_State* l);

#ifdef __cplusplus
extern "C" {
#endif
/* bindings */
int luaopen_core(lua_State* l);
int luaopen_eng(lua_State* l);
#ifdef __cplusplus
}
#endif

/* console commands */
result_t sct_console_runfile(uint argc, const char** argv, void* param);

/* if fname=NULL, runs the whole script, after fname is argument pairs (type, value) */
bool_t sct_call(reshandle_t s_hdl, const char* fname, uint arg_cnt, ...);

void sct_runscript(reshandle_t s_hdl);

bool_t sct_checkresident(reshandle_t hdl);
void sct_collectgarbage(float dt);
void sct_addgarbage(reshandle_t hdl);
void sct_removealltimers();
void sct_removealltriggers();
void sct_removeevents(reshandle_t hdl);

void sct_addgc(lua_State* ls);
void sct_removegc(lua_State* ls);

result_t sct_create_buffs();
void sct_destroy_buffs();

uint sct_choose_alloc(size_t sz);

/*************************************************************************************************
 * globals
 */
static struct sct_mgr g_sct;

/*************************************************************************************************
 * inlines
 */
INLINE const char* sct_geterror(lua_State* l)
{
    ASSERT(lua_isstring(l, -1))
    return lua_tostring(l, -1);
}

INLINE void* sct_alloc_putsize(void* ptr, uint sz)
{
    *((uint*)ptr) = sz;
    return ((uint8*)ptr + sizeof(uint));
}

INLINE uint sct_alloc_getsize(void* ptr, void** preal_ptr)
{
    uint8* bptr = (uint8*)ptr - sizeof(uint);
    uint sz = *((uint*)bptr);
    *preal_ptr = bptr;
    return sz;
}

/*************************************************************************************************/
void sct_zero()
{
    memset(&g_sct, 0x00, sizeof(g_sct));
}

result_t sct_init(const struct sct_params* params, bool_t monitor)
{
    result_t r;

    log_print(LOG_TEXT, "init script ...");

    memcpy(&g_sct.params, params, sizeof(struct sct_params));

    r = sct_create_buffs();
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    r = arr_create(mem_heap(), &g_sct.garbage_scripts, sizeof(reshandle_t), 10, 10, MID_SCT);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    g_sct.tm = timer_createinstance(TRUE);
    g_sct.monitor = monitor;

    /* console commands */
    con_register_cmd("lua_runfile", sct_console_runfile, NULL, "lua_runfile [lua_file]");

    return RET_OK;
}

void sct_release()
{
    /* cleanup */
    sct_removealltimers();
    sct_removealltriggers();

    if (!arr_isempty(&g_sct.garbage_scripts)) {
        log_printf(LOG_INFO, "script: unloading %d resident scripts",
            g_sct.garbage_scripts.item_cnt);
    }
    sct_collectgarbage(0.0f);

    /* */
    uint leak_cnt = 0;
    for (uint i = 0; i < SCT_ALLOC_CNT; i++)
        leak_cnt += mem_pool_getleaks_ts(&g_sct.buffs[i]);
    if (leak_cnt > 0)
        log_printf(LOG_WARNING, "script: %d leaks detected", leak_cnt);

    if (g_sct.tm != NULL)
        timer_destroyinstance(g_sct.tm);
    arr_destroy(&g_sct.garbage_scripts);
    sct_destroy_buffs();
}

result_t sct_create_buffs()
{
    if (IS_FAIL(mem_pool_create_ts(mem_heap(), &g_sct.buffs[SCT_ALLOC_32], 32, 1024, MID_SCT)))
        return RET_OUTOFMEMORY;
    if (IS_FAIL(mem_pool_create_ts(mem_heap(), &g_sct.buffs[SCT_ALLOC_64], 64, 1024, MID_SCT)))
        return RET_OUTOFMEMORY;
    if (IS_FAIL(mem_pool_create_ts(mem_heap(), &g_sct.buffs[SCT_ALLOC_128], 128, 512, MID_SCT)))
        return RET_OUTOFMEMORY;
    if (IS_FAIL(mem_pool_create_ts(mem_heap(), &g_sct.buffs[SCT_ALLOC_256], 256, 512, MID_SCT)))
        return RET_OUTOFMEMORY;
    if (IS_FAIL(mem_pool_create_ts(mem_heap(), &g_sct.buffs[SCT_ALLOC_512], 512, 256, MID_SCT)))
        return RET_OUTOFMEMORY;
    if (IS_FAIL(mem_pool_create_ts(mem_heap(), &g_sct.buffs[SCT_ALLOC_1K], 1024, 128, MID_SCT)))
        return RET_OUTOFMEMORY;
    if (IS_FAIL(mem_pool_create_ts(mem_heap(), &g_sct.buffs[SCT_ALLOC_2K], 2048, 128, MID_SCT)))
        return RET_OUTOFMEMORY;

    return RET_OK;
}

void sct_destroy_buffs()
{
    for (uint i = 0; i < SCT_ALLOC_CNT; i++)
        mem_pool_destroy_ts(&g_sct.buffs[i]);
}

void sct_reload(const char* filepath, reshandle_t hdl, bool_t manual)
{
    ASSERT(hdl != INVALID_HANDLE);
    /* coming from res-mgr
     * - remove all events related to script
     * - re-run the script
     */
    sct_removeevents(hdl);
    if (!manual)
        hdl = rs_load_script(filepath, RS_LOAD_REFRESH);

    if (hdl != INVALID_HANDLE)
        sct_runscript(hdl);
}

sct_t sct_load(const char* lua_filepath, uint thread_id)
{
    struct allocator* tmp_alloc = tsk_get_localalloc(thread_id);
    A_SAVE(tmp_alloc);

    /* load file data */
    file_t f = fio_openmem(tmp_alloc, lua_filepath, FALSE, MID_SCT);
    if (f == NULL) {
        A_LOAD(tmp_alloc);
        err_printf(__FILE__, __LINE__, "script: could not open file '%s'", lua_filepath);
        return NULL;
    }
    size_t size;
    void* buff = fio_detachmem(f, &size, NULL);
    fio_close(f);

    /* create lua_state and open libraries */
    lua_State* ls = lua_newstate(sct_lua_alloc_callback, NULL);
    if (ls == NULL) {
        A_FREE(tmp_alloc, buff);
        A_LOAD(tmp_alloc);
        err_printf(__FILE__, __LINE__, "script: could not create lua_State for '%s'",
            lua_filepath);
        return NULL;
    }
    lua_atpanic(ls, sct_lua_panic_callback);

    luaL_requiref(ls, "table", luaopen_table, 1); lua_pop(ls, 1);
    luaL_requiref(ls, "string", luaopen_string, 1); lua_pop(ls, 1);
    luaL_requiref(ls, "math", luaopen_math, 1); lua_pop(ls, 1);

    luaopen_core(ls);
    luaopen_eng(ls);

    /* compile code */
    int r = luaL_loadbuffer(ls, (const char*)buff, size, lua_filepath);
    A_FREE(tmp_alloc, buff);
    A_LOAD(tmp_alloc);

    if (r != 0) {
        err_printf(__FILE__, __LINE__, "script: lua script '%s' failed: %s",
            lua_filepath, sct_geterror(ls));
        sct_unload(ls);
        return NULL;
    }

    /* ok */
    return ls;
}

void sct_unload(sct_t s)
{
    ASSERT(s);
    sct_removegc((lua_State*)s);
    lua_close((lua_State*)s);
}

void* sct_lua_alloc_callback(void* ud, void* ptr, size_t osize, size_t nsize)
{
    ASSERT(nsize < UINT32_MAX);

    void* rptr;
    if (ptr == NULL && nsize != 0)    {
        /* normal malloc */
        nsize += sizeof(uint);    /* to keep the size */
        uint a_idx = sct_choose_alloc(nsize);
        if (a_idx != INVALID_INDEX)
            rptr = mem_pool_alloc_ts(&g_sct.buffs[a_idx]);
        else
            rptr = mem_alignedalloc(nsize, 16, __FILE__, __LINE__, MID_SCT);
        ASSERT(rptr);
        rptr = sct_alloc_putsize(rptr, (uint)nsize);
    }   else if (ptr != NULL && nsize == 0)   {
        /* free */
        uint sz = sct_alloc_getsize(ptr, &ptr);
        uint a_idx = sct_choose_alloc(sz);

        if (a_idx != INVALID_INDEX)
            mem_pool_free_ts(&g_sct.buffs[a_idx], ptr);
        else
            mem_alignedfree(ptr);

        rptr = NULL;
    }   else if (ptr != NULL && nsize != 0)   {
        /* realloc */
        nsize += sizeof(uint);
        void* real_ptr;
        uint sz = sct_alloc_getsize(ptr, &real_ptr);
        uint a_idx = sct_choose_alloc(nsize);
        uint a_idx2 = sct_choose_alloc(sz);

        /* alloc from new allocator */
        void* tmp;
        if (a_idx != INVALID_INDEX)
            tmp = mem_pool_alloc_ts(&g_sct.buffs[a_idx]);
        else
            tmp = mem_alignedalloc(nsize, 16, __FILE__, __LINE__, MID_SCT);
        ASSERT(tmp);
        tmp = sct_alloc_putsize(tmp, (uint)nsize);
        memcpy(tmp, ptr, minun((uint)nsize, sz) - sizeof(uint));

        /* free from previous allocator */
        if (a_idx2 != INVALID_INDEX) {
            mem_pool_free_ts(&g_sct.buffs[a_idx2], real_ptr);
        }   else    {
            mem_alignedfree(real_ptr);
        }

        rptr = tmp;
    }   else    {
        rptr = NULL;
    }

    return rptr;
}

uint sct_choose_alloc(size_t sz)
{
    if (sz <= 32)
        return SCT_ALLOC_32;
    else if (sz > 32 && sz <=64)
        return SCT_ALLOC_64;
    else if (sz > 64 && sz <= 128)
        return SCT_ALLOC_128;
    else if (sz > 128 && sz <= 256)
        return SCT_ALLOC_256;
    else if (sz > 256 && sz <= 512)
        return SCT_ALLOC_512;
    else if (sz > 512 && sz <= 1024)
        return SCT_ALLOC_1K;
    else if (sz > 1024 && sz <= 2048)
        return SCT_ALLOC_2K;
    else
        return INVALID_INDEX;
}


int sct_lua_panic_callback(lua_State* l)
{
    log_printf(LOG_ERROR, "lua: unprotected error: %s", sct_geterror(l));
    return 0;
}

void sct_throwerror(const char* fmt, ...)
{
    if (g_sct.cur_ls != NULL)   {
        char text[512];

        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        luaL_error(g_sct.cur_ls, text);
    }
}

result_t sct_console_runfile(uint argc, const char** argv, void* param)
{
    if (argc != 1)
        return RET_INVALIDARG;
    sct_runfile(argv[0]);
    return RET_OK;
}

result_t sct_runfile(const char* lua_filepath)
{
    /* create and load the script */
    if (g_sct.monitor)
        fio_mon_unreg(lua_filepath);
    reshandle_t shdl = rs_load_script(lua_filepath, 0);
    if (shdl == INVALID_HANDLE)
        return RET_FAIL;
    sct_runscript(shdl);
    return RET_OK;
}

void sct_runscript(reshandle_t s_hdl)
{
    /* run main script code */
    sct_call(s_hdl, NULL, 0);

    sct_t s = rs_get_script(s_hdl);

    /* unload only if script is not resident
     * and also multi-threaded loading is not in progress */
    if (s != NULL && !sct_checkresident(s_hdl))   {
        char filepath[DH_PATH_MAX];
        strcpy(filepath, rs_get_filepath(s_hdl));
        rs_unloadptr(s_hdl);
    }
}

bool_t sct_call(reshandle_t s_hdl, const char* fname, uint arg_cnt, ...)
{
    lua_State* ls = (lua_State*)rs_get_script(s_hdl);
    if (ls == NULL)
        return TRUE;

    g_sct.cur_ls = ls;
    g_sct.cur_hdl = s_hdl;

    int r;
    if (fname == NULL)  {
        r = lua_pcall(ls, 0, LUA_MULTRET, 0);
        if (lua_gc(ls, LUA_GCCOUNT, 0) > GC_THRESHOLD)  {
            log_printf(LOG_WARNING, "script: '%s' is using too much memory"
                ", may decrease performance", rs_get_filepath(s_hdl));
        }

    }   else    {
        va_list args;
        va_start(args, arg_cnt);

        lua_getglobal(ls, fname);
        for (uint i = 0; i < arg_cnt; i++)    {
            uint type = va_arg(args, unsigned int);
            switch ((enum sct_arg_type)type)    {
            case SCT_ARG_INT:
                lua_pushinteger(ls, va_arg(args, int));
                break;
            case SCT_ARG_FLOAT:
                lua_pushnumber(ls, va_arg(args, double));
                break;
            case SCT_ARG_STRING:
                lua_pushstring(ls, va_arg(args, const char*));
                break;
            case SCT_ARG_PTR:
                lua_pushlightuserdata(ls, va_arg(args, void*));
                break;
            }
        }
        va_end(args);

        r = lua_pcall(ls, arg_cnt, 0, 0);
    }

    g_sct.cur_ls = NULL;
    g_sct.cur_hdl = INVALID_HANDLE;

    if (r != 0) {
        log_printf(LOG_WARNING, "script: running '%s' failed: %s",
            fname == NULL ? "(main)" : fname, sct_geterror(ls));
        return FALSE;
    }

    return TRUE;
}

void sct_update()
{
    PRF_OPENSAMPLE("script");

    bool_t r;
    float dt = g_sct.tm->dt;

    /* call timer tm_events in the scripts */
    struct linked_list* node = g_sct.tm_events;
    while (node != NULL)    {
        struct sct_timer_event* e = (struct sct_timer_event*)node->data;
        struct linked_list* next = node->next;
        e->dt += dt;
        if (e->dt > e->timeout) {
            e->dt = 0.0f;
            r = sct_call(e->hdl, e->funcname, 1, SCT_ARG_INT, e->id);
            if (!r || e->single_shot) {
                /* error occurred (maybe timer function is missing). remove timer */
                sct_removetimer(e->id);
            }
        }
        node = next;
    }

    sct_collectgarbage(dt);

    PRF_CLOSESAMPLE(); /* script */
}

uint sct_addtimer(uint timeout, const char* funcname, bool_t single_shot)
{
    static uint id = 1;

    lua_State* ls = (lua_State*)rs_get_script(g_sct.cur_hdl);
    if (ls == NULL)
        return 0;

    struct sct_timer_event* e = (struct sct_timer_event*)ALLOC(sizeof(struct sct_timer_event),
        MID_SCT);
    memset(e, 0x00, sizeof(struct sct_timer_event));

    e->id = id++;
    str_safecpy(e->funcname, sizeof(e->funcname), funcname);
    e->hdl = g_sct.cur_hdl;
    e->timeout = ((float)timeout) / 1000.0f;
    e->single_shot = single_shot;

    list_add(&g_sct.tm_events, &e->lnode, e);

    sct_addgc(ls);
    return e->id;
}


void sct_removetimer(uint tid)
{
    struct linked_list* node = g_sct.tm_events;
    while (node != NULL)    {
        struct sct_timer_event* e = (struct sct_timer_event*)node->data;
        struct linked_list* next = node->next;
        if (e->id == tid)   {
            list_remove(&g_sct.tm_events, node);
            sct_addgarbage(e->hdl);
            FREE(e);
            break;
        }
        node = next;
    }
}

void sct_addtrigger(cmphandle_t trigger_cmp, const char* funcname)
{
    ASSERT(trigger_cmp != INVALID_HANDLE);

    lua_State* ls = (lua_State*)rs_get_script(g_sct.cur_hdl);
    if (ls == NULL)
        return;

    struct sct_trigger_event* e = (struct sct_trigger_event*)ALLOC(sizeof(struct sct_trigger_event),
        MID_SCT);
    memset(e, 0x00, sizeof(struct sct_trigger_event));

    str_safecpy(e->funcname, sizeof(e->funcname), funcname);
    e->hdl = g_sct.cur_hdl;
    e->cmp_hdl = trigger_cmp;

    list_add(&g_sct.trigger_events, &e->lnode, e);
    sct_addgc(ls);

    cmp_trigger_register_callback(trigger_cmp, sct_trigger_callback, e);
}

void sct_removetrigger(struct sct_trigger_event* te)
{
    struct linked_list* node = g_sct.trigger_events;
    while (node != NULL)    {
        struct sct_trigger_event* e = (struct sct_trigger_event*)node->data;
        struct linked_list* next = node->next;
        if (e == te)   {
            cmp_trigger_unregister_callback(e->cmp_hdl);
            list_remove(&g_sct.trigger_events, node);
            sct_addgarbage(e->hdl);
            FREE(e);
            break;
        }
        node = next;
    }
}

void sct_removetrigger_byfuncname(const char* funcname)
{
    struct linked_list* node = g_sct.trigger_events;
    while (node != NULL)    {
        struct sct_trigger_event* e = (struct sct_trigger_event*)node->data;
        struct linked_list* next = node->next;
        if (str_isequal(funcname, e->funcname))   {
            cmp_trigger_unregister_callback(e->cmp_hdl);
            list_remove(&g_sct.trigger_events, node);
            sct_addgarbage(e->hdl);
            FREE(e);
            break;
        }
        node = next;
    }
}


void sct_addgarbage(reshandle_t hdl)
{
    /* check if we have anymore pending tm_events from the script */
    if (!sct_checkresident(hdl)) {
        lua_State* ls = (lua_State*)rs_get_script(hdl);
        if (ls == NULL)
            return;

        sct_removegc(ls);
        reshandle_t* pres = (reshandle_t*)arr_add(&g_sct.garbage_scripts);
        *pres = hdl;
    }
}

bool_t sct_checkresident(reshandle_t hdl)
{
    /* check for existance of timer events */
    struct linked_list* node = g_sct.tm_events;
    while (node != NULL)    {
        struct sct_timer_event* e = (struct sct_timer_event*)node->data;
        if (e->hdl == hdl)
            return TRUE;
        node = node->next;
    }

    /* check trigger events */
    node = g_sct.trigger_events;
    while (node != NULL)    {
        struct sct_trigger_event* e = (struct sct_trigger_event*)node->data;
        if (e->hdl == hdl)
            return TRUE;
        node = node->next;
    }

    return FALSE;
}

void sct_collectgarbage(float dt)
{
    /* unload scripts that are assigned as garbage */
    uint cnt = g_sct.garbage_scripts.item_cnt;
    reshandle_t* ress = (reshandle_t*)g_sct.garbage_scripts.buffer;

    for (uint i = 0; i < cnt; i++)    {
        ASSERT(ress[i] != INVALID_HANDLE);
        rs_unloadptr(ress[i]);
    }

    arr_clear(&g_sct.garbage_scripts);

    /* do garbage collection for each script that remains in GC list */
    struct linked_list* gcnode = g_sct.gcs;
    while (gcnode != NULL)  {
        struct sct_gcitem* gc = (struct sct_gcitem*)gcnode->data;
        gc->t += dt;
        int alloc_sz = lua_gc(gc->ls, LUA_GCCOUNT, 0);
        alloc_sz -= gc->init_sz;
        if (gc->t > GC_TIMEOUT && alloc_sz > gc->last_sz) {
            if (alloc_sz < gc->threshold)    {
                lua_gc(gc->ls, LUA_GCSTEP, GC_INTERATIONS);
            }   else    {
                lua_gc(gc->ls, LUA_GCCOLLECT, 0);
            }
            gc->t = 0.0f;
            gc->last_sz = alloc_sz;
        }
        gcnode = gcnode->next;
    }
}

void sct_removealltimers()
{
    struct linked_list* node = g_sct.tm_events;
    while (node != NULL)    {
        struct sct_timer_event* e = (struct sct_timer_event*)node->data;
        struct linked_list* next = node->next;

        list_remove(&g_sct.tm_events, node);
        sct_addgarbage(e->hdl);
        FREE(e);

        node = next;
    }
}

void sct_removealltriggers()
{
    struct linked_list* node = g_sct.trigger_events;
    while (node != NULL)    {
        struct linked_list* next = node->next;
        struct sct_trigger_event* e = (struct sct_trigger_event*)node->data;

        list_remove(&g_sct.trigger_events, node);
        sct_addgarbage(e->hdl);
        FREE(e);

        node = next;
    }
}

/* remove registered events for specific script */
void sct_removeevents(reshandle_t hdl)
{
    /* timers */
    struct linked_list* node = g_sct.tm_events;
    while (node != NULL)    {
        struct sct_timer_event* e = (struct sct_timer_event*)node->data;
        struct linked_list* next = node->next;
        if (e->hdl == hdl)   {
            list_remove(&g_sct.tm_events, node);
            FREE(e);
        }
        node = next;
    }

    /* triggers */
    node = g_sct.trigger_events;
    while (node != NULL)    {
        struct linked_list* next = node->next;
        struct sct_trigger_event* e = (struct sct_trigger_event*)node->data;
        if (e->hdl == hdl)  {
            list_remove(&g_sct.trigger_events, node);
            FREE(e);
        }
        node = next;
    }
}

void sct_getmemstats(struct sct_memstats* stats)
{
    stats->buff_alloc = mem_sizebyid(MID_SCT);
    stats->buff_max = 0;
}

void* sct_alloc(size_t sz)
{
    return sct_lua_alloc_callback(NULL, NULL, sz, sz);
}

void sct_free(void* p)
{
    sct_lua_alloc_callback(p, p, 0, 0);
}

void sct_addgc(lua_State* ls)
{
    /* check if we haven't added it to the list yet */
    struct linked_list* node = g_sct.gcs;
    while (node != NULL)    {
        struct sct_gcitem* gc = (struct sct_gcitem*)node->data;
        if (gc->ls == ls)   {
            return; /* already in the list */
        }
        node = node->next;
    }

    /* create new gc item and add it to the list */
    struct sct_gcitem* gc = (struct sct_gcitem*)ALLOC(sizeof(struct sct_gcitem), MID_SCT);
    if (gc == NULL)
        return;
    memset(gc, 0x00, sizeof(struct sct_gcitem));

    gc->ls = ls;
    gc->init_sz = lua_gc(ls, LUA_GCCOUNT, 0);
    gc->threshold = GC_THRESHOLD;
    list_add(&g_sct.gcs, &gc->lnode, gc);
}

void sct_removegc(lua_State* ls)
{
    struct linked_list* node = g_sct.gcs;
    while (node != NULL)    {
        struct sct_gcitem* gc = (struct sct_gcitem*)node->data;
        if (gc->ls == ls)   {
            list_remove(&g_sct.gcs, node);
            FREE(gc);
            break;
        }
        node = node->next;
    }
}

void sct_setthreshold(int mem_sz)
{
    if (g_sct.cur_ls)   {
        lua_State* ls = g_sct.cur_ls;
        struct linked_list* node = g_sct.gcs;
        while (node != NULL)    {
            struct sct_gcitem* gc = (struct sct_gcitem*)node->data;
            if (gc->ls == ls)
                gc->threshold = mem_sz;
            node = node->next;
        }
    }
}

void sct_parseparams(struct sct_params* params, json_t j)
{
    memset(params, 0x00, sizeof(struct sct_params));

    /* script */
    json_t jsct = json_getitem(j, "script");
    if (jsct != NULL)  {
        params->mem_sz = json_geti_child(jsct, "mem-size", 0);
    }   else    {
        params->mem_sz = 0;
    }
}

void sct_trigger_callback(struct cmp_obj* trigger_obj, struct cmp_obj* other_obj,
    enum phx_trigger_state state, void* param)
{
    struct sct_trigger_event* e = (struct sct_trigger_event*)param;
    ASSERT(e);
    bool_t r = sct_call(e->hdl, e->funcname, 3, SCT_ARG_INT, trigger_obj->id,
        SCT_ARG_INT, other_obj->id, SCT_ARG_INT, (int)state);

    /* error occurred (maybe timer function is missing). remove trigger */
    if (!r)
        sct_removetrigger(e);
}

