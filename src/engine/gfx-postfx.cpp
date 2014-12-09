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

#include <stdio.h>
#include "dhcore/core.h"
#include "dhcore/timer.h"

#include "gfx-postfx.h"
#include "gfx-shader.h"
#include "gfx-device.h"
#include "gfx-cmdqueue.h"
#include "engine.h"
#include "gfx.h"
#include "res-mgr.h"
#include "mem-ids.h"
#include "console.h"
#include "gfx-canvas.h"
#include "debug-hud.h"
#include "prf-mgr.h"
#include "camera.h"

#include "renderpaths/gfx-csm.h"

#define PFX_SSAO_PASSES 8
#define PFX_TONEMAP_LUMPREV_SIZE 256
#define PFX_BLOOM_KERNEL_SIZE 15
#define PFX_BLOOM_KERNEL_DEVSQR 0.5f
#define PFX_BLOOM_KERNEL_INTENSITY 1.6f

/*************************************************************************************************
 * types
 */
struct gfx_pfx_downsample
{
    gfx_texture dest_tex; /* must be 1/2 size of the source texture */
    gfx_texture dest_depthtex;
    gfx_rendertarget rt;

    uint shader_id;
    gfx_depthstencilstate ds_writez;
    gfx_sampler sampl;
};

struct gfx_pfx_ssao
{
    float radius;
    float bias;
    float scale;
    float intensity;
    gfx_texture ssao_tex;
    gfx_rendertarget rt;
    uint shader_id;
    gfx_sampler sampl_point;
    gfx_sampler sampl_point_mirror;
    reshandle_t noise_tex;
};

struct gfx_pfx_upsample
{
    gfx_texture tex;    /* must be 2x size of the source */
    gfx_rendertarget rt;
    uint shader_id;
    gfx_sampler sampl_point;
    gfx_sampler sampl_lin;
};

struct gfx_pfx_shadow
{
    gfx_texture tex; /* can be reused (reference only mode) */
    gfx_rendertarget rt;
    int ref_mode;
    int prev_mode;
    gfx_sampler sampl_point;
    gfx_sampler sampl_cmp;
    uint shader_id;
    uint prev_shaderid;
};

struct gfx_pfx_tonemap
{
    gfx_texture tex;
    gfx_rendertarget rt;

    gfx_texture lum_tex;    /* downsampled and mipmapped luminance */
    gfx_texture bright_tex[2]; /* downsampled bright texture */
    gfx_rendertarget lum_rt;
    gfx_rendertarget lumbright_rt;
    gfx_rendertarget blur_rt[2];

    gfx_texture lumadapt_tex[2];   /* 1x1 ping-pong textures(rt) for interpolating luminance */
    gfx_rendertarget lumadapt_rt[2];

    gfx_texture lumprev_tex;
    gfx_rendertarget lumprev_rt;

    gfx_sampler sampl_lin;
    gfx_sampler sampl_point;

    uint shader_id;
    uint lum_shaderid;
    uint lumbright_shaderid;
    uint lumadapt_shaderid;
    uint lumprev_shaderid;
    uint blur_shaderid;

    struct vec4f blur_kernelh[PFX_BLOOM_KERNEL_SIZE];
    struct vec4f blur_kernelv[PFX_BLOOM_KERNEL_SIZE];

    /* params */
    float mid_grey;
    float lum_min;
    float lum_max;
    int bloom;

    struct timer* tm;
    int showlum;
};

struct gfx_pfx_fxaa
{
    gfx_texture tex;
    gfx_rendertarget rt;
    uint shader_id;
    gfx_sampler sampl_lin;
};

/*************************************************************************************************
 * fwd declarations
 */
result_t pfx_console_ssao_setparams(uint argc, const char ** argv, void* param);
result_t pfx_console_shadowcsm_prev(uint argc, const char** argv, void* param);
result_t pfx_console_tonemap_showlum(uint argc, const char** argv, void* param);
result_t gfx_console_tonemap_setparams(uint argc, const char** argv, void* param);
void pfx_tonemap_prevlum(gfx_cmdqueue cmdqueue, struct gfx_pfx_tonemap* pfx);
int pfx_tonemap_debugtext(gfx_cmdqueue cmdqueue, int x, int y, int line_stride,
    void* param);
void pfx_calc_gauss(struct vec4f* kernel, int kernel_cnt, float std_devsqr, float intensity,
    int direction /*=0 horizontal, =1 vertical*/, int width, int height);

/*************************************************************************************************
 *  downsample - with depth
 */
INLINE result_t pfx_downsamplewdepth_creatert(struct gfx_pfx_downsample* pfx, uint width,
    uint height, enum gfxFormat fmt)
{
    pfx->dest_depthtex = gfx_create_texturert(width, height, gfxFormat::DEPTH24_STENCIL8, FALSE);
    pfx->dest_tex = gfx_create_texturert(width, height, fmt, FALSE);
    if (pfx->dest_depthtex == NULL || pfx->dest_tex == NULL)
        return RET_FAIL;

    pfx->rt = gfx_create_rendertarget(&pfx->dest_tex, 1, pfx->dest_depthtex);
    if (pfx->rt == NULL)
        return RET_FAIL;

    return RET_OK;
}

INLINE void pfx_downsamplewdepth_destroyrt(struct gfx_pfx_downsample* pfx)
{
    if (pfx->rt != NULL)
        gfx_destroy_rendertarget(pfx->rt);

    if (pfx->dest_depthtex != NULL)
        gfx_destroy_texture(pfx->dest_depthtex);

    if (pfx->dest_tex != NULL)
        gfx_destroy_texture(pfx->dest_tex);
}


struct gfx_pfx_downsample* gfx_pfx_downsamplewdepth_create(uint width, uint height,
    enum gfxFormat fmt, int depthpass_stencilvalue /*=-1*/)
{
     result_t r;

     struct gfx_pfx_downsample* pfx = (struct gfx_pfx_downsample*)
         ALLOC(sizeof(struct gfx_pfx_downsample), MID_GFX);
     if (pfx == NULL)
         return NULL;
     memset(pfx, 0x00, sizeof(struct gfx_pfx_downsample));

     /* textures */
     r = pfx_downsamplewdepth_creatert(pfx, width, height, fmt);
     if (IS_FAIL(r))    {
         err_print(__FILE__, __LINE__, "pfx-downsamplewdepth failed: could not create buffers");
         gfx_pfx_downsamplewdepth_destroy(pfx);
         return NULL;
     }

     /* shader */
     struct allocator* lsr_alloc = eng_get_lsralloc();
     gfx_shader_beginload(lsr_alloc, "shaders/fsq.vs", "shaders/downsample-depth.ps", NULL, 0);
     pfx->shader_id = gfx_shader_add("pfx-downsamplewdepth", 2, 0,
         gfxInputElemId::POSITION, "vsi_pos", 0,
         gfxInputElemId::TEXCOORD0, "vsi_coord", 0);
     gfx_shader_endload();
     if (pfx->shader_id == 0)   {
         err_print(__FILE__, __LINE__, "pfx-downsamplewdepth failed: could not load shaders");
         gfx_pfx_downsamplewdepth_destroy(pfx);
         return NULL;
     }

     /* states */
     struct gfx_depthstencil_desc ds_desc;
     memcpy(&ds_desc, gfx_get_defaultdepthstencil(), sizeof(ds_desc));
     ds_desc.depth_enable = TRUE;
     ds_desc.depth_write = TRUE;
     ds_desc.depth_func = gfxCmpFunc::ALWAYS;
     pfx->ds_writez = gfx_create_depthstencilstate(&ds_desc);
     if (pfx->ds_writez == NULL)    {
         err_print(__FILE__, __LINE__, "pfx-downsamplewdepth failed: could not create states");
         gfx_pfx_downsamplewdepth_destroy(pfx);
         return NULL;
     }

     struct gfx_sampler_desc sdesc;
     memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
     sdesc.address_u = gfxAddressMode::CLAMP;
     sdesc.address_v = gfxAddressMode::CLAMP;
     sdesc.filter_mag = gfxFilterMode::NEAREST;
     sdesc.filter_min = gfxFilterMode::NEAREST;
     sdesc.filter_mip = gfxFilterMode::UNKNOWN;
     pfx->sampl = gfx_create_sampler(&sdesc);
     if (pfx->sampl == NULL)    {
         err_print(__FILE__, __LINE__, "pfx-downsamplewdepth failed: could not create states");
         gfx_pfx_downsamplewdepth_destroy(pfx);
         return NULL;
     }

     return pfx;
}

void gfx_pfx_downsamplewdepth_destroy(struct gfx_pfx_downsample* pfx)
{
    ASSERT(pfx);

    pfx_downsamplewdepth_destroyrt(pfx);

    if (pfx->shader_id != 0)
        gfx_shader_unload(pfx->shader_id);

    if (pfx->ds_writez != NULL)
        gfx_destroy_depthstencilstate(pfx->ds_writez);

    if (pfx->sampl != NULL)
        gfx_destroy_sampler(pfx->sampl);

    FREE(pfx);
}

gfx_texture gfx_pfx_downsamplewdepth_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_downsample* pfx,
    const struct gfx_view_params* params, gfx_texture src_tex, gfx_texture src_depth,
    OUT gfx_texture* downsample_depthtex)
{
    PRF_OPENSAMPLE("postfx-downsamplewdepth");
    struct gfx_shader* shader = gfx_shader_get(pfx->shader_id);

    gfx_output_setrendertarget(cmdqueue, pfx->rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->rt->desc.rt.width, pfx->rt->desc.rt.height);
    gfx_output_setdepthstencilstate(cmdqueue, pfx->ds_writez, 0);

    gfx_shader_bind(cmdqueue, shader);

    /* constants/textures */
    struct vec2f texelsz;

    vec2f_setf(&texelsz, 1.0f/(float)src_tex->desc.tex.width, 1.0f/(float)src_tex->desc.tex.height);
    gfx_shader_set2f(shader, SHADER_NAME(c_texelsize), texelsz.f);
    gfx_shader_bindconstants(cmdqueue, shader);

    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), pfx->sampl, src_depth);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_tex), pfx->sampl, src_tex);

    gfx_draw_fullscreenquad();

    /* switch back */
    gfx_output_setdepthstencilstate(cmdqueue, NULL, 0);

    /* */
    PRF_CLOSESAMPLE();  /* postfx-downsamplewdepth */

    *downsample_depthtex = pfx->dest_depthtex;
    return pfx->dest_tex;

}

result_t gfx_pfx_downsamplewdepth_resize(struct gfx_pfx_downsample* pfx, uint width,
    uint height)
{
    enum gfxFormat fmt = pfx->dest_tex->desc.tex.fmt;
    pfx_downsamplewdepth_destroyrt(pfx);
    return pfx_downsamplewdepth_creatert(pfx, width, height, fmt);
}


/*************************************************************************************************
 * ssao
 */
INLINE result_t pfx_ssao_creatert(struct gfx_pfx_ssao* pfx, uint width, uint height)
{
    pfx->ssao_tex = gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
    if (pfx->ssao_tex == NULL)
        return RET_FAIL;

    pfx->rt = gfx_create_rendertarget(&pfx->ssao_tex, 1, NULL);
    if (pfx->rt == NULL)
        return RET_FAIL;
    return RET_OK;
}

INLINE void pfx_ssao_destroyrt(struct gfx_pfx_ssao* pfx)
{
    if (pfx->rt != NULL)
        gfx_destroy_rendertarget(pfx->rt);

    if (pfx->ssao_tex != NULL)
        gfx_destroy_texture(pfx->ssao_tex);
}

struct gfx_pfx_ssao* gfx_pfx_ssao_create(uint width, uint height,
    float radius, float bias, float scale, float intensity)
{
    result_t r;

    struct gfx_pfx_ssao* pfx = (struct gfx_pfx_ssao*)ALLOC(sizeof(struct gfx_pfx_ssao), MID_GFX);
    if (pfx == NULL)
        return NULL;

    memset(pfx, 0x00, sizeof(struct gfx_pfx_ssao));
    pfx->noise_tex = INVALID_HANDLE;

    /* textures */
    r = pfx_ssao_creatert(pfx, width, height);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "pfx-ssao init failed: could not create buffers");
        gfx_pfx_ssao_destroy(pfx);
        return NULL;
    }
    pfx->noise_tex = rs_load_texture("textures/noise.dds", 0, FALSE, 0);
    if (pfx->noise_tex == INVALID_HANDLE)   {
        err_print(__FILE__, __LINE__, "pfx-ssao init failed: could not create textures");
        gfx_pfx_ssao_destroy(pfx);
        return NULL;
    }

    /* shaders */
    char passcnt[8];
    str_itos(passcnt, PFX_SSAO_PASSES);
    gfx_shader_beginload(eng_get_lsralloc(), "shaders/fsq.vs", "shaders/ssao.ps", NULL, 1,
        "shaders/df-common.inc");
    pfx->shader_id = gfx_shader_add("pfx-ssao", 2, 1,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
        "_PASSCNT_", passcnt);
    gfx_shader_endload();
    if (pfx->shader_id == 0)    {
        err_print(__FILE__, __LINE__, "pfx-ssao init failed: could not create buffers");
        gfx_pfx_ssao_destroy(pfx);
        return NULL;
    }

    /* states */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.address_u = gfxAddressMode::WRAP;
    sdesc.address_v = gfxAddressMode::WRAP;
    sdesc.filter_mag = gfxFilterMode::NEAREST;
    sdesc.filter_min = gfxFilterMode::NEAREST;
    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    pfx->sampl_point = gfx_create_sampler(&sdesc);

    sdesc.address_u = gfxAddressMode::MIRROR;
    sdesc.address_v = gfxAddressMode::MIRROR;
    pfx->sampl_point_mirror = gfx_create_sampler(&sdesc);

    if (pfx->sampl_point == NULL || pfx->sampl_point_mirror == NULL)    {
        err_print(__FILE__, __LINE__, "pfx-ssao init failed: could not create states");
        gfx_pfx_ssao_destroy(pfx);
        return NULL;
    }

    gfx_pfx_ssao_setparams(pfx, radius, bias, scale, intensity);

    /* console commands */
    if (BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))    {
        con_register_cmd("gfx_ssaoparams", pfx_console_ssao_setparams, pfx,
            "gfx_ssaoparams [radius:R] [bias:B] [scale:S] [intensity:I]");
    }
    return pfx;
}

void gfx_pfx_ssao_setparams(struct gfx_pfx_ssao* pfx, float radius, float bias, float scale,
    float intensity)
{
    pfx->radius = radius;
    pfx->bias = bias;
    pfx->scale = scale;
    pfx->intensity = intensity;
}

void gfx_pfx_ssao_destroy(struct gfx_pfx_ssao* pfx)
{
    ASSERT(pfx);
    pfx_ssao_destroyrt(pfx);

    if (pfx->noise_tex != INVALID_HANDLE)
        rs_unload(pfx->noise_tex);

    if (pfx->shader_id != 0)
        gfx_shader_unload(pfx->shader_id);

    if (pfx->sampl_point != NULL)
        gfx_destroy_sampler(pfx->sampl_point);

    if (pfx->sampl_point_mirror != NULL)
        gfx_destroy_sampler(pfx->sampl_point_mirror);

    FREE(pfx);
}

gfx_texture gfx_pfx_ssao_render(gfx_cmdqueue cmdqueue,
    struct gfx_pfx_ssao* pfx, int stencilcmp_value /* =-1 */,
    const struct gfx_view_params* params, gfx_texture depth_tex, gfx_texture norm_tex)
{
    PRF_OPENSAMPLE("postfx-ssao");
    struct gfx_shader* shader = gfx_shader_get(pfx->shader_id);

    gfx_output_setrendertarget(cmdqueue, pfx->rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->rt->desc.rt.width, pfx->rt->desc.rt.height);
    gfx_shader_bind(cmdqueue, shader);

    /* uniforms */
    struct vec4f ssaoparams;
    struct vec4f rtvsz;
    gfx_texture noise_tex = rs_get_texture(pfx->noise_tex);

    vec4_setf(&ssaoparams, pfx->radius, pfx->bias, pfx->scale, pfx->intensity);
    vec4_setf(&rtvsz, (float)pfx->rt->desc.rt.width, (float)pfx->rt->desc.rt.height,
        (float)noise_tex->desc.tex.width, (float)noise_tex->desc.tex.height);

    gfx_shader_set4f(shader, SHADER_NAME(c_params), ssaoparams.f);
    gfx_shader_set4f(shader, SHADER_NAME(c_rtvsz), rtvsz.f);
    gfx_shader_set4f(shader, SHADER_NAME(c_projparams), params->projparams.f);
    gfx_shader_bindconstants(cmdqueue, shader);

    /* textures */
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_noise), pfx->sampl_point,
        noise_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_norm), pfx->sampl_point_mirror,
        norm_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), pfx->sampl_point_mirror,
        depth_tex);

    gfx_draw_fullscreenquad();

    PRF_CLOSESAMPLE();
    return pfx->ssao_tex;
}

result_t gfx_pfx_ssao_resize(struct gfx_pfx_ssao* pfx, uint width, uint height)
{
    pfx_ssao_destroyrt(pfx);
    return pfx_ssao_creatert(pfx, width, height);
}

int gfx_pfx_ssao_debugtext(gfx_cmdqueue cmdqueue, int x, int y, int line_stride,
    void* param)
{
    char text[64];
    struct gfx_pfx_ssao* pfx = (struct gfx_pfx_ssao*)param;

    /* show ssao properties */
    sprintf(text, "[gfx:ssao] radius: %.3f", pfx->radius);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;
    sprintf(text, "[gfx:ssao] bias: %.3f", pfx->bias);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;
    sprintf(text, "[gfx:ssao] scale: %.2f", pfx->scale);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;
    sprintf(text, "[gfx:ssao] intensity: %.2f", pfx->intensity);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;

    return y;
}

result_t pfx_console_ssao_setparams(uint argc, const char ** argv, void* param)
{
    struct gfx_pfx_ssao* pfx = (struct gfx_pfx_ssao*)param;

    float radius = pfx->radius;
    float bias = pfx->bias;
    float scale = pfx->scale;
    float intensity = pfx->intensity;

    if (argc == 0 || argc > 4)
        return RET_INVALIDARG;
    char arg[256];

    /* extract key/values */
    for (uint i = 0; i < argc; i++)   {
        strcpy(arg, argv[i]);
        char* seperator = strchr(arg, ':');
        if (seperator != NULL)  {
            *seperator = 0;
            const char* key = argv[i];
            const char* value = seperator + 1;

            if (str_isequal_nocase(key, "radius"))
                radius = clampf(str_tofl32(value), 0.001f, 10.0f);
            if (str_isequal_nocase(key, "bias"))
                bias = clampf(str_tofl32(value), 0.0f, 3.14f);
            if (str_isequal_nocase(key, "scale"))
                scale = clampf(str_tofl32(value), EPSILON, 10.0f);
            if (str_isequal_nocase(key, "intensity"))
                intensity = clampf(str_tofl32(value), 0.0f, 10.0f);
        }
    }

    gfx_pfx_ssao_setparams(pfx, radius, bias, scale, intensity);
    return RET_OK;
}


/*************************************************************************************************
 * bilateral upsample
 */
INLINE result_t pfx_upsamplebilateral_creatert(struct gfx_pfx_upsample* pfx,
    uint width, uint height)
{
    pfx->tex = gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
    if (pfx->tex == NULL)
        return RET_FAIL;
    pfx->rt = gfx_create_rendertarget(&pfx->tex, 1, NULL);
    if (pfx->rt == NULL)
        return RET_FAIL;
    return RET_OK;
}

INLINE void pfx_upsamplebilateral_releasert(struct gfx_pfx_upsample* pfx)
{
    if (pfx->rt != NULL)
        gfx_destroy_rendertarget(pfx->rt);
    if (pfx->tex != NULL)
        gfx_destroy_texture(pfx->tex);
}

struct gfx_pfx_upsample* gfx_pfx_upsamplebilateral_create(uint width, uint height)
{
    result_t r;

    struct gfx_pfx_upsample* pfx = (struct gfx_pfx_upsample*)ALLOC(sizeof(struct gfx_pfx_upsample),
        MID_GFX);
    ASSERT(pfx);
    memset(pfx, 0x00, sizeof(struct gfx_pfx_upsample));

    /* textures */
    r = pfx_upsamplebilateral_creatert(pfx, width, height);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "pfx-upsample-b init failed: could not create buffers");
        gfx_pfx_upsamplebilateral_destroy(pfx);
        return NULL;
    }

    /* shader */
    gfx_shader_beginload(eng_get_lsralloc(), "shaders/fsq.vs", "shaders/upsample-bilateral.ps",
        NULL, 1, "shaders/df-common.inc");
    pfx->shader_id = gfx_shader_add("pfx-upsampleb", 2, 0,
        gfxInputElemId::POSITION, "vsi_pos", 0,
        gfxInputElemId::TEXCOORD0, "vsi_coord", 0);
    gfx_shader_endload();
    if (pfx->shader_id == 0)    {
        err_print(__FILE__, __LINE__, "pfx-upsample-b init failed: could not create shaders");
        gfx_pfx_upsamplebilateral_destroy(pfx);
        return NULL;
    }

    /* states */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.address_u = gfxAddressMode::CLAMP;
    sdesc.address_v = gfxAddressMode::CLAMP;
    sdesc.filter_mag = gfxFilterMode::NEAREST;
    sdesc.filter_min = gfxFilterMode::NEAREST;
    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    pfx->sampl_point = gfx_create_sampler(&sdesc);
    if (pfx->sampl_point == NULL)   {
        err_print(__FILE__, __LINE__, "pfx-upsample-b init failed: could not create states");
        gfx_pfx_upsamplebilateral_destroy(pfx);
        return NULL;
    }
    sdesc.filter_mag = gfxFilterMode::LINEAR;
    sdesc.filter_min = gfxFilterMode::LINEAR;
    pfx->sampl_lin = gfx_create_sampler(&sdesc);
    if (pfx->sampl_lin == NULL) {
        err_print(__FILE__, __LINE__, "pfx-upsample-b init failed: could not create states");
        gfx_pfx_upsamplebilateral_destroy(pfx);
        return NULL;
    }

    return pfx;
}

void gfx_pfx_upsamplebilateral_destroy(struct gfx_pfx_upsample* pfx)
{
    ASSERT(pfx);

    pfx_upsamplebilateral_releasert(pfx);

    if (pfx->sampl_point != NULL)
        gfx_destroy_sampler(pfx->sampl_point);

    if (pfx->sampl_lin != NULL)
        gfx_destroy_sampler(pfx->sampl_lin);

    if (pfx->shader_id != 0)
        gfx_shader_unload(pfx->shader_id);

    FREE(pfx);
}

gfx_texture gfx_pfx_upsamplebilateral_render(gfx_cmdqueue cmdqueue,
    struct gfx_pfx_upsample* pfx, const struct gfx_view_params* params,
    gfx_texture src_tex, gfx_texture depth_tex, gfx_texture norm_tex,   /* 1/2 size */
    gfx_texture upsample_depthtex, gfx_texture upsample_normtex /* full size */)
{
    PRF_OPENSAMPLE("postfx-upsamplebilateral");
    struct gfx_shader* shader = gfx_shader_get(pfx->shader_id);
    gfx_output_setrendertarget(cmdqueue, pfx->rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->rt->desc.rt.width, pfx->rt->desc.rt.height);
    gfx_shader_bind(cmdqueue, shader);

    /* textures */
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), pfx->sampl_point,
        depth_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_norm), pfx->sampl_point,
        norm_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth_hires), pfx->sampl_point,
        upsample_depthtex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_norm_hires), pfx->sampl_point,
        upsample_normtex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_tex), pfx->sampl_lin, src_tex);

    /* uniform */
    struct vec2f texelsz;
    vec2f_setf(&texelsz, 1.0f/(float)src_tex->desc.tex.width, 1.0f/(float)src_tex->desc.tex.height);
    gfx_shader_set2f(shader, SHADER_NAME(c_texelsize), texelsz.f);
    gfx_shader_set4f(shader, SHADER_NAME(c_projparams), params->projparams.f);
    gfx_shader_bindconstants(cmdqueue, shader);

    /* */
    gfx_draw_fullscreenquad();

    PRF_CLOSESAMPLE();
    return pfx->tex;
}

result_t gfx_pfx_upsamplebilateral_resize(struct gfx_pfx_upsample* pfx, uint width, uint height)
{
    pfx_upsamplebilateral_releasert(pfx);
    return pfx_upsamplebilateral_creatert(pfx, width, height);
}

/*************************************************************************************************
 * shadow-csm
 */
INLINE result_t pfx_shadowcsm_creatert(struct gfx_pfx_shadow* pfx, uint width, uint height)
{
    if (!pfx->ref_mode) {
        pfx->tex = gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
        if (pfx->tex == NULL)
            return RET_FAIL;
    }

    pfx->rt = gfx_create_rendertarget(&pfx->tex, 1, NULL);
    if (pfx->rt == NULL)
        return RET_FAIL;

    return RET_OK;
}

INLINE void pfx_shadowcsm_destroyrt(struct gfx_pfx_shadow* pfx)
{
    if (!pfx->ref_mode && pfx->tex != NULL)
        gfx_destroy_texture(pfx->tex);

    if (pfx->rt != NULL)
        gfx_destroy_rendertarget(pfx->rt);
}

struct gfx_pfx_shadow* gfx_pfx_shadowcsm_create(uint width, uint height,
    OPTIONAL gfx_texture reuse_tex /*=NULL*/)
{
    result_t r;
    struct gfx_pfx_shadow* pfx = (struct gfx_pfx_shadow*)ALLOC(sizeof(struct gfx_pfx_shadow),
        MID_GFX);
    ASSERT(pfx);
    memset(pfx, 0x00, sizeof(struct gfx_pfx_shadow));

    if (reuse_tex != NULL)  {
        pfx->tex = reuse_tex;
        pfx->ref_mode = TRUE;
    }

    /* textures */
    r = pfx_shadowcsm_creatert(pfx, width, height);
    if (IS_FAIL(r))   {
        err_print(__FILE__, __LINE__, "pfx-shadowcsm init failed: could not create buffers");
        gfx_pfx_shadowcsm_destroy(pfx);
        return NULL;
    }

    /* shaders */
    char cascadecnt[16];
    appGfxDeviceVersion hwver = gfx_get_hwver();
    str_itos(cascadecnt, gfx_csm_get_cascadecnt());
    gfx_shader_beginload(eng_get_lsralloc(), "shaders/fsq-pos.vs", "shaders/df-shadow-csm.ps",
        NULL, 2, "shaders/common.inc", "shaders/df-common.inc");

    if (hwver != appGfxDeviceVersion::D3D10_0 && hwver != appGfxDeviceVersion::GL3_2 && hwver != appGfxDeviceVersion::GL3_3) {
        pfx->shader_id =
            gfx_shader_add("df-csm", 2, 1,
            gfxInputElemId::POSITION, "vsi_pos", 0,
            gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
            "_CASCADE_CNT_", cascadecnt);
        if (pfx->shader_id == 0)    {
            gfx_pfx_shadowcsm_destroy(pfx);
            return NULL;
        }

        pfx->prev_shaderid =
            gfx_shader_add("df-csm-prev", 2, 2,
            gfxInputElemId::POSITION, "vsi_pos", 0,
            gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
            "_PREVIEW_", "1",
            "_CASCADE_CNT_", cascadecnt);
        if (pfx->prev_shaderid == 0)   {
            gfx_pfx_shadowcsm_destroy(pfx);
            return NULL;
        }
    }   else    {
        pfx->shader_id =
            gfx_shader_add("df-csm", 2, 2,
            gfxInputElemId::POSITION, "vsi_pos", 0,
            gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
            "_CASCADE_CNT_", cascadecnt, "_D3D10_", "1");
        if (pfx->shader_id == 0)    {
            gfx_pfx_shadowcsm_destroy(pfx);
            return NULL;
        }

        pfx->prev_shaderid =
            gfx_shader_add("df-csm-prev", 2, 3,
            gfxInputElemId::POSITION, "vsi_pos", 0,
            gfxInputElemId::TEXCOORD0, "vsi_coord", 0,
            "_PREVIEW_", "1", "_CASCADE_CNT_", cascadecnt, "_D3D10_", "1");
        if (pfx->prev_shaderid == 0)   {
            gfx_pfx_shadowcsm_destroy(pfx);
            return NULL;
        }
    }
    gfx_shader_endload();

    /* states */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    sdesc.filter_min = gfxFilterMode::NEAREST;
    sdesc.filter_mag = gfxFilterMode::NEAREST;
    sdesc.address_u = gfxAddressMode::CLAMP;
    sdesc.address_v = gfxAddressMode::CLAMP;
    sdesc.address_w = gfxAddressMode::CLAMP;
    pfx->sampl_point = gfx_create_sampler(&sdesc);
    if (pfx->sampl_point == NULL)   {
        err_print(__FILE__, __LINE__, "pfx-shadowcsm init failed: could not create states");
        gfx_pfx_shadowcsm_destroy(pfx);
        return NULL;
    }

    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    sdesc.filter_min = gfxFilterMode::LINEAR;
    sdesc.filter_mag = gfxFilterMode::LINEAR;
    sdesc.cmp_func = gfxCmpFunc::LESS;
    pfx->sampl_cmp = gfx_create_sampler(&sdesc);
    if (pfx->sampl_cmp == NULL) {
        err_print(__FILE__, __LINE__, "pfx-shadowcsm init failed: could not create states");
        gfx_pfx_shadowcsm_destroy(pfx);
        return NULL;
    }

    /* console commnads */
    if (BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))
        con_register_cmd("gfx_showcsm", pfx_console_shadowcsm_prev, pfx, "gfx_showcsm [1*/0]");

    return pfx;
}

void gfx_pfx_shadowcsm_destroy(struct gfx_pfx_shadow* pfx)
{
    ASSERT(pfx);

    pfx_shadowcsm_destroyrt(pfx);

    if (pfx->shader_id != 0)
        gfx_shader_unload(pfx->shader_id);

    if (pfx->prev_shaderid != 0)
        gfx_shader_unload(pfx->prev_shaderid);

    if (pfx->sampl_point != NULL)
        gfx_destroy_sampler(pfx->sampl_point);

    if (pfx->sampl_cmp != NULL)
        gfx_destroy_sampler(pfx->sampl_cmp);

    FREE(pfx);
}

gfx_texture gfx_pfx_shadowcsm_render(gfx_cmdqueue cmdqueue,
    struct gfx_pfx_shadow* pfx, const struct gfx_view_params* params, gfx_texture depth_tex)
{
    PRF_OPENSAMPLE("postfx-csm");

    struct gfx_shader* shader;
    if (!pfx->prev_mode)
        shader = gfx_shader_get(pfx->shader_id);
    else
        shader = gfx_shader_get(pfx->prev_shaderid);

    gfx_cmdqueue_resetsrvs(cmdqueue);
    gfx_output_setrendertarget(cmdqueue, pfx->rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->rt->desc.rt.width, pfx->rt->desc.rt.height);
    gfx_shader_bind(cmdqueue, shader);

    gfx_texture shadow_tex = gfx_csm_get_shadowtex();

    gfx_shader_set4f(shader, SHADER_NAME(c_projparams), params->projparams.f);
    gfx_shader_setf(shader, SHADER_NAME(c_camfar), params->cam->ffar);
    gfx_shader_set4mv(shader, SHADER_NAME(c_shadow_mats), gfx_csm_get_shadowmats(),
        gfx_csm_get_cascadecnt());
    gfx_shader_set4fv(shader, SHADER_NAME(c_cascades_vs), gfx_csm_get_cascades(&params->view),
        gfx_csm_get_cascadecnt());
    gfx_shader_bindconstants(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), pfx->sampl_point,
        depth_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_shadowmap), pfx->sampl_cmp,
        shadow_tex);

    gfx_draw_fullscreenquad();

    PRF_CLOSESAMPLE(); /* postfx-csm */

    return pfx->tex;
}

result_t gfx_pfx_shadowcsm_resize(struct gfx_pfx_shadow* pfx, uint width, uint height)
{
    pfx_shadowcsm_destroyrt(pfx);
    return pfx_shadowcsm_creatert(pfx, width, height);
}


result_t pfx_console_shadowcsm_prev(uint argc, const char** argv, void* param)
{
    int enable = TRUE;
    if (argc == 1)
        enable = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;
    struct gfx_pfx_shadow* pfx = (struct gfx_pfx_shadow*)param;
    pfx->prev_mode = enable;

    if (enable)
        hud_add_image("pfx-csm", pfx->tex, TRUE, 0, 0, "[CSM]");
    else
        hud_remove_image("pfx-csm");
    return RET_OK;
}

/*************************************************************************************************
 * tonemapping
 */
INLINE result_t pfx_tonemap_creatert(struct gfx_pfx_tonemap* pfx, uint width, uint height)
{
    /* tonemapped output buffer */
    pfx->tex = gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
    if (pfx->tex == NULL)
        return RET_FAIL;
    pfx->rt = gfx_create_rendertarget(&pfx->tex, 1, NULL);
    if (pfx->rt == NULL)
        return RET_FAIL;

    /* mipmapped luminance buffer */
    enum appGfxShadingQuality sh_quality = gfx_get_params()->shading_quality;
    uint divider = sh_quality == appGfxShadingQuality::HIGH ? 3 : 4;
    int lum_texsz = maxi(width/divider, height/divider);
    pfx->lum_tex = gfx_create_texturert(lum_texsz, lum_texsz, gfxFormat::R32_FLOAT, TRUE);
    if (pfx->lum_tex == NULL)
        return RET_FAIL;
    pfx->lum_rt = gfx_create_rendertarget(&pfx->lum_tex, 1, NULL);
    if (pfx->lum_rt == NULL)
        return RET_FAIL;
    pfx->bright_tex[0] = gfx_create_texturert(lum_texsz, lum_texsz, gfxFormat::RGBA_UNORM, FALSE);
    if (pfx->bright_tex[0] == NULL)
        return RET_FAIL;
    pfx->bright_tex[1] = gfx_create_texturert(lum_texsz, lum_texsz, gfxFormat::RGBA_UNORM, FALSE);
    if (pfx->bright_tex[1] == NULL)
        return RET_FAIL;

    gfx_texture lumbright_tex[] = {pfx->lum_tex, pfx->bright_tex[0]};
    pfx->lumbright_rt = gfx_create_rendertarget(lumbright_tex, 2, NULL);
    if (pfx->lumbright_rt == NULL)
        return RET_FAIL;
    pfx->blur_rt[0] = gfx_create_rendertarget(&pfx->bright_tex[0], 1, NULL);
    pfx->blur_rt[1] = gfx_create_rendertarget(&pfx->bright_tex[1], 1, NULL);
    if (pfx->blur_rt[0] == NULL || pfx->blur_rt[1] == NULL)
        return RET_FAIL;

    /* luminance adaptation buffers */
    pfx->lumadapt_tex[0] = gfx_create_texturert(1, 1, gfxFormat::R16_FLOAT, FALSE);
    pfx->lumadapt_tex[1] = gfx_create_texturert(1, 1, gfxFormat::R16_FLOAT, FALSE);
    if (pfx->lumadapt_tex[0] == NULL || pfx->lumadapt_tex[1] == NULL)
        return RET_FAIL;

    pfx->lumadapt_rt[0] = gfx_create_rendertarget(&pfx->lumadapt_tex[0], 1, NULL);
    pfx->lumadapt_rt[1] = gfx_create_rendertarget(&pfx->lumadapt_tex[1], 1, NULL);
    if (pfx->lumadapt_tex[0] == NULL || pfx->lumadapt_tex[1] == NULL)
        return RET_FAIL;

    /* clear adaptation textures */
    gfx_cmdqueue cmdqueue = gfx_get_cmdqueue(0);
    float clear_clr[] = {0.0f, 0.0f, 0.0f, 1.0f};
    gfx_output_setrendertarget(cmdqueue, pfx->lumadapt_rt[0]);
    gfx_output_clearrendertarget(cmdqueue, pfx->lumadapt_rt[0], clear_clr, 1.0f, 0,
        gfxClearFlag::COLOR);

    gfx_output_setrendertarget(cmdqueue, pfx->lumadapt_rt[1]);
    gfx_output_clearrendertarget(cmdqueue, pfx->lumadapt_rt[1], clear_clr, 1.0f, 0,
        gfxClearFlag::COLOR);

    gfx_output_setrendertarget(cmdqueue, NULL);

    /* preview */
    if (BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))   {
        pfx->lumprev_tex = gfx_create_texturert(PFX_TONEMAP_LUMPREV_SIZE, PFX_TONEMAP_LUMPREV_SIZE,
            gfxFormat::RGBA_UNORM, FALSE);
        if (pfx->lumprev_tex == NULL)
            return RET_FAIL;
        pfx->lumprev_rt = gfx_create_rendertarget(&pfx->lumprev_tex, 1, NULL);
        if (pfx->lumprev_rt == NULL)
            return RET_FAIL;
    }

    /* blur kernels */
    pfx_calc_gauss(pfx->blur_kernelh,
        PFX_BLOOM_KERNEL_SIZE, PFX_BLOOM_KERNEL_DEVSQR, PFX_BLOOM_KERNEL_INTENSITY, 0,
        pfx->bright_tex[0]->desc.tex.width, pfx->bright_tex[0]->desc.tex.height);
    pfx_calc_gauss(pfx->blur_kernelv,
        PFX_BLOOM_KERNEL_SIZE, PFX_BLOOM_KERNEL_DEVSQR, PFX_BLOOM_KERNEL_INTENSITY, 1,
        pfx->bright_tex[0]->desc.tex.width, pfx->bright_tex[0]->desc.tex.height);

    return RET_OK;
}

INLINE void pfx_tonemap_destroyrt(struct gfx_pfx_tonemap* pfx)
{
    if (pfx->lumprev_tex != NULL)
        gfx_destroy_texture(pfx->lumprev_tex);
    if (pfx->lumprev_rt != NULL)
        gfx_destroy_rendertarget(pfx->lumprev_rt);
    if (pfx->lumadapt_rt[0] != NULL)
        gfx_destroy_rendertarget(pfx->lumadapt_rt[0]);
    if (pfx->lumadapt_rt[1] != NULL)
        gfx_destroy_rendertarget(pfx->lumadapt_rt[1]);
    if (pfx->lumadapt_tex[0] != NULL)
        gfx_destroy_texture(pfx->lumadapt_tex[0]);
    if (pfx->lumadapt_tex[1] != NULL)
        gfx_destroy_texture(pfx->lumadapt_tex[1]);
    if (pfx->lum_rt != NULL)
        gfx_destroy_rendertarget(pfx->lum_rt);
    if (pfx->lumbright_rt != NULL)
        gfx_destroy_rendertarget(pfx->lumbright_rt);
    if (pfx->blur_rt[0] != NULL)
        gfx_destroy_rendertarget(pfx->blur_rt[0]);
    if (pfx->blur_rt[1] != NULL)
        gfx_destroy_rendertarget(pfx->blur_rt[1]);
    if (pfx->bright_tex[0] != NULL)
        gfx_destroy_texture(pfx->bright_tex[0]);
    if (pfx->bright_tex[1] != NULL)
        gfx_destroy_texture(pfx->bright_tex[1]);
    if (pfx->lum_tex != NULL)
        gfx_destroy_texture(pfx->lum_tex);
    if (pfx->rt != NULL)
        gfx_destroy_rendertarget(pfx->rt);
    if (pfx->tex != NULL)
        gfx_destroy_texture(pfx->tex);
}

struct gfx_pfx_tonemap* gfx_pfx_tonemap_create(uint width, uint height, float mid_grey,
    float lum_min, float lum_max, int bloom)
{
    result_t r;
    struct gfx_pfx_tonemap* pfx = (struct gfx_pfx_tonemap*)ALLOC(sizeof(struct gfx_pfx_tonemap),
        MID_GFX);
    ASSERT(pfx);
    memset(pfx, 0x00, sizeof(struct gfx_pfx_tonemap));

    /* buffers */
    r = pfx_tonemap_creatert(pfx, width, height);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create buffers");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    /* shaders */
    struct allocator* alloc = eng_get_lsralloc();
    const struct gfx_input_element_binding bindings[] = {
        {gfxInputElemId::POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
        {gfxInputElemId::TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
    };
    const struct gfx_shader_define bright_defines[] = {
        {"_BRIGHTPASS_", "1"}
    };
    char kernelsz[10];
    str_itos(kernelsz, PFX_BLOOM_KERNEL_SIZE);
    const struct gfx_shader_define blur_defines[] = {
        {"_KERNELSIZE_", kernelsz}
    };

    pfx->shader_id = gfx_shader_load("pfx-tonemap", alloc, "shaders/fsq.vs", "shaders/tonemap.ps",
        NULL, bindings, 2, NULL, 0, NULL);
    if (pfx->shader_id == 0)    {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create shaders");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    pfx->lum_shaderid = gfx_shader_load("pfx-lum", alloc, "shaders/fsq.vs", "shaders/luminance.ps",
        NULL, bindings, 2, NULL, 0, NULL);
    if (pfx->lum_shaderid == 0) {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create shaders");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    pfx->lumbright_shaderid = gfx_shader_load("pfx-lumbright", alloc, "shaders/fsq.vs",
        "shaders/luminance.ps", NULL, bindings, 2, bright_defines, 1, NULL);
    if (pfx->lumbright_shaderid == 0)   {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create shaders");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    pfx->lumadapt_shaderid = gfx_shader_load("pfx-lumadapt", alloc, "shaders/fsq.vs",
        "shaders/luminance-adapt.ps", NULL, bindings, 2, NULL, 0, NULL);
    if (pfx->lumadapt_shaderid == 0) {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create shaders");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    pfx->lumprev_shaderid = gfx_shader_load("pfx-lumprev", alloc, "shaders/fsq.vs",
        "shaders/luminance-prev.ps", NULL, bindings, 2, NULL, 0, NULL);
    if (pfx->lumprev_shaderid == 0) {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create shaders");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    pfx->blur_shaderid = gfx_shader_load("pfx-blur", alloc, "shaders/fsq.vs",
        "shaders/blur.ps", NULL, bindings, 2, blur_defines, 1, NULL);
    if (pfx->blur_shaderid == 0) {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create shaders");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    /* states */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.address_u = gfxAddressMode::CLAMP;
    sdesc.address_v = gfxAddressMode::CLAMP;
    sdesc.filter_min = gfxFilterMode::NEAREST;
    sdesc.filter_mag = gfxFilterMode::NEAREST;
    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    pfx->sampl_point = gfx_create_sampler(&sdesc);

    sdesc.filter_min = gfxFilterMode::LINEAR;
    sdesc.filter_mag = gfxFilterMode::LINEAR;
    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    pfx->sampl_lin = gfx_create_sampler(&sdesc);

    if (pfx->sampl_point == NULL || pfx->sampl_lin == NULL) {
        err_print(__FILE__, __LINE__, "pfx-tonemap init failed: could not create states");
        gfx_pfx_tonemap_destroy(pfx);
        return NULL;
    }

    /* timer */
    pfx->tm = timer_createinstance(TRUE);
    ASSERT(pfx->tm);

    /* console commands */
    if (BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))   {
        con_register_cmd("gfx_showlum", pfx_console_tonemap_showlum, pfx, "gfx_showlum [1*/0]");
        con_register_cmd("gfx_toneparams", gfx_console_tonemap_setparams, pfx,
            "gfx_toneparams [midgrey:N] [lum_min:N] [lum_max:N] [bloom:1*/0]");
    }

    /* initial values */
    gfx_pfx_tonemap_setparams(pfx, mid_grey, lum_min, lum_max, bloom);

    return pfx;
}

void gfx_pfx_tonemap_destroy(struct gfx_pfx_tonemap* pfx)
{
    ASSERT(pfx);
    pfx_tonemap_destroyrt(pfx);

    if (pfx->sampl_lin != NULL)
        gfx_destroy_sampler(pfx->sampl_lin);
    if (pfx->sampl_point != NULL)
        gfx_destroy_sampler(pfx->sampl_point);
    if (pfx->shader_id != 0)
        gfx_shader_unload(pfx->shader_id);
    if (pfx->lum_shaderid != 0)
        gfx_shader_unload(pfx->lum_shaderid);
    if (pfx->lumbright_shaderid != 0)
        gfx_shader_unload(pfx->lumbright_shaderid);
    if (pfx->lumadapt_shaderid != 0)
        gfx_shader_unload(pfx->lumadapt_shaderid);
    if (pfx->lumprev_shaderid != 0)
        gfx_shader_unload(pfx->lumprev_shaderid);
    if (pfx->blur_shaderid != 0)
        gfx_shader_unload(pfx->blur_shaderid);

    FREE(pfx);
}

gfx_texture gfx_pfx_tonemap_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_tonemap* pfx,
    const struct gfx_view_params* params, gfx_texture hdr_tex, OUT gfx_texture* bloom_tex)
{
    struct gfx_shader* shader;

    PRF_OPENSAMPLE("postfx-tonemap");

    /* luminance pass */
    shader = gfx_shader_get(pfx->bloom ? pfx->lumbright_shaderid : pfx->lum_shaderid);

    gfx_output_setrendertarget(cmdqueue, pfx->bloom ? pfx->lumbright_rt : pfx->lum_rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->lum_rt->desc.rt.width, pfx->lum_rt->desc.rt.height);
    gfx_shader_bind(cmdqueue, shader);

    struct vec2f texelsz;
    vec2f_setf(&texelsz, 1.0f/(float)hdr_tex->desc.tex.width, 1.0f/(float)hdr_tex->desc.tex.height);
    gfx_shader_set2f(shader, SHADER_NAME(c_texelsize), texelsz.f);
    if (pfx->bloom)   {
        gfx_shader_setf(shader, SHADER_NAME(c_midgrey), pfx->mid_grey);
        gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_lum), pfx->sampl_point,
            pfx->lumadapt_tex[0]);
    }
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_tex), pfx->sampl_lin, hdr_tex);
    gfx_shader_bindconstants(cmdqueue, shader);
    gfx_draw_fullscreenquad();

    /* generate mips for luminance / calculate number of mips to send to adaptation */
    gfx_texture_generatemips(cmdqueue, pfx->lum_tex);

    /* adaptation pass */
    gfx_cmdqueue_resetsrvs(cmdqueue);
    shader = gfx_shader_get(pfx->lumadapt_shaderid);
    swapptr((void**)&pfx->lumadapt_rt[0], (void**)&pfx->lumadapt_rt[1]);
    swapptr((void**)&pfx->lumadapt_tex[0], (void**)&pfx->lumadapt_tex[1]);
    gfx_output_setrendertarget(cmdqueue, pfx->lumadapt_rt[0]);
    gfx_output_setviewport(cmdqueue, 0, 0, 1, 1);

    gfx_shader_bind(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_lum_target), pfx->sampl_point,
        pfx->lum_tex);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_lum_adapted), pfx->sampl_point,
        pfx->lumadapt_tex[1]);

    uint mipcnt = pfx->lum_tex->desc.tex.mip_cnt;
    const float lum_range[] = {pfx->lum_min, pfx->lum_max};
    gfx_shader_setf(shader, SHADER_NAME(c_elapsedtm), pfx->tm->dt);
    gfx_shader_setf(shader, SHADER_NAME(c_lastmip), (float)(mipcnt - 1));
    gfx_shader_set2f(shader, SHADER_NAME(c_lum_range), lum_range);
    gfx_shader_bindconstants(cmdqueue, shader);
    gfx_draw_fullscreenquad();

    /* tonemap pass */
    shader = gfx_shader_get(pfx->shader_id);
    gfx_output_setrendertarget(cmdqueue, pfx->rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->tex->desc.tex.width, pfx->tex->desc.tex.height);
    gfx_shader_bind(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_lum), pfx->sampl_point,
        pfx->lumadapt_tex[0]);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_tex), pfx->sampl_point,
        hdr_tex);
    gfx_shader_setf(shader, SHADER_NAME(c_midgrey), pfx->mid_grey);
    gfx_shader_bindconstants(cmdqueue, shader);
    gfx_draw_fullscreenquad();

    /* bloom */
    if (pfx->bloom) {
        shader = gfx_shader_get(pfx->blur_shaderid);
        gfx_shader_bind(cmdqueue, shader);

        /* horizontal */
        gfx_output_setrendertarget(cmdqueue, pfx->blur_rt[1]);
        gfx_output_setviewport(cmdqueue, 0, 0, pfx->bright_tex[1]->desc.tex.width,
            pfx->bright_tex[1]->desc.tex.height);
        gfx_shader_set4fv(shader, SHADER_NAME(c_kernel), pfx->blur_kernelh, PFX_BLOOM_KERNEL_SIZE);
        gfx_shader_bindconstants(cmdqueue, shader);
        gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_tex), pfx->sampl_point,
            pfx->bright_tex[0]);
        gfx_draw_fullscreenquad();
        swapptr((void**)&pfx->blur_rt[0], (void**)&pfx->blur_rt[1]);
        swapptr((void**)&pfx->bright_tex[0], (void**)&pfx->bright_tex[1]);

        /* vertical */
        gfx_cmdqueue_resetsrvs(cmdqueue);
        gfx_output_setrendertarget(cmdqueue, pfx->blur_rt[1]);
        gfx_shader_set4fv(shader, SHADER_NAME(c_kernel), pfx->blur_kernelv, PFX_BLOOM_KERNEL_SIZE);
        gfx_shader_bindconstants(cmdqueue, shader);
        gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_tex), pfx->sampl_point,
            pfx->bright_tex[0]);
        gfx_draw_fullscreenquad();
        swapptr((void**)&pfx->blur_rt[0], (void**)&pfx->blur_rt[1]);
        swapptr((void**)&pfx->bright_tex[0], (void**)&pfx->bright_tex[1]);

        /* bright_tex[0] is the result */
        if (bloom_tex)
            *bloom_tex = pfx->bright_tex[0];
    }   else    {
        *bloom_tex = NULL;
    }

    if (pfx->showlum)
        pfx_tonemap_prevlum(cmdqueue, pfx);

    PRF_CLOSESAMPLE();  /* pfx-tonemap */
    return pfx->tex;
}

void pfx_tonemap_prevlum(gfx_cmdqueue cmdqueue, struct gfx_pfx_tonemap* pfx)
{
    struct gfx_shader* shader = gfx_shader_get(pfx->lumprev_shaderid);
    gfx_output_setrendertarget(cmdqueue, pfx->lumprev_rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->lumprev_rt->desc.rt.width,
        pfx->lumprev_rt->desc.rt.height);
    gfx_shader_bind(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_tex), pfx->sampl_lin,
        pfx->lum_tex);
    gfx_draw_fullscreenquad();
}

result_t gfx_pfx_tonemap_resize(struct gfx_pfx_tonemap* pfx, uint width, uint height)
{
    pfx_tonemap_destroyrt(pfx);
    return pfx_tonemap_creatert(pfx, width, height);
}

result_t pfx_console_tonemap_showlum(uint argc, const char** argv, void* param)
{
    int enable = TRUE;
    if (argc == 1)
        enable = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    struct gfx_pfx_tonemap* pfx = (struct gfx_pfx_tonemap*)param;
    pfx->showlum = enable;
    if (enable) {
        hud_add_image("pfx-lum", pfx->lumprev_tex, FALSE, PFX_TONEMAP_LUMPREV_SIZE,
            PFX_TONEMAP_LUMPREV_SIZE, "[Luminance]");
        if (pfx->bloom)   {
            hud_add_image("pfx-bloom", pfx->bright_tex[0], FALSE, PFX_TONEMAP_LUMPREV_SIZE,
                PFX_TONEMAP_LUMPREV_SIZE, "[Bloom]");
        }
        hud_add_image("pfx-lumfinal", pfx->lumadapt_tex[0], FALSE, 32, 32, "");
        hud_add_label("pfx-lumtext", pfx_tonemap_debugtext, pfx);
    }   else    {
        hud_remove_image("pfx-lum");
        hud_remove_image("pfx-bloom");
        hud_remove_image("pfx-lumfinal");
        hud_remove_label("pfx-lumtext");
    }

    return RET_OK;
}

result_t gfx_console_tonemap_setparams(uint argc, const char** argv, void* param)
{
    struct gfx_pfx_tonemap* pfx = (struct gfx_pfx_tonemap*)param;

    float midgrey = pfx->mid_grey;
    float emin = pfx->lum_min;
    float emax = pfx->lum_max;
    int bloom = pfx->bloom;

    if (argc == 0 || argc > 4)
        return RET_INVALIDARG;
    char arg[256];
    /* extract key/values */
    for (uint i = 0; i < argc; i++)   {
        strcpy(arg, argv[i]);
        char* seperator = strchr(arg, ':');
        if (seperator != NULL)  {
            *seperator = 0;
            const char* key = argv[i];
            const char* value = seperator + 1;

            if (str_isequal_nocase(key, "midgrey"))
                midgrey = clampf(str_tofl32(value), 0.01f, 10.0f);
            if (str_isequal_nocase(key, "lum_min"))
                emin = clampf(str_tofl32(value), 0.0f, 10.0f);
            if (str_isequal_nocase(key, "lum_max"))
                emax = clampf(str_tofl32(value), emin, 20.0f);
            if (str_isequal_nocase(key, "bloom"))
                bloom = str_tobool(value);
        }
    }

    gfx_pfx_tonemap_setparams(pfx, midgrey, emin, emax, bloom);
    return RET_OK;
}


void gfx_pfx_tonemap_setparams(struct gfx_pfx_tonemap* pfx, float midgrey,
    float lum_min, float lum_max, int bloom)
{
    pfx->mid_grey = midgrey;
    pfx->lum_max = lum_max;
    pfx->lum_min = lum_min;
    pfx->bloom = bloom;
}


int pfx_tonemap_debugtext(gfx_cmdqueue cmdqueue, int x, int y, int line_stride,
    void* param)
{
    char text[64];
    struct gfx_pfx_tonemap* pfx = (struct gfx_pfx_tonemap*)param;

    /* show ssao properties */
    sprintf(text, "[gfx:tonemap] mid-grey: %.3f", pfx->mid_grey);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;
    sprintf(text, "[gfx:tonemap] lum-range: (%.2f, %.2f)", pfx->lum_min, pfx->lum_max);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;

    return y;
}

/*************************************************************************************************
 * fxaa
 */
INLINE result_t pfx_fxaa_creatert(struct gfx_pfx_fxaa* pfx, uint width, uint height)
{
    pfx->tex = gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
    if (pfx->tex == NULL)
        return RET_FAIL;

    pfx->rt = gfx_create_rendertarget(&pfx->tex, 1, NULL);
    if (pfx->rt == NULL)
        return RET_FAIL;

    return RET_OK;
}

INLINE void pfx_fxaa_destroyrt(struct gfx_pfx_fxaa* pfx)
{
    if (pfx->rt != NULL)
        gfx_destroy_rendertarget(pfx->rt);
    if (pfx->tex != NULL)
        gfx_destroy_texture(pfx->tex);
}

struct gfx_pfx_fxaa* gfx_pfx_fxaa_create(uint width, uint height)
{
    result_t r;
    struct gfx_pfx_fxaa* pfx = (struct gfx_pfx_fxaa*)ALLOC(sizeof(struct gfx_pfx_fxaa), MID_GFX);
    if (pfx == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }
    memset(pfx, 0x00, sizeof(struct gfx_pfx_fxaa));

    /* textures */
    r = pfx_fxaa_creatert(pfx, width, height);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "pfx-fxaa init failed: could not create buffers");
        gfx_pfx_fxaa_destroy(pfx);
        return NULL;
    }

    /* shader */
    struct allocator* lsr_alloc = eng_get_lsralloc();

    /* we load the fxaa 3rdparty shader manually */
    file_t f = fio_openmem(mem_heap(), "shaders/3rdparty/Fxaa3_11.h", FALSE, MID_GFX);
    if (f == NULL)	{
        err_print(__FILE__, __LINE__, "pfx-fxaa init failed: could not load shaders");
        gfx_pfx_fxaa_destroy(pfx);
        return NULL;
    }

    size_t inc_size;
    char* inc_code = (char*)fio_detachmem(f, &inc_size, NULL);
    ASSERT(inc_size > 0);
    fio_close(f);
    char* code = (char*)ALLOC(inc_size + 1, MID_GFX);
    ASSERT(code);
    memcpy(code, inc_code, inc_size);
    code[inc_size] = 0; /* close string */
    FREE(inc_code); /* used mem_heap for fileopen */

    /* construct defines/bindings */
    const struct gfx_input_element_binding bindings[] = {
        {gfxInputElemId::POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
        {gfxInputElemId::TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
    };

    struct gfx_shader_define defines[4];
    uint define_cnt = 2;
    defines[0].name = "FXAA_PC";
    defines[0].value = "1";
    switch (gfx_get_hwver())  {
    case appGfxDeviceVersion::D3D11_0:
        defines[1].name = "FXAA_HLSL_5";
        defines[1].value = "1";
        break;
    case appGfxDeviceVersion::D3D10_1:
    case appGfxDeviceVersion::D3D10_0:
        defines[1].name = "FXAA_HLSL_4";
        defines[1].value = "1";
        break;
    case appGfxDeviceVersion::GL3_2:
    case appGfxDeviceVersion::GL3_3:
        defines[1].name = "FXAA_GLSL_130";
        defines[1].value = "1";
        break;

    case appGfxDeviceVersion::GL4_0:
    case appGfxDeviceVersion::GL4_1:
    case appGfxDeviceVersion::GL4_2:
        defines[1].name = "FXAA_GLSL_130";
        defines[1].value = "1";
        defines[2].name = "GL_ARB_gpu_shader5";
        defines[2].value = "1";
        define_cnt ++;
        break;
    default:
    	ASSERT(0);
    }

    pfx->shader_id = gfx_shader_load("fxaa", lsr_alloc, "shaders/fsq.vs", "shaders/fxaa.ps",
        NULL, bindings, 2, defines, define_cnt, code);
    FREE(code);
    if (pfx->shader_id == 0)    {
        err_print(__FILE__, __LINE__, "pfx-fxaa init failed: could not load shaders");
        gfx_pfx_fxaa_destroy(pfx);
        return NULL;
    }

    /* states */
    struct gfx_sampler_desc sdesc;
    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.address_u = gfxAddressMode::CLAMP;
    sdesc.address_v = gfxAddressMode::CLAMP;
    sdesc.filter_mip = gfxFilterMode::UNKNOWN;
    sdesc.filter_mag = gfxFilterMode::LINEAR;
    sdesc.filter_min = gfxFilterMode::LINEAR;
    pfx->sampl_lin = gfx_create_sampler(&sdesc);
    if (pfx->sampl_lin == NULL)   {
        err_print(__FILE__, __LINE__, "pfx-fxaa init failed: could not load samplers");
        gfx_pfx_fxaa_destroy(pfx);
        return NULL;
    }

    return pfx;
}

void gfx_pfx_fxaa_destroy(struct gfx_pfx_fxaa* pfx)
{
    ASSERT(pfx);
    pfx_fxaa_destroyrt(pfx);
    if (pfx->shader_id != 0)
        gfx_shader_unload(pfx->shader_id);
    if (pfx->sampl_lin != NULL)
        gfx_destroy_sampler(pfx->sampl_lin);

    FREE(pfx);
}

result_t gfx_pfx_fxaa_resize(struct gfx_pfx_fxaa* pfx, uint width, uint height)
{
    pfx_fxaa_destroyrt(pfx);
    return pfx_fxaa_creatert(pfx, width, height);
}

gfx_texture gfx_pfx_fxaa_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_fxaa* pfx,
    gfx_texture rgbl_tex)
{
    PRF_OPENSAMPLE("postfx-fxaa");

    struct gfx_shader* shader = gfx_shader_get(pfx->shader_id);
    float texelsz[] = {1.0f/(float)pfx->rt->desc.rt.width, 1.0f/(float)pfx->rt->desc.rt.height};

    gfx_output_setrendertarget(cmdqueue, pfx->rt);
    gfx_output_setviewport(cmdqueue, 0, 0, pfx->rt->desc.rt.width, pfx->rt->desc.rt.height);
    gfx_shader_bind(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_rgbl), pfx->sampl_lin,
        rgbl_tex);
    gfx_shader_set2f(shader, SHADER_NAME(c_texelsize), texelsz);
    gfx_shader_bindconstants(cmdqueue, shader);

    gfx_draw_fullscreenquad();

    PRF_CLOSESAMPLE();  /* postfx-fxaa */
    return pfx->tex;
}

/*************************************************************************************************/
/**
 * references :
 *  http://en.wikipedia.org/wiki/Gaussian_blur
 *  http://en.wikipedia.org/wiki/Normal_distribution
 */
void pfx_calc_gauss(struct vec4f* kernel, int kernel_cnt, float std_devsqr, float intensity,
    int direction /*=0 horizontal, =1 vertical*/, int width, int height)
{
    ASSERT(kernel_cnt % 2 == 1);    /* should be odd number */

    int hk = kernel_cnt/2;
    float sum = 0.0f;
    float w_stride = 1.0f / (float)width;
    float h_stride = 1.0f / (float)height;

    for (int i = 0; i < kernel_cnt; i++)  {
        float p = (float)(i - hk);
        float x = p/(float)hk;   /* normalize kernel position to -1~1 */
        float w = expf(-(x*x)/(2.0f*std_devsqr)) / sqrtf(2.0f*PI*std_devsqr);
        sum += w;
        vec4_setf(&kernel[i],
            direction == 0 ? (w_stride*p) : 0.0f,
            direction == 1 ? (h_stride*p) : 0.0f,
            w,
            0.0f);
    }

    /* normalize */
    for (int i = 0; i < kernel_cnt; i++)  {
        kernel[i].z /= sum;
        kernel[i].z *= intensity;
    }
}
