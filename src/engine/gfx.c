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
#include "dhcore/core.h"
#include "dhcore/task-mgr.h"
#include "dhcore/hwinfo.h"

#include "dhapp/app.h"

#include "gfx.h"
#include "gfx-device.h"
#include "gfx-cmdqueue.h"
#include "gfx-shader.h"
#include "gfx-font.h"
#include "gfx-canvas.h"
#include "debug-hud.h"
#include "mem-ids.h"
#include "scene-mgr.h"
#include "camera.h"
#include "cmp-mgr.h"
#include "prf-mgr.h"
#include "gfx-model.h"
#include "engine.h"
#include "console.h"
#include "gfx-occ.h"
#include "gfx-billboard.h"
#include "res-mgr.h"
#include "world-mgr.h"

#include "renderpaths/gfx-fwd.h"
#include "renderpaths/gfx-deferred.h"
#include "renderpaths/gfx-csm.h"
#include "gfx-postfx.h"

#define TONEMAP_DEFAULT_MIDGREY 0.5f
#define TONEMAP_DEFAULT_LUM_MIN 0.1f
#define TONEMAP_DEFAULT_LUM_MAX 1.0f
#define OCC_BUFFER_SIZE 128

/*************************************************************************************************
 * structs/types
 */
/* renderpass sub represents a set of batches and a render-path to render them */
struct gfx_renderpass_sub
{
	const struct gfx_rpath* rpath;	/* render-path used for rendering this pass */
	struct hashtable_chained shader_table; /* key: shader_id, value: index to batch_items */
    struct array batch_items;   /* item: gfx_batch_item */
};

/* renderpass is a collection of renderpass_sub. each sub includes a render-path and a full batch */
struct gfx_renderpass
{
    struct array subpasses; /* item: gfx_renderpass_sub */
    struct gfx_rpath_result result; /* resulting textures/data from all render-paths */
    void* userdata;
};

struct gfx_rpath_ref
{
	uint rpath_idx;
	uint obj_type_comb;
	uint rpath_flags;
};

struct gfx_transparent_item
{
	enum cmp_obj_type objtype;
	uint query_idx;	/* index for scn_render_query item (dependent on type) */
	uint sub_idx;	/* sub-index inside item */
	float z;
};

struct gfx_cull_stats
{
    uint prim_model_cnt;
    uint prim_light_cnt;
    uint csm_model_cnt;
};

struct gfx_fs_vertex
{
    struct vec3f pos;
    struct vec3f coord;
};

struct gfx_renderer
{
    /* note: width, height defined in 'params' will change in runtime
     * width, height represents current active display-target dimensions */
	struct gfx_params params;
	gfx_cmdqueue cmdqueue;  /* default (immediate) command-queue */
	pfn_debug_render debug_render_fn;
	struct array rpaths;	/* item: gfx_rpath */
	struct array rpath_refs;	/* item: gfx_rpath_ref */
    gfx_sampler global_sampler; /* mostly used for diffuse-maps and normal-maps */
    gfx_sampler global_sampler_low; /* mostly used for other maps like specular and ambient */
    struct gfx_renderpass* passes[GFX_RENDERPASS_MAX];

    gfx_buffer fs_vbuff;    /* fullscreen quad vertex-buffer */
    gfx_inputlayout fs_il;  /* fullscreen quad input-layout */
    struct gfx_cull_stats cull_stats;
    struct gfx_pfx_tonemap* tonemap;    /* tonemap postfx */
    struct gfx_pfx_fxaa* fxaa;  /* fxaa postfx */

    uint composite_shaderid;
    gfx_sampler sampl_lin;
    gfx_sampler sampl_point;
    gfx_depthstencilstate ds_always;
    int show_worldbounds;

    reshandle_t tex_blank_black;
    int preview_render;

    struct gfx_device_info info;
    int rtv_width;
    int rtv_height;
};

/*************************************************************************************************
 * fwd
 */
result_t gfx_rpath_register(uint obj_type_comb, uint rpath_flags, const struct gfx_rpath* rpath);
int gfx_register_renderpaths();
result_t gfx_rpath_init();
void gfx_rpath_release();
result_t gfx_rpath_init_registered();
void gfx_rpath_release_registered();
gfx_sampler gfx_create_sampler_fromtexfilter(enum texture_filter filter);

/* batching and renderpasses
 * note that allocators are all assumed as stack allocator, where there is not need for destroy*/
/* first, renderpasses are created and process renderpass routines are called by render loop */
struct gfx_renderpass* gfx_renderpass_create(struct allocator* alloc);
/* primary pass provides render data and send it to additem for further processing
 * @param trans_items: item is gfx_transparent_item
 * @param trans_idxs: item is uint (index to trans_items) */
void gfx_renderpass_process_primary(struct allocator* alloc, const struct frustum* frust,
    struct array* trans_items, struct array* trans_idxs, const struct gfx_view_params* params);
/* for each pass processing, there are items that are need to be added and batched
 * each pass includes a number of subpasses which share the same render-path
 * 'add_item' checks for these subpasses and creates them if required then calls additem_tosubpass*/
void gfx_renderpass_additem(struct allocator* alloc, struct gfx_renderpass* rpass,
    enum cmp_obj_type objtype, uint unique_id, struct gfx_renderpass_item* rpass_item,
    void* ritem, uint sub_idx, const struct mat3f* tmat);

/* subpasses are inside render-passes, each subpass include a render-path and a batching root
 * first, it searches inside a subpass for existing shader batch, if not found, creates ...
 * a new shader batch item (type: gfx_batch_item) and add it to the batches
 * then searches for unique_id (for instancing) inside each batch_item. if not found, creates it,
 * and add a new batch-node (type: gfx_batch_node) for each instancing batch or use existing batch*/
void gfx_renderpass_additem_tosubpass(struct allocator* alloc, struct gfx_renderpass_sub* rpdata,
    enum cmp_obj_type objtype, uint unique_id, struct gfx_renderpass_item* rpass_item,
    void* ritem, uint sub_idx, const struct mat3f* tmat);

/* add and sort transparent items for further processing
 * @param trans_items: item is gfx_transparent_item
 * @param trans_idxs: item is uint (index to trans_items)
 */
void gfx_renderpass_additem_transparent(struct scn_render_query* query, enum cmp_obj_type objtype,
		uint bounds_idx, uint query_idx, uint sub_idx,
		struct array* trans_items, struct array* trans_idxs,
		const struct mat3f* view);

void gfx_renderpass_process_sunshadow(struct allocator* alloc, const struct gfx_view_params* params);

/* finally process render passes renders all (batched) passes by order */
void gfx_process_renderpasses(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
		const struct gfx_view_params* params);

/* data creation/allocation routines for batching/passes */
struct gfx_renderpass* gfx_renderpass_create(struct allocator* alloc);
result_t gfx_renderpass_initsubdata(struct allocator* alloc, struct gfx_renderpass_sub* rpdata,
    const struct gfx_rpath* rpath);
void gfx_batch_inititem(struct allocator* alloc, struct gfx_batch_item* bitem,
    enum cmp_obj_type objtype, uint shader_id);
void gfx_batch_initnode(struct allocator* alloc, struct gfx_batch_node* bnode, uint unique_id,
    uint sub_idx, void* ritem);

result_t gfx_create_fullscreenquad();
void gfx_destroy_fullscreenquad();

/* console commands */
result_t gfx_console_showcullinfo(uint argc, const char** argv, void* param);
result_t gfx_console_showdrawinfo(uint argc, const char** argv, void* param);
result_t gfx_console_showbounds(uint argc, const char** argv, void* param);
int gfx_hud_rendercullinfo(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param);
int gfx_hud_renderdrawinfo(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param);

/* composite */
result_t gfx_composite_init();
void gfx_composite_release();
void gfx_composite_render(gfx_cmdqueue cmdqueue, gfx_texture src_tex, gfx_texture depth_tex,
    gfx_texture add1_tex);

void gfx_render_blank(gfx_cmdqueue cmdqueue, int width, int height);

/*************************************************************************************************
 * globals
 */
static struct gfx_renderer g_gfx;

/*************************************************************************************************
 * inlines
 */
INLINE void gfx_rpath_set(struct gfx_rpath_ref* r, struct gfx_rpath_ref* r0)
{
	r->rpath_idx = r0->rpath_idx;
	r->obj_type_comb = r0->obj_type_comb;
	r->rpath_flags = r0->rpath_flags;
}

INLINE void gfx_rpath_refswap(struct gfx_rpath_ref* r1, struct gfx_rpath_ref* r2)
{
	struct gfx_rpath_ref tmp;
	gfx_rpath_set(&tmp, r1);
	gfx_rpath_set(r1, r2);
	gfx_rpath_set(r2, &tmp);
}


/*************************************************************************************************/
void gfx_zero()
{
	memset(&g_gfx, 0x00, sizeof(g_gfx));
	gfx_zerodev();
	gfx_shader_zero();
	gfx_font_zero();
	gfx_canvas_zero();
	gfx_font_zero();
    gfx_occ_zero();
    gfx_blb_zero();
    g_gfx.tex_blank_black = INVALID_HANDLE;
}

result_t gfx_init(const struct gfx_params* params)
{
	result_t r;

    log_print(LOG_TEXT, "init gfx ...");
	memcpy(&g_gfx.params, params, sizeof(struct gfx_params));

	/* initialize device and cmd-queue */
	r = gfx_initdev(params);
	if (IS_FAIL(r))	{
		err_print(__FILE__, __LINE__, "gfx-init failed: could not initilialize device");
		return RET_FAIL;
	}

    /* print info */
    gfx_get_devinfo(&g_gfx.info);
    const struct gfx_device_info* info = &g_gfx.info;
    log_print(LOG_INFO, "  graphics:");
    char ver[32];
    switch (gfx_get_hwver())        {
        case GFX_HWVER_D3D10_0:     strcpy(ver, "d3d10.0");     break;
        case GFX_HWVER_D3D10_1:     strcpy(ver, "d3d10.1");     break;
        case GFX_HWVER_D3D11_0:     strcpy(ver, "d3d11.0");     break;
        case GFX_HWVER_GL3_2:       strcpy(ver, "GL3.2");       break;
        case GFX_HWVER_GL3_3:       strcpy(ver, "GL3.3");       break;
        case GFX_HWVER_GL4_0:       strcpy(ver, "GL4.0");       break;
        case GFX_HWVER_GL4_1:       strcpy(ver, "GL4.1");       break;
        case GFX_HWVER_GL4_2:       strcpy(ver, "GL4.2");       break;
        default:                    strcpy(ver, "unknown");     break;
    }

    log_printf(LOG_INFO, "\tdriver: %s", info->desc);
    log_printf(LOG_INFO, "\tavailable video memory: %d(mb)",
        info->mem_avail/1024);
    log_printf(LOG_INFO, "\tgfx concurrent-creates: %s",
        info->threading.concurrent_create ? "yes" : "no");
    log_printf(LOG_INFO, "\tgfx concurrent-cmdqueues: %s",
        info->threading.concurrent_cmdlist ? "yes" : "no");
    log_printf(LOG_INFO, "\tgfx driver feature: %s", ver);       


    /* shader cache/manager */
    int disable_cache = BIT_CHECK(params->flags, GFX_FLAG_DEBUG) &&
        BIT_CHECK(params->flags, GFX_FLAG_REBUILDSHADERS);
    if (IS_FAIL(gfx_shader_initmgr(disable_cache)))	{
        err_print(__FILE__, __LINE__, "gfx-init failed: could not initialize shader cache");
        return RET_FAIL;
    }

    /* default cmdqueue */
    g_gfx.cmdqueue = gfx_create_cmdqueue();
	if (g_gfx.cmdqueue == NULL || IS_FAIL(gfx_initcmdqueue(g_gfx.cmdqueue)))    {
		err_print(__FILE__, __LINE__, "gfx-init failed: could not initilialize command-queue");
		return RET_FAIL;
	}
    gfx_set_wndsize((int)params->width, (int)params->height);

	/* font-manager */
	if (IS_FAIL(gfx_font_initmgr()))	{
		err_print(__FILE__, __LINE__, "gfx-init failed: could not initialize font-mgr");
		return RET_FAIL;
	}

	/* canvas */
	if (IS_FAIL(gfx_canvas_init()))	{
		err_print(__FILE__, __LINE__, "gfx-init failed: could not initialize canvas");
		return RET_FAIL;
	}

	/* render path manager */
	if (IS_FAIL(gfx_rpath_init()) || !gfx_register_renderpaths())	{
		err_printf(__FILE__, __LINE__, "gfx-init failed: could not initialize render-path system");
		return RET_FAIL;
	}
    /* initialize render-paths */
    if (IS_FAIL(gfx_rpath_init_registered()))   {
        err_printf(__FILE__, __LINE__, "gfx-init failed: could not initialize render-paths");
        return RET_FAIL;
    }

    /* global sampler which is used for models and common textures */
    g_gfx.global_sampler = gfx_create_sampler_fromtexfilter(params->tex_filter);
    g_gfx.global_sampler_low = gfx_create_sampler_fromtexfilter(
        params->tex_filter != TEXTURE_FILTER_BILINEAR ? TEXTURE_FILTER_TRILINEAR :
        TEXTURE_FILTER_BILINEAR);
    if (g_gfx.global_sampler == NULL || g_gfx.global_sampler_low == NULL)   {
        err_printf(__FILE__, __LINE__, "gfx-init failed: could not create global samplers");
        return RET_FAIL;
    }

    /* fullscreen quad */
    if (IS_FAIL(gfx_create_fullscreenquad()))   {
        err_print(__FILE__, __LINE__, "gfx-init failed: could not create fullscreen quad");
        return RET_FAIL;
    }

    /* postfx */
    g_gfx.tonemap = gfx_pfx_tonemap_create(params->width, params->height,
        TONEMAP_DEFAULT_MIDGREY, TONEMAP_DEFAULT_LUM_MIN, TONEMAP_DEFAULT_LUM_MAX, TRUE);
    if (g_gfx.tonemap == NULL)  {
        err_print(__FILE__, __LINE__, "gfx-init failed: could not create tonemap postfx");
        return RET_FAIL;
    }

    if (BIT_CHECK(params->flags, GFX_FLAG_FXAA))    {
        g_gfx.fxaa = gfx_pfx_fxaa_create(params->width, params->height);
        if (g_gfx.fxaa == NULL) {
            err_print(__FILE__, __LINE__, "gfx-init failed: could not create fxaa postfx");
            return RET_FAIL;
        }
    }

    /* composite */
    if (IS_FAIL(gfx_composite_init()))  {
        err_print(__FILE__, __LINE__, "gfx-init failed: could not create composite");
        return RET_FAIL;
    }

    /* occlusion culling */
    if (IS_FAIL(gfx_occ_init(OCC_BUFFER_SIZE, OCC_BUFFER_SIZE, eng_get_hwinfo()->cpu_caps)))    {
        err_print(__FILE__, __LINE__, "gfx-init failed: could not create occlusion culling");
        return RET_FAIL;
    }

    /* billboard renderer */
    if (IS_FAIL(gfx_blb_init()))    {
        err_print(__FILE__, __LINE__, "gfx-init failed: could not create billboard renderer");
        return RET_FAIL;
    }

    /* blank textures */
    g_gfx.tex_blank_black = rs_load_texture("textures/black1x1.dds", 0, FALSE, 0);
    if (g_gfx.tex_blank_black == INVALID_HANDLE)    {
        err_print(__FILE__, __LINE__, "gfx-init failed: could not load blank textures");
        return RET_FAIL;
    }

    /* renderer console commands */
    con_register_cmd("gfx_cullinfo", gfx_console_showcullinfo, NULL, "gfx_cullinfo [1*/0]");
    con_register_cmd("gfx_drawinfo", gfx_console_showdrawinfo, NULL, "gfx_drawinfo [1*/0]");
    con_register_cmd("gfx_showbounds", gfx_console_showbounds, NULL, "gfx_showbounds [1*/0]");

    gfx_flush(gfx_get_cmdqueue(0));

	return RET_OK;
}

void gfx_release()
{
    if (g_gfx.tex_blank_black != INVALID_HANDLE)
        rs_unload(g_gfx.tex_blank_black);

    gfx_blb_release();

    gfx_occ_release();

    gfx_composite_release();

    if (g_gfx.tonemap != NULL)
        gfx_pfx_tonemap_destroy(g_gfx.tonemap);
    if (g_gfx.fxaa != NULL)
        gfx_pfx_fxaa_destroy(g_gfx.fxaa);

    gfx_destroy_fullscreenquad();

    if (g_gfx.global_sampler_low != NULL)
        gfx_destroy_sampler(g_gfx.global_sampler_low);
    if (g_gfx.global_sampler != NULL)
        gfx_destroy_sampler(g_gfx.global_sampler);

    gfx_rpath_release_registered();

	gfx_rpath_release();

	gfx_canvas_release();

	gfx_font_releasemgr();

	if (g_gfx.cmdqueue != NULL)		{
		gfx_releasecmdqueue(g_gfx.cmdqueue);
		gfx_destroy_cmdqueue(g_gfx.cmdqueue);
	}

    gfx_shader_releasemgr();

    gfx_releasedev();
	gfx_zero();

	log_print(LOG_TEXT, "gfx released.");
}

void gfx_render()
{
	int width = (int)g_gfx.rtv_width;
	int height = (int)g_gfx.rtv_height;

	float widthf = (float)width;
	float heightf = (float)height;
    gfx_cmdqueue cmdqueue = g_gfx.cmdqueue;
    struct array trans_items;
    struct array trans_idxs;
    struct gfx_view_params params;
    struct frustum viewfrust;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    struct gfx_renderpass* rpass;
    result_t r;

    g_gfx.preview_render = FALSE;

    /* we have no camera, just render blank screen */
    struct camera* cam = wld_get_cam();

	/* reset */
    PRF_OPENSAMPLE("render");
    memset(&g_gfx.cull_stats, 0x00, sizeof(struct gfx_cull_stats));

	gfx_reset_framestats(cmdqueue);
    gfx_reset_devstates(cmdqueue);

    /* render */
    params.width = width;
    params.height = height;
    params.cam = cam;
    cam_set_viewsize(cam, widthf, heightf);
    cam_get_perspective(&params.proj, cam);
    cam_get_view(&params.view, cam);
    mat3_mul4(&params.viewproj, &params.view, &params.proj);
    vec3_setv(&params.cam_pos, &cam->pos);
    cam_calc_frustumplanes(viewfrust.planes, &params.viewproj);
    vec4_setf(&params.projparams, params.proj.m11, params.proj.m22,
        params.proj.m33, params.proj.m43);

    /* use frame allocator for main thread (temp) */
    A_SAVE(tmp_alloc);

    /* create transparent objects arrays */
    r = arr_create(tmp_alloc, &trans_items, sizeof(struct gfx_transparent_item), 50, 200, MID_GFX);
    r |= arr_create(tmp_alloc, &trans_idxs, sizeof(uint), 50, 200, MID_GFX);
    ASSERT(IS_OK(r));

    /* create primary pass (note that primary pass is actually rendered last in render passes) */
    PRF_OPENSAMPLE("batch-primary");
    g_gfx.passes[GFX_RENDERPASS_PRIMARY] = gfx_renderpass_create(tmp_alloc);
    gfx_renderpass_process_primary(tmp_alloc, &viewfrust, &trans_items, &trans_idxs, &params);
    PRF_CLOSESAMPLE();

    PRF_OPENSAMPLE("batch-csm");
    g_gfx.passes[GFX_RENDERPASS_SUNSHADOW] = gfx_renderpass_create(tmp_alloc);
    gfx_renderpass_process_sunshadow(tmp_alloc, &params);
    PRF_CLOSESAMPLE();

    gfx_occ_finish(cmdqueue, &params);

    /* process all render passes */
    gfx_process_renderpasses(cmdqueue, NULL, &params);

    /* get possible data from primary render-pass and copy it into backbuffer */
    rpass = g_gfx.passes[GFX_RENDERPASS_PRIMARY];
    if (rpass != NULL && rpass->result.rt != NULL)  {
        gfx_texture bloom_tex;
        gfx_texture ldr_tex;
        if (!g_gfx.preview_render)   {
            /* do additional postfx */

            /* tonemapping */
            ldr_tex = gfx_pfx_tonemap_render(cmdqueue, g_gfx.tonemap, &params,
                (gfx_texture)rpass->result.rt->desc.rt.rt_textures[0], &bloom_tex);

            /* fxaa */
            if (g_gfx.fxaa != NULL) {
                gfx_texture aa_tex = gfx_pfx_fxaa_render(cmdqueue, g_gfx.fxaa, ldr_tex);
                ldr_tex = aa_tex;
            }
        }   else    {
            ldr_tex = (gfx_texture)rpass->result.rt->desc.rt.rt_textures[0];
            bloom_tex = NULL;
        }

        gfx_output_setrendertarget(cmdqueue, NULL);
        gfx_output_setviewportbias(g_gfx.cmdqueue, 0, 0, width, height);
        gfx_composite_render(g_gfx.cmdqueue, ldr_tex,
            (gfx_texture)rpass->result.rt->desc.rt.ds_texture, bloom_tex);
    }   else    {
        gfx_render_blank(cmdqueue, width, height);
    }

    A_LOAD(tmp_alloc);	/* free all memory of culling/batching */

    PRF_CLOSESAMPLE();

    gfx_blb_render(cmdqueue, &params);

    /* debug render */
    PRF_OPENSAMPLE("debug-render");
    gfx_canvas_begin3d(cmdqueue, widthf, heightf, &params.viewproj);
    /* component debug render */
    cmp_debug(0.0f, &params);

    /* additional debug draw ? */
    if (g_gfx.show_worldbounds) {
        struct vec3f wmin, wmax;
        scn_getsize(scn_getactive(), &wmin, &wmax);
        gfx_canvas_setalpha(0.5f);
        gfx_canvas_worldbounds(&wmin, &wmax, 10.0f);
        gfx_canvas_setalpha(1.0f);
    }

    if (g_gfx.debug_render_fn != NULL)
		g_gfx.debug_render_fn(g_gfx.cmdqueue, &params);
    gfx_canvas_end3d();
    PRF_CLOSESAMPLE();

    PRF_OPENSAMPLE("2d-render");
	hud_render(g_gfx.cmdqueue);
	gfx_canvas_render2d(g_gfx.cmdqueue, NULL, widthf, heightf);
    PRF_CLOSESAMPLE();
}

void gfx_set_debug_renderfunc(pfn_debug_render fn)
{
	g_gfx.debug_render_fn = fn;
}

void gfx_set_wndsize(int width, int height)
{
    gfx_set_rtvsize(width, height);
    g_gfx.params.width = width;
    g_gfx.params.height = height;
}

void gfx_get_wndsize(OUT int* width, OUT int* height)
{
    *width = g_gfx.params.width;
    *height = g_gfx.params.height;
}

/* update size varialbes for device/cmdqueue */
void gfx_set_rtvsize(int width, int height)
{
    g_gfx.rtv_width = width;
    g_gfx.rtv_height = height;
}

void gfx_get_rtvsize(OUT int* width, OUT int* height)
{
    *width = g_gfx.rtv_width;
    *height = g_gfx.rtv_height;
}

gfx_cmdqueue gfx_get_cmdqueue(uint id)
{
	if (id == 0)
		return g_gfx.cmdqueue;
	else
		return NULL;
}

int gfx_register_renderpaths()
{
	result_t r;
    uint rpflags;

	struct gfx_rpath rpath;
	memset(&rpath, 0x00, sizeof(rpath));

    /* deferred render-path */
    rpath.name = "deferred";
    rpath.init_fn = gfx_deferred_init;
    rpath.release_fn = gfx_deferred_release;
    rpath.getshader_fn = gfx_deferred_getshader;
    rpath.render_fn = gfx_deferred_render;
    rpath.resize_fn = gfx_deferred_resize;

    rpflags = GFX_RPATH_RAW | GFX_RPATH_ALPHAMAP | GFX_RPATH_DIFFUSEMAP | GFX_RPATH_NORMALMAP |
        GFX_RPATH_SKINNED;
    r = gfx_rpath_register(CMP_OBJTYPE_MODEL, rpflags, &rpath);
    if (IS_FAIL(r))
        return FALSE;

    /* csm */
    rpath.name = "csm";
    rpath.init_fn = gfx_csm_init;
    rpath.release_fn = gfx_csm_release;
    rpath.getshader_fn = gfx_csm_getshader;
    rpath.render_fn = gfx_csm_render;
    rpath.resize_fn = gfx_csm_resize;
    r = gfx_rpath_register(CMP_OBJTYPE_MODEL, rpflags | GFX_RPATH_CSMSHADOW, &rpath);
    if (IS_FAIL(r))
        return FALSE;

	return TRUE;
}

result_t gfx_rpath_init()
{
	result_t r;

	r = arr_create(mem_heap(), &g_gfx.rpaths, sizeof(struct gfx_rpath), 5, 5, MID_GFX);
	if (IS_FAIL(r))
		return RET_OUTOFMEMORY;

	r = arr_create(mem_heap(), &g_gfx.rpath_refs, sizeof(struct gfx_rpath_ref), 5, 5, MID_GFX);
	if (IS_FAIL(r))
		return RET_OUTOFMEMORY;

	return RET_OK;
}

void gfx_rpath_release()
{
	arr_destroy(&g_gfx.rpaths);
	arr_destroy(&g_gfx.rpath_refs);
}


result_t gfx_rpath_register(uint obj_type_comb, uint rpath_flags, const struct gfx_rpath* rpath)
{
	/* check current rpath-rpath_refs and see if we have existing rpath that support obj_type
	 * only one render-path is allowed to render a specific object type/flag combination
	 */
	for (uint i = 0, cnt = g_gfx.rpath_refs.item_cnt; i < cnt; i++)	{
		struct gfx_rpath_ref* ref = &((struct gfx_rpath_ref*)g_gfx.rpath_refs.buffer)[i];
		if ((ref->rpath_flags & rpath_flags) == rpath_flags && (ref->obj_type_comb & obj_type_comb))
        {
			err_printf(__FILE__, __LINE__,
					"rpath-register failed: obj/flag combination already exists");
			return RET_FAIL;
		}
	}

	/* add to rpaths */
	uint rp_idx = g_gfx.rpaths.item_cnt;
	struct gfx_rpath* prp = (struct gfx_rpath*)arr_add(&g_gfx.rpaths);
	if (prp == NULL)
		return RET_OUTOFMEMORY;
	memcpy(prp, rpath, sizeof(struct gfx_rpath));

	/* add to rpath rpath_refs */
	struct gfx_rpath_ref* ref = (struct gfx_rpath_ref*)arr_add(&g_gfx.rpath_refs);
	if (ref == NULL)
		return RET_OUTOFMEMORY;

	ref->obj_type_comb = obj_type_comb;
	ref->rpath_flags = rpath_flags;
	ref->rpath_idx = rp_idx;

	/* re-order rpath-rpath_refs in order to search for CMP_OBJTYPE_UNKNOWN after everything else
	 * use simple insertion sort for this purpose
	 */
	struct gfx_rpath_ref* rpath_refs = (struct gfx_rpath_ref*)g_gfx.rpath_refs.buffer;
	for (uint i = 1, cnt = g_gfx.rpath_refs.item_cnt; i < cnt; i++)	{
		struct gfx_rpath_ref* ref1 = &rpath_refs[i];
		struct gfx_rpath_ref* ref2 = &rpath_refs[i-1];
		uint hole_idx = i;

		while (hole_idx > 0 && (ref1->obj_type_comb < ref2->obj_type_comb))	{
			gfx_rpath_refswap(ref1, ref2);
			hole_idx --;
			ref1 = &rpath_refs[hole_idx];
			ref2 = &rpath_refs[hole_idx - 1];
		}
	}

	return RET_OK;
}

const struct gfx_rpath* gfx_rpath_detect(enum cmp_obj_type obj_type, uint rpath_flags)
{
	for (uint i = 0, cnt = g_gfx.rpath_refs.item_cnt; i < cnt; i++)	{
		struct gfx_rpath_ref* ref = &((struct gfx_rpath_ref*)g_gfx.rpath_refs.buffer)[i];
		if ((ref->rpath_flags & rpath_flags) == rpath_flags &&
			((ref->obj_type_comb & (uint)obj_type) || ref->obj_type_comb == 0 /*unknown*/))
		{
			return &((struct gfx_rpath*)g_gfx.rpaths.buffer)[i];
		}
	}
	/* no render-path found to render specific object/flag ?! */
	return NULL;
}

result_t gfx_rpath_init_registered()
{
    result_t r;
    uint width = g_gfx.params.width;
    uint height = g_gfx.params.height;

    for (uint i = 0; i < g_gfx.rpaths.item_cnt; i++)  {
        struct gfx_rpath* rpath = &((struct gfx_rpath*)g_gfx.rpaths.buffer)[i];
        r = rpath->init_fn(width, height);
        if (IS_FAIL(r)) {
            err_printf(__FILE__, __LINE__, "gfx-init failed: could not init render-path '%s'",
                rpath->name);
            return RET_FAIL;
        }
    }

    return RET_OK;
}

void gfx_rpath_release_registered()
{
    for (uint i = 0; i < g_gfx.rpaths.item_cnt; i++) {
        struct gfx_rpath* rpath = &((struct gfx_rpath*)g_gfx.rpaths.buffer)[i];
        rpath->release_fn();
    }
}

gfx_sampler gfx_create_sampler_fromtexfilter(enum texture_filter filter)
{
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    switch (filter) {
    case TEXTURE_FILTER_TRILINEAR:
        sdesc.filter_mag = GFX_FILTER_LINEAR;
        sdesc.filter_min = GFX_FILTER_LINEAR;
        sdesc.filter_mip = GFX_FILTER_LINEAR;
        break;
    case TEXTURE_FILTER_BILINEAR:
        sdesc.filter_mag = GFX_FILTER_LINEAR;
        sdesc.filter_min = GFX_FILTER_LINEAR;
        sdesc.filter_mip = GFX_FILTER_NEAREST;
        break;
    case TEXTURE_FILTER_ANISO2X:
        sdesc.filter_mag = GFX_FILTER_LINEAR;
        sdesc.filter_min = GFX_FILTER_LINEAR;
        sdesc.filter_mip = GFX_FILTER_LINEAR;
        sdesc.aniso_max = 2;
        break;
    case TEXTURE_FILTER_ANISO4X:
        sdesc.filter_mag = GFX_FILTER_LINEAR;
        sdesc.filter_min = GFX_FILTER_LINEAR;
        sdesc.filter_mip = GFX_FILTER_LINEAR;
        sdesc.aniso_max = 4;
        break;
    case TEXTURE_FILTER_ANISO8X:
        sdesc.filter_mag = GFX_FILTER_LINEAR;
        sdesc.filter_min = GFX_FILTER_LINEAR;
        sdesc.filter_mip = GFX_FILTER_LINEAR;
        sdesc.aniso_max = 8;
        break;
    case TEXTURE_FILTER_ANISO16X:
        sdesc.filter_mag = GFX_FILTER_LINEAR;
        sdesc.filter_min = GFX_FILTER_LINEAR;
        sdesc.filter_mip = GFX_FILTER_LINEAR;
        sdesc.aniso_max = 16;
        break;
    }
    return gfx_create_sampler(&sdesc);
}

gfx_sampler gfx_get_globalsampler()
{
    return g_gfx.global_sampler;
}

gfx_sampler gfx_get_globalsampler_low()
{
    return g_gfx.global_sampler_low;
}

/* note: we assume that 'alloc' is stack allocator */
struct gfx_renderpass* gfx_renderpass_create(struct allocator* alloc)
{
    result_t r;
    struct gfx_renderpass* rpass = (struct gfx_renderpass*)A_ALLOC(alloc,
        sizeof(struct gfx_renderpass), MID_GFX);
    ASSERT(rpass);
    r = arr_create(alloc, &rpass->subpasses, sizeof(struct gfx_renderpass_sub),
        2*GFX_RENDERPASS_MAX, GFX_RENDERPASS_MAX, MID_GFX);
    if (IS_FAIL(r))
        return NULL;
    return rpass;
}

void gfx_renderpass_process_primary(struct allocator* alloc, const struct frustum* frust,
    struct array* trans_items, struct array* trans_idxs, const struct gfx_view_params* params)
{
    struct gfx_renderpass* pass = g_gfx.passes[GFX_RENDERPASS_PRIMARY];

    /* cull scene by frustum */
    uint scene_id = scn_getactive();
    if (scene_id == 0)
        return;

    struct scn_render_query* query = scn_create_query(scene_id, alloc, frust->planes, params, 0);
    ASSERT(query != NULL);

    /* cull stats */
    g_gfx.cull_stats.prim_model_cnt = query->model_cnt;
    g_gfx.cull_stats.prim_light_cnt = query->light_cnt;

	/* models */
	for (uint i = 0, cnt = query->model_cnt; i < cnt; i++)	{
		struct scn_render_model* rmodel = &query->models[i];
		struct gfx_model* gmodel = rmodel->gmodel;
		struct gfx_model_instance* inst = rmodel->inst;

		struct gfx_model_node* mnode = &gmodel->nodes[rmodel->node_idx];
		struct gfx_model_mesh* mmesh = &gmodel->meshes[mnode->mesh_id];

		for (uint k = 0, kcnt = mmesh->submesh_cnt; k < kcnt; k++)	{
			struct gfx_model_submesh* submesh = &mmesh->submeshes[k];
			uint mtl_idx = submesh->mtl_id;
			struct gfx_model_mtl* mtl = &gmodel->mtls[mtl_idx];
			struct gfx_model_mtlgpu* gmtl = inst->mtls[mtl_idx];

			/* TODO: create additional passes for mirros/reflection if required by material */

			/* check if we have any rpath to render the object
             * if not try puting it into TRANSPARENT pass
             */
			if (gmtl->passes[GFX_RENDERPASS_PRIMARY].rpath != NULL) {
				gfx_renderpass_additem(alloc,
                        pass,
						CMP_OBJTYPE_MODEL,
						inst->unique_ids[submesh->offset_idx],
						&gmtl->passes[GFX_RENDERPASS_PRIMARY],
                        rmodel,
                        k,
                        &query->mats[rmodel->mat_idx]);
			}	else if (gmtl->passes[GFX_RENDERPASS_TRANSPARENT].rpath != NULL &&
					BIT_CHECK(mtl->flags, GFX_MODEL_MTLFLAG_TRANSPARENT))
			{
				/* add to transparent objects for further processing */
				gfx_renderpass_additem_transparent(query, CMP_OBJTYPE_MODEL, rmodel->bounds_idx,
						i, k, trans_items, trans_idxs, &params->view);
			}   else    {
                ASSERT(0);
            }
		}
	}   /* endfor: models */

    /* pass lights as userdata for primary pass */
    struct gfx_renderpass_lightdata* ldata = (struct gfx_renderpass_lightdata*)A_ALLOC(alloc,
        sizeof(struct gfx_renderpass_lightdata), MID_GFX);
    if (ldata != NULL) {
        ldata->cnt = query->light_cnt;
        ldata->lights = query->lights;
        ldata->bounds = query->bounds;
        pass->userdata = ldata;
    }

}

void gfx_renderpass_process_sunshadow(struct allocator* alloc, const struct gfx_view_params* params)
{
    struct vec3f sun_dir;
    struct aabb world_bounds;
    uint sec_light = wld_find_section("light");
    const float* world_sundir = wld_get_var(sec_light, wld_find_var(sec_light, "dir"))->fs;
    struct vec3f world_min, world_max;

    uint scene_id = scn_getactive();
    if (scene_id == 0)
        return;
    scn_getsize(scene_id, &world_min, &world_max);

    vec3_setf(&sun_dir, world_sundir[0], world_sundir[1], world_sundir[2]);
    aabb_setv(&world_bounds, &world_min, &world_max);
    struct gfx_renderpass* pass = g_gfx.passes[GFX_RENDERPASS_SUNSHADOW];

    /* calculate csm shadow stuff like matrices and frustum bounds */
    gfx_csm_prepare(params, vec3_norm(&sun_dir, &sun_dir), &world_bounds);
    const struct aabb* frust_bounds = gfx_csm_get_frustumbounds();

    /* shadow csm cull */
    struct scn_render_query* rq =
        scn_create_query_csm(scn_getactive(), alloc, frust_bounds, &sun_dir, params);
    ASSERT(rq != NULL);
    g_gfx.cull_stats.csm_model_cnt = rq->model_cnt;

    for (uint i = 0, cnt = rq->model_cnt; i < cnt; i++)   {
 		struct scn_render_model* rmodel = &rq->models[i];
		struct gfx_model* gmodel = rmodel->gmodel;
		struct gfx_model_instance* inst = rmodel->inst;

		struct gfx_model_node* mnode = &gmodel->nodes[rmodel->node_idx];
		struct gfx_model_mesh* mmesh = &gmodel->meshes[mnode->mesh_id];

        /* make unique-id from geometry(pointer) only (no material/texture data needed) */
#if defined(_X64_)
        uint unique_id = hash_u64((uint64)&gmodel->geos[mmesh->geo_id]);
#else
        uint unique_id = (uint)&gmodel->geos[mmesh->geo_id];
#endif

        /* check if whole model has alpha materials, then treat all-solid object differently
         * all-solid objects are drawn with one call
         * alpha objects are divided into solid and alpha sub-materials */
        if (!inst->alpha_flags[rmodel->node_idx])   {
            /* treat whole mesh as 1 render object (no sub-idx)
             * so get first submaterial as rpath and shader_id */
            struct gfx_model_submesh* submesh = &mmesh->submeshes[0];
            struct gfx_model_mtlgpu* gmtl = inst->mtls[submesh->mtl_id];

            if (gmtl->passes[GFX_RENDERPASS_SUNSHADOW].rpath != NULL) {
                gfx_renderpass_additem(alloc,
                    pass,
                    CMP_OBJTYPE_MODEL,
                    unique_id,
                    &gmtl->passes[GFX_RENDERPASS_SUNSHADOW],
                    rmodel,
                    INVALID_INDEX,  /* pass invalid_index to perform whole mesh draw */
                    &rq->mats[rmodel->mat_idx]);
            }
        }   else    {
		    for (uint k = 0, kcnt = mmesh->submesh_cnt; k < kcnt; k++)	{
			    struct gfx_model_submesh* submesh = &mmesh->submeshes[k];
			    struct gfx_model_mtlgpu* gmtl = inst->mtls[submesh->mtl_id];

			    /* check if we have any rpath to render the object */
			    if (gmtl->passes[GFX_RENDERPASS_SUNSHADOW].rpath != NULL) {
				    gfx_renderpass_additem(alloc,
                            pass,
						    CMP_OBJTYPE_MODEL,
						    unique_id,
						    &gmtl->passes[GFX_RENDERPASS_SUNSHADOW],
                            rmodel,
                            k,
                            &rq->mats[rmodel->mat_idx]);
			    }
		    }   /* endfor: submeshes */
        }
	}   /* endfor: models */
}

void gfx_renderpass_additem(struct allocator* alloc,
    struct gfx_renderpass* rpass,
    enum cmp_obj_type objtype,
    uint unique_id,
	struct gfx_renderpass_item* rpass_item,
    void* ritem,
    uint sub_idx,
    const struct mat3f* tmat)
{
    /* search for rpath and try to find it in current pass data
     * if not, create a new pass data
     */
    const struct gfx_rpath* rpath = rpass_item->rpath;
    for (uint i = 0, cnt = rpass->subpasses.item_cnt; i < cnt; i++)   {
        struct gfx_renderpass_sub* rpdata =
            &((struct gfx_renderpass_sub*)rpass->subpasses.buffer)[i];
        if (rpdata->rpath == rpath) {
            gfx_renderpass_additem_tosubpass(alloc, rpdata, objtype, unique_id, rpass_item,
                ritem, sub_idx, tmat);
            return;
        }
    }

    /* rpath not found, create a new subpass */
    struct gfx_renderpass_sub* rpdata = (struct gfx_renderpass_sub*)arr_add(&rpass->subpasses);
    ASSERT(rpdata);
    gfx_renderpass_initsubdata(alloc, rpdata, rpath);
    gfx_renderpass_additem_tosubpass(alloc, rpdata, objtype, unique_id, rpass_item, ritem,
        sub_idx, tmat);
}

/* note: we assume that 'alloc' is stack allocator and memzero'd */
result_t gfx_renderpass_initsubdata(struct allocator* alloc, struct gfx_renderpass_sub* rpdata,
    const struct gfx_rpath* rpath)
{
    result_t r;

    rpdata->rpath = rpath;
    r = arr_create(alloc, &rpdata->batch_items, sizeof(struct gfx_batch_item), 20, 40, MID_GFX);
    r |= hashtable_chained_create(alloc, alloc, &rpdata->shader_table, 64, MID_GFX);
    return r;
}

/**
 * batching algorithm:
 * data:
 * batch(shader_id #1) --> batch_node(unique_id #1)/subidx --> linked_list(instances)
 *                      batch_node(unique_id #2)/subidx --> linked_list(instances)
 * batch(shader_id #2) --> batch_node(unique_id #1)/subidx --> linked_list(instances)
 *                      batch_node(unique_id #2)/subidx --> linked_list(instances)
 * method: incoming item ...
 *   1) first we search look in shader table, search for shader_id, if not found, create new batch
 *   2) look in batch's unique_id table, if not found, create a new empty batch_node (see data), else ...
 *   3) add new batch_node to batch_item's nodes
 *   4) if the batch_node exists, check if we reach instance_cnt limit in the linked_list of nodes
 *   5) if there is no room for more items, add new batch_node to the linked_list of the first batch node
 */
void gfx_renderpass_additem_tosubpass(struct allocator* alloc,
    struct gfx_renderpass_sub* rpdata,
    enum cmp_obj_type objtype,
    uint unique_id,
    struct gfx_renderpass_item* rpass_item,
    void* ritem,
    uint sub_idx,
    const struct mat3f* tmat)
{
    uint shader_id = rpass_item->shader_id;

    /* find shader-id in the batches */
    struct hashtable_item_chained* item = hashtable_chained_find(&rpdata->shader_table, shader_id);
    struct gfx_batch_item* bitem;
    if (item != NULL)   {
        bitem = &((struct gfx_batch_item*)rpdata->batch_items.buffer)[item->value];
    }   else    {
        /* not found: add new batch item */
        bitem = (struct gfx_batch_item*)arr_add(&rpdata->batch_items);
        ASSERT(bitem);
        gfx_batch_inititem(alloc, bitem, objtype, shader_id);
        hashtable_chained_add(&rpdata->shader_table, shader_id, rpdata->batch_items.item_cnt-1);
    }

    /* find unique-id (batch-node) in the batch-item */
    struct hashtable_item_chained* subitem = hashtable_chained_find(&bitem->uid_table, unique_id);
    struct gfx_batch_node* bnode;
    if (subitem != NULL)    {
        struct gfx_batch_node* bnode_first = (struct gfx_batch_node*)subitem->value;
        /* the first bnode in the list, is always the last node that we added, so ...
         * check for instance_cnt of the last_node and see if we need more bnodes */
        bnode = (bnode_first->bll != NULL) ? (struct gfx_batch_node*)bnode_first->bll->data :
            bnode_first;

        if (bnode->instance_cnt >= GFX_INSTANCES_MAX)   {
        	struct gfx_batch_node* bnode_new = (struct gfx_batch_node*)
                A_ALLOC(alloc, sizeof(struct gfx_batch_node), MID_GFX);
        	ASSERT(bnode_new);
            gfx_batch_initnode(alloc, bnode_new, unique_id, sub_idx, ritem);
            list_add(&bnode_first->bll, &bnode_new->lnode, bnode_new);
            bnode = bnode_new;
        }
    }   else    {
    	/* this is the first item, add it to the batches and the list */
        bnode = (struct gfx_batch_node*)arr_add(&bitem->nodes);
        ASSERT(bnode);
        gfx_batch_initnode(alloc, bnode, unique_id, sub_idx, ritem);
        hashtable_chained_add(&bitem->uid_table, unique_id, (uptr_t)bnode);
        list_add(&bnode->bll, &bnode->lnode, bnode);
    }

    /* add an instance to the batch */
    uint idx = bnode->instance_cnt;
    bnode->instance_mats[idx] = tmat;
    if (objtype == CMP_OBJTYPE_MODEL)
        bnode->poses[idx] = ((struct scn_render_model*)ritem)->pose;

    bnode->instance_cnt++;
}

/* note: we assume that 'alloc' is stack allocator */
void gfx_batch_inititem(struct allocator* alloc, struct gfx_batch_item* bitem,
    enum cmp_obj_type objtype, uint shader_id)
{
    bitem->objtype = objtype;
    bitem->shader_id = shader_id;
    hashtable_chained_create(alloc, alloc, &bitem->uid_table, 512, MID_GFX);
    arr_create(alloc, &bitem->nodes, sizeof(struct gfx_batch_node), 128, 256, MID_GFX);
}

/* note: we assume that 'alloc' is stack allocator */
void gfx_batch_initnode(struct allocator* alloc, struct gfx_batch_node* bnode, uint unique_id,
    uint sub_idx, void* ritem)
{
    bnode->instance_cnt = 0;
    bnode->bll = NULL;
    bnode->lnode.data = NULL;
    bnode->lnode.next = NULL;
    bnode->lnode.prev = NULL;

    bnode->unique_id = unique_id;
    bnode->sub_idx = sub_idx;
    bnode->ritem = ritem;
    bnode->poses[0] = NULL;
}

void gfx_process_renderpasses(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
		const struct gfx_view_params* params)
{
	for (uint i = 0; i < GFX_RENDERPASS_MAX; i++)	{
		struct gfx_renderpass* rpass = g_gfx.passes[i];
		if (rpass != NULL)	{
			/* go through subpasses and pass them to their render-paths */
			for (uint k = 0; k < rpass->subpasses.item_cnt; k++)	{
				struct gfx_renderpass_sub* subpass =
						&((struct gfx_renderpass_sub*)rpass->subpasses.buffer)[k];
				subpass->rpath->render_fn(cmdqueue, rt,
                    (const struct gfx_view_params*)params,
                    (struct gfx_batch_item*)subpass->batch_items.buffer,
                    subpass->batch_items.item_cnt, rpass->userdata, &rpass->result);
			}
		}
	}
}

void gfx_renderpass_additem_transparent(struct scn_render_query* query, enum cmp_obj_type objtype,
		uint bounds_idx, uint query_idx, uint sub_idx,
		struct array* trans_items, struct array* trans_idxs,
		const struct mat3f* view)
{
	struct sphere* s = &query->bounds[bounds_idx];

	struct gfx_transparent_item* titem = (struct gfx_transparent_item*)arr_add(trans_items);
	ASSERT(titem);

	titem->objtype = objtype;
	titem->query_idx = query_idx;
	titem->sub_idx = sub_idx;
	titem->z = s->x*view->m13 + s->y*view->m23 + s->z*view->m33 + view->m43;

	uint* pidx = (uint*)arr_add(trans_idxs);
	ASSERT(pidx);
	*pidx = trans_items->item_cnt - 1;

	/* sort index list, by Z of each transparent item
	 * we use a simplified version of insertion sort, good for small count of objects
	 */
	struct gfx_transparent_item* items = (struct gfx_transparent_item*)trans_items->buffer;
	uint* idxs = (uint*)trans_idxs->buffer;
	uint item_cnt = trans_idxs->item_cnt;
	uint i = item_cnt - 1;

	while (i > 0 && items[idxs[i-1]].z < items[idxs[i]].z)	{
		swapui(&idxs[i-1], &idxs[i]);
		i--;
	}
}

const char* gfx_rpath_getflagstr(uint rpath_flags)
{
    static char str[128];
    str[0] = 0;

    if (BIT_CHECK(rpath_flags, GFX_RPATH_RAW))
        strcat(str, "(raw)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_DIFFUSEMAP))
        strcat(str, "(diffusemap)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_NORMALMAP))
        strcat(str, "(normalmap)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_REFLECTIONMAP))
        strcat(str, "(reflectionmap)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_EMISSIVEMAP))
        strcat(str, "(emissivemap)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_GLOSSMAP))
        strcat(str, "(glossmap)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_ALPHABLEND))
        strcat(str, "(alphablend)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_CSMSHADOW))
        strcat(str, "(csmshadow)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_SKINNED))
        strcat(str, "(skinned)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_SPOTSHADOW))
        strcat(str, "(spotshadow)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_POINTSHADOW))
        strcat(str, "(pointshadow)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_ISOTROPIC))
        strcat(str, "(isotropic)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_SSS))
        strcat(str, "(sss)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_WATER))
        strcat(str, "(water)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_MIRROR))
        strcat(str, "(mirror)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_PARTICLE))
        strcat(str, "(particle)");
    if (BIT_CHECK(rpath_flags, GFX_RPATH_ALPHAMAP))
        strcat(str, "(alphatest)");

    return str;
}

result_t gfx_create_fullscreenquad()
{
    /* note: z is 1.0f (max) for use in position reconstruction too */
#ifdef _GNUC_
    const struct gfx_fs_vertex verts[] = {
        {{.x=1.0f, .y=1.0f, .z=1.0f, .w=1.0f}, {.x=1.0f, .y=0.0f}},
        {{.x=-1.0f, .y=1.0f, .z=1.0f, .w=1.0f}, {.x=0.0f, .y=0.0f}},
        {{.x=1.0f, .y=-1.0f, .z=1.0f, .w=1.0f}, {.x=1.0f, .y=1.0f}},
        {{.x=-1.0f, .y=-1.0f, .z=1.0f, .w=1.0f}, {.x=0.0f, .y=1.0f}}
    };
#else
    const struct gfx_fs_vertex verts[] = {
        {{1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
    };
#endif

    g_gfx.fs_vbuff = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC,
        sizeof(struct gfx_fs_vertex)*4, verts, 0);
    if (g_gfx.fs_vbuff == NULL)
        return RET_FAIL;

    const struct gfx_input_element_binding inputs[] = {
        {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
        {GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
    };

    const struct gfx_input_vbuff_desc vbuffs[] = {
        {sizeof(struct gfx_fs_vertex), g_gfx.fs_vbuff}
    };

    g_gfx.fs_il = gfx_create_inputlayout(vbuffs, GFX_INPUTVB_GETCNT(vbuffs),
        inputs, GFX_INPUT_GETCNT(inputs), NULL, GFX_INDEX_UNKNOWN, 0);
    if (g_gfx.fs_il == NULL)
        return RET_FAIL;

    return RET_OK;
}

void gfx_destroy_fullscreenquad()
{
    if (g_gfx.fs_il != NULL)
        gfx_destroy_inputlayout(g_gfx.fs_il);

    if (g_gfx.fs_vbuff != NULL)
        gfx_destroy_buffer(g_gfx.fs_vbuff);
}

void gfx_draw_fullscreenquad()
{
    gfx_input_setlayout(g_gfx.cmdqueue, g_gfx.fs_il);
    gfx_draw(g_gfx.cmdqueue, GFX_PRIMITIVE_TRIANGLESTRIP, 0, 4, GFX_DRAWCALL_POSTFX);
}

result_t gfx_console_showcullinfo(uint argc, const char** argv, void* param)
{
    int show = TRUE;
    if (argc == 1)
        show = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    if (show)
        hud_add_label("cullinfo", gfx_hud_rendercullinfo, NULL);
    else
        hud_remove_label("cullinfo");

    return RET_OK;
}

int gfx_hud_rendercullinfo(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param)
{
    char str[128];
    sprintf(str, "[gfx:primary] models: %d", g_gfx.cull_stats.prim_model_cnt);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "[gfx:primary] lights: %d", g_gfx.cull_stats.prim_light_cnt);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "[gfx:shadowcsm] models: %d", g_gfx.cull_stats.csm_model_cnt);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    return y;
}

void gfx_resize(uint width, uint height)
{
    if (g_gfx.params.width == width && g_gfx.params.height == height)
        return;

    gfx_set_wndsize((int)width, (int)height);

    if (g_gfx.rpaths.item_cnt == 0)
        return;

    /* apply resizing to all render-paths */
    result_t r;
    for (uint i = 0; i < g_gfx.rpaths.item_cnt; i++)  {
        struct gfx_rpath* rpath = &((struct gfx_rpath*)g_gfx.rpaths.buffer)[i];
        r = rpath->resize_fn(width, height);
        if (IS_FAIL(r)) {
            log_printf(LOG_WARNING, "could not resize render-path '%s' to %dx%d", rpath->name,
                width, height);
        }
    }

    /* resize postfx */
    gfx_pfx_tonemap_resize(g_gfx.tonemap, width, height);
    if (g_gfx.fxaa != NULL)
        gfx_pfx_fxaa_resize(g_gfx.fxaa, width, height);
}

result_t gfx_composite_init()
{
    const struct gfx_input_element_binding bindings[] = {
        {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
        {GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
    };
    const struct gfx_shader_define defines[] = {
        {"_DEPTHBUFFER_", "1"},
        {"_ADD1_", "1"}
    };
    g_gfx.composite_shaderid = gfx_shader_load("composite", eng_get_lsralloc(), "shaders/fsq.vs",
        "shaders/composite.ps", NULL, bindings, 2, defines, 2, NULL);
    if (g_gfx.composite_shaderid == 0)
        return RET_FAIL;

    /* states */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.address_u = GFX_ADDRESS_CLAMP;
    sdesc.address_v = GFX_ADDRESS_CLAMP;
    sdesc.filter_min = GFX_FILTER_NEAREST;
    sdesc.filter_mag = GFX_FILTER_NEAREST;
    sdesc.filter_mip = GFX_FILTER_UNKNOWN;
    g_gfx.sampl_point = gfx_create_sampler(&sdesc);
    if (g_gfx.sampl_point == NULL)
        return RET_FAIL;

    sdesc.filter_min = GFX_FILTER_LINEAR;
    sdesc.filter_mag = GFX_FILTER_LINEAR;
    sdesc.filter_mip = GFX_FILTER_UNKNOWN;
    g_gfx.sampl_lin = gfx_create_sampler(&sdesc);
    if (g_gfx.sampl_lin == NULL)
        return RET_FAIL;

    struct gfx_depthstencil_desc dsdesc;
    memcpy(&dsdesc, gfx_get_defaultdepthstencil(), sizeof(dsdesc));
    dsdesc.depth_enable = TRUE;
    dsdesc.depth_write = TRUE;
    dsdesc.depth_func = GFX_CMP_ALWAYS;
    g_gfx.ds_always = gfx_create_depthstencilstate(&dsdesc);
    if (g_gfx.ds_always == NULL)
        return RET_FAIL;

    return RET_OK;
}

void gfx_composite_release()
{
    if (g_gfx.ds_always != NULL)
        gfx_destroy_depthstencilstate(g_gfx.ds_always);
    if (g_gfx.sampl_lin != NULL)
        gfx_destroy_sampler(g_gfx.sampl_lin);
    if (g_gfx.sampl_point != NULL)
        gfx_destroy_sampler(g_gfx.sampl_point);
    if (g_gfx.composite_shaderid != 0)
        gfx_shader_unload(g_gfx.composite_shaderid);
}

void gfx_composite_render(gfx_cmdqueue cmdqueue, gfx_texture src_tex, gfx_texture depth_tex,
    gfx_texture add1_tex)
{
    struct gfx_shader* shader = gfx_shader_get(g_gfx.composite_shaderid);
    gfx_output_setdepthstencilstate(cmdqueue, g_gfx.ds_always, 0);
    gfx_shader_bind(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_color), g_gfx.sampl_point,
        src_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), g_gfx.sampl_point,
        depth_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_add1), g_gfx.sampl_lin,
        add1_tex != NULL ? add1_tex : rs_get_texture(g_gfx.tex_blank_black));
    gfx_draw_fullscreenquad();
    gfx_output_setdepthstencilstate(cmdqueue, NULL, 0);
}

result_t gfx_console_showdrawinfo(uint argc, const char** argv, void* param)
{
    int show = TRUE;
    if (argc == 1)
        show = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    if (show)
        hud_add_label("drawinfo", gfx_hud_renderdrawinfo, NULL);
    else
        hud_remove_label("drawinfo");

    return RET_OK;
}

result_t gfx_console_showbounds(uint argc, const char** argv, void* param)
{
    int show = TRUE;
    if (argc == 1)
        show = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    g_gfx.show_worldbounds = show;
    return RET_OK;
}

int gfx_hud_renderdrawinfo(gfx_cmdqueue cmdqueue, int x, int y, int line_stride, void* param)
{
    const struct gfx_framestats* s = gfx_get_framestats(g_gfx.cmdqueue);

    char str[128];
    sprintf(str, "drawcalls: %d", s->draw_cnt);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "tri-cnt: %dk", s->prims_cnt/1000);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "[gbuffer] drawcalls: %d", s->draw_group_cnt[GFX_DRAWCALL_GBUFFER]);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "[gbuffer] tri-cnt: %dk", s->draw_prim_cnt[GFX_DRAWCALL_GBUFFER]/1000);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "[shadow] drawcalls: %d", s->draw_group_cnt[GFX_DRAWCALL_SUNSHADOW]);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "[shadow] tri-cnt: %dk", s->draw_prim_cnt[GFX_DRAWCALL_SUNSHADOW]/1000);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "shader-switch: %d", s->shaderchange_cnt);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "render-target-switch: %d", s->rtchange_cnt);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    sprintf(str, "map-cnt: %d", s->map_cnt);
    gfx_canvas_text2dpt(str, x, y, 0);
    y += line_stride;

    return y;
}

const struct gfx_params* gfx_get_params()
{
    return &g_gfx.params;
}

void gfx_render_blank(gfx_cmdqueue cmdqueue, int width, int height)
{
    struct color fill;
    color_setf(&fill, 0.3f, 0.3f, 0.3f, 1.0f);

    gfx_output_setrendertarget(cmdqueue, NULL);
    gfx_output_setviewport(g_gfx.cmdqueue, 0, 0, width, height);
    gfx_output_clearrendertarget(cmdqueue, NULL, fill.f, 1.0f, 0, GFX_CLEAR_COLOR |
        GFX_CLEAR_DEPTH | GFX_CLEAR_STENCIL);
}

void gfx_render_grid(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params)
{
    gfx_canvas_setlinecolor(&g_color_white);

    gfx_canvas_setztest(TRUE);
    struct mat3f center_mat;
    mat3_setidentity(&center_mat);
    gfx_canvas_grid(5.0f, 70.0f, params->cam);
    gfx_canvas_setztest(FALSE);
    gfx_canvas_coords(&center_mat, &params->cam_pos, 0.5f);
}

void gfx_set_gridcallback(int enable)
{
    if (enable)
        gfx_set_debug_renderfunc(gfx_render_grid);
    else
        gfx_set_debug_renderfunc(NULL);
}

void gfx_set_previewrenderflag()
{
    g_gfx.preview_render = TRUE;
}
