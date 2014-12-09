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

#include <smmintrin.h>

#include "dhcore/core.h"
#include "dhcore/hash-table.h"
#include "dhcore/linked-list.h"
#include "dhcore/prims.h"
#include "dhcore/array.h"
#include "dhcore/hash-table.h"
#include "dhcore/task-mgr.h"

#include "renderpaths/gfx-deferred.h"
#include "renderpaths/gfx-csm.h"

#include "gfx.h"
#include "gfx-shader.h"
#include "gfx-cmdqueue.h"
#include "gfx-model.h"
#include "engine.h"
#include "mem-ids.h"
#include "scene-mgr.h"
#include "gfx-device.h"
#include "console.h"
#include "scene-mgr.h"
#include "camera.h"
#include "gfx-canvas.h"
#include "cmp-mgr.h"
#include "prf-mgr.h"
#include "gfx-postfx.h"
#include "debug-hud.h"
#include "gfx-billboard.h"
#include "res-mgr.h"
#include "world-mgr.h"

#include "components/cmp-light.h"

#define DEFERRED_GBUFFER_SHADERCNT 32
#define DEFERRED_PREVIEW_SHADERCNT 7
#define DEFERRED_LIGHT_SHADERCNT 32

/* light shader IDs */
#define DEFERRED_LIGHTSHADER_SUN 0
#define DEFERRED_LIGHTSHADER_LOCAL 1

/* gbuffer texture IDs */
#define DEFERRED_GBUFFER_ALBEDO 0
#define DEFERRED_GBUFFER_NORMAL 1
#define DEFERRED_GBUFFER_MTL 2
#define DEFERRED_GBUFFER_EXTRA 3

#define DEFERRED_MTLS_MAX 4096
#define DEFERRED_LIGHTS_PERPASS_MAX 28
#define DEFERRED_LIGHTS_MAX 512
#define DEFERRED_LIGHTS_TILES_MAX 512

#define DEFERRED_TILE_SIZE 64
#define DEFERRED_HSEED 8572

/* SSAO */
#define SSAO_DEFAULT_RADIUS 0.1f
#define SSAO_DEFAULT_BIAS 0.25f
#define SSAO_DEFAULT_SCALE 1.0f
#define SSAO_DEFAULT_INTENSITY 3.0f

/*************************************************************************************************
 * types
 */
struct deferred_shader
{
    uint rpath_flags;
    uint shader_id;
};

enum deferred_shader_group
{
    DEFERRED_SHADERGROUP_GBUFFER,
    DEFERRED_SHADERGROUP_LIGHT
};

struct deferred_light
{
    float type[4];  /* only index 0 is set to light type */
    struct vec4f pos_vs;
    struct vec4f atten;
    struct vec3f dir_vs;
    struct color color;
};

struct deferred_shader_tile
{
    uint cnt[4];
    uint idxs[DEFERRED_LIGHTS_PERPASS_MAX];
};

struct deferred_tiles
{
    uint cnt; /* tile count */
    uint cnt_x;
    uint cnt_y;
    struct vec4f* simd_data; /* screen-space SIMD friendly tile rects (count = (tile_count)*2) */
    struct deferred_shader_tile* light_lists;   /* light index list for each tile */
    struct rect2di* rects;  /* tile rectangles */
    struct hashtable_open light_table;  /* key: light cmp handle, value: index to lights array */
    uint light_cnt;   /* keep track of current light count */
};

struct ALIGN16 deferred_tile_vertex
{
    struct vec3f pos;
    struct vec2f coord;
};

struct gfx_deferred
{
    struct gfx_cblock* cb_frame;
    struct gfx_cblock* cb_xforms;
    struct gfx_cblock* tb_mtls;
    struct gfx_cblock* tb_lights;
    struct gfx_cblock* cb_light;
    struct gfx_cblock* tb_skins;

    uint width;
    uint height;

    gfx_depthstencilstate ds_gbuff;
    gfx_depthstencilstate ds_nodepth_stest;
    gfx_blendstate blend_add;
    gfx_rasterstate rs_scissor;
    gfx_sampler sampl_point;

    gfx_rendertarget gbuff;
    gfx_texture gbuff_tex[4];
    gfx_texture gbuff_depthtex;

    uint gbuff_shadercnt;
    struct deferred_shader gbuff_shaders[DEFERRED_GBUFFER_SHADERCNT];

    uint light_shadercnt;
    struct deferred_shader light_shaders[DEFERRED_LIGHT_SHADERCNT];

    uint prev_shaders[DEFERRED_PREVIEW_SHADERCNT];
    gfx_rendertarget prev_rt;
    gfx_rendertarget prev_rt_result;
    gfx_texture prev_tex;

    gfx_rendertarget lit_rt;
    gfx_texture lit_tex;
    gfx_rendertarget lit_rt_result;

    struct hashtable_open mtable;  /* index to mtls */
    struct array mtls;  /* item: gfx_cblock (raw - no gpu_buffer) */

    enum gfx_deferred_preview_mode prev_mode;
    int debug_tiles;

    struct deferred_tiles tiles;
    gfx_buffer tile_buff;   /* item: deferred_tile_vertex */
    gfx_inputlayout tile_il;

    struct gfx_pfx_downsample* downsample;  /* downsample (width-depth) postfx */
    struct gfx_pfx_ssao* ssao;  /* ssao postfx */
    struct gfx_pfx_upsample* upsample; /* upsample postfx */
    struct gfx_pfx_shadow* shadowcsm; /* csm shadow postfx */

    gfx_texture ssao_result_tmp;    /* temp saved for preview upsampled ssao buffer */
    reshandle_t light_tex;

    struct gfx_sharedbuffer* gbuff_sharedbuff;
};

/*************************************************************************************************
 * fwd declarations
 */
int deferred_load_gbuffer_shaders(struct allocator* alloc);
int deferred_addshader(uint shader_id, uint rpath_flags, enum deferred_shader_group group);
void deferred_unload_gbuffer_shaders();

void deferred_submit_batchdata(gfx_cmdqueue cmdqueue, struct gfx_batch_item* batch_items,
                               uint batch_cnt, struct gfx_sharedbuffer* shared_buff);
void deferred_preparebatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader, OUT struct gfx_model_geo** pgeo, OUT uint* psubset_idx);
void deferred_drawbatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader, struct gfx_model_geo* geo, uint subset_idx,
    uint xforms_shared_idx);

result_t deferred_creategbuffrt(uint width, uint height);
void deferred_destroygbuffrt();
result_t deferred_createstates();
void deferred_destroystates();

result_t deferred_createprevbuffrt(uint width, uint height);
void deferred_destroyprevbuffrt();
int deferred_load_prev_shaders(struct allocator* alloc);
void deferred_unload_prev_shaders();

void deferred_renderpreview(gfx_cmdqueue cmdqueue, enum gfx_deferred_preview_mode mode,
    const struct gfx_view_params* params);

int deferred_load_light_shaders(struct allocator* alloc);
void deferred_unload_light_shaders();

int deferred_load_light_shaders(struct allocator* alloc);
void deferred_unload_light_shaders();

result_t deferred_createlitrt(uint width, uint height);
void deferred_destroylitrt();

uint deferred_pushmtl(struct gfx_cblock* cb_mtl);
const struct gfx_cblock* deferred_fetchmtl(int);

/* console commands */
result_t deferred_console_setpreview(uint argc, const char** argv, void* param);
result_t deferred_console_setdebugtiles(uint argc, const char** argv, void* param);
result_t deferred_console_defshadowprev(uint argc, const char** argv, void* param);
result_t deferred_console_showssao(uint argc, const char** argv, void* param);

/* gbuffer */
void gfx_deferred_rendergbuffer(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    struct gfx_batch_item* batch_items, uint batch_cnt);

/* tiling/light culling */
result_t deferred_createtilequad();
void deferred_destroytilequad();
result_t deferred_createtiles(struct deferred_tiles* tiles, uint width, uint height);
void deferred_destroytiles(struct deferred_tiles* tiles);
void deferred_cleartiles(struct deferred_tiles* tiles);
struct vec4f* deferred_calc_lightbounds_simd(struct allocator* alloc, const struct sphere* bounds,
    const struct scn_render_light* lights, uint light_cnt, const struct mat3f* view_inv,
    const struct mat4f* viewprojclip);
void deferred_processtiles(struct deferred_tiles* tiles, struct allocator* alloc,
    uint start_idx, uint end_idx, const struct gfx_view_params* params,
    const struct scn_render_light* lights, const struct sphere* bounds, uint light_cnt);

/* lighting */
void deferred_renderlights(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    const struct gfx_renderpass_lightdata* lightdata, gfx_texture ssao_tex,
    gfx_texture shadowcsm_tex);
void deferred_rendersunlight(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    gfx_texture ssao_tex, gfx_texture shadowcsm_tex);
void deferred_renderlocallights(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    const struct gfx_renderpass_lightdata* lightdata);
void deferred_debugtiles(struct deferred_tiles* tiles,
    const struct vec4f* light_rects, uint light_rect_cnt);
void deferred_drawlights(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    const struct gfx_renderpass_lightdata* lightdata);

/*************************************************************************************************
 * globals
 */
static struct gfx_deferred* g_deferred = NULL;

/*************************************************************************************************/
uint gfx_deferred_getshader(enum cmp_obj_type obj_type, uint rpath_flags)
{
    for (uint i = 0, cnt = g_deferred->gbuff_shadercnt; i < cnt; i++)    {
        struct deferred_shader* sh = &g_deferred->gbuff_shaders[i];
        if (rpath_flags == sh->rpath_flags)
            return sh->shader_id;
    }
    return 0;
}

result_t gfx_deferred_init(uint width, uint height)
{
    struct allocator* lsr_alloc = eng_get_lsralloc();
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);

    log_printf(LOG_INFO, "init deferred render-path ...");

    g_deferred = (struct gfx_deferred*)ALLOC(sizeof(struct gfx_deferred), MID_GFX);
    if (g_deferred == NULL)
        return RET_OUTOFMEMORY;
    memset(g_deferred, 0x00, sizeof(struct gfx_deferred));
    g_deferred->width = width;
    g_deferred->height = height;
    g_deferred->light_tex = INVALID_HANDLE;

    log_printf(LOG_INFO, "\tdeferred render-path: loading shaders ...");

    /* gbuffer shaders */
    if (!deferred_load_gbuffer_shaders(lsr_alloc)) {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: loading shaders failed");
        return RET_FAIL;
    }

    /* light shaders */
    if (!deferred_load_light_shaders(lsr_alloc))    {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: loading light shaders");
        return RET_FAIL;
    }

    /* common gbuffer/lighting cblocks */
    uint raw_shaderid = g_deferred->gbuff_shaders[0].shader_id;

    g_deferred->cb_frame = gfx_shader_create_cblock(lsr_alloc, tmp_alloc,
        gfx_shader_get(raw_shaderid), "cb_frame", NULL);

    if (gfx_check_feature(GFX_FEATURE_RANGED_CBUFFERS)) {
        g_deferred->gbuff_sharedbuff =
            gfx_sharedbuffer_create(GFX_DEFAULT_RENDER_OBJ_CNT*GFX_INSTANCES_MAX*48);
        if (g_deferred->gbuff_sharedbuff == NULL)  {
            err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create uniform buffer");
            return RET_FAIL;
        }
    }

    g_deferred->cb_xforms = gfx_shader_create_cblock(lsr_alloc, tmp_alloc,
        gfx_shader_get(raw_shaderid), "cb_xforms", g_deferred->gbuff_sharedbuff);

    g_deferred->tb_mtls = gfx_shader_create_cblock_tbuffer(mem_heap(),
        gfx_shader_get(g_deferred->light_shaders[DEFERRED_LIGHTSHADER_SUN].shader_id), "tb_mtls",
        sizeof(struct vec4f)*5*DEFERRED_MTLS_MAX);
    g_deferred->tb_lights = gfx_shader_create_cblock_tbuffer(mem_heap(),
        gfx_shader_get(g_deferred->light_shaders[DEFERRED_LIGHTSHADER_LOCAL].shader_id), "tb_lights",
        sizeof(struct deferred_light)*DEFERRED_LIGHTS_MAX);
    g_deferred->cb_light = gfx_shader_create_cblock(mem_heap(), tmp_alloc,
        gfx_shader_get(g_deferred->light_shaders[DEFERRED_LIGHTSHADER_LOCAL].shader_id), "cb_light",
        NULL);
    g_deferred->tb_skins = gfx_shader_create_cblock_tbuffer(mem_heap(),
        gfx_shader_get(gfx_deferred_getshader(CMP_OBJTYPE_MODEL, GFX_RPATH_SKINNED | GFX_RPATH_RAW)),
        "tb_skins", sizeof(struct vec4f)*3*GFX_INSTANCES_MAX*GFX_SKIN_BONES_MAX);

    if (g_deferred->cb_frame == NULL || g_deferred->cb_xforms == NULL ||
        g_deferred->tb_mtls == NULL || g_deferred->tb_lights == NULL ||
        g_deferred->cb_light == NULL || g_deferred->tb_skins == NULL)
    {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create cblocks");
        return RET_FAIL;
    }

    /* gbuffer buffers */
    if (IS_FAIL(deferred_creategbuffrt(width, height))) {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create gbuffer textures");
        return RET_FAIL;
    }

    /* light buffers */
    if (IS_FAIL(deferred_createlitrt(width, height)))   {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create light textures");
        return RET_FAIL;
    }

    /* states */
    if (IS_FAIL(deferred_createstates()))   {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create device states");
        return RET_FAIL;
    }

    /* mtl manager */
    if (IS_FAIL(arr_create(mem_heap(), &g_deferred->mtls, sizeof(struct gfx_cblock*),
        100, 200, MID_GFX)))
    {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create mtl buffer");
        return RET_FAIL;
    }
    if (IS_FAIL(hashtable_open_create(mem_heap(), &g_deferred->mtable, 100, 200, MID_GFX))) {
        err_printf(__FILE__, __LINE__, "gfx-deferred init failed: could not create mtable");
        return RET_FAIL;
    }

    /* tiles */
    if (IS_FAIL(deferred_createtiles(&g_deferred->tiles, width, height)))   {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create tiles");
        return RET_FAIL;
    }

    /* post-fx */
    g_deferred->downsample = gfx_pfx_downsamplewdepth_create(width/2, height/2,
        gfxFormat::R16G16_FLOAT, 0);
    if (g_deferred->downsample == NULL) {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create downsample");
        return RET_FAIL;
    }

    g_deferred->ssao = gfx_pfx_ssao_create(width/2, height/2,
        SSAO_DEFAULT_RADIUS, SSAO_DEFAULT_BIAS, SSAO_DEFAULT_SCALE, SSAO_DEFAULT_INTENSITY);
    if (g_deferred->ssao == NULL) {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create ssao");
        return RET_FAIL;
    }

    g_deferred->upsample = gfx_pfx_upsamplebilateral_create(width, height);
    if (g_deferred->upsample == NULL)   {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create upsampleb");
        return RET_FAIL;
    }
    g_deferred->shadowcsm = gfx_pfx_shadowcsm_create(width, height, NULL);
    if (g_deferred->shadowcsm == NULL)  {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create shadowcsm");
        return RET_FAIL;
    }

    /* light texture */
    g_deferred->light_tex = rs_load_texture("textures/light.dds", 0, TRUE, 0);
    if (g_deferred->light_tex == INVALID_HANDLE)  {
        err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not load light texture");
        return RET_FAIL;
    }

    /* debug/preview stuff */
    if (BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))   {
        if (!deferred_load_prev_shaders(lsr_alloc))    {
            err_print(__FILE__, __LINE__,
                "gfx-deferred init failed: could not create preview shaders");
            return RET_FAIL;
        }

        if (IS_FAIL(deferred_createprevbuffrt(width, height))) {
            err_print(__FILE__, __LINE__,
                "gfx-deferred init failed: could not create preview buffer");
            return RET_FAIL;
        }

        /* register console commands */
        con_register_cmd("gfx_showgbuff", deferred_console_setpreview, NULL,
            "gfx_showgbuff [0][albedo][specmul][norm][norm2][depth][mtl][gloss]");
        con_register_cmd("gfx_debugtiles", deferred_console_setdebugtiles, NULL,
            "gfx_debugtiles [1*/0]");
        con_register_cmd("gfx_showssao", deferred_console_showssao, NULL, "gfx_showssao [1*/0]");
    }

    g_deferred->prev_mode = GFX_DEFERRED_PREVIEW_NONE;

    return RET_OK;
}

void gfx_deferred_release()
{
    if (g_deferred != NULL) {
        if (g_deferred->gbuff_sharedbuff != NULL)
            gfx_sharedbuffer_destroy(g_deferred->gbuff_sharedbuff);

        if (g_deferred->light_tex != INVALID_HANDLE)
            rs_unload(g_deferred->light_tex);

        /* postfx */
        if (g_deferred->shadowcsm != NULL)
            gfx_pfx_shadowcsm_destroy(g_deferred->shadowcsm);

        if (g_deferred->upsample != NULL)
            gfx_pfx_upsamplebilateral_destroy(g_deferred->upsample);

        if (g_deferred->downsample != NULL)
            gfx_pfx_downsamplewdepth_destroy(g_deferred->downsample);

        if (g_deferred->ssao != NULL)
            gfx_pfx_ssao_destroy(g_deferred->ssao);

        /* */
        deferred_destroytiles(&g_deferred->tiles);

        arr_destroy(&g_deferred->mtls);
        hashtable_open_destroy(&g_deferred->mtable);

        /* render targets */
        deferred_destroyprevbuffrt();
        deferred_destroylitrt();
        deferred_destroygbuffrt();

        deferred_unload_prev_shaders();

        deferred_destroystates();

        /* cblocks */
        if (g_deferred->cb_frame != NULL)
            gfx_shader_destroy_cblock(g_deferred->cb_frame);
        if (g_deferred->cb_xforms != NULL)
            gfx_shader_destroy_cblock(g_deferred->cb_xforms);
        if (g_deferred->tb_mtls != NULL)
            gfx_shader_destroy_cblock(g_deferred->tb_mtls);
        if (g_deferred->tb_lights != NULL)
            gfx_shader_destroy_cblock(g_deferred->tb_lights);
        if (g_deferred->cb_light != NULL)
            gfx_shader_destroy_cblock(g_deferred->cb_light);
        if (g_deferred->tb_skins != NULL)
            gfx_shader_destroy_cblock(g_deferred->tb_skins);

        /* shaders */
        deferred_unload_light_shaders();
        deferred_unload_gbuffer_shaders();

        FREE(g_deferred);
        g_deferred = NULL;
    }
}

/* userdata: gfx_renderpass_lightdata* */
void gfx_deferred_render(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
    const struct gfx_view_params* params, struct gfx_batch_item* batch_items, uint batch_cnt,
    void* userdata, OUT struct gfx_rpath_result* result)
{
    ASSERT(batch_cnt != 0);

    PRF_OPENSAMPLE("rpath-deferred");

    /* deferred is a primary pass, so 'userdata' is lightdata */
    struct gfx_renderpass_lightdata* ldata = (struct gfx_renderpass_lightdata*)userdata;

    /* reset */
    hashtable_open_clear(&g_deferred->mtable);
    arr_clear(&g_deferred->mtls);

    /* gbuffer */
    gfx_deferred_rendergbuffer(cmdqueue, params, batch_items, batch_cnt);

    /* csm postfx */
    gfx_texture shadowcsm_tex = gfx_pfx_shadowcsm_render(cmdqueue, g_deferred->shadowcsm,
        params, g_deferred->gbuff_depthtex);

    /* downsample / ssao postfx */
    gfx_texture downsample_depthtex;
    gfx_texture downsample_tex = gfx_pfx_downsamplewdepth_render(cmdqueue, g_deferred->downsample,
        params, g_deferred->gbuff_tex[DEFERRED_GBUFFER_EXTRA], g_deferred->gbuff_depthtex,
        &downsample_depthtex);
    gfx_texture ssao_small_tex = gfx_pfx_ssao_render(cmdqueue, g_deferred->ssao, 0, params,
        downsample_depthtex, downsample_tex);
    gfx_texture ssao_tex = gfx_pfx_upsamplebilateral_render(cmdqueue, g_deferred->upsample,
        params, ssao_small_tex, downsample_depthtex, downsample_tex,
        g_deferred->gbuff_depthtex, g_deferred->gbuff_tex[DEFERRED_GBUFFER_EXTRA]);
    g_deferred->ssao_result_tmp = ssao_tex;

    /*********************************************************************************************/
    if (g_deferred->prev_mode == GFX_DEFERRED_PREVIEW_NONE) {
        deferred_renderlights(cmdqueue, params, ldata, ssao_tex, shadowcsm_tex);
        result->rt = g_deferred->lit_rt_result;
    } else  {
        deferred_renderpreview(cmdqueue, g_deferred->prev_mode, params);
        result->rt = g_deferred->prev_rt_result;
    }

    PRF_CLOSESAMPLE();  /* deferred */
}

void gfx_deferred_rendergbuffer(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    struct gfx_batch_item* batch_items, uint batch_cnt)
{
    PRF_OPENSAMPLE("gbuffer");

    int supports_shared_cbuff = gfx_check_feature(GFX_FEATURE_RANGED_CBUFFERS);
    if (supports_shared_cbuff)
        deferred_submit_batchdata(cmdqueue, batch_items, batch_cnt, g_deferred->gbuff_sharedbuff);

    /*********************************************************************************************/
    struct gfx_cblock* cb_frame = g_deferred->cb_frame;
    gfx_cmdqueue_resetsrvs(cmdqueue);
    gfx_output_setrendertarget(cmdqueue, g_deferred->gbuff);
    gfx_output_clearrendertarget(cmdqueue, g_deferred->gbuff, NULL, 1.0f, 0,
        gfxClearFlag::DEPTH | gfxClearFlag::STENCIL);
    /* write value 1 to stencil, if drawn */
    gfx_output_setdepthstencilstate(cmdqueue, g_deferred->ds_gbuff, 1);
    gfx_output_setviewport(cmdqueue, 0, 0, params->width, params->height);

    gfx_cb_set4m(cb_frame, SHADER_NAME(c_viewproj), &params->viewproj);
    gfx_cb_set3m(cb_frame, SHADER_NAME(c_view), &params->view);
    gfx_shader_updatecblock(cmdqueue, cb_frame);

    for (uint i = 0; i < batch_cnt; i++)	{
        struct gfx_batch_item* bitem = &batch_items[i];
        struct gfx_shader* shader = gfx_shader_get(bitem->shader_id);
        ASSERT(shader);
        gfx_shader_bind(cmdqueue, shader);

        /* do not send cb_xforms to shader if we are using shared buffer (bind later before draw) */
        struct gfx_cblock* cbs[3];
        uint xforms_shared_idx;
        if (supports_shared_cbuff)  {
            cbs[0] = g_deferred->cb_frame;
            xforms_shared_idx = 1;
        }   else    {
            cbs[0] = g_deferred->cb_frame;
            cbs[1] = g_deferred->cb_xforms;
            xforms_shared_idx = 2;
        }
        gfx_shader_bindcblocks(cmdqueue, shader, (const struct gfx_cblock**)cbs, xforms_shared_idx);

        /* batch draw */
        for (int k = 0; k < bitem->nodes.item_cnt; k++)	{
            struct gfx_batch_node* bnode_first = &((struct gfx_batch_node*)bitem->nodes.buffer)[k];
            struct gfx_model_geo* geo;
            uint subset_idx;

            deferred_preparebatchnode(cmdqueue, bnode_first, shader, &geo, &subset_idx);
            if (bnode_first->poses[0] != NULL)  {
                gfx_shader_bindcblock_tbuffer(cmdqueue, shader, SHADER_NAME(tb_skins),
                    g_deferred->tb_skins);
            }


            struct linked_list* node = bnode_first->bll;
            while (node != NULL)	{
                struct gfx_batch_node* bnode = (struct gfx_batch_node*)node->data;
                deferred_drawbatchnode(cmdqueue, bnode, shader, geo, subset_idx, xforms_shared_idx);
                node = node->next;
            }
        }	/* for: each batch-item */
    }
    gfx_output_setdepthstencilstate(cmdqueue, NULL, 0);
    PRF_CLOSESAMPLE();  /* gbuffer */
}

/* prepass for submitting per-object (xforms) data to shaders (uniform sharing required)
 * offset/size data will be assigned into meta_data member of each batch */
void deferred_submit_batchdata(gfx_cmdqueue cmdqueue, struct gfx_batch_item* batch_items,
                               uint batch_cnt, struct gfx_sharedbuffer* shared_buff)
{
    gfx_sharedbuffer_reset(g_deferred->gbuff_sharedbuff);
    for (uint i = 0; i < batch_cnt; i++)	{
        struct gfx_batch_item* bitem = &batch_items[i];

        for (int k = 0; k < bitem->nodes.item_cnt; k++)	{
            struct gfx_batch_node* bnode_first =
                &((struct gfx_batch_node*)bitem->nodes.buffer)[k];
            struct linked_list* node = bnode_first->bll;
            while (node != NULL)	{
                struct gfx_batch_node* bnode = (struct gfx_batch_node*)node->data;
                gfx_cb_set3mvp(g_deferred->cb_xforms, SHADER_NAME(c_mats),
                    bnode->instance_mats, bnode->instance_cnt);
                bnode->meta_data = gfx_sharedbuffer_write(shared_buff,
                    cmdqueue, g_deferred->cb_xforms->cpu_buffer, 48*bnode->instance_cnt);
                node = node->next;
            }
        }	/* for: each batch-item */
    }
}

result_t gfx_deferred_resize(uint width, uint height)
{
    result_t r;
    r = gfx_pfx_downsamplewdepth_resize(g_deferred->downsample, width/2, height/2);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = gfx_pfx_ssao_resize(g_deferred->ssao, width/2, height/2);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = gfx_pfx_upsamplebilateral_resize(g_deferred->upsample, width, height);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = gfx_pfx_shadowcsm_resize(g_deferred->shadowcsm, width, height);
    if (IS_FAIL(r))
        return RET_FAIL;

    deferred_destroygbuffrt();
    r = deferred_creategbuffrt(width, height);
    if (IS_FAIL(r))
        return RET_FAIL;

    deferred_destroylitrt();
    r = deferred_createlitrt(width, height);
    if (IS_FAIL(r))
        return RET_FAIL;

    deferred_destroytiles(&g_deferred->tiles);
    deferred_createtiles(&g_deferred->tiles, width, height);

    if (BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))   {
        deferred_destroyprevbuffrt();
        r = deferred_createprevbuffrt(width, height);
        if (IS_FAIL(r))
            return RET_FAIL;
    }

    g_deferred->width = width;
    g_deferred->height = height;

    return RET_OK;
}

int deferred_load_gbuffer_shaders(struct allocator* alloc)
{
    /* gbuffer shaders */
    int r;
    char max_instances_str[8];
    char max_bones_str[8];

    str_itos(max_instances_str, GFX_INSTANCES_MAX);
    str_itos(max_bones_str, GFX_SKIN_BONES_MAX);

    gfx_shader_beginload(alloc, "shaders/df-gbuffer.vs", "shaders/df-gbuffer.ps", NULL,
        2, "shaders/df-common.inc", "shaders/skin.inc");
    /* raw (nothing) */
    r = deferred_addshader(gfx_shader_add("def-raw", 3, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        "_MAX_INSTANCES_", max_instances_str),
        GFX_RPATH_RAW, DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* raw - skinned */
    r = deferred_addshader(gfx_shader_add("def-s", 5, 3,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::BLENDINDEX, "vsi_blendidxs", 1,
        gfxInputElemId::BLENDWEIGHT, "vsi_blendweights", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_SKIN_", "1",
        "_MAX_BONES_", max_bones_str),
        GFX_RPATH_RAW | GFX_RPATH_SKINNED, DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* diffusemap */
    r = deferred_addshader(gfx_shader_add("def-d", 3, 2,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        "_DIFFUSEMAP_", "1",
        "_MAX_INSTANCES_", max_instances_str),
        GFX_RPATH_RAW|GFX_RPATH_DIFFUSEMAP, DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* diffusemap - skinned */
    r = deferred_addshader(gfx_shader_add("def-ds", 5, 4,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::BLENDINDEX, "vsi_blendidxs", 1,
        gfxInputElemId::BLENDWEIGHT, "vsi_blendweights", 1,
        "_MAX_INSTANCES_", max_instances_str, "_DIFFUSEMAP_", "1", "_SKIN_", "1",
        "_MAX_BONES_", max_bones_str),
        GFX_RPATH_RAW|GFX_RPATH_DIFFUSEMAP|GFX_RPATH_SKINNED, DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* diffusemap - skinned - alphamap */
    r = deferred_addshader(gfx_shader_add("def-dsa", 5, 5,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::BLENDINDEX, "vsi_blendidxs", 1,
        gfxInputElemId::BLENDWEIGHT, "vsi_blendweights", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_DIFFUSEMAP_", "1",
        "_SKIN_", "1",
        "_ALPHAMAP_", "1",
        "_MAX_BONES_", max_bones_str),
        GFX_RPATH_RAW|GFX_RPATH_DIFFUSEMAP|GFX_RPATH_SKINNED|GFX_RPATH_ALPHAMAP,
        DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* diffusemap - normalmap */
    r = deferred_addshader(gfx_shader_add("def-dn", 5, 3,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::TANGENT, "vsi_tangent", 1,
        gfxInputElemId::BINORMAL, "vsi_binorm", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_DIFFUSEMAP_", "1",
        "_NORMALMAP_", "1"),
        GFX_RPATH_RAW|GFX_RPATH_DIFFUSEMAP|GFX_RPATH_NORMALMAP,
        DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* diffusemap - normalmap - alphamap */
    r = deferred_addshader(gfx_shader_add("def-dna", 5, 4,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::TANGENT, "vsi_tangent", 1,
        gfxInputElemId::BINORMAL, "vsi_binorm", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_DIFFUSEMAP_", "1",
        "_NORMALMAP_", "1",
        "_ALPHAMAP_", "1"),
        GFX_RPATH_RAW|GFX_RPATH_DIFFUSEMAP|GFX_RPATH_NORMALMAP|GFX_RPATH_ALPHAMAP,
        DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* diffusemap - normalmap - skinned */
    r = deferred_addshader(gfx_shader_add("def-dnsk", 7, 5,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::TANGENT, "vsi_tangent", 2,
        gfxInputElemId::BINORMAL, "vsi_binorm", 2,
        gfxInputElemId::BLENDINDEX, "vsi_blendidxs", 1,
        gfxInputElemId::BLENDWEIGHT, "vsi_blendweights", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_DIFFUSEMAP_", "1",
        "_NORMALMAP_", "1",
        "_SKIN_", "1",
        "_MAX_BONES_", max_bones_str),
        GFX_RPATH_RAW|GFX_RPATH_DIFFUSEMAP|GFX_RPATH_NORMALMAP|GFX_RPATH_SKINNED,
        DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* diffusemap - normalmap - skinned - alphamap */
    r = deferred_addshader(gfx_shader_add("def-dnsa", 7, 6,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::TANGENT, "vsi_tangent", 2,
        gfxInputElemId::BINORMAL, "vsi_binorm", 2,
        gfxInputElemId::BLENDINDEX, "vsi_blendidxs", 1,
        gfxInputElemId::BLENDWEIGHT, "vsi_blendweights", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_DIFFUSEMAP_", "1",
        "_NORMALMAP_", "1",
        "_SKIN_", "1",
        "_ALPHAMAP_", "1",
        "_MAX_BONES_", max_bones_str),
        GFX_RPATH_RAW|GFX_RPATH_DIFFUSEMAP|GFX_RPATH_NORMALMAP|GFX_RPATH_SKINNED|GFX_RPATH_ALPHAMAP,
        DEFERRED_SHADERGROUP_GBUFFER);
    if (!r)   return FALSE;
    /* normalmap */
    r = deferred_addshader(gfx_shader_add("def-n", 5, 2,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::NORMAL, "vsi_norm", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord0", 0,
        gfxInputElemId::TANGENT, "vsi_tangent", 1,
        gfxInputElemId::BINORMAL, "vsi_binorm", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_NORMALMAP_", "1"),
        GFX_RPATH_RAW|GFX_RPATH_NORMALMAP,
        DEFERRED_SHADERGROUP_GBUFFER);
    gfx_shader_endload();
    if (!r)   return FALSE;

    return TRUE;
}

int deferred_addshader(uint shader_id, uint rpath_flags, enum deferred_shader_group group)
{
    if (shader_id == 0)
        return FALSE;

    switch (group)  {
    case DEFERRED_SHADERGROUP_GBUFFER:
        ASSERT(g_deferred->gbuff_shadercnt < DEFERRED_GBUFFER_SHADERCNT);
        g_deferred->gbuff_shaders[g_deferred->gbuff_shadercnt].shader_id = shader_id;
        g_deferred->gbuff_shaders[g_deferred->gbuff_shadercnt].rpath_flags = rpath_flags;
        g_deferred->gbuff_shadercnt ++;
        break;
    case DEFERRED_SHADERGROUP_LIGHT:
        ASSERT(g_deferred->light_shadercnt < DEFERRED_LIGHT_SHADERCNT);
        g_deferred->light_shaders[g_deferred->light_shadercnt].shader_id = shader_id;
        g_deferred->light_shaders[g_deferred->light_shadercnt].rpath_flags = rpath_flags;
        g_deferred->light_shadercnt ++;
    default:
    	break;
    }

    return TRUE;
}

void deferred_unload_gbuffer_shaders()
{
    for (uint i = 0; i < g_deferred->gbuff_shadercnt; i++)
        gfx_shader_unload(g_deferred->gbuff_shaders[i].shader_id);
}

void deferred_preparebatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader, OUT struct gfx_model_geo** pgeo, OUT uint* psubset_idx)
{
    struct scn_render_model* rmodel = (struct scn_render_model*)bnode->ritem;
    struct gfx_model* gmodel = rmodel->gmodel;
    struct gfx_model_instance* inst = rmodel->inst;
    struct gfx_model_mesh* mesh = &gmodel->meshes[gmodel->nodes[rmodel->node_idx].mesh_id];
    struct gfx_model_geo* geo = &gmodel->geos[mesh->geo_id];
    uint mtl_id = mesh->submeshes[bnode->sub_idx].mtl_id;
    uint subset_idx = mesh->submeshes[bnode->sub_idx].subset_id;

    *pgeo = geo;
    *psubset_idx = subset_idx;

    gfx_shader_setui(shader, SHADER_NAME(c_mtlidx), deferred_pushmtl(inst->mtls[mtl_id]->cb));
    gfx_shader_setf(shader, SHADER_NAME(c_gloss), gmodel->mtls[mtl_id].spec_exp);

    gfx_input_setlayout(cmdqueue, geo->inputlayout);
    gfx_model_setmtl(cmdqueue, shader, inst, mtl_id);
    gfx_shader_bindconstants(cmdqueue, shader);
}

void deferred_drawbatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader, struct gfx_model_geo* geo, uint subset_idx,
    uint xforms_shared_idx)
{
    struct gfx_model_geosubset* subset = &geo->subsets[subset_idx];

    /* set transform matrices, bind only for shared mode (we have updated the buffer in a prepass) */
    struct gfx_cblock* cb_xforms = g_deferred->cb_xforms;
    if (cb_xforms->shared_buff != NULL)  {
        sharedbuffer_pos_t pos = bnode->meta_data;
        gfx_shader_bindcblock_shared(cmdqueue, shader, g_deferred->cb_xforms,
            cb_xforms->shared_buff->gpu_buff,
            GFX_SHAREDBUFFER_OFFSET(pos), GFX_SHAREDBUFFER_SIZE(pos), xforms_shared_idx);
    }   else    {
        gfx_cb_set3mvp(cb_xforms, SHADER_NAME(c_mats), bnode->instance_mats, bnode->instance_cnt);
        gfx_shader_updatecblock(cmdqueue, cb_xforms);
    }

    /* skin data */
    if (bnode->poses[0] != NULL)   {
        struct gfx_cblock* tb_skins = g_deferred->tb_skins;
        for (uint i = 0, cnt = bnode->instance_cnt; i < cnt; i++) {
            const struct gfx_model_posegpu* pose = bnode->poses[i];

            gfx_cb_set3mv_offset(tb_skins, 0, pose->skin_mats, pose->mat_cnt,
                i*GFX_SKIN_BONES_MAX*sizeof(struct vec4f)*3);
        }

        gfx_shader_updatecblock(cmdqueue, tb_skins);
    }

    /* draw */
    gfx_draw_indexedinstance(cmdqueue, gfxPrimitiveType::TRIANGLE_LIST, subset->ib_idx, subset->idx_cnt,
        geo->ib_type, bnode->instance_cnt, GFX_DRAWCALL_GBUFFER);
}

result_t deferred_creategbuffrt(uint width, uint height)
{
    /* textures */
    g_deferred->gbuff_tex[DEFERRED_GBUFFER_ALBEDO] =
        gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
    g_deferred->gbuff_tex[DEFERRED_GBUFFER_NORMAL] =
        gfx_create_texturert(width, height, gfxFormat::R16G16_FLOAT, FALSE);
    g_deferred->gbuff_tex[DEFERRED_GBUFFER_MTL] =
        gfx_create_texturert(width, height, gfxFormat::R16G16_UINT, FALSE);
    g_deferred->gbuff_tex[DEFERRED_GBUFFER_EXTRA] =
        gfx_create_texturert(width, height, gfxFormat::R16G16_FLOAT, FALSE);
    g_deferred->gbuff_depthtex = gfx_create_texturert(width, height, gfxFormat::DEPTH24_STENCIL8,
        FALSE);

    if (g_deferred->gbuff_tex[0] == NULL ||
        g_deferred->gbuff_tex[1] == NULL ||
        g_deferred->gbuff_tex[2] == NULL ||
        g_deferred->gbuff_tex[3] == NULL ||
        g_deferred->gbuff_depthtex == NULL)
    {
        return RET_FAIL;
    }

    g_deferred->gbuff = gfx_create_rendertarget(g_deferred->gbuff_tex, 4,
        g_deferred->gbuff_depthtex);
    if (g_deferred->gbuff == NULL)
        return RET_FAIL;

    return RET_OK;
}

void deferred_destroygbuffrt()
{
    if (g_deferred->gbuff != NULL)
        gfx_destroy_rendertarget(g_deferred->gbuff);

    if (g_deferred->gbuff_tex[0] != NULL)
        gfx_destroy_texture(g_deferred->gbuff_tex[0]);
    if (g_deferred->gbuff_tex[1] != NULL)
        gfx_destroy_texture(g_deferred->gbuff_tex[1]);
    if (g_deferred->gbuff_tex[2] != NULL)
        gfx_destroy_texture(g_deferred->gbuff_tex[2]);
    if (g_deferred->gbuff_tex[3] != NULL)
        gfx_destroy_texture(g_deferred->gbuff_tex[3]);
    if (g_deferred->gbuff_depthtex != NULL)
        gfx_destroy_texture(g_deferred->gbuff_depthtex);

    g_deferred->gbuff = NULL;
    g_deferred->gbuff_tex[0] = NULL;
    g_deferred->gbuff_tex[1] = NULL;
    g_deferred->gbuff_tex[2] = NULL;
    g_deferred->gbuff_tex[3] = NULL;
    g_deferred->gbuff_depthtex = NULL;
}

result_t deferred_createstates()
{
    /* depthstencil states */
    struct gfx_depthstencil_desc dsdesc;
    memcpy(&dsdesc, gfx_get_defaultdepthstencil(), sizeof(dsdesc));
    dsdesc.depth_enable = TRUE;
    dsdesc.depth_write = TRUE;
    dsdesc.depth_func = gfxCmpFunc::LESS;
    dsdesc.stencil_enable = /*TRUE*/FALSE;
    dsdesc.stencil_frontface_desc.cmp_func = gfxCmpFunc::ALWAYS;
    dsdesc.stencil_frontface_desc.pass_op = gfxStencilOp::REPLACE;
    g_deferred->ds_gbuff = gfx_create_depthstencilstate(&dsdesc);
    if (g_deferred->ds_gbuff == NULL)
        return RET_FAIL;

    memcpy(&dsdesc, gfx_get_defaultdepthstencil(), sizeof(dsdesc));
    dsdesc.stencil_enable = /*TRUE*/FALSE;
    dsdesc.stencil_frontface_desc.cmp_func = gfxCmpFunc::EQUAL;
    dsdesc.stencil_frontface_desc.pass_op = gfxStencilOp::KEEP;
    g_deferred->ds_nodepth_stest = gfx_create_depthstencilstate(&dsdesc);
    if (g_deferred->ds_nodepth_stest == NULL)
        return RET_FAIL;

    /* samplers */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    sdesc.filter_min = gfxFilterMode::NEAREST;
    sdesc.filter_mag = gfxFilterMode::NEAREST;
    sdesc.address_u = gfxAddressMode::CLAMP;
    sdesc.address_v = gfxAddressMode::CLAMP;
    sdesc.address_w = gfxAddressMode::CLAMP;
    g_deferred->sampl_point = gfx_create_sampler(&sdesc);
    if (g_deferred->sampl_point == NULL)
        return RET_FAIL;

    /* blend states */
    struct gfx_blend_desc bdesc;
    memcpy(&bdesc, gfx_get_defaultblend(), sizeof(bdesc));
    bdesc.enable = TRUE;
    bdesc.color_op = gfxBlendOp::ADD;
    bdesc.dest_blend = gfxBlendMode::ONE;
    bdesc.src_blend = gfxBlendMode::ONE;
    g_deferred->blend_add = gfx_create_blendstate(&bdesc);
    if (g_deferred->blend_add == NULL)
        return RET_FAIL;

    /* rasterizer states */
    struct gfx_rasterizer_desc rdesc;
    memcpy(&rdesc, gfx_get_defaultraster(), sizeof(rdesc));
    rdesc.scissor_test = TRUE;
    g_deferred->rs_scissor = gfx_create_rasterstate(&rdesc);
    if (g_deferred->rs_scissor == NULL)
        return RET_FAIL;

    return RET_OK;
}

void deferred_destroystates()
{
    if (g_deferred->ds_gbuff != NULL)
        gfx_destroy_depthstencilstate(g_deferred->ds_gbuff);
    if (g_deferred->ds_nodepth_stest != NULL)
        gfx_destroy_depthstencilstate(g_deferred->ds_nodepth_stest);
    if (g_deferred->sampl_point != NULL)
        gfx_destroy_sampler(g_deferred->sampl_point);
    if (g_deferred->blend_add != NULL)
        gfx_destroy_blendstate(g_deferred->blend_add);
    if (g_deferred->rs_scissor != NULL)
        gfx_destroy_rasterstate(g_deferred->rs_scissor);
}

result_t deferred_createprevbuffrt(uint width, uint height)
{
    g_deferred->prev_tex = gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
    if (g_deferred->prev_tex == NULL)
        return RET_FAIL;

    g_deferred->prev_rt = gfx_create_rendertarget(&g_deferred->prev_tex, 1, NULL);
    if (g_deferred->prev_rt == NULL)
        return RET_FAIL;

    g_deferred->prev_rt_result = gfx_create_rendertarget(&g_deferred->prev_tex, 1,
        g_deferred->gbuff_depthtex);
    if (g_deferred->prev_rt_result == NULL)
        return RET_FAIL;

    return RET_OK;
}

void deferred_destroyprevbuffrt()
{
    if (g_deferred->prev_tex != NULL)
        gfx_destroy_texture(g_deferred->prev_tex);
    if (g_deferred->prev_rt != NULL)
        gfx_destroy_rendertarget(g_deferred->prev_rt);
    if (g_deferred->prev_rt_result != NULL)
        gfx_destroy_rendertarget(g_deferred->prev_rt_result);

    g_deferred->prev_rt = NULL;
    g_deferred->prev_tex = NULL;
}

int deferred_load_prev_shaders(struct allocator* alloc)
{
    gfx_shader_beginload(alloc, "shaders/fsq.vs", "shaders/gbuff-prev.ps", NULL, 2,
        "shaders/df-common.inc", "shaders/common.inc");
    g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_ALBEDO] =
        gfx_shader_add("gbuff-prev-albedo", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_VIEWALBEDO_", "1");
    if (g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_ALBEDO] == 0)
        return FALSE;

    g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_SPECULAR] =
        gfx_shader_add("gbuff-prev-specular", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_VIEWSPECULARMUL_", "1");
    if (g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_SPECULAR] == 0)
        return FALSE;

    g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_NORMALS] =
        gfx_shader_add("gbuff-prev-normals", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_VIEWNORMALS_", "1");
    if (g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_NORMALS] == 0)
        return FALSE;
    g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_NORMALS_NOMAP] =
        gfx_shader_add("gbuff-prev-normalsnomap", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_VIEWNORMALSNOMAP_", "1");
    if (g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_NORMALS_NOMAP] == 0)
        return FALSE;
    g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_DEPTH] =
        gfx_shader_add("gbuff-prev-depth", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_VIEWDEPTH_", "1");
    if (g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_DEPTH] == 0)
        return FALSE;
    g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_MTL] =
        gfx_shader_add("gbuff-prev-mtl", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_VIEWMTL_", "1");
    if (g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_MTL] == 0)
        return FALSE;
    g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_GLOSS] =
        gfx_shader_add("gbuff-prev-gloss", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_VIEWGLOSS_", "1");
    if (g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_GLOSS] == 0)
        return FALSE;
    gfx_shader_endload();
    return TRUE;
}

void deferred_unload_prev_shaders()
{
    for (uint i = 0; i < DEFERRED_PREVIEW_SHADERCNT; i++) {
        if (g_deferred->prev_shaders[i] != 0)
            gfx_shader_unload(g_deferred->prev_shaders[i]);
    }
}

void gfx_deferred_setpreview(enum gfx_deferred_preview_mode mode)
{
    if (g_deferred != NULL && BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))
        g_deferred->prev_mode = mode;
}

void deferred_renderpreview(gfx_cmdqueue cmdqueue, enum gfx_deferred_preview_mode mode,
    const struct gfx_view_params* params )
{
    ASSERT(mode != GFX_DEFERRED_PREVIEW_NONE);

    PRF_OPENSAMPLE("preview");

    struct gfx_shader* shader;
    gfx_texture tex;
    const char* desc;

    switch (mode)   {
    case GFX_DEFERRED_PREVIEW_ALBEDO:
        shader = gfx_shader_get(g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_ALBEDO]);
        tex = g_deferred->gbuff_tex[DEFERRED_GBUFFER_ALBEDO];
        desc = "[gbuffer: albedo]";
        break;
    case GFX_DEFERRED_PREVIEW_SPECULAR:
        shader = gfx_shader_get(g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_SPECULAR]);
        tex = g_deferred->gbuff_tex[DEFERRED_GBUFFER_ALBEDO];
        desc = "[gbuffer: specular multiplier]";
        break;
    case GFX_DEFERRED_PREVIEW_NORMALS:
        shader = gfx_shader_get(g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_NORMALS]);
        tex = g_deferred->gbuff_tex[DEFERRED_GBUFFER_NORMAL];
        desc = "[gbuffer: normals]";
        break;
    case GFX_DEFERRED_PREVIEW_NORMALS_NOMAP:
        shader = gfx_shader_get(g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_NORMALS_NOMAP]);
        tex = g_deferred->gbuff_tex[DEFERRED_GBUFFER_EXTRA];
        desc = "[gbuffer: normals (no maps)]";
        break;
    case GFX_DEFERRED_PREVIEW_DEPTH:
        shader = gfx_shader_get(g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_DEPTH]);
        tex = g_deferred->gbuff_depthtex;
        desc = "[gbuffer: depth]";
        break;
    case GFX_DEFERRED_PREVIEW_MTL:
        shader = gfx_shader_get(g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_MTL]);
        tex = g_deferred->gbuff_tex[DEFERRED_GBUFFER_MTL];
        desc = "[gbuffer: material indexes]";
        break;
    case GFX_DEFERRED_PREVIEW_GLOSS:
        shader = gfx_shader_get(g_deferred->prev_shaders[GFX_DEFERRED_PREVIEW_GLOSS]);
        tex = g_deferred->gbuff_tex[DEFERRED_GBUFFER_MTL];
        desc = "[gbuffer: gloss]";
        break;
    default:
        shader = NULL;
        tex = NULL;
        desc = "";
        break;
    }

    ASSERT(shader != NULL);
    ASSERT(tex != NULL);

    gfx_output_setrendertarget(cmdqueue, g_deferred->prev_rt);
    gfx_output_setviewport(cmdqueue, 0, 0, g_deferred->width, g_deferred->height);
    gfx_shader_bind(cmdqueue, shader);

#if defined(_GL_)
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_viewmap), g_deferred->sampl_point,
    		tex);
#elif defined(_D3D_)
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_viewmap), tex);
#endif

    if (mode == GFX_DEFERRED_PREVIEW_DEPTH) {
        Camera* cam = wld_get_cam();
        float camprops[] = {cam->fnear, cam->ffar};
        gfx_shader_set2f(shader, SHADER_NAME(c_camprops), camprops);
        gfx_shader_bindconstants(cmdqueue, shader);
    }   else if (mode == GFX_DEFERRED_PREVIEW_MTL)   {
        gfx_shader_setf(shader, SHADER_NAME(c_mtlmax), (float)g_deferred->mtls.item_cnt);
        gfx_shader_bindconstants(cmdqueue, shader);
    }

#if defined(_D3D_)
    /* every other shader has sampler */
    if (mode != GFX_DEFERRED_PREVIEW_MTL && mode != GFX_DEFERRED_PREVIEW_GLOSS)
        gfx_shader_bindsampler(cmdqueue, shader, SHADER_NAME(s_viewmap), g_deferred->sampl_point);
#endif

    gfx_draw_fullscreenquad();

    /* preview description */
    struct rect2di rc;
    rect2di_seti(&rc, 0, g_deferred->height - 16, (int)g_deferred->width, g_deferred->height);
    gfx_canvas_settextcolor(&g_color_yellow);
    gfx_canvas_setfont(INVALID_HANDLE);
    gfx_canvas_text2drc(desc, &rc, GFX_TEXT_CENTERALIGN);
    gfx_canvas_settextcolor(&g_color_white);

    PRF_CLOSESAMPLE(); /* preview */

    gfx_set_previewrenderflag();
}

result_t deferred_console_setpreview(uint argc, const char** argv, void* param)
{
    if (argc != 1)
        return RET_INVALIDARG;

    if (str_isequal_nocase(argv[0], "0"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_NONE);
    else if (str_isequal_nocase(argv[0], "albedo"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_ALBEDO);
    else if (str_isequal_nocase(argv[0], "specmul"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_SPECULAR);
    else if (str_isequal_nocase(argv[0], "norm"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_NORMALS);
    else if (str_isequal_nocase(argv[0], "norm2"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_NORMALS_NOMAP);
    else if (str_isequal_nocase(argv[0], "depth"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_DEPTH);
    else if (str_isequal_nocase(argv[0], "mtl"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_MTL);
    else if (str_isequal_nocase(argv[0], "gloss"))
        gfx_deferred_setpreview(GFX_DEFERRED_PREVIEW_GLOSS);
    else
        return RET_INVALIDARG;

    return RET_OK;
}

result_t deferred_console_setdebugtiles(uint argc, const char** argv, void* param)
{
    int enable = TRUE;
    if (argc == 1)
        enable = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    g_deferred->debug_tiles = enable;
    if (enable)
        g_deferred->prev_mode = GFX_DEFERRED_PREVIEW_NONE;
    return RET_OK;
}


int deferred_load_light_shaders(struct allocator* alloc)
{
    int r;

    char mtlsmax[16];
    char lightsmax[16];
    char lightidx_max[16];
    char tiles_max[16];

    str_itos(mtlsmax, DEFERRED_MTLS_MAX);
    str_itos(lightsmax, DEFERRED_LIGHTS_MAX);
    str_itos(lightidx_max, DEFERRED_LIGHTS_PERPASS_MAX);
    str_itos(tiles_max, DEFERRED_LIGHTS_TILES_MAX);

    gfx_shader_beginload(alloc, "shaders/fsq-pos.vs", "shaders/df-light.ps", NULL, 2,
        "shaders/df-common.inc", "shaders/brdf.inc");
    r = deferred_addshader(gfx_shader_add("dlight-sun", 2, 2,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_MAX_MTLS_", mtlsmax,
        "_SUN_LIGHTING_", "1"),
        0, DEFERRED_SHADERGROUP_LIGHT);
    gfx_shader_endload();
    if (!r)
        return FALSE;

    gfx_shader_beginload(alloc, "shaders/df-light.vs", "shaders/df-light.ps", NULL, 2,
        "shaders/df-common.inc", "shaders/brdf.inc");
    r = deferred_addshader(gfx_shader_add("dlight-local", 2, 5,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_MAX_MTLS_", mtlsmax,
        "_LOCAL_LIGHTING_", "1",
        "_MAX_LIGHTS_", lightsmax,
        "_MAX_LIGHT_INDEXES_", lightidx_max,
        "_MAX_TILES_", tiles_max),
        0, DEFERRED_SHADERGROUP_LIGHT);
    gfx_shader_endload();
    if (!r)
        return FALSE;

    return r;
}

void deferred_unload_light_shaders()
{
    for (uint i = 0; i < g_deferred->light_shadercnt; i++)    {
        if (g_deferred->light_shaders[i].shader_id != 0)
            gfx_shader_unload(g_deferred->light_shaders[i].shader_id);
    }
}

result_t deferred_createlitrt(uint width, uint height)
{
    g_deferred->lit_tex = gfx_create_texturert(width, height, gfxFormat::R11G11B10_FLOAT, FALSE);
    if (g_deferred->lit_tex == NULL)
        return RET_FAIL;

    g_deferred->lit_rt = gfx_create_rendertarget(&g_deferred->lit_tex, 1, NULL);
    if (g_deferred->lit_rt == NULL)
        return RET_FAIL;
    g_deferred->lit_rt_result = gfx_create_rendertarget(&g_deferred->lit_tex, 1,
        g_deferred->gbuff_depthtex);
    if (g_deferred->lit_rt_result == NULL)
        return RET_FAIL;
    return RET_OK;
}

void deferred_destroylitrt()
{
    if (g_deferred->lit_rt != NULL)
        gfx_destroy_rendertarget(g_deferred->lit_rt);
    if (g_deferred->lit_rt_result != NULL)
        gfx_destroy_rendertarget(g_deferred->lit_rt_result);
    if (g_deferred->lit_tex != NULL)
        gfx_destroy_texture(g_deferred->lit_tex);
}

void deferred_renderlocallights(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    const struct gfx_renderpass_lightdata* lightdata)
{
    PRF_OPENSAMPLE("local lights");

    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);

    /* clear */
    deferred_cleartiles(&g_deferred->tiles);

    /* states */
    gfx_output_setblendstate(cmdqueue, g_deferred->blend_add, NULL);

    /* fetch shader */
    struct gfx_shader* shader =
        gfx_shader_get(g_deferred->light_shaders[DEFERRED_LIGHTSHADER_LOCAL].shader_id);
    gfx_shader_bind(cmdqueue, shader);

    /* batch/cull */
    deferred_processtiles(&g_deferred->tiles, tmp_alloc, 0, g_deferred->tiles.cnt, params,
        lightdata->lights, lightdata->bounds, lightdata->cnt);

    /* push lights to gpu */
    gfx_shader_updatecblock(cmdqueue, g_deferred->tb_lights);
    gfx_shader_bindcblock_tbuffer(cmdqueue, shader, SHADER_NAME(tb_lights), g_deferred->tb_lights);

    /* set materials */
    gfx_shader_bindcblock_tbuffer(cmdqueue, shader, SHADER_NAME(tb_mtls), g_deferred->tb_mtls);

    /* constants */
    float rtvsz[] = {(float)g_deferred->width, (float)g_deferred->height};
    uint grid[] = {g_deferred->tiles.cnt_x, g_deferred->tiles.cnt_y, DEFERRED_TILE_SIZE};
    gfx_shader_set4f(shader, SHADER_NAME(c_projparams), params->projparams.f);
    gfx_shader_setf(shader, SHADER_NAME(c_camfar), params->cam->ffar);
    gfx_shader_set2f(shader, SHADER_NAME(c_rtsz), rtvsz);
    gfx_shader_set3ui(shader, SHADER_NAME(c_grid), grid);
    gfx_shader_bindcblocks(cmdqueue, shader, (const struct gfx_cblock**)&g_deferred->cb_light, 1);
    gfx_input_setlayout(cmdqueue, g_deferred->tile_il);

    /* textures */
#if defined(_D3D_)
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_depth), g_deferred->gbuff_depthtex);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_albedo), g_deferred->gbuff_tex[0]);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_norm), g_deferred->gbuff_tex[1]);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_mtl), g_deferred->gbuff_tex[2]);
#elif defined(_GL_)
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), g_deferred->sampl_point,
        g_deferred->gbuff_depthtex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_albedo), g_deferred->sampl_point,
        g_deferred->gbuff_tex[0]);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_norm), g_deferred->sampl_point,
        g_deferred->gbuff_tex[1]);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_mtl), g_deferred->sampl_point,
        g_deferred->gbuff_tex[2]);
#endif

    /* for each tile apply light indexes and render */
    uint i = 0;
    while (i < g_deferred->tiles.cnt)   {
        /* setup vertex-shader */
        gfx_shader_setui(shader, SHADER_NAME(c_celloffset), i);
        gfx_shader_bindconstants(cmdqueue, shader);

        /* setup tile data */
        int max_i = mini(g_deferred->tiles.cnt, i + DEFERRED_LIGHTS_TILES_MAX);
        gfx_cb_setp(g_deferred->cb_light, SHADER_NAME(c_tiles), &g_deferred->tiles.light_lists[i],
            sizeof(struct deferred_shader_tile) * (max_i-i));
        gfx_shader_updatecblock(cmdqueue, g_deferred->cb_light);

        gfx_draw_instance(cmdqueue, gfxPrimitiveType::TRIANGLE_STRIP, 0, 4, max_i - i,
            GFX_DRAWCALL_LIGHTING);

        i += DEFERRED_LIGHTS_TILES_MAX;
    }

    gfx_output_setrasterstate(cmdqueue, NULL);
    gfx_output_setblendstate(cmdqueue, NULL, NULL);

    PRF_CLOSESAMPLE();

    deferred_drawlights(cmdqueue, params, lightdata);
}

void deferred_renderlights(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    const struct gfx_renderpass_lightdata* lightdata, gfx_texture ssao_tex,
    gfx_texture shadowcsm_tex)
{
    PRF_OPENSAMPLE("lighting");

    struct color clear_color;
    color_tolinear(&clear_color, color_setf(&clear_color, 0.1f, 0.1f, 0.1f, 1.0f));

    gfx_cmdqueue_resetsrvs(cmdqueue);
    gfx_output_setrendertarget(cmdqueue, g_deferred->lit_rt);
    gfx_output_clearrendertarget(cmdqueue, g_deferred->lit_rt, clear_color.f, 1.0f, 0,
        gfxClearFlag::COLOR);
    gfx_output_setviewport(cmdqueue, 0, 0, g_deferred->width, g_deferred->height);

    /* push materials to gpu */
    gfx_shader_updatecblock(cmdqueue, g_deferred->tb_mtls);

    /* render directional (sun) light */
    deferred_rendersunlight(cmdqueue, params, ssao_tex, shadowcsm_tex);

    if (lightdata->cnt > 0)
        deferred_renderlocallights(cmdqueue, params, lightdata);

    gfx_cmdqueue_resetsrvs(cmdqueue);

    PRF_CLOSESAMPLE(); /* lighting */
}

void deferred_rendersunlight(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    gfx_texture ssao_tex, gfx_texture shadowcsm_tex)
{
    PRF_OPENSAMPLE("sun-light");

    /* fetch shader */
    struct gfx_shader* shader =
        gfx_shader_get(g_deferred->light_shaders[DEFERRED_LIGHTSHADER_SUN].shader_id);
    gfx_shader_bind(cmdqueue, shader);

    /* construct sun light data for shader */
    struct vec3f lightdir;
    struct color lightcolor;
    struct color ambient_sky;
    struct color ambient_ground;
    struct vec3f sky_vect;

    uint sec_light = wld_find_section("light");
    uint sec_ambient = wld_find_section("ambient");

    const float* world_sundir = wld_get_var(sec_light, wld_find_var(sec_light, "dir"))->fv;
    const float* world_suncolor = wld_get_var(sec_light, wld_find_var(sec_light, "color"))->fv;
    float world_sunintensity = wld_get_var(sec_light, wld_find_var(sec_light, "intensity"))->f;

    const float* world_ambientsky =
        wld_get_var(sec_ambient, wld_find_var(sec_ambient, "sky-color"))->fv;
    const float* world_ambientground =
        wld_get_var(sec_ambient, wld_find_var(sec_ambient, "ground-color"))->fv;
    const float* world_skyvect =
        wld_get_var(sec_ambient, wld_find_var(sec_ambient, "sky-vector"))->fv;
    float world_ambientintensity =
        wld_get_var(sec_ambient, wld_find_var(sec_ambient, "intensity"))->f;

    /* transform direction in view-space and inverse it for light-calculation in shader */
    vec3_setf(&lightdir, world_sundir[0], world_sundir[1], world_sundir[2]);
    vec3_transformsr(&lightdir, vec3_norm(&lightdir, &lightdir), &params->view);
    vec3_muls(&lightdir, &lightdir, -1.0f);

    /* linearize light color and premultiply by intensity */
    color_setf(&lightcolor, world_suncolor[0], world_suncolor[1], world_suncolor[2], 1.0f);
    color_muls(&lightcolor, color_tolinear(&lightcolor, &lightcolor), world_sunintensity);

    /* sky/ground ambient colors */
    color_tolinear(&ambient_sky, color_setf(&ambient_sky, world_ambientsky[0], world_ambientsky[1],
        world_ambientsky[2], 1.0f));
    color_tolinear(&ambient_ground, color_setf(&ambient_ground, world_ambientground[0],
        world_ambientground[1], world_ambientground[2], 1.0f));

    /* transform sky-vector (normally up vector) to view-space */
    vec3_setf(&sky_vect, world_skyvect[0], world_skyvect[1], world_skyvect[2]);
    vec3_transformsr(&sky_vect, vec3_norm(&sky_vect, &sky_vect), &params->view);

    /* set materials */
    gfx_shader_bindcblock_tbuffer(cmdqueue, shader, SHADER_NAME(tb_mtls), g_deferred->tb_mtls);

    /* constants */
    gfx_shader_set3f(shader, SHADER_NAME(c_lightdirinv_vs), lightdir.f);
    gfx_shader_set4f(shader, SHADER_NAME(c_lightcolor), lightcolor.f);
    gfx_shader_set4f(shader, SHADER_NAME(c_projparams), params->projparams.f);
    gfx_shader_setf(shader, SHADER_NAME(c_camfar), params->cam->ffar);
    gfx_shader_set4f(shader, SHADER_NAME(c_ambient_sky), ambient_sky.f);
    gfx_shader_set4f(shader, SHADER_NAME(c_ambient_ground), ambient_ground.f);
    gfx_shader_set3f(shader, SHADER_NAME(c_skydir_vs), sky_vect.f);
    gfx_shader_setf(shader, SHADER_NAME(c_ambient_intensity), world_ambientintensity);
    gfx_shader_bindconstants(cmdqueue, shader);

    /* textures */
#if defined(_D3D_)
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_depth), g_deferred->gbuff_depthtex);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_albedo), g_deferred->gbuff_tex[0]);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_norm), g_deferred->gbuff_tex[1]);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_mtl), g_deferred->gbuff_tex[2]);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_shadows), shadowcsm_tex);
    gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_ssao), ssao_tex);
#elif defined(_GL_)
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), g_deferred->sampl_point,
        g_deferred->gbuff_depthtex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_albedo), g_deferred->sampl_point,
        g_deferred->gbuff_tex[0]);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_norm), g_deferred->sampl_point,
        g_deferred->gbuff_tex[1]);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_mtl), g_deferred->sampl_point,
        g_deferred->gbuff_tex[2]);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_shadows), g_deferred->sampl_point,
        shadowcsm_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_ssao), g_deferred->sampl_point,
        ssao_tex);
#endif

    /* draw */
    gfx_draw_fullscreenquad();

    PRF_CLOSESAMPLE();  /* sun light */
}

uint deferred_pushmtl(struct gfx_cblock* cb_mtl)
{
    /* construct hash and search in mtable and see if we already have it */
    uint h = hash_murmur32(cb_mtl->cpu_buffer, cb_mtl->buffer_size, SHADER_HSEED);
    struct hashtable_item* item = hashtable_open_find(&g_deferred->mtable, h);
    if (item == NULL)   {
        if (g_deferred->mtls.item_cnt == DEFERRED_MTLS_MAX) {
            log_print(LOG_WARNING, "deferred materials exceeding specified limit- set to 0");
            return 0;
        }

        struct gfx_cblock** pcb = (struct gfx_cblock**)arr_add(&g_deferred->mtls);
        ASSERT(pcb);
        *pcb = cb_mtl;

        uint idx = g_deferred->mtls.item_cnt - 1;
        hashtable_open_add(&g_deferred->mtable, h, idx);

        /* update data in tb_mtl */
        gfx_cb_setpv_offset(g_deferred->tb_mtls, 0, cb_mtl->cpu_buffer, cb_mtl->buffer_size,
            idx*cb_mtl->buffer_size);

        return idx;
    }   else    {
        return (uint)item->value;
    }
}

const struct gfx_cblock* deferred_fetchmtl(int idx)
{
    ASSERT(idx < (int)g_deferred->mtls.item_cnt);
    return &((struct gfx_cblock*)g_deferred->mtls.buffer)[idx];
}


result_t deferred_createtiles(struct deferred_tiles* tiles, uint width, uint height)
{
    uint cnt_x = width / DEFERRED_TILE_SIZE;
    uint cnt_y = height / DEFERRED_TILE_SIZE;
    uint last_width = width % DEFERRED_TILE_SIZE;
    uint last_height = height % DEFERRED_TILE_SIZE;
    cnt_x = (last_width != 0) ? (cnt_x+1) : cnt_x;
    cnt_y = (last_height != 0) ? (cnt_y+1) : cnt_y;
    uint cnt = cnt_x * cnt_y;

    tiles->cnt = cnt;
    tiles->cnt_x = cnt_x;
    tiles->cnt_y = cnt_y;

    /* simd data layout :
     * two vec4f for each tile
     * #1: x=x_min, y=y_min, z=x_min, w=y_min
     * #2: y=x_max, y=y_max, z=z_max, w=y_max
     */
    tiles->simd_data = (struct vec4f*)ALIGNED_ALLOC(sizeof(struct vec4f)*cnt*2, MID_GFX);
    if (tiles->simd_data == NULL)
        return RET_OUTOFMEMORY;
    for (uint y = 0; y < cnt_y; y++)  {
        for (uint x = 0; x < cnt_x; x++)  {
            uint idx = x + y*cnt_x;

            struct vec4f* r1 = &tiles->simd_data[idx*2];
            struct vec4f* r2 = &tiles->simd_data[idx*2 + 1];

            /* min */
            vec4_setf(r1, (float)(x*DEFERRED_TILE_SIZE), (float)(y*DEFERRED_TILE_SIZE), 0.0f, 0.0f);
            r1->z = r1->x;
            r1->w = r1->y;

            /* max */
            if (x != cnt_x - 1 || last_width == 0)
                r2->x = r1->x + (float)DEFERRED_TILE_SIZE;
            else
                r2->x = r1->x + (float)last_width;

            if (y != cnt_y - 1 || last_height == 0)
                r2->y = r1->y + (float)DEFERRED_TILE_SIZE;
            else
                r2->y = r1->y + (float)last_height;
            r2->z = r2->x;
            r2->w = r2->y;
        }
    }

    /* tile rectangles, create from simd_data */
    tiles->rects = (struct rect2di*)ALIGNED_ALLOC(sizeof(struct rect2di)*cnt, MID_GFX);
    if (tiles->rects == NULL)
        return RET_OUTOFMEMORY;

    for (uint i = 0; i < cnt; i++)    {
        struct vec4f* tile_min = &tiles->simd_data[2*i];
        struct vec4f* tile_max = &tiles->simd_data[2*i + 1];
        rect2di_seti(&tiles->rects[i], (int)tile_min->x, (int)tile_min->y,
            (int)(tile_max->x - tile_min->x), (int)(tile_max->y - tile_min->y));
    }

    /* lists */
    result_t r;
    tiles->light_lists = (struct deferred_shader_tile*)
        ALLOC(sizeof(struct deferred_shader_tile)*cnt, MID_GFX);
    if (tiles->light_lists == NULL)
        return RET_OUTOFMEMORY;
    memset(tiles->light_lists, 0x00, sizeof(struct deferred_shader_tile)*cnt);

    /* light allocator */
    r = hashtable_open_create(mem_heap(), &tiles->light_table, 100, 200, MID_GFX);
    if (IS_FAIL(r))
        return r;

    r = deferred_createtilequad();
    if (IS_FAIL(r))
        return RET_FAIL;

    return RET_OK;
}

void deferred_destroytiles(struct deferred_tiles* tiles)
{
    if (tiles->light_lists != NULL)
        FREE(tiles->light_lists);
    if (tiles->rects != NULL)
        ALIGNED_FREE(tiles->rects);
    if (tiles->simd_data != NULL)
        ALIGNED_FREE(tiles->simd_data);
    hashtable_open_destroy(&tiles->light_table);
    deferred_destroytilequad();
}

void deferred_cleartiles(struct deferred_tiles* tiles)
{
    for (uint i = 0, cnt = tiles->cnt; i < cnt; i++)
        tiles->light_lists[i].cnt[0] = 0;
    hashtable_open_clear(&tiles->light_table);
    tiles->light_cnt = 0;
}

uint deferred_createlight(struct deferred_tiles* tiles, const struct scn_render_light* light,
    const struct mat3f* view)
{
    /* search in light_table and see if we already cached light data */
    uint hash = hash_u64(light->light_hdl);
    struct hashtable_item* item = hashtable_open_find(&tiles->light_table, hash);
    if (item != NULL)   {
        return (uint)item->value;
    }   else    {
        uint idx = tiles->light_cnt;

        if (idx == DEFERRED_LIGHTS_MAX) {
            log_print(LOG_WARNING, "lights exceed maximum limit, reset to 0.");
            return 0;
        }

        struct cmp_light* ldata = (struct cmp_light*)cmp_getinstancedata(light->light_hdl);
        struct deferred_light dlight;

        /* calculate light data (view-space) */
        dlight.type[0] = (float)ldata->type;
        vec3_transformsrt(&dlight.pos_vs, &ldata->pos, view);
        vec4_setf(&dlight.atten, ldata->atten_near, ldata->atten_far, cosf(ldata->atten_narrow),
            cosf(ldata->atten_wide));
        vec3_transformsr(&dlight.dir_vs, &ldata->dir, view);
        /* premultiply lightcolor by intensity */
        float intensity = ldata->intensity*light->intensity_mul;
        color_setc(&dlight.color,
            color_muls(&dlight.color, &ldata->color_lin, intensity));
        dlight.color.a = intensity;

        /* add to db/tbuffer */
        gfx_cb_setpv_offset(g_deferred->tb_lights, 0, &dlight, sizeof(dlight), idx*sizeof(dlight));
        hashtable_open_add(&tiles->light_table, hash, idx);
        tiles->light_cnt++;

        return idx;
    }
}

/* process (cull) batch of tiles and build light lists for each tile
 * start_idx: index of the starting tile
 * end_idx: index of the end tile (=count to process to end)
 */
void deferred_processtiles(struct deferred_tiles* tiles, struct allocator* alloc,
    uint start_idx, uint end_idx, const struct gfx_view_params* params,
    const struct scn_render_light* lights, const struct sphere* bounds, uint light_cnt)
{
    PRF_OPENSAMPLE("process-tiles");

    float wh = (float)g_deferred->width * 0.5f;
    float hh = (float)g_deferred->height * 0.5f;

    /* calculate world->clip space matrix */
    struct mat4f viewprojclip;
    struct mat4f clip;
    mat4_setf(&clip,
        wh, 0.0f, 0.0f, 0.0f,
        0.0f, -hh, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        wh + 0.5f, hh + 0.5f, 0.0f, 1.0f);
    mat4_mul(&viewprojclip, &params->viewproj, &clip);

    /* calculate inverse-view matrix from view */
    struct mat3f view_inv;
    mat3_setf(&view_inv,
        params->view.m11, params->view.m21, params->view.m31,
        params->view.m12, params->view.m22, params->view.m32,
        params->view.m13, params->view.m23, params->view.m33,
        params->cam_pos.x, params->cam_pos.y, params->cam_pos.z);

    /* calculate screen-space simd-friendly rectangles */
    struct vec4f* r = deferred_calc_lightbounds_simd(alloc, bounds, lights, light_cnt, &view_inv,
        &viewprojclip);
    if (r == NULL)
        return;

    int* cull = (int*)A_ALLOC(alloc, sizeof(int)*light_cnt, MID_GFX);
    if (cull == NULL)   {
        A_FREE(alloc, r);
        return;
    }

    /* process two lights in each iteration
     * in each iteration we get a tile and test it with two lights in each subloop iteration */
    const struct vec4f* tiles_simd = tiles->simd_data;
    for (uint i = start_idx; i < end_idx; i++)  {
        simd_t _tmin = _mm_load_ps(tiles_simd[2*i].f);   /* tile_min = x_min, y_min, x_min, y_min */
        simd_t _tmax = _mm_load_ps(tiles_simd[2*i+1].f); /* tile_max = x_max, y_max, x_max, y_max */

        for (uint k = 0; k < light_cnt; k+=2)  {
            struct vec4f* v1 = &r[k];				/* v1 = x_min1, y_min1, x_min2, y_min2 */
            struct vec4f* v2 = &r[k + 1];			/* v2 = x_max1, y_max1, x_max2, y_max2 */

            simd_t _vmin = _mm_load_ps(v1->f);
            simd_t _vmax = _mm_load_ps(v2->f);
            simd_t _r1 = _mm_cmpgt_ps(_tmin, _vmax); /* (x_min > x_max1), (y_min > y_max1)
                                                      * (x_min > x_max2), (y_min > y_max2) */
            simd_t _r2 = _mm_cmplt_ps(_tmax, _vmin); /* (x_max < x_min1), (y_max < y_min1)
                                                      * (x_max < x_min2), (y_max < y_min2) */

            simd_t _r = _mm_or_ps(_r1, _r2);    /* OR of two above statements */

            int mask = _mm_movemask_ps(_r);   /* if any values are 0xffffff
                                                 * then no intersection is occured */
            cull[k] = (mask & 0x3); /* mask higher bits for rectangle #1  */
            cull[k+1] = (mask & 0xC); /* mask lower bits for rectangle #2  */
        }

        /* gather lights */
        for (uint k = 0; k < light_cnt; k++)  {
            if (cull[k] == 0 && tiles->light_lists[i].cnt[0] < DEFERRED_LIGHTS_PERPASS_MAX)   {
                tiles->light_lists[i].idxs[tiles->light_lists[i].cnt[0]++] =
                    deferred_createlight(tiles, &lights[k], &params->view);
            }
        }
    }

    /* debug */
    if (g_deferred->debug_tiles)
        deferred_debugtiles(tiles, r, light_cnt);

    A_FREE(alloc, cull);
    A_ALIGNED_FREE(alloc, r);

    PRF_CLOSESAMPLE();  /* process-tiles */
}

#if defined(_SIMD_SSE_)
/* gets light data and transforms them into simd friendly bounds in clip-space (or pixel space)
 * @return each result is a pair that contains two 2D bounding boxes (count = light_cnt)
 * #1: (x1_min, y1_min, x2_min, y2_min)
 * #2: (x1_max, y1_max, x2_max, y2_max)
 */
struct vec4f* deferred_calc_lightbounds_simd(struct allocator* alloc, const struct sphere* bounds,
    const struct scn_render_light* lights, uint light_cnt, const struct mat3f* view_inv,
    const struct mat4f* viewprojclip)
{
    struct vec3f xaxis;
    struct vec3f yaxis;
    struct vec3f campos;

    mat3_get_xaxis(&xaxis, view_inv);
    mat3_get_yaxis(&yaxis, view_inv);
    mat3_get_trans(&campos, view_inv);

    /* bounds is simd friendly SOA
     * we have 2 vec4f for each bound, one for min, one for max
     * data layout:
     * x1 -> x2 -> x3 -> x4 -> ...
     * y1 -> y2 -> y3 -> y4 -> ...
     * z1 -> z2 -> z3 -> z4 -> ...
     * w1 -> w2 -> w3 -> w4 -> ...
     *(min) (max) (min) (max)
     */
    struct vec4f_simd bounds_simd;
    memset(&bounds_simd, 0x00, sizeof(bounds_simd));
    if (IS_FAIL(vec4simd_create(&bounds_simd, alloc, light_cnt*2)))
        return NULL;

    /* make bounds (untransformed) and put them into simd-array */
    struct vec3f maxpt;
    struct vec3f minpt;

    for (uint i = 0; i < light_cnt; i++)  {
        const struct scn_render_light* l = &lights[i];
        const struct sphere* s = &bounds[l->bounds_idx];

        simd_t _p = _mm_set_ps(1.0f, s->z, s->y, s->x);
        simd_t _r = _mm_set1_ps(s->r);

        /* zaxis calculation (after sub, w will be 0.0) */
        simd_t _zaxis = _mm_sub_ps(_mm_load_ps(campos.f), _p);

        /* normalize zaxis */
        simd_t _zaxis_lsqr = _mm_mul_ps(_zaxis, _zaxis);
        _zaxis_lsqr = _mm_hadd_ps(_zaxis_lsqr, _zaxis_lsqr);
        _zaxis_lsqr = _mm_hadd_ps(_zaxis_lsqr, _zaxis_lsqr);
        _zaxis = _mm_mul_ps(_zaxis, _mm_rsqrt_ps(_zaxis_lsqr));

        /* scale axises by radius */
        _zaxis = _mm_mul_ps(_zaxis, _r);
        simd_t _xaxis = _mm_mul_ps(_mm_load_ps(xaxis.f), _r);
        simd_t _yaxis = _mm_mul_ps(_mm_load_ps(yaxis.f), _r);
        simd_t _maxis = _mm_add_ps(_xaxis, _yaxis);

        /* minpt = p - (xaxis*sphere.r + yaxis*sphere.r) + zaxis*sphere.r */
        simd_t _minpt = _mm_add_ps(_mm_sub_ps(_p, _maxis), _zaxis);

        /* maxpt = p + xaxis*sphere.r + yaxis*sphere.r + zaxis*sphere.r */
        simd_t _maxpt = _mm_add_ps(_mm_add_ps(_p, _maxis), _zaxis);

        /* assign to SOA structure */
        _mm_store_ps(minpt.f, _minpt);
        _mm_store_ps(maxpt.f, _maxpt);

        uint idx = i*2;
        bounds_simd.xs[idx] = minpt.x;
        bounds_simd.xs[idx+1] = maxpt.x;

        bounds_simd.ys[idx] = minpt.y;
        bounds_simd.ys[idx+1] = maxpt.y;

        bounds_simd.zs[idx] = minpt.z;
        bounds_simd.zs[idx+1] = maxpt.z;

        bounds_simd.ws[idx] = 1.0f;
        bounds_simd.ws[idx+1] = 1.0f;
    }

    /* multiply simd ready data by transform matrix (transform into clip-space) */
    /* first prepare data and simd matrix */
    struct mat4f_simd m;
    uint result_cnt = bounds_simd.cnt / 2;
    struct vec4f* result = (struct vec4f*)A_ALIGNED_ALLOC(alloc, result_cnt*sizeof(struct vec4f),
        MID_GFX);
    if (result == NULL) {
        vec4simd_destroy(&bounds_simd);
        return NULL;
    }

    mat4simd_setm(&m, viewprojclip);

    /* calculate two sphere bounding boxes on each iteration */
    for (uint i = 0, cnt = bounds_simd.cnt; i < cnt; i+=4)  {
        uint ridx = i/2;
        simd_t _vx = _mm_load_ps(bounds_simd.xs + i);
        simd_t _vy = _mm_load_ps(bounds_simd.ys + i);
        simd_t _vz = _mm_load_ps(bounds_simd.zs + i);
        simd_t _t;
        simd_t _w;

        /* calc Vector->w values of 4 vects */
        _w = _mm_add_ps(_mm_mul_ps(_vx, _mm_load_ps(m.m14)),
            _mm_mul_ps(_vy, _mm_load_ps(m.m24)));		/* x*m14 + y*m24 */
        _w = _mm_add_ps(_w, _mm_mul_ps(_vz, _mm_load_ps(m.m34)));   /* x*m14 + y*m24 + z*m34 */
        _w = _mm_add_ps(_w, _mm_load_ps(m.m44));    /* x*m14 + y*m24 + z*m34 + 1.0*m44 */

        /* calc Vector->x values of 4 vects */
        _t = _mm_add_ps(_mm_mul_ps(_vx, _mm_load_ps(m.m11)),
            _mm_mul_ps(_vy, _mm_load_ps(m.m21)));   /* x*m11 + y*m21 */
        _t = _mm_add_ps(_t, _mm_mul_ps(_vz, _mm_load_ps(m.m31)));   /* x*m11 + y*m21 + z*m31 */
        _t = _mm_add_ps(_t, _mm_load_ps(m.m41));    /* x*m11 + y*m21 + z*m31 + 1.0*m41 */
        simd_t _x = _mm_div_ps(_t, _w); /* x = x/w */

        /* calc Vector->y values of 4 vects */
        _t = _mm_add_ps(_mm_mul_ps(_vx, _mm_load_ps(m.m12)),
            _mm_mul_ps(_vy, _mm_load_ps(m.m22)));		/* x*m12 + y*m22 */
        _t = _mm_add_ps(_t, _mm_mul_ps(_vz, _mm_load_ps(m.m32)));   /* x*m12 + y*m22 + z*m32 */
        _t = _mm_add_ps(_t, _mm_load_ps(m.m42));    /* x*m12 + y*m22 + z*m32 + 1.0*m42 */
        simd_t _y = _mm_div_ps(_t, _w); /* y = y/w */

        _t = _mm_shuffle_ps(_x, _y, _MM_SHUFFLE(2, 0, 2, 0));   /* x1, x2, y1, y2 (min) */
        _vx = _mm_shuffle_ps(_t, _t, _MM_SHUFFLE(3, 1, 2, 0));  /* x1, y1, x2, y2 (min) */
        _t = _mm_shuffle_ps(_x, _y, _MM_SHUFFLE(3, 1, 3, 1));   /* x1, x2, y1, y2 (max) */
        _vy = _mm_shuffle_ps(_t, _t, _MM_SHUFFLE(3, 1, 2, 0));  /* x1, y1, x2, y2 (max) */

        /* we put two rects min, max (4 vector4s) into two vector4 consisting of their x,y only */
        _mm_store_ps(result[ridx].f, _mm_min_ps(_vx, _vy));
        _mm_store_ps(result[ridx+1].f, _mm_max_ps(_vx, _vy));
    }

    vec4simd_destroy(&bounds_simd);
    return result;

}
#else
#error "not implemented"
#endif

void deferred_debugtiles(struct deferred_tiles* tiles,
    const struct vec4f* light_rects, uint light_rect_cnt)
{
    /* screen-space light bounds */
    gfx_canvas_setlinecolor(&g_color_yellow);
    gfx_canvas_setalpha(0.5f);
    for (uint i = 0; i < light_rect_cnt; i+=2)   {
        struct rect2di rc;
        const struct vec4f* v1 = &light_rects[i];
        const struct vec4f* v2 = &light_rects[i+1];

        rect2di_seti(&rc, (int)v1->x, (int)v1->y, (int)(v2->x - v1->x),
            (int)(v2->y - v1->y));
        gfx_canvas_rect2d(&rc, 1, GFX_RECT2D_HOLLOW);

        if ((i+1) < light_rect_cnt) {
            rect2di_seti(&rc, (int)v1->z, (int)v1->w, (int)(v2->z - v1->z),
                (int)(v2->w - v1->w));
            gfx_canvas_rect2d(&rc, 1, GFX_RECT2D_HOLLOW);
        }
    }

    /* tiles */
    /* grid */
    gfx_canvas_setalpha(1.0f);
    gfx_canvas_setlinecolor(&g_color_blue);
    uint cnt_x = tiles->cnt_x;
    uint cnt_y = tiles->cnt_y;
    for (uint y = 0; y < cnt_y; y++)    {
        uint idx = y * cnt_x;
        struct rect2di rc1;
        struct rect2di rc2;
        rect2di_setr(&rc1, &tiles->rects[idx]);
        rect2di_setr(&rc2, &tiles->rects[idx + cnt_x - 1]);
        gfx_canvas_line2d(rc1.x, rc1.y, rc2.x + rc2.w, rc2.y, 2);
    }

    for (uint x = 0; x < cnt_x; x++)  {
        struct rect2di rc1;
        struct rect2di rc2;
        rect2di_setr(&rc1, &tiles->rects[x]);
        rect2di_setr(&rc2, &tiles->rects[x + (cnt_y-1)*cnt_x]);
        gfx_canvas_line2d(rc1.x, rc1.y, rc2.x, rc2.y + rc2.h, 2);
    }

    /* number of lightes in the tile (text) */
    gfx_canvas_setfont(INVALID_HANDLE);
    for (uint i = 0; i < tiles->cnt; i++) {
        struct rect2di rc;
        rect2di_setr(&rc, &tiles->rects[i]);
        char num[16];
        str_itos(num, tiles->light_lists[i].cnt[0]);
        if (tiles->light_lists[i].cnt[0] > 0)
            gfx_canvas_settextcolor(&g_color_red);
        else
            gfx_canvas_settextcolor(&g_color_white);
        gfx_canvas_text2dpt(num, rc.x + rc.w/2 - 5, rc.y + rc.h/2 - 5, 0);
    }
    gfx_canvas_settextcolor(&g_color_white);
}

result_t deferred_console_showssao(uint argc, const char** argv, void* param)
{
    int enable = TRUE;
    if (argc == 1)
        enable = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    if (enable && g_deferred->ssao_result_tmp != NULL)    {
        hud_add_image("ssao", g_deferred->ssao_result_tmp, TRUE, 0, 0, "[SSAO]");
        hud_add_label("ssao", gfx_pfx_ssao_debugtext, g_deferred->ssao);
    }   else    {
        hud_remove_image("ssao");
        hud_remove_label("ssao");
    }

    return RET_OK;
}

result_t deferred_createtilequad()
{
    /* create one tile in screen-space */
    const float tile_sz = DEFERRED_TILE_SIZE;
    const float coord_sz = tile_sz/((float)g_deferred->width);

#ifdef _GNUC_
    const struct deferred_tile_vertex verts[] = {
        {{.x=tile_sz, .y=0.0f, .z=1.0f, .w=1.0f}, {.x=coord_sz, .y=0.0f}},
        {{.x=0.0f, .y=0.0f, .z=1.0f, .w=1.0f}, {.x=0.0f, .y=0.0f}},
        {{.x=tile_sz, .y=tile_sz, .z=1.0f, .w=1.0f}, {.x=coord_sz, .y=coord_sz}},
        {{.x=0.0f, .y=tile_sz, .z=1.0f, .w=1.0f}, {.x=0.0f, .y=coord_sz}}
    };
#else
    const struct deferred_tile_vertex verts[] = {
        {{tile_sz, 0.0f, 1.0f, 1.0f}, {coord_sz, 0.0f}},
        {{0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{tile_sz, tile_sz, 1.0f, 1.0f}, {coord_sz, coord_sz}},
        {{0.0f, tile_sz, 1.0f, 1.0f}, {0.0f, coord_sz}}
    };
#endif


    g_deferred->tile_buff = gfx_create_buffer(gfxBufferType::VERTEX, gfxMemHint::STATIC,
        sizeof(struct deferred_tile_vertex)*4, verts, 0);
    if (g_deferred->tile_buff == NULL)
        return RET_FAIL;

    const struct gfx_input_element_binding inputs[] = {
        {gfxInputElemId::POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
        {gfxInputElemId::TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
    };

    const struct gfx_input_vbuff_desc vbuffs[] = {
        {sizeof(struct deferred_tile_vertex), g_deferred->tile_buff}
    };

    g_deferred->tile_il = gfx_create_inputlayout(vbuffs, GFX_INPUTVB_GETCNT(vbuffs),
        inputs, GFX_INPUT_GETCNT(inputs), NULL, gfxIndexType::UNKNOWN, 0);
    if (g_deferred->tile_il == NULL)
        return RET_FAIL;
    return RET_OK;
}

void deferred_destroytilequad()
{
    if (g_deferred->tile_il != NULL)
        gfx_destroy_inputlayout(g_deferred->tile_il);
    if (g_deferred->tile_buff != NULL)
        gfx_destroy_buffer(g_deferred->tile_buff);
}

void deferred_drawlights(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params,
    const struct gfx_renderpass_lightdata* lightdata)
{
    struct vec4f coord;
    struct color clr;
    vec4_setf(&coord, 0.0f, 0.0f, 1.0f, 1.0f);

    for (uint i = 0; i < lightdata->cnt; i++) {
        struct scn_render_light* light = &lightdata->lights[i];
        struct cmp_light* ldata = (struct cmp_light*)cmp_getinstancedata(light->light_hdl);
        color_setc(&clr, &ldata->color_lin);
        clr.a = ldata->intensity * light->intensity_mul;

        gfx_blb_push(cmdqueue, &ldata->pos, &clr, ldata->atten_narrow*1.5f,
            ldata->atten_narrow*1.5f, rs_get_texture(g_deferred->light_tex), &coord);
    }
}
