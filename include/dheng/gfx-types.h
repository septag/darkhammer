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


#ifndef __GFXTYPES_H__
#define __GFXTYPES_H__

#include "dhcore/types.h"
#include "dhcore/vec-math.h"

/* fwd */
struct camera;

/* include mostly enums that their values may be different in each API */
#if defined(_D3D_)
#include "d3d/gfx-types-d3d.h"
#elif defined(_GL_)
#include "gl/gfx-types-gl.h"
#endif

#include "gfx-input-types.h"

#define GFX_MAX_CBUFFERS 8
#define GFX_MAX_SAMPLERS 16
#define GFX_MAX_SHADERRESOURCES 16
#define GFX_MAX_RENDERTARGETS 8
#define GFX_MAX_VIEWPORTS 8
#define GFX_MAX_INPUTS 16
#define GFX_PROGRAM_MAX_SHADERS 3
#define GFX_RT_MAX_TEXTURES	4
#define GFX_DRAWCALL_GROUP_COUNT 12

/* render passes
 * note that their order is important
 */
#define GFX_RENDERPASS_SUNSHADOW 0 /* sun shadow render-path used for shadow casting objects */
#define GFX_RENDERPASS_SPOTSHADOW 1
#define GFX_RENDERPASS_POINTSHADOW 2
#define GFX_RENDERPASS_MIRROR 3 /* mirror render-path used for water and other reflection effects */
#define GFX_RENDERPASS_PRIMARY 4 /* Primary render-path that is used for every drawable object */
#define GFX_RENDERPASS_TRANSPARENT 5 /* transparent objects should use previous z-buffer and z-ordering*/
#define GFX_RENDERPASS_MAX 6

#define GFX_INPUT_OFFSET_PACKED INVALID_INDEX

/* each drawable object can have multiple passes assigned */
struct gfx_renderpass_item
{
	const struct gfx_rpath* rpath;
	uint shader_id;
};

/* render override callback function */
/* shader flags are used for picking the right shader for each object */
enum gfx_rpath_flag
{
	GFX_RPATH_DIFFUSEMAP = (1<<0),	/* has diffuse-map */
	GFX_RPATH_NORMALMAP = (1<<1),	/* has normal-map */
	GFX_RPATH_REFLECTIONMAP = (1<<3), /* has reflection-map */
	GFX_RPATH_EMISSIVEMAP = (1<<4),	/* has emissive-map */
	GFX_RPATH_GLOSSMAP = (1<<5),	/* has gloss-map */
	GFX_RPATH_SPECULAR = (1<<6),	/* has specular-map */
	GFX_RPATH_ALPHABLEND = (1<<7),	/* transparent */
	GFX_RPATH_CSMSHADOW = (1<<8),	/* is sun-shadow object */
	GFX_RPATH_SKINNED = (1<<9),	/* is skinned object (skeletal) */
	GFX_RPATH_SPOTSHADOW = (1<<10), /* is spot-shadow object */
	GFX_RPATH_POINTSHADOW = (1<<11), /* is point-shadow object */
	GFX_RPATH_ISOTROPIC = (1<<12), /* has isotropic BRDF */
	GFX_RPATH_SSS = (1<<13), /* has sub-surface-scattering BRDF */
	GFX_RPATH_WATER = (1<<14),	/* is water */
	GFX_RPATH_MIRROR = (1<<15), /* is mirror */
	GFX_RPATH_PARTICLE = (1<<16), /* is particle */
	GFX_RPATH_RAW = (1<<17),	/* most basic primitive rendering */
    GFX_RPATH_ALPHAMAP = (1<<18), /* we have alpha test */
	GFX_RPATH_ALL = 0xffffffff
};

enum gfx_obj_type
{
    GFX_OBJ_NULL = 0,
    GFX_OBJ_PROGRAM, /* program contains all shaders (vertex/pixel/geo/..) */
    GFX_OBJ_DEPTHSTENCILSTATE,
    GFX_OBJ_RASTERSTATE,
    GFX_OBJ_BLENDSTATE,
    GFX_OBJ_SAMPLER,
    GFX_OBJ_BUFFER,	/* constant-blocks, texture-buffers, vertex-buffers, etc. */
    GFX_OBJ_TEXTURE, /* textures that are loaded with image data and used in shaders */
    GFX_OBJ_INPUTLAYOUT,
    GFX_OBJ_RENDERTARGET
};

enum gfx_shader_type
{
    GFX_SHADER_NONE = 0,
    GFX_SHADER_VERTEX = 1,
    GFX_SHADER_PIXEL = 2,
    GFX_SHADER_GEOMETRY = 3,
    GFX_SHADER_HULL = 4,
    GFX_SHADER_DOMAIN = 5
};

enum gfx_gpu_vendor
{
    GFX_GPU_UNKNOWN = 0,
    GFX_GPU_NVIDIA,
    GFX_GPU_ATI,
    GFX_GPU_INTEL
};

enum gfx_drawcall_type
{
    GFX_DRAWCALL_GBUFFER = 0,
    GFX_DRAWCALL_LIGHTING = 1,
    GFX_DRAWCALL_MTL = 2,
    GFX_DRAWCALL_2D = 3,
    GFX_DRAWCALL_DEBUG = 4,
    GFX_DRAWCALL_POSTFX = 5,
    GFX_DRAWCALL_MISC = 6,
    GFX_DRAWCALL_PARTICLES = 7,
    GFX_DRAWCALL_DECALS = 8,
    GFX_DRAWCALL_FWD = 9,
    GFX_DRAWCALL_SUNSHADOW = 10,
    GFX_DRAWCALL_LOCALSHADOW = 11,
};

/* works for D3D11 spec too */
enum gfx_color_write
{
    GFX_COLORWRITE_RED         = 0x1,
    GFX_COLORWRITE_GREEN       = 0x2,
    GFX_COLORWRITE_BLUE        = 0x4,
    GFX_COLORWRITE_ALPHA       = 0x8,
    GFX_COLORWRITE_ALL         = (GFX_COLORWRITE_RED | GFX_COLORWRITE_GREEN |
                                  GFX_COLORWRITE_BLUE | GFX_COLORWRITE_ALPHA),
    GFX_COLORWRITE_NONE        = 0x0,
    GFX_COLORWRITE_UNKNOWN     = 0xff
};

/*************************************************************************************************
 * SHARED STRUCTURES
 */
struct gfx_gpu_memstats
{
    uint texture_cnt;
    uint rttexture_cnt;
    uint buffer_cnt;

    size_t textures;
    size_t rt_textures;
    size_t buffers;
};

struct gfx_framestats
{
    uint draw_cnt;
    uint prims_cnt;
    uint draw_group_cnt[GFX_DRAWCALL_GROUP_COUNT];
    uint draw_prim_cnt[GFX_DRAWCALL_GROUP_COUNT];
    uint texchange_cnt;
    uint samplerchange_cnt;
    uint shaderchange_cnt;
    uint map_cnt;
    uint rtchange_cnt;
    uint blendstatechange_cnt;
    uint rsstatechange_cnt;
    uint dsstatechange_cnt;
    uint cbufferchange_cnt;
    uint clearrt_cnt;
    uint cleards_cnt;
    uint input_cnt;
};

struct gfx_subresource_data
{
	void* p;
	uint size;
	uint pitch_row;
	uint pitch_slice;
};

struct gfx_shader_define
{
	const char* name;
	const char* value;
};

struct gfx_blend_desc
{
    int enable;
    enum gfx_blend_mode src_blend;
    enum gfx_blend_mode dest_blend;
    enum gfx_blend_op color_op;
    uint8 write_mask;	/* enum gfx_color_write combination */
};


struct gfx_stencilop_desc
{
    enum gfx_stencil_op fail_op;
    enum gfx_stencil_op depthfail_op;
    enum gfx_stencil_op pass_op;
    enum gfx_cmp_func cmp_func;
};

struct gfx_depthstencil_desc
{
    int depth_enable;
    int depth_write;
    enum gfx_cmp_func depth_func;

    int stencil_enable;
    uint stencil_mask;
    struct gfx_stencilop_desc stencil_frontface_desc;
    struct gfx_stencilop_desc stencil_backface_desc;
};

struct gfx_threading_support
{
    int concurrent_create;
    int concurrent_cmdlist;
};

struct gfx_device_info
{
    enum gfx_gpu_vendor vendor;
    char desc[256];
    int mem_avail;	/* in kb */
    struct gfx_threading_support threading;
    uint fmt_support;
};

struct gfx_input_element
{
	enum gfx_input_element_id id;
	enum gfx_input_element_fmt fmt;	/* format of single component */
	int component_cnt;	/* number of components in element */
	int stride;	/* can be 0 if it's packed */
};

struct gfx_input_element_binding
{
	enum gfx_input_element_id id;
	const char* var_name;	/* variable name in the shader */
    uint vb_idx;  /* which vertex-buffer it belongs to */
    uint elem_offset; /* offset inside parent structure, this can be GFX_INPUT_OFFSET_PACKED */
};

#define GFX_INPUT_GETCNT(inputs) sizeof(inputs)/sizeof(struct gfx_input_element_binding)

struct gfx_viewport
{
    float x;
    float y;
    float w;
    float h;
    float depth_min;
    float depth_max;
};

struct gfx_box
{
    uint left;
    uint top;
    uint front;
    uint right;
    uint bottom;
    uint back;
};

struct gfx_sampler_desc
{
    enum gfx_filter_mode filter_min;
    enum gfx_filter_mode filter_mag;
    enum gfx_filter_mode filter_mip;
    enum gfx_address_mode address_u;
    enum gfx_address_mode address_v;
    enum gfx_address_mode address_w;
    uint aniso_max;
    enum gfx_cmp_func cmp_func;
    float border_color[4];
    int lod_min;
    int lod_max;
};

struct gfx_rasterizer_desc
{
    enum gfx_fill_mode fill;
    enum gfx_cull_mode cull;
    float depth_bias;
    float slopescaled_depthbias;
    int scissor_test;
    int depth_clip;
};

struct gfx_msaa_desc
{
    uint count;
    uint quality;
};

struct gfx_displaytarget_desc
{
    uint width;
    uint height;
    uint refresh_rate;
    int vsync;
    int fullscreen;
    int depthstencil;

    void* hwnd;

#if defined(_LINUX_)
    void* display; /* Display struct */
    void* vi; /* XVisualInfo struct */
#endif
};

struct gfx_shader_binary_data
{
#if defined(_GL_)
	void* prog_data;
	uint prog_size;
	uint fmt;
#elif defined(_D3D_)
	void* vs_data;
	uint vs_size;
	void* ps_data;
	uint ps_size;
	void* gs_data;
	uint gs_size;
#endif
};

struct gfx_shader_data
{
	void* vs_source;
	void* ps_source;
	void* gs_source;
	size_t vs_size;
	size_t ps_size;
	size_t gs_size;
};

/**
 * render view parameters, it is passed to many rendering functions for rendering
 * @ingroup gfx
 */
struct ALIGN16 gfx_view_params
{
    int width;    /**< width of the current render target */
    int height;   /**< height of the current render target */

    struct camera* cam; /**< current view camera @see camera */
    struct mat3f view; /**< current view matrix (calculated from camera) */
    struct mat4f proj; /**< current projection matrix (calculated from camera) */
    struct mat4f viewproj; /**< view(x)projection matrix */
    struct vec4f cam_pos; /**< camera position */
    struct vec4f projparams; /**< projection matrix parameters () */
};

/* brief/useful object description for gfx objects */
struct gfx_obj_desc
{
    union   {
    	/* programs */
        struct	{
        	uptr_t shaders[GFX_PROGRAM_MAX_SHADERS];
        	enum gfx_shader_type shader_types[GFX_PROGRAM_MAX_SHADERS];
        	uint shader_cnt;
            void* d3d_reflects[GFX_PROGRAM_MAX_SHADERS];
        	void* d3d_il;	/* input-layout object for d3d */
        } prog;

        /* textures */
        struct  {
        	enum gfx_texture_type type;
            uint width;
            uint height;
            uint depth;	/* .. same as array count (if not=1 then we have array/3d tex) */
            enum gfx_format fmt;
            int has_alpha;
            void* d3d_srv;
            void* d3d_rtv;
            uint size;
            int is_rt;
            uint mip_cnt;
            int gl_type;
            int gl_fmt;
        } tex;

        /* buffers */
        struct	{
        	enum gfx_buffer_type type;
            void* d3d_srv;  /* d3d tbuffers */
            uint gl_tbuff; /* gl tbuffer texture */
            uint alignment;   /* buffer mapping alignment */
        	uint size;
        } buff;

        /* input-layouts */
        struct	{
        	void* vbuffs[GFX_INPUTELEMENT_ID_CNT];	/* type = gfx_obj. any element can be NULL! */
            uint strides[GFX_INPUTELEMENT_ID_CNT];
            uint vbuff_cnt;
        	void* ibuff;	/* type = gfx_obj */
            enum gfx_index_type idxfmt;
        } il;

        /* render targets */
        struct	{
        	uint rt_cnt;
        	void* rt_textures[GFX_RT_MAX_TEXTURES]; /* type = gfx_obj */
        	void* ds_texture;	/* type = gfx_obj */
        	uint width;
        	uint height;
        } rt;

        struct gfx_blend_desc blend;
        struct gfx_rasterizer_desc raster;
        struct gfx_depthstencil_desc ds;
    };
};

struct gfx_obj_data
{
    uptr_t api_obj;
    enum gfx_obj_type type;
    struct gfx_obj_desc desc;
};

/* fwd */
struct gfx_cmdqueue_s;
struct gfx_displaytarget_s;

typedef struct gfx_cmdqueue_s* gfx_cmdqueue;

/* object typedefs (for readability) */
typedef struct gfx_obj_data* gfx_program;
typedef struct gfx_obj_data* gfx_depthstencilstate;
typedef struct gfx_obj_data* gfx_rasterstate;
typedef struct gfx_obj_data* gfx_blendstate;
typedef struct gfx_obj_data* gfx_sampler;
typedef struct gfx_obj_data* gfx_buffer;
typedef struct gfx_obj_data* gfx_texture;
typedef struct gfx_obj_data* gfx_inputlayout;
typedef struct gfx_obj_data* gfx_rendertarget;
typedef void* gfx_syncobj;

/* gfx features that may be varied to different devices */
enum gfx_feature
{
    GFX_FEATURE_RANGED_CBUFFERS,
    GFX_FEATURE_THREADED_CREATES
};

struct gfx_input_vbuff_desc
{
    size_t stride;
    gfx_buffer vbuff;
};

#define GFX_INPUTVB_GETCNT(vbuffs) sizeof(vbuffs)/sizeof(struct gfx_input_vbuff_desc)

#endif /* __GFXTYPES_H__ */
