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

#ifndef GFX_CMDQUEUE_H_
#define GFX_CMDQUEUE_H_

#include "dhcore/types.h"
#include "gfx-types.h"

/* TODO: remove un-used apis */
#include "engine-api.h"

_EXTERN_BEGIN_

gfx_cmdqueue gfx_create_cmdqueue();
result_t gfx_initcmdqueue(gfx_cmdqueue cmdqueue);
void gfx_releasecmdqueue(gfx_cmdqueue cmdqueue);
void gfx_destroy_cmdqueue(gfx_cmdqueue cmdqueue);

/* buffer / texture */
void gfx_buffer_update(gfx_cmdqueue cmdqueue, gfx_buffer buffer, const void* data, uint size);
void* gfx_buffer_map(gfx_cmdqueue cmdqueue, gfx_buffer buffer, uint offset, uint size,
		uint mode /* enum gfx_map_mode */, int sync_cpu);
void gfx_buffer_unmap(gfx_cmdqueue cmdqueue, gfx_buffer buffer);

void gfx_rendertarget_blit(gfx_cmdqueue cmdqueue,
		int dest_x, int dest_y, int dest_width, int dest_height,
		gfx_rendertarget src_rt, int src_x, int src_y, int src_width, int src_height);
void gfx_rendertarget_blitraw(gfx_cmdqueue cmdqueue, gfx_rendertarget src_rt);
void gfx_texture_generatemips(gfx_cmdqueue cmdqueue, gfx_texture tex);
void gfx_texture_update(gfx_cmdqueue cmdqueue, gfx_texture tex, const void* pixels);

/* input state */
void gfx_input_setlayout(gfx_cmdqueue cmdqueue, gfx_inputlayout inputlayout);

/* shader */
void gfx_program_setbindings(gfx_cmdqueue cmdqueue, const uint* bindings, uint binding_cnt);
void gfx_program_set(gfx_cmdqueue cmdqueue, gfx_program prog);
void gfx_program_setcblock(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
		gfx_buffer buffer, uint shaderbind_id, uint bind_idx);
void gfx_program_setsampler(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
		gfx_sampler sampler, uint shaderbind_id, uint texture_unit);
void gfx_program_settexture(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
    gfx_texture tex, uint texture_unit);
void gfx_program_setcblock_tbuffer(gfx_cmdqueue cmdqueue, gfx_program prog,
    enum gfx_shader_type shader, gfx_buffer buffer, uint shaderbind_id, uint texture_unit);
void gfx_program_bindcblock_range(gfx_cmdqueue cmdqueue,  gfx_program prog,
                                  enum gfx_shader_type shader, gfx_buffer buffer,
                                  uint shaderbind_id, uint bind_idx,
                                  uint offset, uint size);

/* output states */
void gfx_output_setviewport(gfx_cmdqueue cmdqueue, int x, int y, int width, int height);
void gfx_output_setviewportbias(gfx_cmdqueue cmdqueue, int x, int y, int width, int height);
void gfx_output_setblendstate(gfx_cmdqueue cmdqueue, gfx_blendstate blend,
		OPTIONAL const float* blend_color);
void gfx_output_setscissor(gfx_cmdqueue cmdqueue, int x, int y, int width, int height);
void gfx_output_setrasterstate(gfx_cmdqueue cmdqueue, gfx_rasterstate raster);
void gfx_output_setdepthstencilstate(gfx_cmdqueue cmdqueue, gfx_depthstencilstate ds,
		int stencil_ref);
void gfx_output_setrendertarget(gfx_cmdqueue cmdqueue, OPTIONAL gfx_rendertarget rt);
void gfx_output_clearrendertarget(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
    const float color[4], float depth, uint8 stencil, uint flags);

/* draw */
void gfx_draw(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type, uint vert_idx,
		uint vert_cnt, uint draw_id);
void gfx_draw_indexed(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
		uint ib_idx, uint idx_cnt, enum gfx_index_type ib_type, uint draw_id);
void gfx_draw_instance(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
		uint vert_idx, uint vert_cnt, uint instance_cnt, uint draw_id);
void gfx_draw_indexedinstance(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
		uint ib_idx, uint idx_cnt, enum gfx_index_type ib_type, uint instance_cnt,
		uint draw_id);

/* sync */
void gfx_flush(gfx_cmdqueue cmdqueue);
gfx_syncobj gfx_addsync(gfx_cmdqueue cmdqueue);
void gfx_waitforsync(gfx_cmdqueue cmdqueue, gfx_syncobj syncobj);
void gfx_removesync(gfx_cmdqueue cmdqueue, gfx_syncobj syncobj);

/* info */
void gfx_reset_framestats(gfx_cmdqueue cmdqueue);
void gfx_reset_devstates(gfx_cmdqueue cmdqueue);
const struct gfx_framestats* gfx_get_framestats(gfx_cmdqueue cmdqueue);

/* misc/internal */
void gfx_cmdqueue_resetsrvs(gfx_cmdqueue cmdqueue);

_EXTERN_END_

#endif /* GFX_CMDQUEUE_H_ */
