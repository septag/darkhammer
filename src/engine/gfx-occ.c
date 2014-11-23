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
#if !defined(_SIMD_SSE_)
#error "Non-SSE version is not implemented yet"
#endif

#include <stdio.h>
#include <smmintrin.h>

#include "dhcore/core.h"
#include "dhcore/hwinfo.h"

#include "gfx.h"
#include "gfx-types.h"
#include "gfx-occ.h"
#include "gfx-device.h"
#include "gfx-cmdqueue.h"
#include "gfx-shader.h"
#include "gfx-model.h"
#include "gfx-canvas.h"

#include "camera.h"
#include "mem-ids.h"
#include "engine.h"
#include "console.h"
#include "debug-hud.h"

#define STEPX_SIZE 4
#define STEPY_SIZE 1
#define OCC_FAR 100.0f
#define OCC_THRESHOLD 5.0f /* N pixels must be visible */

/*************************************************************************************************
 * types
 */

/* callbacks that are used for rasterization, they are cpu depdendant, so we have a version for ..
 * each cpu and call them by their callbacks */
typedef void (*pfn_drawtri)(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);
typedef float (*pfn_testtri)(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);

struct gfx_occ_stats
{
    uint occ_obj_cnt; /* occluder object count */
    uint occ_tri_cnt; /* occluder tri-cnt */
    uint test_obj_cnt;   /* test object count */
    uint test_tri_cnt;   /* test tri-cnt */
};

struct gfx_occ
{
    float* zbuff; /* 16-byte aligned */
    int zbuff_width;
    int zbuff_height;

    /* for preview only */
    gfx_texture tex;
    gfx_texture prev_tex;
    gfx_rendertarget prev_rt;
    uint prev_shaderid;
    gfx_sampler sampl_point;

#if defined(_OCCDEMO_)
    float* zbuff_ext;  /* for drawing occludees */
    gfx_texture tex_ext;
#endif

    struct mat4f viewport;
    struct mat4f viewprojvp;  /* world to viewport matrix */

    int debug;
    struct gfx_occ_stats stats;

    pfn_drawtri drawtri_fn;
    pfn_testtri testtri_fn;
};

/*************************************************************************************************
 * globals
 */
static struct gfx_occ g_occ;

/*************************************************************************************************
 * fwd declarations
 */
void occ_clearzbuff(float* zbuff, int pixel_cnt);
void occ_transform_verts(struct vec3f* rs, const struct vec3f* vs, uint vert_cnt,
    const struct mat3f* world, const struct mat4f* viewprojvp);
void occ_transform_verts_noworld(struct vec3f* rs, const struct vec3f* vs, uint vert_cnt,
    const struct mat4f* viewprojvp);
simd4i_t occ_calc_edge(struct vec4i* e_stepx, struct vec4i* e_stepy,
    const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* origin);

/* we have two versions for each functions, because current AMD processor does not support SSE4.1 */
void occ_drawtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);
void occ_drawtri_amd(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);
float occ_testtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);
float occ_testtri_amd(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);

result_t occ_creatert(uint width, uint height);
void occ_destroyrt();
result_t occ_console_show(uint argc, const char ** argv, void* param);
void occ_renderpreview(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);
int occ_renderprevtext(gfx_cmdqueue cmqueue, int x, int y, int line_stride, void* param);

/*************************************************************************************************
 * inlines
 */
INLINE float occ_calc_area(const struct vec3f* a, const struct vec3f* b, const struct vec3f* c)
{
    return (b->x - a->x)*(c->y - a->y) - (b->y - a->y)*(c->x - a->x);
}

INLINE int min3(int n1, int n2, int n3)
{
    return mini(n1, mini(n2, n3));
}

INLINE int max3(int n1, int n2, int n3)
{
    return maxi(n1, maxi(n2, n3));
}

/*************************************************************************************************/
void gfx_occ_zero()
{
    memset(&g_occ, 0x00, sizeof(g_occ));
}

result_t gfx_occ_init(uint width, uint height, uint cpu_caps)
{
    ASSERT(width % 4 == 0);
    ASSERT(height % 4 == 0);

    uint size = width*height;
    g_occ.zbuff = (float*)ALIGNED_ALLOC(size*sizeof(float), MID_GFX);
    if (g_occ.zbuff == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }
#if defined(_OCCDEMO_)
    g_occ.zbuff_ext = ALIGNED_ALLOC(size*sizeof(float), MID_GFX);
    if (g_occ.zbuff_ext == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }
#endif

    /* create preview buffers/shaders for dev-mode */
    if (BIT_CHECK(eng_get_params()->flags, appEngineFlags::CONSOLE))   {
        if (IS_FAIL(occ_creatert(width, height)))   {
            err_print(__FILE__, __LINE__, "occ-init failed: could not create buffers");
            return RET_FAIL;
        }
        const struct gfx_input_element_binding bindings[] = {
            {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
            {GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
        };

#if defined(_OCCDEMO_)
        const struct gfx_shader_define defines[] = {"_EXTRA_", "1"};
        uint define_cnt = 1;
#else
        const struct gfx_shader_define* defines = NULL;
        uint define_cnt = 0;
#endif
        g_occ.prev_shaderid = gfx_shader_load("occ-prev", eng_get_lsralloc(),
            "shaders/fsq.vs", "shaders/occ-prev.ps", NULL, bindings, 2, defines, define_cnt, NULL);
        if (g_occ.prev_shaderid == 0)   {
            err_print(__FILE__, __LINE__, "occ-init failed: could not load preview shader");
            return RET_FAIL;
        }
        struct gfx_sampler_desc sdesc;
        memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
        sdesc.address_u = gfxAddressMode::CLAMP;
        sdesc.address_v = gfxAddressMode::CLAMP;
        sdesc.filter_min = gfxFilterMode::NEAREST;
        sdesc.filter_mag = gfxFilterMode::NEAREST;
        sdesc.filter_mip = gfxFilterMode::UNKNOWN;
        g_occ.sampl_point = gfx_create_sampler(&sdesc);

        con_register_cmd("gfx_showocc", occ_console_show, NULL, "gfx_showocc [1*/0]");
    }

    /* */
    gfx_occ_setviewport(0, 0, (int)width, (int)height);
    mat4_set_ident(&g_occ.viewprojvp);
    g_occ.zbuff_width = width;
    g_occ.zbuff_height = height;

    memset(&g_occ.stats, 0x00, sizeof(struct gfx_occ_stats));

    if (BIT_CHECK(cpu_caps, HWINFO_CPUEXT_SSE4))    {
        g_occ.drawtri_fn = occ_drawtri;
        g_occ.testtri_fn = occ_testtri;
    }   else    {
        g_occ.drawtri_fn = occ_drawtri_amd;
        g_occ.testtri_fn = occ_testtri_amd;
    }

    return RET_OK;
}

void gfx_occ_setviewport(int x, int y, int width, int height)
{
    float wf = (float)width;
    float hf = (float)height;

    mat4_setf(&g_occ.viewport,
        0.5f*wf,        0.0f,       0.0f,       0.0f,
        0.0f,           -0.5f*hf,    0.0f,       0.0f,
        0.0f,           0.0f,       -1.0f,      0.0f,
        0.5f*wf,        0.5f*hf,    1.0f,       1.0f);
}

void gfx_occ_release()
{
    if (g_occ.zbuff != NULL)
        ALIGNED_FREE(g_occ.zbuff);
#if defined(_OCCDEMO_)
    if (g_occ.zbuff_ext != NULL)
        ALIGNED_FREE(g_occ.zbuff_ext);
#endif
    if (g_occ.prev_shaderid != 0)
        gfx_shader_unload(g_occ.prev_shaderid);
    if (g_occ.sampl_point != NULL)
        gfx_destroy_sampler(g_occ.sampl_point);

    occ_destroyrt();
    gfx_occ_zero();
}

result_t occ_creatert(uint width, uint height)
{
    struct gfx_subresource_data data;
    data.p = g_occ.zbuff;
    data.pitch_row = width;
    data.pitch_slice = 0;
    data.size = width*height*sizeof(float);
    g_occ.tex = gfx_create_texture(gfxTextureType::TEX_2D, width, height, 1, gfxFormat::R32_FLOAT,
        1, 1, data.size, &data, gfxMemHint::DYNAMIC, 0);
    if (g_occ.tex == NULL)
        return RET_FAIL;
    g_occ.prev_tex = gfx_create_texturert(width, height, gfxFormat::RGBA_UNORM, FALSE);
    if (g_occ.prev_tex == NULL)
        return RET_FAIL;
    g_occ.prev_rt = gfx_create_rendertarget(&g_occ.prev_tex, 1, NULL);
    if (g_occ.prev_rt == NULL)
        return RET_FAIL;

#if defined(_OCCDEMO_)
    g_occ.tex_ext = gfx_create_texture(gfxTextureType::TEX_2D, width, height, 1, gfxFormat::R32_FLOAT,
        1, 1, data.size, &data, gfxMemHint::DYNAMIC);
    if (g_occ.tex_ext == NULL)
        return RET_FAIL;
#endif

    return RET_OK;
}

void occ_destroyrt()
{
    if (g_occ.prev_rt != NULL)
        gfx_destroy_rendertarget(g_occ.prev_rt);
    if (g_occ.prev_tex != NULL)
        gfx_destroy_texture(g_occ.prev_tex);
    if (g_occ.tex != NULL)
        gfx_destroy_texture(g_occ.tex);
#if defined(_OCCDEMO_)
    if (g_occ.tex_ext != NULL)
        gfx_destroy_texture(g_occ.tex_ext);
#endif

}

void gfx_occ_setmatrices(const struct mat4f* viewproj)
{
    /* calculate final transform matrix */
    mat4_mul(&g_occ.viewprojvp, viewproj, &g_occ.viewport);
}

void gfx_occ_clear()
{
    ASSERT(g_occ.zbuff != NULL);
    occ_clearzbuff(g_occ.zbuff, g_occ.zbuff_width*g_occ.zbuff_height);
#if defined(_OCCDEMO_)
    occ_clearzbuff(g_occ.zbuff_ext, g_occ.zbuff_width*g_occ.zbuff_height);
#endif
    memset(&g_occ.stats, 0x00, sizeof(struct gfx_occ_stats));

}

void occ_clearzbuff(float* zbuff, int pixel_cnt)
{
    ASSERT(pixel_cnt % 4 == 0);

#if defined(_SIMD_SSE_)
    simd_t one = _mm_set1_ps(1.0f);
    for (int i = 0; i < pixel_cnt; i+=4)
        _mm_stream_ps(&zbuff[i], one);
#else
    for (int i = 0; i < pixel_cnt; i+=4)   {
        zbuff[i] = 1.0f;
        zbuff[i+1] = 1.0f;
        zbuff[i+2] = 1.0f;
        zbuff[i+3] = 1.0f;
    }
#endif
}

void gfx_occ_drawoccluder(struct allocator* tmp_alloc, struct gfx_model_occ* occ,
    const struct mat3f* world)
{
    /* allocate and transform vertices */
    struct vec3f* verts = (struct vec3f*)A_ALIGNED_ALLOC(tmp_alloc,
        occ->vert_cnt*sizeof(struct vec3f), MID_GFX);
    ASSERT(verts);

    occ_transform_verts(verts, occ->poss, occ->vert_cnt, world, &g_occ.viewprojvp);

    /* rasterize triangles into z-buffer */
    for (uint i = 0, cnt = occ->tri_cnt; i < cnt; i++)    {
        uint idx = i*3;

        /* inverse winding-order, because we are using directx coordinates (screen-space) */
        struct vec3f* v0 = &verts[occ->indexes[idx + 2]];
        struct vec3f* v1 = &verts[occ->indexes[idx + 1]];
        struct vec3f* v2 = &verts[occ->indexes[idx]];

        g_occ.drawtri_fn(v0, v1, v2);
    }

    A_ALIGNED_FREE(tmp_alloc, verts);
    g_occ.stats.occ_obj_cnt ++;
    g_occ.stats.occ_tri_cnt += occ->tri_cnt;
}

/* reference: http://www.opengl.org/wiki/Vertex_Transformation */
void occ_transform_verts(struct vec3f* rs, const struct vec3f* vs, uint vert_cnt,
    const struct mat3f* world, const struct mat4f* viewprojvp)
{
    struct mat4f xform_mat;

    /* calculate final transform matrix */
    mat3_mul4(&xform_mat, world, viewprojvp);

    simd_t epv = _mm_set1_ps(0.0000001f);
    simd_t one = _mm_set1_ps(1.0f);

    for (uint i = 0; i < vert_cnt; i++)   {
        /* to viewport-space */
        simd_t row1 = _mm_load_ps(xform_mat.row1);
        simd_t row2 = _mm_load_ps(xform_mat.row2);
        simd_t row3 = _mm_load_ps(xform_mat.row3);
        simd_t row4 = _mm_load_ps(xform_mat.row4);
        simd_t v = _mm_load_ps(vs[i].f);
        simd_t r = _mm_mul_ps(_mm_all_x(v), row1);
        r = _mm_madd(_mm_all_y(v), row2, r);
        r = _mm_madd(_mm_all_z(v), row3, r);
        r = _mm_madd(_mm_all_w(v), row4, r);

        /* normalize and save w_inv value in 'w' component (for culling) */
        simd_t w_inv = _mm_div_ps(one, _mm_max_ps(_mm_all_w(r), epv));
        r = _mm_mul_ps(r, w_inv);
        _mm_store_ps(rs[i].f, r);
        _mm_store_ss(&rs[i].w, w_inv);
    }
}

#if 0
INLINE struct vec3f* occ_intersect_zplane(struct vec3f* r, const struct vec3f* v0,
    const struct vec3f* v1, float zplane)
{
    float d = v1->z - v0->z;
    if (math_iszero(d))
        return vec3_setv(r, v0);
    float t = (zplane - v0->z) / d;
    vec3_sub(r, v1, v0);
    vec3_muls(r, r, t);
    return vec3_add(r, v0, r);
}


/* reference: http://en.wikipedia.org/wiki/Sutherland-Hodgman_clipping_algorithm */
/* returns number of output triangles */
int occ_clip(struct vec3f rv[6],
    const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2)
{
    const struct vec3f* vs[] = {v0, v1, v2};

    /* clip against z=0 plane */
    int c = 0;
    const struct vec3f* E;
    const struct vec3f* S = vs[2];
    float d;

    for (int i = 0; i < 3; i++)   {
        E = vs[i];
        if (E->z >= 0.0f)    {   /* E inside z=0 plane */
            if (S->z < 0.0f)
                occ_intersect_zplane(&rv[c++], S, E, 0.0f);
            vec3_setv(&rv[c++], E);
        }   else if (S->z >= 0.0f)   {   /* S inside z=0 plane */
            occ_intersect_zplane(&rv[c++], S, E, 0.0f);
        }

        S = E;
    }

    if (c == 4) {
        vec3_setv(&rv[5], &rv[3]);
        vec3_setv(&rv[4], &rv[2]);
        vec3_setv(&rv[3], &rv[0]);
        c = 6;
    }
    return c/3;
}
#endif

void occ_drawtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2)
{
    float* buff = g_occ.zbuff;
    int w = g_occ.zbuff_width;
    int h = g_occ.zbuff_height;

    /* cull degenerate and back-facing triangles (counter-clockwise is front) */
    float area = occ_calc_area(v0, v1, v2);
    if (area <= EPSILON)
        return;

    /* cull triangle againts near z-plane */
    if (v0->w > 1.0f || v1->w > 1.0f || v2->w > 1.0f)
        return;

    /* extract the stuff we need from the triangle */
    struct vec2i vs[3];
    simd_t z0 = _mm_set1_ps(1.0f - v0->z);
    simd_t z1 = _mm_set1_ps(1.0f - v1->z);
    simd_t z2 = _mm_set1_ps(1.0f - v2->z);

    vec2i_seti(&vs[0], (int)v0->x, (int)v0->y);
    vec2i_seti(&vs[1], (int)v1->x, (int)v1->y);
    vec2i_seti(&vs[2], (int)v2->x, (int)v2->y);

    /* compute bounding box */
    /* TODO: we can defer this operation and do in SIMD style and save the result in a buffer */
    struct vec2i minpt;
    struct vec2i maxpt;
    minpt.x = min3(vs[0].x, vs[1].x, vs[2].x);
    minpt.y = min3(vs[0].y, vs[1].y, vs[2].y);
    maxpt.x = max3(vs[0].x, vs[1].x, vs[2].x);
    maxpt.y = max3(vs[0].y, vs[1].y, vs[2].y);

    /* align the box x-values to 4 pixels */
    minpt.x = minpt.x - (minpt.x & 3);
    int align = 4 - (maxpt.x & 3);
    maxpt.x = (maxpt.x + align - 1) & ~(align - 1);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    /* construct edge values */
    struct vec2i p;
    struct vec4i e12[2];
    struct vec4i e20[2];
    struct vec4i e01[2];

    vec2i_setv(&p, &minpt);
    simd4i_t w0_row = occ_calc_edge(&e12[0], &e12[1], &vs[1], &vs[2], &p);
    simd4i_t w1_row = occ_calc_edge(&e20[0], &e20[1], &vs[2], &vs[0], &p);
    simd4i_t w2_row = occ_calc_edge(&e01[0], &e01[1], &vs[0], &vs[1], &p);

    /* generate optimized z values for interpolation */
    simd_t a = _mm_set1_ps(area);
    z1 = _mm_div_ps(_mm_sub_ps(z1, z0), a);
    z2 = _mm_div_ps(_mm_sub_ps(z2, z0), a);

    /* rasterize: process 4 pixels in each iteration */
    int idx = minpt.x + minpt.y*w;
    simd4i_t zero = _mm_set1_epi32(0);
    for (p.y = minpt.y; p.y <= maxpt.y; p.y+=STEPY_SIZE, idx+=w)   {
        simd4i_t w0 = w0_row;
        simd4i_t w1 = w1_row;
        simd4i_t w2 = w2_row;
        int x_idx = idx;

        for (p.x = minpt.x; p.x <= maxpt.x; p.x+=STEPX_SIZE,
                x_idx+=STEPX_SIZE,
                w0 = _mm_add_epi32(w0, _mm_load_si128((simd4i_t*)e12[0].n)),
                w1 = _mm_add_epi32(w1, _mm_load_si128((simd4i_t*)e20[0].n)),
                w2 = _mm_add_epi32(w2, _mm_load_si128((simd4i_t*)e01[0].n)))
        {
            /* check inside the triangle (OR and compare results) */
            simd4i_t mask = _mm_cmplt_epi32(zero, _mm_or_si128(_mm_or_si128(w0, w1), w2));
            if (_mm_test_all_zeros(mask, mask))
                continue;

            /* interpolate depth */
            simd_t depth = z0;
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w1), z1));
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w2), z2));

            /* write to buffer (with the help of masks
             * if (mask[lane] == 0 AND prev_depth > depth) then pixel will not be written */
            simd_t prev_depth = _mm_load_ps(&buff[x_idx]);
            simd_t depth_mask = _mm_cmplt_ps(depth, prev_depth);
            simd4i_t final_mask = _mm_and_si128(mask, _mm_castps_si128(depth_mask));
            depth = _mm_blendv_ps(prev_depth, depth, _mm_castsi128_ps(final_mask));
            _mm_store_ps(&buff[x_idx], depth);
        }

        w0_row = _mm_add_epi32(w0_row, _mm_load_si128((simd4i_t*)e12[1].n));
        w1_row = _mm_add_epi32(w1_row, _mm_load_si128((simd4i_t*)e20[1].n));
        w2_row = _mm_add_epi32(w2_row, _mm_load_si128((simd4i_t*)e01[1].n));
    }
}

/**
 * @param e_stepx returns stepx for specified edge
 * @param e_stepy returns stepy for specified edge
 * @return barycentric for 4 pixels */
simd4i_t occ_calc_edge(struct vec4i* e_stepx, struct vec4i* e_stepy,
    const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* origin)
{
    struct vec4i x;
    struct vec4i y;
    struct vec4i tmp;
    struct vec4i r;

    /* edge setup */
    int A = v0->y - v1->y;
    int B = v1->x - v0->x;
    int C = v0->x*v1->y - v0->y*v1->x;

    vec4i_seta(e_stepx, STEPX_SIZE*A);
    vec4i_seta(e_stepy, STEPY_SIZE*B);

    vec4i_add(&x, vec4i_seta(&x, origin->x), vec4i_seti(&tmp, 0, 1, 2, 3));
    vec4i_seta(&y, origin->y);

    vec4i_mul(&r, vec4i_seta(&tmp, A), &x);
    vec4i_add(&r, &r, vec4i_mul(&tmp, vec4i_seta(&tmp, B), &y));
    vec4i_add(&r, &r, vec4i_seta(&tmp, C));

    return _mm_load_si128((simd4i_t*)r.n);
}

result_t occ_console_show(uint argc, const char ** argv, void* param)
{
    int show = TRUE;
    if (argc == 1)
        show = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;
    g_occ.debug = show;

    if (show)   {
        hud_add_image("gfx-occ", g_occ.prev_tex, FALSE, 256, 256, "[Occluders]");
        hud_add_label("gfx-occ-text", occ_renderprevtext, NULL);
    }    else   {
        hud_remove_image("gfx-occ");
        hud_remove_label("gfx-occ-text");
    }
    return RET_OK;
}

int occ_renderprevtext(gfx_cmdqueue cmqueue, int x, int y, int line_stride, void* param)
{
    char text[64];

    sprintf(text, "[occ] occluder-cnt: %d", g_occ.stats.occ_obj_cnt);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;

    sprintf(text, "[occ] occluder-tri-cnt: %d", g_occ.stats.occ_tri_cnt);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;

    sprintf(text, "[occ] test-cnt: %d", g_occ.stats.test_obj_cnt);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;

    sprintf(text, "[occ] test-tri-cnt: %d", g_occ.stats.test_tri_cnt);
    gfx_canvas_text2dpt(text, x, y, 0);
    y += line_stride;

    return y;
}


void gfx_occ_finish(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params)
{
    if (g_occ.debug)
        occ_renderpreview(cmdqueue, params);
}

void occ_renderpreview(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params)
{
    struct gfx_shader* shader = gfx_shader_get(g_occ.prev_shaderid);

    /* update texture data */
    gfx_texture_update(cmdqueue, g_occ.tex, g_occ.zbuff);
#if defined(_OCCDEMO_)
    gfx_texture_update(cmdqueue, g_occ.tex_ext, g_occ.zbuff_ext);
#endif

    /* render depth preview */
    gfx_output_setrendertarget(cmdqueue, g_occ.prev_rt);
    gfx_output_setviewport(cmdqueue, 0, 0, g_occ.zbuff_width, g_occ.zbuff_height);
    gfx_shader_bind(cmdqueue, shader);
    const float camprops[] = {params->cam->fnear, OCC_FAR};
    gfx_shader_set2f(shader, SHADER_NAME(c_camprops), camprops);
    gfx_shader_bindconstants(cmdqueue, shader);
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth), g_occ.sampl_point,
        g_occ.tex);
#if defined(_OCCDEMO_)
    gfx_shader_bindsamplertexture(cmdqueue, shader, SHADER_NAME(s_depth_ext), g_occ.sampl_point,
        g_occ.tex_ext);
#endif
    gfx_draw_fullscreenquad();
}

float gfx_occ_getfar()
{
    return OCC_FAR;
}

int gfx_occ_testbounds(const struct sphere* s, const struct vec3f* xaxis,
    const struct vec3f* yaxis, const struct vec3f* campos)
{
    struct vec4f quad_pts[4];
    g_occ.stats.test_obj_cnt ++;
    g_occ.stats.test_tri_cnt += 2;

    /**
     * calculate object bounding quad
     * quad is a billboard tangent to the furthest/nearest point of the sphere relative to camera */
    simd_t _p = _mm_set_ps(1.0f, s->z, s->y, s->x);
    simd_t _r = _mm_set1_ps(s->r);

    /* zaxis calculation (after sub, w will be 0.0) */
    simd_t _zaxis = _mm_sub_ps(_mm_load_ps(campos->f), _p);

    /* normalize zaxis */
    simd_t _zaxis_lsqr = _mm_mul_ps(_zaxis, _zaxis);
    _zaxis_lsqr = _mm_hadd_ps(_zaxis_lsqr, _zaxis_lsqr);
    _zaxis_lsqr = _mm_hadd_ps(_zaxis_lsqr, _zaxis_lsqr);
    _zaxis = _mm_mul_ps(_zaxis, _mm_rsqrt_ps(_zaxis_lsqr));

    /* scale axises by radius */
    _zaxis = _mm_mul_ps(_zaxis, _r);
    simd_t _xaxis = _mm_mul_ps(_mm_load_ps(xaxis->f), _r);
    simd_t _yaxis = _mm_mul_ps(_mm_load_ps(yaxis->f), _r);
    _p = _mm_add_ps(_p, _zaxis);

    /* calculate quad */
    simd_t _pts[4];
    _pts[0] = _mm_sub_ps(_p, _mm_add_ps(_xaxis, _yaxis));
    _pts[1] = _mm_add_ps(_p, _mm_sub_ps(_yaxis, _xaxis));
    _pts[2] = _mm_add_ps(_p, _mm_add_ps(_xaxis, _yaxis));
    _pts[3] = _mm_add_ps(_p, _mm_sub_ps(_xaxis, _yaxis));

    /* transform quad to clip-space */
    simd_t epv = _mm_set1_ps(0.0000001f);
    simd_t one = _mm_set1_ps(1.0f);
    simd_t row1 = _mm_load_ps(g_occ.viewprojvp.row1);
    simd_t row2 = _mm_load_ps(g_occ.viewprojvp.row2);
    simd_t row3 = _mm_load_ps(g_occ.viewprojvp.row3);
    simd_t row4 = _mm_load_ps(g_occ.viewprojvp.row4);

    for (uint i = 0; i < 4; i++)   {
        /* to viewport-space */
        simd_t v = _pts[i];
        simd_t r = _mm_mul_ps(_mm_all_x(v), row1);
        r = _mm_madd(_mm_all_y(v), row2, r);
        r = _mm_madd(_mm_all_z(v), row3, r);
        r = _mm_add_ps(row4, r);

        /* normalize and save w_inv value in 'w' component (for culling) */
        simd_t w_inv = _mm_div_ps(one, _mm_max_ps(_mm_all_w(r), epv));
        r = _mm_mul_ps(r, w_inv);
        _mm_store_ps(quad_pts[i].f, r);
        _mm_store_ss(&quad_pts[i].w, w_inv);
    }

    float sum = g_occ.testtri_fn(&quad_pts[0], &quad_pts[1], &quad_pts[2]);
    if (sum > OCC_THRESHOLD)
        return TRUE;
    sum += g_occ.testtri_fn(&quad_pts[2], &quad_pts[3], &quad_pts[0]);
    return sum > OCC_THRESHOLD;
}

float occ_testtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2)
{
    float* buff = g_occ.zbuff;
#if defined(_OCCDEMO_)
    float* buff_ext = g_occ.zbuff_ext;
#endif
    int w = g_occ.zbuff_width;
    int h = g_occ.zbuff_height;

    /* cull degenerate and back-facing triangles (counter-clockwise is front) */
    float area = occ_calc_area(v0, v1, v2);
    if (area <= EPSILON)
        return 0.0f;

    /* cull triangle againts near z-plane */
    if (v0->w > 1.0f || v1->w > 1.0f || v2->w > 1.0f)
        return 0.0f;

    /* extract the stuff we need from the triangle */
    struct vec2i vs[3];
    simd_t z0 = _mm_set1_ps(1.0f - v0->z);
    simd_t z1 = _mm_set1_ps(1.0f - v1->z);
    simd_t z2 = _mm_set1_ps(1.0f - v2->z);

    vec2i_seti(&vs[0], (int)v0->x, (int)v0->y);
    vec2i_seti(&vs[1], (int)v1->x, (int)v1->y);
    vec2i_seti(&vs[2], (int)v2->x, (int)v2->y);

    /* compute bounding box */
    /* TODO: we can defer this operation and do in SIMD style and save the result in a buffer */
    struct vec2i minpt;
    struct vec2i maxpt;
    minpt.x = min3(vs[0].x, vs[1].x, vs[2].x);
    minpt.y = min3(vs[0].y, vs[1].y, vs[2].y);
    maxpt.x = max3(vs[0].x, vs[1].x, vs[2].x);
    maxpt.y = max3(vs[0].y, vs[1].y, vs[2].y);

    /* align the box x-values to 4 pixels */
    minpt.x = minpt.x - (minpt.x & 3);
    int align = 4 - (maxpt.x & 3);
    maxpt.x = (maxpt.x + align - 1) & ~(align - 1);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    /* construct edge values */
    struct vec2i p;
    struct vec4i e12[2];
    struct vec4i e20[2];
    struct vec4i e01[2];

    vec2i_setv(&p, &minpt);
    simd4i_t w0_row = occ_calc_edge(&e12[0], &e12[1], &vs[1], &vs[2], &p);
    simd4i_t w1_row = occ_calc_edge(&e20[0], &e20[1], &vs[2], &vs[0], &p);
    simd4i_t w2_row = occ_calc_edge(&e01[0], &e01[1], &vs[0], &vs[1], &p);

    /* generate optimized z values for interpolation */
    simd_t a = _mm_set1_ps(area);
    z1 = _mm_div_ps(_mm_sub_ps(z1, z0), a);
    z2 = _mm_div_ps(_mm_sub_ps(z2, z0), a);

    /* rasterize: process 4 pixels in each iteration */
    int idx = minpt.x + minpt.y*w;

    float cnt = 0.0f;
    simd4i_t zero = _mm_set1_epi32(0);
    simd_t onef = _mm_set1_ps(1.0f);
    simd_t zerof = _mm_set1_ps(0.0f);
    simd_t cnt4 = _mm_set1_ps(0.0f);
    for (p.y = minpt.y; p.y <= maxpt.y; p.y+=STEPY_SIZE, idx+=w)   {
        simd4i_t w0 = w0_row;
        simd4i_t w1 = w1_row;
        simd4i_t w2 = w2_row;
        int x_idx = idx;

        for (p.x = minpt.x; p.x <= maxpt.x; p.x+=STEPX_SIZE,
                x_idx+=STEPX_SIZE,
                w0 = _mm_add_epi32(w0, _mm_load_si128((simd4i_t*)e12[0].n)),
                w1 = _mm_add_epi32(w1, _mm_load_si128((simd4i_t*)e20[0].n)),
                w2 = _mm_add_epi32(w2, _mm_load_si128((simd4i_t*)e01[0].n)))
        {
            /* check inside the triangle (OR and compare results) */
            simd4i_t mask = _mm_cmplt_epi32(zero, _mm_or_si128(_mm_or_si128(w0, w1), w2));
            if (_mm_test_all_zeros(mask, mask))
                continue;

            /* interpolate depth */
            simd_t depth = z0;
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w1), z1));
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w2), z2));

            /* count zbuffer writes
             * if (mask[lane] == 0 AND prev_depth > depth) then pixel will not be written */
            simd_t prev_depth = _mm_load_ps(&buff[x_idx]);
            simd_t depth_mask = _mm_cmplt_ps(depth, prev_depth);
            simd4i_t final_mask = _mm_and_si128(mask, _mm_castps_si128(depth_mask));
            simd_t test = _mm_blendv_ps(zerof, onef, _mm_castsi128_ps(final_mask));
            test = _mm_hadd_ps(test, test);
            test = _mm_hadd_ps(test, test);
            cnt4 = _mm_add_ss(cnt4, test);

#if defined(_OCCDEMO_)
            /* demo only: write to secondry buffer for preview */
            _mm_store_ps(&buff_ext[x_idx], depth);
#endif
        }

        w0_row = _mm_add_epi32(w0_row, _mm_load_si128((simd4i_t*)e12[1].n));
        w1_row = _mm_add_epi32(w1_row, _mm_load_si128((simd4i_t*)e20[1].n));
        w2_row = _mm_add_epi32(w2_row, _mm_load_si128((simd4i_t*)e01[1].n));
        _mm_store_ss(&cnt, cnt4);

#if !defined(_OCCDEMO_)
        /* early exit (have to do more tests in terms of performance) */
        if (cnt > OCC_THRESHOLD)
            return cnt;
#endif
    }

    return cnt;
}

/*************************************************************************************************
 * AMD variations (without SSE4.1)
 */
INLINE int occ_amd_testallzeros(simd4i_t xmm)
{
    return _mm_movemask_epi8(_mm_cmpeq_epi8(xmm, _mm_setzero_si128())) == 0xFFFF;
}

INLINE simd_t occ_amd_blendps(simd_t a, simd_t b, simd_t mask)
{
    b = _mm_and_ps(mask, b);
    a = _mm_andnot_ps(mask, a);
    return _mm_or_ps(a, b);
}

void occ_drawtri_amd(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2)
{
    float* buff = g_occ.zbuff;
    int w = g_occ.zbuff_width;
    int h = g_occ.zbuff_height;

    /* cull degenerate and back-facing triangles (counter-clockwise is front) */
    float area = occ_calc_area(v0, v1, v2);
    if (area <= EPSILON)
        return;

    /* cull triangle againts near z-plane */
    if (v0->w > 1.0f || v1->w > 1.0f || v2->w > 1.0f)
        return;

    /* extract the stuff we need from the triangle */
    struct vec2i vs[3];
    simd_t z0 = _mm_set1_ps(1.0f - v0->z);
    simd_t z1 = _mm_set1_ps(1.0f - v1->z);
    simd_t z2 = _mm_set1_ps(1.0f - v2->z);

    vec2i_seti(&vs[0], (int)v0->x, (int)v0->y);
    vec2i_seti(&vs[1], (int)v1->x, (int)v1->y);
    vec2i_seti(&vs[2], (int)v2->x, (int)v2->y);

    /* compute bounding box */
    /* TODO: we can defer this operation and do in SIMD style and save the result in a buffer */
    struct vec2i minpt;
    struct vec2i maxpt;
    minpt.x = min3(vs[0].x, vs[1].x, vs[2].x);
    minpt.y = min3(vs[0].y, vs[1].y, vs[2].y);
    maxpt.x = max3(vs[0].x, vs[1].x, vs[2].x);
    maxpt.y = max3(vs[0].y, vs[1].y, vs[2].y);

    /* align the box x-values to 4 pixels */
    minpt.x = minpt.x - (minpt.x & 3);
    int align = 4 - (maxpt.x & 3);
    maxpt.x = (maxpt.x + align - 1) & ~(align - 1);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    /* construct edge values */
    struct vec2i p;
    struct vec4i e12[2];
    struct vec4i e20[2];
    struct vec4i e01[2];

    vec2i_setv(&p, &minpt);
    simd4i_t w0_row = occ_calc_edge(&e12[0], &e12[1], &vs[1], &vs[2], &p);
    simd4i_t w1_row = occ_calc_edge(&e20[0], &e20[1], &vs[2], &vs[0], &p);
    simd4i_t w2_row = occ_calc_edge(&e01[0], &e01[1], &vs[0], &vs[1], &p);

    /* generate optimized z values for interpolation */
    simd_t a = _mm_set1_ps(area);
    z1 = _mm_div_ps(_mm_sub_ps(z1, z0), a);
    z2 = _mm_div_ps(_mm_sub_ps(z2, z0), a);

    /* rasterize: process 4 pixels in each iteration */
    int idx = minpt.x + minpt.y*w;
    simd4i_t zero = _mm_set1_epi32(0);
    for (p.y = minpt.y; p.y <= maxpt.y; p.y+=STEPY_SIZE, idx+=w)   {
        simd4i_t w0 = w0_row;
        simd4i_t w1 = w1_row;
        simd4i_t w2 = w2_row;
        int x_idx = idx;

        for (p.x = minpt.x; p.x <= maxpt.x; p.x+=STEPX_SIZE,
                x_idx+=STEPX_SIZE,
                w0 = _mm_add_epi32(w0, _mm_load_si128((simd4i_t*)e12[0].n)),
                w1 = _mm_add_epi32(w1, _mm_load_si128((simd4i_t*)e20[0].n)),
                w2 = _mm_add_epi32(w2, _mm_load_si128((simd4i_t*)e01[0].n)))
        {
            /* check inside the triangle (OR and compare results) */
            simd4i_t mask = _mm_cmplt_epi32(zero, _mm_or_si128(_mm_or_si128(w0, w1), w2));
            if (occ_amd_testallzeros(mask))
                continue;

            /* interpolate depth */
            simd_t depth = z0;
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w1), z1));
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w2), z2));

            /* write to buffer (with the help of masks
             * if (mask[lane] == 0 AND prev_depth > depth) then pixel will not be written */
            simd_t prev_depth = _mm_load_ps(&buff[x_idx]);
            simd_t depth_mask = _mm_cmplt_ps(depth, prev_depth);
            simd4i_t final_mask = _mm_and_si128(mask, _mm_castps_si128(depth_mask));
            depth = occ_amd_blendps(prev_depth, depth, _mm_castsi128_ps(final_mask));
            _mm_store_ps(&buff[x_idx], depth);
        }

        w0_row = _mm_add_epi32(w0_row, _mm_load_si128((simd4i_t*)e12[1].n));
        w1_row = _mm_add_epi32(w1_row, _mm_load_si128((simd4i_t*)e20[1].n));
        w2_row = _mm_add_epi32(w2_row, _mm_load_si128((simd4i_t*)e01[1].n));
    }
}

float occ_testtri_amd(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2)
{
    float* buff = g_occ.zbuff;
#if defined(_OCCDEMO_)
    float* buff_ext = g_occ.zbuff_ext;
#endif
    int w = g_occ.zbuff_width;
    int h = g_occ.zbuff_height;

    /* cull degenerate and back-facing triangles (counter-clockwise is front) */
    float area = occ_calc_area(v0, v1, v2);
    if (area <= EPSILON)
        return 0.0f;

    /* cull triangle againts near z-plane */
    if (v0->w > 1.0f || v1->w > 1.0f || v2->w > 1.0f)
        return 0.0f;

    /* extract the stuff we need from the triangle */
    struct vec2i vs[3];
    simd_t z0 = _mm_set1_ps(1.0f - v0->z);
    simd_t z1 = _mm_set1_ps(1.0f - v1->z);
    simd_t z2 = _mm_set1_ps(1.0f - v2->z);

    vec2i_seti(&vs[0], (int)v0->x, (int)v0->y);
    vec2i_seti(&vs[1], (int)v1->x, (int)v1->y);
    vec2i_seti(&vs[2], (int)v2->x, (int)v2->y);

    /* compute bounding box */
    /* TODO: we can defer this operation and do in SIMD style and save the result in a buffer */
    struct vec2i minpt;
    struct vec2i maxpt;
    minpt.x = min3(vs[0].x, vs[1].x, vs[2].x);
    minpt.y = min3(vs[0].y, vs[1].y, vs[2].y);
    maxpt.x = max3(vs[0].x, vs[1].x, vs[2].x);
    maxpt.y = max3(vs[0].y, vs[1].y, vs[2].y);

    /* align the box x-values to 4 pixels */
    minpt.x = minpt.x - (minpt.x & 3);
    int align = 4 - (maxpt.x & 3);
    maxpt.x = (maxpt.x + align - 1) & ~(align - 1);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    /* construct edge values */
    struct vec2i p;
    struct vec4i e12[2];
    struct vec4i e20[2];
    struct vec4i e01[2];

    vec2i_setv(&p, &minpt);
    simd4i_t w0_row = occ_calc_edge(&e12[0], &e12[1], &vs[1], &vs[2], &p);
    simd4i_t w1_row = occ_calc_edge(&e20[0], &e20[1], &vs[2], &vs[0], &p);
    simd4i_t w2_row = occ_calc_edge(&e01[0], &e01[1], &vs[0], &vs[1], &p);

    /* generate optimized z values for interpolation */
    simd_t a = _mm_set1_ps(area);
    z1 = _mm_div_ps(_mm_sub_ps(z1, z0), a);
    z2 = _mm_div_ps(_mm_sub_ps(z2, z0), a);

    /* rasterize: process 4 pixels in each iteration */
    int idx = minpt.x + minpt.y*w;

    float cnt = 0.0f;
    simd4i_t zero = _mm_set1_epi32(0);
    simd_t onef = _mm_set1_ps(1.0f);
    simd_t zerof = _mm_set1_ps(0.0f);
    simd_t cnt4 = _mm_set1_ps(0.0f);
    for (p.y = minpt.y; p.y <= maxpt.y; p.y+=STEPY_SIZE, idx+=w)   {
        simd4i_t w0 = w0_row;
        simd4i_t w1 = w1_row;
        simd4i_t w2 = w2_row;
        int x_idx = idx;

        for (p.x = minpt.x; p.x <= maxpt.x; p.x+=STEPX_SIZE,
                x_idx+=STEPX_SIZE,
                w0 = _mm_add_epi32(w0, _mm_load_si128((simd4i_t*)e12[0].n)),
                w1 = _mm_add_epi32(w1, _mm_load_si128((simd4i_t*)e20[0].n)),
                w2 = _mm_add_epi32(w2, _mm_load_si128((simd4i_t*)e01[0].n)))
        {
            /* check inside the triangle (OR and compare results) */
            simd4i_t mask = _mm_cmplt_epi32(zero, _mm_or_si128(_mm_or_si128(w0, w1), w2));
            if (occ_amd_testallzeros(mask))
                continue;

            /* interpolate depth */
            simd_t depth = z0;
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w1), z1));
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w2), z2));

            /* count zbuffer writes
             * if (mask[lane] == 0 AND prev_depth > depth) then pixel will not be written */
            simd_t prev_depth = _mm_load_ps(&buff[x_idx]);
            simd_t depth_mask = _mm_cmplt_ps(depth, prev_depth);
            simd4i_t final_mask = _mm_and_si128(mask, _mm_castps_si128(depth_mask));
            simd_t test = occ_amd_blendps(zerof, onef, _mm_castsi128_ps(final_mask));
            test = _mm_hadd_ps(test, test);
            test = _mm_hadd_ps(test, test);
            cnt4 = _mm_add_ss(cnt4, test);

#if defined(_OCCDEMO_)
            /* demo only: write to secondry buffer for preview */
            _mm_store_ps(&buff_ext[x_idx], depth);
#endif
        }

        w0_row = _mm_add_epi32(w0_row, _mm_load_si128((simd4i_t*)e12[1].n));
        w1_row = _mm_add_epi32(w1_row, _mm_load_si128((simd4i_t*)e20[1].n));
        w2_row = _mm_add_epi32(w2_row, _mm_load_si128((simd4i_t*)e01[1].n));
        _mm_store_ss(&cnt, cnt4);

#if !defined(_OCCDEMO_)
        /* early exit (have to do more tests in terms of performance) */
        if (cnt > OCC_THRESHOLD)
            return cnt;
#endif
    }

    return cnt;
}
