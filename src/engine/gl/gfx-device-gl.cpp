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

#include <stdio.h>

#include "dhcore/core.h"
#include "dhcore/pool-alloc.h"
#include "dhcore/color.h"
#include "dhcore/mt.h"
#include "dhcore/hash-table.h"
#include "dhcore/linked-list.h"
#include "dhcore/array.h"
#include "dhcore/timer.h"

#include "dheng/gfx-device.h"
#include "dheng/mem-ids.h"
#include "dheng/gfx.h"
#include "dheng/gfx-texture.h"

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
  #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

// AMD
#define VBO_FREE_MEMORY_ATI 0x87FB
#define TEXTURE_FREE_MEMORY_ATI 0x87FC
#define RENDERBUFFER_FREE_MEMORY_ATI 0x87FD
#define TOTAL_PHYSICAL_MEMORY_ATI 0x87FE

// NVIDIA
#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049 

// Types
struct DelayedSignal
{
    uint thread_id;
    uint signal_id;
    int waiting;
    uint pending_cnt; // Pending creates
    uint creates_cnt; // Actual creates in progress
    uint err_cnt;
    GLuint stream_pbo;  // PBO used to multi-threaded texture staging
};

struct DelayedItem
{
    struct gfx_obj_data* obj;
    uint thread_id;
    struct DelayedSignal* signal;
    struct linked_list lnode;
    void* mapped;

    union   {
        struct {
            enum gfxBufferType type;
            enum gfxMemHint memhint;
            uint size;
            void* data;
        } buff;

        struct {
            enum gfxTextureType type;
            uint width;
            uint height;
            uint depth;
            enum gfxFormat fmt;
            uint mip_cnt;
            uint array_size;
            uint total_size;
            enum gfxMemHint memhint;
            void* data;
            struct gfx_subresource_data* subress;
        } tex;

        struct {
            uint vbuff_cnt;
            uint input_cnt;
            struct gfx_input_vbuff_desc* vbuffs;
            struct gfx_input_element_binding* inputs;
            gfx_buffer idxbuffer;
            enum gfxIndexType itype;
            void* data;
        } il;
    } params;
};

struct GfxDevice
{
	appGfxParams params;
	struct gfx_gpu_memstats memstats;
	struct pool_alloc obj_pool; /* no need to be thread-safe, we always call it in main thread */

    uint buffer_alignment;
    uint buffer_uniform_max;

    mt_mutex objcreate_mtx; /* for locking objcreates data */
    struct linked_list* objcreates;   /* data: DelayedItem */
    struct linked_list* objunmaps;    /* data: DelayedItem, after created and filled,
                                         will be added to this list */
    mt_event objcreate_event;
    struct array objcreate_signals; /* item: DelayedSignal */
    int release_delayed;

    appGfxDeviceVersion ver;
};

/*************************************************************************************************
 * Globals
 */
static struct GfxDevice g_gfxdev;

/*************************************************************************************************
 * Fwd declarations
 */
void gfx_debug_enableoutput();
void APIENTRY gfx_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, GLvoid* userParam);
void shader_make_defines(char* define_code,
		const struct gfx_shader_define* defines, uint define_cnt);
int texture_is_compressed(enum gfxFormat fmt);
GLenum texture_get_glformat(enum gfxFormat fmt, OUT GLenum* type, OUT GLenum* internal_fmt);
int texture_has_alpha(enum gfxFormat fmt);

GLuint gfx_create_buffer_gl(enum gfxBufferType type, enum gfxMemHint memhint,
                            uint size, const void* data);
GLuint gfx_create_texture_gl(enum gfxTextureType type, uint width, uint height, uint depth,
                             enum gfxFormat fmt, uint mip_cnt, uint array_size, uint total_size,
                             const struct gfx_subresource_data* data, enum gfxMemHint memhint);

gfx_texture gfx_delayed_createtexture(enum gfxTextureType type, uint width, uint height,
                                      uint depth, enum gfxFormat fmt, uint mip_cnt,
                                      uint array_size, uint total_size,
                                      const struct gfx_subresource_data* data,
                                      enum gfxMemHint memhint, uint thread_id);
gfx_buffer gfx_delayed_createbuffer(enum gfxBufferType type, enum gfxMemHint memhint,
                                    uint size, const void* data, uint thread_id);
void gfx_delayed_releaseitem(struct DelayedItem* citem);
struct DelayedSignal* gfx_delayed_getsignal(uint thread_id);
struct DelayedSignal* gfx_delayed_createsignal(uint thread_id);

void APIENTRY gfx_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, GLvoid* user_param);
const char* gfx_debug_getseverity(GLenum severity);
const char* gfx_debug_getsource(GLenum source);
const char* gfx_debug_gettype(GLenum type);

/*************************************************************************************************
 * inlines
 */
INLINE struct gfx_obj_data* create_obj(uptr_t api_obj, enum gfx_obj_type type)
{
	struct gfx_obj_data* obj = (struct gfx_obj_data*)mem_pool_alloc(&g_gfxdev.obj_pool);
    ASSERT(obj != NULL);
    memset(obj, 0x00, sizeof(struct gfx_obj_data));
    obj->api_obj = (uptr_t)api_obj;
    obj->type = type;
    return obj;
}

INLINE void destroy_obj(struct gfx_obj_data* obj)
{
    obj->type = GFX_OBJ_NULL;
    mem_pool_free(&g_gfxdev.obj_pool, obj);
}

INLINE void shader_output_error(GLuint shader)
{
	char err_info[1000];
	glGetShaderInfoLog(shader, sizeof(err_info), NULL, err_info);
	err_printf(__FILE__, __LINE__, "glsl-compiler failed: %s", err_info);
}

INLINE GLenum sampler_choose_minfiter(enum gfxFilterMode min_filter,
    enum gfxFilterMode mip_filter)
{
	if (min_filter == gfxFilterMode::LINEAR && mip_filter == gfxFilterMode::LINEAR)
		return GL_LINEAR_MIPMAP_LINEAR;
	else if (min_filter == gfxFilterMode::LINEAR && mip_filter == gfxFilterMode::NEAREST)
		return GL_LINEAR_MIPMAP_NEAREST;
	else if (min_filter == gfxFilterMode::NEAREST && mip_filter == gfxFilterMode::NEAREST)
		return GL_NEAREST_MIPMAP_NEAREST;
	else if (min_filter == gfxFilterMode::NEAREST && mip_filter == gfxFilterMode::LINEAR)
		return GL_NEAREST_MIPMAP_LINEAR;
	else if (min_filter == gfxFilterMode::LINEAR && mip_filter == gfxFilterMode::UNKNOWN)
		return GL_LINEAR;
	else if (min_filter == gfxFilterMode::NEAREST && mip_filter == gfxFilterMode::UNKNOWN)
		return GL_NEAREST;
	else
		return GL_NEAREST;
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
	static const uint elem_cnt = sizeof(elems)/sizeof(struct gfx_input_element);

	for (uint i = 0; i < elem_cnt; i++)	{
		if (id == elems[i].id)
			return &elems[i];
	}
	return NULL;
}

INLINE appGfxDeviceVersion gfx_get_glver(int major, int minor)
{
    if (major == 3)  {
        if (minor == 2)
            return appGfxDeviceVersion::GL3_2;
        else if (minor >= 3)
            return appGfxDeviceVersion::GL3_3;
    }   else if (major == 4) {
        if (minor==0)
            return appGfxDeviceVersion::GL4_0;
        else if (minor==1)
            return appGfxDeviceVersion::GL4_1;
        else if (minor >= 2)
            return appGfxDeviceVersion::GL4_2;
    }

    return appGfxDeviceVersion::UNKNOWN;
}

/*************************************************************************************************/
void gfx_zerodev()
{
    memset(&g_gfxdev, 0x00, sizeof(struct GfxDevice));
}

result_t gfx_initdev(const appGfxParams* params)
{
	result_t r;

	memcpy(&g_gfxdev.params, params, sizeof(appGfxParams));

    log_print(LOG_INFO, "  init gfx-device ...");

    /* init GL functions */
    GLenum glew_ret = glewInit();
    if (glew_ret != GLEW_OK)   {
        err_printf(__FILE__, __LINE__, "gl-app init failed: could not init GLEW: %s",
            glewGetString(glew_ret));
        return RET_FAIL;
    }    

    /* recheck the version */
    int major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    if (major < 3 || (major == 3 && minor < 2))    {
        err_printf(__FILE__, __LINE__, "OpenGL context version does not meet the"
            " requested requirements (GL ver: %d.%d)", major, minor);
        return RET_FAIL;
    }

    g_gfxdev.ver = gfx_get_glver(major, minor);

    /* set debug callback */
    if (GLEW_ARB_debug_output)  {
        glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
        /* turn shader compiler errors off, I will catch them myself */
        glDebugMessageControlARB(GL_DEBUG_SOURCE_SHADER_COMPILER, GL_DONT_CARE, GL_DONT_CARE,
            0, NULL, GL_FALSE);
        /* turn API 'other' errors (they are just info for nvidia drivers) off, don't need them */
        glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE,
            0, NULL, GL_FALSE);
        glDebugMessageCallbackARB(gfx_debug_callback, NULL);
    }    

	/* object pool */
	r = mem_pool_create(mem_heap(), &g_gfxdev.obj_pool, sizeof(struct gfx_obj_data),
			200, MID_GFX);
	if (IS_FAIL(r))
		return RET_OUTOFMEMORY;

    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, (GLint*)&g_gfxdev.buffer_alignment);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, (GLint*)&g_gfxdev.buffer_uniform_max);

    /* threaded object creation */
    mt_mutex_init(&g_gfxdev.objcreate_mtx);
    g_gfxdev.objcreate_event = mt_event_create(mem_heap());
    r = arr_create(mem_heap(), &g_gfxdev.objcreate_signals, sizeof(struct DelayedSignal),
        16, 16, MID_GFX);
    if (IS_FAIL(r))
        return RET_OUTOFMEMORY;

	return RET_OK;
}

void gfx_releasedev()
{
    gfx_delayed_release();

    for (uint i = 0; i < g_gfxdev.objcreate_signals.item_cnt; i++)   {
        struct DelayedSignal* s = &((struct DelayedSignal*)
            g_gfxdev.objcreate_signals.buffer)[i];
        if (s->stream_pbo != 0)
            glDeleteBuffers(1, &s->stream_pbo);
    }

    mt_mutex_release(&g_gfxdev.objcreate_mtx);
    if (g_gfxdev.objcreate_event != NULL)
        mt_event_destroy(g_gfxdev.objcreate_event);
    arr_destroy(&g_gfxdev.objcreate_signals);

    /* detect leaks and delete remaining objects */
    uint leaks_cnt = mem_pool_getleaks(&g_gfxdev.obj_pool);
    if (leaks_cnt > 0)
        log_printf(LOG_WARNING, "gfx-device: total %d leaks found", leaks_cnt);

    mem_pool_destroy(&g_gfxdev.obj_pool);

	gfx_zerodev();
}

gfx_inputlayout gfx_delayed_createinputlayout(const struct gfx_input_vbuff_desc* vbuffs,
    uint vbuff_cnt, const struct gfx_input_element_binding* inputs, uint input_cnt,
    OPTIONAL gfx_buffer idxbuffer, OPTIONAL enum gfxIndexType itype, uint thread_id)
{
    struct DelayedItem* citem = (struct DelayedItem*)
        ALLOC(sizeof(struct DelayedItem), MID_GFX);
    if (citem == NULL)
        return NULL;
    memset(citem, 0x00, sizeof(struct DelayedItem));

    /* gfx object */
    gfx_inputlayout obj = create_obj(0, GFX_OBJ_INPUTLAYOUT);
    obj->desc.il.ibuff = idxbuffer;
    obj->desc.il.idxfmt = itype;
    obj->desc.il.vbuff_cnt = vbuff_cnt;
    for (uint i = 0; i < vbuff_cnt; i++)  {
        obj->desc.il.vbuffs[i] = vbuffs[i].vbuff;
        obj->desc.il.strides[i] = vbuffs[i].stride;
    }

    /* data for dalayed creating */
    size_t total_sz =
        sizeof(struct gfx_input_element_binding)*input_cnt +
        sizeof(struct gfx_input_vbuff_desc)*vbuff_cnt;

    uint8* data = (uint8*)ALLOC(total_sz, MID_GFX);
    if (data == NULL)   {
        destroy_obj(obj);
        FREE(citem);
        return NULL;
    }
    citem->obj = obj;
    citem->thread_id = thread_id;

    citem->params.il.data = data;
    citem->params.il.vbuffs = (struct gfx_input_vbuff_desc*)data;
    data += sizeof(struct gfx_input_vbuff_desc)*vbuff_cnt;
    citem->params.il.inputs = (struct gfx_input_element_binding*)data;
    memcpy(citem->params.il.vbuffs, vbuffs, sizeof(struct gfx_input_vbuff_desc)*vbuff_cnt);
    memcpy(citem->params.il.inputs, inputs, sizeof(struct gfx_input_element_binding)*input_cnt);
    citem->params.il.vbuff_cnt = vbuff_cnt;
    citem->params.il.input_cnt = input_cnt;
    citem->params.il.itype = itype;
    citem->params.il.idxbuffer = idxbuffer;

    mt_mutex_lock(&g_gfxdev.objcreate_mtx);
    struct DelayedSignal* s = gfx_delayed_createsignal(thread_id);
    s->pending_cnt ++;
    citem->signal = s;
    list_add(&g_gfxdev.objcreates, &citem->lnode, citem);
    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);

    return obj;
}

GLuint gfx_create_inputlayout_gl(const struct gfx_input_vbuff_desc* vbuffs,
    uint vbuff_cnt, const struct gfx_input_element_binding* inputs,
    uint input_cnt, gfx_buffer idxbuffer, enum gfxIndexType itype)
{
    glGetError();

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    uint elem_offset = 0;
    for (uint i = 0; i < input_cnt; i++)  {
        uint vb_idx = inputs[i].vb_idx;
        ASSERT(vb_idx < vbuff_cnt);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)vbuffs[vb_idx].vbuff->api_obj);

        gfxInputElemId elem_id = inputs[i].id;
        ASSERT(elem_id != gfxInputElemId::COUNT);
        const struct gfx_input_element* elem = get_elem(elem_id);
        GLuint vert_att_idx = (uint)elem_id;

        glEnableVertexAttribArray(vert_att_idx);
        if (elem->fmt == gfxInputElemFormat::FLOAT)    {
            glVertexAttribPointer(vert_att_idx, elem->component_cnt,
                (GLenum)elem->fmt,
                GL_FALSE, (GLsizei)vbuffs[vb_idx].stride,
                (inputs[i].elem_offset == GFX_INPUT_OFFSET_PACKED) ? (void*)(uptr_t)elem_offset :
                (void*)(uptr_t)inputs[i].elem_offset);
        }   else    {
            glVertexAttribIPointer(vert_att_idx, elem->component_cnt,
                (GLenum)elem->fmt,
                (GLsizei)vbuffs[vb_idx].stride,
                (inputs[i].elem_offset == GFX_INPUT_OFFSET_PACKED) ? (void*)(uptr_t)elem_offset :
                (void*)(uptr_t)inputs[i].elem_offset);
        }
        elem_offset += elem->stride;
    }
    glBindVertexArray(0);

    if (glGetError() != GL_NO_ERROR)    {
        glDeleteVertexArrays(1, &vao);
        return 0;
    }

    return vao;
}

gfx_inputlayout gfx_create_inputlayout(const struct gfx_input_vbuff_desc* vbuffs, uint vbuff_cnt,
                                       const struct gfx_input_element_binding* inputs,
                                       uint input_cnt, OPTIONAL gfx_buffer idxbuffer,
                                       OPTIONAL enum gfxIndexType itype, uint thread_id)
{
    if (thread_id != 0) {
        return gfx_delayed_createinputlayout(vbuffs, vbuff_cnt, inputs, input_cnt, idxbuffer, itype,
            thread_id);
    }

    GLuint vao = gfx_create_inputlayout_gl(vbuffs, vbuff_cnt, inputs, input_cnt, idxbuffer, itype);
    if (vao == 0)
        return NULL;

    gfx_inputlayout obj = create_obj((uptr_t)vao, GFX_OBJ_INPUTLAYOUT);
    obj->desc.il.ibuff = idxbuffer;
    obj->desc.il.idxfmt = itype;
    obj->desc.il.vbuff_cnt = vbuff_cnt;
    for (uint i = 0; i < vbuff_cnt; i++)  {
        obj->desc.il.vbuffs[i] = vbuffs[i].vbuff;
        obj->desc.il.strides[i] = vbuffs[i].stride;
    }
    return obj;
}

void gfx_destroy_inputlayout(gfx_inputlayout input_layout)
{
	GLuint id = (GLuint)input_layout->api_obj;
	glDeleteVertexArrays(1, &id);
	destroy_obj(input_layout);
}

gfx_program gfx_create_program(const struct gfx_shader_data* source_data,
		const struct gfx_input_element_binding* bindings, uint binding_cnt,
		const struct gfx_shader_define* defines, uint define_cnt,
		struct gfx_shader_binary_data* bin_data)
{
	GLuint vs_id = 0;
	GLuint ps_id = 0;
	GLuint gs_id = 0;
	GLuint prog_id = 0;
	GLint status;
	uint shader_cnt = 0;
	enum gfx_shader_type shader_types[GFX_PROGRAM_MAX_SHADERS];
	GLuint shaders[GFX_PROGRAM_MAX_SHADERS];
	const GLchar* codes[2];
	char define_code[1024];
	GLint lengths[2];
    gfx_program obj;

	shader_make_defines(define_code, defines, define_cnt);
	codes[0] = define_code;
	lengths[0] = strlen(define_code);

	prog_id = glCreateProgram();

	/* compile shaders */
	if (source_data->vs_source != NULL)	{
		codes[1] = (const GLchar*)source_data->vs_source;
		lengths[1] = (GLint)source_data->vs_size;
		vs_id = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs_id, 2, codes, lengths);
		glCompileShader(vs_id);

		glGetShaderiv(vs_id, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)	{
			shader_output_error(vs_id);
			goto err_cleanup;
		}
		glAttachShader(prog_id, vs_id);
		shader_types[shader_cnt] = GFX_SHADER_VERTEX;
		shaders[shader_cnt] = vs_id;
		shader_cnt ++;
	}

	if (source_data->ps_source != NULL)	{
		codes[1] = (const GLchar*)source_data->ps_source;
		lengths[1] = (GLint)source_data->ps_size;
		ps_id = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(ps_id, 2, codes, lengths);
		glCompileShader(ps_id);

		glGetShaderiv(ps_id, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)		{
			shader_output_error(ps_id);
			goto err_cleanup;
		}
		glAttachShader(prog_id, ps_id);
		shader_types[shader_cnt] = GFX_SHADER_PIXEL;
		shaders[shader_cnt] = ps_id;
		shader_cnt ++;
	}

	if (source_data->gs_source != NULL)	{
		codes[1] = (const GLchar*)source_data->gs_source;
		lengths[1] = (GLint)source_data->gs_size;
		gs_id = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(gs_id, 2, codes, lengths);
		glCompileShader(gs_id);

		glGetShaderiv(gs_id, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)		{
			shader_output_error(gs_id);
			goto err_cleanup;
		}
		glAttachShader(prog_id, gs_id);
		shader_types[shader_cnt] = GFX_SHADER_GEOMETRY;
		shaders[shader_cnt] = gs_id;
        shader_cnt ++;
	}

	/* bind elements (attributes) */
	for (uint i = 0; i < binding_cnt; i++)
		glBindAttribLocation(prog_id, (GLuint)bindings[i].id, bindings[i].var_name);

	/* link */
	if (bin_data != NULL)
		glProgramParameteri(prog_id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	glLinkProgram(prog_id);

	glGetProgramiv(prog_id, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)	{
		char err_info[1000];
		glGetProgramInfoLog(prog_id, sizeof(err_info), NULL, err_info);
		err_printf(__FILE__, __LINE__, "glsl-compiler failed: %s", err_info);
		goto err_cleanup;
	}

	/* link successful, get binary code */
	if (bin_data != NULL)	{
		GLint bin_size;
		glGetProgramiv(prog_id, GL_PROGRAM_BINARY_LENGTH, &bin_size);
		if (bin_size > 0)	{
			bin_data->prog_size = bin_size;
			bin_data->prog_data = ALLOC(bin_size, MID_GFX);
			if (bin_data->prog_data == NULL)	{
				err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
				goto err_cleanup;
			}
			glGetProgramBinary(prog_id, bin_size, NULL, (GLenum*)&bin_data->fmt,
					bin_data->prog_data);
		}
	}

	/* final obj */
	obj = create_obj((uptr_t)prog_id, GFX_OBJ_PROGRAM);
	obj->desc.prog.shader_cnt = shader_cnt;
	for (uint i = 0; i < shader_cnt; i++)	{
		obj->desc.prog.shader_types[i] = shader_types[i];
		obj->desc.prog.shaders[i] = (uptr_t)shaders[i];
	}
	return obj;

err_cleanup:
	if (vs_id != 0)
		glDeleteShader(vs_id);
	if (ps_id != 0)
		glDeleteShader(ps_id);
	if (gs_id != 0)
		glDeleteShader(gs_id);
	if (prog_id != 0)
		glDeleteProgram(prog_id);

	return NULL;
}

void shader_make_defines(char* define_code,
		const struct gfx_shader_define* defines, uint define_cnt)
{
    char version[32];
	char line[128];
    appGfxDeviceVersion gfxver = gfx_get_hwver();

    /* make version preprocessor */
    switch (gfxver)   {
    case appGfxDeviceVersion::GL3_2:
        strcpy(version, "#version 150\n");
        break;
    case appGfxDeviceVersion::GL3_3:
        strcpy(version, "#version 330\n");
        break;
    case appGfxDeviceVersion::GL4_0:
        strcpy(version, "#version 400\n");
        break;
    case appGfxDeviceVersion::GL4_1:
        strcpy(version, "#version 410\n");
        break;
    case appGfxDeviceVersion::GL4_2:
        strcpy(version, "#version 420\n");
        break;
    case appGfxDeviceVersion::GL4_3:
        strcpy(version, "#version 430\n");
        break;
    default:
        version[0] = 0;
    }

    strcpy(define_code, version);

    /* attrib location is required if GL is 3.2 (osx) */
    if (gfxver == appGfxDeviceVersion::GL3_2)
        strcat(define_code, "#extension GL_ARB_explicit_attrib_location : require\n");

	for (uint i = 0; i < define_cnt; i++)	{
		sprintf(line, "#define %s %s\n", defines[i].name, defines[i].value);
		strcat(define_code, line);
	}
}

gfx_program gfx_create_program_bin(const struct gfx_program_bin_desc* bindesc)
{
	GLint status;
	GLuint prog_id = glCreateProgram();

	glProgramBinary(prog_id, (GLenum)bindesc->fmt, bindesc->data, (GLsizei)bindesc->size);
	glGetProgramiv(prog_id, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)	{
		char err_info[1000];
		glGetProgramInfoLog(prog_id, sizeof(err_info), NULL, err_info);
		err_printf(__FILE__, __LINE__, "glsl-linker failed: %s", err_info);
		glDeleteProgram(prog_id);
		return NULL;
	}

	return create_obj((uptr_t)prog_id, GFX_OBJ_PROGRAM);
}


void gfx_destroy_program(gfx_program prog)
{
	for (uint i = 0; i < prog->desc.prog.shader_cnt; i++)	{
		GLuint shader_id = (GLuint)prog->desc.prog.shaders[i];
		if (shader_id != 0)
			glDeleteShader(shader_id);
	}
	glDeleteProgram((GLuint)prog->api_obj);
	destroy_obj(prog);
}

/* runs in main thread */
void gfx_delayed_createobjects()
{
    if (!mt_mutex_try(&g_gfxdev.objcreate_mtx))
        return;

    struct linked_list* lnode = g_gfxdev.objcreates;
    while (lnode != NULL)   {
        struct DelayedItem* citem = (struct DelayedItem*)lnode->data;
        struct gfx_obj_data* obj = citem->obj;
        ASSERT(obj);

        struct DelayedSignal* s = citem->signal;
        ASSERT(s);

        /* important: if not created,then proceed */
        if (citem->mapped == NULL)  {
            switch (obj->type)  {
            case GFX_OBJ_BUFFER:
                {
                    GLuint buff_id = gfx_create_buffer_gl(
                        citem->params.buff.type,
                        citem->params.buff.memhint,
                        citem->params.buff.size,
                        NULL);

                    if (buff_id != 0)    {
                        citem->mapped = glMapBufferRange(
                            (GLenum)citem->params.buff.type, 0, (GLsizeiptr)citem->params.buff.size,
                            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
                        ASSERT(citem->mapped);

                        obj->api_obj = (uptr_t)buff_id;
                        s->creates_cnt ++;
                    }   else    {
                        s->err_cnt ++;
                    }
                }
                break;
            case GFX_OBJ_TEXTURE:
                {
                    if (s->stream_pbo == 0)
                        glGenBuffers(1, &s->stream_pbo);
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, s->stream_pbo);
                    glBufferData(GL_PIXEL_UNPACK_BUFFER, citem->params.tex.total_size, NULL,
                        GL_STATIC_DRAW);

                    citem->mapped = glMapBufferRange(
                        GL_PIXEL_UNPACK_BUFFER, 0, (GLsizeiptr)citem->params.tex.total_size,
                        GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
                    ASSERT(citem->mapped);

                    s->creates_cnt ++;
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }
                break;

            case GFX_OBJ_INPUTLAYOUT:
                citem->mapped = (void*)0x1;
                s->creates_cnt ++;  /* don't do anything now (until all buffers are created) */
                break;

            default:
                break;
            }
        }

        /* check if all objects are processed, trigger signal */
        if (s->waiting && (s->err_cnt + s->creates_cnt) == s->pending_cnt)    {
            s->pending_cnt = s->creates_cnt = s->err_cnt = 0;
            s->waiting = FALSE;
            mt_event_trigger(g_gfxdev.objcreate_event, s->signal_id);
        }

        lnode = lnode->next;
    }

    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);
}

/* runs in loader thread */
void gfx_delayed_fillobjects(uint thread_id)
{
    mt_mutex_lock(&g_gfxdev.objcreate_mtx);

    struct linked_list* lnode = g_gfxdev.objcreates;
    while (lnode != NULL)  {
        struct DelayedItem* citem = (struct DelayedItem*)lnode->data;
        struct gfx_obj_data* obj = citem->obj;
        struct linked_list* lnext = lnode->next;

        if (citem->thread_id == thread_id)  {
            if (citem->mapped != NULL)  {
                switch (obj->type)  {
                case GFX_OBJ_BUFFER:
                    memcpy(citem->mapped, citem->params.buff.data, citem->params.buff.size);
                    FREE(citem->params.buff.data);
                    break;
                case GFX_OBJ_TEXTURE:
                    memcpy(citem->mapped, citem->params.tex.data, citem->params.tex.total_size);
                    FREE(citem->params.tex.data);
                    break;
                default:
                    break;
                }
            }

            list_remove(&g_gfxdev.objcreates, lnode);
            list_add(&g_gfxdev.objunmaps, lnode, citem);
        }
        lnode = lnext;
    }

    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);
}

/* runs in main thread, final stage: unmaps objects and finalize creation, also does cleanup */
void gfx_delayed_finalizeobjects()
{
    if (!mt_mutex_try(&g_gfxdev.objcreate_mtx))
        return;

    struct linked_list* lnode = g_gfxdev.objunmaps;
    while (lnode != NULL)   {
        struct DelayedItem* citem = (struct DelayedItem*)lnode->data;
        struct gfx_obj_data* obj = citem->obj;
        struct linked_list* lnext = lnode->next;

        if (citem->mapped != NULL)  {
            switch (obj->type)  {
            case GFX_OBJ_BUFFER:
                glBindBuffer((GLenum)obj->desc.buff.type, (GLuint)obj->api_obj);
                glUnmapBuffer((GLenum)obj->desc.buff.type);
                break;
            case GFX_OBJ_TEXTURE:
                {
                    struct DelayedSignal* s = citem->signal;
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, s->stream_pbo);
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

                    obj->api_obj = gfx_create_texture_gl(
                        citem->params.tex.type,
                        citem->params.tex.width,
                        citem->params.tex.height,
                        citem->params.tex.depth,
                        citem->params.tex.fmt,
                        citem->params.tex.mip_cnt,
                        citem->params.tex.array_size,
                        citem->params.tex.total_size,
                        citem->params.tex.subress,
                        citem->params.tex.memhint);

                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                    FREE(citem->params.tex.subress);
                }
                break;

            case GFX_OBJ_INPUTLAYOUT:
                {
                    obj->api_obj = gfx_create_inputlayout_gl(
                        citem->params.il.vbuffs,
                        citem->params.il.vbuff_cnt,
                        citem->params.il.inputs,
                        citem->params.il.input_cnt,
                        citem->params.il.idxbuffer,
                        citem->params.il.itype);
                    FREE(citem->params.il.data);
                }
                break;

            default:
                break;
            }


        }

        list_remove(&g_gfxdev.objunmaps, lnode);
        FREE(citem);
        lnode = lnext;
    }

    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);
}

/* runs in loader thread */
void gfx_delayed_waitforobjects(uint thread_id)
{
    mt_mutex_lock(&g_gfxdev.objcreate_mtx);
    if (g_gfxdev.release_delayed)   {
        mt_mutex_unlock(&g_gfxdev.objcreate_mtx);
        return;
    }

    struct DelayedSignal* s = gfx_delayed_getsignal(thread_id);
    ASSERT(s);
    s->waiting = TRUE;
    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);
    mt_event_wait(g_gfxdev.objcreate_event, s->signal_id, MT_TIMEOUT_INFINITE);
}

/* runs in main thread */
void gfx_delayed_release()
{
    /* cleanup threaded pending object creates */
    mt_mutex_lock(&g_gfxdev.objcreate_mtx);
    struct linked_list* lnode = g_gfxdev.objcreates;
    while (lnode != NULL)   {
        struct linked_list* lnext = lnode->next;
        struct DelayedItem* citem = (struct DelayedItem*)lnode->data;
        gfx_delayed_releaseitem(citem);
        lnode = lnext;
    }

    lnode = g_gfxdev.objunmaps;
    while (lnode != NULL)   {
        struct linked_list* lnext = lnode->next;
        struct DelayedItem* citem = (struct DelayedItem*)lnode->data;
        gfx_delayed_releaseitem(citem);
        lnode = lnext;
    }

    g_gfxdev.objcreates = NULL;
    g_gfxdev.release_delayed = TRUE;
    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);

    for (uint i = 0; i < g_gfxdev.objcreate_signals.item_cnt; i++)   {
        struct DelayedSignal* s =
            &((struct DelayedSignal*)g_gfxdev.objcreate_signals.buffer)[i];
        s->waiting = TRUE;
        mt_event_trigger(g_gfxdev.objcreate_event, s->signal_id);
    }
}

void gfx_delayed_releaseitem(struct DelayedItem* citem)
{
    switch (citem->obj->type)   {
    case GFX_OBJ_BUFFER:
        if (citem->params.buff.data != NULL)    {
            FREE(citem->params.buff.data);
        }
        break;
    case GFX_OBJ_TEXTURE:
        if (citem->params.tex.data != NULL) {
            FREE(citem->params.tex.data);
        }

        if (citem->params.tex.subress != NULL)  {
            FREE(citem->params.tex.subress);
        }

        break;
    case GFX_OBJ_INPUTLAYOUT:
        if (citem->params.il.data != NULL)  {
            FREE(citem->params.il.data);
        }
        break;
    default:
        break;
    }

    FREE(citem);
}

gfx_buffer gfx_create_buffer(enum gfxBufferType type, enum gfxMemHint memhint,
		uint size, const void* data, uint thread_id)
{
    if (thread_id != 0)
        return gfx_delayed_createbuffer(type, memhint, size, data, thread_id);

    GLuint buff_id = gfx_create_buffer_gl(type, memhint, size, data);
    if (buff_id == 0)
        return NULL;

    /* create tbuffer texture for SHADERTEXTURE types */
    GLuint tbuff = 0;
    if (type == gfxBufferType::SHADER_TEXTURE)   {
        glGenTextures(1, &tbuff);
        glBindTexture(GL_TEXTURE_BUFFER, tbuff);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buff_id);
        if (glGetError() != GL_NO_ERROR)    {
            glDeleteBuffers(1, &buff_id);
            glDeleteBuffers(1, &tbuff);
            return NULL;
        }
    }

	gfx_buffer obj = create_obj((uptr_t)buff_id, GFX_OBJ_BUFFER);
	obj->desc.buff.type = type;
	obj->desc.buff.size = size;
    obj->desc.buff.gl_tbuff = tbuff;
    obj->desc.buff.alignment = g_gfxdev.buffer_alignment;

    g_gfxdev.memstats.buffer_cnt ++;
    g_gfxdev.memstats.buffers += size;
	return obj;
}

/* runs in loader thread */
gfx_buffer gfx_delayed_createbuffer(enum gfxBufferType type, enum gfxMemHint memhint,
                                    uint size, const void* data, uint thread_id)
{
    struct DelayedItem* citem = (struct DelayedItem*)
        ALLOC(sizeof(struct DelayedItem), MID_GFX);
    if (citem == NULL)
        return NULL;
    memset(citem, 0x00, sizeof(struct DelayedItem));
    gfx_buffer obj = create_obj(0, GFX_OBJ_BUFFER);
    obj->desc.buff.type = type;
    obj->desc.buff.size = size;
    obj->desc.buff.alignment = g_gfxdev.buffer_alignment;

    void* ndata = ALLOC(size, MID_GFX);
    if (data == NULL)   {
        destroy_obj(obj);
        FREE(citem);
        return NULL;
    }
    memcpy(ndata, data, size);

    citem->obj = obj;
    citem->thread_id = thread_id;
    citem->params.buff.data = ndata;
    citem->params.buff.memhint = memhint;
    citem->params.buff.size = size;
    citem->params.buff.type = type;

    mt_mutex_lock(&g_gfxdev.objcreate_mtx);
    struct DelayedSignal* s = gfx_delayed_createsignal(thread_id);
    s->pending_cnt ++;
    citem->signal = s;
    list_add(&g_gfxdev.objcreates, &citem->lnode, citem);
    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);

    return obj;
}

GLuint gfx_create_buffer_gl(enum gfxBufferType type, enum gfxMemHint memhint,
                            uint size, const void* data)
{
    glGetError();

    GLuint buff_id;
    glGenBuffers(1, &buff_id);
    glBindBuffer((GLenum)type, buff_id);
    glBufferData((GLenum)type, (GLsizeiptr)size, (const GLvoid*)data, (GLenum)memhint);
    if (glGetError() != GL_NO_ERROR)	{
        glDeleteBuffers(1, &buff_id);
        return 0;
    }

    return buff_id;
}


void gfx_destroy_buffer(gfx_buffer buff)
{
    g_gfxdev.memstats.buffer_cnt --;
    g_gfxdev.memstats.buffers -= buff->desc.buff.size;

	GLuint id = (GLuint)buff->api_obj;
    if (id != 0)
	    glDeleteBuffers(1, &id);

    GLuint tbuff = (GLuint)buff->desc.buff.gl_tbuff;
    if (tbuff != 0)
        glDeleteTextures(1, &tbuff);

	destroy_obj(buff);
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

gfx_sampler gfx_create_sampler(const struct gfx_sampler_desc* desc)
{
	glGetError();

	GLuint sampler_id;
	glGenSamplers(1, &sampler_id);
	glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER,
			sampler_choose_minfiter(desc->filter_min, desc->filter_mip));
	glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, (GLint)desc->filter_mag);
	glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, (GLint)desc->address_u);
	glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, (GLint)desc->address_v);
	glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_R, (GLint)desc->address_w);
	if (desc->cmp_func != gfxCmpFunc::OFF)	{
		glSamplerParameteri(sampler_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glSamplerParameteri(sampler_id, GL_TEXTURE_COMPARE_FUNC, (GLint)desc->cmp_func);
	}	else	{
		glSamplerParameteri(sampler_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	}
	glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_LOD, (GLint)desc->lod_min);
	glSamplerParameteri(sampler_id, GL_TEXTURE_MAX_LOD, (GLint)desc->lod_max);
	glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLfloat)desc->aniso_max);
	glSamplerParameterfv(sampler_id, GL_TEXTURE_BORDER_COLOR, desc->border_color);

	if (glGetError() != GL_NO_ERROR)	{
		glDeleteSamplers(1, &sampler_id);
		return NULL;
	}

	return create_obj((uptr_t)sampler_id, GFX_OBJ_SAMPLER);
}


void gfx_destroy_sampler(gfx_sampler sampler)
{
	GLuint sampler_id = (GLuint)sampler->api_obj;
	glDeleteSamplers(1, &sampler_id);
	destroy_obj(sampler);
}


gfx_texture gfx_create_texture(enum gfxTextureType type, uint width, uint height, uint depth,
		enum gfxFormat fmt, uint mip_cnt, uint array_size, uint total_size,
		const struct gfx_subresource_data* data, enum gfxMemHint memhint, uint thread_id)
{
	ASSERT(array_size == 1);	/* not implemented yet */

    if (thread_id != 0) {
        return gfx_delayed_createtexture(type, width, height, depth, fmt, mip_cnt, array_size,
            total_size, data, memhint, thread_id);
    }

    GLuint tex_id = gfx_create_texture_gl(type, width, height, depth, fmt, mip_cnt, array_size,
        total_size, data, memhint);
    if (tex_id == 0)
        return NULL;

    GLenum gl_type = 0;
    GLenum gl_internal = 0;
    GLenum gl_fmt = texture_get_glformat(fmt, &gl_type, &gl_internal);

	gfx_texture obj = create_obj((uptr_t)tex_id, GFX_OBJ_TEXTURE);
	obj->desc.tex.type = type;
	obj->desc.tex.width = width;
	obj->desc.tex.height = height;
	obj->desc.tex.fmt = fmt;
	obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.size = total_size;
    obj->desc.tex.depth = array_size;
    obj->desc.tex.is_rt = FALSE;
    obj->desc.tex.depth = array_size;
    obj->desc.tex.mip_cnt = mip_cnt;
    obj->desc.tex.gl_type = gl_type;
    obj->desc.tex.gl_fmt = gl_fmt;

    g_gfxdev.memstats.texture_cnt ++;
    g_gfxdev.memstats.textures += total_size;
	return obj;
}

gfx_texture gfx_delayed_createtexture(enum gfxTextureType type, uint width, uint height,
                                      uint depth, enum gfxFormat fmt, uint mip_cnt,
                                      uint array_size, uint total_size,
                                      const struct gfx_subresource_data* data,
                                      enum gfxMemHint memhint, uint thread_id)
{
    struct DelayedItem* citem = (struct DelayedItem*)
    ALLOC(sizeof(struct DelayedItem), MID_GFX);
    if (citem == NULL)
        return NULL;
    memset(citem, 0x00, sizeof(struct DelayedItem));
    gfx_buffer obj = create_obj(0, GFX_OBJ_TEXTURE);

    GLenum gl_type = 0;
    GLenum gl_internal = 0;
    GLenum gl_fmt = texture_get_glformat(fmt, &gl_type, &gl_internal);
    obj->desc.tex.type = type;
    obj->desc.tex.width = width;
    obj->desc.tex.height = height;
    obj->desc.tex.fmt = fmt;
    obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.size = total_size;
    obj->desc.tex.depth = array_size;
    obj->desc.tex.is_rt = FALSE;
    obj->desc.tex.depth = array_size;
    obj->desc.tex.mip_cnt = mip_cnt;
    obj->desc.tex.gl_type = gl_type;
    obj->desc.tex.gl_fmt = gl_fmt;

    void* ndata = ALLOC(total_size, MID_GFX);
    if (data == NULL)   {
        destroy_obj(obj);
        FREE(citem);
        return NULL;
    }
    memcpy(ndata, data[0].p, total_size);

    citem->obj = obj;
    citem->thread_id = thread_id;

    uint subres_cnt = array_size * mip_cnt;
    citem->params.tex.data = ndata;
    citem->params.tex.memhint = memhint;
    citem->params.tex.total_size = total_size;
    citem->params.tex.type = type;
    citem->params.tex.array_size = array_size;
    citem->params.tex.width = width;
    citem->params.tex.height = height;
    citem->params.tex.depth = depth;
    citem->params.tex.fmt = fmt;
    citem->params.tex.mip_cnt = mip_cnt;
    citem->params.tex.subress = (struct gfx_subresource_data*)
        ALLOC(sizeof(struct gfx_subresource_data)*mip_cnt*array_size, MID_GFX);
    ASSERT(citem->params.tex.subress);

    /* re-evaluate subresources, the 'p' member if the structure now presents offset to the main buffer */
    uint offset = 0;
    for (uint i = 0; i < subres_cnt; i++) {
        struct gfx_subresource_data* subres = &citem->params.tex.subress[i];
        subres->p = (void*)(uptr_t)offset;
        subres->pitch_row = data[i].pitch_row;
        subres->pitch_slice = data[i].pitch_slice;
        subres->size = data[i].size;
        offset += data[i].size;
    }

    mt_mutex_lock(&g_gfxdev.objcreate_mtx);
    struct DelayedSignal* s = gfx_delayed_createsignal(thread_id);
    s->pending_cnt ++;
    citem->signal = s;

    list_add(&g_gfxdev.objcreates, &citem->lnode, citem);
    mt_mutex_unlock(&g_gfxdev.objcreate_mtx);

    return obj;
}

struct DelayedSignal* gfx_delayed_createsignal(uint thread_id)
{
    struct DelayedSignal* s = gfx_delayed_getsignal(thread_id);
    if (s == NULL)  {
        s = (struct DelayedSignal*)arr_add(&g_gfxdev.objcreate_signals);
        ASSERT(s);
        memset(s, 0x00, sizeof(struct DelayedSignal));
        s->thread_id = thread_id;
        s->signal_id = mt_event_addsignal(g_gfxdev.objcreate_event);
    }

    return s;
}

struct DelayedSignal* gfx_delayed_getsignal(uint thread_id)
{
    for (uint i = 0, cnt = g_gfxdev.objcreate_signals.item_cnt; i < cnt; i++)    {
        struct DelayedSignal* s =
            &((struct DelayedSignal*)g_gfxdev.objcreate_signals.buffer)[i];
        if (s->thread_id == thread_id)
            return s;
    }
    return NULL;
}


gfx_texture gfx_create_texturert(uint width, uint height, enum gfxFormat fmt,
    int has_mipmap)
{
	GLenum gl_fmt;
	GLenum gl_type;
    GLenum gl_internal;
	GLuint tex_id;
    uint mipcnt = 1;

    if (has_mipmap) {
        mipcnt = 1 + (uint)floorf(log10f((float)maxui(width, height))/log10f(2.0f));
    }

	glGetError();
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);
	gl_fmt = texture_get_glformat(fmt, &gl_type, &gl_internal);
	glTexImage2D(GL_TEXTURE_2D, 0, gl_internal, (GLsizei)width, (GLsizei)height, 0,
			gl_fmt, gl_type, NULL);
    if (has_mipmap)
        glGenerateMipmap(GL_TEXTURE_2D);

	if (glGetError() != GL_NO_ERROR)	{
		glDeleteTextures(1, &tex_id);
		return NULL;
	}

	gfx_texture obj = create_obj((uptr_t)tex_id, GFX_OBJ_TEXTURE);
	obj->desc.tex.type = gfxTextureType::TEX_2D;
	obj->desc.tex.fmt = fmt;
	obj->desc.tex.width = width;
	obj->desc.tex.height = height;
    obj->desc.tex.depth = 1;
	obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.size = width*height*(gfx_texture_getbpp(fmt)/8);
    obj->desc.tex.is_rt = TRUE;
    obj->desc.tex.mip_cnt = mipcnt;

    g_gfxdev.memstats.rttexture_cnt ++;
    g_gfxdev.memstats.rt_textures += obj->desc.tex.size;

	return obj;
}


GLuint gfx_create_texture_gl(enum gfxTextureType type, uint width, uint height, uint depth,
                             enum gfxFormat fmt, uint mip_cnt, uint array_size, uint total_size,
                             const struct gfx_subresource_data* data, enum gfxMemHint memhint)
{
    glGetError();

    GLuint tex_id;
    uint i, c;
    GLenum gl_fmt = 0;
    GLenum gl_type = 0;
    GLenum gl_internal = 0;
    uint w = width;
    uint h = height;
    GLenum cube_targets[] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

    glGenTextures(1, &tex_id);
    glBindTexture((GLenum)type, tex_id);

    int is_compressed = texture_is_compressed(fmt);
    if (!is_compressed)
        gl_fmt = texture_get_glformat(fmt, &gl_type, &gl_internal);

    switch (type)   {
    case gfxTextureType::TEX_1D:
        if (is_compressed)  {
            for (i = 0; i < mip_cnt; i++)   {
                glCompressedTexImage1D((GLenum)type, (GLint)i, (GLenum)fmt, (GLsizei)w, 0,
                    (GLsizei)data[i].size, data[i].p);
                w = maxui(1, w >> 1);
            }
        }   else    {
            for (i = 0; i < mip_cnt; i++)   {
                glTexImage1D((GLenum)type, (GLint)i, gl_internal, (GLsizei)w, 0,
                    gl_fmt, gl_type, data[i].p);
                w = maxui(1, w >> 1);
            }
        }
    case gfxTextureType::TEX_2D:
        if (is_compressed)  {
            for (i = 0; i < mip_cnt; i++)   {
                glCompressedTexImage2D((GLenum)type, (GLint)i, (GLenum)fmt, (GLsizei)w,
                    (GLsizei)h, 0, (GLsizei)data[i].size, data[i].p);
                w = maxui(1, w >> 1);
                h = maxui(1, h >> 1);
            }
        }   else    {
            for (i = 0; i < mip_cnt; i++)   {
                glTexImage2D((GLenum)type, (GLint)i, gl_internal, (GLsizei)w, (GLsizei)h,
                    0, gl_fmt, gl_type, data[i].p);
                w = maxui(1, w >> 1);
                h = maxui(1, h >> 1);
            }
        }
        break;
    case gfxTextureType::TEX_3D:
        if (is_compressed)  {
            for (i = 0; i < mip_cnt; i++)   {
                glCompressedTexImage3D((GLenum)type, (GLint)i, (GLenum)fmt, (GLsizei)w, (GLsizei)h,
                    (GLsizei)depth, 0, (GLsizei)data[i].size, data[i].p);
                w = maxui(1, w >> 1);
                h = maxui(1, h >> 1);
            }
        }   else    {
            for (i = 0; i < mip_cnt; i++)   {
                glTexImage3D((GLenum)type, (GLint)i, gl_internal, (GLsizei)w, (GLsizei)h,
                    (GLsizei)depth, 0, gl_fmt, gl_type, data[i].p);
                w = maxui(1, w >> 1);
                h = maxui(1, h >> 1);
            }
        }
        break;
    case gfxTextureType::TEX_CUBE:
        if (is_compressed)  {
            for (c = 0; c < 6; c++)     {
                w = width;
                h = height;
                for (i = 0; i < mip_cnt; i++)   {
                    uint didx = i + c*mip_cnt;
                    glCompressedTexImage2D(cube_targets[c], (GLint)i, (GLenum)fmt, (GLsizei)w,
                        (GLsizei)h, 0, (GLsizei)data[didx].size, data[didx].p);
                    w = maxui(1, w >> 1);
                    h = maxui(1, h >> 1);
                }
            }
        }   else    {
            for (c = 0; c < 6; c++)     {
                uint w = width;
                uint h = height;
                for (i = 0; i < mip_cnt; i++)   {
                    uint didx = i + c*mip_cnt;
                    glTexImage2D(cube_targets[c], (GLint)i, gl_internal,  (GLsizei)w, (GLsizei)h,
                        0, gl_fmt, gl_type, data[didx].p);
                    w = maxui(1, w >> 1);
                    h = maxui(1, h >> 1);
                }
            }
        }
        break;

    default:
        /* not implemented */
        ASSERT(FALSE);
        glDeleteTextures(1, &tex_id);
        return 0;
    }

    if (glGetError() != GL_NO_ERROR)    {
        glDeleteTextures(1, &tex_id);
        return 0;
    }

    return tex_id;
}

gfx_texture gfx_create_texturert_arr(uint width, uint height, uint arr_cnt,
		enum gfxFormat fmt)
{
	ASSERT(arr_cnt > 1);

	GLenum gl_fmt;
	GLenum gl_type;
    GLenum gl_internal;
	GLuint tex_id;

	glGetError();
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
	gl_fmt = texture_get_glformat(fmt, &gl_type, &gl_internal);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, gl_internal, (GLsizei)width, (GLsizei)height,
	    (GLsizei)arr_cnt, 0, gl_fmt, gl_type, NULL);

	if (glGetError() != GL_NO_ERROR)	{
		glDeleteTextures(1, &tex_id);
		return NULL;
	}

	gfx_texture obj = create_obj((uptr_t)tex_id, GFX_OBJ_TEXTURE);
	obj->desc.tex.type = gfxTextureType::TEX_2D_ARRAY;
	obj->desc.tex.fmt = fmt;
	obj->desc.tex.width = width;
	obj->desc.tex.height = height;
	obj->desc.tex.depth = arr_cnt;
	obj->desc.tex.has_alpha = texture_has_alpha(fmt);
    obj->desc.tex.size = width*height*arr_cnt*(gfx_texture_getbpp(fmt)/8);
    obj->desc.tex.is_rt = TRUE;
    obj->desc.tex.mip_cnt = 1;

    g_gfxdev.memstats.rttexture_cnt ++;
    g_gfxdev.memstats.rt_textures += obj->desc.tex.size;

	return obj;
}

gfx_texture gfx_create_texturert_cube(uint width, uint height, enum gfxFormat fmt)
{
	GLenum gl_fmt;
	GLenum gl_type;
	GLuint tex_id;
    GLenum gl_internal;
	GLenum cube_targets[] = {
			GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
			GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
			GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

	glGetError();
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
	gl_fmt = texture_get_glformat(fmt, &gl_type, &gl_internal);
	for (uint i = 0; i < 6; i++)	{
		glTexImage2D((GLenum)cube_targets[i], 0, gl_internal, (GLsizei)width, (GLsizei)height, 0,
				gl_fmt, gl_type, NULL);
	}

	if (glGetError() != GL_NO_ERROR)	{
		glDeleteTextures(1, &tex_id);
		return NULL;
	}

	gfx_texture obj = create_obj((uptr_t)tex_id, GFX_OBJ_TEXTURE);
	obj->desc.tex.type = gfxTextureType::TEX_CUBE;
	obj->desc.tex.fmt = fmt;
	obj->desc.tex.width = width;
	obj->desc.tex.height = height;
	obj->desc.tex.depth = 1;
	obj->desc.tex.has_alpha = texture_has_alpha(fmt);
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

	GLuint tex_id = (GLuint)tex->api_obj;
    if (tex_id != 0)
	    glDeleteTextures(1, &tex_id);

	destroy_obj(tex);
}

gfx_rendertarget gfx_create_rendertarget(gfx_texture* rt_textures, uint rt_cnt,
		OPTIONAL gfx_texture ds_texture)
{
	GLuint fbo_id;
    uint width;
    uint height;

    if (rt_cnt > 0) {
        width = rt_textures[0]->desc.tex.width;
        height = rt_textures[0]->desc.tex.height;
    }   else if (ds_texture != NULL)    {
        width = ds_texture->desc.tex.width;
        height = ds_texture->desc.tex.height;
    }   else    {
    	width = 0;
    	height = 0;
        ASSERT(0);
    }

	glGetError();
	glGenFramebuffers(1, &fbo_id);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_id);

	/* color - render targets */
	if (rt_cnt > 0)	{
		for (uint i = 0; i < rt_cnt; i++)	{
			glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                (GLuint)rt_textures[i]->api_obj, 0);
		}
	}   else    {
        glDrawBuffer(GL_NONE);
    }

	/* depth - render target */
	if (ds_texture != NULL)	{
		enum gfxFormat dsfmt = ds_texture->desc.tex.fmt;
		GLenum attachment = (dsfmt == gfxFormat::DEPTH24_STENCIL8) ? GL_DEPTH_STENCIL_ATTACHMENT :
				GL_DEPTH_ATTACHMENT;

		switch (ds_texture->desc.tex.type)	{
		case gfxTextureType::TEX_2D:
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
					(GLuint)ds_texture->api_obj, 0);
			break;
		case gfxTextureType::TEX_CUBE:
        case gfxTextureType::TEX_2D_ARRAY:
			glFramebufferTexture(GL_DRAW_FRAMEBUFFER, attachment, (GLuint)ds_texture->api_obj, 0);
			break;
        default:
        	ASSERT(0);
		}
	}

	if (glGetError() != GL_NO_ERROR ||
        glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
		glDeleteFramebuffers(1, &fbo_id);
		return NULL;
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	gfx_texture obj = create_obj((uptr_t)fbo_id, GFX_OBJ_RENDERTARGET);
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
	GLuint fbo_id = (GLuint)rt->api_obj;
	glDeleteFramebuffers(1, &fbo_id);
	destroy_obj(rt);
}

int texture_is_compressed(enum gfxFormat fmt)
{
	switch (fmt)	{
	case gfxFormat::BC1:
	case gfxFormat::BC1_SRGB:
	case gfxFormat::BC2:
	case gfxFormat::BC2_SRGB:
	case gfxFormat::BC3:
	case gfxFormat::BC3_SRGB:
	case gfxFormat::BC4:
	case gfxFormat::BC4_SNORM:
	case gfxFormat::BC5:
	case gfxFormat::BC5_SNORM:
		return TRUE;
	default:
		return FALSE;
	}
}

int texture_has_alpha(enum gfxFormat fmt)
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


GLenum texture_get_glformat(enum gfxFormat fmt, OUT GLenum* type, OUT GLenum* internal_fmt)
{
    *internal_fmt = fmt;
	switch (fmt)	{
	case gfxFormat::RGBA_UNORM:
        *type = GL_UNSIGNED_BYTE;
        return GL_RGBA;
	case gfxFormat::RGBA_UNORM_SRGB:
		*type = GL_UNSIGNED_BYTE;
        *internal_fmt = GL_SRGB_ALPHA;
		return GL_RGBA;
	case GFX_FORMAT_RGB_UNORM:
		*type = GL_UNSIGNED_BYTE;
		return GL_RGB;
	case gfxFormat::R32G32B32A32_FLOAT:
		*type = GL_FLOAT;
		return GL_RGBA;
	case gfxFormat::R32G32B32A32_UINT:
		*type = GL_UNSIGNED_INT;
		return GL_RGBA_INTEGER;
	case gfxFormat::R32G32B32A32_SINT:
		*type = GL_INT;
		return GL_RGBA_INTEGER;
	case gfxFormat::R32G32B32_FLOAT:
		*type = GL_FLOAT;
		return GL_RGB;
	case gfxFormat::R32G32B32_UINT:
		*type = GL_UNSIGNED_INT;
		return GL_RGB_INTEGER;
	case gfxFormat::R32G32B32_SINT:
		*type = GL_INT;
		return GL_RGB_INTEGER;
	case gfxFormat::R16G16B16A16_FLOAT:
		*type = GL_FLOAT;
		return GL_RGBA;
	case gfxFormat::R16G16B16A16_UNORM:
        *type = GL_UNSIGNED_SHORT;
        return GL_RGBA;
	case gfxFormat::R16G16B16A16_UINT:
		*type = GL_UNSIGNED_SHORT;
		return GL_RGBA_INTEGER;
	case gfxFormat::R16G16B16A16_SINT:
        *type = GL_SHORT;
        return GL_RGBA_INTEGER;
	case gfxFormat::R16G16B16A16_SNORM:
		*type = GL_SHORT;
		return GL_RGBA;
	case gfxFormat::R32G32_FLOAT:
		*type = GL_FLOAT;
		return GL_RG;
	case gfxFormat::R32G32_UINT:
		*type = GL_UNSIGNED_INT;
		return GL_RG_INTEGER;
	case gfxFormat::R32G32_SINT:
		*type = GL_INT;
		return GL_RG_INTEGER;
	case gfxFormat::R10G10B10A2_UNORM:
        *type = GL_UNSIGNED_INT_10_10_10_2;
        return GL_RGBA;
	case gfxFormat::R10G10B10A2_UINT:
		*type = GL_UNSIGNED_INT_10_10_10_2;
		return GL_RGBA_INTEGER;
	case gfxFormat::R11G11B10_FLOAT:
		*type = GL_FLOAT;
		return GL_RGB;
	case gfxFormat::R16G16_FLOAT:
		*type = GL_FLOAT;
		return GL_RG;
	case gfxFormat::R16G16_UNORM:
        *type = GL_UNSIGNED_SHORT;
        return GL_RG;
	case gfxFormat::R16G16_UINT:
		*type = GL_UNSIGNED_SHORT;
		return GL_RG_INTEGER;
	case gfxFormat::R16G16_SNORM:
        *type = GL_SHORT;
        return GL_RG_INTEGER;
	case gfxFormat::R16G16_SINT:
		*type = GL_SHORT;
		return GL_RG_INTEGER;
	case gfxFormat::R32_FLOAT:
		*type = GL_FLOAT;
		return GL_RED;
	case gfxFormat::R32_UINT:
		*type = GL_UNSIGNED_INT;
		return GL_RED_INTEGER;
	case gfxFormat::R32_SINT:
		*type = GL_INT;
		return GL_RED_INTEGER;
	case gfxFormat::R8G8_UNORM:
        *type = GL_UNSIGNED_BYTE;
        return GL_RG;
	case gfxFormat::R8G8_UINT:
		*type = GL_UNSIGNED_BYTE;
		return GL_RG_INTEGER;
	case gfxFormat::R8G8_SNORM:
        *type = GL_BYTE;
        return GL_RG;
	case gfxFormat::R8G8_SINT:
		*type = GL_BYTE;
		return GL_RG_INTEGER;
	case gfxFormat::R16_FLOAT:
		*type = GL_FLOAT;
		return GL_RED;
	case gfxFormat::R16_UNORM:
        *type = GL_UNSIGNED_SHORT;
        return GL_RED;
	case gfxFormat::R16_UINT:
		*type = GL_UNSIGNED_SHORT;
		return GL_RED_INTEGER;
	case gfxFormat::R16_SNORM:
        *type = GL_SHORT;
        return GL_RED;
	case gfxFormat::R16_SINT:
		*type = GL_SHORT;
		return GL_RED_INTEGER;
	case gfxFormat::R8_UNORM:
        *type = GL_UNSIGNED_BYTE;
        return GL_RED;
	case gfxFormat::R8_UINT:
		*type = GL_UNSIGNED_BYTE;
		return GL_RED_INTEGER;
	case gfxFormat::R8_SNORM:
        *type = GL_BYTE;
        return GL_RED;
	case gfxFormat::R8_SINT:
		*type = GL_BYTE;
		return GL_RED_INTEGER;
    case gfxFormat::DEPTH24_STENCIL8:
        *type = GL_UNSIGNED_INT_24_8;
        *internal_fmt = GL_DEPTH24_STENCIL8;
        return GL_DEPTH_STENCIL;
    case gfxFormat::DEPTH32:
        *type = GL_UNSIGNED_INT;
        *internal_fmt = GL_DEPTH_COMPONENT;
        return GL_DEPTH_COMPONENT;
    case gfxFormat::DEPTH16:
        *type = GL_UNSIGNED_SHORT;
        *internal_fmt = GL_DEPTH_COMPONENT;
        return GL_DEPTH_COMPONENT;
	default:
		*type = 0;
		return 0;
	}
}

gfx_blendstate gfx_create_blendstate(const struct gfx_blend_desc* blend)
{
	gfx_blendstate obj = create_obj(0, GFX_OBJ_BLENDSTATE);
	memcpy(&obj->desc.blend, blend, sizeof(struct gfx_blend_desc));
	return obj;
}

void gfx_destroy_blendstate(gfx_blendstate blend)
{
	destroy_obj(blend);
}

gfx_rasterstate gfx_create_rasterstate(const struct gfx_rasterizer_desc* raster)
{
	gfx_rasterstate obj = create_obj(0, GFX_OBJ_RASTERSTATE);
	memcpy(&obj->desc.raster, raster, sizeof(struct gfx_rasterizer_desc));
	return obj;
}

void gfx_destroy_rasterstate(gfx_rasterstate raster)
{
	destroy_obj(raster);
}

gfx_depthstencilstate gfx_create_depthstencilstate(const struct gfx_depthstencil_desc* ds)
{
	gfx_depthstencilstate obj = create_obj(0, GFX_OBJ_DEPTHSTENCILSTATE);
	memcpy(&obj->desc.ds, ds, sizeof(struct gfx_depthstencil_desc));
	return obj;
}

void gfx_destroy_depthstencilstate(gfx_depthstencilstate ds)
{
	destroy_obj(ds);
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

const struct gfx_gpu_memstats* gfx_get_memstats()
{
    return &g_gfxdev.memstats;
}

int gfx_check_feature(enum gfx_feature ft)
{
    switch (ft) {
    case GFX_FEATURE_THREADED_CREATES:
        return TRUE;
    /* ranged cbuffers are automatically supported by GL3+ devices */
    case GFX_FEATURE_RANGED_CBUFFERS:
        return TRUE;
    default:
        return FALSE;
    }
}

void APIENTRY gfx_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
        GLsizei length, const GLchar* message, GLvoid* user_param)
{
    printf("[Warning] OpenGL: %s (id: %d, source: %s, type: %s, severity: %s)\n", message,
        id, gfx_debug_getsource(source), gfx_debug_gettype(type),
        gfx_debug_getseverity(severity));
}

const char* gfx_debug_getseverity(GLenum severity)
{
    switch (severity)   {
    case GL_DEBUG_SEVERITY_HIGH_ARB:
        return "high";
    case GL_DEBUG_SEVERITY_MEDIUM_ARB:
        return "medium";
    case GL_DEBUG_SEVERITY_LOW_ARB:
        return "low";
    default:
        return "";
    }
}

const char* gfx_debug_getsource(GLenum source)
{
    switch (source) {
    case GL_DEBUG_SOURCE_API_ARB:
        return "api";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
        return "window-system";
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
        return "shader-compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
        return "3rdparty";
    case GL_DEBUG_SOURCE_APPLICATION_ARB:
        return "applcation";
    case GL_DEBUG_SOURCE_OTHER_ARB:
        return "other";
    default:
        return "";
    }
}

const char* gfx_debug_gettype(GLenum type)
{
    switch (type)   {
    case GL_DEBUG_TYPE_ERROR_ARB:
        return "error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
        return "deprecated";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
        return "undefined";
    case GL_DEBUG_TYPE_PORTABILITY_ARB:
        return "portability";
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:
        return "performance";
    case GL_DEBUG_TYPE_OTHER_ARB:
        return "other";
    default:
        return "";
    }
}

const char* gfx_get_driverstr()
{
    static char info[256];
    sprintf(info, "%s %s %s", glGetString(GL_RENDERER), glGetString(GL_VERSION),
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

void gfx_get_devinfo(struct GfxDevice_info* info)
{
    memset(info, 0x00, sizeof(struct GfxDevice_info));

    const char* vendor = (const char*)glGetString(GL_VENDOR);
    if (strstr(vendor, "ATI"))
        info->vendor = GFX_GPU_ATI;
    else if (strstr(vendor, "NVIDIA"))
        info->vendor = GFX_GPU_NVIDIA;
    else if (strstr(vendor, "INTEL"))
        info->vendor = GFX_GPU_INTEL;
    else
        info->vendor = GFX_GPU_UNKNOWN;
    sprintf(info->desc, "%s, version: %s, GLSL: %s",
        glGetString(GL_RENDERER), glGetString(GL_VERSION),
        glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (GLEW_ATI_meminfo)   {
        GLint vbo_free = 0;
        glGetIntegerv(VBO_FREE_MEMORY_ATI, &vbo_free);
        info->mem_avail = vbo_free;
    } else if (GLEW_NVX_gpu_memory_info)    {
        glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &info->mem_avail);
    }
}

appGfxDeviceVersion gfx_get_hwver()
{
    return g_gfxdev.ver;
}

#endif  /* _GL_ */
