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

#include "gfx-canvas.h"
#include "dhcore/core.h"
#include "dhcore/linked-list.h"
#include "dhcore/vec-math.h"
#include "dhcore/queue.h"
#include "dhcore/pool-alloc.h"
#include "gfx-shader.h"
#include "gfx-device.h"
#include "gfx-cmdqueue.h"
#include "mem-ids.h"
#include "res-mgr.h"
#include "camera.h"
#include "engine.h"
#include "gfx-model.h"
#include "camera.h"
#include "gfx-buffers.h"

#include <stdio.h>
#include <wchar.h>

#define QUESTION_MARK_ID            63
#define SPACE_ID                    32
#define QUAD_COUNT                  5000

/*************************************************************************************************
 * types
 */

struct canvas_vertex2d
{
    struct vec4f pos;
    struct color clr;
    struct vec2f coord;
};

struct canvas_vertex3d
{
    struct vec4f pos;
    struct vec2f coord;
};

struct canvas_item2d;
/* quad streaming callback function */
/*
 * verts: destination verts to be written
 * quad_cnt: maximum quads available in the buffer
 * streamed_cnt: number of streamed (written) quads
 * item: batch item is also passed
 * Returns TRUE if all quads are written from source, FALSE if there is still some quads
 *   remaining in the source
 */
typedef int (*pfn_canvas_quadstream)(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt);

enum canvas_item2d_type
{
    ITEM2D_TEXT,
    ITEM2D_BMP,
    ITEM2D_RECT,
    ITEM2D_LINE
};

struct canvas_brush
{
    struct color clr0;
    struct color clr1;
    enum gfx_gradient_type grad;  /* = GFX_CANVAS_GRAD_NULL if brush is solid */
};

struct canvas_item2d
{
    struct linked_list l_node;
    struct queue q_node;

    struct linked_list* siblings;
    enum canvas_item2d_type type;
    pfn_canvas_quadstream stream_func;
    struct vec2i p0;
    struct vec2i p1;
    struct color c;
    struct canvas_brush brush;
    uint flags;
    fonthandle_t font;
    struct rect2di clip_rc;
    int clip_enable;

    union   {
        char text[256];
        wchar textw[256];
        int width;
        gfx_texture tex;
    };
};

struct shapes3d
{
	/* vertex buffers */
    gfx_buffer solid_spheres_buff[3];	/* pos */
    gfx_buffer bound_sphere_buff; /* pos */
    gfx_buffer solid_aabb_buff; /* pos */
    gfx_buffer bound_aabb_buff; /* pos */
    gfx_buffer capsule_buff; /* pos */
    gfx_buffer generic_buff; /* generic buffer for streaming */
    gfx_buffer prism_buff;

    /* input layouts */
    gfx_inputlayout solid_spheres[3];
    gfx_inputlayout bound_sphere;
    gfx_inputlayout solid_aabb;
    gfx_inputlayout bound_aabb;
    gfx_inputlayout capsule;
    gfx_inputlayout generic;
    gfx_inputlayout prism;

    uint capsule_halfidx; /* index of second half-sphere, index of first-half is 0 */
    uint capsule_cylidx; /* index of cylinder in capsule */
    uint capsule_cylcnt; /* number of verts in capsule's cylinder */
};

struct states3d
{
    gfx_blendstate blend_alpha;
    gfx_rasterstate rs_solid;
    gfx_rasterstate rs_solid_nocull;
    gfx_rasterstate rs_wire;
    gfx_rasterstate rs_wire_nocull;
    gfx_depthstencilstate ds_depthoff;
    gfx_depthstencilstate ds_depthon;
    gfx_sampler sampl_lin;
};

struct buffers2d
{
	gfx_buffer vb_pos_clr_coord;    /* item: canvas_vertex2d */
	gfx_buffer ib;
	gfx_inputlayout il;
};

struct canvas
{
    struct queue* items2d; /* canvas_item2d.q_node */
    struct canvas_item2d* last_item2d;
    struct pool_alloc item_pool;    /* canvas_item2d */
    struct buffers2d buffers2d;
    uint shader2d_id;
    fonthandle_t font_hdl;
    fonthandle_t def_font_hdl;
    struct color text_color;
    struct canvas_brush brush;
    struct color line_color;
    float alpha;
    int wireframe;
    gfx_sampler sampler2d;
    struct rect2di clip_rc;
    int clip_enable;
    gfx_rasterstate rs_scissor;

    struct gfx_contbuffer contbuffer;

    struct shapes3d shapes;
    struct states3d states;
    uint shader3d_id;
    uint shader3dtex_id;
    reshandle_t bounds_tex; /* texture for world-bounds (any bounds actually) */

    /* 3d canvas frame data */
    gfx_cmdqueue cmdqueue;
    float rt_width;
    float rt_height;
    struct gfx_shader* shader3d;
    struct mat4f viewproj;
};

/*************************************************************************************************
 * globals
 */
static struct canvas g_cvs;

const struct gfx_input_element_binding canvas_shader2d_il[] = {
    {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
    {GFX_INPUTELEMENT_ID_COLOR, "vsi_color", 0, GFX_INPUT_OFFSET_PACKED},
    {GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED}
};

const struct gfx_input_element_binding canvas_shader3d_il[] = {
    {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED},
    {GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord", 0, GFX_INPUT_OFFSET_PACKED},
};

/*************************************************************************************************
 * inlines
 */
INLINE uint canvas_find_char(const struct gfx_font* font, uint ch_id)
{
    struct hashtable_item* item = hashtable_fixed_find(&font->char_table, ch_id);
    if (item != NULL)   return (uint)item->value;
    else                return INVALID_INDEX;
}

INLINE const struct gfx_font_chardesc* canvas_resolve_char(const struct gfx_font* font, uint ch_id)
{
    /* TODO: can do special unicode language rules */
    uint index = canvas_find_char(font, ch_id);
    if (index == INVALID_INDEX)    index = canvas_find_char(font, QUESTION_MARK_ID);
    if (index == INVALID_INDEX)    index = canvas_find_char(font, SPACE_ID);
    return (index != INVALID_INDEX) ? &font->chars[index] : NULL;
}

INLINE float canvas_apply_kerning(const struct gfx_font* font,
                           const struct gfx_font_chardesc* ch,
                           const struct gfx_font_chardesc* next_ch)
{
    for (uint i = 0; i < ch->kern_cnt; i++)    {
        if (font->kerns[ch->kern_idx + i].second_char == next_ch->char_id)
            return font->kerns[ch->kern_idx + i].amount;
    }
    return 0;
}

/*************************************************************************************************
 * forward declarations
 */
float canvas_get_textwidth(const struct gfx_font* font, const void* text, int unicode,
    uint text_len, float* firstchar_width);
const struct vec2f* canvas_get_alignpos(struct vec2f* r, const struct gfx_font* font,
    float text_width, float firstchar_width, const struct vec2i* p0, const struct vec2i* p1,
    uint flags);
void canvas_put_into_renderlist(struct canvas_item2d* item);
int canvas_require_batch(const struct canvas_item2d* item1, const struct canvas_item2d* item2);

int canvas_stream_text(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt);
int canvas_stream_line(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt);
int canvas_stream_rectborder(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt);
int canvas_stream_rect(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt);
int canvas_stream_rect_flipy(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt);

result_t canvas_create_buffers();
result_t canvas_create_shapes();
result_t canvas_create_states();
void canvas_release_buffers();
void canvas_release_shapes();
void canvas_release_states();

gfx_buffer canvas_create_solidsphere(uint horz_seg_cnt, uint vert_seg_cnt);
gfx_buffer canvas_create_boundsphere(uint seg_cnt);
gfx_buffer canvas_create_solidaabb();
gfx_buffer canvas_create_boundaabb();
gfx_buffer canvas_create_prism();
gfx_buffer canvas_create_capsule(uint horz_seg_cnt, uint vert_seg_cnt, uint* halfsphere_idx,
    uint* cyl_idx, uint* cyl_cnt);
void canvas_set_perobject(gfx_cmdqueue cmdqueue, const struct mat3f* m, const struct color* c,
    gfx_texture tex);
int canvas_transform_toclip(struct vec2i* r, const struct vec4f* v, const struct mat4f* viewproj);

void canvas_3d_switchtexture();
void canvas_3d_switchnormal();

/*************************************************************************************************/
void gfx_canvas_zero()
{
    memset(&g_cvs, 0x00, sizeof(g_cvs));
    g_cvs.font_hdl = INVALID_HANDLE;
    g_cvs.def_font_hdl = INVALID_HANDLE;
    g_cvs.bounds_tex = INVALID_HANDLE;
}

result_t gfx_canvas_init()
{
    result_t r;

    log_print(LOG_INFO, "init gfx-canvas ...");

    /* memory pool for items */
    r = mem_pool_create(mem_heap(), &g_cvs.item_pool, sizeof(struct canvas_item2d), 200,
    		MID_GFX);
    if (IS_FAIL(r))
        return r;

    /* buffers */
    if (IS_FAIL(canvas_create_buffers()))   {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not create buffers");
        gfx_canvas_release();
        return RET_FAIL;
    }

    /* canvas2d shader */
    g_cvs.shader2d_id = gfx_shader_load("canvas2d", eng_get_lsralloc(),
    		"shaders/canvas2d.vs",
    		"shaders/canvas2d.ps",
    		NULL, canvas_shader2d_il, GFX_INPUT_GETCNT(canvas_shader2d_il), NULL, 0, NULL);

    if (g_cvs.shader2d_id == 0)    {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not load 2d shader");
        return RET_FAIL;
    }

    /* canvas3d shaders */
    g_cvs.shader3d_id = gfx_shader_load("canvas3d", eng_get_lsralloc(),
        "shaders/canvas3d.vs",
        "shaders/canvas3d.ps",
        NULL, canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, 0, NULL);
    if (g_cvs.shader3d_id == 0)    {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not load 3d shader");
        return RET_FAIL;
    }

    const struct gfx_shader_define defines3d[] = {
        {"_TEXTURE_", "1"}
    };
    g_cvs.shader3dtex_id = gfx_shader_load("canvas3d-tex", eng_get_lsralloc(),
        "shaders/canvas3d.vs",
        "shaders/canvas3d.ps",
        NULL, canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), defines3d, 1, NULL);
    if (g_cvs.shader3dtex_id == 0)  {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not load 3d shader");
        return RET_FAIL;
    }

    /* default font */
    g_cvs.def_font_hdl =
        gfx_font_register(eng_get_lsralloc(), "fonts/monospace16/monospace16.fnt",
        		NULL, "monospace", 16, 0);
    g_cvs.font_hdl = g_cvs.def_font_hdl;
    if (g_cvs.def_font_hdl == INVALID_HANDLE)    {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not load default font");
        return RET_FAIL;
    }

    if (IS_FAIL(canvas_create_shapes()))   {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not create shapes");
        return RET_FAIL;
    }

    if (IS_FAIL(canvas_create_states()))   {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not load default font");
        return RET_FAIL;
    }

    g_cvs.bounds_tex = rs_load_texture("textures/do-not-pass.dds", 0, FALSE, 0);
    if (g_cvs.bounds_tex == INVALID_HANDLE) {
        err_print(__FILE__, __LINE__, "init gfx-canvas failed: could not load textures");
        return RET_FAIL;
    }

    /* default props */
    color_setf(&g_cvs.line_color, 1.0f, 1.0f, 1.0f, 1.0f);
    color_setc(&g_cvs.brush.clr0, &g_color_white);
    color_setc(&g_cvs.brush.clr1, &g_color_white);
    color_setf(&g_cvs.text_color, 1.0f, 1.0f, 1.0f, 1.0f);
    g_cvs.alpha = 1.0f;

    return RET_OK;
}

result_t canvas_create_buffers()
{
    ASSERT(QUAD_COUNT*4 < UINT16_MAX);	/* index buffer limitation */

    /* vertex/index buffers */
    g_cvs.buffers2d.vb_pos_clr_coord =
        gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_DYNAMIC,
        sizeof(struct canvas_vertex2d)*QUAD_COUNT*4, NULL, 0);
    if (g_cvs.buffers2d.vb_pos_clr_coord == NULL)
        return RET_FAIL;
    uint16* indexes = (uint16*)ALLOC(sizeof(uint16)*QUAD_COUNT*6, MID_GFX);
    if (indexes == NULL)
    	return RET_OUTOFMEMORY;

    for (uint16 i = 0; i < QUAD_COUNT; i++)     {
        uint16 v = i*4;
        uint16 idx = i*6;

        indexes[idx] = v;
        indexes[idx+1] = v + 1;
        indexes[idx+2] = v + 2;

        indexes[idx+3] = v + 1;
        indexes[idx+4] = v + 3;
        indexes[idx+5] = v + 2;
    }

    g_cvs.buffers2d.ib = gfx_create_buffer(GFX_BUFFER_INDEX, GFX_MEMHINT_STATIC,
    		sizeof(uint16)*QUAD_COUNT*6, indexes, 0);
    FREE(indexes);
    if (g_cvs.buffers2d.ib == NULL)
        return RET_FAIL;

    /* input layout */
    const struct gfx_input_vbuff_desc vbuffs_il[] = {
        {sizeof(struct canvas_vertex2d), g_cvs.buffers2d.vb_pos_clr_coord}
    };

    g_cvs.buffers2d.il = gfx_create_inputlayout(
        vbuffs_il, GFX_INPUTVB_GETCNT(vbuffs_il),
        canvas_shader2d_il, GFX_INPUT_GETCNT(canvas_shader2d_il),
        g_cvs.buffers2d.ib, GFX_INDEX_UINT16, 0);
    if (g_cvs.buffers2d.il == NULL)
    	return RET_FAIL;

    return RET_OK;
}

result_t canvas_create_shapes()
{
	/* vertex buffers (vec4f:pos only) */
    struct shapes3d* s = &g_cvs.shapes;
    s->solid_spheres_buff[2] = canvas_create_solidsphere(4, 3);
    s->solid_spheres_buff[1] = canvas_create_solidsphere(4*3, 3*3);
    s->solid_spheres_buff[0] = canvas_create_solidsphere(4*8, 3*8);
    s->bound_aabb_buff = canvas_create_boundaabb();
    s->bound_sphere_buff = canvas_create_boundsphere(30);
    s->solid_aabb_buff = canvas_create_solidaabb();
    s->prism_buff = canvas_create_prism();
    s->capsule_buff = canvas_create_capsule(4*3, 3*3, &s->capsule_halfidx, &s->capsule_cylidx,
        &s->capsule_cylcnt);
    s->generic_buff = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_DYNAMIC,
        sizeof(struct canvas_vertex3d)*1000, NULL, 0);

    if (s->solid_spheres_buff[2] == NULL ||
        s->solid_spheres_buff[1] == NULL ||
        s->solid_spheres_buff[0] == NULL ||
        s->bound_aabb_buff == NULL ||
        s->bound_sphere_buff == NULL ||
        s->solid_aabb_buff == NULL ||
        s->capsule_buff == NULL ||
        s->generic_buff == NULL ||
        s->prism_buff == NULL)
    {
        return RET_FAIL;
    }
    gfx_contbuffer_init(&g_cvs.contbuffer, s->generic_buff);

    /* create input layouts */
    const struct gfx_input_vbuff_desc vbuffs0[] = {
        {sizeof(struct canvas_vertex3d), s->solid_spheres_buff[0]}
    };
    s->solid_spheres[0] = gfx_create_inputlayout(vbuffs0, GFX_INPUTVB_GETCNT(vbuffs0),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs1[] = {
        {sizeof(struct canvas_vertex3d), s->solid_spheres_buff[1]}
    };
    s->solid_spheres[1] = gfx_create_inputlayout(vbuffs1, GFX_INPUTVB_GETCNT(vbuffs1),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs2[] = {
        {sizeof(struct canvas_vertex3d), s->solid_spheres_buff[2]}
    };
    s->solid_spheres[2] = gfx_create_inputlayout(vbuffs2, GFX_INPUTVB_GETCNT(vbuffs2),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs3[] = {
        {sizeof(struct canvas_vertex3d), s->bound_aabb_buff}
    };
    s->bound_aabb = gfx_create_inputlayout(vbuffs3, GFX_INPUTVB_GETCNT(vbuffs3),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs4[] = {
        {sizeof(struct canvas_vertex3d), s->bound_sphere_buff}
    };
    s->bound_sphere = gfx_create_inputlayout(vbuffs4, GFX_INPUTVB_GETCNT(vbuffs4),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs5[] = {
        {sizeof(struct canvas_vertex3d), s->solid_aabb_buff}
    };
    s->solid_aabb = gfx_create_inputlayout(vbuffs5, GFX_INPUTVB_GETCNT(vbuffs5),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs6[] = {
        {sizeof(struct canvas_vertex3d), s->prism_buff}
    };
    s->prism = gfx_create_inputlayout(vbuffs6, GFX_INPUTVB_GETCNT(vbuffs6),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs7[] = {
        {sizeof(struct canvas_vertex3d), s->capsule_buff}
    };
    s->capsule = gfx_create_inputlayout(vbuffs7, GFX_INPUTVB_GETCNT(vbuffs7),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    const struct gfx_input_vbuff_desc vbuffs8[] = {
        {sizeof(struct canvas_vertex3d), s->generic_buff}
    };
    s->generic = gfx_create_inputlayout(vbuffs8, GFX_INPUTVB_GETCNT(vbuffs8),
        canvas_shader3d_il, GFX_INPUT_GETCNT(canvas_shader3d_il), NULL, GFX_INDEX_UNKNOWN, 0);

    if (s->solid_spheres[2] == NULL ||
        s->solid_spheres[1] == NULL ||
        s->solid_spheres[0] == NULL ||
        s->bound_aabb == NULL ||
        s->bound_sphere == NULL ||
        s->solid_aabb == NULL ||
        s->capsule == NULL ||
        s->generic == NULL ||
        s->prism == NULL)
    {
        return RET_FAIL;
    }

    return RET_OK;
}

result_t canvas_create_states()
{
	struct gfx_sampler_desc sdesc;
	memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
	sdesc.filter_mip = GFX_FILTER_UNKNOWN;
	g_cvs.sampler2d = gfx_create_sampler(&sdesc);
	if (g_cvs.sampler2d == NULL)
		return RET_FAIL;

    struct states3d* s = &g_cvs.states;

    /* alphaBlendState (normal alpha blending, SRC=SRC_ALPHA, DEST=INV_SRC_ALPHA) */
    struct gfx_blend_desc blend_desc;
    memcpy(&blend_desc, gfx_get_defaultblend(), sizeof(blend_desc));
    blend_desc.enable = TRUE;
    blend_desc.src_blend = GFX_BLEND_SRC_ALPHA;
    blend_desc.dest_blend = GFX_BLEND_INV_SRC_ALPHA;
    s->blend_alpha = gfx_create_blendstate(&blend_desc);

    /* depth enable state (always is Zwrite = FALSE) */
    struct gfx_depthstencil_desc ds_desc;
    memcpy(&ds_desc, gfx_get_defaultdepthstencil(), sizeof(ds_desc));
    ds_desc.depth_enable = FALSE;
    ds_desc.depth_write = FALSE;
    s->ds_depthoff = gfx_create_depthstencilstate(&ds_desc);

    ds_desc.depth_enable = TRUE;
    ds_desc.depth_write = FALSE;
    s->ds_depthon = gfx_create_depthstencilstate(&ds_desc);

    struct gfx_rasterizer_desc rs_desc;
    /* rasterizer states (2d) */
    memcpy(&rs_desc, gfx_get_defaultraster(), sizeof(rs_desc));
    rs_desc.scissor_test = TRUE;
    g_cvs.rs_scissor = gfx_create_rasterstate(&rs_desc);

    /* Rasterizer states (3d)
     * rasterizer states have default (CULL_FRONT) because the shapes are built for CW winding
     * */
    memcpy(&rs_desc, gfx_get_defaultraster(), sizeof(rs_desc));
    rs_desc.fill = GFX_FILL_WIREFRAME;
    s->rs_wire = gfx_create_rasterstate(&rs_desc);
    rs_desc.cull = GFX_CULL_NONE;
    s->rs_wire_nocull = gfx_create_rasterstate(&rs_desc);

    rs_desc.cull = GFX_CULL_FRONT;
    rs_desc.fill = GFX_FILL_SOLID;
    s->rs_solid = gfx_create_rasterstate(&rs_desc);
    rs_desc.cull = GFX_CULL_NONE;
    s->rs_solid_nocull = gfx_create_rasterstate(&rs_desc);

    if (s->blend_alpha == NULL ||
        s->ds_depthoff == NULL ||
        s->ds_depthon == NULL ||
        s->rs_wire == NULL ||
        s->rs_wire_nocull == NULL ||
        s->rs_solid == NULL ||
        s->rs_solid_nocull == NULL ||
        g_cvs.rs_scissor == NULL)
    {
        return RET_FAIL;
    }

    memcpy(&sdesc, gfx_get_defaultsampler(), sizeof(sdesc));
    sdesc.address_u = GFX_ADDRESS_WRAP;
    sdesc.address_v = GFX_ADDRESS_WRAP;
    sdesc.filter_min = GFX_FILTER_LINEAR;
    sdesc.filter_mag = GFX_FILTER_LINEAR;
    sdesc.filter_mip = GFX_FILTER_LINEAR;
    s->sampl_lin = gfx_create_sampler(&sdesc);

    return RET_OK;
}

void canvas_release_buffers()
{
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, g_cvs.buffers2d.vb_pos_clr_coord);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, g_cvs.buffers2d.ib);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, g_cvs.buffers2d.il);
}

void canvas_release_shapes()
{
    struct shapes3d* s = &g_cvs.shapes;

    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->solid_spheres[2]);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->solid_spheres[1]);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->solid_spheres[0]);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->bound_aabb);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->bound_sphere);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->solid_aabb);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->capsule);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->generic);
    GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, s->prism);

    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->solid_spheres_buff[2]);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->solid_spheres_buff[1]);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->solid_spheres_buff[0]);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->bound_aabb_buff);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->bound_sphere_buff);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->solid_aabb_buff);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->capsule_buff);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->generic_buff);
    GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, s->prism_buff);
}

void canvas_release_states()
{
	GFX_DESTROY_DEVOBJ(gfx_destroy_rasterstate, g_cvs.rs_scissor);
	GFX_DESTROY_DEVOBJ(gfx_destroy_sampler, g_cvs.sampler2d);

    struct states3d* s = &g_cvs.states;

    GFX_DESTROY_DEVOBJ(gfx_destroy_blendstate, s->blend_alpha);
    GFX_DESTROY_DEVOBJ(gfx_destroy_depthstencilstate, s->ds_depthoff);
    GFX_DESTROY_DEVOBJ(gfx_destroy_depthstencilstate, s->ds_depthon);
    GFX_DESTROY_DEVOBJ(gfx_destroy_rasterstate, s->rs_solid_nocull);
    GFX_DESTROY_DEVOBJ(gfx_destroy_rasterstate, s->rs_solid);
    GFX_DESTROY_DEVOBJ(gfx_destroy_rasterstate, s->rs_wire);
    GFX_DESTROY_DEVOBJ(gfx_destroy_rasterstate, s->rs_wire_nocull);
    GFX_DESTROY_DEVOBJ(gfx_destroy_sampler, s->sampl_lin);
}

void gfx_canvas_release()
{
    if (g_cvs.shader2d_id != 0)
        gfx_shader_unload(g_cvs.shader2d_id);
    if (g_cvs.shader3d_id != 0)
    	gfx_shader_unload(g_cvs.shader3d_id);
    if (g_cvs.shader3dtex_id != 0)
        gfx_shader_unload(g_cvs.shader3dtex_id);

    canvas_release_buffers();
    canvas_release_shapes();
    canvas_release_states();
    if (g_cvs.bounds_tex != INVALID_HANDLE)
        rs_unload(g_cvs.bounds_tex);

    mem_pool_destroy(&g_cvs.item_pool);
    gfx_canvas_zero();
}

void gfx_canvas_text2dpt(const void* text, int x, int y, uint flags)
{
    /* create a new text item */
    struct canvas_item2d* item = (struct canvas_item2d*)mem_pool_alloc(&g_cvs.item_pool);
    ASSERT(item);
    memset(item, 0x00, sizeof(struct canvas_item2d));

    item->type = ITEM2D_TEXT;
    item->stream_func = canvas_stream_text;
    vec2i_seti(&item->p0, x, y);
    vec2i_seti(&item->p1, x, y);
    color_setc(&item->c, &g_cvs.text_color);
    item->c.a *= g_cvs.alpha;
    item->flags = flags;
    item->font = g_cvs.font_hdl;
    item->clip_enable = g_cvs.clip_enable;
    item->clip_rc = g_cvs.clip_rc;

    if (BIT_CHECK(flags, GFX_TEXT_UNICODE))    {
        wcscpy(item->textw, (const wchar*)text);
        if (item->textw[0] == 0)    {
            mem_pool_free(&g_cvs.item_pool, item);
            return;
        }
    }    else    {
        strcpy(item->text, (const char*)(text));
        if (item->text[0] == 0)    {
            mem_pool_free(&g_cvs.item_pool, item);
            return;
        }
    }

    canvas_put_into_renderlist(item);
}

void gfx_canvas_text2drc(const void* text, const struct rect2di* rc, uint flags)
{
    /* create a new text item */
    struct canvas_item2d* item = (struct canvas_item2d*)mem_pool_alloc(&g_cvs.item_pool);
    ASSERT(item);
    memset(item, 0x00, sizeof(struct canvas_item2d));

    item->type = ITEM2D_TEXT;
    item->stream_func = canvas_stream_text;
    vec2i_seti(&item->p0, rc->x, rc->y);
    vec2i_seti(&item->p1, rc->x + rc->w, rc->y + rc->h);
    color_setc(&item->c, &g_cvs.text_color);
    item->c.a *= g_cvs.alpha;
    item->flags = flags;
    item->font = g_cvs.font_hdl;
    item->clip_enable = g_cvs.clip_enable;
    item->clip_rc = g_cvs.clip_rc;

    if (BIT_CHECK(flags, GFX_TEXT_UNICODE))    {
        wcscpy(item->textw, (const wchar*)text);
        if (item->textw[0] == 0)    {
            mem_pool_free(&g_cvs.item_pool, item);
            return;
        }
    }    else    {
        strcpy(item->text, (const char*)text);
        if (item->text[0] == 0)    {
            mem_pool_free(&g_cvs.item_pool, item);
            return;
        }
    }

    canvas_put_into_renderlist(item);
}

void gfx_canvas_rect2d(const struct rect2di* rc, int line_width, uint flags)
{
    struct canvas_item2d* item = (struct canvas_item2d*)mem_pool_alloc(&g_cvs.item_pool);
    ASSERT(item);
    memset(item, 0x00, sizeof(struct canvas_item2d));

    item->type = ITEM2D_RECT;
    if (!BIT_CHECK(flags, GFX_RECT2D_HOLLOW))    {
        item->stream_func = canvas_stream_rect;
        item->brush = g_cvs.brush;
        item->brush.clr0.a *= g_cvs.alpha;
        item->brush.clr1.a *= g_cvs.alpha;
    }   else    {
        item->stream_func = canvas_stream_rectborder;
        item->c = g_cvs.line_color;
        item->c.a *= g_cvs.alpha;
        item->width = line_width;
    }

    vec2i_seti(&item->p0, rc->x, rc->y);
    vec2i_seti(&item->p1, rc->x + rc->w, rc->y + rc->h);
    item->flags = flags;
    item->font = INVALID_HANDLE;
    item->width = line_width;
    item->clip_enable = g_cvs.clip_enable;
    item->clip_rc = g_cvs.clip_rc;

    canvas_put_into_renderlist(item);
}

void gfx_canvas_bmp2d(gfx_texture tex, uint width, uint height,
		const struct rect2di* rc, uint flags)
{
    struct canvas_item2d* item = (struct canvas_item2d*)mem_pool_alloc(&g_cvs.item_pool);
    ASSERT(item);
    memset(item, 0x00, sizeof(struct canvas_item2d));

    flags |= GFX_BMP2D_EXTRAFLAG;

    if (width == 0)
        width = tex->desc.tex.width;
    if (height == 0)
        height = tex->desc.tex.height;

    item->type = ITEM2D_BMP;
    item->stream_func = BIT_CHECK(flags, GFX_BMP2D_FLIPY) ? canvas_stream_rect_flipy :
        canvas_stream_rect;
    if (BIT_CHECK(flags, GFX_BMP2D_NOFIT))  {
        vec2i_seti(&item->p0, rc->x, rc->y);
        vec2i_seti(&item->p1, rc->x + width, rc->y + height);
    }   else    {
        vec2i_seti(&item->p0, rc->x, rc->y);
        vec2i_seti(&item->p1, rc->x + rc->w, rc->y + rc->h);
    }
    item->brush = g_cvs.brush;
    item->brush.clr0.a *= g_cvs.alpha;
    item->brush.clr1.a *= g_cvs.alpha;
    item->flags = flags;
    item->font = INVALID_HANDLE;
    item->tex = tex;
    item->clip_enable = g_cvs.clip_enable;
    item->clip_rc = g_cvs.clip_rc;

    canvas_put_into_renderlist(item);
}

void gfx_canvas_line2d(int x0, int y0, int x1, int y1, int line_width)
{
    struct canvas_item2d* item = (struct canvas_item2d*)mem_pool_alloc(&g_cvs.item_pool);
    ASSERT(item);
    memset(item, 0x00, sizeof(struct canvas_item2d));

    item->type = ITEM2D_LINE;
    item->stream_func = canvas_stream_line;
    vec2i_seti(&item->p0, x0, y0);
    vec2i_seti(&item->p1, x1, y1);
    color_setc(&item->c, &g_cvs.line_color);
    item->c.a *= g_cvs.alpha;
    item->font = INVALID_HANDLE;
    item->width = line_width;
    item->clip_enable = g_cvs.clip_enable;
    item->clip_rc = g_cvs.clip_rc;

    canvas_put_into_renderlist(item);
}

int canvas_require_batch(const struct canvas_item2d* item1, const struct canvas_item2d* item2)
{
    /* generic check:
     * item types are different
     * clipping is different */
    if (item1->type != item2->type)
        return TRUE;
    if (item1->clip_enable != item2->clip_enable)
        return TRUE;
    if (item1->clip_enable && !rect2di_isequal(item1->clip_rc, item2->clip_rc))
        return TRUE;

    /* type specific check
     * texts: fonts are different
     * images: textures are different */
    switch (item1->type)   {
    case ITEM2D_TEXT:
        return item1->font != item2->font;
    case ITEM2D_BMP:
        return item1->tex != item2->tex;
    default:
        return FALSE;
    }
}


void canvas_put_into_renderlist(struct canvas_item2d* item)
{
    if (g_cvs.last_item2d != NULL)    {
        if (canvas_require_batch(item, g_cvs.last_item2d))    {
            queue_push(&g_cvs.items2d, &item->q_node, item);
            list_add(&item->siblings, &item->l_node, item);
            g_cvs.last_item2d = item;
        }   else    {
            list_addlast(&g_cvs.last_item2d->siblings, &item->l_node, item);
        }
    }    else    {
        queue_push(&g_cvs.items2d, &item->q_node, item);
        list_add(&item->siblings, &item->l_node, item);
        g_cvs.last_item2d = item;
    }
}

void gfx_canvas_render2d(gfx_cmdqueue cmdqueue, gfx_rendertarget rt, float rt_width, float rt_height)
{
    if (g_cvs.items2d == NULL)
        return;

    /* global shader */
    struct gfx_shader* cv_shader = gfx_shader_get(g_cvs.shader2d_id);

    struct gfx_ringbuffer vb;
    gfx_ringbuffer_init(&vb, g_cvs.buffers2d.vb_pos_clr_coord, 4);
    const uint quad_sz = (uint)sizeof(struct canvas_vertex2d)*4;

    /* start render */
    /* TODO: gfx_output_setrendertargets(cmdqueue, &rtv, 1, NULL); */
    gfx_shader_bind(cmdqueue, cv_shader);
    gfx_input_setlayout(cmdqueue, g_cvs.buffers2d.il);
    gfx_shader_bindsampler(cmdqueue, cv_shader, SHADER_NAME(s_bmp), g_cvs.sampler2d);
    gfx_output_setblendstate(cmdqueue, g_cvs.states.blend_alpha, NULL);
	gfx_output_setrasterstate(cmdqueue, NULL);
    gfx_output_setdepthstencilstate(cmdqueue, g_cvs.states.ds_depthoff, 0);

    /* get a batch from queue and render it */
    struct canvas_item2d* item;
    uint written_quads = 0;   /* number of quads that is written from source to main vbuffer */
    uint quad_idx = 0;        /* quad index relative to main vbuffer */
    uint start_idx = 0;       /* start aud index to begin draw (see quad_idx) */
    uint quad_cnt = 0;        /* number of quads to draw in each call */
    int src_eof;             /* end-of-file for source stream */
    uint quads_perdraw;       /* traces quad index in mapped buffer (resets on every "map") */
    uint map_offset;
    uint map_sz;
    uint quads_max;
    int prev_clip = FALSE;

    float rtsz[] = {rt_width, rt_height};
    gfx_shader_set2f(cv_shader, SHADER_NAME(c_rtsz), rtsz);

    struct queue* qitem;;
    while ((qitem = queue_pop(&g_cvs.items2d)) != NULL)    {
    	/* enter each draw batch */
        /* set shader constants specific to each batch type */
        item = (struct canvas_item2d*)qitem->data;
        switch (item->type)	{
        case ITEM2D_BMP:
            gfx_shader_seti(cv_shader, SHADER_NAME(c_type), 1);
            gfx_shader_bindtexture(cmdqueue, cv_shader, SHADER_NAME(s_bmp), item->tex);
        	break;
        case ITEM2D_TEXT:
            gfx_shader_seti(cv_shader, SHADER_NAME(c_type), 1);
            gfx_shader_bindtexture(cmdqueue, cv_shader, SHADER_NAME(s_bmp),
            		rs_get_texture(gfx_font_getf(item->font)->tex_hdl));
        	break;
        default:
        	gfx_shader_seti(cv_shader, SHADER_NAME(c_type), 0);
        	break;
        }

        gfx_shader_bindconstants(cmdqueue, cv_shader);

        /* set scissor test of each batch */
        if (item->clip_enable)	{
        	if (!prev_clip)
        		gfx_output_setrasterstate(cmdqueue, g_cvs.rs_scissor);

        	gfx_output_setscissor(cmdqueue, item->clip_rc.x, item->clip_rc.y,
        			item->clip_rc.w, item->clip_rc.h);
        	prev_clip = TRUE;
        }	else if (!item->clip_enable && prev_clip)	{
        	gfx_output_setrasterstate(cmdqueue, NULL);
        	prev_clip = FALSE;
        }

        /* move through the linked list and draw them using QuadStream of each item */
        struct canvas_vertex2d* verts = (struct canvas_vertex2d*)
            gfx_ringbuffer_map(cmdqueue, &vb, &map_offset, &map_sz);
        quads_max = map_sz / quad_sz;
        quads_perdraw = 0;

        /* iterate through each batch items */
        struct linked_list* litem = &item->l_node;
        do    {
            struct canvas_item2d* cur_item = (struct canvas_item2d*)litem->data;
            src_eof = item->stream_func(&verts[4*quads_perdraw], quads_max, cur_item,
                &written_quads);

            quad_cnt += written_quads;
            quads_max -= written_quads;
            quad_idx += written_quads;
            quad_idx %= QUAD_COUNT;
            quads_perdraw += written_quads;

            /* repeat writing to ring buffer until all source quads are done */
            while (!src_eof)        {
                gfx_ringbuffer_unmap(cmdqueue, &vb, quads_perdraw*quad_sz);

                if (quad_idx == 0)    {
                    /* we were in the last ring and quad stream is incomplete
                     * draw the last ring in the buffer to reset */
                	gfx_draw_indexed(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, start_idx*6, quad_cnt*6,
                			GFX_INDEX_UINT16, GFX_DRAWCALL_2D);
                    quad_cnt = 0;
                    start_idx = 0;
                }

                /* check if we are at the start of the buffer and wait for sync object */
                verts = (struct canvas_vertex2d*)
                    gfx_ringbuffer_map(cmdqueue, &vb, &map_offset, &map_sz);
                quads_max = map_sz / quad_sz;
                quads_perdraw = 0;

                src_eof = item->stream_func(&verts[4*quads_perdraw], quads_max, cur_item,
                    &written_quads);

                quads_max -= written_quads;
                quad_cnt += written_quads;
                quad_idx += written_quads;
                quad_idx %= QUAD_COUNT;
                quads_perdraw += written_quads;
            }

            /* get next item and delete the current one */
            mem_pool_free(&g_cvs.item_pool, cur_item);
            litem = litem->next;
        } while (litem != NULL);

        /* draw remaining item */
        gfx_ringbuffer_unmap(cmdqueue, &vb, quads_perdraw*quad_sz);
    	gfx_draw_indexed(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, start_idx*6, quad_cnt*6,
    			GFX_INDEX_UINT16, GFX_DRAWCALL_2D);

        quad_cnt = 0;
        start_idx = quad_idx;
    }

    g_cvs.last_item2d = NULL;
    if (prev_clip)
    	gfx_output_setrasterstate(cmdqueue, NULL);
}

/* stream callback implementations */
int canvas_stream_text(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt)
{
    static uint char_offset = 0;
    static struct vec2f pos;
    static const struct canvas_item2d* prev_item = NULL;

    uint text_len;
    uint quad_idx = 0;
    uint vertex_idx = 0;
    float direction = BIT_CHECK(item->flags, GFX_TEXT_RTL) ? -1.0f : 1.0f;
    float text_width = 0;
    float firstchar_width = 0;
    struct color color;
    wchar textw[256];
    color_setc(&color, &item->c);

    const struct gfx_font* font = gfx_font_getf(item->font);
    gfx_texture texture = rs_get_texture(font->tex_hdl);
    float width = (float)(texture->desc.tex.width);
    float height = (float)(texture->desc.tex.height);

    /* */
    if (!BIT_CHECK(item->flags, GFX_TEXT_UNICODE))    {
        text_len = (uint)strlen(item->text);
        if (!vec2i_isequal(&item->p0, &item->p1))
            text_width = canvas_get_textwidth(font, item->text, FALSE, text_len, &firstchar_width);
    }    else    {
        text_len = (uint)wcslen(item->textw);
        wcscpy(textw, item->textw);
        /* resolve unicode using meta rules */
        if (font->meta_rules != NULL)
            gfx_font_resolveunicode(font, item->textw, textw, text_len);

        if (!vec2i_isequal(&item->p0, &item->p1))
            text_width = canvas_get_textwidth(font, textw, TRUE, text_len, &firstchar_width);
    }

    /* new text item came in, reset static variables */
    if (item != prev_item)    {
        char_offset = 0;
        canvas_get_alignpos(&pos, font, text_width, firstchar_width, &item->p0, &item->p1,
        		item->flags);
        prev_item = item;
    }

    /* stream text quads */
    while (char_offset < text_len && quad_idx < quad_cnt)        {
        const struct gfx_font_chardesc* ch;
        const struct gfx_font_chardesc* next_ch = NULL;

        if (!BIT_CHECK(item->flags, GFX_TEXT_UNICODE))    {
            ch = canvas_resolve_char(font, item->text[char_offset]);
            if (char_offset < text_len - 1)
                next_ch = canvas_resolve_char(font, item->text[char_offset + 1]);
        }    else    {
            ch = canvas_resolve_char(font, textw[char_offset]);
            if (char_offset < text_len - 1)
                next_ch = canvas_resolve_char(font, textw[char_offset + 1]);
        }

        if (ch == NULL)
            continue;

        /* Crop if we have rectangle bounds */
        if (!vec2i_isequal(&item->p0, &item->p1))    {
            if ((BIT_CHECK(item->flags, GFX_TEXT_RTL) &&
                next_ch != NULL && (pos.x - next_ch->xadvance) < item->p0.x) ||
                ((pos.x + ch->xadvance) > item->p1.x))
            {
                break;
            }
        }

        struct canvas_vertex2d* v0 = &verts[vertex_idx];
        struct canvas_vertex2d* v1 = &verts[vertex_idx + 1];
        struct canvas_vertex2d* v2 = &verts[vertex_idx + 2];
        struct canvas_vertex2d* v3 = &verts[vertex_idx + 3];

        /* top-right */
        vec3_setf(&v0->pos, pos.x + ch->xoffset + ch->width, pos.y + ch->yoffset, 0.0f);
        vec2f_setf(&v0->coord, (ch->x + ch->width)/width, 1.0f - ch->y/height);
        color_setc(&v0->clr, &color);

        /* top-left */
        vec3_setf(&v1->pos, pos.x + ch->xoffset, pos.y + ch->yoffset, 0.0f);
        vec2f_setf(&v1->coord, ch->x/width, 1.0f - ch->y/height);
        color_setc(&v1->clr, &color);

        /* bottom right */
        vec3_setf(&v2->pos, pos.x + ch->xoffset + ch->width, pos.y + ch->yoffset + ch->height, 0.0f);
        vec2f_setf(&v2->coord, (ch->x + ch->width)/width, 1.0f - (ch->y + ch->height)/height);
        color_setc(&v2->clr, &color);

        /* bottom left */
        vec3_setf(&v3->pos, pos.x + ch->xoffset, pos.y + ch->yoffset + ch->height, 0.0f);
        vec2f_setf(&v3->coord, ch->x/width, 1.0f - (ch->y + ch->height)/height);
        color_setc(&v3->clr, &color);

        /* advance horizontally */
        if (BIT_CHECK(item->flags, GFX_TEXT_RTL) && next_ch != NULL)
        	pos.x -= next_ch->xadvance;
        else
        	pos.x += ch->xadvance;

        /* Apply kerning */
        if (next_ch != NULL)
            pos.x += canvas_apply_kerning(font, ch, next_ch) * direction;

        quad_idx ++;
        char_offset ++;
        vertex_idx += 4;
    }

    *streamed_cnt = quad_idx;

    /* check if all requested quads are filled, and there is still text remaining */
    if (quad_idx == quad_cnt && char_offset != text_len)
        return FALSE;

    /* stramed the whole text
     * reset static props */
    char_offset = 0;
    prev_item = NULL;

    return TRUE;
}

float canvas_get_textwidth(const struct gfx_font* font, const void* text,
                    int unicode, uint text_len, float* firstchar_width)
{
    float width = 0;
    const uint8* buffer = (const uint8*)text;
    uint item_size = 1;
    if (unicode)
        item_size = sizeof(wchar);

    for (uint i = 0; i < text_len; i++)    {
        const struct gfx_font_chardesc* ch = canvas_resolve_char(font, buffer[i*item_size]);
        if (ch != NULL)    {
            width += ch->xadvance;
            if (i == 0)        {
                *firstchar_width = width;
            }    else if (i < text_len - 1)    {
                const struct gfx_font_chardesc* next_ch =
                    canvas_resolve_char(font, buffer[(i+1)*item_size]);
                if (next_ch != NULL)
                    width += canvas_apply_kerning(font, ch, next_ch);
            }
        }
    }

    return width;
}

const struct vec2f* canvas_get_alignpos(struct vec2f* r,
                                 const struct gfx_font* font,
                                 float text_width, float firstchar_width,
                                 const struct vec2i* p0, const struct vec2i* p1,
                                 uint flags)
{
    if (math_iszero(text_width))
        return vec2f_setf(r, (float)p0->x, (float)p0->y);

    r->y = BIT_CHECK(flags, GFX_TEXT_VERTICALALIGN) ?
        (float)clampi(p0->y + (p1->y - p0->y)/2 - font->line_height/2, p0->y, p1->y) : (float)p0->y;

    if (BIT_CHECK(flags, GFX_TEXT_CENTERALIGN))	{
        if (BIT_CHECK(flags, GFX_TEXT_RTL))
        	r->x = (float)(p0->x + ((p1->x - p0->x)/2 + text_width/2));
        else
        	r->x = (float)(p0->x + ((p1->x - p0->x)/2 - text_width/2));
    }    else if (BIT_CHECK(flags, GFX_TEXT_RIGHTALIGN))        {
        if (BIT_CHECK(flags, GFX_TEXT_RTL))
        	r->x = (float)(p0->x + ((p1->x - p0->x) - firstchar_width));
        else
        	r->x = (float)(p0->x + ((p1->x - p0->x) - text_width));
    }    else    {
        if (BIT_CHECK(flags, GFX_TEXT_RTL))
        	r->x = (float)(p0->x + text_width);
        else
        	r->x = (float)p0->x;
    }
    return r;
}

int canvas_stream_rect(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt)
{
    struct canvas_vertex2d* v0 = &verts[0];
    struct canvas_vertex2d* v1 = &verts[1];
    struct canvas_vertex2d* v2 = &verts[2];
    struct canvas_vertex2d* v3 = &verts[3];
    struct color clrs[4];
    const struct canvas_brush* brush = &item->brush;
    switch (brush->grad)    {
        case GFX_GRAD_NULL:
        clrs[0] = brush->clr0;
        clrs[1] = brush->clr0;
        clrs[2] = brush->clr0;
        clrs[3] = brush->clr0;
        break;
        case GFX_GRAD_TTB:
        clrs[0] = brush->clr0;
        clrs[1] = brush->clr0;
        clrs[2] = brush->clr1;
        clrs[3] = brush->clr1;
        break;
        case GFX_GRAD_BTT:
        clrs[0] = brush->clr1;
        clrs[1] = brush->clr1;
        clrs[2] = brush->clr0;
        clrs[3] = brush->clr0;
        break;
        case GFX_GRAD_RTL:
        clrs[0] = brush->clr0;
        clrs[1] = brush->clr1;
        clrs[2] = brush->clr0;
        clrs[3] = brush->clr1;
        break;
        case GFX_GRAD_LTR:
        clrs[0] = brush->clr1;
        clrs[1] = brush->clr0;
        clrs[2] = brush->clr1;
        clrs[3] = brush->clr0;
        break;
    }

    /* top-right */
    vec3_setf(&v0->pos, (float)item->p1.x, (float)item->p0.y, 0.0f);
    vec2f_setf(&v0->coord, 1.0f, 1.0f);
    v0->clr = clrs[0];

    /* top-left */
    vec3_setf(&v1->pos, (float)item->p0.x, (float)item->p0.y, 0.0f);
    vec2f_setf(&v1->coord, 0.0f, 1.0f);
    v1->clr = clrs[1];

    /* bottom-right */
    vec3_setf(&v2->pos, (float)item->p1.x, (float)item->p1.y, 0.0f);
    vec2f_setf(&v2->coord, 1.0f, 0.0f);
    v2->clr = clrs[2];

    /* bottom-left */
    vec3_setf(&v3->pos, (float)item->p0.x, (float)item->p1.y, 0.0f);
    vec2f_setf(&v3->coord, 0.0f, 0.0f);
    v3->clr = clrs[3];

    *streamed_cnt = 1;
    return TRUE;
}

int canvas_stream_rect_flipy(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt)
{
    struct canvas_vertex2d* v0 = &verts[0];
    struct canvas_vertex2d* v1 = &verts[1];
    struct canvas_vertex2d* v2 = &verts[2];
    struct canvas_vertex2d* v3 = &verts[3];
    struct color clrs[4];
    const struct canvas_brush* brush = &item->brush;
    switch (brush->grad)    {
        case GFX_GRAD_NULL:
        clrs[0] = brush->clr0;
        clrs[1] = brush->clr0;
        clrs[2] = brush->clr0;
        clrs[3] = brush->clr0;
        break;
        case GFX_GRAD_TTB:
        clrs[0] = brush->clr0;
        clrs[1] = brush->clr0;
        clrs[2] = brush->clr1;
        clrs[3] = brush->clr1;
        break;
        case GFX_GRAD_BTT:
        clrs[0] = brush->clr1;
        clrs[1] = brush->clr1;
        clrs[2] = brush->clr0;
        clrs[3] = brush->clr0;
        break;
        case GFX_GRAD_RTL:
        clrs[0] = brush->clr0;
        clrs[1] = brush->clr1;
        clrs[2] = brush->clr0;
        clrs[3] = brush->clr1;
        break;
        case GFX_GRAD_LTR:
        clrs[0] = brush->clr1;
        clrs[1] = brush->clr0;
        clrs[2] = brush->clr1;
        clrs[3] = brush->clr0;
        break;
    }

    /* top-right */
    vec3_setf(&v0->pos, (float)item->p1.x, (float)item->p0.y, 0.0f);
    vec2f_setf(&v0->coord, 1.0f, 0.0f);
    v0->clr = clrs[0];

    /* top-left */
    vec3_setf(&v1->pos, (float)item->p0.x, (float)item->p0.y, 0.0f);
    vec2f_setf(&v1->coord, 0.0f, 0.0f);
    v1->clr = clrs[1];

    /* bottom-right */
    vec3_setf(&v2->pos, (float)item->p1.x, (float)item->p1.y, 0.0f);
    vec2f_setf(&v2->coord, 1.0f, 1.0f);
    v1->clr = clrs[2];

    /* bottom-left */
    vec3_setf(&v3->pos, (float)item->p0.x, (float)item->p1.y, 0.0f);
    vec2f_setf(&v3->coord, 0.0f, 1.0f);
    v1->clr = clrs[3];

    *streamed_cnt = 1;
    return TRUE;
}


int canvas_stream_line(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt)
{
    struct vec2f p0;    vec2f_setf(&p0, (float)item->p0.x, (float)item->p0.y);
    struct vec2f p1;    vec2f_setf(&p1, (float)item->p1.x, (float)item->p1.y);
    if (p0.y > p1.y)    {
        swapf(&p0.x, &p1.x);
        swapf(&p0.y, &p1.y);
    }
    struct vec2f p0p1;
    vec2f_sub(&p0p1, &p1, &p0);

    float alpha = math_acosf(p0p1.x / vec2f_len(&p0p1)) + PI_HALF;
    float hw = (float)(item->width) * 0.5f;

    struct vec2f d;
    struct vec2f p;
    vec2f_setf(&d, hw*cos(alpha), hw*sin(alpha));

    struct canvas_vertex2d* v0 = &verts[0];
    struct canvas_vertex2d* v1 = &verts[1];
    struct canvas_vertex2d* v2 = &verts[2];
    struct canvas_vertex2d* v3 = &verts[3];

    /* top-right */
    vec2f_add(&p, &p0, &d);
    vec3_setf(&v0->pos, p.x, p.y, 0.0f);
    vec2f_setf(&v0->coord, 1.0f, 1.0f);
    color_setc(&v0->clr, &item->c);

    /* top-left */
    vec2f_add(&p, &p1, &d);
    vec3_setf(&v1->pos, p.x, p.y, 0.0f);
    vec2f_setf(&v1->coord, 0.0f, 1.0f);
    color_setc(&v1->clr, &item->c);

    /* bottom-right */
    vec2f_sub(&p, &p0, &d);
    vec3_setf(&v2->pos, p.x, p.y, 0.0f);
    vec2f_setf(&v2->coord, 1.0f, 0.0f);
    color_setc(&v2->clr, &item->c);

    /* bottom-left */
    vec2f_sub(&p, &p1, &d);
    vec3_setf(&v3->pos, p.x, p.y, 0.0f);
    vec2f_setf(&v3->coord, 0.0f, 0.0f);
    color_setc(&v3->clr, &item->c);

    *streamed_cnt = 1;
    return TRUE;
}

int canvas_stream_rectborder(struct canvas_vertex2d* verts, uint quad_cnt,
    const struct canvas_item2d* item, uint* streamed_cnt)
{
    /* draw four lines (quads) around the rectangle */
    struct vec2f pts[8];
    struct vec2f p0;    vec2f_setf(&p0, (float)item->p0.x, (float)item->p0.y);
    struct vec2f p1;    vec2f_setf(&p1, (float)item->p1.x, (float)item->p1.y);

    vec2f_setv(&pts[0], &p0);           vec2f_setf(&pts[1], p1.x, p0.y);    /* edge #1 */
    vec2f_setv(&pts[2], &pts[1]);       vec2f_setv(&pts[3], &p1);           /* edge #2 */
    vec2f_setf(&pts[4], p0.x, p1.y);    vec2f_setv(&pts[5], &pts[3]);       /* edge #3 */
    vec2f_setv(&pts[6], &p0);           vec2f_setv(&pts[7], &pts[4]);       /* edge #4 */

    uint num = mini(quad_cnt, 4);
    float d = (float)item->width * 0.5f;

    for (uint i = 0; i < num; i++)    {
        uint idx = i*2;	/* line index */
        uint qidx = i*4;	/* vertex index (4 for each line) (for each line we have a quad) */
        vec2f_setv(&p0, &pts[idx]);
        vec2f_setv(&p1, &pts[idx+1]);

        struct vec2f dv;
        vec2f_setf(&dv, d*math_sign(p1.y - p0.y), d*math_sign(p1.x - p0.x));

        struct canvas_vertex2d* v0 = &verts[qidx];
        struct canvas_vertex2d* v1 = &verts[qidx + 1];
        struct canvas_vertex2d* v2 = &verts[qidx + 2];
        struct canvas_vertex2d* v3 = &verts[qidx + 3];

        /* top-right */
        vec3_setf(&v0->pos, p1.x + dv.x, p0.y - dv.y, 0.0f);
        vec2f_setf(&v0->coord, 1.0f, 1.0f);
        v0->clr = item->c;

        /* top-left */
        vec3_setf(&v1->pos, p0.x - dv.x, p0.y - dv.y, 0.0f);
        vec2f_setf(&v1->coord, 0.0f, 1.0f);
        v1->clr = item->c;

        /* bottom-right */
        vec3_setf(&v2->pos, p1.x + dv.x, p1.y + dv.y, 0.0f);
        vec2f_setf(&v2->coord, 1.0f, 0.0f);
        v2->clr = item->c;

        /* bottom-left */
        vec3_setf(&v3->pos, p0.x - dv.x, p1.y + dv.y, 0.0f);
        vec2f_setf(&v3->coord, 0.0f, 0.0f);
        v3->clr = item->c;
    }

    *streamed_cnt = num;
    return TRUE;
}


void gfx_canvas_setfont(fonthandle_t hdl)
{
    if (hdl != INVALID_HANDLE)
        g_cvs.font_hdl = hdl;
    else
        g_cvs.font_hdl = g_cvs.def_font_hdl;
}

uint16 gfx_canvas_getfontheight()
{
    return gfx_font_getf(g_cvs.font_hdl)->line_height;
}

void gfx_canvas_setalpha(float alpha)
{
	g_cvs.alpha = alpha;
}

void gfx_canvas_setfillcolor_solid(const struct color* c)
{
    color_setc(&g_cvs.brush.clr0, c);
    color_setc(&g_cvs.brush.clr1, c);
    g_cvs.brush.grad = GFX_GRAD_NULL;
}

void gfx_canvas_setlinecolor(const struct color* c)
{
    color_setc(&g_cvs.line_color, c);
}

void gfx_canvas_settextcolor(const struct color* c)
{
    color_setc(&g_cvs.text_color, c);
}

gfx_buffer canvas_create_solidsphere(uint horz_seg_cnt, uint vert_seg_cnt)
{
    /* in solid sphere we have horozontal segments and vertical segments
     * horizontal  */
    uint horz_cnt = clampi(horz_seg_cnt, 4, 30);
    uint vert_cnt = clampi(vert_seg_cnt, 3, 30);
    if (horz_cnt % 2 != 0)        horz_cnt ++;        /* horozontal must be even number */
    if (vert_cnt % 2 == 0)        vert_cnt ++;        /* vertical must be odd number */

    const uint vertex_cnt = horz_cnt*3*2 + (vert_cnt - 3)*3*2*horz_cnt;

    /* set extreme points (radius = 1.0f) */
    struct vec4f y_max;     vec3_setf(&y_max, 0.0f, 1.0f, 0.0f);
    struct vec4f y_min;     vec3_setf(&y_min, 0.0f, -1.0, 0.0f);

    /* start from lower extreme point and draw slice of circles
     * connect them to the lower level
     * if we are on the last level (i == iter_cnt-1) connect to upper extreme
     * else just make triangles connected to lower level */
    uint iter_cnt = vert_cnt - 1;
    uint idx = 0;
    uint lower_idx = idx;
    uint delta_idx;
    float r;

    /* Phi: vertical angle */
    float delta_phi = PI / (float)iter_cnt;
    float phi = -PI_HALF + delta_phi;

    /* Theta: horizontal angle */
    float delta_theta = PI_2X / (float)horz_cnt;
    float theta = 0.0f;
    float y;

    /* create buffer and fill it with data */
    uint size = vertex_cnt * sizeof(struct canvas_vertex3d);
    struct canvas_vertex3d* verts = (struct canvas_vertex3d*)ALLOC(size, MID_GFX);
    if (verts == NULL)
        return NULL;

    for (uint i = 0; i < iter_cnt; i++)    {
        /* calculate z and slice radius */
        r = cos(phi);
        y = sin(phi);
        phi += delta_phi;

        /* normal drawing (quad between upper level and lower level) */
        if (i != 0 && i != iter_cnt - 1)    {
            theta = 0.0f;
            for (uint k = 0; k < horz_cnt; k++)        {
                /* current level verts */
                vec3_setf(&verts[idx].pos, r*cos(theta), y, r*sin(theta));
                vec3_setf(&verts[idx + 1].pos, r*cos(theta+delta_theta), y, r*sin(theta+delta_theta));
                vec3_setv(&verts[idx + 2].pos, &verts[lower_idx].pos);
                vec3_setv(&verts[idx + 3].pos, &verts[idx + 1].pos);
                vec3_setv(&verts[idx + 4].pos, &verts[lower_idx + 1].pos);
                vec3_setv(&verts[idx + 5].pos, &verts[lower_idx].pos);

                idx += 6;
                theta += delta_theta;
                lower_idx += delta_idx;
            }
            delta_idx = 6;
            continue;
        }

        /* lower cap */
        if (i == 0)    {
            theta = 0.0f;
            lower_idx = idx;
            delta_idx = 3;
            for (uint k = 0; k < horz_cnt; k++)    {
                vec3_setf(&verts[idx].pos, r*cos(theta), y, r*sin(theta));
                vec3_setf(&verts[idx + 1].pos, r*cos(theta+delta_theta), y, r*sin(theta+delta_theta));
                vec3_setv(&verts[idx + 2].pos, &y_min);
                idx += delta_idx;
                theta += delta_theta;
            }
        }

        /* higher cap */
        if (i == iter_cnt - 1)        {
            for (uint k = 0; k < horz_cnt; k++)    {
                vec3_setv(&verts[idx].pos, &y_max);
                vec3_setv(&verts[idx + 1].pos, &verts[lower_idx + 1].pos);
                vec3_setv(&verts[idx + 2].pos, &verts[lower_idx].pos);
                idx += 3;
                theta += delta_theta;
                lower_idx += delta_idx;
            }
        }
    }

    gfx_buffer buf = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC, size, verts, 0);
    FREE(verts);

    return buf;
}


gfx_buffer canvas_create_boundsphere(uint seg_cnt)
{
    seg_cnt = clampi(seg_cnt, 4, 35);
    const uint vertex_cnt = seg_cnt * 2;

    /* create buffer and fill it with data */
    uint size = vertex_cnt * sizeof(struct canvas_vertex3d);
    struct canvas_vertex3d* verts = (struct canvas_vertex3d*)ALLOC(size, MID_GFX);
    if (verts == NULL)
        return NULL;

    const float dt = PI_2X / (float)seg_cnt;
    float theta = 0.0f;
    uint idx = 0;

    /* circle on the XY plane (center = (0, 0, 0), radius = 1) */
    for (uint i = 0; i < seg_cnt; i++)    {
        vec3_setf(&verts[idx].pos, cos(theta), sin(theta), 0.0f);
        vec3_setf(&verts[idx + 1].pos, cos(theta + dt), sin(theta + dt), 0.0f);
        idx += 2;
        theta += dt;
    }

    gfx_buffer buf = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC, size, verts, 0);
    FREE(verts);
    return buf;
}


gfx_buffer canvas_create_prism()
{
    const uint vertex_cnt = 6*3;
    uint size = vertex_cnt * sizeof(struct canvas_vertex3d);

    struct canvas_vertex3d* verts = (struct canvas_vertex3d*)ALLOC(size, MID_GFX);
    if (verts == NULL)
        return NULL;

    const float w = 0.5f;

    /* base */
    vec3_setf(&verts[0].pos, w, 0.0f, -w);
    vec3_setf(&verts[1].pos, w, 0.0f, w);
    vec3_setf(&verts[2].pos, -w, 0.0f, w);

    vec3_setf(&verts[3].pos, -w, 0.0f, w);
    vec3_setf(&verts[4].pos, -w, 0.0f, -w);
    vec3_setf(&verts[5].pos, w, 0.0f, -w);

    /* sides */
    vec3_setf(&verts[6].pos, -w, 0.0f, -w);
    vec3_setf(&verts[7].pos, 0.0f, 1.0f, 0.0f);
    vec3_setf(&verts[8].pos, w, 0.0f, -w);

    vec3_setf(&verts[9].pos, -w, 0.0f, w);
    vec3_setf(&verts[10].pos, 0.0f, 1.0f, 0.0f);
    vec3_setf(&verts[11].pos, -w, 0.0f, -w);

    vec3_setf(&verts[12].pos, w, 0.0f, w);
    vec3_setf(&verts[13].pos, 0.0f, 1.0f, 0.0f);
    vec3_setf(&verts[14].pos, -w, 0.0f, w);

    vec3_setf(&verts[15].pos, w, 0.0f, w);
    vec3_setf(&verts[16].pos, w, 0.0f, -w);
    vec3_setf(&verts[17].pos, 0.0f, 1.0f, 0.0f);

    gfx_buffer buff = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC, size, verts, 0);
    FREE(verts);

    return buff;
}

gfx_buffer canvas_create_solidaabb()
{
    const uint vertex_cnt = 36;
    uint size = vertex_cnt * sizeof(struct canvas_vertex3d);

    struct aabb aabb;
    struct vec4f pts[8];
    aabb_setzero(&aabb);
    aabb_pushptf(&aabb, -0.5f, -0.5f, -0.5f);
    aabb_pushptf(&aabb, 0.5f, 0.5f, 0.5f);
    aabb_getptarr(pts, &aabb);

    struct canvas_vertex3d* verts = (struct canvas_vertex3d*)ALLOC(size, MID_GFX);
    if (verts == NULL)
        return NULL;

    /* z- */
    vec3_setv(&verts[0].pos, &pts[0]);
    vec3_setv(&verts[1].pos, &pts[2]);
    vec3_setv(&verts[2].pos, &pts[3]);
    vec3_setv(&verts[3].pos, &pts[3]);
    vec3_setv(&verts[4].pos, &pts[1]);
    vec3_setv(&verts[5].pos, &pts[0]);
    /* z+ */
    vec3_setv(&verts[6].pos, &pts[5]);
    vec3_setv(&verts[7].pos, &pts[7]);
    vec3_setv(&verts[8].pos, &pts[6]);
    vec3_setv(&verts[9].pos, &pts[6]);
    vec3_setv(&verts[10].pos, &pts[4]);
    vec3_setv(&verts[11].pos, &pts[5]);
    /* x+ */
    vec3_setv(&verts[12].pos, &pts[1]);
    vec3_setv(&verts[13].pos, &pts[3]);
    vec3_setv(&verts[14].pos, &pts[7]);
    vec3_setv(&verts[15].pos, &pts[7]);
    vec3_setv(&verts[16].pos, &pts[5]);
    vec3_setv(&verts[17].pos, &pts[1]);
    /* x- */
    vec3_setv(&verts[18].pos, &pts[6]);
    vec3_setv(&verts[19].pos, &pts[2]);
    vec3_setv(&verts[20].pos, &pts[0]);
    vec3_setv(&verts[21].pos, &pts[0]);
    vec3_setv(&verts[22].pos, &pts[4]);
    vec3_setv(&verts[23].pos, &pts[6]);
    /* y- */
    vec3_setv(&verts[24].pos, &pts[1]);
    vec3_setv(&verts[25].pos, &pts[5]);
    vec3_setv(&verts[26].pos, &pts[4]);
    vec3_setv(&verts[27].pos, &pts[4]);
    vec3_setv(&verts[28].pos, &pts[0]);
    vec3_setv(&verts[29].pos, &pts[1]);
    /* y+ */
    vec3_setv(&verts[30].pos, &pts[3]);
    vec3_setv(&verts[31].pos, &pts[2]);
    vec3_setv(&verts[32].pos, &pts[6]);
    vec3_setv(&verts[33].pos, &pts[6]);
    vec3_setv(&verts[34].pos, &pts[7]);
    vec3_setv(&verts[35].pos, &pts[3]);

    gfx_buffer buf = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC, size, verts, 0);
    FREE(verts);
    return buf;
}

gfx_buffer canvas_create_boundaabb()
{
    const uint vertex_cnt = 24;
    uint size = vertex_cnt * sizeof(struct canvas_vertex3d);

    struct aabb aabb;
    struct vec4f pts[8];
    aabb_setzero(&aabb);
    aabb_pushptf(&aabb, -0.5f, -0.5f, -0.5f);
    aabb_pushptf(&aabb, 0.5f, 0.5f, 0.5f);
    aabb_getptarr(pts, &aabb);

    struct canvas_vertex3d* verts = (struct canvas_vertex3d*)ALLOC(size, MID_GFX);
    if (verts == NULL)
        return NULL;

    /* write edges
     * bottom edges */
    vec3_setv(&verts[0].pos, &pts[0]);    vec3_setv(&verts[1].pos, &pts[1]);
    vec3_setv(&verts[2].pos, &pts[1]);    vec3_setv(&verts[3].pos, &pts[5]);
    vec3_setv(&verts[4].pos, &pts[5]);    vec3_setv(&verts[5].pos, &pts[4]);
    vec3_setv(&verts[6].pos, &pts[4]);    vec3_setv(&verts[7].pos, &pts[0]);

    /* middle edges */
    vec3_setv(&verts[8].pos, &pts[0]);    vec3_setv(&verts[9].pos, &pts[2]);
    vec3_setv(&verts[10].pos, &pts[1]);   vec3_setv(&verts[11].pos, &pts[3]);
    vec3_setv(&verts[12].pos, &pts[5]);   vec3_setv(&verts[13].pos, &pts[7]);
    vec3_setv(&verts[14].pos, &pts[4]);   vec3_setv(&verts[15].pos, &pts[6]);

    /* top edges */
    vec3_setv(&verts[16].pos, &pts[2]);   vec3_setv(&verts[17].pos, &pts[3]);
    vec3_setv(&verts[18].pos, &pts[3]);   vec3_setv(&verts[19].pos, &pts[7]);
    vec3_setv(&verts[20].pos, &pts[7]);   vec3_setv(&verts[21].pos, &pts[6]);
    vec3_setv(&verts[22].pos, &pts[6]);   vec3_setv(&verts[23].pos, &pts[2]);

    gfx_buffer buf = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC, size, verts, 0);
    FREE(verts);
    return buf;
}

gfx_buffer canvas_create_capsule(uint horz_seg_cnt, uint vert_seg_cnt, uint* halfsphere_idx,
                               uint* cyl_idx, uint* cyl_cnt)
{
    /* in solid sphere we have horozontal segments and vertical segments */
    /* horizontal  */
    uint horz_cnt = clampi(horz_seg_cnt, 4, 30);
    uint vert_cnt = clampi(vert_seg_cnt, 3, 30);
    if (horz_cnt % 2 != 0)        horz_cnt ++;        /* horozontal must be even number */
    if (vert_cnt % 2 == 0)        vert_cnt ++;        /* vertical must be odd number */

    const uint vertex_cnt = horz_cnt*3*2 + (vert_cnt - 3)*3*2*horz_cnt + horz_cnt*3*2;

    /* set extreme points (radius = 1.0f) */
    struct vec4f y_max; vec3_setf(&y_max, 1.0f, 0.0f, 0.0f);
    struct vec4f y_min; vec3_setf(&y_min, -1.0f, 0.0, 0.0f);

    /* start from lower extreme point and draw slice of circles
     * connect them to the lower level
     * if we are on the last level (i == iter_cnt-1) connect to upper extreme
     * else just make triangles connected to lower level */
    uint iter_cnt = vert_cnt - 1;
    uint idx = 0;
    uint lower_idx = idx;
    uint delta_idx = 0;
    float r;

    /* Phi: vertical angle */
    float delta_phi = PI / (float)iter_cnt;
    float phi = -PI_HALF + delta_phi;

    /* Theta: horizontal angle */
    float delta_theta = PI_2X / (float)horz_cnt;
    float theta = 0.0f;
    float x;

    /* create buffer and fill it with data */
    uint size = vertex_cnt * sizeof(struct canvas_vertex3d);
    struct canvas_vertex3d* verts = (struct canvas_vertex3d*)ALLOC(size, MID_GFX);
    if (verts == NULL)
        return NULL;

    for (uint i = 0; i < iter_cnt; i++)    {
        /* calculate z and slice radius */
        r = cos(phi);
        x = sin(phi);
        phi += delta_phi;

        /* divide half sphere */
        if (i == (vert_cnt / 2))
            *halfsphere_idx = idx;

        /* normal drawing (quad between upper level and lower level) */
        if (i != 0 && i != iter_cnt - 1)    {
            theta = 0.0f;
            for (uint k = 0; k < horz_cnt; k++)        {
                /* current level verts */
                vec3_setf(&verts[idx].pos, x, r*sin(theta), r*cos(theta));
                vec3_setf(&verts[idx + 1].pos, x, r*sin(theta+delta_theta), r*cos(theta+delta_theta));
                vec3_setv(&verts[idx + 2].pos, &verts[lower_idx].pos);
                vec3_setv(&verts[idx + 3].pos, &verts[idx + 1].pos);
                vec3_setv(&verts[idx + 4].pos, &verts[lower_idx + 1].pos);
                vec3_setv(&verts[idx + 5].pos, &verts[lower_idx].pos);

                idx += 6;
                theta += delta_theta;
                lower_idx += delta_idx;
            }
            delta_idx = 6;
            continue;
        }

        /* lower cap */
        if (i == 0)    {
            theta = 0.0f;
            lower_idx = idx;
            delta_idx = 3;
            for (uint k = 0; k < horz_cnt; k++)    {
                vec3_setf(&verts[idx].pos, x, r*sin(theta), r*cos(theta));
                vec3_setf(&verts[idx + 1].pos, x, r*sin(theta+delta_theta), r*cos(theta+delta_theta));
                vec3_setv(&verts[idx + 2].pos, &y_min);

                idx += delta_idx;
                theta += delta_theta;
            }
        }

        /* higher cap */
        if (i == iter_cnt - 1)        {
            for (uint k = 0; k < horz_cnt; k++)    {
                vec3_setv(&verts[idx].pos, &y_max);
                vec3_setv(&verts[idx + 1].pos, &verts[lower_idx + 1].pos);
                vec3_setv(&verts[idx + 2].pos, &verts[lower_idx].pos);

                idx += 3;
                theta += delta_theta;
                lower_idx += delta_idx;
            }
        }
    }

    /* cylinder */
    *cyl_idx = idx;
    for (uint k = 0; k < horz_cnt; k++)    {
        float z = cos(theta);
        float y = sin(theta);
        float dz = cos(theta + delta_theta);
        float dy = sin(theta + delta_theta);

        vec3_setf(&verts[idx].pos, 1.0f, y, z);
        vec3_setf(&verts[idx + 1].pos, 1.0f, dy, dz);
        vec3_setf(&verts[idx + 2].pos, -1.0f, y, z);
        vec3_setv(&verts[idx + 3].pos, &verts[idx + 1].pos);
        vec3_setf(&verts[idx + 4].pos, -1.0f, dy, dz);
        vec3_setv(&verts[idx + 5].pos, &verts[idx + 2].pos);

        idx += 6;
        theta += delta_theta;
    }
    *cyl_cnt = idx - (*cyl_idx);

    gfx_buffer buf = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC, size, verts, 0);
    FREE(verts);
    return buf;
}

void gfx_canvas_box(const struct aabb* b, const struct mat3f* world)
{
    if (aabb_iszero(b))
        return;

    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;

    uint vertex_cnt =
        g_cvs.shapes.solid_aabb_buff->desc.buff.size / sizeof(struct canvas_vertex3d);

    /* draw AABB scaled by new aabb and applied world matrix */
    struct vec4f center;
    struct mat3f xform;     mat3_setidentity(&xform);

    vec3_add(&center, &b->minpt, &b->maxpt);
    vec3_muls(&center, &center, 0.5);

    float w = b->maxpt.x - b->minpt.x;
    float h = b->maxpt.y - b->minpt.y;
    float d = b->maxpt.z - b->minpt.z;
    mat3_set_scalef(&xform, w, h, d);
    mat3_set_trans(&xform, &center);

    /* multiply by world matrix to combine transformations */
    mat3_mul(&xform, &xform, world);
    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);

    gfx_input_setlayout(cmdqueue, g_cvs.shapes.solid_aabb);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, 0, vertex_cnt, GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_boundaabb(const struct aabb* b, const struct mat4f* viewproj, int show_info)
{
    if (aabb_iszero(b))
        return;

    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint vertex_cnt =
        g_cvs.shapes.bound_aabb_buff->desc.buff.size / sizeof(struct canvas_vertex3d);

    /* draw AABB scaled by new aabb and applied world matrix */
    struct vec4f center;
    struct mat3f xform;     mat3_setidentity(&xform);

    vec3_muls(&center, vec3_add(&center, &b->minpt, &b->maxpt), 0.5f);

    float w = b->maxpt.x - b->minpt.x;
    float h = b->maxpt.y - b->minpt.y;
    float d = b->maxpt.z - b->minpt.z;
    mat3_set_scalef(&xform, w, h, d);
    mat3_set_trans(&xform, &center);

    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.bound_aabb);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_LINELIST, 0, vertex_cnt, GFX_DRAWCALL_DEBUG);

    /* info on 2d canvas (dimensions, center) */
    if (show_info)    {
        struct vec2i center2d;

        if (canvas_transform_toclip(&center2d, &center, viewproj))        {
            char text[64];
            const struct gfx_font* font = gfx_font_getf(g_cvs.font_hdl);

            struct rect2di rc;  rect2di_seti(&rc, center2d.x-3, center2d.y-3, 6, 6);
            gfx_canvas_rect2d(&rc, 0, 0);
            sprintf(text, "(%.1f, %.1f, %.1f)", center.x, center.y, center.z);
            gfx_canvas_text2dpt(text, center2d.x -5, center2d.y + 5, 0);
            sprintf(text, "(w=%.1f, h=%.1f, d=%.1f)", w, h, d);
            gfx_canvas_text2dpt(text, center2d.x -5, center2d.y + 5 + font->line_height, 0);
        }
    }
}


void gfx_canvas_prism(float width, float height, const struct mat3f* world)
{
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint vertex_cnt = g_cvs.shapes.prism_buff->desc.buff.size / sizeof(struct canvas_vertex3d);

    struct mat3f xform;
    mat3_setidentity(&xform);
    mat3_set_scalef(&xform, width, height, width);
    mat3_mul(&xform, &xform, world);

    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.prism);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, 0, vertex_cnt, GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_sphere(const struct sphere* s, const struct mat3f* world,
		enum gfx_sphere_detail detail)
{
	uint level = (uint)detail;
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint vertex_cnt = g_cvs.shapes.solid_spheres_buff[level]->desc.buff.size /
        sizeof(struct canvas_vertex3d);

    /* translate by sphere center, and resize by sphere radius */
    struct mat3f xform;
    mat3_setidentity(&xform);
    mat3_set_scalef(&xform, s->r, s->r, s->r);
    mat3_set_transf(&xform, s->x, s->y, s->z);
    mat3_mul(&xform, &xform, world);

    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.solid_spheres[level]);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, 0, vertex_cnt, GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_capsule(float radius, float half_height, const struct mat3f* world)
{
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;

    /* translate by sphere center, and resize by sphere radius */
    struct mat3f xform;
    mat3_setidentity(&xform);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.capsule);

    /* lower half-sphere */
    mat3_set_scalef(&xform, radius, radius, radius);
    mat3_set_transf(&xform, -(half_height + radius)*0.5f, 0.0f, 0.0f);
    mat3_mul(&xform, &xform, world);
    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, 0, g_cvs.shapes.capsule_halfidx,
    		GFX_DRAWCALL_DEBUG);

    /* upper half-sphere */
    mat3_setidentity(&xform);
    mat3_set_scalef(&xform, radius, radius, radius);
    mat3_set_transf(&xform, (half_height + radius)*0.5f, 0.0f, 0.0f);
    mat3_mul(&xform, &xform, world);
    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, g_cvs.shapes.capsule_halfidx,
        g_cvs.shapes.capsule_halfidx, GFX_DRAWCALL_DEBUG);

    /* cylinder */
    mat3_setidentity(&xform);
    mat3_set_scalef(&xform, half_height, radius, radius);
    mat3_mul(&xform, &xform, world);
    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, g_cvs.shapes.capsule_cylidx,
    		g_cvs.shapes.capsule_cylcnt, GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_boundsphere(const struct sphere* s, const struct mat4f* viewproj,
                            const struct mat3f* view, int show_info)
{
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint vertex_cnt =
        g_cvs.shapes.bound_sphere_buff->desc.buff.size / sizeof(struct canvas_vertex3d);

    /* inverse of rotation part (billboard style) */
    struct mat3f inv_view;
    mat3_setf(&inv_view, view->m11,    view->m21,    view->m31,
                         view->m12,    view->m22,    view->m32,
                         view->m13,    view->m23,    view->m33,
                         0.0f,         0.0f,         0.0f);

    /* translate by sphere center, and resize by sphere radius
     * combine with inverse view transform to render billboard effect */
    struct mat3f xform;
    mat3_setidentity(&xform);
    mat3_set_scalef(&xform, s->r, s->r, s->r);
    mat3_set_transf(&xform, s->x, s->y, s->z);
    mat3_mul(&xform, &inv_view, &xform);

    canvas_set_perobject(cmdqueue, &xform, &g_cvs.brush.clr0, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.bound_sphere);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_LINELIST, 0, vertex_cnt, GFX_DRAWCALL_DEBUG);

    /* 2d stuff
     * draw sphere center as a quad
     * draw two-sided arrow for showing the diagonal line
     * draw sphere info */
    if (show_info)    {
        struct vec4f pt;
        struct vec4f pt_corner1;
        struct vec4f pt_corner2;
        const float pi_over4 = PI/4.0f;
        vec3_setzero(&pt);
        vec3_setf(&pt_corner1, cos(pi_over4), sin(pi_over4), 0.0f);
        vec3_setf(&pt_corner2, cos(PI + pi_over4), sin(PI + pi_over4), 0.0f);
        vec3_transformsrt(&pt, &pt, &xform);
        vec3_transformsrt(&pt_corner1, &pt_corner1, &xform);
        vec3_transformsrt(&pt_corner2, &pt_corner2, &xform);

        struct vec2i pt2d;
        struct vec2i pt2d_corner1;
        struct vec2i pt2d_corner2;
        char text[64];

        /* sphere center */
        if (canvas_transform_toclip(&pt2d, &pt, viewproj))        {
            struct rect2di rc;
            rect2di_seti(&rc, pt2d.x-3, pt2d.y-3, 6, 6);
            gfx_canvas_rect2d(&rc, 0, 0);
            sprintf(text, "(%.1f, %.1f, %.1f)", pt.x, pt.y, pt.z);
            gfx_canvas_text2dpt(text, pt2d.x - 5, pt2d.y + 5, 0);
        }

        /* sphere radius */
        if (canvas_transform_toclip(&pt2d_corner1, &pt_corner1, viewproj) &&
            canvas_transform_toclip(&pt2d_corner2, &pt_corner2, viewproj))
        {
            gfx_canvas_line2d(pt2d_corner2.x, pt2d_corner2.y, pt2d_corner1.x, pt2d_corner1.y, 1);

            /* draw radius arrow of sphere */
            gfx_canvas_arrow2d(&pt2d_corner2, &pt2d_corner1, TRUE, 1, 3.0f);

            sprintf(text, "(r=%.1f)", s->r);
            gfx_canvas_text2dpt(text, pt2d_corner1.x + 5, pt2d_corner1.y - 5, 0);
        }
    }
}

void gfx_canvas_bmp3d(gfx_texture tex, const struct vec4f* pos, int width, int height,
                      const struct mat4f* viewproj)
{
    struct vec2i pos2d;
    if (width == 0)
        width = tex->desc.tex.width;
    if (height == 0)
        height = tex->desc.tex.height;

    if (canvas_transform_toclip(&pos2d, pos, viewproj))    {
        int wh = width / 2;
        int hh = height / 2;
        struct rect2di rc;
        rect2di_seti(&rc, pos2d.x - wh, pos2d.y - hh, width, height);
        gfx_canvas_bmp2d(tex, width, height, &rc, 0);
    }
}

void gfx_canvas_frustum(const struct vec4f frustum_pts[8])
{
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint offset;
    struct canvas_vertex3d* verts;

    /* we have two quads for two planes of frustum_pts, which each have 4 lines
     * and the lines between them
     * so gives us 4*2 + 4 = 12 lines -> 24 verts */
    const uint vertex_cnt = 24;
    const uint size = vertex_cnt * sizeof(struct canvas_vertex3d);

    verts = (struct canvas_vertex3d*)gfx_contbuffer_map(cmdqueue, &g_cvs.contbuffer, size, &offset);
    if (verts == NULL)
    	return;

    /* near quad */
    vec3_setv(&verts[0].pos, &frustum_pts[0]);
    vec3_setv(&verts[1].pos, &frustum_pts[1]);
    vec3_setv(&verts[2].pos, &frustum_pts[1]);
    vec3_setv(&verts[3].pos, &frustum_pts[2]);
    vec3_setv(&verts[4].pos, &frustum_pts[2]);
    vec3_setv(&verts[5].pos, &frustum_pts[3]);
    vec3_setv(&verts[6].pos, &frustum_pts[3]);
    vec3_setv(&verts[7].pos, &frustum_pts[0]);
    /* far quad */
    vec3_setv(&verts[8].pos, &frustum_pts[4]);
    vec3_setv(&verts[9].pos, &frustum_pts[5]);
    vec3_setv(&verts[10].pos, &frustum_pts[5]);
    vec3_setv(&verts[11].pos, &frustum_pts[6]);
    vec3_setv(&verts[12].pos, &frustum_pts[6]);
    vec3_setv(&verts[13].pos, &frustum_pts[7]);
    vec3_setv(&verts[14].pos, &frustum_pts[7]);
    vec3_setv(&verts[15].pos, &frustum_pts[4]);
    /* lines between two quads */
    vec3_setv(&verts[16].pos, &frustum_pts[0]);
    vec3_setv(&verts[17].pos, &frustum_pts[4]);
    vec3_setv(&verts[18].pos, &frustum_pts[1]);
    vec3_setv(&verts[19].pos, &frustum_pts[5]);
    vec3_setv(&verts[20].pos, &frustum_pts[2]);
    vec3_setv(&verts[21].pos, &frustum_pts[6]);
    vec3_setv(&verts[22].pos, &frustum_pts[3]);
    vec3_setv(&verts[23].pos, &frustum_pts[7]);

    gfx_contbuffer_unmap(cmdqueue, &g_cvs.contbuffer);

    struct mat3f ident;
    mat3_setidentity(&ident);
    canvas_set_perobject(cmdqueue, &ident, &g_cvs.line_color, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.generic);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_LINELIST, offset/sizeof(struct canvas_vertex3d),
    		vertex_cnt, GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_text3d(const char* text, const struct vec4f* pos, const struct mat4f* viewproj)
{
    struct vec2i pos2d;
    if (canvas_transform_toclip(&pos2d, pos, viewproj))    {
        gfx_canvas_text2dpt(text, pos2d.x, pos2d.y, 0);
    }
}

void gfx_canvas_text3dmultiline(const char* text, const struct vec4f* pos,
                              const struct mat4f* viewproj)
{
    struct vec2i pos2d;
    if (canvas_transform_toclip(&pos2d, pos, viewproj))    {
        const struct gfx_font* font = gfx_font_getf(g_cvs.font_hdl);
        uint16 lh = font->line_height;

        char mltext[512];
        strcpy(mltext, text);
        char* token = strtok(mltext, "\n");
        while (token)    {
            gfx_canvas_text2dpt(token, pos2d.x, pos2d.y, 0);
            pos2d.y += lh;
            token = strtok(NULL, "\n");
        }
    }
}

void gfx_canvas_coords(const struct mat3f* xform, const struct vec4f* campos, float scale)
{
    float length;

    struct vec4f xaxis;     vec3_setf(&xaxis, xform->m11, xform->m12, xform->m13);
    struct vec4f yaxis;     vec3_setf(&yaxis, xform->m21, xform->m22, xform->m23);
    struct vec4f zaxis;     vec3_setf(&zaxis, xform->m31, xform->m32, xform->m33);
    struct vec4f center;    vec3_setf(&center, xform->m41, xform->m42, xform->m43);

    struct vec4f tmpv;
    length = vec3_len(vec3_sub(&tmpv, &center, campos));
    length *= (0.1f * scale);

    vec3_muls(&xaxis, &xaxis, length);
    vec3_muls(&yaxis, &yaxis, length);
    vec3_muls(&zaxis, &zaxis, length);

    length *= 0.08f;
    struct color prev_color;
    color_setc(&prev_color, &g_cvs.line_color);

    color_setc(&g_cvs.line_color, &g_color_red);
    gfx_canvas_arrow3d(&center, vec3_add(&tmpv, &center, &xaxis), &zaxis, length);
    color_setc(&g_cvs.line_color, &g_color_green);
    gfx_canvas_arrow3d(&center, vec3_add(&tmpv, &center, &yaxis), &zaxis, length);
    color_setc(&g_cvs.line_color, &g_color_blue);
    gfx_canvas_arrow3d(&center, vec3_add(&tmpv, &center, &zaxis), &yaxis, length);

    color_setc(&g_cvs.line_color, &prev_color);
}

void gfx_canvas_arrow3d(const struct vec4f* p0, const struct vec4f* p1,
                        const struct vec4f* plane_n, float width)
{
    if (vec3_isequal(p0, p1))
        return;

    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint offset;
    uint vertex_cnt;
    struct canvas_vertex3d* verts;

    /* each arrow has 3 lines, which will be 6 total vertices */
    vertex_cnt = 6;
    const uint size = vertex_cnt * sizeof(struct canvas_vertex3d);
    verts = (struct canvas_vertex3d*)gfx_contbuffer_map(cmdqueue, &g_cvs.contbuffer, size, &offset);
    if (verts == NULL)
    	return;

    struct vec4f l;
    vec3_sub(&l, p1, p0);
    float length = vec3_len(&l);
    vec3_muls(&l, &l, 1.0f/length);        /* normalize */
    width = clampf(width, EPSILON, length*0.25f);

    /*      ^
     *     /|\
     *    / | \
     *   / l|  \
     *   -------
     *      d
     * l is prependicular to d
     * we want to find d, so we have two equations, length of both d and l is 1.0
     * d and l are prependicular, we use cross product with planeVect to determine d */
    struct vec4f d;
    struct vec4f tmpv;
    vec3_norm(&d, vec3_cross(&d, &l, plane_n));
    vec3_muls(&l, &l, length - width*3.0f);
    vec3_muls(&d, &d, width);

    /* main line */
    vec3_setv(&verts[0].pos, p0);
    vec3_setv(&verts[1].pos, p1);

    /* arrow head (end) */
    vec3_setv(&verts[2].pos, p1);
    vec3_setv(&verts[3].pos, vec3_add(&tmpv, vec3_add(&tmpv, p0, &l), &d));
    vec3_setv(&verts[4].pos, p1);
    vec3_setv(&verts[5].pos, vec3_sub(&tmpv, vec3_add(&tmpv, p0, &l), &d));

    gfx_contbuffer_unmap(cmdqueue, &g_cvs.contbuffer);

    struct mat3f ident;
    mat3_setidentity(&ident);
    canvas_set_perobject(cmdqueue, &ident, &g_cvs.line_color, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.generic);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_LINELIST, offset/sizeof(struct canvas_vertex3d),
    		vertex_cnt, GFX_DRAWCALL_DEBUG);
}


void gfx_canvas_prism_2pts(const struct vec3f* p0, const struct vec3f* p1, float base_width)
{
    if (vec3_isequal(p0, p1))
        return;

    struct vec4f l;
    vec3_sub(&l, p1, p0);
    float length = vec3_len(&l);
    vec3_muls(&l, &l, 1.0f/length);        /* normalize */
    base_width = clampf(base_width, EPSILON, length*0.25f);

    float angle = math_acosf(vec3_dot(&g_vec3_unity, &l));
    struct vec3f axis;
    vec3_cross(&axis, &g_vec3_unity, &l);

    struct quat4f q;
    struct mat3f mat;

    quat_fromaxis(&q, &axis, angle);
    mat3_set_trans_rot(&mat, p0, &q);

    gfx_canvas_prism(base_width, length, &mat);
}


void gfx_canvas_arrow2d(const struct vec2i* p0, const struct vec2i* p1, int twoway,
                        uint line_width, float width)
{
    if (vec2i_isequal(p0, p1))
        return;

    struct vec2f l;
    vec2f_setf(&l, (float)(p1->x - p0->x), (float)(p1->y - p0->y));
    float len = vec2f_len(&l);
    vec2f_muls(&l, &l, 1.0f/len);
    width = clampf(width, EPSILON, len*0.25f);

    /*      ^
     *     /|\
     *    / | \
     *   / l|  \
     *   -------
     *        d
     * l is prependicular to d
     * we want to find d, so we have two equations, length of both d and l is 1.0
     * d and l are prependicular
     * so Xd = -Yl, Yd = Xl */

    struct vec2f d;
    vec2f_setf(&d, -l.y, l.x);
    vec2f_muls(&l, &l, len - width*3.0f);
    vec2f_muls(&d, &d, width);

    struct vec2i li;    vec2i_seti(&li, (int)l.x, (int)l.y);
    struct vec2i di;    vec2i_seti(&di, (int)d.x, (int)d.y);

    /* main line */
    gfx_canvas_line2d(p0->x, p0->y, p1->x, p1->y, line_width);

    /* arrow heads (end) */
    gfx_canvas_line2d(p1->x, p1->y, p0->x + li.x + di.x, p0->y + li.y + di.y, line_width);
    gfx_canvas_line2d(p1->x, p1->y, p0->x + li.x - di.x, p0->y + li.y - di.y, line_width);

    if (twoway)    {
        /* arrow heads (start) */
        gfx_canvas_line2d(p0->x, p0->y, p1->x - li.x + di.x, p1->y - li.y + di.y, line_width);
        gfx_canvas_line2d(p0->x, p0->y, p1->x - li.x - di.x, p1->y - li.y - di.y, line_width);
    }
}

void gfx_canvas_grid(float spacing, float depth_max, const struct camera* cam)
{
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint offset;
    struct canvas_vertex3d* verts;
    spacing = clampf(spacing, 1.0f, 20.0f);

    /* project camera frustum_pts corners onto XZ plane
     * and make an AABB from projected frustum_pts
     * change camera far view */
    struct vec4f corners[8];
    struct mat3f proj_xz;
    struct aabb box;    aabb_setzero(&box);

    mat3_setidentity(&proj_xz);
    float ffar = minf(depth_max, cam->ffar);
    float fnear = -1.0f;
    cam_calc_frustumcorners(cam, corners, &fnear, &ffar);
    mat3_set_proj(&proj_xz, &g_vec3_unity);

    for (uint i = 0; i < 8; i++)    {
        aabb_pushptv(&box, vec3_transformsrt(&corners[i], &corners[i], &proj_xz));
    }

    /* snap grid bounds to 'spacing'
     * example: spacing = 5, snap bounds to -5, 0, 5, ... */
    spacing = ceil(spacing);
    int nspace = (int)spacing;

    vec3_setf(&box.minpt,
        (float)((int)box.minpt.x -((int)box.minpt.x)%nspace),
        0.0f,
        (float)((int)box.minpt.z - ((int)box.minpt.z)%nspace));
    vec3_setf(&box.maxpt,
        (float)((int)box.maxpt.x - ((int)box.maxpt.x)%nspace),
        0.0f,
        (float)((int)box.maxpt.z - ((int)box.maxpt.z)%nspace));

    float w = box.maxpt.x - box.minpt.x;
    float d = box.maxpt.z - box.minpt.z;
    int lines_x_cnt = ((int)w)/nspace + 1;
    int lines_z_cnt = ((int)d)/nspace + 1;
    if (math_iszero(w) || math_iszero(d))
        return;

    /* draw rectangular XZ grid by calculated min and max
     * calculate number of verts needed */
    uint vertex_cnt = ((uint)(lines_x_cnt + lines_z_cnt))*2;
    const uint size = vertex_cnt * sizeof(struct canvas_vertex3d);
    uint idx = 0;

    verts = (struct canvas_vertex3d*)gfx_contbuffer_map(cmdqueue, &g_cvs.contbuffer, size, &offset);
    if (verts == NULL)
    	return;

    for (float zoffset = box.minpt.z; zoffset <= box.maxpt.z; zoffset += spacing, idx += 2)  {
        vec3_setf(&verts[idx].pos, box.minpt.x, 0.0f, zoffset);
        vec3_setf(&verts[idx + 1].pos, box.maxpt.x, 0.0f, zoffset);
    }

    for (float xoffset = box.minpt.x; xoffset <= box.maxpt.x; xoffset += spacing, idx += 2)  {
        vec3_setf(&verts[idx].pos, xoffset, 0.0f, box.minpt.z);
        vec3_setf(&verts[idx + 1].pos, xoffset, 0.0f, box.maxpt.z);
    }

    struct mat3f ident;
    mat3_setidentity(&ident);
    gfx_contbuffer_unmap(cmdqueue, &g_cvs.contbuffer);
    canvas_set_perobject(cmdqueue, &ident, &g_cvs.line_color, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.generic);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_LINELIST, offset/sizeof(struct canvas_vertex3d),
    		vertex_cnt, GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_line3d(const struct vec4f* p0, const struct vec4f* p1)
{
    if (vec3_isequal(p0, p1))
        return;

    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint offset;
    uint vertex_cnt = 2;
    struct canvas_vertex3d* verts;

    /* each arrow has 3 lines, which will be 6 total vertices */
    const uint size = vertex_cnt * sizeof(struct canvas_vertex3d);
    verts = (struct canvas_vertex3d*)gfx_contbuffer_map(cmdqueue, &g_cvs.contbuffer, size, &offset);
    if (verts == NULL)
    	return;

    /* main line */
    struct mat3f ident;
    mat3_setidentity(&ident);

    vec3_setv(&verts[0].pos, p0);
    vec3_setv(&verts[1].pos, p1);
    gfx_contbuffer_unmap(cmdqueue, &g_cvs.contbuffer);
    canvas_set_perobject(cmdqueue, &ident, &g_cvs.line_color, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.generic);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_LINELIST, offset/sizeof(struct canvas_vertex3d),
    		vertex_cnt, GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_light_pt(const struct vec4f* pos, float atten[2])
{
    /* draw two spheres that represent near atten and far atten */
    struct sphere s;
    sphere_setf(&s, pos->x, pos->y, pos->z, atten[0]);

    struct color c;
    struct mat3f ident; mat3_setidentity(&ident);

    c = g_cvs.brush.clr0;
    color_muls(&g_cvs.brush.clr0, &c, 0.7f);
    gfx_canvas_sphere(&s, &ident, GFX_SPHERE_MEDIUM);

    g_cvs.brush.clr0 = c;
    s.r = atten[1];
    gfx_canvas_sphere(&s, &ident, GFX_SPHERE_MEDIUM);
}

void gfx_canvas_light_spot(const struct mat3f* xform, float atten[4])
{
    /* draw an sliced cone, which first slice is near atten, and end slice is far atten */
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    uint offset;
    struct canvas_vertex3d* verts;
    const uint segments = 16;

    /* we have three circles
     * four lines = 8 vertices */
    uint vertex_cnt = segments*2*3 + 8;
    struct vec4f line_pts_start[4];
    struct vec4f line_pts_end[4];
    const uint size = vertex_cnt * sizeof(struct canvas_vertex3d);
    verts = (struct canvas_vertex3d*)gfx_contbuffer_map(cmdqueue, &g_cvs.contbuffer, size, &offset);
    if (verts == NULL)
    	return;

    /* draw 3d circle on z = and prependicular to Z-axis (near slice) */
    float step = (2*PI) / segments;
    float z = atten[0];
    float r = z * tanf(atten[2]);
    for (int i = 0; i < segments; i++)    {
        float angle = ((float)i)*step;
        float next_angle = angle + step;

        float x = r * cos(angle);
        float y = r * sin(angle);
        vec3_setf(&verts[i*2].pos, x, y, z);

        x = r * cos(next_angle);
        y = r * sin(next_angle);
        vec3_setf(&verts[i*2 + 1].pos, x, y, z);

        if (i % 4 == 0)
            vec3_setv(&line_pts_start[i/4], &verts[i*2].pos);
    }

    /* draw 3d circle on z=far */
    z = atten[1];
    r = z * tanf(atten[2]);
    int idx = segments*2;
    for (int i = 0; i < segments; i++)    {
        float angle = ((float)i)*step;
        float next_angle = angle + step;

        float x = r * cos(angle);
        float y = r * sin(angle);
        vec3_setf(&verts[i*2 + idx].pos, x, y, z);

        x = r * cos(next_angle);
        y = r * sin(next_angle);
        vec3_setf(&verts[i*2 + idx + 1].pos, x, y, z);
    }

    /* draw 3d circle on z=far and prependicular to Z-axis (far slice) */
    z = atten[1];
    r = z * tanf(atten[3]);
    idx += segments*2;
    for (int i = 0; i < segments; i++)    {
        float angle = ((float)i)*step;
        float next_angle = angle + step;

        float x = r * cosf(angle);
        float y = r * sinf(angle);
        vec3_setf(&verts[i*2 + idx].pos, x, y, z);

        x = r * cos(next_angle);
        y = r * sin(next_angle);
        vec3_setf(&verts[i*2 + idx + 1].pos, x, y, z);

        if (i % 4 == 0)
            vec3_setv(&line_pts_end[i/4], &verts[i*2 + idx].pos);
    }

    /* draw four lines that connect two circles */
    idx += segments*2;
    for (int i = 0; i < 4; i++)    {
        vec3_setv(&verts[idx + i*2].pos, &line_pts_start[i]);
        vec3_setv(&verts[idx + i*2 + 1].pos, &line_pts_end[i]);
    }

    gfx_contbuffer_unmap(cmdqueue, &g_cvs.contbuffer);

    /* draw */
    canvas_set_perobject(cmdqueue, xform, &g_cvs.brush.clr0, NULL);
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.generic);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_LINELIST, offset/sizeof(struct canvas_vertex3d), vertex_cnt,
    		GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_worldbounds(const struct vec3f* minpt, const struct vec3f* maxpt, float height)
{
    gfx_cmdqueue cmdqueue = g_cvs.cmdqueue;
    struct aabb box;
    uint offset;
    uint size = 24*sizeof(struct canvas_vertex3d);

    struct vec3f box_verts[8];
    aabb_setv(&box, minpt, maxpt);
    aabb_getptarr(box_verts, &box);

    /* we need texture rendering */
    canvas_3d_switchtexture();

    struct canvas_vertex3d* verts = (struct canvas_vertex3d*)
        gfx_contbuffer_map(cmdqueue, &g_cvs.contbuffer, size, &offset);
    if (verts == NULL)
        return;

    float width = box.maxpt.x - box.minpt.x;
    float depth = box.maxpt.z - box.minpt.z;
    float tile_w = maxf(1.0f, width/100.0f);
    float tile_d = maxf(1.0f, depth/100.0f);

    /* side #1 */
    vec3_setv(&verts[0].pos, &box_verts[0]);        vec2f_setf(&verts[0].coord, 0.0f, 2.0f);
    vec3_setv(&verts[1].pos, &box_verts[2]);        vec2f_setf(&verts[1].coord, 0.0f, 0.0f);
    vec3_setv(&verts[2].pos, &box_verts[1]);        vec2f_setf(&verts[2].coord, tile_w, 2.0f);
    vec3_setv(&verts[3].pos, &box_verts[1]);        vec2f_setf(&verts[3].coord, tile_w, 2.0f);
    vec3_setv(&verts[4].pos, &box_verts[2]);        vec2f_setf(&verts[4].coord, 0.0f, 0.0f);
    vec3_setv(&verts[5].pos, &box_verts[3]);        vec2f_setf(&verts[5].coord, tile_w, 0.0f);

    /* side #2 */
    vec3_setv(&verts[6].pos, &box_verts[1]);        vec2f_setf(&verts[6].coord, 0.0f, 2.0f);
    vec3_setv(&verts[7].pos, &box_verts[3]);        vec2f_setf(&verts[7].coord, 0.0f, 0.0f);
    vec3_setv(&verts[8].pos, &box_verts[5]);        vec2f_setf(&verts[8].coord, tile_d, 2.0f);
    vec3_setv(&verts[9].pos, &box_verts[5]);        vec2f_setf(&verts[9].coord, tile_d, 2.0f);
    vec3_setv(&verts[10].pos, &box_verts[3]);       vec2f_setf(&verts[10].coord, 0.0f, 0.0f);
    vec3_setv(&verts[11].pos, &box_verts[7]);       vec2f_setf(&verts[11].coord, tile_d, 0.0f);

    /* side #3 */
    vec3_setv(&verts[12].pos, &box_verts[7]);       vec2f_setf(&verts[12].coord, 0.0f, 0.0f);
    vec3_setv(&verts[13].pos, &box_verts[6]);       vec2f_setf(&verts[13].coord, tile_w, 0.0f);
    vec3_setv(&verts[14].pos, &box_verts[4]);       vec2f_setf(&verts[14].coord, tile_w, 2.0f);
    vec3_setv(&verts[15].pos, &box_verts[4]);       vec2f_setf(&verts[15].coord, tile_w, 2.0f);
    vec3_setv(&verts[16].pos, &box_verts[5]);       vec2f_setf(&verts[16].coord, 0.0f, 2.0f);
    vec3_setv(&verts[17].pos, &box_verts[7]);       vec2f_setf(&verts[17].coord, 0.0f, 0.0f);

    /* side #4 */
    vec3_setv(&verts[18].pos, &box_verts[4]);       vec2f_setf(&verts[18].coord, 0.0f, 2.0f);
    vec3_setv(&verts[19].pos, &box_verts[6]);       vec2f_setf(&verts[19].coord, 0.0f, 0.0f);
    vec3_setv(&verts[20].pos, &box_verts[2]);       vec2f_setf(&verts[20].coord, tile_d, 0.0f);
    vec3_setv(&verts[21].pos, &box_verts[2]);       vec2f_setf(&verts[21].coord, tile_d, 0.0f);
    vec3_setv(&verts[22].pos, &box_verts[0]);       vec2f_setf(&verts[22].coord, tile_d, 2.0f);
    vec3_setv(&verts[23].pos, &box_verts[4]);       vec2f_setf(&verts[23].coord, 0.0f, 2.0f);

    gfx_contbuffer_unmap(cmdqueue, &g_cvs.contbuffer);

    struct mat3f m;
    gfx_canvas_setwireframe(FALSE, FALSE);
    canvas_set_perobject(cmdqueue, mat3_setidentity(&m), &g_color_yellow,
        rs_get_texture(g_cvs.bounds_tex));
    gfx_input_setlayout(cmdqueue, g_cvs.shapes.generic);
    gfx_draw(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, offset/sizeof(struct canvas_vertex3d), 24,
        GFX_DRAWCALL_DEBUG);

    canvas_3d_switchnormal();
}

void gfx_canvas_geo(const struct gfx_model_geo* geo, const struct mat3f* world)
{
	canvas_set_perobject(g_cvs.cmdqueue, world, &g_cvs.brush.clr0, NULL);
    gfx_input_setlayout(g_cvs.cmdqueue, geo->inputlayout);
    gfx_draw_indexed(g_cvs.cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, 0, geo->tri_cnt*3, geo->ib_type,
    		GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_georaw(gfx_inputlayout il, const struct mat3f* world, const struct color* clr,
    uint tri_cnt, enum gfx_index_type ib_type)
{
    canvas_set_perobject(g_cvs.cmdqueue, world, clr, NULL);
    gfx_input_setlayout(g_cvs.cmdqueue, il);
    gfx_draw_indexed(g_cvs.cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, 0, tri_cnt*3, ib_type,
        GFX_DRAWCALL_DEBUG);
}

void gfx_canvas_cam(const struct camera* cam, const struct vec4f* activecam_pos,
		const struct mat4f* viewproj, int show_info)
{
    struct vec4f frustum[8];
    cam_calc_frustumcorners(cam, frustum, NULL, NULL);
    gfx_canvas_frustum(frustum);

    /* axis */
    struct mat3f m;
    mat3_setf(&m,
    		cam->right.x, cam->right.y, cam->right.z,
    		cam->up.x, cam->up.y, cam->up.z,
    		cam->look.x, cam->look.y, cam->look.z,
    		cam->pos.x, cam->pos.y, cam->pos.z);
    gfx_canvas_coords(&m, activecam_pos, 1.0f);

    struct vec2i pos2d;
    if (show_info && canvas_transform_toclip(&pos2d, &cam->pos, viewproj))	{
    	char text[64];
    	struct rect2di rc;
    	rect2di_seti(&rc, pos2d.x-3, pos2d.y-3, 6, 6);

    	sprintf(text, "cam(%.1f, %.1f, %.1f)", cam->pos.x, cam->pos.y, cam->pos.z);
    	gfx_canvas_rect2d(&rc, 0, 0);
    	gfx_canvas_text2dpt(text, pos2d.x - 5, pos2d.y + 5, 0);
    }
}

void canvas_set_perobject(gfx_cmdqueue cmdqueue, const struct mat3f* m, const struct color* c,
    gfx_texture tex)
{
    struct color clr;

    color_setf(&clr, c->r, c->g, c->b, g_cvs.alpha);
    gfx_shader_set3m(g_cvs.shader3d, SHADER_NAME(c_world), m);
    gfx_shader_set4f(g_cvs.shader3d, SHADER_NAME(c_color), clr.f);
    gfx_shader_bindconstants(cmdqueue, g_cvs.shader3d);

    if (tex != NULL)    {
        gfx_shader_bindsamplertexture(cmdqueue, g_cvs.shader3d, SHADER_NAME(s_tex),
            g_cvs.states.sampl_lin, tex);
    }
}

int canvas_transform_toclip(struct vec2i* r, const struct vec4f* v, const struct mat4f* viewproj)
{
    const float wh = g_cvs.rt_width*0.5f;
    const float hh = g_cvs.rt_height*0.5f;
    struct vec4f v4;
    vec4_setf(&v4, v->x, v->y, v->z, 1.0f);
    vec4_transform(&v4, &v4, viewproj);     /* to projection space */
    vec4_muls(&v4, &v4, 1.0f/v4.w);         /* normalize */
    r->x = (int)(v4.x*wh + wh + 0.5f);    /* to clip-space */
    r->y = (int)(-v4.y*hh + hh + 0.5f);

    /* z cull */
    if (v4.z < 0.0f || v4.z > 1.0f)
        return FALSE;

    return TRUE;
}

void gfx_canvas_begin3d(gfx_cmdqueue cmdqueue, float rt_width, float rt_height,
                        const struct mat4f* viewproj)
{
    g_cvs.rt_width = rt_width;
    g_cvs.rt_height = rt_height;
    g_cvs.cmdqueue = cmdqueue;

    /* set shader */
    g_cvs.shader3d = gfx_shader_get(g_cvs.shader3d_id);

    gfx_shader_bind(cmdqueue, g_cvs.shader3d);
    gfx_shader_set4m(g_cvs.shader3d, SHADER_NAME(c_viewproj), viewproj);
    gfx_shader_bindconstants(cmdqueue, g_cvs.shader3d);


    /* set default states */
    /* DEFAULT STATE:
     * alpha-blending
     * Z-test
     * solid render
     * CULL: CCW
     */
    gfx_output_setblendstate(cmdqueue, g_cvs.states.blend_alpha, NULL);
    gfx_output_setdepthstencilstate(cmdqueue, g_cvs.states.ds_depthon, 0);
    gfx_output_setrasterstate(cmdqueue, g_cvs.states.rs_solid);
    mat4_setm(&g_cvs.viewproj, viewproj);
}

void canvas_3d_switchtexture()
{
    g_cvs.shader3d = gfx_shader_get(g_cvs.shader3dtex_id);
    gfx_shader_bind(g_cvs.cmdqueue, g_cvs.shader3d);
    gfx_shader_set4m(g_cvs.shader3d, SHADER_NAME(c_viewproj), &g_cvs.viewproj);
    gfx_shader_bindconstants(g_cvs.cmdqueue, g_cvs.shader3d);
}

void canvas_3d_switchnormal()
{
    g_cvs.shader3d = gfx_shader_get(g_cvs.shader3d_id);
    gfx_shader_bind(g_cvs.cmdqueue, g_cvs.shader3d);
    gfx_shader_set4m(g_cvs.shader3d, SHADER_NAME(c_viewproj), &g_cvs.viewproj);
    gfx_shader_bindconstants(g_cvs.cmdqueue, g_cvs.shader3d);
}

void gfx_canvas_end3d()
{
    gfx_output_setrasterstate(g_cvs.cmdqueue, NULL);
    gfx_output_setdepthstencilstate(g_cvs.cmdqueue, NULL, 0);
    gfx_output_setblendstate(g_cvs.cmdqueue, NULL, NULL);
}

void gfx_canvas_setwireframe(int enable, int cull)
{
    if (enable)
        gfx_output_setrasterstate(g_cvs.cmdqueue,
            cull ? g_cvs.states.rs_wire : g_cvs.states.rs_wire_nocull);
    else
        gfx_output_setrasterstate(g_cvs.cmdqueue,
            cull ? g_cvs.states.rs_solid : g_cvs.states.rs_solid_nocull);

    g_cvs.wireframe = enable;
}

void gfx_canvas_setztest(int enable)
{
	gfx_output_setdepthstencilstate(g_cvs.cmdqueue,
			enable ? g_cvs.states.ds_depthon : g_cvs.states.ds_depthoff, 0);
}

void gfx_canvas_setclip2d(int enable, int x, int y, int w, int h)
{
	g_cvs.clip_enable = enable;
	g_cvs.clip_rc.x = x;
	g_cvs.clip_rc.y = y;
	g_cvs.clip_rc.w = w;
	g_cvs.clip_rc.h = h;
}

