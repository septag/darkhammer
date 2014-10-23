/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
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

#include "dhcore/core.h"
#include "dhcore/task-mgr.h"

#include "renderpaths/gfx-csm.h"

#include "gfx-device.h"
#include "gfx.h"
#include "engine.h"
#include "gfx-shader.h"
#include "mem-ids.h"
#include "camera.h"
#include "gfx-cmdqueue.h"
#include "prf-mgr.h"
#include "gfx-model.h"
#include "scene-mgr.h"
#include "res-mgr.h"
#include "console.h"
#include "debug-hud.h"

#define CSM_SHADER_CNT 4
#define CSM_CASCADE_CNT 3
#define CSM_SHADOW_SIZE 1024
#define CSM_FAR_MAX 50.0f
#define CSM_PREV_SIZE 256

/*************************************************************************************************
 * types
 */
struct csm_shader
{
    uint rpath_flags;
    uint shader_id;
};

struct ALIGN16 csm_cascade
{
    struct sphere bounds;
    struct mat3f view;
    struct mat4f proj;
    float nnear;
    float nfar;
};

struct gfx_csm
{
	float shadowmap_size;	/* width/height of the shadow map */
	gfx_rendertarget shadow_rt;
	gfx_rendertarget prev_rt;
	gfx_texture shadow_tex; /* shadow map (array(d3d10.1+) or cube(d3d10)) */
	gfx_texture prev_tex[CSM_CASCADE_CNT];
    uint shader_cnt;
    struct csm_shader shaders[CSM_SHADER_CNT];
    struct vec4f cascade_planes[CSM_SHADER_CNT*4];  /* 4 planes for each cascade instead of 6 */
    uint prev_shader;
    struct gfx_cblock* cb_frame;
    struct gfx_cblock* cb_xforms;
    struct gfx_cblock* cb_frame_gs;
    struct gfx_cblock* tb_skins;
    gfx_rasterstate rs_bias;
    gfx_rasterstate rs_bias_doublesided;
    gfx_depthstencilstate ds_depth;
    struct csm_cascade cascades[CSM_CASCADE_CNT];
    struct frustum cascade_frusts[CSM_CASCADE_CNT];
    struct mat4f cascade_vps[CSM_CASCADE_CNT];
    struct mat4f shadow_mats[CSM_CASCADE_CNT];
    struct aabb frustum_bounds;
    struct vec3f light_dir;
    int debug_csm;
    gfx_sampler sampl_linear;
    struct gfx_sharedbuffer* sharedbuff;    /* shared buffer for csm drawing pass */
};

/*************************************************************************************************
 * fwd declarations
 */
result_t csm_create_shadowrt(uint width, uint height);
void csm_destroy_shadowrt();
result_t csm_create_prevrt(uint width, uint height);
void csm_destroy_prevrt();
int csm_load_shaders(struct allocator* alloc);
void csm_unload_shaders();
int csm_load_prev_shaders(struct allocator* alloc);
void csm_unload_prev_shaders();

int csm_add_shader(uint shader_id, uint rpath_flags);
result_t csm_create_states();
void csm_destroy_states();

void csm_split_range(float nnear, float nfar, float splits[CSM_CASCADE_CNT+1]);
void csm_calc_minsphere(struct sphere* bounds, const struct frustum* f, const struct mat3f* view,
    const struct mat3f* view_inv);
struct mat4f* csm_calc_orthoproj(struct mat4f* r, float w, float h, float zn, float zf);
struct mat4f* csm_round_mat(struct mat4f* r, const struct mat4f* m, float shadow_size);

void csm_drawbatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader, uint xforms_shared_idx);
void csm_preparebatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader);
void csm_submit_batchdata(gfx_cmdqueue cmdqueue, struct gfx_batch_item* batch_items,
        uint batch_cnt, struct gfx_sharedbuffer* shared_buff);
void csm_renderpreview(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);

/* console commands */
result_t csm_console_debugcsm(uint argc, const char** argv, void* param);

/*************************************************************************************************
 * globals
 */
static struct gfx_csm* g_csm = NULL;

/*************************************************************************************************/
uint gfx_csm_getshader(enum cmp_obj_type obj_type, uint rpath_flags)
{
    for (uint i = 0, cnt = g_csm->shader_cnt; i < cnt; i++)    {
        struct csm_shader* sh = &g_csm->shaders[i];
        if ((rpath_flags & sh->rpath_flags) == rpath_flags)
            return sh->shader_id;
    }
    return 0;
}

result_t gfx_csm_init(uint width, uint height)
{
	result_t r;

    log_printf(LOG_INFO, "init csm render-path ...");

    struct allocator* lsr_alloc = eng_get_lsralloc();
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);

    g_csm = (struct gfx_csm*)ALIGNED_ALLOC(sizeof(struct gfx_csm), MID_GFX);
    if (g_csm == NULL)
        return RET_OUTOFMEMORY;
    memset(g_csm, 0x00, sizeof(struct gfx_csm));

    /* render targets and buffers */
	r = csm_create_shadowrt(CSM_SHADOW_SIZE, CSM_SHADOW_SIZE);
	if (IS_FAIL(r))	{
		err_print(__FILE__, __LINE__, "gfx-csm init failed: could not create shadow map buffers");
		return RET_FAIL;
	}

	if (BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DEV))	{
        if (!csm_load_prev_shaders(lsr_alloc))  {
            err_print(__FILE__, __LINE__, "gfx-csm init failed: could not load preview shaders");
            return RET_FAIL;
        }

		r = csm_create_prevrt(CSM_PREV_SIZE, CSM_PREV_SIZE);
		if (IS_FAIL(r))	{
			err_print(__FILE__, __LINE__, "gfx-csm init failed: could not create prev buffers");
			return RET_FAIL;
		}

        /* console commands */
        con_register_cmd("gfx_debugcsm", csm_console_debugcsm, NULL, "gfx_debugcsm [1*/0]");
	}

    /* shaders */
    if (!csm_load_shaders(lsr_alloc))   {
        err_print(__FILE__, __LINE__, "gfx-csm init failed: could not load shaders");
        return RET_FAIL;
    }

    /* cblocks */
    if (gfx_check_feature(GFX_FEATURE_RANGED_CBUFFERS)) {
        g_csm->sharedbuff = gfx_sharedbuffer_create(GFX_DEFAULT_RENDER_OBJ_CNT*GFX_INSTANCES_MAX*48);
        if (g_csm->sharedbuff == NULL)  {
            err_print(__FILE__, __LINE__, "gfx-deferred init failed: could not create uniform buffer");
            return RET_FAIL;
        }
    }

    g_csm->cb_frame = gfx_shader_create_cblock(lsr_alloc, tmp_alloc,
        gfx_shader_get(g_csm->shaders[0].shader_id), "cb_frame", NULL);
    g_csm->cb_xforms = gfx_shader_create_cblock(lsr_alloc, tmp_alloc,
        gfx_shader_get(g_csm->shaders[0].shader_id), "cb_xforms", g_csm->sharedbuff);
    g_csm->cb_frame_gs = gfx_shader_create_cblock(lsr_alloc, tmp_alloc,
        gfx_shader_get(g_csm->shaders[0].shader_id), "cb_frame_gs", NULL);
    g_csm->tb_skins = gfx_shader_create_cblock_tbuffer(mem_heap(),
        gfx_shader_get(gfx_csm_getshader(CMP_OBJTYPE_MODEL, GFX_RPATH_SKINNED | GFX_RPATH_CSMSHADOW)),
        "tb_skins", sizeof(struct vec4f)*3*GFX_INSTANCES_MAX*GFX_SKIN_BONES_MAX);
    if (g_csm->cb_frame == NULL || g_csm->cb_xforms == NULL || g_csm->cb_frame_gs == NULL ||
        g_csm->tb_skins == NULL)
    {
        err_print(__FILE__, __LINE__, "gfx-csm init failed: could not create cblocks");
        return RET_FAIL;
    }

    /* states */
    r = csm_create_states();
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "gfx-csm init failed: could not create states");
        return RET_FAIL;
    }

    g_csm->shadowmap_size = (float)CSM_SHADOW_SIZE;

	return RET_OK;
}

void gfx_csm_release()
{
    if (g_csm != NULL)  {
        if (g_csm->sharedbuff != NULL)
            gfx_sharedbuffer_destroy(g_csm->sharedbuff);

        csm_destroy_states();

        if (g_csm->cb_frame != NULL)
            gfx_shader_destroy_cblock(g_csm->cb_frame);
        if (g_csm->cb_xforms != NULL)
            gfx_shader_destroy_cblock(g_csm->cb_xforms);
        if (g_csm->cb_frame_gs != NULL)
            gfx_shader_destroy_cblock(g_csm->cb_frame_gs);
        if (g_csm->tb_skins != NULL)
            gfx_shader_destroy_cblock(g_csm->tb_skins);

        csm_unload_prev_shaders();
        csm_unload_shaders();
	    csm_destroy_shadowrt();
	    csm_destroy_prevrt();

        ALIGNED_FREE(g_csm);
        g_csm = NULL;
    }
}

void gfx_csm_render(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
        const struct gfx_view_params* params, struct gfx_batch_item* batch_items, uint batch_cnt,
        void* userdata, OUT struct gfx_rpath_result* result)
{
    ASSERT(batch_cnt != 0);

    PRF_OPENSAMPLE("rpath-csm");

    int supports_shared_cbuff = gfx_check_feature(GFX_FEATURE_RANGED_CBUFFERS);
    if (supports_shared_cbuff)
        csm_submit_batchdata(cmdqueue, batch_items, batch_cnt, g_csm->sharedbuff);

    gfx_cmdqueue_resetsrvs(cmdqueue);
    gfx_output_setrendertarget(cmdqueue, g_csm->shadow_rt);
    gfx_output_setviewport(cmdqueue, 0, 0, CSM_SHADOW_SIZE, CSM_SHADOW_SIZE);
    gfx_output_setrasterstate(cmdqueue, g_csm->rs_bias);
    gfx_output_setdepthstencilstate(cmdqueue, g_csm->ds_depth, 0);
    gfx_output_clearrendertarget(cmdqueue, g_csm->shadow_rt, NULL, 1.0f, 0, GFX_CLEAR_DEPTH);

    struct gfx_cblock* cb_frame = g_csm->cb_frame;
    struct gfx_cblock* cb_frame_gs = g_csm->cb_frame_gs;
    struct mat3f* views[CSM_CASCADE_CNT];
    float fovfactors[4];
    float texelsz[4] = {1.0f / (float)CSM_SHADOW_SIZE, 0, 0, 0};
    for (uint i = 0; i < CSM_CASCADE_CNT && i < 4; i++)    {
        views[i] = &g_csm->cascades[i].view;
        fovfactors[i] = maxf(g_csm->cascades[i].proj.m11, g_csm->cascades[i].proj.m22);
    }

    gfx_cb_set4f(cb_frame, SHADER_NAME(c_texelsz), texelsz);
    gfx_cb_set4f(cb_frame, SHADER_NAME(c_fovfactors), fovfactors);
    gfx_cb_set4f(cb_frame, SHADER_NAME(c_lightdir), g_csm->light_dir.f);
    gfx_cb_set3mvp(cb_frame, SHADER_NAME(c_views), (const struct mat3f**)views, CSM_CASCADE_CNT);
    gfx_cb_set4mv(cb_frame, SHADER_NAME(c_cascade_mats), g_csm->cascade_vps, CSM_CASCADE_CNT);
    gfx_shader_updatecblock(cmdqueue, cb_frame);

    gfx_cb_set4fv(cb_frame_gs, SHADER_NAME(c_cascade_planes), g_csm->cascade_planes,
        4*CSM_CASCADE_CNT);
    gfx_shader_updatecblock(cmdqueue, cb_frame_gs);

    for (uint i = 0; i < batch_cnt; i++)  {
        struct gfx_batch_item* bitem = &batch_items[i];
        struct gfx_shader* shader = gfx_shader_get(bitem->shader_id);
        ASSERT(shader);
        gfx_shader_bind(cmdqueue, shader);

        /* do not send cb_xforms to shader if we are using shared buffer (bind later before draw) */
        struct gfx_cblock* cbs[3];
        uint xforms_shared_idx;
        if (supports_shared_cbuff)  {
            cbs[0] = cb_frame;
            cbs[1] = cb_frame_gs;
            xforms_shared_idx = 2;
        }   else    {
            cbs[0] = cb_frame;
            cbs[1] = cb_frame_gs;
            cbs[2] = g_csm->cb_xforms;
            xforms_shared_idx = 3;
        }
        gfx_shader_bindcblocks(cmdqueue, shader, (const struct gfx_cblock**)cbs, xforms_shared_idx);

        /* batch draw */
        for (int k = 0; k < bitem->nodes.item_cnt; k++)  {
            struct gfx_batch_node* bnode_first = &((struct gfx_batch_node*)bitem->nodes.buffer)[k];

            csm_preparebatchnode(cmdqueue, bnode_first, shader);
            if (bnode_first->poses[0] != NULL)  {
                gfx_shader_bindcblock_tbuffer(cmdqueue, shader, SHADER_NAME(tb_skins),
                    g_csm->tb_skins);
            }

            struct linked_list* node = bnode_first->bll;
            while (node != NULL)    {
                struct gfx_batch_node* bnode = (struct gfx_batch_node*)node->data;
                csm_drawbatchnode(cmdqueue, bnode, shader, xforms_shared_idx);
                node = node->next;
            }
        }
    }

    /* switch back */
    gfx_output_setrasterstate(cmdqueue, NULL);
    gfx_output_setdepthstencilstate(cmdqueue, NULL, 0);

    if (g_csm->debug_csm)
        csm_renderpreview(cmdqueue, params);

    PRF_CLOSESAMPLE();  /* csm */
}

/* prepass for submitting per-object (xforms) data to shaders (uniform sharing required)
 * offset/size data will be assigned into meta_data member of each batch */
void csm_submit_batchdata(gfx_cmdqueue cmdqueue, struct gfx_batch_item* batch_items,
                          uint batch_cnt, struct gfx_sharedbuffer* shared_buff)
{
    gfx_sharedbuffer_reset(g_csm->sharedbuff);
    for (uint i = 0; i < batch_cnt; i++)	{
        struct gfx_batch_item* bitem = &batch_items[i];

        for (int k = 0; k < bitem->nodes.item_cnt; k++)	{
            struct gfx_batch_node* bnode_first =
                &((struct gfx_batch_node*)bitem->nodes.buffer)[k];
            struct linked_list* node = bnode_first->bll;
            while (node != NULL)	{
                struct gfx_batch_node* bnode = (struct gfx_batch_node*)node->data;
                gfx_cb_set3mvp(g_csm->cb_xforms, SHADER_NAME(c_mats),
                    bnode->instance_mats, bnode->instance_cnt);
                bnode->meta_data = gfx_sharedbuffer_write(shared_buff,
                    cmdqueue, g_csm->cb_xforms->cpu_buffer, 48*bnode->instance_cnt);
                node = node->next;
            }
        }	/* for: each batch-item */
    }
}

void csm_preparebatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader)
{
    struct scn_render_model* rmodel = (struct scn_render_model*)bnode->ritem;
    struct gfx_model* gmodel = rmodel->gmodel;
    struct gfx_model_instance* inst = rmodel->inst;
    struct gfx_model_mesh* mesh = &gmodel->meshes[gmodel->nodes[rmodel->node_idx].mesh_id];
    struct gfx_model_geo* geo = &gmodel->geos[mesh->geo_id];
    gfx_sampler sampler = gfx_get_globalsampler();

    /* set diffuse texture for alpha-test */
    if (bnode->sub_idx != INVALID_INDEX)    {
        uint mtl_id = mesh->submeshes[bnode->sub_idx].mtl_id;

        struct gfx_model_mtlgpu* gmtl = inst->mtls[mtl_id];
        if (gmtl->textures[GFX_MODEL_DIFFUSEMAP] != INVALID_HANDLE)		{
            uint name_hash = SHADER_NAME(s_mtl_diffusemap);
            if (gfx_shader_isvalidtex(shader, name_hash))    {
                gfx_shader_bindsamplertexture(cmdqueue, shader, name_hash, sampler,
                    rs_get_texture(gmtl->textures[GFX_MODEL_DIFFUSEMAP]));
            }
        }
    }

    gfx_input_setlayout(cmdqueue, geo->inputlayout);
}

void csm_drawbatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
    struct gfx_shader* shader, uint xforms_shared_idx)
{
    struct scn_render_model* rmodel = (struct scn_render_model*)bnode->ritem;
    struct gfx_model* gmodel = rmodel->gmodel;
    struct gfx_model_mesh* mesh = &gmodel->meshes[gmodel->nodes[rmodel->node_idx].mesh_id];
    struct gfx_model_geo* geo = &gmodel->geos[mesh->geo_id];

    /* set transform matricesbind only for shared mode (we have updated the buffer in a prepass) */
    struct gfx_cblock* cb_xforms = g_csm->cb_xforms;
    if (cb_xforms->shared_buff != NULL)  {
        sharedbuffer_pos_t pos = bnode->meta_data;
        gfx_shader_bindcblock_shared(cmdqueue, shader, g_csm->cb_xforms,
            cb_xforms->shared_buff->gpu_buff,
            GFX_SHAREDBUFFER_OFFSET(pos), GFX_SHAREDBUFFER_SIZE(pos), xforms_shared_idx);
    }   else    {
        gfx_cb_set3mvp(g_csm->cb_xforms, SHADER_NAME(c_mats), bnode->instance_mats,
            bnode->instance_cnt);
        gfx_shader_updatecblock(cmdqueue, g_csm->cb_xforms);
    }

    /* skin data */
    if (bnode->poses[0] != NULL)   {
        struct gfx_cblock* tb_skins = g_csm->tb_skins;
        for (uint i = 0, cnt = bnode->instance_cnt; i < cnt; i++) {
            const struct gfx_model_posegpu* pose = bnode->poses[i];

            gfx_cb_set3mv_offset(tb_skins, 0, pose->skin_mats, pose->mat_cnt,
                i*GFX_SKIN_BONES_MAX*sizeof(struct vec4f)*3);
        }

        gfx_shader_updatecblock(cmdqueue, tb_skins);
    }

    /* draw */
    if (bnode->sub_idx == INVALID_INDEX)    {
        gfx_draw_indexedinstance(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, 0, geo->tri_cnt*3,
            geo->ib_type, bnode->instance_cnt, GFX_DRAWCALL_SUNSHADOW);
    }   else    {
        uint mtl_id = mesh->submeshes[bnode->sub_idx].mtl_id;
        uint subset_idx = mesh->submeshes[bnode->sub_idx].subset_id;
        struct gfx_model_geosubset* subset = &geo->subsets[subset_idx];

        int is_doublesided = BIT_CHECK(gmodel->mtls[mtl_id].flags, GFX_MODEL_MTLFLAG_DOUBLESIDED);
        if (is_doublesided)
            gfx_output_setrasterstate(cmdqueue, g_csm->rs_bias_doublesided);

        gfx_draw_indexedinstance(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, subset->ib_idx,
            subset->idx_cnt, geo->ib_type, bnode->instance_cnt, GFX_DRAWCALL_SUNSHADOW);

        /* switch back */
        if (is_doublesided)
            gfx_output_setrasterstate(cmdqueue, g_csm->rs_bias);
    }
}

result_t gfx_csm_resize(uint width, uint height)
{
	if (BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DEV))	{
		csm_destroy_prevrt();
		if (IS_FAIL(csm_create_prevrt(width, height)))
			return RET_FAIL;
	}
	return RET_OK;
}

result_t csm_create_shadowrt(uint width, uint height)
{
	enum gfx_hwver hwver = gfx_get_hwver();
	if (hwver == GFX_HWVER_D3D10_0 || hwver == GFX_HWVER_GL3_3 || hwver == GFX_HWVER_GL3_2)
	{
		g_csm->shadow_tex = gfx_create_texturert_cube(width, height, GFX_FORMAT_DEPTH32);
	}	else	{
		g_csm->shadow_tex = gfx_create_texturert_arr(width, height, CSM_CASCADE_CNT,
            GFX_FORMAT_DEPTH32);
	}

	if (g_csm->shadow_tex == NULL)
		return RET_FAIL;

	g_csm->shadow_rt = gfx_create_rendertarget(NULL, 0, g_csm->shadow_tex);
	if (g_csm->shadow_rt == NULL)
		return RET_FAIL;

	return RET_OK;
}

void csm_destroy_shadowrt()
{
	if (g_csm->shadow_rt != NULL)
		gfx_destroy_rendertarget(g_csm->shadow_rt);
	if (g_csm->shadow_tex != NULL)
		gfx_destroy_texture(g_csm->shadow_tex);
}

result_t csm_create_prevrt(uint width, uint height)
{
    for (uint i = 0; i < CSM_CASCADE_CNT; i++)    {
	    g_csm->prev_tex[i] = gfx_create_texturert(width, height, GFX_FORMAT_RGBA_UNORM, FALSE);
	    if (g_csm->prev_tex[i] == NULL)
		    return RET_FAIL;
    }

	g_csm->prev_rt = gfx_create_rendertarget(g_csm->prev_tex, CSM_CASCADE_CNT, NULL);
	if (g_csm->prev_rt == NULL)
		return RET_FAIL;

	return RET_OK;
}

void csm_destroy_prevrt()
{
	if (g_csm->prev_rt != NULL)
		gfx_destroy_rendertarget(g_csm->prev_rt);
	for (uint i = 0; i < CSM_CASCADE_CNT; i++)    {
        if (g_csm->prev_tex[i] != NULL)
		    gfx_destroy_texture(g_csm->prev_tex[i]);
    }
}

int csm_load_shaders(struct allocator* alloc)
{
    int r;
    char max_instances_str[8];
    char cascade_cnt_str[8];
    char max_bones_str[8];

    /* include all extra stuff in rpath flags (because csm-renderer can render them all) */
    uint extra_rpath = GFX_RPATH_DIFFUSEMAP | GFX_RPATH_NORMALMAP | GFX_RPATH_ALPHAMAP |
        GFX_RPATH_REFLECTIONMAP | GFX_RPATH_EMISSIVEMAP | GFX_RPATH_GLOSSMAP | GFX_RPATH_RAW;

    str_itos(max_instances_str, GFX_INSTANCES_MAX);
    str_itos(cascade_cnt_str, CSM_CASCADE_CNT);
    str_itos(max_bones_str, GFX_SKIN_BONES_MAX);

    /* for normal csm, do not load pixel-shader */
    gfx_shader_beginload(alloc, "shaders/csm.vs", NULL, "shaders/csm.gs", 1, "shaders/skin.inc");
    r = csm_add_shader(gfx_shader_add("csm-raw", 2, 2,
        GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
        GFX_INPUTELEMENT_ID_NORMAL, "vsi_norm", 0,
        "_MAX_INSTANCES_", max_instances_str,
        "_CASCADE_CNT_", cascade_cnt_str),
        GFX_RPATH_CSMSHADOW | extra_rpath);
    if (!r)
        return FALSE;
    r = csm_add_shader(gfx_shader_add("csm-skin", 4, 4,
        GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
        GFX_INPUTELEMENT_ID_NORMAL, "vsi_norm", 0,
        GFX_INPUTELEMENT_ID_BLENDINDEX, "vsi_blendidxs", 1,
        GFX_INPUTELEMENT_ID_BLENDWEIGHT, "vsi_blendweights", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_CASCADE_CNT_", cascade_cnt_str,
        "_SKIN_", "1",
        "_MAX_BONES_", max_bones_str),
        GFX_RPATH_CSMSHADOW | GFX_RPATH_SKINNED | extra_rpath);
    if (!r)
        return FALSE;
    gfx_shader_endload();

    /* for alpha-test shaders, load pixel-shader too */
    gfx_shader_beginload(alloc, "shaders/csm.vs", "shaders/csm.ps", "shaders/csm.gs", 1,
        "shaders/skin.inc");
    r = csm_add_shader(gfx_shader_add("csm-alpha", 3, 3,
        GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
        GFX_INPUTELEMENT_ID_NORMAL, "vsi_norm",  0,
        GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0,
        "_MAX_INSTANCES_", max_instances_str,
        "_CASCADE_CNT_", cascade_cnt_str,
        "_ALPHAMAP_", "1"),
        GFX_RPATH_CSMSHADOW | GFX_RPATH_ALPHAMAP | extra_rpath);
    if (!r)
        return FALSE;
    r = csm_add_shader(gfx_shader_add("csm-skin-alpha", 5, 5,
        GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
        GFX_INPUTELEMENT_ID_NORMAL, "vsi_norm", 0,
        GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0,
        GFX_INPUTELEMENT_ID_BLENDINDEX, "vsi_blendidxs", 1,
        GFX_INPUTELEMENT_ID_BLENDWEIGHT, "vsi_blendweights", 1,
        "_MAX_INSTANCES_", max_instances_str,
        "_CASCADE_CNT_", cascade_cnt_str,
        "_ALPHAMAP_", "1",
        "_SKIN_", "1",
        "_MAX_BONES_", max_bones_str),
        GFX_RPATH_CSMSHADOW | GFX_RPATH_SKINNED | GFX_RPATH_ALPHAMAP | extra_rpath);
    if (!r)
        return FALSE;
    gfx_shader_endload();

    return TRUE;
}

void csm_unload_shaders()
{
    for (uint i = 0; i < g_csm->shader_cnt; i++)
        gfx_shader_unload(g_csm->shaders[i].shader_id);
}

int csm_add_shader(uint shader_id, uint rpath_flags)
{
    ASSERT(g_csm->shader_cnt < CSM_SHADER_CNT);

    if (shader_id == 0)
        return FALSE;

    g_csm->shaders[g_csm->shader_cnt].rpath_flags = rpath_flags;
    g_csm->shaders[g_csm->shader_cnt].shader_id = shader_id;
    g_csm->shader_cnt ++;

    return TRUE;
}

result_t csm_create_states()
{
    struct gfx_rasterizer_desc rdesc;
    memcpy(&rdesc, gfx_get_defaultraster(), sizeof(rdesc));

    rdesc.cull = GFX_CULL_FRONT;
    if ((g_csm->rs_bias = gfx_create_rasterstate(&rdesc)) == NULL)
        return RET_FAIL;

    rdesc.cull = GFX_CULL_NONE;
    if ((g_csm->rs_bias_doublesided = gfx_create_rasterstate(&rdesc)) == NULL)
        return RET_FAIL;

    /* depth-stencil */
    struct gfx_depthstencil_desc dsdesc;
    memcpy(&dsdesc, gfx_get_defaultdepthstencil(), sizeof(dsdesc));
    dsdesc.depth_enable = TRUE;
    dsdesc.depth_write = TRUE;
    g_csm->ds_depth = gfx_create_depthstencilstate(&dsdesc);
    if (g_csm->ds_depth == NULL)
        return RET_FAIL;

    /* samplers */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.filter_min = GFX_FILTER_LINEAR;
    sdesc.filter_mag = GFX_FILTER_LINEAR;
    sdesc.filter_mip = GFX_FILTER_UNKNOWN;
    g_csm->sampl_linear = gfx_create_sampler(&sdesc);
    if (g_csm->sampl_linear == NULL)
        return RET_FAIL;

    return RET_OK;
}

void csm_destroy_states()
{
    if (g_csm->ds_depth != NULL)
        gfx_destroy_depthstencilstate(g_csm->ds_depth);

    if (g_csm->rs_bias != NULL)
        gfx_destroy_rasterstate(g_csm->rs_bias);

    if (g_csm->rs_bias_doublesided != NULL)
        gfx_destroy_rasterstate(g_csm->rs_bias_doublesided);

    if (g_csm->sampl_linear != NULL)
        gfx_destroy_sampler(g_csm->sampl_linear);
}

void csm_calc_cascadeplanes(struct vec4f* planes, const struct plane vp_planes[6],
    const struct mat4f* cmat)
{
    /* near plane */
    const struct plane* p;
    struct vec4f pv;
    struct mat4f mt;

    mat4_inv(&mt, cmat);
    mat4_transpose_self(&mt);

    /* right plane */
    p = &vp_planes[CAM_FRUSTUM_RIGHT];
    vec4_setf(&pv, p->nx, p->ny, p->nz, p->d);
    vec4_transform(&planes[0], &pv, &mt);

    /* left plane */
    p = &vp_planes[CAM_FRUSTUM_LEFT];
    vec4_setf(&pv, p->nx, p->ny, p->nz, p->d);
    vec4_transform(&planes[1], &pv, &mt);

    /* top plane */
    p = &vp_planes[CAM_FRUSTUM_TOP];
    vec4_setf(&pv, p->nx, p->ny, p->nz, p->d);
    vec4_transform(&planes[2], &pv, &mt);

    /* bottom plane */
    p = &vp_planes[CAM_FRUSTUM_BOTTOM];
    vec4_setf(&pv, p->nx, p->ny, p->nz, p->d);
    vec4_transform(&planes[3], &pv, &mt);
}

void gfx_csm_prepare(const struct gfx_view_params* params, const struct vec3f* light_dir,
    const struct aabb* world_bounds)
{
    static const float tolerance = 10.0f;

    struct vec3f dir;
    vec3_setv(&dir, light_dir);

    float texoffset_x = 0.5f + (0.5f/g_csm->shadowmap_size);
    float texoffset_y = 0.5f + (0.5f/g_csm->shadowmap_size);

    struct mat3f view_inv;
    struct mat4f tex_mat;
    struct mat4f tmp_mat;
    float splits[CSM_CASCADE_CNT+1];

    mat3_setf(&view_inv,
        params->view.m11, params->view.m21, params->view.m31,
        params->view.m12, params->view.m22, params->view.m32,
        params->view.m13, params->view.m23, params->view.m33,
        params->cam_pos.x, params->cam_pos.y, params->cam_pos.z);
    mat4_setf(&tex_mat,
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        texoffset_x, texoffset_y, 0.0f, 1.0f);

    float csm_far = minf(CSM_FAR_MAX, params->cam->ffar);
    csm_split_range(params->cam->fnear, csm_far, splits);

    /* calculate cascades */
    struct frustum f;   /* frustum points for cascades */
    struct plane vp_planes[6];

    for (uint i = 0; i < CSM_CASCADE_CNT; i++)    {
        cam_calc_frustumcorners(params->cam, (struct vec3f*)f.points, &splits[i], &splits[i+1]);
        csm_calc_minsphere(&g_csm->cascades[i].bounds, &f, &params->view, &view_inv);
        memcpy(&g_csm->cascade_frusts[i], &f, sizeof(f));

        /* cascade matrixes: first we find two extreme points of the world, related to cascade */
        struct vec3f scenter;
        struct ray r;
        struct plane upper_plane;
        struct plane lower_plane;
        struct vec3f upper_pt;
        struct vec3f lower_pt;
        struct vec3f xaxis;
        struct vec3f yaxis;
        struct vec3f viewpos;
        struct vec3f tmp;
        struct vec3f lowerpt_vs;

        float sr = g_csm->cascades[i].bounds.r;
        vec3_setf(&scenter, g_csm->cascades[i].bounds.x, g_csm->cascades[i].bounds.y,
            g_csm->cascades[i].bounds.z);
        ray_setv(&r, &scenter, &dir);
        plane_setv(&upper_plane, &g_vec3_unity_neg, fabs(world_bounds->maxpt.y));
        plane_setv(&lower_plane, &g_vec3_unity, fabs(world_bounds->minpt.y));
        vec3_sub(&upper_pt, &r.pt,
            vec3_muls(&tmp, &dir, fabs(ray_intersect_plane(&r, &upper_plane)) + tolerance));
        vec3_add(&lower_pt, &r.pt,
            vec3_muls(&tmp, &dir, fabs(ray_intersect_plane(&r, &lower_plane)) + tolerance));

        /* view matrix of light view for the cascade :
         * dir = light_dir
         * up = (1, 0, 0)
         * pos = sphere center
         */
        vec3_norm(&xaxis, vec3_cross(&tmp, &g_vec3_unitx, &dir));
        vec3_cross(&yaxis, &dir, &xaxis);
        vec3_sub(&viewpos, &upper_pt, vec3_muls(&tmp, &dir, tolerance));
        mat3_setf(&g_csm->cascades[i].view,
            xaxis.x, yaxis.x, dir.x,
            xaxis.y, yaxis.y, dir.y,
            xaxis.z, yaxis.z, dir.z,
            -vec3_dot(&xaxis, &viewpos), -vec3_dot(&yaxis, &viewpos), -vec3_dot(&dir, &viewpos));

        /* orthographic projection matrix for cascade */
        vec3_transformsrt(&lowerpt_vs, &lower_pt, &g_csm->cascades[i].view);
        float nnear = tolerance*0.5f;
        float nfar = lowerpt_vs.z + sr;
        csm_calc_orthoproj(&g_csm->cascades[i].proj,
            sr*2.0f + 1.0f/g_csm->shadowmap_size,
            sr*2.0f + 1.0f/g_csm->shadowmap_size,
            nnear, nfar);
        g_csm->cascades[i].nnear = nnear;
        g_csm->cascades[i].nfar = nfar;

        /* calculate final matrix */
        mat3_mul4(&g_csm->cascade_vps[i], &g_csm->cascades[i].view, &g_csm->cascades[i].proj);

        cam_calc_frustumplanes(vp_planes, &g_csm->cascade_vps[i]);
        csm_calc_cascadeplanes(&g_csm->cascade_planes[i*4], vp_planes, &g_csm->cascade_vps[i]);

        csm_round_mat(&g_csm->cascade_vps[i], &g_csm->cascade_vps[i], g_csm->shadowmap_size);

        mat4_mul(&g_csm->shadow_mats[i],
            mat3_mul4(&tmp_mat, &view_inv, &g_csm->cascade_vps[i]), &tex_mat);
    }

    /* caculate shadow area bounds (aabb) */
    struct aabb cascade_near;
    struct aabb cascade_far;
    aabb_setzero(&g_csm->frustum_bounds);

    aabb_from_sphere(&cascade_near, &g_csm->cascades[0].bounds);
    aabb_from_sphere(&cascade_far, &g_csm->cascades[CSM_CASCADE_CNT-1].bounds);
    aabb_merge(&g_csm->frustum_bounds, &cascade_near, &cascade_far);

    vec3_setv(&g_csm->light_dir, &dir);
}

void csm_split_range(float nnear, float nfar, float splits[CSM_CASCADE_CNT+1])
{
    /* Practical split scheme:
     *
     * CLi = n*(f/n)^(i/numsplits)
     * CUi = n + (f-n)*(i/numsplits)
     * Ci = CLi*(lambda) + CUi*(1-lambda)
     *
     * lambda scales between logarithmic and uniform */
    static const float weight = 0.75f;

    for (uint i = 0; i < CSM_CASCADE_CNT; i++)	{
        float idm = ((float)i) / CSM_CASCADE_CNT;
        float nlog = nnear * powf(nfar/nnear, idm);
        float nuniform = nnear + (nfar - nnear)*idm;
        splits[i] = nlog*weight + nuniform*(1.0f - weight);
    }

    splits[0] = nnear;
    splits[CSM_CASCADE_CNT] = nfar;
}

void csm_calc_minsphere(struct sphere* bounds, const struct frustum* f,
    const struct mat3f* view, const struct mat3f* view_inv)
{
    /* reference:
     * http://www.gamedev.net/topic/604797-minimum-bounding-sphere-of-a-frustum/
     */
    struct vec3f a;
    struct vec3f b;
    struct vec3f p;
    struct vec3f tmp;

    vec3_transformsrt(&a, &f->points[1], view);
    vec3_transformsrt(&b, &f->points[5], view);

    float z = (vec3_dot(&b, &b) - vec3_dot(&a, &a))/(2.0f*(b.z - a.z));
    vec3_setf(&p, 0.0f, 0.0f, z);
    vec3_transformsrt(&p, &p, view_inv);

    sphere_setf(bounds, p.x, p.y, p.z, vec3_len(vec3_sub(&tmp, &f->points[5], &p)) + 0.01f);
}

struct mat4f* csm_calc_orthoproj(struct mat4f* r, float w, float h, float zn, float zf)
{
    return mat4_setf(r,
        2.0f/w, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f/h, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f/(zf-zn), 0.0f,
        0.0f, 0.0f, zn/(zn-zf), 1.0f);
}

/* reference: shaderX6 - cascade shadow stabilization (micheal villant) */
struct mat4f* csm_round_mat(struct mat4f* r, const struct mat4f* m, float shadow_size)
{
    struct vec3f origin;
    vec3_transformsrt_m4(&origin, &g_vec3_zero, m);
    float texcoord_x = origin.x*shadow_size*0.5f;
    float texcoord_y = origin.y*shadow_size*0.5f;
    float texcoord_x_round = math_round(texcoord_x);
    float texcoord_y_round = math_round(texcoord_y);
    float dx = texcoord_x_round - texcoord_x;
    float dy = texcoord_y_round - texcoord_y;
    dx /= shadow_size*0.5f;
    dy /= shadow_size*0.5f;

    struct mat4f round_mat;
    mat4_set_ident(&round_mat);
    round_mat.m41 = dx;
    round_mat.m42 = dy;
    round_mat.m43 = 0.0f;
    return mat4_mul(r, m, &round_mat);
}

uint gfx_csm_get_cascadecnt()
{
    return CSM_CASCADE_CNT;
}

const struct aabb* gfx_csm_get_frustumbounds()
{
    return &g_csm->frustum_bounds;
}

const struct mat4f* gfx_csm_get_shadowmats()
{
    return g_csm->shadow_mats;
}

gfx_texture gfx_csm_get_shadowtex()
{
    return g_csm->shadow_tex;
}

const struct vec4f* gfx_csm_get_cascades(const struct mat3f* view)
{
    static struct vec4f cascades[CSM_CASCADE_CNT];
    struct vec4f center;
    for (uint i = 0; i < CSM_CASCADE_CNT; i++)    {
        const struct sphere* s = &g_csm->cascades[i].bounds;
        vec3_setf(&center, s->x, s->y, s->z);
        vec3_transformsrt(&center, &center, view);
        vec4_setf(&cascades[i], center.x, center.y, center.z, s->r);
    }
    return cascades;
}

result_t csm_console_debugcsm(uint argc, const char** argv, void* param)
{
    int enable = TRUE;
    if (argc == 1)
        enable = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;
    g_csm->debug_csm = enable;

    for (int i = 0; i < CSM_CASCADE_CNT; i++)    {
        char num[10];
        char alias[32];
        strcat(strcpy(alias, "CSM"), str_itos(num, i));
        if (enable)
            hud_add_image(alias, g_csm->prev_tex[i], FALSE, CSM_PREV_SIZE, CSM_PREV_SIZE, alias);
        else
            hud_remove_image(alias);
    }

    return RET_OK;
}

int csm_load_prev_shaders(struct allocator* alloc)
{
    char cascadecnt[10];
    enum gfx_hwver hwver = gfx_get_hwver();

    gfx_shader_beginload(alloc, "shaders/fsq.vs", "shaders/csm-prev.ps", NULL, 1,
        "shaders/common.inc");
    if (hwver == GFX_HWVER_D3D10_0 || hwver == GFX_HWVER_GL3_2 || hwver == GFX_HWVER_GL3_3)    {
        g_csm->prev_shader = gfx_shader_add("csm-prev", 2, 2,
            GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
            GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0,
            "_CASCADE_CNT_", str_itos(cascadecnt, CSM_CASCADE_CNT), "_D3D10_", "1");
    }    else   {
        g_csm->prev_shader = gfx_shader_add("csm-prev", 2, 1,
            GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
            GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0,
            "_CASCADE_CNT_", str_itos(cascadecnt, CSM_CASCADE_CNT));
    }

    gfx_shader_endload();
    if (g_csm->prev_shader == 0)
        return FALSE;

    return TRUE;
}

void csm_unload_prev_shaders()
{
    if (g_csm->prev_shader != 0)
        gfx_shader_unload(g_csm->prev_shader);
}

void csm_renderpreview(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params)
{
    struct gfx_shader* shader = gfx_shader_get(g_csm->prev_shader);

    gfx_output_setrendertarget(cmdqueue, g_csm->prev_rt);
    gfx_output_setviewport(cmdqueue, 0, 0, CSM_PREV_SIZE, CSM_PREV_SIZE);
    gfx_shader_bind(cmdqueue, shader);

    /* constants */
    struct vec4f orthoparams[CSM_CASCADE_CNT];
    float max_fars[CSM_CASCADE_CNT];
    for (uint i = 0; i < CSM_CASCADE_CNT; i++)    {
        const struct mat4f* ortho = &g_csm->cascades[i].proj;
        vec4_setf(&orthoparams[i], ortho->m11, ortho->m22, ortho->m33, ortho->m43);
        max_fars[i] = g_csm->cascades[i].nfar;
    }

    gfx_shader_set4fv(shader, SHADER_NAME(c_orthoparams), orthoparams, CSM_CASCADE_CNT);
    gfx_shader_setfv(shader, SHADER_NAME(c_max_far), max_fars, CSM_CASCADE_CNT);
    gfx_shader_bindconstants(cmdqueue, shader);

    /* textures */
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_shadowmap), g_csm->sampl_linear,
        g_csm->shadow_tex);

    /* draw */
    gfx_draw_fullscreenquad();

    gfx_set_previewrenderflag();
}
