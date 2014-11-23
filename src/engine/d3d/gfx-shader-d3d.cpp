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

#include "dhcore/core.h"
#include "dhcore/stack-alloc.h"
#include "dhcore/task-mgr.h"

#if defined(_D3D_)

#if defined(_MSVC_)
/* win8.1 sdk inculdes some d3d types that triggeres "redifinition" warnings with external DX-SDK */
#pragma warning(disable: 4005)
#endif


#include <D3DCommon.h>
#include <D3Dcompiler.h>
#include <d3d11.h>

#include "dhcore/vec-math.h"
#include "dhcore/array.h"

#include "gfx-shader.h"
#include "gfx-cmdqueue.h"
#include "gfx-device.h"
#include "mem-ids.h"
#include "engine.h"

#define DEFAULT_CB_NAME "$Globals"

/*************************************************************************************************
 * types
 */
struct shader_constant_meta
{
#if defined(_DEBUG_)
    char name[32];
#endif
    uint name_hash;
    uint usage_cnt;
    uint global_cb_ids[GFX_PROGRAM_MAX_SHADERS];  /* indexes to meta->global_cbs */
    D3D10_SHADER_VARIABLE_CLASS cls;
    D3D10_SHADER_VARIABLE_TYPE type;
};

struct shader_cblock_meta
{
#if defined(_DEBUG_)
    char name[32];
#endif
    uint name_hash;
    uint usage_cnt;
    uint size;
    uint ids[GFX_PROGRAM_MAX_SHADERS];    /* index to the cblock/SRV(for tbuffer) in shader */
    enum gfx_shader_type shader_types[GFX_PROGRAM_MAX_SHADERS];
};

struct shader_sampler_meta
{
#if defined(_DEBUG_)
    char name[32];
#endif
    uint name_hash;
    uint usage_cnt;
    uint ids[GFX_PROGRAM_MAX_SHADERS];
    uint srv_ids[GFX_PROGRAM_MAX_SHADERS];
    enum gfx_shader_type shader_types[GFX_PROGRAM_MAX_SHADERS];
};

struct shader_texture_meta
{
#if defined(_DEBUG_)
    char name[32];
#endif
    uint name_hash;
    uint shaderbind_id;
    int used;
};

struct shader_metadata
{
    uint constant_cnt;
    uint cblock_cnt;
    uint sampler_cnt;
    struct shader_constant_meta* constants;
    struct shader_cblock_meta* cblocks;
    struct shader_sampler_meta* samplers;
    struct gfx_cblock* global_cbs[GFX_PROGRAM_MAX_SHADERS];
    uint global_cb_ids[GFX_PROGRAM_MAX_SHADERS];
};

/*************************************************************************************************
 * inlines
 */
INLINE uint shader_find_constant(struct gfx_shader* shader, uint name_hash)
{
    struct hashtable_item* item = hashtable_fixed_find(&shader->const_bindtable, name_hash);
    if (item != NULL)
        return (uint)item->value;
    else
        return INVALID_INDEX;
}

INLINE uint shader_find_sampler(uint name_hash, const struct array* samplers)
{
    const struct shader_sampler_meta* ss = (const struct shader_sampler_meta*)samplers->buffer;
    for (uint i = 0; i < (uint)samplers->item_cnt; i++) {
        if (ss[i].name_hash == name_hash)
            return i;
    }
    return INVALID_INDEX;
}

INLINE enum gfxUniformType shader_get_vartype(
    D3D10_SHADER_VARIABLE_CLASS cls, D3D10_SHADER_VARIABLE_TYPE type,
    uint row_cnt, uint col_cnt)
{
    if (cls == D3D10_SVC_SCALAR)    {
        switch (type)   {
        case D3D10_SVT_FLOAT:
            return gfxUniformType::FLOAT;
        case D3D10_SVT_INT:
            return gfxUniformType::INT;
        case D3D10_SVT_UINT:
            return gfxUniformType::UINT;
        }
    } else if (cls == D3D10_SVC_MATRIX_COLUMNS)    {
        if (type == D3D10_SVT_FLOAT && row_cnt == 4 && col_cnt == 4)
            return gfxUniformType::MAT4x4;
        else if (type == D3D10_SVT_FLOAT && row_cnt == 4 && col_cnt == 3)
            return gfxUniformType::MAT4x3;
    }   else if (cls == D3D10_SVC_VECTOR)   {
        if (type == D3D10_SVT_FLOAT)    {
            switch (col_cnt)    {
            case 4:
                return gfxUniformType::FLOAT4;
            case 3:
                return gfxUniformType::FLOAT3;
            case 2:
                return gfxUniformType::FLOAT2;
            }
        }   else if (type == D3D10_SVT_INT) {
            switch (col_cnt)    {
            case 4:
                return gfxUniformType::INT4;
            case 3:
                return gfxUniformType::INT3;
            case 2:
                return gfxUniformType::INT2;
            }
        }   else if (type == D3D10_SVT_UINT) {
            switch (col_cnt)    {
            case 4:
                return gfxUniformType::INT4;
            case 3:
                return gfxUniformType::INT3;
            case 2:
                return gfxUniformType::INT2;
            }
        }
    }   else if (cls == D3D10_SVC_STRUCT)   {
        return gfxUniformType::STRUCT;
    }

    return gfxUniformType::UNKNOWN;
}

INLINE ID3D11ShaderReflectionConstantBuffer* shader_findconstantbuffer(ID3D11ShaderReflection* refl,
    const char* block_name)
{
    D3D11_SHADER_DESC desc;
    D3D11_SHADER_BUFFER_DESC cb_desc;
    refl->GetDesc(&desc);

    for (uint i = 0; i < desc.ConstantBuffers; i++)   {
        ID3D11ShaderReflectionConstantBuffer* cb = refl->GetConstantBufferByIndex(i);
        cb->GetDesc(&cb_desc);
        if (str_isequal(cb_desc.Name, block_name))
            return cb;
    }
    return NULL;
}

/*************************************************************************************************
 * forward declarations
 */
struct gfx_cblock* shader_create_cblock(struct allocator* alloc,
        struct allocator* tmp_alloc, struct gfx_shader* shader,
        ID3D11ShaderReflectionConstantBuffer* d3d_cb, const char* name,
        struct gfx_sharedbuffer* shared_buff);
int shader_find_var(uint name_hash, D3D10_SHADER_VARIABLE_CLASS cls,
    D3D10_SHADER_VARIABLE_TYPE type, const struct array* constants, OUT uint* idx);
int shader_gather_constants(ID3D11ShaderReflection* refl, uint cb_idx, uint global_cbidx,
    INOUT struct array* constants);
int shader_gather_cbuffer(ID3D11ShaderReflection* refl, uint cb_idx,
    enum gfx_shader_type parent_shadertype, INOUT struct array* cbuffers);
int shader_gather_samplers(ID3D11ShaderReflection* refl,
    enum gfx_shader_type parent_shadertype, struct shader_texture_meta* textures,
    uint texture_cnt, INOUT struct array* samplers);
void shader_gather_textures(struct array* textures, ID3D11ShaderReflection* refl);

_EXTERN_ void shader_init_cblocks(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_cblocks(struct gfx_shader* shader);
_EXTERN_ void shader_init_samplers(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_samplers(struct gfx_shader* shader);
_EXTERN_ void shader_init_constants(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_constants(struct gfx_shader* shader);
_EXTERN_ result_t shader_init_metadata(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_metadata(struct gfx_shader* shader);

/*************************************************************************************************/
struct gfx_cblock* gfx_shader_create_cblock(struct allocator* alloc,
    struct allocator* tmp_alloc, struct gfx_shader* shader, const char* block_name,
    struct gfx_sharedbuffer* shared_buff)
{
    ASSERT(shader->prog);

    struct gfx_obj_desc* desc = &shader->prog->desc;
    for (uint i = 0; i < desc->prog.shader_cnt; i++)  {
        ID3D11ShaderReflection* refl = (ID3D11ShaderReflection*)desc->prog.d3d_reflects[i];
        ID3D11ShaderReflectionConstantBuffer* cb = shader_findconstantbuffer(refl, block_name);
        if (cb != NULL)
            return shader_create_cblock(alloc, tmp_alloc, shader, cb, block_name, shared_buff);
    }

    return NULL;
}

void gfx_shader_destroy_cblock(struct gfx_cblock* cblock)
{
    if (cblock->gpu_buffer != NULL)
        gfx_destroy_buffer(cblock->gpu_buffer);

    A_ALIGNED_FREE(cblock->alloc, cblock);
}

result_t shader_init_metadata(struct gfx_shader* shader)
{
    result_t r;
    struct shader_metadata* meta = (struct shader_metadata*)
        A_ALLOC(shader->alloc, sizeof(struct shader_metadata), MID_GFX);
    if (meta == NULL)
        return RET_OUTOFMEMORY;

    memset(meta, 0x00, sizeof(struct shader_metadata));
    shader->meta_data = meta;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);

    struct array constants;
    struct array cbuffers;
    struct array samplers;
    struct array textures;  /* contains the hash_names of textures inside each shader */

    r = arr_create(tmp_alloc, &constants, sizeof(struct shader_constant_meta), 20, 20, MID_GFX);
    if (FAILED(r))
        return RET_OUTOFMEMORY;

    r = arr_create(tmp_alloc, &cbuffers, sizeof(struct shader_cblock_meta), 10, 5, MID_GFX);
    if (FAILED(r))
        return RET_OUTOFMEMORY;

    r = arr_create(tmp_alloc, &samplers, sizeof(struct shader_sampler_meta), 20, 5, MID_GFX);
    if (FAILED(r))
        return RET_OUTOFMEMORY;

    r = arr_create(tmp_alloc, &textures, sizeof(struct shader_texture_meta), 20, 5, MID_GFX);
    if (FAILED(r))
        return RET_OUTOFMEMORY;

    D3D11_SHADER_DESC shader_desc;
    struct gfx_obj_desc* desc = &shader->prog->desc;
    for (uint i = 0; i < desc->prog.shader_cnt; i++)  {
        ID3D11ShaderReflection* refl = (ID3D11ShaderReflection*)desc->prog.d3d_reflects[i];
        refl->GetDesc(&shader_desc);

        /* cbuffers and everything inside them
         * check for global variables too
         */
        for (uint k = 0; k < shader_desc.ConstantBuffers; k++)    {
            D3D11_SHADER_BUFFER_DESC cb_desc;
            ID3D11ShaderReflectionConstantBuffer* d3d_cb = refl->GetConstantBufferByIndex(k);
            d3d_cb->GetDesc(&cb_desc);

            /* We have a global cbuffer ?! */
            if (str_isequal(cb_desc.Name, DEFAULT_CB_NAME))    {
                /* global constant-buffers */
                uint cb_idx = (uint)desc->prog.shader_types[i] - 1;
                meta->global_cbs[cb_idx] = shader_create_cblock(shader->alloc, tmp_alloc, shader,
                    d3d_cb, "", NULL);
                meta->global_cb_ids[cb_idx] = k;

                /* gather free constants */
                if (!shader_gather_constants(refl, k, cb_idx, &constants))   {
                    err_print(__FILE__, __LINE__, "hlsl link failed: duplicate constant names "
                        "with different types found");
                    return RET_FAIL;
                }
            }   else    {
                /* we have a normal cbuffer, add it to meta-database
                 * and determine which shader they belong
                 */
                if (!shader_gather_cbuffer(refl, k, desc->prog.shader_types[i], &cbuffers))    {
                    err_printf(__FILE__, __LINE__, "hlsl link failed: duplicate cbuffer name '%s' "
                        "with different sizes found", cb_desc.Name);
                    return RET_FAIL;
                }
            }
        }

        /* textures and samplers */
        arr_clear(&textures);
        shader_gather_textures(&textures, refl);
        if (!shader_gather_samplers(refl, desc->prog.shader_types[i],
            (struct shader_texture_meta*)textures.buffer, textures.item_cnt, &samplers))
        {
            err_print(__FILE__, __LINE__, "hlsl link failed: samplers "
                "doesn't have attached texture with same name (t_)");
            return RET_FAIL;
        }
    }

    /* save meta data */
    if (constants.item_cnt > 0) {
        meta->constant_cnt = constants.item_cnt;
        meta->constants = (struct shader_constant_meta*)A_ALLOC(shader->alloc,
            sizeof(struct shader_constant_meta)*constants.item_cnt, MID_GFX);
        memcpy(meta->constants, constants.buffer, constants.item_cnt*constants.item_sz);
    }
    arr_destroy(&constants);

    if (cbuffers.item_cnt > 0)  {
        meta->cblock_cnt = cbuffers.item_cnt;
        meta->cblocks = (struct shader_cblock_meta*)A_ALLOC(shader->alloc,
            sizeof(struct shader_cblock_meta)*cbuffers.item_cnt, MID_GFX);
        memcpy(meta->cblocks, cbuffers.buffer, cbuffers.item_cnt*cbuffers.item_sz);
    }
    arr_destroy(&cbuffers);

    if (samplers.item_cnt > 0)  {
        meta->sampler_cnt = samplers.item_cnt;
        meta->samplers = (struct shader_sampler_meta*)A_ALLOC(shader->alloc,
            sizeof(struct shader_sampler_meta)*samplers.item_cnt, MID_GFX);
        memcpy(meta->samplers, samplers.buffer, samplers.item_cnt*samplers.item_sz);
    }
    arr_destroy(&samplers);
    arr_destroy(&textures);
    return RET_OK;
}

void shader_destroy_metadata(struct gfx_shader* shader)
{
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    if (meta != NULL)   {
        struct allocator* alloc = shader->alloc;

        if (meta->samplers != NULL)
            A_FREE(alloc, meta->samplers);
        if (meta->cblocks != NULL)
            A_FREE(alloc, meta->cblocks);
        if (meta->constants != NULL)
            A_FREE(alloc, meta->constants);

        for (uint i = 0; i < GFX_PROGRAM_MAX_SHADERS; i++)    {
            if (meta->global_cbs[i] != NULL)
                gfx_shader_destroy_cblock(meta->global_cbs[i]);
        }

        memset(meta, 0x00, sizeof(struct shader_metadata));
        A_FREE(alloc, meta);
    }
}

/* search in constants array and check the constants in cbuffer for faulty duplicate variables
 * if it's ok, add a constant_meta to the array
 */
int shader_gather_constants(ID3D11ShaderReflection* refl, uint cb_idx, uint global_cbidx,
        INOUT struct array* constants)
{
    ID3D11ShaderReflectionConstantBuffer* d3d_cb = refl->GetConstantBufferByIndex(cb_idx);
    D3D11_SHADER_BUFFER_DESC cb_desc;

    d3d_cb->GetDesc(&cb_desc);
    for (uint i = 0; i < cb_desc.Variables; i++)  {
        ID3D11ShaderReflectionVariable* d3d_var = d3d_cb->GetVariableByIndex(i);
        D3D11_SHADER_VARIABLE_DESC var_desc;
        D3D11_SHADER_TYPE_DESC type_desc;

        d3d_var->GetDesc(&var_desc);
        d3d_var->GetType()->GetDesc(&type_desc);

        uint c_idx;
        uint name_hash = hash_str(var_desc.Name);
        if (shader_find_var(name_hash, type_desc.Class, type_desc.Type, constants, &c_idx))  {
            /* add to database of constants */
            if (c_idx == INVALID_INDEX) {
                struct shader_constant_meta* new_const =
                    (struct shader_constant_meta*)arr_add(constants);
                ASSERT(new_const);
                memset(new_const, 0x00, sizeof(struct shader_constant_meta));
#if defined(_DEBUG_)
                str_safecpy(new_const->name, sizeof(new_const->name), var_desc.Name);
#endif
                new_const->cls = type_desc.Class;
                new_const->type = type_desc.Type;
                new_const->global_cb_ids[new_const->usage_cnt++] = global_cbidx;
                new_const->name_hash = name_hash;
            }   else    {
                struct shader_constant_meta* new_const =
                    ((struct shader_constant_meta*)constants->buffer + c_idx);
                new_const->global_cb_ids[new_const->usage_cnt++] = global_cbidx;
            }
        }   else    {
            return FALSE;   /* link error */
        }
    }

    return TRUE;
}

/* returns FALSE if variable is used with different class/type
 * idx will be the index in the array of the same
 */
int shader_find_var(uint name_hash, D3D10_SHADER_VARIABLE_CLASS cls,
    D3D10_SHADER_VARIABLE_TYPE type, const struct array* constants, OUT uint* idx)
{
    *idx = INVALID_INDEX;
    uint cnt = constants->item_cnt;
    struct shader_constant_meta* cs = (struct shader_constant_meta*)constants->buffer;
    for (uint i = 0; i < cnt; i++)    {
        if (cs[i].name_hash == name_hash && (cs[i].cls != cls || cs[i].type != type))
            return FALSE;    /* variable is used with different class/type */
        else if (cs[i].name_hash == name_hash)   {
            *idx = i;
            return TRUE;
        }
    }

    return TRUE;
}

/* returns BoundResource index if cb is tbuffer */
uint shader_check_tbuffer(ID3D11ShaderReflection* refl, const char* cb_name)
{
    D3D11_SHADER_DESC shader_desc;
    D3D11_SHADER_INPUT_BIND_DESC rdesc;

    refl->GetDesc(&shader_desc);
    for (uint i = 0; i < shader_desc.BoundResources; i++) {
        refl->GetResourceBindingDesc(i, &rdesc);
        if (rdesc.Type == D3D10_SIT_TBUFFER && str_isequal(cb_name, rdesc.Name))
            return i;
    }
    return INVALID_INDEX;
}

/* returns FALSE if duplicate cbuffers with different sizes is found */
int shader_gather_cbuffer(ID3D11ShaderReflection* refl, uint cb_idx,
    enum gfx_shader_type parent_shadertype, INOUT struct array* cbuffers)
{
    ID3D11ShaderReflectionConstantBuffer* d3d_cb = refl->GetConstantBufferByIndex(cb_idx);
    D3D11_SHADER_BUFFER_DESC cb_desc;

    d3d_cb->GetDesc(&cb_desc);

    /* search in existing cbuffers */
    uint name_hash = hash_str(cb_desc.Name);
    struct shader_cblock_meta* cbs = (struct shader_cblock_meta*)cbuffers->buffer;
    for (int i = 0; i < cbuffers->item_cnt; i++) {
        if (cbs[i].name_hash == name_hash)  {
            if (cbs[i].size != cb_desc.Size)
                return FALSE;

            struct shader_cblock_meta* new_cb = (struct shader_cblock_meta*)cbuffers->buffer + i;
            new_cb->shader_types[new_cb->usage_cnt] = parent_shadertype;
            new_cb->ids[new_cb->usage_cnt] = cb_idx;
            new_cb->usage_cnt ++;
            return TRUE;
        }
    }

    /* does not exist, create a new one */
    uint tbuffer_idx = shader_check_tbuffer(refl, cb_desc.Name);

    struct shader_cblock_meta* new_cb = (struct shader_cblock_meta*)arr_add(cbuffers);
    memset(new_cb, 0x00, sizeof(struct shader_cblock_meta));
#if defined(_DEBUG_)
    str_safecpy(new_cb->name, sizeof(new_cb->name), cb_desc.Name);
#endif
    new_cb->name_hash = name_hash;
    new_cb->shader_types[new_cb->usage_cnt] = parent_shadertype;
    if (tbuffer_idx == INVALID_INDEX)   {
        new_cb->ids[new_cb->usage_cnt] = cb_idx;
    }    else    {
        D3D11_SHADER_INPUT_BIND_DESC rdesc;
        refl->GetResourceBindingDesc(tbuffer_idx, &rdesc);
        new_cb->ids[new_cb->usage_cnt] = rdesc.BindPoint;
    }
    new_cb->usage_cnt ++;
    new_cb->size = cb_desc.Size;
    return TRUE;
}

void shader_gather_textures(struct array* textures, ID3D11ShaderReflection* refl)
{
    D3D11_SHADER_DESC shader_desc;
    D3D11_SHADER_INPUT_BIND_DESC rdesc;

    refl->GetDesc(&shader_desc);
    for (uint i = 0; i < shader_desc.BoundResources; i++) {
        refl->GetResourceBindingDesc(i, &rdesc);
        if (rdesc.Type == D3D10_SIT_TEXTURE)    {
            struct shader_texture_meta* t = (struct shader_texture_meta*)arr_add(textures);
            ASSERT(t);
#if defined(_DEBUG_)
            str_safecpy(t->name, sizeof(t->name), rdesc.Name);
#endif
            t->name_hash = hash_str(rdesc.Name);
            t->shaderbind_id = rdesc.BindPoint;
            t->used = FALSE;
        }
    }
}

struct gfx_cblock* shader_create_cblock(struct allocator* alloc,
    struct allocator* tmp_alloc, struct gfx_shader* shader,
    ID3D11ShaderReflectionConstantBuffer* d3d_cb, const char* name,
    struct gfx_sharedbuffer* shared_buff)
{
    /* create stack allocator and calculate final size */
    struct stack_alloc stack_mem;
    struct allocator stack_alloc;

    struct gfx_obj_desc* desc = &shader->prog->desc;
    D3D11_SHADER_BUFFER_DESC cb_desc;
    d3d_cb->GetDesc(&cb_desc);

    size_t total_sz =
        sizeof(struct gfx_cblock) +
        cb_desc.Variables*sizeof(struct gfx_constant_desc) +
        cb_desc.Size +
        hashtable_fixed_estimate_size(cb_desc.Variables);

    if (IS_FAIL(mem_stack_create(alloc, &stack_mem, total_sz, MID_GFX)))    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }
    mem_stack_bindalloc(&stack_mem, &stack_alloc);

    struct gfx_cblock* cb = (struct gfx_cblock*)A_ALLOC(&stack_alloc, sizeof(struct gfx_cblock),
        MID_GFX);
    ASSERT(cb);
    memset(cb, 0x0, sizeof(struct gfx_cblock));
    cb->alloc = alloc;

    /* constants */
    if (cb_desc.Variables > 0)  {
        cb->constant_cnt = cb_desc.Variables;
        cb->constants = (struct gfx_constant_desc*)A_ALLOC(&stack_alloc,
            sizeof(struct gfx_constant_desc)*cb_desc.Variables, MID_GFX);
        memset(cb->constants, 0x00, sizeof(struct gfx_constant_desc)*cb_desc.Variables);

        for (uint i = 0; i < cb_desc.Variables; i++)  {
            D3D11_SHADER_VARIABLE_DESC var_desc;
            D3D11_SHADER_TYPE_DESC type_desc;

            ID3D11ShaderReflectionVariable* d3d_var = d3d_cb->GetVariableByIndex(i);
            d3d_var->GetDesc(&var_desc);
            d3d_var->GetType()->GetDesc(&type_desc);

            struct gfx_constant_desc* desc = &cb->constants[i];
            strcpy(desc->name, var_desc.Name);
            desc->shader_idx = INVALID_INDEX;
            desc->type = shader_get_vartype(type_desc.Class, type_desc.Type,
                type_desc.Rows, type_desc.Columns);
            desc->offset = var_desc.StartOffset;
            desc->arr_size = type_desc.Elements;
            desc->arr_stride = type_desc.Rows * type_desc.Columns * 4;
            uint m = (desc->arr_stride % 16);
            if (m != 0)
                desc->arr_stride += (16 - m);
            desc->elem_size = desc->arr_stride;
        }
    }   else    {
        /* no variables inside cblock ?! */
        ASSERT(0);
    }

    /* push constants into ctable */
    hashtable_fixed_create(&stack_alloc, &cb->ctable, cb_desc.Variables, MID_GFX);

    for (uint i = 0; i < cb_desc.Variables; i++)
        hashtable_fixed_add(&cb->ctable, hash_str(cb->constants[i].name), i);

    /* buffers (gpu/cpu) */
    cb->cpu_buffer = (uint8*)A_ALLOC(&stack_alloc, cb_desc.Size, MID_GFX);
    ASSERT(cb_desc.Type != D3D11_CT_TBUFFER);
    if (shared_buff == NULL)    {
        cb->gpu_buffer = gfx_create_buffer(gfxBufferType::CONSTANT, gfxMemHint::DYNAMIC, cb_desc.Size,
            NULL, 0);
        if (cb->cpu_buffer == NULL || cb->gpu_buffer == NULL)		{
            gfx_shader_destroy_cblock(cb);
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            return NULL;
        }
    }
    cb->buffer_size = cb_desc.Size;
    cb->name_hash = hash_str(name);
    cb->end_offset = cb_desc.Size;
    cb->shared_buff = shared_buff;
    memset(cb->cpu_buffer, 0x00, cb_desc.Size);

    return cb;

}

/* return FALSE if sampler does not have texture with same name (t_) in the shader */
int shader_gather_samplers(ID3D11ShaderReflection* refl,
    enum gfx_shader_type parent_shadertype, struct shader_texture_meta* textures,
    uint texture_cnt, INOUT struct array* samplers)
{
    D3D11_SHADER_DESC shader_desc;
    D3D11_SHADER_INPUT_BIND_DESC rdesc;

    refl->GetDesc(&shader_desc);

    for (uint i = 0; i < shader_desc.BoundResources; i++) {
        refl->GetResourceBindingDesc(i, &rdesc);
        if (rdesc.Type == D3D10_SIT_SAMPLER)    {
            uint name_hash = hash_str(rdesc.Name);

            /* try to find a texture with the same name only (t_) in the begining */
            char tex_name[32];
            strcpy(tex_name, rdesc.Name);
            tex_name[0] = 't';
            uint texture_hash_name = hash_str(tex_name);
            uint texture_id = INVALID_INDEX;
            for (uint k = 0; k < texture_cnt; k++)    {
                if (texture_hash_name == textures[k].name_hash) {
                    texture_id = textures[k].shaderbind_id;
                    textures[k].used = TRUE;
                    break;
                }
            }
            if (texture_id == INVALID_INDEX)
                return FALSE;

            /* check in existing samplers */
            uint cur_sampler = shader_find_sampler(name_hash, samplers);
            struct shader_sampler_meta* s;
            if (cur_sampler == INVALID_INDEX)   {
                s = (struct shader_sampler_meta*)arr_add(samplers);
                memset(s, 0x00, sizeof(struct shader_sampler_meta));
            }   else    {
                s = ((struct shader_sampler_meta*)samplers->buffer + cur_sampler);
            }
#if defined(_DEBUG_)
            str_safecpy(s->name, sizeof(s->name), rdesc.Name);
#endif
            s->name_hash = name_hash;
            s->ids[s->usage_cnt] = rdesc.BindPoint;
            s->srv_ids[s->usage_cnt] = texture_id;
            s->shader_types[s->usage_cnt] = parent_shadertype;
            s->usage_cnt ++;
        }
    }

    /* check remaining textures and see if they are not used (maybe referenced without Sample) */
    for (uint i = 0; i < texture_cnt; i++)    {
        if (!textures[i].used)  {
            uint cur_sampler = shader_find_sampler(textures[i].name_hash, samplers);
            struct shader_sampler_meta* s;
            if (cur_sampler == INVALID_INDEX)   {
                s = (struct shader_sampler_meta*)arr_add(samplers);
                memset(s, 0x00, sizeof(struct shader_sampler_meta));
            }   else    {
                s = ((struct shader_sampler_meta*)samplers->buffer + cur_sampler);
            }
#if defined(_DEBUG_)
            str_safecpy(s->name, sizeof(s->name), textures[i].name);
#endif
            s->name_hash = textures[i].name_hash;
            s->ids[s->usage_cnt] = INVALID_INDEX;
            s->srv_ids[s->usage_cnt] = textures[i].shaderbind_id;
            s->shader_types[s->usage_cnt] = parent_shadertype;
            s->usage_cnt ++;
        }
    }

    return TRUE;
}

void gfx_shader_bindtexture(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, gfx_texture tex)
{
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable, name_hash);
    ASSERT(item != NULL);
    struct shader_sampler_meta* s = &meta->samplers[item->value];
    for (uint i = 0; i < s->usage_cnt; i++)
        gfx_program_settexture(cmdqueue, shader->prog, s->shader_types[i], tex, s->srv_ids[i]);
}

void gfx_shader_bindsampler(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, gfx_sampler sampler)
{
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    const struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable,
        name_hash);
    ASSERT(item);
    const struct shader_sampler_meta* s = &meta->samplers[item->value];
    for (uint i = 0; i < s->usage_cnt; i++)
        gfx_program_setsampler(cmdqueue, shader->prog, s->shader_types[i], sampler,
            s->ids[i], s->srv_ids[i]);
}

void gfx_shader_bindsamplertexture(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, gfx_sampler sampler, gfx_texture tex)
{
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable, name_hash);
    ASSERT(item != NULL);

    struct shader_sampler_meta* s = &meta->samplers[item->value];
    for (uint i = 0; i < s->usage_cnt; i++)
        gfx_program_settexture(cmdqueue, shader->prog, s->shader_types[i], tex, s->srv_ids[i]);

    for (uint i = 0; i < s->usage_cnt; i++)   {
        gfx_program_setsampler(cmdqueue, shader->prog, s->shader_types[i], sampler,
            s->ids[i], s->srv_ids[i]);
    }
}

void gfx_shader_bindcblocks(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    const struct gfx_cblock** cblocks, uint cblock_cnt)
{
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    for (uint i = 0; i < cblock_cnt; i++)		{
        const struct gfx_cblock* cb = cblocks[i];
        uint hash_val = cb->name_hash;
        ASSERT(cb->gpu_buffer->desc.buff.type == gfxBufferType::CONSTANT);

        struct hashtable_item* item = hashtable_fixed_find(&shader->cblock_bindtable, hash_val);
        if (item != NULL)	{
            const struct shader_cblock_meta* mcb = &meta->cblocks[item->value];
            for (uint k = 0; k < mcb->usage_cnt; k++)  {
                gfx_program_setcblock(cmdqueue, shader->prog, mcb->shader_types[k],
                    cb->gpu_buffer, mcb->ids[k], i);
            }
        }
    }
}

void gfx_shader_bindcblock_tbuffer(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, const struct gfx_cblock* cblock)
{
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;

    struct hashtable_item* item = hashtable_fixed_find(&shader->sampler_bindtable, name_hash);
    ASSERT(item);
    struct shader_sampler_meta* s = &meta->samplers[item->value];
    for (uint k = 0; k < s->usage_cnt; k++)  {
        gfx_program_setcblock_tbuffer(cmdqueue, shader->prog, s->shader_types[k],
            cblock->gpu_buffer, s->srv_ids[k], 0);
    }
}

void shader_init_cblocks(struct gfx_shader* shader)
{
    ASSERT(shader->alloc);
    ASSERT(shader->prog);
    ASSERT(shader->meta_data);

    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    uint block_cnt = meta->cblock_cnt;

    if (block_cnt > 0)	{
        hashtable_fixed_create(shader->alloc, &shader->cblock_bindtable, block_cnt, MID_GFX);

        for (uint i = 0; i < block_cnt; i++)
            hashtable_fixed_add(&shader->cblock_bindtable, meta->cblocks[i].name_hash, i);
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
    ASSERT(shader->meta_data);

    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    uint sampler_cnt = meta->sampler_cnt;
    if (sampler_cnt > 0)    {
        hashtable_fixed_create(shader->alloc, &shader->sampler_bindtable, sampler_cnt, MID_GFX);
        for (uint i = 0; i < sampler_cnt; i++)
            hashtable_fixed_add(&shader->sampler_bindtable, meta->samplers[i].name_hash, i);
    }
}

void shader_destroy_samplers(struct gfx_shader* shader)
{
    hashtable_fixed_destroy(&shader->sampler_bindtable);
}

void shader_init_constants(struct gfx_shader* shader)
{
    ASSERT(shader->alloc);
    ASSERT(shader->prog);
    ASSERT(shader->meta_data);

    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    uint constant_cnt = meta->constant_cnt;
    if (constant_cnt > 0)   {
        hashtable_fixed_create(shader->alloc, &shader->const_bindtable, constant_cnt, MID_GFX);
        for (uint i = 0; i < constant_cnt; i++)
            hashtable_fixed_add(&shader->const_bindtable, meta->constants[i].name_hash, i);
    }
}

void shader_destroy_constants(struct gfx_shader* shader)
{
    hashtable_fixed_destroy(&shader->const_bindtable);
}

void gfx_shader_bindconstants(gfx_cmdqueue cmdqueue, struct gfx_shader* shader)
{
    const struct gfx_cblock* cblocks[GFX_PROGRAM_MAX_SHADERS];
    uint bind_ids[GFX_PROGRAM_MAX_SHADERS];
    enum gfx_shader_type types[GFX_PROGRAM_MAX_SHADERS];
    uint cnt = 0;

    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    for (uint i = 0; i < GFX_PROGRAM_MAX_SHADERS; i++)   {
        if (meta->global_cbs[i] != NULL)    {
            gfx_shader_updatecblock(cmdqueue, meta->global_cbs[i]);
            cblocks[cnt] = meta->global_cbs[i];
            bind_ids[cnt] = meta->global_cb_ids[i];
            types[cnt] = (enum gfx_shader_type)(i + 1);
            cnt ++;
        }
    }

    for (uint i = 0; i < cnt; i++)    {
        gfx_program_setcblock(cmdqueue, shader->prog, types[i], cblocks[i]->gpu_buffer,
            bind_ids[i], i);
    }
}


/*************************************************************************************************/
void gfx_shader_set4m(struct gfx_shader* shader, uint name_hash, const struct mat4f* m)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set4m(meta->global_cbs[c->global_cb_ids[i]], name_hash, m);
}

void gfx_shader_set3m(struct gfx_shader* shader, uint name_hash, const struct mat3f* m)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set3m(meta->global_cbs[c->global_cb_ids[i]], name_hash, m);
}

void gfx_shader_set4f(struct gfx_shader* shader, uint name_hash, const float* fv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set4f(meta->global_cbs[c->global_cb_ids[i]], name_hash, fv);
}

void gfx_shader_set3f(struct gfx_shader* shader, uint name_hash, const float* fv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set3f(meta->global_cbs[c->global_cb_ids[i]], name_hash, fv);
}

void gfx_shader_set2f(struct gfx_shader* shader, uint name_hash, const float* fv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set2f(meta->global_cbs[c->global_cb_ids[i]], name_hash, fv);
}

void gfx_shader_setf(struct gfx_shader* shader, uint name_hash, float f)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_setf(meta->global_cbs[c->global_cb_ids[i]], name_hash, f);
}

void gfx_shader_set4i(struct gfx_shader* shader, uint name_hash, const int* nv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set4i(meta->global_cbs[c->global_cb_ids[i]], name_hash, nv);
}

void gfx_shader_set3i(struct gfx_shader* shader, uint name_hash, const int* nv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set3i(meta->global_cbs[c->global_cb_ids[i]], name_hash, nv);
}

void gfx_shader_set3ui(struct gfx_shader* shader, uint name_hash, const uint* nv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set3ui(meta->global_cbs[c->global_cb_ids[i]], name_hash, nv);
}

void gfx_shader_set2i(struct gfx_shader* shader, uint name_hash, const int* nv)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set2i(meta->global_cbs[c->global_cb_ids[i]], name_hash, nv);
}

void gfx_shader_seti(struct gfx_shader* shader, uint name_hash, int n)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_seti(meta->global_cbs[c->global_cb_ids[i]], name_hash, n);
}

void gfx_shader_setui(struct gfx_shader* shader, uint name_hash, uint n)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_setui(meta->global_cbs[c->global_cb_ids[i]], name_hash, n);
}

void gfx_shader_set3mv(struct gfx_shader* shader, uint name_hash,
    const struct mat3f* mv, uint cnt)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set3mv(meta->global_cbs[c->global_cb_ids[i]], name_hash, mv, cnt);
}

void gfx_shader_set4mv(struct gfx_shader* shader, uint name_hash,
    const struct mat4f* mv, uint cnt)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set4mv(meta->global_cbs[c->global_cb_ids[i]], name_hash, mv, cnt);
}

void gfx_shader_set4fv(struct gfx_shader* shader, uint name_hash,
    const struct vec4f* vv, uint cnt)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_set4fv(meta->global_cbs[c->global_cb_ids[i]], name_hash, vv, cnt);
}

void gfx_shader_setfv(struct gfx_shader* shader, uint name_hash, const float* fv, uint cnt)
{
    uint idx = shader_find_constant(shader, name_hash);
    ASSERT(idx != INVALID_INDEX);
    struct shader_metadata* meta = (struct shader_metadata*)shader->meta_data;
    struct shader_constant_meta* c = &meta->constants[idx];
    for (uint i = 0; i < c->usage_cnt; i++)
        gfx_cb_setfv(meta->global_cbs[c->global_cb_ids[i]], name_hash, fv, cnt);
}

int gfx_shader_isvalidtex(struct gfx_shader* shader, uint name_hash)
{
    return hashtable_fixed_find(&shader->sampler_bindtable, name_hash) != NULL;
}

int gfx_shader_isvalid(struct gfx_shader* shader, uint name_hash)
{
	return shader_find_constant(shader, name_hash) != INVALID_INDEX;
}

#endif  /* _D3D_ */
