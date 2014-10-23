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
#include "dhcore/color.h"
#include "dhcore/task-mgr.h"

#include "gfx-device.h"
#include "gfx-billboard.h"
#include "gfx-shader.h"
#include "gfx-cmdqueue.h"
#include "gfx-buffers.h"
#include "engine.h"
#include "mem-ids.h"

#define BILLBOARDS_BUFF_SIZE 512
#define BUFF_SEGMENTS 2

/*************************************************************************************************
 * types
 */
struct blb_mgr
{
    uint shader_id;
    struct gfx_cblock* cb_frame;
    gfx_buffer vbuff;
    struct gfx_ringbuffer rbuff;   /* ring-buffer, wrapper for vbuff */
    void* buff;    /* temp buffer for billboard storage, size of each = 1/BUFF_SEGMENTS */
    uint blb_idx; /* index of the current billboard */
    uint blb_cnt;
    gfx_texture tex;
    gfx_blendstate blend_alpha;
    gfx_sampler sampl_lin;
    gfx_depthstencilstate ds_ztest;
    gfx_inputlayout il;
};

struct ALIGN16 blb_vertex
{
    struct vec3f pos;
    struct vec4f coord;
    struct vec4f billboard;
    struct color color;
};

/*************************************************************************************************
 * globals
 */
static struct blb_mgr g_blb;

/*************************************************************************************************/
void gfx_blb_zero()
{
    memset(&g_blb, 0x00, sizeof(g_blb));
}

result_t gfx_blb_init()
{
    /* shaders, pass POSITION (FLOAT4) for bindings */
    const struct gfx_input_element_binding inputs[] = {
        {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
        {GFX_INPUTELEMENT_ID_TEXCOORD2, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED},
        {GFX_INPUTELEMENT_ID_TEXCOORD3, "vsi_billboard", 0, GFX_INPUT_OFFSET_PACKED},
        {GFX_INPUTELEMENT_ID_COLOR, "vsi_color", 0, GFX_INPUT_OFFSET_PACKED}
    };

    g_blb.shader_id = gfx_shader_load("gfx-blb", eng_get_lsralloc(),
        "shaders/billboard.vs", "shaders/billboard.ps", "shaders/billboard.gs",
        inputs, GFX_INPUT_GETCNT(inputs), NULL, 0, NULL);
    if (g_blb.shader_id == 0)   {
        err_print(__FILE__, __LINE__, "gfx-billboard init failed: could not load billboard shader");
        return RET_FAIL;
    }
    g_blb.cb_frame = gfx_shader_create_cblock(eng_get_lsralloc(), tsk_get_tmpalloc(0),
        gfx_shader_get(g_blb.shader_id), "cb_frame", NULL);
    if (g_blb.cb_frame == NULL) {
        err_print(__FILE__, __LINE__, "gfx-billboard init failed: could not create cblocks");
        return RET_FAIL;
    }

    /* buffers */
    g_blb.vbuff = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_DYNAMIC,
        sizeof(struct blb_vertex)*BILLBOARDS_BUFF_SIZE, NULL, 0);
    if (g_blb.vbuff == NULL)    {
        err_print(__FILE__, __LINE__, "gfx-billboard init failed: could not create buffers");
        return RET_FAIL;
    }
    gfx_ringbuffer_init(&g_blb.rbuff, g_blb.vbuff, BUFF_SEGMENTS);

    /* temp buffer for billboard queue */
    uint sz = BILLBOARDS_BUFF_SIZE/BUFF_SEGMENTS;
    g_blb.buff = ALIGNED_ALLOC(sizeof(struct blb_vertex)*sz, MID_GFX);
    if (g_blb.buff == NULL)    {
        err_print(__FILE__, __LINE__, "gfx-billboard init failed: could not create cpu buffers");
        return RET_FAIL;
    }

    /* states */
    /* alphaBlendState (normal alpha blending, SRC=SRC_ALPHA, DEST=INV_SRC_ALPHA) */
    struct gfx_blend_desc blend_desc;
    memcpy(&blend_desc, gfx_get_defaultblend(), sizeof(blend_desc));
    blend_desc.enable = TRUE;
    blend_desc.src_blend = GFX_BLEND_SRC_ALPHA;
    blend_desc.dest_blend = GFX_BLEND_INV_SRC_ALPHA;
    g_blb.blend_alpha = gfx_create_blendstate(&blend_desc);

    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.address_u = GFX_ADDRESS_WRAP;
    sdesc.address_v = GFX_ADDRESS_WRAP;
    sdesc.filter_min = GFX_FILTER_LINEAR;
    sdesc.filter_mag = GFX_FILTER_LINEAR;
    sdesc.filter_mip = GFX_FILTER_LINEAR;
    g_blb.sampl_lin = gfx_create_sampler(&sdesc);
    if (g_blb.blend_alpha == NULL || g_blb.sampl_lin == NULL)   {
        err_print(__FILE__, __LINE__, "gfx-billboard init failed: could not create states");
        return RET_FAIL;
    }

    struct gfx_depthstencil_desc dsdesc;
    memcpy(&dsdesc, gfx_get_defaultdepthstencil(), sizeof(dsdesc));
    dsdesc.depth_enable = TRUE;
    dsdesc.depth_write = FALSE;
    g_blb.ds_ztest = gfx_create_depthstencilstate(&dsdesc);

    /* input-layout */
    const struct gfx_input_vbuff_desc vbuffs[] = {
        {sizeof(struct blb_vertex), g_blb.vbuff}
    };

    g_blb.il = gfx_create_inputlayout(vbuffs, GFX_INPUTVB_GETCNT(vbuffs),
        inputs, GFX_INPUT_GETCNT(inputs), NULL, GFX_INDEX_UNKNOWN, 0);

    return RET_OK;
}

void gfx_blb_release()
{
    if (g_blb.vbuff != NULL)
        gfx_destroy_buffer(g_blb.vbuff);
    if (g_blb.buff != NULL) {
        ALIGNED_FREE(g_blb.buff);
    }

    if (g_blb.cb_frame != NULL)
        gfx_shader_destroy_cblock(g_blb.cb_frame);

    if (g_blb.shader_id != 0)
        gfx_shader_unload(g_blb.shader_id);

    if (g_blb.sampl_lin != NULL)
        gfx_destroy_sampler(g_blb.sampl_lin);

    if (g_blb.blend_alpha != NULL)
        gfx_destroy_blendstate(g_blb.blend_alpha);

    if (g_blb.ds_ztest != NULL)
        gfx_destroy_depthstencilstate(g_blb.ds_ztest);

    if (g_blb.il != NULL)
        gfx_destroy_inputlayout(g_blb.il);

    gfx_blb_zero();
}

void gfx_blb_push(gfx_cmdqueue cmdqueue,
    const struct vec4f* pos, const struct color* color,
    float sx, float sy, gfx_texture tex,
    const struct vec4f* texcoord)
{
    /* TODO: do batching */
    uint idx = g_blb.blb_idx;

    struct blb_vertex* vert = &((struct blb_vertex*)g_blb.buff)[idx];
    vec3_setv(&vert->pos, pos);
    vec4_setv(&vert->coord, texcoord);
    vec4_setf(&vert->billboard, sx, sy, 0.0f, 0.0f);
    color_setc(&vert->color, color);
    g_blb.tex = tex;

    g_blb.blb_idx ++;
    g_blb.blb_cnt ++;

    /* limit_reached: save into gpu buffer */
    if (g_blb.blb_idx == (BILLBOARDS_BUFF_SIZE/BUFF_SEGMENTS))  {
        uint offset;
        uint sz;

        void* gverts = gfx_ringbuffer_map(cmdqueue, &g_blb.rbuff, &offset, &sz);
        memcpy(gverts, g_blb.buff, sz);
        gfx_ringbuffer_unmap(cmdqueue, &g_blb.rbuff, sz);

        g_blb.blb_idx = 0;
    }
}

void gfx_blb_render(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params)
{
    if (g_blb.blb_cnt == 0)
        return;

    /* apply remaining lights into gpu buffers */
    uint idx = g_blb.blb_idx;
    if (idx > 0)   {
        uint offset;
        uint sz;

        void* gverts = gfx_ringbuffer_map(cmdqueue, &g_blb.rbuff, &offset, &sz);
        memcpy(gverts, g_blb.buff, minui(sz, idx*sizeof(struct blb_vertex)));
        gfx_ringbuffer_unmap(cmdqueue, &g_blb.rbuff, sz);
    }

    /* calculate inverse-view matrix from view */
    struct mat3f view_inv;
    mat3_setf(&view_inv,
        params->view.m11, params->view.m21, params->view.m31,
        params->view.m12, params->view.m22, params->view.m32,
        params->view.m13, params->view.m23, params->view.m33,
        params->cam_pos.x, params->cam_pos.y, params->cam_pos.z);

    struct gfx_shader* shader = gfx_shader_get(g_blb.shader_id);
    gfx_shader_bind(cmdqueue, shader);
    gfx_output_setblendstate(cmdqueue, g_blb.blend_alpha, NULL);
    gfx_output_setdepthstencilstate(cmdqueue, g_blb.ds_ztest, 0);
    gfx_input_setlayout(cmdqueue, g_blb.il);

    struct mat3f mi;
    struct gfx_cblock* cb_frame = g_blb.cb_frame;
    uint cnt = minui(BILLBOARDS_BUFF_SIZE, g_blb.blb_cnt);

    mat3_set_ident(&mi);
    gfx_shader_set3m(shader, SHADER_NAME(c_world), &mi);
    gfx_cb_set3m(cb_frame, SHADER_NAME(c_viewinv), &view_inv);
    gfx_cb_set4m(cb_frame, SHADER_NAME(c_viewproj), &params->viewproj);
    gfx_shader_updatecblock(cmdqueue, cb_frame);

    gfx_shader_bindconstants(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_billboard), g_blb.sampl_lin,
        g_blb.tex);
    gfx_shader_bindcblocks(cmdqueue, shader, (const struct gfx_cblock**)&cb_frame, 1);

    gfx_draw(cmdqueue, GFX_PRIMITIVE_POINTLIST, 0, cnt, GFX_DRAWCALL_PARTICLES);

    gfx_ringbuffer_reset(&g_blb.rbuff);

    gfx_output_setblendstate(cmdqueue, NULL, NULL);
    gfx_output_setdepthstencilstate(cmdqueue, NULL, 0);

    g_blb.blb_cnt = 0;
    g_blb.blb_idx = 0;
}
