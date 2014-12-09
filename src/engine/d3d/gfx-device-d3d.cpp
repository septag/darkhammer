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
//#pragma warning(disable: 4005)
#endif

#include <stdio.h>
#include <dxgi.h>
#include <d3d11.h>
#include <D3Dcompiler.h>
#include <D3Dcommon.h>

#include "dhcore/win.h"
#include "dhcore/mt.h"
#include "dhapp/app.h"

#include "dhcore/pool-alloc.h"
#include "dhcore/color.h"

#include "dheng/gfx-device.h"
#include "dheng/gfx-cmdqueue.h"
#include "dheng/gfx.h"
#include "dheng/gfx-texture.h"

#include "dhcore/mt.h"


#if !defined(RELEASE)
#define RELEASE(x)  if ((x) != NULL)  {   (x)->Release();    (x) = NULL;   }
#endif

#define BACKBUFFER_COUNT 2
#define DEFAULT_DISPLAY_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM

/* types */
struct gfx_device
{
    appGfxParams params;
    struct gfx_gpu_memstats memstats;
    struct pool_alloc obj_pool;
    gfx_rasterstate default_raster;
    gfx_depthstencilstate default_depthstencil;
    gfx_blendstate default_blend;
    int threaded_creates;
    int threaded_cmdqueues;
    appGfxDeviceVersion ver;
    mt_mutex objpool_mtx;
};

/* globals */
struct gfx_device g_gfxdev;

/* inlines */
INLINE const char* get_hwverstr(appGfxDeviceVersion hwver)
{
    switch (hwver)  {
    case appGfxDeviceVersion::D3D10_0:
        return "10.0";
    case appGfxDeviceVersion::D3D10_1:
        return "10.1";
    case appGfxDeviceVersion::D3D11_0:
        return "11.0";
    case appGfxDeviceVersion::D3D11_1:
        return "11.1";
    }
    return "";
}

INLINE struct gfx_obj_data* create_obj(uptr_t api_obj, enum gfx_obj_type type)
{
    mt_mutex_lock(&g_gfxdev.objpool_mtx);
	struct gfx_obj_data* obj = (struct gfx_obj_data*)mem_pool_alloc(&g_gfxdev.obj_pool);
    mt_mutex_unlock(&g_gfxdev.objpool_mtx);
    ASSERT(obj != NULL);
    memset(obj, 0x00, sizeof(struct gfx_obj_data));
    obj->api_obj = (uptr_t)api_obj;
    obj->type = type;

    return obj;
}

/* releases all device objects */
INLINE void destroy_obj(struct gfx_obj_data* obj)
{
    ID3D11DeviceChild* d3d_obj =  (ID3D11DeviceChild*)obj->api_obj;
    RELEASE(d3d_obj);
    obj->type = GFX_OBJ_NULL;
    mt_mutex_lock(&g_gfxdev.objpool_mtx);
    mem_pool_free(&g_gfxdev.obj_pool, obj);
    mt_mutex_unlock(&g_gfxdev.objpool_mtx);
}

INLINE enum D3D11_FILTER sampler_choose_filter(
    enum gfxFilterMode filter_min, enum gfxFilterMode filter_mag,
    enum gfxFilterMode filter_mip, int anisotropic, int has_cmp)
{
    if (anisotropic)
        return !has_cmp ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_COMPARISON_ANISOTROPIC;

    if (filter_min == gfxFilterMode::LINEAR && filter_mag == gfxFilterMode::LINEAR &&
        filter_mip == gfxFilterMode::LINEAR)
        return !has_cmp ? D3D11_FILTER_MIN_MAG_MIP_LINEAR :
        		D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    else if (filter_min == gfxFilterMode::LINEAR && filter_mag == gfxFilterMode::LINEAR &&
        (filter_mip == gfxFilterMode::NEAREST || filter_mip == gfxFilterMode::UNKNOWN))
        return !has_cmp ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT :
        		D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    else if (filter_min == gfxFilterMode::LINEAR && filter_mag == gfxFilterMode::NEAREST &&
        filter_mip == gfxFilterMode::LINEAR)
        return !has_cmp ? D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR :
        		D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    else if (filter_min == gfxFilterMode::LINEAR && filter_mag == gfxFilterMode::NEAREST &&
        (filter_mip == gfxFilterMode::NEAREST || filter_mip == gfxFilterMode::UNKNOWN))
        return !has_cmp ? D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT :
        		D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
    else if (filter_min == gfxFilterMode::NEAREST && filter_mag == gfxFilterMode::LINEAR &&
        filter_mip == gfxFilterMode::LINEAR)
        return !has_cmp ? D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR :
        		D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
    else if (filter_min == gfxFilterMode::NEAREST && filter_mag == gfxFilterMode::LINEAR &&
        (filter_mip == gfxFilterMode::NEAREST || filter_mip == gfxFilterMode::UNKNOWN))
        return !has_cmp ? D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT :
        		D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
    else if (filter_min == gfxFilterMode::NEAREST && filter_mag == gfxFilterMode::NEAREST &&
        filter_mip == gfxFilterMode::LINEAR)
        return !has_cmp ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR :
        		D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
    else
        return !has_cmp ? D3D11_FILTER_MIN_MAG_MIP_POINT :
        		D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;

}

INLINE int texture_has_alpha(enum gfxFormat fmt)
{
    return (fmt == gfxFormat::BC2 ||
    fmt == gfxFormat::BC3 ||
    fmt == gfxFormat::BC2_SRGB ||
    fmt == gfxFormat::BC3_SRGB ||
    fmt == gfxFormat::RGBA_UNORM ||
    fmt == gfxFormat::R32G32B32A32_FLOAT ||
    fmt == gfxFormat::R32G32B32A32_UINT ||
    fmt == gfxFormat::R10G10B10A2_UNORM);
}

INLINE int texture_is_depth(enum gfxFormat fmt)
{
    return (fmt == gfxFormat::DEPTH16 ||
        fmt == gfxFormat::DEPTH24_STENCIL8 ||
        fmt == gfxFormat::DEPTH32);
}

INLINE const char* shader_get_target(enum gfx_shader_type type)
{
    static char target[32];
    appGfxDeviceVersion hwver = gfx_get_hwver();

    switch (type)   {
    case GFX_SHADER_VERTEX:
        strcpy(target, "vs_");
        break;
    case GFX_SHADER_PIXEL:
        strcpy(target, "ps_");
        break;
    case GFX_SHADER_GEOMETRY:
        strcpy(target, "gs_");
        break;
    }

    switch (hwver)    {
    case appGfxDeviceVersion::D3D10_0:
        strcat(target, "4_0");
        break;
    case appGfxDeviceVersion::D3D10_1:
        strcat(target, "4_1");
        break;
    case appGfxDeviceVersion::D3D11_0:
        strcat(target, "5_0");
        break;
    }

    return target;
}

INLINE void shader_output_error(const char* err_desc)
{
    char err_info[1000];
    uint s = (uint)strlen(err_desc);
    snprintf(err_info, sizeof(err_info)-1, "hlsl-compiler failed: %s", err_desc);
    err_print(__FILE__, __LINE__, err_info);
}

INLINE const D3D11_INPUT_ELEMENT_DESC* shader_get_element_byid(gfxInputElemId id)
{
    static const D3D11_INPUT_ELEMENT_DESC elem_pos = {
        "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_norm = {
        "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_tangent = {
        "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_binorm = {
        "BINORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_coord0 = {
        "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_index = {
        "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_weight = {
        "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_coord1 = {
        "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_coord2 = {
        "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_coord3 = {
        "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    static const D3D11_INPUT_ELEMENT_DESC elem_color = {
        "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };

    switch (id) {
    case gfxInputElemId::POSITION:
        return &elem_pos;
    case gfxInputElemId::NORMAL:
        return &elem_norm;
    case gfxInputElemId::TANGENT:
        return &elem_tangent;
    case gfxInputElemId::BINORMAL:
        return &elem_binorm;
    case gfxInputElemId::TEXCOORD0:
        return &elem_coord0;
    case gfxInputElemId::TEXCOORD1:
        return &elem_coord1;
    case gfxInputElemId::TEXCOORD2:
        return &elem_coord2;
    case gfxInputElemId::TEXCOORD3:
        return &elem_coord3;
    case gfxInputElemId::COLOR:
        return &elem_color;
    case gfxInputElemId::BLENDINDEX:
        return &elem_index;
    case gfxInputElemId::BLENDWEIGHT:
        return &elem_weight;
    default:
        return NULL;
    }
}


INLINE DXGI_FORMAT texture_get_rawfmt(enum gfxFormat fmt)
{
    switch (fmt)    {
    case gfxFormat::DEPTH24_STENCIL8:
        return DXGI_FORMAT_R24G8_TYPELESS;
    case gfxFormat::DEPTH16:
        return DXGI_FORMAT_R16_TYPELESS;
    case gfxFormat::DEPTH32:
        return DXGI_FORMAT_R32_TYPELESS;
    default:
        return (DXGI_FORMAT)fmt;
    }
}


INLINE DXGI_FORMAT texture_get_srvfmt(enum gfxFormat fmt)
{
    switch (fmt)    {
    case gfxFormat::DEPTH24_STENCIL8:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case gfxFormat::DEPTH16:
        return DXGI_FORMAT_R16_UNORM;
    case gfxFormat::DEPTH32:
        return DXGI_FORMAT_R32_FLOAT;
    default:
        return (DXGI_FORMAT)fmt;
    }
}

INLINE const struct gfx_input_element* get_elem(gfxInputElemId id)
{
    static const struct gfx_input_element elems[] = {
        {gfxInputElemId::POSITION, gfxInputElemFormat::FLOAT, 4, sizeof(struct vec4f)},
        {gfxInputElemId::NORMAL, gfxInputElemFormat::FLOAT, 3, sizeof(struct vec4f)},
        {gfxInputElemId::TEXCOORD0, gfxInputElemFormat::FLOAT, 2, sizeof(struct vec2f)},
        {gfxInputElemId::TANGENT, gfxInputElemFormat::FLOAT, 3, sizeof(struct vec4f)},
        {gfxInputElemId::BINORMAL, gfxInputElemFormat::FLOAT, 3, sizeof(struct vec4f)},
        {gfxInputElemId::BLENDINDEX, gfxInputElemFormat::INT, 4, sizeof(struct vec4i)},
        {gfxInputElemId::BLENDWEIGHT, gfxInputElemFormat::FLOAT, 4, sizeof(struct vec4f)},
        {gfxInputElemId::TEXCOORD1, gfxInputElemFormat::FLOAT, 2, sizeof(struct vec2f)},
        {gfxInputElemId::TEXCOORD2, gfxInputElemFormat::FLOAT, 4, sizeof(struct vec4f)},
        {gfxInputElemId::TEXCOORD3, gfxInputElemFormat::FLOAT, 4, sizeof(struct vec4f)},
        {gfxInputElemId::COLOR, gfxInputElemFormat::FLOAT, 4, sizeof(struct color)}
    };
    static const uint elem_cnt = gfxInputElemId::COUNT;

    for (uint i = 0; i < elem_cnt; i++)	{
        if (id == elems[i].id)
            return &elems[i];
    }
    return NULL;
}

INLINE enum gfxFormat texture_get_nonsrgb(enum gfxFormat fmt)
{
    switch (fmt)    {
    case gfxFormat::RGBA_UNORM_SRGB:
        return gfxFormat::RGBA_UNORM;
    case gfxFormat::BC1_SRGB:
        return gfxFormat::BC1;
    case gfxFormat::BC2_SRGB:
        return gfxFormat::BC2;
    case gfxFormat::BC3_SRGB:
        return gfxFormat::BC3;
    default:
        return fmt;
    }
}

ID3D11InputLayout* gfx_create_inputlayout_fromshader(const void* vs_data, uint vs_size,
                                                     const struct gfx_input_element_binding* bindings,
                                                     uint binding_cnt);

/* */
void gfx_zerodev()
{
    memset(&g_gfxdev, 0x00, sizeof(struct gfx_device));
}

result_t gfx_initdev(const appGfxParams* params)
{
    result_t r;
    HRESULT dxhr;

    memcpy(&g_gfxdev.params, params, sizeof(appGfxParams));

    log_print(LOG_INFO, "  init gfx-device ...");

    r = mem_pool_create(mem_heap(), &g_gfxdev.obj_pool, sizeof(struct gfx_obj_data), 100, MID_GFX);
    if (IS_FAIL(r))     {
        err_print(__FILE__, __LINE__, "gfx-device init failed: could not create object pool");
        return RET_FAIL;
    }
    mt_mutex_init(&g_gfxdev.objpool_mtx);

    /* check features support */
    ID3D11Device* dev = app_d3d_getdevice();
    D3D11_FEATURE_DATA_THREADING topt;
    dxhr = dev->CheckFeatureSupport(D3D11_FEATURE_THREADING, &topt, sizeof(topt));
    if (FAILED(dxhr))   {
        err_print(__FILE__, __LINE__, "gfx-device init failed: could not retreive device features");
        return RET_FAIL;
    }

    if (!topt.DriverConcurrentCreates)  {
        log_print(LOG_WARNING, "Device doesn't have multi-thread resource creation support, "
            "background resource loading will be disabled");
    }   else    {
        g_gfxdev.threaded_creates = TRUE;
    }

    g_gfxdev.threaded_cmdqueues = topt.DriverCommandLists;
    g_gfxdev.ver = app_d3d_getver();

    /* D3D 11.1 */
    /*
    D3D11_FEATURE_DATA_D3D11_OPTIONS options;
    dxhr = dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, (void*)&options, sizeof(options));
    if (FAILED(dxhr))   {
        err_print(__FILE__, __LINE__, "gfx-device init failed: could not check feature support");
        return RET_FAIL;
    }

    if (!options.MapNoOverwriteOnDynamicConstantBuffer) {
        err_print(__FILE__, __LINE__, "gfx-device init failed: device does not support requested features");
        return RET_FAIL;
    }
    */

    return RET_OK;
}

void gfx_releasedev()
{
    /* detect leaks and delete remaining objects */
    uint leaks_cnt = mem_pool_getleaks(&g_gfxdev.obj_pool);
    if (leaks_cnt > 0)
        log_printf(LOG_WARNING, "gfx-device: total %d leaks found", leaks_cnt);

    mem_pool_destroy(&g_gfxdev.obj_pool);
    mt_mutex_release(&g_gfxdev.objpool_mtx);

    gfx_zerodev();
}


gfx_depthstencilstate gfx_create_depthstencilstate(const struct gfx_depthstencil_desc* ds)
{
    HRESULT dxhr;

    /* Desc */
    D3D11_DEPTH_STENCIL_DESC d3d_desc;
    d3d_desc.DepthEnable = ds->depth_enable;
    d3d_desc.DepthFunc = (D3D11_COMPARISON_FUNC)(ds->depth_func);
    d3d_desc.DepthWriteMask = ds->depth_write ? D3D11_DEPTH_WRITE_MASK_ALL :
        D3D11_DEPTH_WRITE_MASK_ZERO;
    d3d_desc.StencilEnable = ds->stencil_enable;
    d3d_desc.StencilWriteMask = ds->stencil_mask;
    d3d_desc.StencilReadMask = ds->stencil_mask;
    d3d_desc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)
        (ds->stencil_frontface_desc.cmp_func);
    d3d_desc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)
        (ds->stencil_frontface_desc.pass_op);
    d3d_desc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)
        (ds->stencil_frontface_desc.fail_op);
    d3d_desc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)
        (ds->stencil_frontface_desc.depthfail_op);
    d3d_desc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)
        (ds->stencil_backface_desc.cmp_func);
    d3d_desc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)(ds->stencil_backface_desc.pass_op);
    d3d_desc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)(ds->stencil_backface_desc.fail_op);
    d3d_desc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)
        (ds->stencil_backface_desc.depthfail_op);

    ID3D11DepthStencilState* d;
    dxhr = app_d3d_getdevice()->CreateDepthStencilState(&d3d_desc, &d);
    if (FAILED(dxhr))
        return NULL;

    gfx_depthstencilstate obj = create_obj((uptr_t)d, GFX_OBJ_DEPTHSTENCILSTATE);
    memcpy(&obj->desc.ds, ds, sizeof(struct gfx_depthstencil_desc));
    return obj;
}

void gfx_destroy_depthstencilstate(gfx_depthstencilstate ds)
{
    destroy_obj(ds);
}

gfx_rasterstate gfx_create_rasterstate(const struct gfx_rasterizer_desc* raster)
{
    HRESULT dxhr;

    D3D11_RASTERIZER_DESC d3d_desc;
    d3d_desc.FillMode = (D3D11_FILL_MODE)(raster->fill);
    d3d_desc.CullMode = (D3D11_CULL_MODE)(raster->cull);
    d3d_desc.FrontCounterClockwise = TRUE;
    d3d_desc.MultisampleEnable = FALSE;
    d3d_desc.ScissorEnable = raster->scissor_test;
    d3d_desc.DepthBiasClamp = 0.0f;
    d3d_desc.DepthBias = *((int*)&raster->depth_bias);
    d3d_desc.DepthClipEnable = raster->depth_clip;
    d3d_desc.AntialiasedLineEnable = FALSE;
    d3d_desc.SlopeScaledDepthBias = raster->slopescaled_depthbias;

    ID3D11RasterizerState* rs;
    dxhr = app_d3d_getdevice()->CreateRasterizerState(&d3d_desc, &rs);
    if (FAILED(dxhr))
        return NULL;

    gfx_rasterstate obj = create_obj((uptr_t)rs, GFX_OBJ_RASTERSTATE);
    memcpy(&obj->desc.raster, raster, sizeof(struct gfx_rasterizer_desc));
    return obj;
}

void gfx_destroy_rasterstate(gfx_rasterstate raster)
{
    destroy_obj(raster);
}

gfx_blendstate gfx_create_blendstate(const struct gfx_blend_desc* blend)
{
    HRESULT dxhr;
    D3D11_BLEND_DESC d3d_desc;
    for (uint i = 0; i < 8; i++)  {
        d3d_desc.RenderTarget[i].BlendEnable = blend->enable;
        d3d_desc.RenderTarget[i].BlendOp = (D3D11_BLEND_OP)(blend->color_op);
        d3d_desc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        d3d_desc.RenderTarget[i].DestBlend = (D3D11_BLEND)(blend->dest_blend);
        d3d_desc.RenderTarget[i].SrcBlend = (D3D11_BLEND)(blend->src_blend);
        d3d_desc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
        d3d_desc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        d3d_desc.RenderTarget[i].RenderTargetWriteMask = blend->write_mask;
    }
    d3d_desc.AlphaToCoverageEnable = FALSE;
    d3d_desc.IndependentBlendEnable = FALSE;

    ID3D11BlendState* b;
    dxhr = app_d3d_getdevice()->CreateBlendState(&d3d_desc, &b);
    if (FAILED(dxhr))
        return NULL;

    gfx_blendstate obj = create_obj((uptr_t)b, GFX_OBJ_BLENDSTATE);
    memcpy(&obj->desc.blend, blend, sizeof(struct gfx_blend_desc));
    return obj;
}

void gfx_destroy_blendstate(gfx_blendstate blend)
{
    destroy_obj(blend);
}

gfx_sampler gfx_create_sampler(const struct gfx_sampler_desc* desc)
{
    HRESULT dxhr;

    D3D11_SAMPLER_DESC d3d_desc;
    d3d_desc.Filter = sampler_choose_filter(desc->filter_min, desc->filter_mag, desc->filter_mip,
        desc->aniso_max>1, desc->cmp_func != gfxCmpFunc::OFF);
    d3d_desc.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)(desc->address_u);
    d3d_desc.AddressV = (D3D11_TEXTURE_ADDRESS_MODE)(desc->address_v);
    d3d_desc.AddressW = (D3D11_TEXTURE_ADDRESS_MODE)(desc->address_w);
    d3d_desc.MipLODBias = 0.0f;
    d3d_desc.MaxAnisotropy = clampui(desc->aniso_max, 1, 16);
    d3d_desc.ComparisonFunc = (D3D11_COMPARISON_FUNC)(desc->cmp_func);
    d3d_desc.BorderColor[0] = desc->border_color[0];
    d3d_desc.BorderColor[1] = desc->border_color[1];
    d3d_desc.BorderColor[2] = desc->border_color[2];
    d3d_desc.BorderColor[3] = desc->border_color[3];
    d3d_desc.MinLOD = (FLOAT)desc->lod_min;
    d3d_desc.MaxLOD = (FLOAT)desc->lod_max;

    ID3D11SamplerState* s;
    dxhr = app_d3d_getdevice()->CreateSamplerState(&d3d_desc, &s);
    if (FAILED(dxhr))
        return NULL;

    return create_obj((uptr_t)s, GFX_OBJ_SAMPLER);
}

void gfx_destroy_sampler(gfx_sampler sampler)
{
    destroy_obj(sampler);
}

gfx_buffer gfx_create_buffer(enum gfxBufferType type, enum gfxMemHint memhint,
    uint size, const void* data, uint thread_id)
{
    HRESULT dxhr;

    D3D11_BUFFER_DESC d3d_desc;
    d3d_desc.ByteWidth = size;
    d3d_desc.Usage = (D3D11_USAGE)(memhint);
    switch (memhint)    {
    case gfxMemHint::STATIC:
        d3d_desc.CPUAccessFlags = 0;
        break;
    case gfxMemHint::DYNAMIC:
        d3d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        break;
    case gfxMemHint::READ:
        d3d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        break;
    }

    switch (type)   {
    case gfxBufferType::VERTEX:
        d3d_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        break;
    case gfxBufferType::INDEX:
        d3d_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        break;
    case gfxBufferType::CONSTANT:
        d3d_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        break;
    case GFX_BUFFER_SHADERSTORAGE:
        ASSERT(0);
        break;
    case gfxBufferType::SHADER_TEXTURE:
        d3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        break;
    case gfxBufferType::STREAM_OUT:
        d3d_desc.BindFlags = D3D11_BIND_STREAM_OUTPUT;
        break;
    }

    d3d_desc.MiscFlags = 0;
    d3d_desc.StructureByteStride = 0;

    ID3D11Buffer* b;
    if (data != NULL)        {
        D3D11_SUBRESOURCE_DATA d3d_data;
        d3d_data.pSysMem = data;
        d3d_data.SysMemPitch = 0;
        d3d_data.SysMemSlicePitch = 0;
        dxhr = app_d3d_getdevice()->CreateBuffer(&d3d_desc, &d3d_data, &b);
    }    else    {
        dxhr = app_d3d_getdevice()->CreateBuffer(&d3d_desc, NULL, &b);
    }

    if (FAILED(dxhr))
        return NULL;

    ID3D11ShaderResourceView* d3d_srv = NULL;
    if (type == gfxBufferType::SHADER_TEXTURE)   {
        D3D11_SHADER_RESOURCE_VIEW_DESC viewdesc;
        viewdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        viewdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        viewdesc.Buffer.ElementOffset = 0;
        viewdesc.Buffer.ElementWidth = size / 16;
        dxhr = app_d3d_getdevice()->CreateShaderResourceView(b, &viewdesc, &d3d_srv);
        if (FAILED(dxhr))
            return NULL;
    }


    gfx_buffer obj = create_obj((uptr_t)b, GFX_OBJ_BUFFER);
    obj->desc.buff.size = size;
    obj->desc.buff.type = type;
    obj->desc.buff.d3d_srv = d3d_srv;
    obj->desc.buff.alignment = 16;

    g_gfxdev.memstats.buffer_cnt ++;
    g_gfxdev.memstats.buffers += size;
    return obj;
}

void gfx_destroy_buffer(gfx_buffer buff)
{
    g_gfxdev.memstats.buffer_cnt --;
    g_gfxdev.memstats.buffers -= buff->desc.buff.size;

    if (buff->desc.buff.d3d_srv != NULL)
        ((ID3D11ShaderResourceView*)buff->desc.buff.d3d_srv)->Release();

    destroy_obj(buff);
}


gfx_buffer gfx_create_texture(enum gfxTextureType type, uint width, uint height, uint depth,
    enum gfxFormat fmt, uint mip_cnt, uint array_size, uint total_size,
    const struct gfx_subresource_data* data, enum gfxMemHint memhint, uint thread_id)
{
    HRESULT dxhr = E_FAIL;
    ID3D11DeviceChild* tex;
    uint cpu_access;

    /* allocate data */
    uint subres_cnt = array_size*mip_cnt*
        ((type == gfxTextureType::TEX_CUBE) || (type == gfxTextureType::TEX_CUBE_ARRAY) ? 6 : 1);
    D3D11_SUBRESOURCE_DATA* d3d_data = (D3D11_SUBRESOURCE_DATA*)
        ALLOC(sizeof(D3D11_SUBRESOURCE_DATA)*array_size*mip_cnt, MID_GFX);
    if (d3d_data == NULL)
        return NULL;
    for (uint i = 0; i < subres_cnt; i++) {
        d3d_data[i].pSysMem = data[i].p;
        d3d_data[i].SysMemPitch = data[i].pitch_row;
        d3d_data[i].SysMemSlicePitch = data[i].pitch_slice;
    }

    switch (memhint)    {
    case gfxMemHint::DYNAMIC:
        cpu_access = D3D11_CPU_ACCESS_WRITE;
        break;
    case gfxMemHint::READ:
        cpu_access = D3D11_CPU_ACCESS_READ;
        break;
    default:
        cpu_access = 0;
        break;
    }

    /* texture creation */
    switch (type)   {
    case gfxTextureType::TEX_2D:
    case gfxTextureType::TEX_2D_ARRAY:
    case gfxTextureType::TEX_CUBE_ARRAY:
    case gfxTextureType::TEX_CUBE:
        {
            D3D11_TEXTURE2D_DESC d3d_desc;
            d3d_desc.Width = width;
            d3d_desc.Height = height;
            d3d_desc.MipLevels = mip_cnt;
            d3d_desc.ArraySize = ((type == gfxTextureType::TEX_CUBE) || (type == gfxTextureType::TEX_CUBE_ARRAY)) ?
                (6*array_size) : array_size;
            d3d_desc.Format = (DXGI_FORMAT)(fmt);
            d3d_desc.Usage = (D3D11_USAGE)memhint;
            d3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            d3d_desc.CPUAccessFlags = cpu_access;
            d3d_desc.MiscFlags = ((type == gfxTextureType::TEX_CUBE) || (type == gfxTextureType::TEX_CUBE_ARRAY)) ?
                D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
            d3d_desc.SampleDesc.Count = 1;
            d3d_desc.SampleDesc.Quality = 0;
            dxhr = app_d3d_getdevice()->CreateTexture2D(&d3d_desc, d3d_data, (ID3D11Texture2D**)&tex);
        }
        break;

    case gfxTextureType::TEX_1D:
    case gfxTextureType::TEX_1D_ARRAY:
        {
            D3D11_TEXTURE1D_DESC d3d_desc;
            d3d_desc.Width = width;
            d3d_desc.MipLevels = mip_cnt;
            d3d_desc.ArraySize = array_size;
            d3d_desc.Format = (DXGI_FORMAT)(fmt);
            d3d_desc.Usage = (D3D11_USAGE)memhint;
            d3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            d3d_desc.CPUAccessFlags = cpu_access;
            d3d_desc.MiscFlags = 0;
            dxhr = app_d3d_getdevice()->CreateTexture1D(&d3d_desc, d3d_data, (ID3D11Texture1D**)&tex);
        }
        break;

    case gfxTextureType::TEX_3D:
        {
            D3D11_TEXTURE3D_DESC d3d_desc;
            d3d_desc.Width = width;
            d3d_desc.Height = height;
            d3d_desc.Depth = depth;
            d3d_desc.MipLevels = mip_cnt;
            d3d_desc.Format = (DXGI_FORMAT)(fmt);
            d3d_desc.Usage = (D3D11_USAGE)memhint;
            d3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            d3d_desc.CPUAccessFlags = cpu_access;
            d3d_desc.MiscFlags = 0;
            dxhr = app_d3d_getdevice()->CreateTexture3D(&d3d_desc, d3d_data, (ID3D11Texture3D**)&tex);
        }
        break;
    }

    FREE(d3d_data);

    if (FAILED(dxhr))
        return NULL;

    /* shader res-view */
    ID3D11ShaderResourceView* srv;
    dxhr = app_d3d_getdevice()->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &srv);
    if (FAILED(dxhr))   {
        tex->Release();
        return NULL;
    }

    /* */
    gfx_texture obj = create_obj((uptr_t)tex, GFX_OBJ_TEXTURE);
    obj->desc.tex.type = type;
    obj->desc.tex.width = width;
    obj->desc.tex.height = height;
    obj->desc.tex.fmt = fmt;
    obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.d3d_srv = srv;    /* direct3d specific */
    obj->desc.tex.size = total_size;
    obj->desc.tex.is_rt = FALSE;
    obj->desc.tex.mip_cnt = mip_cnt;

    g_gfxdev.memstats.texture_cnt ++;
    g_gfxdev.memstats.textures += total_size;

    return obj;
}

gfx_texture gfx_create_texturert(uint width, uint height, enum gfxFormat fmt, int has_mipmap)
{
    HRESULT dxhr;
    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* srv;
    ID3D11RenderTargetView* rtv = NULL;
    ID3D11DepthStencilView* dsv = NULL;
    DXGI_FORMAT fmt_raw = texture_get_rawfmt(fmt);
    DXGI_FORMAT fmt_srv = texture_get_srvfmt(fmt);
    int is_depth = texture_is_depth(fmt);
    uint mipcnt = 1;
    if (has_mipmap) {
        mipcnt = 1 + (uint)floorf(log10f((float)maxui(width, height))/log10f(2.0f));
    }

    D3D11_TEXTURE2D_DESC d3d_desc;
    d3d_desc.Width = width;
    d3d_desc.Height = height;
    d3d_desc.MipLevels = mipcnt;
    d3d_desc.ArraySize = 1;
    d3d_desc.Format = (DXGI_FORMAT)(fmt_raw);
    d3d_desc.Usage = D3D11_USAGE_DEFAULT;
    d3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (!is_depth)
        d3d_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    else
        d3d_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    d3d_desc.CPUAccessFlags = 0;
    d3d_desc.MiscFlags = has_mipmap ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    d3d_desc.SampleDesc.Count = 1;
    d3d_desc.SampleDesc.Quality = 0;
    dxhr = app_d3d_getdevice()->CreateTexture2D(&d3d_desc, NULL, (ID3D11Texture2D**)&tex);
    if (FAILED(dxhr))
        return NULL;

    /* shader-resource-view */
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    memset(&srv_desc, 0x0, sizeof(srv_desc));
    srv_desc.Format = (DXGI_FORMAT)fmt_srv;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;
    dxhr = app_d3d_getdevice()->CreateShaderResourceView(tex, &srv_desc, &srv);
    if (FAILED(dxhr))   {
        tex->Release();
        return NULL;
    }

    /* render-target-view/depth-stencil-view */
    /* render-target-views are not SRGB (if we want SRGB we simply convert color in shader) */
    if (!is_depth)  {
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
        memset(&rtv_desc, 0x00, sizeof(rtv_desc));
        rtv_desc.Format = (DXGI_FORMAT)texture_get_nonsrgb(fmt);
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;
        dxhr = app_d3d_getdevice()->CreateRenderTargetView(tex, &rtv_desc, &rtv);
    }   else    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
        memset(&dsv_desc, 0x00, sizeof(dsv_desc));
        dsv_desc.Format = (DXGI_FORMAT)fmt;
        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Flags = 0;
        dsv_desc.Texture2D.MipSlice = 0;
        dxhr = app_d3d_getdevice()->CreateDepthStencilView(tex, &dsv_desc, &dsv);
    }
    if (FAILED(dxhr))   {
        tex->Release();
        srv->Release();
        return NULL;
    }

    gfx_texture obj = create_obj((uptr_t)tex, GFX_OBJ_TEXTURE);
    obj->desc.tex.type = gfxTextureType::TEX_2D;
    obj->desc.tex.width = width;
    obj->desc.tex.height = height;
    obj->desc.tex.fmt = fmt;
    obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.d3d_srv = srv;    /* direct3d specific */
    obj->desc.tex.d3d_rtv = rtv != NULL ? (void*)rtv : (void*)dsv;    /* direct3d specific */
    obj->desc.tex.size = width*height*(gfx_texture_getbpp(fmt)/8);
    obj->desc.tex.is_rt = TRUE;
    obj->desc.tex.mip_cnt = mipcnt;

    g_gfxdev.memstats.rttexture_cnt ++;
    g_gfxdev.memstats.rt_textures += obj->desc.tex.size;

    return obj;
}

gfx_texture gfx_create_texturert_arr(uint width, uint height, uint arr_cnt,
    enum gfxFormat fmt)
{
    HRESULT dxhr;
    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* srv;
    ID3D11RenderTargetView* rtv = NULL;
    ID3D11DepthStencilView* dsv = NULL;
    DXGI_FORMAT fmt_raw = texture_get_rawfmt(fmt);
    DXGI_FORMAT fmt_srv = texture_get_srvfmt(fmt);
    int is_depth = texture_is_depth(fmt);

    D3D11_TEXTURE2D_DESC d3d_desc;
    d3d_desc.Width = width;
    d3d_desc.Height = height;
    d3d_desc.MipLevels = 1;
    d3d_desc.ArraySize = arr_cnt;
    d3d_desc.Format = (DXGI_FORMAT)(fmt_raw);
    d3d_desc.Usage = D3D11_USAGE_DEFAULT;
    d3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (!is_depth)
        d3d_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    else
        d3d_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    d3d_desc.CPUAccessFlags = 0;
    d3d_desc.MiscFlags = 0;
    d3d_desc.SampleDesc.Count = 1;
    d3d_desc.SampleDesc.Quality = 0;
    dxhr = app_d3d_getdevice()->CreateTexture2D(&d3d_desc, NULL, (ID3D11Texture2D**)&tex);
    if (FAILED(dxhr))
        return NULL;

    /* shader-resource-view */
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    memset(&srv_desc, 0x0, sizeof(srv_desc));
    srv_desc.Format = (DXGI_FORMAT)fmt_srv;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srv_desc.Texture2DArray.MipLevels = 1;
    srv_desc.Texture2DArray.ArraySize = arr_cnt;
    srv_desc.Texture2DArray.FirstArraySlice = 0;
    srv_desc.Texture2DArray.MostDetailedMip = 0;

    dxhr = app_d3d_getdevice()->CreateShaderResourceView(tex, &srv_desc, &srv);
    if (FAILED(dxhr))   {
        tex->Release();
        return NULL;
    }

    /* render-target-view/depth-stencil-view */
    /* render-target-views are not SRGB (if we want SRGB we simply convert color in shader) */
    if (!is_depth)  {
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
        memset(&rtv_desc, 0x00, sizeof(rtv_desc));
        rtv_desc.Format = (DXGI_FORMAT)texture_get_nonsrgb(fmt);
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtv_desc.Texture2DArray.ArraySize = arr_cnt;
        rtv_desc.Texture2DArray.MipSlice = 0;
        rtv_desc.Texture2DArray.FirstArraySlice = 0;
        dxhr = app_d3d_getdevice()->CreateRenderTargetView(tex, &rtv_desc, &rtv);
    }   else    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
        memset(&dsv_desc, 0x00, sizeof(dsv_desc));
        dsv_desc.Format = (DXGI_FORMAT)fmt;
        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsv_desc.Flags = 0;
        dsv_desc.Texture2DArray.ArraySize = arr_cnt;
        dsv_desc.Texture2DArray.FirstArraySlice = 0;
        dsv_desc.Texture2DArray.MipSlice = 0;
        dxhr = app_d3d_getdevice()->CreateDepthStencilView(tex, &dsv_desc, &dsv);
    }
    if (FAILED(dxhr))   {
        tex->Release();
        srv->Release();
        return NULL;
    }

    gfx_texture obj = create_obj((uptr_t)tex, GFX_OBJ_TEXTURE);
    obj->desc.tex.type = gfxTextureType::TEX_2D_ARRAY;
    obj->desc.tex.width = width;
    obj->desc.tex.height = height;
    obj->desc.tex.fmt = fmt;
    obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.d3d_srv = srv;    /* direct3d specific */
    obj->desc.tex.d3d_rtv = rtv != NULL ? (void*)rtv : (void*)dsv;    /* direct3d specific */
    obj->desc.tex.size = width*height*arr_cnt*(gfx_texture_getbpp(fmt)/8);
    obj->desc.tex.is_rt = TRUE;
    obj->desc.tex.mip_cnt = 1;

    g_gfxdev.memstats.rttexture_cnt ++;
    g_gfxdev.memstats.rt_textures += obj->desc.tex.size;

    return obj;
}


gfx_texture gfx_create_texturert_cube(uint width, uint height, enum gfxFormat fmt)
{
    HRESULT dxhr;
    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* srv;
    ID3D11RenderTargetView* rtv = NULL;
    ID3D11DepthStencilView* dsv = NULL;
    DXGI_FORMAT fmt_raw = texture_get_rawfmt(fmt);
    DXGI_FORMAT fmt_srv = texture_get_srvfmt(fmt);
    int is_depth = texture_is_depth(fmt);

    D3D11_TEXTURE2D_DESC d3d_desc;
    d3d_desc.Width = width;
    d3d_desc.Height = height;
    d3d_desc.MipLevels = 1;
    d3d_desc.ArraySize = 6;
    d3d_desc.Format = (DXGI_FORMAT)(fmt_raw);
    d3d_desc.Usage = D3D11_USAGE_DEFAULT;
    d3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d3d_desc.CPUAccessFlags = 0;
    d3d_desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    d3d_desc.SampleDesc.Count = 1;
    d3d_desc.SampleDesc.Quality = 0;
    if (!is_depth)
        d3d_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    else
        d3d_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
    dxhr = app_d3d_getdevice()->CreateTexture2D(&d3d_desc, NULL, (ID3D11Texture2D**)&tex);
    if (FAILED(dxhr))
        return NULL;

    /* shader-resource-view */
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    memset(&srv_desc, 0x0, sizeof(srv_desc));
    srv_desc.Format = (DXGI_FORMAT)fmt_srv;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srv_desc.TextureCube.MostDetailedMip = 0;
    srv_desc.TextureCube.MipLevels = 1;
    dxhr = app_d3d_getdevice()->CreateShaderResourceView(tex, &srv_desc, &srv);
    if (FAILED(dxhr))   {
        tex->Release();
        return NULL;
    }

    /* render-target-view/depth-stencil-view */
    if (!is_depth)  {
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
        memset(&rtv_desc, 0x00, sizeof(rtv_desc));
        rtv_desc.Format = (DXGI_FORMAT)fmt;
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtv_desc.Texture2DArray.ArraySize = 6;
        rtv_desc.Texture2DArray.FirstArraySlice = 0;
        rtv_desc.Texture2DArray.MipSlice = 0;
        dxhr = app_d3d_getdevice()->CreateRenderTargetView(tex, &rtv_desc, &rtv);
    }   else    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
        memset(&dsv_desc, 0x00, sizeof(dsv_desc));
        dsv_desc.Format = (DXGI_FORMAT)fmt;
        dsv_desc.Flags = 0;
        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsv_desc.Texture2DArray.ArraySize = 6;
        dsv_desc.Texture2DArray.FirstArraySlice = 0;
        dsv_desc.Texture2DArray.MipSlice = 0;
        dxhr = app_d3d_getdevice()->CreateDepthStencilView(tex, &dsv_desc, &dsv);
    }
    if (FAILED(dxhr))   {
        tex->Release();
        srv->Release();
        return NULL;
    }

    gfx_texture obj = create_obj((uptr_t)tex, GFX_OBJ_TEXTURE);
    obj->desc.tex.type = gfxTextureType::TEX_CUBE;
    obj->desc.tex.width = width;
    obj->desc.tex.height = height;
    obj->desc.tex.depth = 1;
    obj->desc.tex.fmt = fmt;
    obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.d3d_srv = srv;    /* direct3d specific */
    obj->desc.tex.d3d_rtv = rtv != NULL ? (void*)rtv : (void*)dsv;    /* direct3d specific */
    obj->desc.tex.size = width*height*(gfx_texture_getbpp(fmt)/8)*6;
    obj->desc.tex.is_rt = TRUE;
    obj->desc.tex.mip_cnt = 1;

    g_gfxdev.memstats.rttexture_cnt ++;
    g_gfxdev.memstats.rt_textures += obj->desc.tex.size;
    return obj;
}

void gfx_destroy_texture(gfx_texture tex)
{
    if (!tex->desc.tex.is_rt)   {
        g_gfxdev.memstats.texture_cnt --;
        g_gfxdev.memstats.textures -= tex->desc.tex.size;
    }   else    {
        g_gfxdev.memstats.rttexture_cnt --;
        g_gfxdev.memstats.rt_textures -= tex->desc.tex.size;
    }

    ID3D11DeviceChild* srv = (ID3D11DeviceChild*)tex->desc.tex.d3d_srv;
    RELEASE(srv);
    ID3D11DeviceChild* rtv = (ID3D11DeviceChild*)tex->desc.tex.d3d_rtv;
    RELEASE(rtv);
    destroy_obj(tex);
}


gfx_rendertarget gfx_create_rendertarget(gfx_texture* rt_textures, uint rt_cnt,
		OPTIONAL gfx_texture ds_texture)
{
    uint width;
    uint height;

    if (rt_cnt > 0) {
        width = rt_textures[0]->desc.tex.width;
        height = rt_textures[0]->desc.tex.height;
    }   else if (ds_texture != NULL)    {
        width = ds_texture->desc.tex.width;
        height = ds_texture->desc.tex.height;
    }   else    {
        ASSERT(0);
    }

    gfx_rendertarget obj = create_obj(0, GFX_OBJ_RENDERTARGET);
    obj->desc.rt.rt_cnt = rt_cnt;
    for (uint i = 0; i < rt_cnt; i++)
        obj->desc.rt.rt_textures[i] = rt_textures[i];
    obj->desc.rt.ds_texture = ds_texture;
    obj->desc.rt.width = width;
    obj->desc.rt.height = height;
    return obj;
}

void gfx_destroy_rendertarget(gfx_rendertarget rt)
{
    destroy_obj(rt);
}

gfx_inputlayout gfx_create_inputlayout(const struct gfx_input_vbuff_desc* vbuffs, uint vbuff_cnt,
                                       const struct gfx_input_element_binding* inputs,
                                       uint input_cnt, OPTIONAL gfx_buffer idxbuffer,
                                       OPTIONAL enum gfxIndexType itype, uint thread_id)
{
    gfx_inputlayout obj = create_obj(0, GFX_OBJ_INPUTLAYOUT);
    obj->desc.il.vbuff_cnt = vbuff_cnt;
    for (uint i = 0; i < vbuff_cnt; i++)  {
        obj->desc.il.vbuffs[i] = (void*)vbuffs[i].vbuff;
        obj->desc.il.strides[i] = (uint)vbuffs[i].stride;
    }

    obj->desc.il.ibuff = (void*)idxbuffer;
    obj->desc.il.idxfmt = itype;
    return obj;
}

void gfx_destroy_inputlayout(gfx_inputlayout input_layout)
{
    destroy_obj(input_layout);
}

const struct gfx_depthstencil_desc* gfx_get_defaultdepthstencil()
{
    static const struct gfx_depthstencil_desc desc = {
        FALSE,
        FALSE,
        gfxCmpFunc::LESS,
        FALSE,
        0xffffffff,
        {
            gfxStencilOp::KEEP,
                gfxStencilOp::KEEP,
                gfxStencilOp::KEEP,
                gfxCmpFunc::ALWAYS
        },
        {
            gfxStencilOp::KEEP,
                gfxStencilOp::KEEP,
                gfxStencilOp::KEEP,
                gfxCmpFunc::ALWAYS
            }
    };

    return &desc;
}

const struct gfx_rasterizer_desc* gfx_get_defaultraster()
{
    static const struct gfx_rasterizer_desc desc = {
        gfxFillMode::SOLID,
        gfxCullMode::BACK,
        0.0f,
        0.0f,
        FALSE,
        TRUE
    };
    return &desc;
}

const struct gfx_sampler_desc* gfx_get_defaultsampler()
{
    const static struct gfx_sampler_desc desc = {
        gfxFilterMode::LINEAR,
        gfxFilterMode::LINEAR,
        gfxFilterMode::NEAREST,
        gfxAddressMode::WRAP,
        gfxAddressMode::WRAP,
        gfxAddressMode::WRAP,
        1,
        gfxCmpFunc::OFF,
        {0.0f, 0.0f, 0.0f, 0.0f},
        -1000,
        1000
    };
    return &desc;
}

const struct gfx_blend_desc* gfx_get_defaultblend()
{
    static const struct gfx_blend_desc desc = {
        FALSE,
        gfxBlendMode::ONE,
        gfxBlendMode::ZERO,
        gfxBlendOp::ADD,
        GFX_COLORWRITE_ALL
    };

    return &desc;
}


gfx_program gfx_create_program(const struct gfx_shader_data* source_data,
		const struct gfx_input_element_binding* bindings, uint binding_cnt,
		const struct gfx_shader_define* defines, uint define_cnt,
		struct gfx_shader_binary_data* bin_data)
{
    HRESULT dxhr;
    D3D_SHADER_MACRO* macros = NULL;
    ID3D10Blob* vs_blob = NULL;
    ID3D10Blob* ps_blob = NULL;
    ID3D10Blob* gs_blob = NULL;
    ID3D10Blob* error_blob = NULL;
    ID3D11InputLayout* il = NULL;
    ID3D11ShaderReflection* vs_refl = NULL;
    ID3D11ShaderReflection* ps_refl = NULL;
    ID3D11ShaderReflection* gs_refl = NULL;
    ID3D11VertexShader* vs = NULL;
    ID3D11PixelShader* ps = NULL;
    ID3D11GeometryShader* gs = NULL;
    enum gfx_shader_type shader_types[GFX_PROGRAM_MAX_SHADERS];
    void* shaders[GFX_PROGRAM_MAX_SHADERS];
    void* reflects[GFX_PROGRAM_MAX_SHADERS];
    uint shader_cnt = 0;
    gfx_program obj;

    /* macros/defines */
    if (define_cnt > 0) {
         macros = (D3D_SHADER_MACRO*)ALLOC(sizeof(D3D_SHADER_MACRO)*(define_cnt+1),
            MID_GFX);
        if (macros == NULL)
            return NULL;
        for (uint i = 0; i < define_cnt; i++) {
            macros[i].Name = defines[i].name;
            macros[i].Definition = defines[i].value;
        }
        macros[define_cnt].Name = NULL;
        macros[define_cnt].Definition = NULL;
    }

    /* compile flags/treat warnings as errors */
    uint flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
    if(BIT_CHECK(g_gfxdev.params.flags, appGfxFlags::DEBUG))
        BIT_ADD(flags, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION);
    else
        BIT_ADD(flags, D3DCOMPILE_OPTIMIZATION_LEVEL3);

    /* compile shaders */
    /* vertex-shader is mandatory */
    ASSERT(source_data->vs_source != NULL);
    dxhr = D3DCompile(source_data->vs_source, source_data->vs_size,
        "vertex-shader", macros, NULL,
        "main", shader_get_target(GFX_SHADER_VERTEX), flags, 0,
        &vs_blob, &error_blob);
    if (FAILED(dxhr))   {
        shader_output_error((const char*)error_blob->GetBufferPointer());
        goto err_cleanup;
    }
    RELEASE(error_blob);
    dxhr = D3DReflect(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
        IID_ID3D11ShaderReflection, (void**)&vs_refl);
    if (FAILED(dxhr))   {
        err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create vs reflector");
        goto err_cleanup;
    }

    if (source_data->ps_source != NULL) {
        dxhr = D3DCompile(source_data->ps_source, source_data->ps_size,
            "pixel-shader", macros, NULL,
            "main", shader_get_target(GFX_SHADER_PIXEL), flags, 0,
            &ps_blob, &error_blob);
        if (FAILED(dxhr))   {
            shader_output_error((const char*)error_blob->GetBufferPointer());
            goto err_cleanup;
        }
        RELEASE(error_blob);
        dxhr = D3DReflect(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
            IID_ID3D11ShaderReflection, (void**)&ps_refl);
        if (FAILED(dxhr))   {
            err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create ps reflector");
            goto err_cleanup;
        }
    }

    if (source_data->gs_source != NULL) {
        dxhr = D3DCompile(source_data->gs_source, source_data->gs_size,
            "geometry-shader", macros, NULL,
            "main", shader_get_target(GFX_SHADER_GEOMETRY), flags, 0,
            &gs_blob, &error_blob);
        if (FAILED(dxhr))   {
            shader_output_error((const char*)error_blob->GetBufferPointer());
            goto err_cleanup;
        }
        RELEASE(error_blob);
        dxhr = D3DReflect(gs_blob->GetBufferPointer(), gs_blob->GetBufferSize(),
            IID_ID3D11ShaderReflection, (void**)&gs_refl);
        if (FAILED(dxhr))   {
            err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create gs reflector");
            goto err_cleanup;
        }
    }

    if (macros != NULL) {
        FREE(macros);
        macros = NULL;
    }

    /* bind elements/create input layout
     * note that in GL input layout is created in gfx_create_inputlayout and not here
     * but in DX we have to create the layout with compiled VS code
     */
    il = gfx_create_inputlayout_fromshader(vs_blob->GetBufferPointer(), 
        (uint)vs_blob->GetBufferSize(), bindings, binding_cnt);
    if (il == NULL)   {
        err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create input-layout");
        goto err_cleanup;
    }

    /* link shaders/create shader objects */
    if (vs_blob != NULL)    {
        dxhr = app_d3d_getdevice()->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
            NULL, &vs);
        if (FAILED(dxhr))   {
            err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create vertex-shader");
            goto err_cleanup;
        }
        shader_types[shader_cnt] = GFX_SHADER_VERTEX;
        shaders[shader_cnt] = vs;
        reflects[shader_cnt] = vs_refl;
        shader_cnt ++;
    }

    if (ps_blob != NULL)    {
        dxhr = app_d3d_getdevice()->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
            NULL, &ps);
        if (FAILED(dxhr))   {
            err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create pixel-shader");
            goto err_cleanup;
        }
        shader_types[shader_cnt] = GFX_SHADER_PIXEL;
        shaders[shader_cnt] = ps;
        reflects[shader_cnt] = ps_refl;
        shader_cnt ++;
    }

    if (gs_blob != NULL)    {
        dxhr = app_d3d_getdevice()->CreateGeometryShader(gs_blob->GetBufferPointer(), gs_blob->GetBufferSize(),
            NULL, &gs);
        if (FAILED(dxhr))   {
            err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create geometry-shader");
            goto err_cleanup;
        }
        shader_types[shader_cnt] = GFX_SHADER_GEOMETRY;
        shaders[shader_cnt] = gs;
        reflects[shader_cnt] = gs_refl;
        shader_cnt ++;
    }

    /* binary output */
    if (bin_data != NULL)   {
        if (vs != NULL) {
            bin_data->vs_size = (uint)vs_blob->GetBufferSize();
            bin_data->vs_data = ALLOC(bin_data->vs_size, MID_GFX);
            ASSERT(bin_data->vs_data);
            memcpy(bin_data->vs_data, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize());
        }
        if (ps != NULL) {
            bin_data->ps_size = (uint)ps_blob->GetBufferSize();
            bin_data->ps_data = ALLOC(bin_data->ps_size, MID_GFX);
            ASSERT(bin_data->ps_data);
            memcpy(bin_data->ps_data, ps_blob->GetBufferPointer(), ps_blob->GetBufferSize());
        }
        if (gs != NULL) {
            bin_data->gs_size = (uint)gs_blob->GetBufferSize();
            bin_data->gs_data = ALLOC(bin_data->gs_size, MID_GFX);
            ASSERT(bin_data->gs_data);
            memcpy(bin_data->gs_data, gs_blob->GetBufferPointer(), gs_blob->GetBufferSize());
        }
    }
    RELEASE(vs_blob);
    RELEASE(ps_blob);
    RELEASE(gs_blob);

    /* ok */
    obj = create_obj(0, GFX_OBJ_PROGRAM);
    obj->desc.prog.shader_cnt = shader_cnt;
    obj->desc.prog.d3d_il = il;
    for (uint i = 0; i < shader_cnt; i++) {
        obj->desc.prog.shaders[i] = (uptr_t)shaders[i];
        obj->desc.prog.shader_types[i] = shader_types[i];
        obj->desc.prog.d3d_reflects[i] = reflects[i];
    }

    return obj;

err_cleanup:
    RELEASE(vs);
    RELEASE(ps);
    RELEASE(gs);
    RELEASE(error_blob);
    RELEASE(vs_blob);
    RELEASE(ps_blob);
    RELEASE(gs_blob);
    RELEASE(il);
    RELEASE(vs_refl);
    RELEASE(ps_refl);
    RELEASE(gs_refl);
    if (macros != NULL)
        FREE(macros);
    return NULL;
}

gfx_program gfx_create_program_bin(const struct gfx_program_bin_desc* bindesc)
{
    ID3D11InputLayout* il = NULL;
    ID3D11ShaderReflection* vs_refl = NULL;
    ID3D11ShaderReflection* ps_refl = NULL;
    ID3D11ShaderReflection* gs_refl = NULL;
    ID3D11VertexShader* vs = NULL;
    ID3D11PixelShader* ps = NULL;
    ID3D11GeometryShader* gs = NULL;
    enum gfx_shader_type shader_types[GFX_PROGRAM_MAX_SHADERS];
    void* shaders[GFX_PROGRAM_MAX_SHADERS];
    void* reflects[GFX_PROGRAM_MAX_SHADERS];
    uint shader_cnt = 0;
    gfx_program obj;
    HRESULT dxhr;

    /* shader linking */
    ASSERT(bindesc->vs != NULL);
    dxhr = app_d3d_getdevice()->CreateVertexShader(bindesc->vs, bindesc->vs_sz, NULL, &vs);
    if (FAILED(dxhr) ||
        FAILED(D3DReflect(bindesc->vs, bindesc->vs_sz, IID_ID3D11ShaderReflection,
        (void**)&vs_refl)))
    {
        err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create vertex-shader");
        goto err_cleanup;
    }
    shader_types[shader_cnt] = GFX_SHADER_VERTEX;
    shaders[shader_cnt] = vs;
    reflects[shader_cnt] = vs_refl;
    shader_cnt ++;

    if (bindesc->ps != NULL)    {
        dxhr = app_d3d_getdevice()->CreatePixelShader(bindesc->ps, bindesc->ps_sz, NULL, &ps);
        if (FAILED(dxhr) ||
            FAILED(D3DReflect(bindesc->ps, bindesc->ps_sz, IID_ID3D11ShaderReflection,
            (void**)&ps_refl)))
        {
            err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create pixel-shader");
            goto err_cleanup;
        }
        shader_types[shader_cnt] = GFX_SHADER_PIXEL;
        shaders[shader_cnt] = ps;
        reflects[shader_cnt] = ps_refl;
        shader_cnt ++;
    }

    if (bindesc->gs != NULL)    {
        dxhr = app_d3d_getdevice()->CreateGeometryShader(bindesc->gs, bindesc->gs_sz, NULL, &gs);
        if (FAILED(dxhr) ||
            FAILED(D3DReflect(bindesc->gs, bindesc->gs_sz, IID_ID3D11ShaderReflection,
            (void**)&gs_refl)))
        {
            err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create geometry-shader");
            goto err_cleanup;
        }
        shader_types[shader_cnt] = GFX_SHADER_GEOMETRY;
        shaders[shader_cnt] = gs;
        reflects[shader_cnt] = gs_refl;
        shader_cnt ++;
    }

    /* bind elements/create input layout
     * note that in GL input layout is created in gfx_create_inputlayout and not here
     * but in DX we have to create the layout with compiled VS code
     */
    il = gfx_create_inputlayout_fromshader(bindesc->vs, bindesc->vs_sz, bindesc->inputs,
        bindesc->input_cnt);
    if (il == NULL)   {
        err_print(__FILE__, __LINE__, "hlsl-compiler failed: could not create input-layout");
        goto err_cleanup;
    }

    /* ok */
    obj = create_obj(0, GFX_OBJ_PROGRAM);
    obj->desc.prog.shader_cnt = shader_cnt;
    obj->desc.prog.d3d_il = il;
    for (uint i = 0; i < shader_cnt; i++) {
        obj->desc.prog.shaders[i] = (uptr_t)shaders[i];
        obj->desc.prog.shader_types[i] = shader_types[i];
        obj->desc.prog.d3d_reflects[i] = reflects[i];
    }

    return obj;

err_cleanup:
    RELEASE(vs);
    RELEASE(ps);
    RELEASE(gs);
    RELEASE(il);
    RELEASE(vs_refl);
    RELEASE(ps_refl);
    RELEASE(gs_refl);
    return NULL;
}

ID3D11InputLayout* gfx_create_inputlayout_fromshader(const void* vs_data, uint vs_size,
                                                     const struct gfx_input_element_binding* bindings,
                                                     uint binding_cnt)
{
    D3D11_INPUT_ELEMENT_DESC elems[gfxInputElemId::COUNT];
    ASSERT(binding_cnt  < gfxInputElemId::COUNT);

    for (uint i = 0; i < binding_cnt; i++)    {
        memcpy(&elems[i], shader_get_element_byid(bindings[i].id), sizeof(D3D11_INPUT_ELEMENT_DESC));
        elems[i].InputSlot = bindings[i].vb_idx;
        elems[i].AlignedByteOffset =
            (bindings[i].elem_offset == GFX_INPUT_OFFSET_PACKED) ? elems[i].AlignedByteOffset :
            bindings[i].elem_offset;
    }

    ID3D11InputLayout* il = NULL;
    app_d3d_getdevice()->CreateInputLayout(elems, binding_cnt, vs_data, vs_size, &il);
    return il;
}

void gfx_destroy_program(gfx_program prog)
{
    uint shader_cnt = prog->desc.prog.shader_cnt;
    ID3D11InputLayout* il = (ID3D11InputLayout*)prog->desc.prog.d3d_il;
    RELEASE(il);

    for (uint i = 0; i < shader_cnt; i++) {
        ID3D11ShaderReflection* refl = (ID3D11ShaderReflection*)prog->desc.prog.d3d_reflects[i];
        RELEASE(refl);
        ID3D11DeviceChild* shader = (ID3D11DeviceChild*)prog->desc.prog.shaders[i];
        RELEASE(shader);
    }

    destroy_obj(prog);
}

const struct gfx_gpu_memstats* gfx_get_memstats()
{
    return &g_gfxdev.memstats;
}

int gfx_check_feature(enum gfx_feature ft)
{
    switch (ft) {
    case GFX_FEATURE_THREADED_CREATES:
        return g_gfxdev.threaded_creates;
    case GFX_FEATURE_RANGED_CBUFFERS:
        return FALSE;   /* not implemented */
    default:
        return FALSE;
    }
}

/* no need for implementation */
void gfx_delayed_createobjects()
{
}

void gfx_delayed_waitforobjects(uint thread_id)
{
}

void gfx_delayed_fillobjects(uint thread_id)
{
}

void gfx_delayed_release()
{
}

void gfx_delayed_finalizeobjects()
{
}

const char* gfx_get_driverstr()
{
    static char info[256];

    DXGI_ADAPTER_DESC desc;
    HRESULT dxhr = app_d3d_getadapter()->GetDesc(&desc);
    if (FAILED(dxhr))
        return NULL;

    char gpu_desc[128];
    str_widetomb(gpu_desc, desc.Description, sizeof(gpu_desc));

    sprintf(info, "%s r%d v%s %s",
        gpu_desc,
        desc.Revision,
        get_hwverstr(g_gfxdev.ver),
#if defined(_X64_)
        "x64"
#elif defined(_X86_)
        "x86"
#else
        "[]"
#endif
        );
    return info;
}

void gfx_get_devinfo(struct gfx_device_info* info)
{
    memset(info, 0x00, sizeof(struct gfx_device_info));

    IDXGIAdapter* adapter = app_d3d_getadapter();
    if (adapter == NULL)
        return;

    DXGI_ADAPTER_DESC desc;
    HRESULT dxhr = adapter->GetDesc(&desc);
    if (FAILED(dxhr))
        return;

    /* basic info */
    str_widetomb(info->desc, desc.Description, sizeof(info->desc));
    switch (desc.VendorId)        {
    case 0x1002:        info->vendor = GFX_GPU_ATI;     break;
    case 0x10DE:        info->vendor = GFX_GPU_NVIDIA;  break;
    case 0x163C:        info->vendor = GFX_GPU_INTEL;   break;
    default:            info->vendor = GFX_GPU_UNKNOWN; break;
    }
    info->mem_avail = (int)(desc.DedicatedVideoMemory / 1024);

    /* threading */
    D3D11_FEATURE_DATA_THREADING d3d_thr;
    if (SUCCEEDED(app_d3d_getdevice()->CheckFeatureSupport(D3D11_FEATURE_THREADING, &d3d_thr, 
        sizeof(d3d_thr))))
    {
        info->threading.concurrent_cmdlist = d3d_thr.DriverCommandLists;
        info->threading.concurrent_create = d3d_thr.DriverConcurrentCreates;
    }
}

appGfxDeviceVersion gfx_get_hwver()
{
    return g_gfxdev.ver;
}

#endif  /* _D3D_ */
