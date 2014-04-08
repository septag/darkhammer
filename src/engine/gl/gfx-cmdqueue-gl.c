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

#if defined(_GL_)

#include "GL/glew.h"
#include "dhcore/core.h"

#include "gfx-cmdqueue.h"
#include "mem-ids.h"
#include "gfx-device.h"
#include "gfx.h"
#include "gfx-shader.h"
#include "engine.h"
#include "app.h"

#define BUFFER_OFFSET(offset) ((uint8*)NULL + (offset))

/*************************************************************************************************
 * types
 */
struct gfx_cmdqueue_s
{
	struct gfx_framestats stats;
	int rtv_width;
	int rtv_height;
	struct gfx_rasterizer_desc last_raster;
	struct gfx_depthstencil_desc last_depthstencil;
	struct gfx_blend_desc last_blend;
    uint blit_shaderid;
    gfx_depthstencilstate blit_ds;
    gfx_sampler sampl_point;
};

/*************************************************************************************************
 * inlines
 */
INLINE uint get_indextype_size(enum gfx_index_type type)
{
	return (type == GFX_INDEX_UINT16) ? sizeof(uint16) : sizeof(uint);
}

/*************************************************************************************************
 * forward declarations
 */
void output_setrasterstate(gfx_cmdqueue cmdqueue, const struct gfx_rasterizer_desc* desc);
void output_setdepthstencilstate(gfx_cmdqueue cmdqueue, const struct gfx_depthstencil_desc* desc,
		int stencil_ref);
void output_setblendstate(gfx_cmdqueue cmdqueue, const struct gfx_blend_desc* desc,
		const float* blend_color);

/*************************************************************************************************/
gfx_cmdqueue gfx_create_cmdqueue()
{
	gfx_cmdqueue cmdqueue = (gfx_cmdqueue)ALLOC(sizeof(struct gfx_cmdqueue_s), MID_GFX);
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
	output_setrasterstate(cmdqueue, gfx_get_defaultraster());
	output_setdepthstencilstate(cmdqueue, gfx_get_defaultdepthstencil(), 0);
	output_setblendstate(cmdqueue, gfx_get_defaultblend(), NULL);

	return RET_OK;
}

void gfx_releasecmdqueue(gfx_cmdqueue cmdqueue)
{
}

void gfx_input_setlayout(gfx_cmdqueue cmdqueue, gfx_inputlayout inputlayout)
{
	ASSERT(inputlayout->type == GFX_OBJ_INPUTLAYOUT);

	glBindVertexArray((GLuint)inputlayout->api_obj);

    /* this part should be integrated with vertex-array-object and I shouldn't bind it again
     * don't know the reason yet ! */
    if (inputlayout->desc.il.ibuff != NULL)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ((gfx_buffer)inputlayout->desc.il.ibuff)->api_obj);

	cmdqueue->stats.input_cnt ++;
}

void gfx_program_set(gfx_cmdqueue cmdqueue, gfx_program prog)
{
	glUseProgram((GLuint)prog->api_obj);

	cmdqueue->stats.shaderchange_cnt ++;
}

void gfx_buffer_update(gfx_cmdqueue cmdqueue, gfx_buffer buffer, const void* data, uint size)
{
	ASSERT(buffer->type == GFX_OBJ_BUFFER);

	uint s = minui(size, buffer->desc.buff.size);
    GLuint target = (GLenum)buffer->desc.buff.type;
    glBindBuffer(target, (GLuint)buffer->api_obj);
    void* dest = glMapBufferRange(target, 0, s, GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_WRITE_BIT);
    if (dest != NULL)   {
        memcpy(dest, data, s);
        glUnmapBuffer(target);
    	cmdqueue->stats.map_cnt ++;
    }
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
	glUniformBlockBinding((GLuint)prog->api_obj, (GLuint)shaderbind_id, (GLuint)bind_idx);
	glBindBufferBase(GL_UNIFORM_BUFFER, (GLuint)bind_idx, (GLuint)buffer->api_obj);
}

void gfx_program_bindcblock_range(gfx_cmdqueue cmdqueue,  gfx_program prog,
                                  enum gfx_shader_type shader, gfx_buffer buffer,
                                  uint shaderbind_id, uint bind_idx,
                                  uint offset, uint size)
{
    glUniformBlockBinding((GLuint)prog->api_obj, (GLuint)shaderbind_id, (GLuint)bind_idx);
    glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)bind_idx, (GLuint)buffer->api_obj, offset, size);
}

void gfx_program_setsampler(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
		gfx_sampler sampler, uint shaderbind_id, uint texture_unit)
{
	glBindSampler(texture_unit, (GLuint)sampler->api_obj);
	glUniform1i(shaderbind_id, texture_unit);
}

void gfx_program_settexture(gfx_cmdqueue cmdqueue, gfx_program prog, enum gfx_shader_type shader,
        gfx_texture tex, uint texture_unit)
{
	glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture((GLenum)tex->desc.tex.type, (GLuint)tex->api_obj);
}

void gfx_output_setviewport(gfx_cmdqueue cmdqueue, int x, int y, int width, int height)
{
	/* convert y to meet engine coordinate system */
	y = cmdqueue->rtv_height - y;
	y = y - height;
	glViewport(x, y, width, height);
    glDepthRangef(0.0f, 1.0f);
}

void gfx_output_setviewportbias(gfx_cmdqueue cmdqueue, int x, int y, int width, int height)
{
    const float bias = 0.0000152588f;

    /* convert y to meet engine coordinate system */
    y = cmdqueue->rtv_height - y;
    y = y - height;
    glViewport(x, y, width, height);
    glDepthRangef(2.0f*bias, 1.0f-2.0f*bias);
}

void gfx_output_setblendstate(gfx_cmdqueue cmdqueue, gfx_blendstate blend,
		OPTIONAL const float* blend_color)
{
	const struct gfx_blend_desc* desc;
	if (blend != NULL)
		desc = &blend->desc.blend;
	else
		desc = gfx_get_defaultblend();

	output_setblendstate(cmdqueue, desc, blend_color);

	cmdqueue->stats.blendstatechange_cnt ++;
}

void output_setblendstate(gfx_cmdqueue cmdqueue, const struct gfx_blend_desc* desc,
		const float* blend_color)
{
	struct gfx_blend_desc* last = &cmdqueue->last_blend;
    struct gfx_blend_desc b;
    memcpy(&b, last, sizeof(struct gfx_blend_desc));

	if (desc->enable != b.enable)	{
		if (desc->enable)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
        last->enable = desc->enable;
	}

	if (desc->src_blend != b.src_blend || desc->dest_blend != b.dest_blend) {
		glBlendFunc((GLenum)desc->src_blend, (GLenum)desc->dest_blend);
        last->src_blend = desc->src_blend;
        last->dest_blend = desc->dest_blend;
    }

	if (desc->color_op != b.color_op)   {
		glBlendEquation((GLenum)desc->color_op);
        last->color_op = desc->color_op;
    }

	if (desc->write_mask != b.write_mask)	{
		glColorMask(BIT_CHECK(desc->write_mask, GFX_COLORWRITE_RED),
				BIT_CHECK(desc->write_mask, GFX_COLORWRITE_GREEN),
				BIT_CHECK(desc->write_mask, GFX_COLORWRITE_BLUE),
				BIT_CHECK(desc->write_mask, GFX_COLORWRITE_ALPHA));
        last->write_mask = desc->write_mask;
	}

	if (blend_color != NULL)
		glBlendColor(blend_color[0], blend_color[1], blend_color[2], blend_color[3]);
	else
		glBlendColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void gfx_output_setscissor(gfx_cmdqueue cmdqueue, int x, int y, int width, int height)
{
	/* convert y to meet engine coordinate system */
	y = cmdqueue->rtv_height - y;
	y = y - height;
	glScissor(x, y, width, height);
}

void gfx_output_setrasterstate(gfx_cmdqueue cmdqueue, gfx_rasterstate raster)
{
	const struct gfx_rasterizer_desc* desc;
	if (raster != NULL)
		desc = &raster->desc.raster;
	else
		desc = gfx_get_defaultraster();

	output_setrasterstate(cmdqueue, desc);

	cmdqueue->stats.rsstatechange_cnt ++;
}

void output_setrasterstate(gfx_cmdqueue cmdqueue, const struct gfx_rasterizer_desc* desc)
{
	struct gfx_rasterizer_desc* last = &cmdqueue->last_raster;
    struct gfx_rasterizer_desc r;
    memcpy(&r, last, sizeof(struct gfx_rasterizer_desc));

	if (desc->fill != r.fill)   {
		glPolygonMode(GL_FRONT_AND_BACK, (GLenum)desc->fill);
        last->fill = desc->fill;
    }

    if (desc->slopescaled_depthbias != r.slopescaled_depthbias ||
		desc->depth_bias != r.depth_bias)
    {
		glPolygonOffset(desc->slopescaled_depthbias, desc->depth_bias);
        last->slopescaled_depthbias = desc->slopescaled_depthbias;
        last->depth_bias = desc->depth_bias;
	}

	if (desc->cull != r.cull)	{
		if (desc->cull != GFX_CULL_NONE)	{
			glEnable(GL_CULL_FACE);
			glCullFace((GLenum)desc->cull);
		}   else    {
			glDisable(GL_CULL_FACE);
        }
        last->cull = desc->cull;
	}
	if (desc->depth_clip != r.depth_clip)	{
		if (desc->depth_clip)
			glDisable(GL_DEPTH_CLAMP);
		else
			glEnable(GL_DEPTH_CLAMP);
        last->depth_clip = desc->depth_clip;
	}

	if (desc->scissor_test != r.scissor_test)	{
		if (desc->scissor_test)
			glEnable(GL_SCISSOR_TEST);
		else
			glDisable(GL_SCISSOR_TEST);
        last->scissor_test = desc->scissor_test;
	}
}

void gfx_output_setdepthstencilstate(gfx_cmdqueue cmdqueue, gfx_depthstencilstate ds,
		int stencil_ref)
{
	const struct gfx_depthstencil_desc* desc;
	if (ds != NULL)
		desc = &ds->desc.ds;
	else
		desc = gfx_get_defaultdepthstencil();

	output_setdepthstencilstate(cmdqueue, desc, stencil_ref);

	cmdqueue->stats.dsstatechange_cnt ++;
}

void output_setdepthstencilstate(gfx_cmdqueue cmdqueue, const struct gfx_depthstencil_desc* desc,
		int stencil_ref)
{
    struct gfx_depthstencil_desc* last = &cmdqueue->last_depthstencil;
    struct gfx_depthstencil_desc d;
    memcpy(&d, last, sizeof(struct gfx_depthstencil_desc));

	if (desc->depth_enable != d.depth_enable)	{
		if (desc->depth_enable)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
        last->depth_enable = desc->depth_enable;
	}

	if (desc->stencil_enable != d.stencil_enable)	{
		if (desc->stencil_enable)
			glEnable(GL_STENCIL_TEST);
		else
			glDisable(GL_STENCIL_TEST);
        last->stencil_enable = desc->stencil_enable;
	}

	if (desc->depth_enable)	{
		if (desc->depth_write != d.depth_write) {
			glDepthMask((GLboolean)desc->depth_write);
            last->depth_write = desc->depth_write;
        }
        if (desc->depth_func != d.depth_func)   {
			glDepthFunc((GLenum)desc->depth_func);
            last->depth_func = desc->depth_func;
        }
	}

	if (desc->stencil_enable)	{
		glStencilFuncSeparate(GL_FRONT,
				(GLenum)desc->stencil_frontface_desc.cmp_func, stencil_ref, desc->stencil_mask);
		glStencilOpSeparate(GL_FRONT,
				(GLenum)desc->stencil_frontface_desc.fail_op,
				(GLenum)desc->stencil_frontface_desc.depthfail_op,
				(GLenum)desc->stencil_frontface_desc.pass_op);
		glStencilFuncSeparate(GL_BACK,
				(GLenum)desc->stencil_backface_desc.cmp_func, stencil_ref, desc->stencil_mask);
		glStencilOpSeparate(GL_BACK,
				(GLenum)desc->stencil_backface_desc.fail_op,
				(GLenum)desc->stencil_backface_desc.depthfail_op,
				(GLenum)desc->stencil_backface_desc.pass_op);

        last->stencil_frontface_desc.cmp_func = desc->stencil_frontface_desc.cmp_func;
        last->stencil_frontface_desc.fail_op = desc->stencil_frontface_desc.fail_op;
        last->stencil_frontface_desc.depthfail_op = desc->stencil_frontface_desc.depthfail_op;
        last->stencil_frontface_desc.pass_op = desc->stencil_frontface_desc.pass_op;
        last->stencil_backface_desc.cmp_func = desc->stencil_backface_desc.cmp_func;
        last->stencil_backface_desc.fail_op = desc->stencil_backface_desc.fail_op;
        last->stencil_backface_desc.depthfail_op = desc->stencil_backface_desc.depthfail_op;
        last->stencil_backface_desc.pass_op = desc->stencil_backface_desc.pass_op;
        last->stencil_mask = desc->stencil_mask;
	}
}

void* gfx_buffer_map(gfx_cmdqueue cmdqueue, gfx_buffer buffer, uint offset, uint size,
		uint mode /* enum gfx_map_mode */, bool_t sync_cpu)
{
	ASSERT(buffer->type == GFX_OBJ_BUFFER);
	GLenum target = (GLenum)buffer->desc.buff.type;
	GLbitfield flags = sync_cpu ? mode : (mode | GL_MAP_UNSYNCHRONIZED_BIT);

	cmdqueue->stats.map_cnt ++;

	glBindBuffer(target, (GLuint)buffer->api_obj);
	return glMapBufferRange(target, (GLintptr)offset, (GLsizeiptr)size, flags);
}

void gfx_buffer_unmap(gfx_cmdqueue cmdqueue, gfx_buffer buffer)
{
	ASSERT(buffer->type == GFX_OBJ_BUFFER);
	GLenum target = (GLenum)buffer->desc.buff.type;

	glBindBuffer(target, (GLuint)buffer->api_obj);
	glUnmapBuffer(target);
}

void gfx_draw(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type, uint vert_idx,
		uint vert_cnt, uint draw_id)
{
	glDrawArrays((GLenum)type, (GLint)vert_idx, (GLsizei)vert_cnt);

	cmdqueue->stats.draw_cnt ++;
	cmdqueue->stats.prims_cnt += vert_cnt;
	cmdqueue->stats.draw_prim_cnt[draw_id] += vert_cnt;
	cmdqueue->stats.draw_group_cnt[draw_id] ++;
}

void gfx_draw_indexed(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
		uint ib_idx, uint idx_cnt, enum gfx_index_type ib_type, uint draw_id)
{
	glDrawElements((GLenum)type, idx_cnt, (GLenum)ib_type,
			BUFFER_OFFSET(ib_idx*get_indextype_size(ib_type)));

	cmdqueue->stats.draw_cnt ++;
	cmdqueue->stats.prims_cnt += idx_cnt;
	cmdqueue->stats.draw_prim_cnt[draw_id] += idx_cnt;
	cmdqueue->stats.draw_group_cnt[draw_id] ++;
}

void gfx_draw_instance(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
		uint vert_idx, uint vert_cnt, uint instance_cnt, uint draw_id)
{
	glDrawArraysInstanced((GLenum)type, (GLint)vert_idx, (GLsizei)vert_cnt, instance_cnt);

	cmdqueue->stats.draw_cnt ++;
	cmdqueue->stats.prims_cnt += vert_cnt;
	cmdqueue->stats.draw_prim_cnt[draw_id] += vert_cnt;
	cmdqueue->stats.draw_group_cnt[draw_id] ++;
}

void gfx_draw_indexedinstance(gfx_cmdqueue cmdqueue, enum gfx_primitive_type type,
		uint ib_idx, uint idx_cnt, enum gfx_index_type ib_type, uint instance_cnt,
		uint draw_id)
{
	glDrawElementsInstanced((GLenum)type, idx_cnt, (GLenum)ib_type,
			BUFFER_OFFSET(ib_idx*get_indextype_size(ib_type)), (GLsizei)instance_cnt);

	cmdqueue->stats.draw_cnt ++;
	cmdqueue->stats.prims_cnt += idx_cnt;
	cmdqueue->stats.draw_prim_cnt[draw_id] += idx_cnt;
	cmdqueue->stats.draw_group_cnt[draw_id] ++;
}

void gfx_cmdqueue_setrtvsize(gfx_cmdqueue cmdqueue, uint width, uint height)
{
	cmdqueue->rtv_width = width;
	cmdqueue->rtv_height = height;
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
	static const GLenum bindings[] = {
			GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
			GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5
	};
	static const GLenum def_binding[] = {
			GL_BACK_LEFT
	};

	if (rt != NULL)	{
		ASSERT(rt->type == GFX_OBJ_RENDERTARGET);
		GLuint fbo = (GLuint)rt->api_obj;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        if (rt->desc.rt.rt_cnt != 0)
		    glDrawBuffers(rt->desc.rt.rt_cnt, bindings);
        else
            glDrawBuffer(GL_NONE);

	    gfx_set_rtvsize(rt->desc.rt.width, rt->desc.rt.height);
	}	else	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glDrawBuffers(1, def_binding);

		gfx_set_rtvsize(app_get_wndwidth(), app_get_wndheight());
	}

	cmdqueue->stats.rtchange_cnt ++;
}

void gfx_rendertarget_blit(gfx_cmdqueue cmdqueue,
		int dest_x, int dest_y, int dest_width, int dest_height,
		gfx_rendertarget src_rt, int src_x, int src_y, int src_width, int src_height)
{
	dest_y = cmdqueue->rtv_height - dest_y;
	dest_y = dest_y - dest_height;

	src_y = src_rt->desc.rt.height - src_y;
	src_y = src_y - src_height;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)src_rt->api_obj);
	glBlitFramebuffer(src_x, src_y, src_x + src_width, src_y + src_height,
			dest_x, dest_y, dest_x + dest_width, dest_y + dest_height,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void gfx_rendertarget_blitraw(gfx_cmdqueue cmdqueue, gfx_rendertarget src_rt)
{
    GLuint flags = GL_COLOR_BUFFER_BIT;
    if (src_rt->desc.rt.ds_texture != NULL)
        flags |= (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)src_rt->api_obj);
    glBlitFramebuffer(0, 0, src_rt->desc.rt.width, src_rt->desc.rt.height,
        0, 0, cmdqueue->rtv_width, cmdqueue->rtv_height, flags, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void gfx_flush(gfx_cmdqueue cmdqueue)
{
	glFlush();
}

gfx_syncobj gfx_addsync(gfx_cmdqueue cmdqueue)
{
	return glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void gfx_waitforsync(gfx_cmdqueue cmdqueue, gfx_syncobj syncobj)
{
	glClientWaitSync((GLsync)syncobj, GL_SYNC_FLUSH_COMMANDS_BIT, 5000000000);
}

void gfx_removesync(gfx_cmdqueue cmdqueue, gfx_syncobj syncobj)
{
	glDeleteSync((GLsync)syncobj);
}

void gfx_program_setbindings(gfx_cmdqueue cmdqueue, const uint* bindings, uint binding_cnt)
{
}

void gfx_cmdqueue_resetsrvs(gfx_cmdqueue cmdqueue)
{
    for (uint i = 0; i < 8; i++)  {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void gfx_output_clearrendertarget(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
    const float color[4], float depth, uint8 stencil, uint flags)
{
    if (rt == NULL) {
        app_clear_rendertarget(color, depth, stencil, flags);
        return;
    }

    if (rt->desc.rt.ds_texture != NULL &&
        (BIT_CHECK(flags, GFX_CLEAR_DEPTH) || BIT_CHECK(flags, GFX_CLEAR_STENCIL)))
    {
        glClearDepth(depth);
        glClearStencil(stencil);
        cmdqueue->stats.cleards_cnt ++;
    }

    if (BIT_CHECK(flags, GFX_CLEAR_COLOR))  {
        glClearColor(color[0], color[1], color[2], color[3]);
        cmdqueue->stats.clearrt_cnt ++;
    }

    glClear((GLbitfield)flags);
}

void gfx_program_setcblock_tbuffer(gfx_cmdqueue cmdqueue, gfx_program prog,
    enum gfx_shader_type shader, gfx_buffer buffer, uint shaderbind_id, uint texture_unit)
{
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_BUFFER, buffer->desc.buff.gl_tbuff);
    glUniform1i(shaderbind_id, texture_unit);

}

void gfx_texture_generatemips(gfx_cmdqueue cmdqueue, gfx_texture tex)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex->api_obj);
    glGenerateMipmap(GL_TEXTURE_2D);
}


void gfx_texture_update(gfx_cmdqueue cmdqueue, gfx_texture tex, const void* pixels)
{
    GLenum type = (GLenum)tex->desc.tex.type;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(type, (GLuint)tex->api_obj);
    glTexSubImage2D(type, 0, 0, 0, tex->desc.tex.width, tex->desc.tex.height,
        tex->desc.tex.gl_fmt, tex->desc.tex.gl_type, pixels);
    ASSERT(glGetError() == GL_NO_ERROR);
}


#endif  /* _GL_ */
