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


#ifndef __GFXDEVICE_H__
#define __GFXDEVICE_H__

#include "dhcore/types.h"
#include "gfx-types.h"
#include "init-params.h"

/* TODO: remove un-used api */
#include "engine-api.h"

#define GFX_DESTROY_DEVOBJ(destroy_func, obj)	\
	if (obj != NULL)	{	\
		destroy_func(obj);	\
		obj = NULL;	\
	}

/* fwd */
struct gfx_cblock;

/* functions */
_EXTERN_BEGIN_

void gfx_zerodev();
result_t gfx_initdev(const struct gfx_params* params);
void gfx_releasedev();

/* create/destroy device objects */
gfx_inputlayout gfx_create_inputlayout(const struct gfx_input_vbuff_desc* vbuffs, uint vbuff_cnt,
                                       const struct gfx_input_element_binding* inputs,
                                       uint input_cnt, OPTIONAL gfx_buffer idxbuffer,
                                       OPTIONAL enum gfx_index_type itype, uint thread_id);

void gfx_destroy_inputlayout(gfx_inputlayout input_layout);

gfx_program gfx_create_program(const struct gfx_shader_data* source_data,
		const struct gfx_input_element_binding* bindings, uint binding_cnt,
		const struct gfx_shader_define* defines, uint define_cnt,
		struct gfx_shader_binary_data* bin_data);
gfx_program gfx_create_program_bin(const struct gfx_program_bin_desc* bindesc);
void gfx_destroy_program(gfx_program prog);

gfx_buffer gfx_create_buffer(enum gfx_buffer_type type, enum gfx_mem_hint memhint,
		uint size, const void* data, uint thread_id);
void gfx_destroy_buffer(gfx_buffer buff);

gfx_rendertarget gfx_create_rendertarget(gfx_texture* rt_textures, uint rt_cnt,
		OPTIONAL gfx_texture ds_texture);
void gfx_destroy_rendertarget(gfx_rendertarget rt);

const struct gfx_sampler_desc* gfx_get_defaultsampler();
const struct gfx_blend_desc* gfx_get_defaultblend();
const struct gfx_rasterizer_desc* gfx_get_defaultraster();
const struct gfx_depthstencil_desc* gfx_get_defaultdepthstencil();

gfx_sampler gfx_create_sampler(const struct gfx_sampler_desc* desc);
void gfx_destroy_sampler(gfx_sampler sampler);

gfx_blendstate gfx_create_blendstate(const struct gfx_blend_desc* blend);
void gfx_destroy_blendstate(gfx_blendstate blend);

gfx_rasterstate gfx_create_rasterstate(const struct gfx_rasterizer_desc* raster);
void gfx_destroy_rasterstate(gfx_rasterstate raster);

gfx_depthstencilstate gfx_create_depthstencilstate(const struct gfx_depthstencil_desc* ds);
void gfx_destroy_depthstencilstate(gfx_depthstencilstate ds);

/*
 * data should be an array of subresource_data, layout of data array is not this:
 * (tex-array0)[mip0, mip1, ...] - (tex-array1)[mip0, mip1, mip2]
 *             data[0], data[1], ...            data[n], data[n+1], data[n+2], ...
 */
gfx_texture gfx_create_texture(enum gfx_texture_type type, uint width, uint height,
		uint depth, enum gfx_format fmt, uint mip_cnt, uint array_size, uint total_size,
		const struct gfx_subresource_data* data, enum gfx_mem_hint memhint, uint thread_id);
gfx_texture gfx_create_texturert(uint width, uint height, enum gfx_format fmt,
    bool_t has_mipmap /* =FALSE */);
gfx_texture gfx_create_texturert_cube(uint width, uint height, enum gfx_format fmt);
gfx_texture gfx_create_texturert_arr(uint width, uint height, uint arr_cnt,
		enum gfx_format fmt);
void gfx_destroy_texture(gfx_texture tex);

/* get info */
const struct gfx_gpu_memstats* gfx_get_memstats();
bool_t gfx_check_feature(enum gfx_feature ft);

/* Multi-thread (delayed) object creation routines, currently only implemented for GL,
 * D3D11 spec doesn't need these */
/* Creates queued objects, called from main thread */
void gfx_delayed_createobjects();
/* Wait for created signal, called from loader thread */
void gfx_delayed_waitforobjects(uint thread_id);
/* Perform memory copy to mapped buffers only, called from loader threads */
void gfx_delayed_fillobjects(uint thread_id);
void gfx_delayed_finalizeobjects();
void gfx_delayed_release();

_EXTERN_END_

#endif /* __GFXDEVICE_H__ */
