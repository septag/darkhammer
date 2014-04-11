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

/**
 * Additional contribution:
 * Davide Bacchet: workdaround for mac uniform enumeration
 *                 fix for GLSL 3.2 shader compilation
 */

#if defined(_GL_)

#include "GL/glew.h"

#include "dhcore/core.h"
#include "dhcore/vec-math.h"
#include "dhcore/stack-alloc.h"

#include "gfx-shader.h"
#include "gfx-cmdqueue.h"
#include "gfx-device.h"
#include "mem-ids.h"

#define UNIFORMS_MAX	64

/*************************************************************************************************
 * types
 */
struct ALIGN16 mat3f_cm
{
    union   {
        struct {
            float m11, m21, m31, m41;    /* column #1 */
            float m12, m22, m32, m42;    /* column #2 */
            float m13, m23, m33, m43;    /* column #3 */
        };

        struct {
            float col1[4];
            float col2[4];
            float col3[4];
        };

        float    f[12];
    };
};

struct meta_cblock_tbuffer
{
    GLuint tbuff_tex;
    GLint texture_unit;
    GLint uniform_loc;
};

/*************************************************************************************************
 * inlines
 */
INLINE uint shader_find_constant(struct gfx_shader* shader, uint name_hash)
{
	struct hashtable_item* item = hashtable_fixed_find(&shader->const_bindtable, name_hash);
	if (item != NULL)
		return item->value;
	else
		return INVALID_INDEX;
}

INLINE struct mat3f_cm* mat3f_togpu(struct mat3f_cm* rm, const struct mat3f* m)
{
    rm->m11 = m->m11;   rm->m21 = m->m21;   rm->m31 = m->m31;   rm->m41 = m->m41;
    rm->m12 = m->m12;   rm->m22 = m->m22;   rm->m32 = m->m32;   rm->m42 = m->m42;
    rm->m13 = m->m13;   rm->m23 = m->m23;   rm->m33 = m->m33;   rm->m43 = m->m43;
    return rm;
}


/* according to GLSL std140 */
INLINE uint get_constant_size(enum gfx_constant_type type)
{
	switch (type)	{
	case GFX_CONSTANT_FLOAT:
		return sizeof(GLfloat);
	case GFX_CONSTANT_FLOAT2:
		return sizeof(GLfloat)*2;
	case GFX_CONSTANT_FLOAT3:
		return sizeof(GLfloat)*4;
	case GFX_CONSTANT_FLOAT4:
		return sizeof(GLfloat)*4;
	case GFX_CONSTANT_INT:
		return sizeof(GLint);
	case GFX_CONSTANT_INT2:
		return sizeof(GLint)*2;
	case GFX_CONSTANT_INT3:
		return sizeof(GLint)*4;
	case GFX_CONSTANT_INT4:
		return sizeof(GLint)*4;
	case GFX_CONSTANT_UINT:
		return sizeof(GLuint);
	case GFX_CONSTANT_MAT4x3:
		return sizeof(GLfloat)*12;
	case GFX_CONSTANT_MAT4x4:
		return sizeof(GLfloat)*16;
	default:
		return sizeof(GLfloat)*4;   /* vec4 as default */
	}
}

INLINE int uniform_check_sampler(GLuint prog_id, GLuint idx)
{
	char name[32];
	GLint size;
	GLenum type;
	glGetActiveUniform(prog_id, idx, sizeof(name), NULL, &size, &type, name);
	switch (type)	{
	case GL_SAMPLER_1D:
	case GL_SAMPLER_2D:
	case GL_SAMPLER_3D:
	case GL_SAMPLER_CUBE:
	case GL_SAMPLER_1D_SHADOW:
	case GL_SAMPLER_2D_SHADOW:
	case GL_SAMPLER_1D_ARRAY:
	case GL_SAMPLER_2D_ARRAY:
	case GL_SAMPLER_1D_ARRAY_SHADOW:
	case GL_SAMPLER_2D_ARRAY_SHADOW:
	case GL_SAMPLER_2D_MULTISAMPLE:
	case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_SAMPLER_CUBE_SHADOW:
	case GL_SAMPLER_BUFFER:
	case GL_SAMPLER_2D_RECT:
	case GL_SAMPLER_2D_RECT_SHADOW:
	case GL_INT_SAMPLER_1D:
	case GL_INT_SAMPLER_2D:
	case GL_INT_SAMPLER_3D:
	case GL_INT_SAMPLER_CUBE:
	case GL_INT_SAMPLER_1D_ARRAY:
	case GL_INT_SAMPLER_2D_ARRAY	:
	case GL_INT_SAMPLER_2D_MULTISAMPLE:
	case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_INT_SAMPLER_BUFFER:
	case GL_INT_SAMPLER_2D_RECT:
	case GL_UNSIGNED_INT_SAMPLER_1D:
	case GL_UNSIGNED_INT_SAMPLER_2D:
	case GL_UNSIGNED_INT_SAMPLER_3D:
	case GL_UNSIGNED_INT_SAMPLER_CUBE:
	case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_BUFFER:
	case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
		return TRUE;
	default:
		return FALSE;
	}
}

/*************************************************************************************************
 * fwd
 */
uint shader_gather_uniforms(GLuint prog_id, OUT GLuint uniforms[UNIFORMS_MAX]);
uint shader_gather_cbuniforms(GLuint prog_id, GLuint block_idx,
    OUT GLuint indices[UNIFORMS_MAX],
    OUT GLint offsets[UNIFORMS_MAX], OUT GLint strides[UNIFORMS_MAX],
    OUT GLint arr_sizes[UNIFORMS_MAX], OUT GLint types[UNIFORMS_MAX]);
#if defined(_OSX_)
void glGetActiveUniformName_OSX(GLuint program, GLuint uniform_idx, GLsizei buff_sz,
    GLsizei *length, char *uniform_name);
#endif

/*************************************************************************************************/
void gfx_shader_bindcblock_tbuffer(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, const struct gfx_cblock* cblock)
{
    struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable,
        cblock->name_hash);
    ASSERT(item != NULL);
    struct gfx_shader_sampler* s = &shader->samplers[item->value];

    gfx_program_setcblock_tbuffer(cmdqueue, shader->prog, GFX_SHADER_NONE, cblock->gpu_buffer,
        s->id, s->texture_unit);
}

struct gfx_cblock* gfx_shader_create_cblock(struct allocator* alloc, struct allocator* tmp_alloc,
		struct gfx_shader* shader, const char* block_name, struct gfx_sharedbuffer* shared_buff)
{
	ASSERT(shader->prog);

    /* */
    GLuint prog_id = (GLuint)shader->prog->api_obj;
    GLuint block_idx = glGetUniformBlockIndex(prog_id, block_name);
    if (block_idx == GL_INVALID_INDEX)
        return NULL;

    /* block size */
    GLint size;
    glGetActiveUniformBlockiv(prog_id, block_idx, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

    /* gather valid uniforms (no array items, struct members) */
    GLuint indices[UNIFORMS_MAX];
    GLint offsets[UNIFORMS_MAX];
    GLint strides[UNIFORMS_MAX];
    GLint arr_sizes[UNIFORMS_MAX];
    GLint types[UNIFORMS_MAX];
    uint c_cnt = shader_gather_cbuniforms(prog_id, block_idx, indices, offsets, strides,
        arr_sizes, types);
    if (c_cnt == 0) {
        ASSERT(0);  /* no vars inside cblock ?! */
        return NULL;
    }

    /* create stack allocator and calculate final size */
    struct stack_alloc stack_mem;
    struct allocator stack_alloc;

    size_t total_sz =
        sizeof(struct gfx_cblock) +
        c_cnt*sizeof(struct gfx_constant_desc) +
        size +
        hashtable_fixed_estimate_size(c_cnt);

    if (IS_FAIL(mem_stack_create(alloc, &stack_mem, total_sz, MID_GFX)))    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }
    mem_stack_bindalloc(&stack_mem, &stack_alloc);

    /* */
	struct gfx_cblock* cblock = (struct gfx_cblock*)A_ALLOC(&stack_alloc, sizeof(struct gfx_cblock),
        MID_GFX);
	ASSERT(cblock);
	memset(cblock, 0x00, sizeof(struct gfx_cblock));
	cblock->alloc = alloc;

	/* name hash */
	cblock->name_hash = hash_str(block_name);

    /* create constants */
    cblock->constants = (struct gfx_constant_desc*)A_ALLOC(&stack_alloc,
        sizeof(struct gfx_constant_desc)*c_cnt, MID_GFX);
    ASSERT(cblock->constants != NULL);
    memset(cblock->constants, 0x0, sizeof(struct gfx_constant_desc)*c_cnt);
    cblock->constant_cnt = c_cnt;

    /* also push constants into ctable */
    hashtable_fixed_create(&stack_alloc, &cblock->ctable, c_cnt, MID_GFX);

    char name[32];
    for (uint i = 0; i < c_cnt; i++)  {
        struct gfx_constant_desc* desc = &cblock->constants[i];
#if defined(_OSX_)
        glGetActiveUniformName_OSX(prog_id, indices[i], sizeof(name), NULL, name);
#else
        glGetActiveUniformName(prog_id, indices[i], sizeof(name), NULL, name);
#endif
        /* if we have an array constant (c_name[]), slice from '[' character */
        char* bracket = strchr(name, '[');
        if (bracket != NULL)
            *bracket = 0;

        strcpy(desc->name, name);
        desc->shader_idx = indices[i];
        desc->offset = (uint)offsets[i];
        desc->elem_size = get_constant_size((enum gfx_constant_type)types[i]);
        desc->arr_size = (uint)arr_sizes[i];
        desc->arr_stride = (uint)strides[i];
        desc->type = (enum gfx_constant_type)types[i];

        /* add to dictionary */
        hashtable_fixed_add(&cblock->ctable, hash_str(cblock->constants[i].name), i);
    }

	/* check what shaders is blocked used */
	GLint used;
	glGetActiveUniformBlockiv(prog_id, block_idx, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,
        &used);
	if (used == GL_TRUE)
		BIT_ADD(cblock->shader_usage, GFX_SHADERUSAGE_VS);

	glGetActiveUniformBlockiv(prog_id, block_idx, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER,
        &used);
	if (used == GL_TRUE)
		BIT_ADD(cblock->shader_usage, GFX_SHADERUSAGE_PS);

	glGetActiveUniformBlockiv(prog_id, block_idx, GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER,
        &used);
	if (used == GL_TRUE)
		BIT_ADD(cblock->shader_usage, GFX_SHADERUSAGE_GS);

	/* buffers (gpu/cpu) */
	cblock->cpu_buffer = (uint8*)A_ALLOC(&stack_alloc, size, MID_GFX);
    ASSERT(cblock->cpu_buffer);

    /* do not create gpu buffer if we have shared buffers */
    if (shared_buff == NULL)    {
	    cblock->gpu_buffer = gfx_create_buffer(GFX_BUFFER_CONSTANT, GFX_MEMHINT_DYNAMIC, size,
            NULL, 0);
	    if (cblock->gpu_buffer == NULL)		{
		    gfx_shader_destroy_cblock(cblock);
		    err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
		    return NULL;
	    }
    }
	cblock->buffer_size = size;
    cblock->end_offset = size;
    cblock->shared_buff = shared_buff;
	memset(cblock->cpu_buffer, 0x00, size);

	return cblock;
}

uint shader_gather_cbuniforms(GLuint prog_id, GLuint block_idx,
    OUT GLuint indices[UNIFORMS_MAX],
    OUT GLint offsets[UNIFORMS_MAX], OUT GLint strides[UNIFORMS_MAX],
    OUT GLint arr_sizes[UNIFORMS_MAX], OUT GLint types[UNIFORMS_MAX])
{
    char c_name[32];
    GLint uniform_cnt;
    uint cnt = 0;
    GLint last_offset = 0;
    GLint arr_cnt = 0;
    char c_lastname[32];
    c_lastname[0] = 0;
    uint member_cnt = 0;
    uint final_member_cnt = 1;
    int is_struct = FALSE;
    int zero_to_one = FALSE;

    glGetActiveUniformBlockiv(prog_id, block_idx, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniform_cnt);
    if (uniform_cnt == 0)
        return 0;

    GLint* buff = (GLint*)ALLOC(sizeof(GLint)*uniform_cnt*5, MID_GFX);
    GLint* c_indices = buff;
    GLint* c_sizes = buff + uniform_cnt;
    GLint* c_offsets = buff + uniform_cnt*2;
    GLint* c_types = buff + uniform_cnt*3;
    GLint* c_strides = buff + uniform_cnt*4;

    glGetActiveUniformBlockiv(prog_id, block_idx, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, c_indices);
    glGetActiveUniformsiv(prog_id, uniform_cnt, (GLuint*)c_indices, GL_UNIFORM_SIZE, c_sizes);
    glGetActiveUniformsiv(prog_id, uniform_cnt, (GLuint*)c_indices, GL_UNIFORM_OFFSET, c_offsets);
    glGetActiveUniformsiv(prog_id, uniform_cnt, (GLuint*)c_indices, GL_UNIFORM_TYPE, c_types);
    glGetActiveUniformsiv(prog_id, uniform_cnt, (GLuint*)c_indices, GL_UNIFORM_ARRAY_STRIDE, c_strides);


    for (uint i = 0; i < (uint)uniform_cnt && cnt < UNIFORMS_MAX; i++, member_cnt++, arr_cnt++){
#if defined(_OSX_)
        glGetActiveUniformName_OSX(prog_id, c_indices[i], sizeof(c_name), NULL, c_name);
#else
        glGetActiveUniformName(prog_id, c_indices[i], sizeof(c_name), NULL, c_name);
#endif

        /* if we have array - just keep the first one (xxxx[0])
         * also, calculate each array item stride */
        char* bracket = strchr(c_name, '[');
        char* dot = strchr(c_name, '.');
        if (bracket != NULL)
            *bracket = 0;
        if (dot != NULL)
            *dot = 0;
        int new_val = (bracket == NULL || (bracket != NULL && *(bracket+1) == '0'));
        if (new_val && !str_isequal(c_name, c_lastname)) {
            /* set array count for previous uniform */
            if (cnt > 0 && is_struct)
                arr_sizes[cnt-1] = arr_cnt/final_member_cnt;

            /* add new uniform */
            indices[cnt] = c_indices[i];
            offsets[cnt] = c_offsets[i];
            last_offset = c_offsets[i];
            if (dot == NULL)    {
                types[cnt] = c_types[i];
                is_struct = FALSE;
            }   else    {
                types[cnt] = (GLint)GFX_CONSTANT_STRUCT;
                is_struct = TRUE;
            }
            strides[cnt] = c_strides[i];   /* filled later (in arrays>1) */
            arr_sizes[cnt] = c_sizes[i]; /* can be filled later (in arrays>1) */
            arr_cnt = 0;
            member_cnt = 0;
            zero_to_one = FALSE;
            cnt ++;

            strcpy(c_lastname, c_name);
        }   else if (bracket != NULL && *(bracket+1) == '1' && !zero_to_one)    {
            /* save array stride (difference between current offset and the last one) */
            strides[cnt-1] = c_offsets[i] - last_offset;
            final_member_cnt = member_cnt;
            member_cnt = 0;
            zero_to_one = TRUE;
        }
    }

    ASSERT(cnt > 0);
    if (is_struct)
        arr_sizes[cnt-1] = arr_cnt/final_member_cnt;

    FREE(buff);
    return cnt;
}

void gfx_shader_destroy_cblock(struct gfx_cblock* cblock)
{
	struct allocator* alloc = cblock->alloc;

	if (cblock->gpu_buffer != NULL)
		gfx_destroy_buffer(cblock->gpu_buffer);

	A_ALIGNED_FREE(alloc, cblock);
}

void shader_init_cblocks(struct gfx_shader* shader)
{
	ASSERT(shader->alloc);
	ASSERT(shader->prog);

	GLuint prog_id = (GLuint)shader->prog->api_obj;
	GLint block_cnt;

	glGetProgramiv(prog_id, GL_ACTIVE_UNIFORM_BLOCKS, &block_cnt);
	if (block_cnt > 0)	{
		hashtable_fixed_create(shader->alloc, &shader->cblock_bindtable, block_cnt, MID_GFX);

		for (GLint idx = 0; idx < block_cnt; idx++)	{
			char name[32];
			glGetActiveUniformBlockName(prog_id, idx, sizeof(name), NULL, name);
			hashtable_fixed_add(&shader->cblock_bindtable, hash_str(name),
                glGetUniformBlockIndex(prog_id, name));
		}
	}
}

void shader_destroy_cblocks(struct gfx_shader* shader)
{
	hashtable_fixed_destroy(&shader->cblock_bindtable);
}

void shader_init_samplers(struct gfx_shader* shader)
{
	ASSERT(shader->alloc);
	ASSERT(shader->prog);

	GLuint prog_id = (GLuint)shader->prog->api_obj;
	uint s_cnt = 0;
	GLuint uniforms[UNIFORMS_MAX];

	/* count samplers */
    uint uniform_cnt = shader_gather_uniforms(prog_id, uniforms);
	for (uint i = 0; i < uniform_cnt; i++)		{
		if (uniform_check_sampler(prog_id, uniforms[i]))
			uniforms[s_cnt++] = uniforms[i];
	}

	if (s_cnt > 0)	{
        char name[32];

		shader->sampler_cnt = s_cnt;
		shader->samplers = (struct gfx_shader_sampler*)A_ALLOC(shader->alloc,
            sizeof(struct gfx_shader_sampler)*s_cnt, MID_GFX);
		ASSERT(shader->samplers);

		hashtable_fixed_create(shader->alloc, &shader->sampler_bindtable, s_cnt, MID_GFX);

		for (uint i = 0; i < s_cnt; i++)		{
#if defined(_OSX_)
            glGetActiveUniformName_OSX(prog_id, uniforms[i], sizeof(name), NULL, name);
#else
            glGetActiveUniformName(prog_id, uniforms[i], sizeof(name), NULL, name);
#endif
			shader->samplers[i].id = glGetUniformLocation(prog_id, name);
			shader->samplers[i].texture_unit = i;
			hashtable_fixed_add(&shader->sampler_bindtable, hash_str(name), (uint64)i);
		}
	}

}

void shader_destroy_samplers(struct gfx_shader* shader)
{
	if (shader->samplers != NULL)
		A_FREE(shader->alloc, shader->samplers);
	hashtable_fixed_destroy(&shader->sampler_bindtable);
}

void shader_init_constants(struct gfx_shader* shader)
{
	GLuint uniforms[UNIFORMS_MAX];
	GLuint prog_id = (GLuint)shader->prog->api_obj;
    uint c_cnt = 0;   /* count of constants (no samplers) */

    uint uniform_cnt = shader_gather_uniforms(prog_id, uniforms);

    /* filter out samplers */
	for (uint i = 0; i < uniform_cnt; i++) {
        if (!uniform_check_sampler(prog_id, uniforms[i]))
            uniforms[c_cnt++] = uniforms[i];
    }

	if (c_cnt > 0)	{
		char name[32];
		hashtable_fixed_create(shader->alloc, &shader->const_bindtable, c_cnt, MID_GFX);
		for (uint i = 0; i < c_cnt; i++)	{
#if defined(_OSX_)
            glGetActiveUniformName_OSX(prog_id, uniforms[i], sizeof(name), NULL, name);
#else
            glGetActiveUniformName(prog_id, uniforms[i], sizeof(name), NULL, name);
#endif
            GLint uniform_loc = glGetUniformLocation(prog_id, name);
            ASSERT(uniform_loc != -1);

            /* remove from '[' character if it's array */
            char* arr_ident = strchr(name, '[');
            if (arr_ident != NULL)
                *arr_ident = 0;

			hashtable_fixed_add(&shader->const_bindtable, hash_str(name), (uint64)uniform_loc);
		}
	}
}

void shader_destroy_constants(struct gfx_shader* shader)
{
	hashtable_fixed_destroy(&shader->const_bindtable);
}

result_t shader_init_metadata(struct gfx_shader* shader)
{
    return RET_OK;
}

void shader_destroy_metadata(struct gfx_shader* shader)
{
}

void gfx_shader_bindconstants(gfx_cmdqueue cmdqueue, struct gfx_shader* shader)
{
}

void gfx_shader_bindtexture(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, gfx_texture tex)
{
    struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable, name_hash);
    ASSERT(item != NULL);
    struct gfx_shader_sampler* s = &shader->samplers[item->value];
    gfx_program_settexture(cmdqueue, shader->prog, GFX_SHADER_NONE, tex, s->texture_unit);
}

void gfx_shader_bindsampler(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, gfx_sampler sampler)
{
    const struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable,
        name_hash);
    ASSERT(item);
    const struct gfx_shader_sampler* s = &shader->samplers[item->value];
    gfx_program_setsampler(cmdqueue, shader->prog, GFX_SHADER_NONE, sampler,
        s->id, s->texture_unit);
}

void gfx_shader_bindsamplertexture(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, gfx_sampler sampler, gfx_texture tex)
{
    struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable, name_hash);
    ASSERT(item != NULL);
    struct gfx_shader_sampler* s = &shader->samplers[item->value];
    gfx_program_settexture(cmdqueue, shader->prog, GFX_SHADER_NONE, tex, s->texture_unit);
    gfx_program_setsampler(cmdqueue, shader->prog, GFX_SHADER_NONE, sampler,
        s->id, s->texture_unit);
}

void gfx_shader_bindcblocks(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    const struct gfx_cblock** cblocks, uint cblock_cnt)
{
    for (uint i = 0; i < cblock_cnt; i++)		{
        const struct gfx_cblock* cb = cblocks[i];
        uint hash_val = cb->name_hash;
        ASSERT(cb->gpu_buffer->desc.buff.type == GFX_BUFFER_CONSTANT);

        struct hashtable_item* item = hashtable_fixed_find(&shader->cblock_bindtable, hash_val);
        if (item != NULL)	{
            gfx_program_setcblock(cmdqueue, shader->prog, GFX_SHADER_NONE, cb->gpu_buffer,
                item->value, i);
        }
    }
}

/*************************************************************************************************/
void gfx_shader_set4m(struct gfx_shader* shader, uint name_hash, const struct mat4f* m)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniformMatrix4fv((GLint)idx, 1, GL_TRUE, m->f);
}

void gfx_shader_set3m(struct gfx_shader* shader, uint name_hash, const struct mat3f* m)
{
	struct mat3f_cm mcm;
	mat3f_togpu(&mcm, m);

	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniformMatrix3x4fv((GLint)idx, 1, GL_FALSE, mcm.f);
}

void gfx_shader_set4f(struct gfx_shader* shader, uint name_hash, const float* fv)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform4f((GLint)idx, fv[0], fv[1], fv[2], fv[3]);
}

void gfx_shader_set3f(struct gfx_shader* shader, uint name_hash, const float* fv)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform3f((GLint)idx, fv[0], fv[1], fv[2]);
}

void gfx_shader_set2f(struct gfx_shader* shader, uint name_hash, const float* fv)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform2f((GLint)idx, fv[0], fv[1]);
}

void gfx_shader_setf(struct gfx_shader* shader, uint name_hash, float f)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform1f((GLint)idx, f);
}

void gfx_shader_set4i(struct gfx_shader* shader, uint name_hash, const int* nv)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform4i((GLint)idx, nv[0], nv[1], nv[2], nv[3]);
}

void gfx_shader_set3i(struct gfx_shader* shader, uint name_hash, const int* nv)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform3i((GLint)idx, nv[0], nv[1], nv[2]);
}

void gfx_shader_set3ui(struct gfx_shader* shader, uint name_hash, const uint* nv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    glUniform3ui((GLint)idx, nv[0], nv[1], nv[2]);
}

void gfx_shader_set2i(struct gfx_shader* shader, uint name_hash, const int* nv)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform2i((GLint)idx, nv[0], nv[1]);
}

void gfx_shader_seti(struct gfx_shader* shader, uint name_hash, int n)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform1i((GLint)idx, n);
}

void gfx_shader_setui(struct gfx_shader* shader, uint name_hash, uint n)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform1ui((GLint)idx, n);
}

void gfx_shader_set3mv(struct gfx_shader* shader, uint name_hash,
		const struct mat3f* mv, uint cnt)
{
	struct mat3f_cm mcm;
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);

	for (uint i = 0; i < cnt; i++)	{
		mat3f_togpu(&mcm, &mv[i]);
		glUniformMatrix3x4fv((GLint)idx, 1, GL_TRUE, mcm.f);
	}
}

void gfx_shader_set4mv(struct gfx_shader* shader, uint name_hash,
		const struct mat4f* mv, uint cnt)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniformMatrix4fv((GLint)idx, (GLsizei)cnt, GL_TRUE, mv[0].f);
}

void gfx_shader_set4fv(struct gfx_shader* shader, uint name_hash,
		const struct vec4f* vv, uint cnt)
{
	uint idx = shader_find_constant(shader, name_hash);
	ASSERT(idx != INVALID_INDEX);
	glUniform4fv((GLint)idx, (GLsizei)cnt, vv[0].f);
}

void gfx_shader_setfv(struct gfx_shader* shader, uint name_hash, const float* fv, uint cnt)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    glUniform1fv((GLint)idx, (GLsizei)cnt, fv);
}

int gfx_shader_isvalidtex(struct gfx_shader* shader, uint name_hash)
{
	return hashtable_fixed_find(&shader->sampler_bindtable, name_hash) != NULL;
}

int gfx_shader_isvalid(struct gfx_shader* shader, uint name_hash)
{
	return shader_find_constant(shader, name_hash) != INVALID_INDEX;
}


uint shader_gather_uniforms(GLuint prog_id, OUT GLuint uniforms[UNIFORMS_MAX])
{
    GLint uniform_cnt;
    char name[32];
    uint cnt = 0;

    glGetProgramiv(prog_id, GL_ACTIVE_UNIFORMS, &uniform_cnt);
    if (uniform_cnt == 0)
        return 0;

    /* uniform must not be owned by any cblock */
    GLuint* indices = (GLuint*)ALLOC(sizeof(GLuint)*uniform_cnt, MID_GFX);
    GLint* owner_blocks = (GLint*)ALLOC(sizeof(GLint)*uniform_cnt, MID_GFX);

    for (GLuint i = 0; i < (GLuint)uniform_cnt; i++)
        indices[i] = i;

    ASSERT(owner_blocks);
    glGetActiveUniformsiv(prog_id, uniform_cnt, indices, GL_UNIFORM_BLOCK_INDEX, owner_blocks);
    for (GLint i = 0; i < uniform_cnt && cnt < UNIFORMS_MAX; i++)    {
        if (owner_blocks[i] == -1)  {
            /* keep the first (xxxxx[0]) array item and ditch others */
#if defined(_OSX_)
            glGetActiveUniformName_OSX(prog_id, i, sizeof(name), NULL, name);
#else
            glGetActiveUniformName(prog_id, i, sizeof(name), NULL, name);
#endif
            char* arr_indent = strchr(name, '[');
            if (arr_indent == NULL ||
                (arr_indent != NULL && *(arr_indent + 1) == '0'))
            {
                uniforms[cnt++] = i;
            }
        }
    }
    FREE(owner_blocks);
    FREE(indices);
    return cnt;
}

#if defined(_OSX_)
/* custom func for opengl uniform name retrieving
 * Author: Davide Bacchet
 * fix for apple bug in uniform name retrieving inside blocks
 * (ref: http://renderingpipeline.com/2012/07/macos-x-opengl-driver-bugs/)
 * if the name contains a point, and it starts by 'cb_', then strip out that part. Ex:
 *       layout(std140) uniform cb_frame
 *		 {
 *			mat3x4 c_view;
 *          mat4 c_viewproj;
 *		 }
 * should retrieve as name: 'c_viewproj'
 * apple drivers instead retrieve: 'cb_frame.c_viewproj' --> convert to 'c_viewproj'
 */
void glGetActiveUniformName_OSX(GLuint program, GLuint uniform_idx, GLsizei buff_sz,
    GLsizei *length, char *uniform_name)
{
    glGetActiveUniformName(program, uniform_idx, buff_sz, length, uniform_name);
    char tmp[128];
    char* dot = strchr(uniform_name, '.');
    if (dot != NULL
        && strlen(uniform_name)>2
        && uniform_name[0]=='c' && uniform_name[1]=='b' && uniform_name[2]=='_')
    {
        strcpy(tmp, dot + 1);
        strcpy(uniform_name, tmp);
        if (length)
            *length = strlen(uniform_name);
    }
}
#endif

#endif  /* _GL_ */
