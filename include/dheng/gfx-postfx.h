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

#ifndef __GFXPOSTFX_H__
#define __GFXPOSTFX_H__

#include "gfx-types.h"

/* fwd */
struct gfx_pfx_downsample;
struct gfx_pfx_ssao;
struct gfx_pfx_upsample;
struct gfx_pfx_shadow;
struct gfx_pfx_tonemap;
struct gfx_pfx_fxaa;

/**
 * downsample 1/2
 */
struct gfx_pfx_downsample* gfx_pfx_downsample_create(uint width, uint height);
void gfx_pfx_downsample_destroy(struct gfx_pfx_downsample* pfx);
gfx_texture gfx_pfx_downsample_render(struct gfx_pfx_downsample* pfx,
    const struct gfx_view_params* params, gfx_texture src_tex);
result_t gfX_pfx_downsample_resize(struct gfx_pfx_downsample* pfx, uint width, uint height);

/**
 * downsample 1/2 with depth
 */
struct gfx_pfx_downsample* gfx_pfx_downsamplewdepth_create(uint width, uint height,
    enum gfxFormat fmt, int depthpass_stencilvalue /*=-1*/);
void gfx_pfx_downsamplewdepth_destroy(struct gfx_pfx_downsample* pfx);
gfx_texture gfx_pfx_downsamplewdepth_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_downsample* pfx,
    const struct gfx_view_params* params, gfx_texture src_tex, gfx_texture src_depth,
    OUT gfx_texture* downsample_depthtex);
result_t gfx_pfx_downsamplewdepth_resize(struct gfx_pfx_downsample* pfx, uint width,
    uint height);

/**
 * SSAO
 */
struct gfx_pfx_ssao* gfx_pfx_ssao_create(uint width, uint height,
    float radius, float bias, float scale, float intensity);
void gfx_pfx_ssao_setparams(struct gfx_pfx_ssao* pfx, float radius, float bias, float scale,
    float intensity);
void gfx_pfx_ssao_destroy(struct gfx_pfx_ssao* pfx);
gfx_texture gfx_pfx_ssao_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_ssao* pfx,
    int stencilcmp_value /* =-1 */, const struct gfx_view_params* params,
    gfx_texture depth_tex, gfx_texture norm_tex);
result_t gfx_pfx_ssao_resize(struct gfx_pfx_ssao* pfx, uint width, uint height);
int gfx_pfx_ssao_debugtext(gfx_cmdqueue cmdqueue, int x, int y, int line_stride,
    void* param);

/**
 * bilateral upsample
 */
struct gfx_pfx_upsample* gfx_pfx_upsamplebilateral_create(uint width, uint height);
void gfx_pfx_upsamplebilateral_destroy(struct gfx_pfx_upsample* pfx);
gfx_texture gfx_pfx_upsamplebilateral_render(gfx_cmdqueue cmdqueue,
    struct gfx_pfx_upsample* pfx, const struct gfx_view_params* params,
    gfx_texture src_tex, gfx_texture depth_tex, gfx_texture norm_tex,   /* 1/2 size */
    gfx_texture upsample_depthtex, gfx_texture upsample_normtex /* full size */);
result_t gfx_pfx_upsamplebilateral_resize(struct gfx_pfx_upsample* pfx, uint width,
    uint height);

/**
 * deferred csm shadow
 * puts rendered csm shadow in .x (red) component of the texture
 */
struct gfx_pfx_shadow* gfx_pfx_shadowcsm_create(uint width, uint height,
    OPTIONAL gfx_texture reuse_tex /*=NULL*/);
void gfx_pfx_shadowcsm_destroy(struct gfx_pfx_shadow* pfx);
gfx_texture gfx_pfx_shadowcsm_render(gfx_cmdqueue cmdqueue,
    struct gfx_pfx_shadow* pfx, const struct gfx_view_params* params, gfx_texture depth_tex);
result_t gfx_pfx_shadowcsm_resize(struct gfx_pfx_shadow* pfx, uint width, uint height);

/**
 * tonemaping postfx
 */
struct gfx_pfx_tonemap* gfx_pfx_tonemap_create(uint width, uint height, float mid_grey,
    float lum_min, float lum_max, int bloom);
void gfx_pfx_tonemap_destroy(struct gfx_pfx_tonemap* pfx);
gfx_texture gfx_pfx_tonemap_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_tonemap* pfx,
    const struct gfx_view_params* params, gfx_texture hdr_tex, OUT gfx_texture* bloom_tex);
result_t gfx_pfx_tonemap_resize(struct gfx_pfx_tonemap* pfx, uint width, uint height);
void gfx_pfx_tonemap_setparams(struct gfx_pfx_tonemap* pfx, float midgrey,
    float exposure_min, float exposure_max, int bloom);

/**
 * fxaa
 */
struct gfx_pfx_fxaa* gfx_pfx_fxaa_create(uint width, uint height);
void gfx_pfx_fxaa_destroy(struct gfx_pfx_fxaa* pfx);
result_t gfx_pfx_fxaa_resize(struct gfx_pfx_fxaa* pfx, uint width, uint height);
gfx_texture gfx_pfx_fxaa_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_fxaa* pfx,
    gfx_texture rgbl_tex);

/**
 * bloom
 */
struct gfx_pfx_bloom* gfx_pfx_bloom_create(uint width, uint height, int kernel_size,
    float intensity);
void gfx_pfx_bloom_destroy(struct gfx_pfx_bloom* pfx);
result_t gfx_pfx_bloom_resize(struct gfx_pfx_bloom* pfx, uint width, uint height);
gfx_texture gfx_pfx_bloom_render(gfx_cmdqueue cmdqueue, struct gfx_pfx_bloom* pfx,
    struct gfx_pfx_tonemap* tonemap, gfx_texture src_tex);

#endif /* __GFXPOSTFX_H__ */
