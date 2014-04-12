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
#include <time.h>

#include "config.h"
#include "engine.h"
#include "dhcore/core.h"
#include "dhcore/timer.h"
#include "dhcore/json.h"
#include "dhcore/pak-file.h"
#include "dhcore/freelist-alloc.h"
#include "dhcore/task-mgr.h"
#include "dhcore/hwinfo.h"

#include "dhapp/input.h"

#include "mem-ids.h"
#include "gfx.h"
#include "dhcore/file-io.h"
#include "dhcore/stack-alloc.h"
#include "res-mgr.h"
#include "debug-hud.h"
#include "console.h"
#include "prf-mgr.h"
#include "cmp-mgr.h"
#include "scene-mgr.h"
#include "script.h"
#include "gfx-canvas.h"
#include "gfx-cmdqueue.h"
#include "lod-scheme.h"
#include "phx.h"
#include "world-mgr.h"
#include "gfx-device.h"

#define GRAPH_WIDTH 250
#define GRAPH_HEIGHT 100

#define FRAME_STACK_SIZE (8*1024*1024)
#define LSR_SIZE (128*1024)
#define DATA_SIZE (8*1024*1024)

/*************************************************************************************************
 * types
 */
struct engine
{
    struct pak_file data_pak;   /* only loaded in release mode */
    char share_dir[DH_PATH_MAX];

    struct init_params params;
    struct frame_stats frame_stats;
    struct timer* timer;
    int simulate_mode;
    int init;
    struct hwinfo hwinfo;
    size_t tmp_sz;  /* temp memory size (for each thread we have one) */

    struct allocator data_alloc;
    struct freelist_alloc_ts data_freelist;

    struct allocator lsr_alloc;
    struct stack_alloc lsr_stack;

    uint alloc_tmp0_frameid;  /* maximum tmp allocated frameid */
    uint alloc_tmp0_max; /* maxomum allocated bytes from temp0 stack allocator */
    int fps_lock;    /* =0 if not locked */
};

/*************************************************************************************************/
/* engine global */
static struct engine* g_eng = NULL;

/*************************************************************************************************
 * fwd declarations
 */
void eng_world_regvars();
result_t eng_console_showfps(uint argc, const char ** argv, void* param);
result_t eng_console_showft(uint argc, const char ** argv, void* param);
result_t eng_console_showgraph(uint argc, const char ** argv, void* param);
result_t eng_console_lockfps(uint argc, const char** argv, void* param);

int eng_hud_drawcallgraph(gfx_cmdqueue cmdqueue, ui_widget widget, int x, int y, int update,
    void* param);
int eng_hud_drawftgraph(gfx_cmdqueue cmdqueue, ui_widget widget, int x, int y, int update,
    void* param);
int eng_hud_drawfpsgraph(gfx_cmdqueue cmdqueue, ui_widget widget, int x, int y, int update,
    void* param);
int eng_hud_drawfps(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param);
int eng_hud_drawft(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param);

/*************************************************************************************************/
void eng_zero()
{
    gfx_zero();
    rs_zero();
    con_zero();
    hud_zero();
    sct_zero();
    lod_zero();
    wld_zero();

#if defined(_PROFILE_)
    prf_zero();
#endif
    phx_zero();
}

/*************************************************************************************************
 * Heap allocation callbacks
 */
void* eng_allocfn_data(size_t size, const char* source, uint line, uint id, void* param)
{
    return mem_alloc(size, source, line, MID_DATA);
}

void* eng_alignedallocfn_data(size_t size, uint8 alignment, const char* source, uint line,
                              uint id, void* param)
{
    return mem_alignedalloc(size, alignment, source, line, MID_DATA);
}

void* eng_allocfn_lsr(size_t size, const char* source, uint line, uint id, void* param)
{
    return mem_alloc(size, source, line, MID_LSR);
}

void* eng_alignedallocfn_lsr(size_t size, uint8 alignment, const char* source, uint line,
                             uint id, void* param)
{
    return mem_alignedalloc(size, alignment, source, line, MID_LSR);
}

/*************************************************************************************************/
result_t eng_init(const struct init_params* params)
{
    result_t r = RET_OK;

    ASSERT(g_eng == NULL);
    g_eng = (struct engine*)ALLOC(sizeof(struct engine), 0);
    if (g_eng == 0)
        return err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
    memset(g_eng, 0x00, sizeof(struct engine));

    eng_zero();

    memcpy(&g_eng->params, params, sizeof(struct init_params));

    hw_getinfo(&g_eng->hwinfo, HWINFO_ALL);

    /* console (before anything else) */
    if (BIT_CHECK(params->flags, ENG_FLAG_CONSOLE))	{
		r |= con_init(params->console_lines_max);
		if (IS_FAIL(r))
			return RET_FAIL;
		log_outputfunc(TRUE, con_log, NULL);
    }

    /* show build options */
#if !defined(FULL_VERSION)
#error "must define FULL_VERSION macro"
#endif

    time_t raw_tm;
    time(&raw_tm);

    log_printf(LOG_TEXT, "init darkhammer engine v%s build[%s, %s, %s, %s], time: %s", 
        FULL_VERSION,
#if defined(_DEBUG_)
    		"debug"
#else
    		"release"
#endif
    		,
#if defined(_PROFILE_)
    		"profile"
#else
    		"no-profile"
#endif
    		,
#if defined(_X86_)
    		"x86"
#elif defined(_X64_)
    		"x64"
#endif
    		,
#if defined(_ENABLEASSERT_)
    		"assert"
#else
    		"no-assert"
#endif
            , asctime(localtime(&raw_tm)));

    /* hardware info */
    hw_printinfo(&g_eng->hwinfo, HWINFO_ALL);

    size_t tmp_sz = params->dev.buffsize_tmp;
    size_t data_sz = data_sz = params->dev.buffsize_data;
    tmp_sz = tmp_sz != 0 ? ((size_t)tmp_sz*1024) : FRAME_STACK_SIZE;
    data_sz = data_sz != 0 ? ((size_t)data_sz*1024) : DATA_SIZE;

    /* allocators */
    /* dynamic allocator for data in dev (editor) mode, stack allocator in game (normal) mode */
    if (BIT_CHECK(params->flags, ENG_FLAG_OPTIMIZEMEMORY))   {
        /* lsr (load-stay-resident) allocator for essential engine data */
        r |= mem_stack_create(mem_heap(), &g_eng->lsr_stack, LSR_SIZE, MID_DATA);
        mem_stack_bindalloc(&g_eng->lsr_stack, &g_eng->lsr_alloc);

        r |= mem_freelist_create_ts(mem_heap(), &g_eng->data_freelist, data_sz, MID_DATA);
        mem_freelist_bindalloc_ts(&g_eng->data_freelist, &g_eng->data_alloc);
    }   else    {
        mem_heap_bindalloc(&g_eng->data_alloc);
        mem_heap_bindalloc(&g_eng->lsr_alloc);

        g_eng->data_alloc.alloc_fn = eng_allocfn_data;
        g_eng->data_alloc.alignedalloc_fn = eng_alignedallocfn_data;
        g_eng->lsr_alloc.alloc_fn = eng_allocfn_lsr;
        g_eng->lsr_alloc.alignedalloc_fn = eng_alignedallocfn_lsr;

        r = RET_OK;
    }

    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: out of memory for allocators");
        return RET_FAIL;
    }

    /* timer manager and frame timer */
    g_eng->timer = timer_createinstance(TRUE);

    /* add engine's own data path to file-mgr */
    if (params->data_path != NULL)   {
        char data_path_ext[DH_PATH_MAX];
        path_getfileext(data_path_ext, params->data_path);
        if (str_isequal_nocase(data_path_ext, "pak"))    {
            if (IS_FAIL(pak_open(&g_eng->data_pak, mem_heap(), params->data_path, 0)))    {
                err_print(__FILE__, __LINE__, "engine init: could not open data pak");
                return RET_FAIL;
            }
        }   else   {
            if (!util_pathisdir(params->data_path)) {
                err_print(__FILE__, __LINE__, "engine init: data path is not valid");
                return RET_FAIL;
            }
            fio_addvdir(params->data_path, FALSE);
        }
        /* assume that share directory is same as data dir */
        path_getdir(g_eng->share_dir, params->data_path);
    }   else    {
        char data_path[DH_PATH_MAX];
        char share_dir[DH_PATH_MAX];
        path_norm(share_dir, SHARE_DIR);
        path_join(data_path, share_dir, "data", NULL);
        if (!util_pathisdir(data_path)) {
            err_print(__FILE__, __LINE__, "engine init: data path is not valid");
            return RET_FAIL;
        }

        fio_addvdir(data_path, FALSE);  /* set default (config.h configured on build) data dir */
        strcpy(g_eng->share_dir, share_dir);
    }

    uint rs_flags = 0;
    /* activate hot loading in DEV mode */
    rs_flags |= BIT_CHECK(params->flags, ENG_FLAG_DEV) ? RS_FLAG_HOTLOADING : 0;
    if (!BIT_CHECK(params->flags, ENG_FLAG_DISABLEBGLOAD))  {
        rs_flags |= RS_FLAG_PREPARE_BGLOAD;
    }

    /* task manager */
    uint thread_cnt = maxui(g_eng->hwinfo.cpu_core_cnt - 1, 1);
    r = tsk_initmgr(thread_cnt, 0, tmp_sz, 0);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init task-mgr");
        return RET_FAIL;
    }
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    A_SAVE(tmp_alloc);

    /* resource manager (with only 1 thread for multi-thread loading) */
    r = rs_initmgr(rs_flags, 1);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init res-mgr");
        return RET_FAIL;
    }
    rs_set_dataalloc(&g_eng->lsr_alloc);

    /* graphics renderer */
    r = gfx_init(&params->gfx);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init gfx");
        return RET_FAIL;
    }

    /* debug HUD */
    r = hud_init(BIT_CHECK(params->flags, ENG_FLAG_CONSOLE));
    if (IS_FAIL(r))	{
        err_print(__FILE__, __LINE__, "engine init failed: could not init debug-hud");
        return RET_FAIL;
    }

    /* Physics */
    if (!BIT_CHECK(params->flags, ENG_FLAG_DISABLEPHX)) {
        r = phx_init(params);
        if (IS_FAIL(r)) {
            err_print(__FILE__, __LINE__, "engine init failed: could not init physics");
            return RET_FAIL;
        }
    }

    /* component manager */
    r = cmp_initmgr();
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init cmp-mgr");
        return RET_FAIL;
    }
    cmp_set_globalalloc(&g_eng->data_alloc, tsk_get_tmpalloc(0));

    /* world manager */
    r = wld_initmgr();
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init world-mgr");
        return RET_FAIL;
    }

    /* scene manager */
    r = scn_initmgr();
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init scene-mgr");
        return RET_FAIL;
    }

    /* init lua */
    r = sct_init(&params->sct, BIT_CHECK(params->flags, ENG_FLAG_DEV) ? TRUE : FALSE);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init script engine");
        return RET_FAIL;
    }

    /* web-server */
#if defined(_PROFILE_)
    r = prf_initmgr();
    if (IS_FAIL(r))	{
    	log_print(LOG_WARNING, "profiler manager init failed: service will not be available");
    	prf_releasemgr();
    }
#endif

    /* lod-scheme */
    r = lod_initmgr();
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: could not init lod-scheme");
        return RET_FAIL;
    }

    /* init basic resources */
    r = rs_init_resources();
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "engine init failed: coult not init res-mgr resources");
        return RET_FAIL;
    }

    /* switch back to normal data allocator */
    rs_set_dataalloc(&g_eng->data_alloc);

    /* enable background-loading if res-mgr is prepared for (see above rs_initmgr) */
    if (gfx_check_feature(GFX_FEATURE_THREADED_CREATES))
        rs_add_flags(RS_FLAG_BGLOADING);
    log_print(LOG_TEXT, "init ok: ready.");

    /* init world vars */
    eng_world_regvars();

    /* engine specific console commnads */
    con_register_cmd("showfps", eng_console_showfps, NULL, "showfps [1*/0]");
    con_register_cmd("showft", eng_console_showft, NULL, "showft [1*/0]");
    con_register_cmd("showgraph", eng_console_showgraph, NULL, "showgraph [ft][fps][drawcalls]");
    con_register_cmd("lockfps", eng_console_lockfps, NULL, "lockfps [fps]");

    /* execute console commands - should be the final stage if initialization */
    if (BIT_CHECK(params->flags, ENG_FLAG_CONSOLE))	{
		for (uint i = 0; i < params->console_cmds_cnt; i++)	{
			con_exec(params->console_cmds + i*128);
		}
    }

    A_LOAD(tmp_alloc);
    return RET_OK;
}

void eng_release()
{
    if (g_eng == NULL)
        return;

    rs_release_resources();

    lod_releasemgr();
#if !defined(_DEBUG_)
    pak_close(&g_eng->data_pak);
#endif
	prf_releasemgr();
    sct_release();
    wld_releasemgr();
    scn_releasemgr();
    cmp_releasemgr();
    phx_release();
    hud_release();
    gfx_release();
    rs_reportleaks();
    rs_releasemgr();
    tsk_releasemgr();

    if (g_eng->timer != NULL)
        timer_destroyinstance(g_eng->timer);

    /* check for main memory leaks */
    if (BIT_CHECK(g_eng->params.flags, ENG_FLAG_DEV))    {
        uint leak_cnt = mem_freelist_getleaks_ts(&g_eng->data_freelist, NULL);
        if (leak_cnt > 0)
            log_printf(LOG_WARNING, "%d leaks found on dynamic 'data' memory", leak_cnt);
    }

    mem_freelist_destroy_ts(&g_eng->data_freelist);
    mem_stack_destroy(&g_eng->lsr_stack);

    log_print(LOG_TEXT, "engine released.");

	if (BIT_CHECK(g_eng->params.flags, ENG_FLAG_CONSOLE))	{
		log_outputfunc(FALSE, NULL, NULL);
		con_release();
	}

    FREE(g_eng);
    g_eng = NULL;
}

void eng_update()
{
    /* variables for fps calculation */
    static float elapsed_tm = 0.0f;
    static uint frame_cnt = 0;
    static int simulated = FALSE;

    /* reset frame stack on start of each frame */
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    A_SAVE(tmp_alloc);

    uint64 start_tick = timer_querytick();
    g_eng->frame_stats.start_tick = start_tick;

    /* check for file changes (dev-mode) */
    if (BIT_CHECK(g_eng->params.flags, ENG_FLAG_DEV))
        fio_mon_update();

    /* update all timers */
    timer_update(start_tick);

    /* do all the work ... */
    float dt = g_eng->timer->dt;

    /* resource manager */
    rs_update();

    /* physics */
    if (!BIT_CHECK(g_eng->params.flags, ENG_FLAG_DISABLEPHX)) {
        if (simulated)
            phx_wait();
        phx_update_xforms(simulated);   /* gather results */
    }

    /* update scripting */
    sct_update();

    /* physics: run simulation */
    if (!BIT_CHECK(g_eng->params.flags, ENG_FLAG_DISABLEPHX))
        simulated = phx_update_sim(dt);

    /* update component system stages */
    PRF_OPENSAMPLE("Component (pre-render)");
    cmp_update(dt, CMP_UPDATE_STAGE1);
    cmp_update(dt, CMP_UPDATE_STAGE2);
    cmp_update(dt, CMP_UPDATE_STAGE3);
    cmp_update(dt, CMP_UPDATE_STAGE4);
    PRF_CLOSESAMPLE();

    /* cull and render active scene */
    PRF_OPENSAMPLE("Render");
    gfx_render();
    PRF_CLOSESAMPLE();

    PRF_OPENSAMPLE("Component (post-render)");
    cmp_update(dt, CMP_UPDATE_STAGE5);
    PRF_CLOSESAMPLE();

    /* clear update list of components */
    cmp_clear_updates();

    /* final frame stats calculation */
    fl64 ft = timer_calctm(start_tick, timer_querytick());
    if (g_eng->fps_lock > 0)  {
        fl64 target_ft = 1.0 / (fl64)g_eng->fps_lock;
        while (ft < target_ft)  {
            ft = timer_calctm(start_tick, timer_querytick());
        }
    }

#if defined(_PROFILE_)
    /* present samples in _PROFILE_ mode */
    prf_presentsamples(ft);
#endif

    g_eng->frame_stats.ft = (float)ft;
    g_eng->frame_stats.frame ++;
    frame_cnt ++;
    elapsed_tm += (float)ft;
    if (elapsed_tm > 1.0f)  {
        g_eng->frame_stats.fps = frame_cnt;
        frame_cnt = 0;
        elapsed_tm = 0.0f;
    }

    A_LOAD(tmp_alloc);
}

const struct frame_stats* eng_get_framestats()
{
	return &g_eng->frame_stats;
}

void eng_send_guimsgs(char c, uint vkey)
{
	hud_send_input(c, input_kb_translatekey(vkey));
}

const struct init_params* eng_get_params()
{
	return &g_eng->params;
}

struct allocator* eng_get_lsralloc()
{
    return &g_eng->lsr_alloc;
}

struct allocator* eng_get_dataalloc()
{
    return &g_eng->data_alloc;
}

void eng_get_memstats(struct eng_mem_stats* stats)
{
    memset(stats, 0x00, sizeof(struct eng_mem_stats));
    if (BIT_CHECK(g_eng->params.flags, ENG_FLAG_OPTIMIZEMEMORY)) {
        stats->data_max = g_eng->data_freelist.fl.size;
        stats->data_size = g_eng->data_freelist.fl.alloc_size;
        stats->lsr_max = g_eng->lsr_stack.size;
        stats->lsr_size = g_eng->lsr_stack.offset;
    }   else    {
        stats->data_max = 0;
        stats->data_size = mem_sizebyid(MID_DATA);
        stats->lsr_max = 0;
        stats->lsr_size = mem_sizebyid(MID_LSR);
    }

    stats->tmp0_total = 0;
    stats->tmp0_max = 0;
    stats->tmp0_max_frameid = 0;
}

void eng_world_regvars()
{
    /* add default world vars */
    struct variant v;

    /* light section */
    uint light_id = wld_register_section("light");
    ASSERT(light_id != 0);
    wld_set_var(light_id, wld_register_var(light_id, "dir", VAR_TYPE_FLOAT3, NULL, NULL),
        var_set3f(&v, 0.0f, -1.0f, 0.5f));
    wld_set_var(light_id, wld_register_var(light_id, "color", VAR_TYPE_FLOAT4, NULL, NULL),
        var_set4fv(&v, g_color_white.f));
    wld_set_var(light_id, wld_register_var(light_id, "intensity", VAR_TYPE_FLOAT, NULL, NULL),
        var_setf(&v, 1.0f));

    /* ambient section */
    uint ambient_id = wld_register_section("ambient");
    ASSERT(ambient_id != 0);
    wld_set_var(ambient_id, wld_register_var(ambient_id, "ground-color", VAR_TYPE_FLOAT4, NULL, NULL),
        var_set4f(&v, 0.29f, 0.45f, 0.32f, 1.0f));
    wld_set_var(ambient_id, wld_register_var(ambient_id, "sky-color", VAR_TYPE_FLOAT4, NULL, NULL),
        var_set4f(&v, 0.5f, 0.5f, 0.5f, 1.0f));
    wld_set_var(ambient_id, wld_register_var(ambient_id, "intensity", VAR_TYPE_FLOAT, NULL, NULL),
        var_setf(&v, 0.2f));
    wld_set_var(ambient_id, wld_register_var(ambient_id, "sky-vector", VAR_TYPE_FLOAT3, NULL, NULL),
        var_set3fv(&v, g_vec3_unity.f));

    /* physics section */
    uint phys_id = wld_register_section("physics");
    ASSERT(phys_id != 0);
    wld_set_var(phys_id,
        wld_register_var(phys_id, "gravity-vector", VAR_TYPE_FLOAT3, phx_setgravity_callback, NULL),
        var_set3f(&v, 0.0f, -9.81f, 0.0f));
}

result_t eng_console_showfps(uint argc, const char** argv, void* param)
{
    int value = TRUE;
    if (argc == 1)
        value = str_tobool(argv[0]);
    if (value)
        hud_add_label("fps", eng_hud_drawfps, NULL);
    else
        hud_remove_label("fps");

    return RET_OK;
}

result_t eng_console_showft(uint argc, const char** argv, void* param)
{
    int value = TRUE;
    if (argc == 1)
        value = str_tobool(argv[0]);
    if (value)
        hud_add_label("ft", eng_hud_drawft, NULL);
    else
        hud_remove_label("ft");

    return RET_OK;
}

int eng_hud_drawfps(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param)
{
    const struct frame_stats* stats = eng_get_framestats();
    char text[32];
    sprintf(text, "fps = %d", stats->fps);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;
    return y;
}

int eng_hud_drawft(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param)
{
    const struct frame_stats* stats = eng_get_framestats();
    char text[32];
    sprintf(text, "ft = %.2f ms", stats->ft*1000.0f);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;
    return y;
}

result_t eng_console_showgraph(uint argc, const char** argv, void* param)
{
    if (argc != 1 && argc != 2)
        return RET_INVALIDARG;
    int show = TRUE;
    if (argc == 2)
        show = str_tobool(argv[1]);

    struct rect2di rc;

    if (str_isequal_nocase(argv[0], "ft"))	{
        if (show)   {
            hud_add_graph("graph-ft", eng_hud_drawftgraph, ui_create_graphline("ft", 0, 16.0f, 60,
                *rect2di_seti(&rc, 0, 0, GRAPH_WIDTH, GRAPH_HEIGHT), FALSE), NULL);
        }   else    {
            hud_remove_graph("graph-ft");
        }
    }
    else if (str_isequal_nocase(argv[0], "fps"))	{
        if (show)   {
            hud_add_graph("graph-fps", eng_hud_drawfpsgraph,
                ui_create_graphline("fps", 0, 60.0f, 60,
                *rect2di_seti(&rc, 0, 0, GRAPH_WIDTH, GRAPH_HEIGHT), TRUE),NULL);
        }   else    {
            hud_remove_graph("graph-fps");
        }
    }
    else if (str_isequal_nocase(argv[0], "drawcalls"))	{
        if (show)   {
            hud_add_graph("graph-drawcall", eng_hud_drawcallgraph,
                ui_create_graphline("drawcalls", 0, 100.0f, 60,
                *rect2di_seti(&rc, 0, 0, GRAPH_WIDTH, GRAPH_HEIGHT), TRUE), NULL);
        }   else    {
            hud_remove_graph("graph-drawcall");
        }
    }
    else    {
        return RET_INVALIDARG;
    }

    return RET_OK;
}

int eng_hud_drawfpsgraph(gfx_cmdqueue cmdqueue, ui_widget widget, int x, int y, int update,
    void* param)
{
    const struct frame_stats* stats = eng_get_framestats();

    if (update)
        ui_graphline_addvalue(widget, (float)stats->fps);

    ui_widget_move(widget, x, y);
    ui_widget_draw(widget);

    return y + GRAPH_HEIGHT;
}

int eng_hud_drawftgraph(gfx_cmdqueue cmdqueue, ui_widget widget, int x, int y, int update,
    void* param)
{
    const struct frame_stats* stats = eng_get_framestats();
    if (update)
        ui_graphline_addvalue(widget, stats->ft*1000.0f);

    ui_widget_move(widget, x, y);
    ui_widget_draw(widget);

    return y + GRAPH_HEIGHT;
}

int eng_hud_drawcallgraph(gfx_cmdqueue cmdqueue, ui_widget widget, int x, int y,
    int update, void* param)
{
    const struct gfx_framestats* stats = gfx_get_framestats(cmdqueue);
    if (update)
        ui_graphline_addvalue(widget, (float)stats->draw_cnt);
    ui_widget_move(widget, x, y);
    ui_widget_draw(widget);

    return y + GRAPH_HEIGHT;
}

const struct hwinfo* eng_get_hwinfo()
{
    return &g_eng->hwinfo;
}

result_t eng_console_lockfps(uint argc, const char** argv, void* param)
{
    if (argc != 1)
        return RET_INVALIDARG;

    g_eng->fps_lock = clampui(str_toint32(argv[0]), 0, 1000);
    return RET_OK;
}

void eng_pause()
{
    timer_pauseall();
}


void eng_resume()
{
    timer_resumeall();
}

const char* eng_get_sharedir()
{
    return g_eng->share_dir;
}

float eng_get_frametime()
{
    return g_eng->frame_stats.ft;
}