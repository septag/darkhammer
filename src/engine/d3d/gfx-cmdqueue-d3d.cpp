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

#include "dhcore/core.h"

#if defined(_D3D_)
#if defined(_MSVC_)
/* win8.1 sdk inculdes some d3d types that triggeres "redifinition" warnings with external DX-SDK */
#pragma warning(disable: 4005)
#endif

#include <dxgi.h>
#include <d3d11.h>
//#include <d3d11_1.h>

#include "gfx-cmdqueue.h"
#include "mem-ids.h"
#include "gfx-device.h"
#include "gfx.h"
#include "gfx-shader.h"
#include "engine.h"
#include "app.h"

#if !defined(RELEASE)
#define RELEASE(x)  if ((x) != NULL)  {   (x)->Release();    (x) = NULL;   }
#endif

/* structures */
struct gfx_cmdqueue_s
{
    ID3D11DeviceContext* context;
    struct gfx_framestats stats;
    int shaders_set[GFX_PROGRAM_MAX_SHADERS];
    gfx_depthstencilstate default_depthstencil;
    gfx_rasterstate default_raster;
    gfx_blendstate default_blend;
    int rtv_width;
    int rtv_height;
    gfx_rendertarget cur_rt;
    uint input_bindings[GFX_INPUTELEMENT_ID_CNT];
    uint input_binding_cnt;

    uint blit_shaderid;   /* blit shader is used for d3d10.0 spec */
    gfx_depthstencilstate blit_ds;
};

/* fwd */
/* implemented in app-d3d.cpp */
ID3D11Texture2D* app_d3d_getbackbuff();
ID3D11Texture2D* app_d3d_getdepthbuff();

/* inlines/callbacks */
void _setshader_vs(ID3D11DeviceContext* context, ID3D11DeviceChild* shader)
{
    context->VSSetShader((ID3D11VertexShader*)shader, NULL, 0);
}
void _setshader_ps(ID3D11DeviceContext* context, ID3D11DeviceChild* shader)
{
    context->PSSetShader((ID3D11PixelShader*)shader, NULL, 0);
}
void _setshader_gs(ID3D11DeviceContext* context, ID3D11DeviceChild* shader)
{
    context->GSSetShader((ID3D11GeometryShader*)shader, NULL, 0);
}

void _resetshader_vs(ID3D11DeviceContext* context)
{
    context->VSSetShader(NULL, NULL, 0);
}
void _resetshader_ps(ID3D11DeviceContext* context)
{
    context->PSSetShader(NULL, NULL, 0);
}
void _resetshader_gs(ID3D11DeviceContext* context)
{
    context->GSSetShader(NULL, NULL, 0);
}

void _vs_set_cbuffers(ID3D11DeviceContext* context, uint slot_idx, uint buffer_cnt,
                      ID3D11Buffer* const* cbuffers)
{
    context->VSSetConstantBuffers(slot_idx, buffer_cnt, cbuffers);
}
void _ps_set_cbuffers(ID3D11DeviceContext* context, uint slot_idx, uint buffer_cnt,
                      ID3D11Buffer* const* cbuffers)
{
    context->PSSetConstantBuffers(slot_idx, buffer_cnt, cbuffers);
}
void _gs_set_cbuffers(ID3D11DeviceContext* context, uint slot_idx, uint buffer_cnt,
                      ID3D11Buffer* const* cbuffers)
{
    context->GSSetConstantBuffers(slot_idx, buffer_cnt, cbuffers);
}

void _vs_set_samplers(ID3D11DeviceContext* context, uint slot_idx, uint sampler_cnt,
                      ID3D11SamplerState* const* samplers)
{
    context->VSSetSamplers(slot_idx, sampler_cnt, samplers);
}
void _ps_set_samplers(ID3D11DeviceContext* context, uint slot_idx, uint sampler_cnt,
                      ID3D11SamplerState* const* samplers)
{
    context->PSSetSamplers(slot_idx, sampler_cnt, samplers);
}
void _gs_set_samplers(ID3D11DeviceContext* context, uint slot_idx, uint sampler_cnt,
                      ID3D11SamplerState* const* samplers)
{
    context->GSSetSamplers(slot_idx, sampler_cnt, samplers);
}

void _vs_set_resources(ID3D11DeviceContext* context, uint slot_idx, uint rsv_cnt,
                       ID3D11ShaderResourceView* const* srvs)
{
    context->VSSetShaderResources(slot_idx, rsv_cnt, srvs);
}
void _ps_set_resources(ID3D11DeviceContext* context, uint slot_idx, uint rsv_cnt,
                       ID3D11ShaderResourceView* const* srvs)
{
    context->PSSetShaderResources(slot_idx, rsv_cnt, srvs);
}
void _gs_set_resources(ID3D11DeviceContext* context, uint slot_idx, uint rsv_cnt,
                       ID3D11ShaderResourceView* const* srvs)
{
    context->GSSetShaderResources(slot_idx, rsv_cnt, srvs);
}

/* globals/callbacks */
typedef void (*pfn_setshader)(ID3D11DeviceContext*, ID3D11DeviceChild*);
typedef void (*pfn_resetshader)(ID3D11DeviceContext*);
typedef void (*pfn_set_cbuffers)(ID3D11DeviceContext*, uint, uint, ID3D11Buffer* const*);
typedef void (*pfn_set_samplers)(ID3D11DeviceContext*, uint, uint, ID3D11SamplerState* const*);
typedef void (*pfn_set_resources)(ID3D11DeviceContext*, uint, uint, ID3D11ShaderResourceView* const*);

pfn_setshader g_fn_setshader[] = {
    _setshader_vs, _setshader_ps, _setshader_gs
};
pfn_resetshader g_fn_resetshader[] = {
    _resetshader_vs, _resetshader_ps, _resetshader_gs
};
pfn_set_cbuffers g_fn_setcbuffers[] = {
    _vs_set_cbuffers, _ps_set_cbuffers, _gs_set_cbuffers
};
pfn_set_samplers g_fn_setsamplers[] = {
    _vs_set_samplers, _ps_set_samplers, _gs_set_samplers
};
pfn_set_resources g_fn_setresources[] = {
    _vs_set_resources, _ps_set_resources, _gs_set_resources
};

/*  */
gfx_cmdqueue gfx_create_cmdqueue()
{
    gfx_cmdqueue cmdqueue =  (gfx_cmdqueue)ALLOC(sizeof(struct gfx_cmdqueue_s), MID_GFX);
    if (cmdqueue == NULL)
        return NULL;
    memset(cmdqueue, 0x00, sizeof(struct gfx_cmdqueue_s));
    return cmdqueue;
}

void gfx_destroy_cmdqueue(gfx_cmdqueue cmdqueue)
{
    FREE(cmdqueue);
}

result_t gfx_initcmdqueue(gfx_cmdqueue cmdqueue, void* param)
{
    /* param is d3d main device context, if =NULL we should create a new one */
    if (param != NULL)
        cmdqueue->context = (ID3D11DeviceContext*)param;
    else
        ASSERT(0);

    /* create default states */
    cmdqueue->default_raster = gfx_create_rasterstate(gfx_get_defaultraster());
    cmdqueue->default_depthstencil = gfx_create_depthstencilstate(gfx_get_defaultdepthstencil());
    cmdqueue->default_blend = gfx_create_blendstate(gfx_get_defaultblend());
    if (cmdqueue->default_raster == NULL ||
        cmdqueue->default_depthstencil == NULL ||
        cmdqueue->default_blend == NULL)
    {
        err_print(__FILE__, __LINE__, "gfx-cmdqueue init failed: could not create default states");
        return RET_FAIL;
    }

    /* blitting stuff (only for d3d10.0) */
    if (gfx_get_hwver() == GFX_HWVER_D3D10_0)   {
        struct gfx_depthstencil_desc dsdesc;
        memcpy(&dsdesc, gfx_get_defaultdepthstencil(), sizeof(dsdesc));
        dsdesc.depth_enable = TRUE;
        dsdesc.depth_write = TRUE;
        dsdesc.depth_func = GFX_CMP_ALWAYS;
        cmdqueue->blit_ds = gfx_create_depthstencilstate(&dsdesc);
        if (cmdqueue->blit_ds == NULL)  {
            err_print(__FILE__, __LINE__, "gfx-cmdqueue init failed: could not create blit ds state");
            return RET_FAIL;
        }

        const struct gfx_input_element_binding bindings[] = {
            {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
            {GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
        };
        cmdqueue->blit_shaderid = gfx_shader_load("blit-raw", eng_get_lsralloc(),
            "shaders/fsq.vs", "shaders/blit-raw.ps", NULL,
            bindings, 2, NULL, 0, NULL);
        if (cmdqueue->blit_shaderid == 0)   {
            err_print(__FILE__, __LINE__, "gfx-cmdqueue init failed: could not create blit shader");
            return RET_FAIL;
        }
    }

    gfx_output_setrasterstate(cmdqueue, cmdqueue->default_raster);
    gfx_output_setdepthstencilstate(cmdqueue, cmdqueue->default_depthstencil, 0);
    gfx_output_setblendstate(cmdqueue, cmdqueue->default_blend, NULL);

    return RET_OK;
}

void gfx_releasecmdqueue(gfx_cmdqueue cmdqueue)
{
    /* destroy default states */
    GFX_DESTROY_DEVOBJ(gfx_destroy_rasterstate, cmdqueue->default_raster);
    GFX_DESTROY_DEVOBJ(gfx_destroy_blendstate, cmdqueue->default_blend);
    GFX_DESTROY_DEVOBJ(gfx_destroy_depthstencilstate, cmdqueue->default_depthstencil);
    GFX_DESTROY_DEVOBJ(gfx_destroy_depthstencilstate, cmdqueue->blit_ds);

    if (cmdqueue->blit_shaderid != 0)
        gfx_shader_unload(cmdqueue->blit_shaderid);

    /* if it's not a main-context we can release it */
    if (cmdqueue->context != app_gfx_getcontext())
        RELEASE(cmdqueue->context);

    memset(cmdqueue, 0x00, sizeof(struct gfx_cmdqueue_s));
}

void gfx_input_setlayout(gfx_cmdqueue cmdqueue, gfx_inputlayout inputlayout)
{
    ASSERT(inputlayout->type == GFX_OBJ_INPUTLAYOUT);

    /* vertex buffers */
    ID3D11Buffer* vbs[GFX_INPUTELEMENT_ID_CNT];
    uint offsets[GFX_INPUTELEMENT_ID_CNT];
    uint strides[GFX_INPUTELEMENT_ID_CNT];
    uint vb_cnt = inputlayout->desc.il.vbuff_cnt;

    for (uint i = 0; i < vb_cnt; i++) {
        vbs[i] = (ID3D11Buffer*)((gfx_inputlayout)inputlayout->desc.il.vbuffs[i])->api_obj;
        strides[i] = inputlayout->desc.il.strides[i];
        offsets[i] = 0;
    }

    cmdqueue->context->IASetVertexBuffers(0, vb_cnt, vbs, strides, offsets);

    /* index buffer */
    if (inputlayout->desc.il.ibuff != NULL) {
        cmdqueue->context->IASetIndexBuffer(
            (ID3D11Buffer*)((gfx_inputlayout)inputlayout->desc.il.ibuff)->api_obj,
            (DXGI_FORMAT)inputlayout->desc.il.idxfmt, 0);
    }
}

void gfx_program_set(gfx_cmdqueue cmdqueue, gfx_program prog)
{
    int shaders_set[GFX_PROGRAM_MAX_SHADERS];
    memset(shaders_set, 0x00, sizeof(shaders_set));

    for (uint i = 0; i < prog->desc.prog.shader_cnt; i++) {
        uint idx = (uint)prog->desc.prog.shader_types[i] - 1;
        g_fn_setshader[idx](cmdqueue->context, (ID3D11DeviceChild*)prog->desc.prog.shaders[i]);
        shaders_set[idx] = TRUE;
    }

    /* reset previously set shaders */
    for (uint i = 0; i < GFX_PROGRAM_MAX_SHADERS; i++)    {
        if (!shaders_set[i] && cmdqueue->shaders_set[i])
            g_fn_resetshader[i](cmdqueue->context);
        cmdqueue->shaders_set[i] = shaders_set[i];
    }

    /* set inputlayout */
    cmdqueue->context->IASetInputLayout((ID3D11InputLayout*)prog->desc.prog.d3d_il);
    cmdqueue->stats.shaderchange_cnt ++;
    cmdqueue->stats.input_cnt ++;
}

void gfx_buffer_update(gfx_cmdqueue cmdqueue, gfx_buffer buffer, const void* data, uint size)
{
    ID3D11DeviceContext* context = cmdqueue->context;
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr;
    hr = context->Map((ID3D11Resource*)buffer->api_obj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))  {
        memcpy(mapped.pData, data, size);
        context->Unmap((ID3D11Resource*)buffer->api_obj, 0);
        cmdqueue->stats.map_cnt ++;
    }
}

void gfx_draw(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type, uint vert_idx,
    uint vert_cnt, uint draw_id)
{
    cmdqueue->context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)type);
    cmdqueue->context->Draw(vert_cnt, vert_idx);

    cmdqueue->stats.draw_cnt ++;
    cmdqueue->stats.prims_cnt += vert_cnt;
    cmdqueue->stats.draw_group_cnt[draw_id] ++;
    cmdqueue->stats.draw_prim_cnt[draw_id] += vert_cnt;
}

void gfx_draw_indexed(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
    uint ib_idx, uint idx_cnt, enum gfx_index_type ib_type, uint draw_id)
{
    cmdqueue->context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)type);
    cmdqueue->context->DrawIndexed(idx_cnt, ib_idx, 0);

    cmdqueue->stats.draw_cnt ++;
    cmdqueue->stats.prims_cnt += idx_cnt;
    cmdqueue->stats.draw_group_cnt[draw_id] ++;
    cmdqueue->stats.draw_prim_cnt[draw_id] += idx_cnt;
}

void gfx_draw_instance(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
    uint vert_idx, uint vert_cnt, uint instance_cnt, uint draw_id)
{
    cmdqueue->context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)type);
    cmdqueue->context->DrawInstanced(vert_cnt, instance_cnt, vert_idx, 0);

    uint prim_cnt = instance_cnt*vert_cnt;
    cmdqueue->stats.draw_cnt ++;
    cmdqueue->stats.prims_cnt += prim_cnt;
    cmdqueue->stats.draw_group_cnt[draw_id] ++;
    cmdqueue->stats.draw_prim_cnt[draw_id] += prim_cnt;
}

void gfx_draw_indexedinstance(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
    uint ib_idx, uint idx_cnt, enum gfx_index_type ib_type, uint instance_cnt,
    uint draw_id)
{
    cmdqueue->context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)type);
    cmdqueue->context->DrawIndexedInstanced(idx_cnt, instance_cnt, ib_idx, 0, 0);

    uint prim_cnt = instance_cnt*idx_cnt;
    cmdqueue->stats.draw_cnt ++;
    cmdqueue->stats.prims_cnt += prim_cnt;
    cmdqueue->stats.draw_group_cnt[draw_id] ++;
    cmdqueue->stats.draw_prim_cnt[draw_id] += prim_cnt;
}

void gfx_reset_framestats(gfx_cmdqueue cmdqueue)
{
    memset(&cmdqueue->stats, 0x00, sizeof(struct gfx_framestats));
}

const struct gfx_framestats* gfx_get_framestats(gfx_cmdqueue cmdqueue)
{
    return &cmdqueue->stats;
}

void gfx_program_setcblock(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
    gfx_buffer buffer, uint shaderbind_id, uint bind_idx)
{
    ASSERT(shader != GFX_SHADER_NONE);

    uint shader_idx = (uint)shader - 1;
    ID3D11Buffer* vb = (ID3D11Buffer*)buffer->api_obj;
    g_fn_setcbuffers[shader_idx](cmdqueue->context, shaderbind_id, 1, &vb);
}

void gfx_program_bindcblock_range(gfx_cmdqueue cmdqueue,  gfx_program prog,
                                  enum gfx_shader_type shader, gfx_buffer buffer,
                                  uint shaderbind_id, uint bind_idx,
                                  uint offset, uint size)
{
    ASSERT(0); /* not implemented */
}


void gfx_program_setsampler(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
    gfx_sampler sampler, uint shaderbind_id, uint texture_unit)
{
    ASSERT(shader != GFX_SHADER_NONE);

    uint shader_idx = (uint)shader - 1;
    ID3D11SamplerState* s = (ID3D11SamplerState*)sampler->api_obj;
    g_fn_setsamplers[shader_idx](cmdqueue->context, shaderbind_id, 1, &s);
}

void gfx_program_settexture(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
    gfx_texture tex, uint texture_unit)
{
    ASSERT(shader != GFX_SHADER_NONE);

    uint shader_idx = (uint)shader - 1;
    ID3D11ShaderResourceView* srv = (ID3D11ShaderResourceView*)tex->desc.tex.d3d_srv;
    g_fn_setresources[shader_idx](cmdqueue->context, texture_unit, 1, &srv);
}

void gfx_program_setcblock_tbuffer(gfx_cmdqueue cmdqueue, gfx_program prog,
    enum gfx_shader_type shader, gfx_buffer buffer, uint shaderbind_id, uint texture_unit)
{
    ASSERT(shader != GFX_SHADER_NONE);

    uint shader_idx = (uint)shader - 1;
    ID3D11ShaderResourceView* srv = (ID3D11ShaderResourceView*)buffer->desc.buff.d3d_srv;
    g_fn_setresources[shader_idx](cmdqueue->context, shaderbind_id, 1, &srv);
}


void gfx_output_setblendstate(gfx_cmdqueue cmdqueue, gfx_blendstate blend,
    OPTIONAL const float* blend_color)
{
    if (blend == NULL)
        blend = cmdqueue->default_blend;

    const static float default_clr[] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float* clr = (blend_color != NULL) ? blend_color : default_clr;
    ID3D11BlendState* b = (ID3D11BlendState*)blend->api_obj;
    cmdqueue->context->OMSetBlendState(b, clr, 0xffffffff);
    cmdqueue->stats.blendstatechange_cnt ++;
}

void gfx_output_setdepthstencilstate(gfx_cmdqueue cmdqueue, gfx_depthstencilstate ds,
		int stencil_ref)
{
    if (ds == NULL)
        ds = cmdqueue->default_depthstencil;

    ID3D11DepthStencilState* d = (ID3D11DepthStencilState*)ds->api_obj;
    cmdqueue->context->OMSetDepthStencilState(d, (uint)stencil_ref);
    cmdqueue->stats.dsstatechange_cnt ++;
}

void gfx_output_setrasterstate(gfx_cmdqueue cmdqueue, gfx_rasterstate raster)
{
    if (raster == NULL)
        raster = cmdqueue->default_raster;

    ID3D11RasterizerState* r = (ID3D11RasterizerState*)raster->api_obj;
    cmdqueue->context->RSSetState(r);
    cmdqueue->stats.rsstatechange_cnt ++;
}

void gfx_output_setscissor(gfx_cmdqueue cmdqueue, int x, int y, int width, int height)
{
    D3D11_RECT d3d_rect = {x, y, x + width, y + height};
    cmdqueue->context->RSSetScissorRects(1, &d3d_rect);
}

void gfx_output_setviewport(gfx_cmdqueue cmdqueue, int x, int y, int width, int height)
{
    D3D11_VIEWPORT vp = {(FLOAT)x, (FLOAT)y, (FLOAT)width, (FLOAT)height, 0.0f, 1.0f};
    cmdqueue->context->RSSetViewports(1, &vp);
}

void gfx_output_setviewportbias(gfx_cmdqueue cmdqueue, int x, int y, int width, int height)
{
    const float bias = 0.0000152588f;
    D3D11_VIEWPORT vp = {(FLOAT)x, (FLOAT)y, (FLOAT)width, (FLOAT)height, 0.0f, 1.0f};
    vp.MinDepth += bias*2.0f;
    vp.MaxDepth -= bias*2.0f;
    cmdqueue->context->RSSetViewports(1, &vp);
}


void* gfx_buffer_map(gfx_cmdqueue cmdqueue, gfx_buffer buffer, uint offset, uint size,
    uint mode /* enum gfx_map_mode */, int sync_cpu)
{
    ASSERT(buffer->type == GFX_OBJ_BUFFER);

    D3D11_MAPPED_SUBRESOURCE mapped;

    UINT flags = sync_cpu ? 0 : D3D11_MAP_FLAG_DO_NOT_WAIT;

    /* for now we always use synced cpu in D3D, because of differences in opengl/d3d drivers
     * TODO: I haven't figured out how to properly optimize opengl mapping */
    HRESULT dxhr = cmdqueue->context->Map((ID3D11Resource*)buffer->api_obj,
        0, (D3D11_MAP)mode, flags, &mapped);

    if (SUCCEEDED(dxhr))   {
        cmdqueue->stats.map_cnt ++;
        return ((uint8*)mapped.pData + offset);
    }   else    {
        return NULL;
    }
}

void gfx_buffer_unmap(gfx_cmdqueue cmdqueue, gfx_buffer buffer)
{
    ASSERT(buffer->type == GFX_OBJ_BUFFER);
    cmdqueue->context->Unmap((ID3D11Resource*)buffer->api_obj, 0);
}

void gfx_cmdqueue_setrtvsize(gfx_cmdqueue cmdqueue, uint width, uint height)
{
	cmdqueue->rtv_width = (int)width;
    cmdqueue->rtv_height = (int)height;
}

void gfx_cmdqueue_getrtvsize(gfx_cmdqueue cmdqueue, OUT uint* width, OUT uint* height)
{
	*width = (uint)cmdqueue->rtv_width;
	*height = (uint)cmdqueue->rtv_height;
}

void gfx_reset_devstates(gfx_cmdqueue cmdqueue)
{
    gfx_output_setblendstate(cmdqueue, NULL, NULL);
    gfx_output_setrasterstate(cmdqueue, NULL);
    gfx_output_setdepthstencilstate(cmdqueue, NULL, 0);
}

void gfx_output_setrendertarget(gfx_cmdqueue cmdqueue, OPTIONAL gfx_rendertarget rt)
{
    if (rt != NULL) {
        gfx_set_rtvsize(rt->desc.rt.width, rt->desc.rt.height);

        ID3D11RenderTargetView* rtvs[GFX_RT_MAX_TEXTURES];
        uint rt_cnt = rt->desc.rt.rt_cnt;
        for (uint i = 0; i < rt_cnt; i++) {
            rtvs[i] = (ID3D11RenderTargetView*)
            ((gfx_texture)rt->desc.rt.rt_textures[i])->desc.tex.d3d_rtv;
        }

        ID3D11DepthStencilView* dsv = NULL;
        if (rt->desc.rt.ds_texture != NULL)
            dsv = (ID3D11DepthStencilView*)((gfx_texture)rt->desc.rt.ds_texture)->desc.tex.d3d_rtv;
        cmdqueue->context->OMSetRenderTargets(rt_cnt, rt_cnt > 0 ? rtvs : NULL, dsv);
        cmdqueue->cur_rt = rt;
    }   else    {
        app_set_rendertarget(NULL);
        cmdqueue->cur_rt = NULL;
    }

    cmdqueue->stats.rtchange_cnt ++;
}

void gfx_output_clearrendertarget(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
    const float color[4], float depth, uint8 stencil, uint flags)
{
    if (rt == NULL) {
        app_window_clear(color, depth, stencil, flags);
        return;
    }

    if (rt->desc.rt.ds_texture != NULL &&
        BIT_CHECK(flags, GFX_CLEAR_DEPTH) || BIT_CHECK(flags, GFX_CLEAR_STENCIL))
    {
        ASSERT(rt->desc.rt.ds_texture);
        ID3D11DepthStencilView* dsv =
            (ID3D11DepthStencilView*)((gfx_texture)rt->desc.rt.ds_texture)->desc.tex.d3d_rtv;
        cmdqueue->context->ClearDepthStencilView(dsv, flags, depth, stencil);
        cmdqueue->stats.cleards_cnt ++;
    }

    if (rt->desc.rt.rt_cnt > 0 && BIT_CHECK(flags, GFX_CLEAR_COLOR))  {
        uint rt_cnt = rt->desc.rt.rt_cnt;
        for (uint i = 0; i < rt_cnt; i++) {
            ID3D11RenderTargetView* rtv = (ID3D11RenderTargetView*)
                ((gfx_texture)rt->desc.rt.rt_textures[i])->desc.tex.d3d_rtv;
            cmdqueue->context->ClearRenderTargetView(rtv, color);
        }
        cmdqueue->stats.clearrt_cnt += rt_cnt;
    }
}


void gfx_rendertarget_blit(gfx_cmdqueue cmdqueue,
    int dest_x, int dest_y, int dest_width, int dest_height,
    gfx_rendertarget src_rt, int src_x, int src_y, int src_width, int src_height)
{
    ID3D11Resource* dest_res = cmdqueue->cur_rt != NULL ?
        (ID3D11Resource*)((gfx_texture)cmdqueue->cur_rt->desc.rt.rt_textures[0])->api_obj :
        app_d3d_getbackbuff();

    D3D11_BOX src_box = {
        src_x, src_y, 0,
        src_x + src_width, src_y + src_height, 0
    };
    ID3D11Resource* src_res =
        (ID3D11Resource*)((gfx_texture)src_rt->desc.rt.rt_textures[0])->api_obj;
    cmdqueue->context->CopySubresourceRegion(dest_res, 0, dest_x, dest_y, 0,
        src_res, 0, &src_box);
}

void gfx_rendertarget_blitraw(gfx_cmdqueue cmdqueue, gfx_rendertarget src_rt)
{
    if (gfx_get_hwver() != GFX_HWVER_D3D10_0 || src_rt->desc.rt.ds_texture == NULL)   {
        /* copy color buffer to current render-target */
        ID3D11Resource* dest_res = cmdqueue->cur_rt != NULL ?
            (ID3D11Resource*)((gfx_texture)cmdqueue->cur_rt->desc.rt.rt_textures[0])->api_obj :
            app_d3d_getbackbuff();
        ID3D11Resource* src_res =
            (ID3D11Resource*)((gfx_texture)src_rt->desc.rt.rt_textures[0])->api_obj;
        cmdqueue->context->CopyResource(dest_res, src_res);

        /* do it for depth-buffer too */
        if (src_rt->desc.rt.ds_texture != NULL) {
            ID3D11Resource* src_depth =
                (ID3D11Resource*)((gfx_texture)src_rt->desc.rt.ds_texture)->api_obj;
            ID3D11Resource* dest_depth = cmdqueue->cur_rt != NULL ?
                (ID3D11Resource*)((gfx_texture)cmdqueue->cur_rt->desc.rt.ds_texture)->api_obj :
                app_d3d_getdepthbuff();
            cmdqueue->context->CopyResource(dest_depth, src_depth);
        }
    }   else    {
        struct gfx_shader* shader = gfx_shader_get(cmdqueue->blit_shaderid);
        gfx_output_setdepthstencilstate(cmdqueue, cmdqueue->blit_ds, 0);
        gfx_shader_bind(cmdqueue, shader);
        gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_color),
            (gfx_texture)src_rt->desc.rt.rt_textures[0]);
        gfx_shader_bindtexture(cmdqueue, shader, SHADER_NAME(s_depth),
            (gfx_texture)src_rt->desc.rt.ds_texture);
        gfx_draw_fullscreenquad();
        gfx_output_setdepthstencilstate(cmdqueue, NULL, 0);
    }
}

void gfx_program_setbindings(gfx_cmdqueue cmdqueue, const uint* bindings, uint binding_cnt)
{
    cmdqueue->input_binding_cnt = binding_cnt;
    memcpy(cmdqueue->input_bindings, bindings, sizeof(uint)*binding_cnt);
}

void gfx_cmdqueue_resetsrvs(gfx_cmdqueue cmdqueue)
{
    static ID3D11ShaderResourceView* srvs[] = {
        NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL
    };
    cmdqueue->context->PSSetShaderResources(0, 16, srvs);
}

void gfx_texture_generatemips(gfx_cmdqueue cmdqueue, gfx_texture tex)
{
    cmdqueue->context->GenerateMips((ID3D11ShaderResourceView*)tex->desc.tex.d3d_srv);
}

void gfx_texture_update(gfx_cmdqueue cmdqueue, gfx_texture tex, const void* pixels)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    cmdqueue->context->Map((ID3D11Resource*)tex->api_obj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, pixels, tex->desc.tex.size);
    cmdqueue->context->Unmap((ID3D11Resource*)tex->api_obj, 0);
}

void gfx_flush(gfx_cmdqueue cmdqueue)
{
    cmdqueue->context->Flush();
}

#endif  /* _D3D_ */
