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

#ifndef GFX_SHADER_H_
#define GFX_SHADER_H_

#include "dhcore/types.h"
#include "dhcore/allocator.h"
#include "dhcore/hash-table.h"
#include "dhcore/allocator.h"
#include "dhcore/vec-math.h"
#include "dhcore/hash.h"
#include "gfx-types.h"
#include "gfx-buffers.h"

/* TODO: remove un-used api */
#include "engine-api.h"

#define SHADER_HSEED	4354

#if defined(_DEBUG_)
#define SHADER_NAME(name) hash_str(#name)
#else
#include "gfx-shader-hashes.h"
#define SHADER_NAME(name) GFX_SHADERNAME_##name
#endif

/* types */
struct gfx_constant_desc
{
	char name[32];
	uint shader_idx;
	gfxUniformType type;
	uint elem_size;
	uint arr_size;
	uint arr_stride;
	uint offset;
};

enum gfx_cblock_shaderusage
{
	GFX_SHADERUSAGE_NONE = 0,
	GFX_SHADERUSAGE_VS = (1<<0),
	GFX_SHADERUSAGE_PS = (1<<1),
	GFX_SHADERUSAGE_GS = (1<<2)
};

struct gfx_shader_sampler
{
    int id;	/* id (uniform) in the shader */
    uint texture_unit;
};

struct gfx_cblock
{
	uint name_hash;
	uint shader_usage;	/* enum gfx_cblock_shaderusage */
	struct gfx_constant_desc* constants;
	uint constant_cnt;
	struct hashtable_fixed ctable;	/* name(hashed)->index in constants */
	gfx_buffer gpu_buffer;
	uint8* cpu_buffer;
	uint buffer_size;
    uint end_offset;  /* maximum offset that gpu update should apply (normally equals buffer_size) */
    struct gfx_sharedbuffer* shared_buff;  /* =NULL if shared uniform buffer is not provided */
	struct allocator* alloc;
    int is_tbuff;
};

struct gfx_shader
{
#if defined(_DEBUG_)
	char name[32];
#endif
	struct allocator* alloc;
	struct hashtable_fixed const_bindtable;	/* free constants name(hashed)->binding location in shader */
	struct hashtable_fixed cblock_bindtable;	/* blockname(hashed)->binding location in shader */
	struct hashtable_fixed sampler_bindtable; /* samplername(hashed)->index to samplers */
	struct gfx_shader_sampler* samplers;
	uint sampler_cnt;
	gfx_program prog;
    uint bindings[gfxInputElemId::COUNT];
    uint binding_cnt;
    void* meta_data;    /* meta-data is used by each graphics api differently */
};


/* fwd */
struct file_mgr;

/* */
void gfx_shader_zero();
result_t gfx_shader_initmgr(int disable_cache);
void gfx_shader_releasemgr();

/* returns 0 if shader could not be loaded */
ENGINE_API uint gfx_shader_load(const char* alias, struct allocator* alloc,
		const char* vs_filepath, const char* ps_filepath, const char* gs_filepath,
		const struct gfx_input_element_binding* bindings, uint binding_cnt,
		OPTIONAL const struct gfx_shader_define* defines, uint define_cnt,
        OPTIONAL const char* include_code);
ENGINE_API void gfx_shader_unload(uint shader_id);

/* load helpers */

/**
 * begin loading multiple shaders from one source (using preprocessor defines)
 * @param include_cnt number of include files that should go after this parameter
 * @param ... user should provide required include files if 'include_cnt > 0'
 * @see gfx_shader_add
 */
ENGINE_API void gfx_shader_beginload(struct allocator* alloc,
		const char* vs_filepath, const char* ps_filepath, const char* gs_filepath,
        uint include_cnt, ...);
ENGINE_API void gfx_shader_endload();

/**
 * adds a shader to the database based on beginload params. must be called after shader_beginload\n
 * format after define_cnt paramter:\n
 * 	[bindings], [define pairs]\n
 * 	bindings are groups of: gfx_input_element_id, var_name, vb_idx\n
 * 	define bindings are: def_name, def_value\n
 * for example we have 2 input bindings and 1 define:\n
 * 	 gfx_shader_add("test", 2, 1, GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
 * 	 GFX_INPUTELEMENT_ID_NORMAL, "vsi_norm", 1, "_DIFFUSEMAP_", "")
 * @param alias shader alias name
 * @param binding_cnt number of vertex input bindings
 * @param define_cnt number of preprocessor definitions
 * @return shader_id if successful, zero if error occured
 */
ENGINE_API uint gfx_shader_add(const char* alias, uint binding_cnt, uint define_cnt, ...);

ENGINE_API struct gfx_shader* gfx_shader_get(uint shader_id);

struct gfx_cblock* gfx_shader_create_cblock(struct allocator* alloc,
		struct allocator* tmp_alloc,
		struct gfx_shader* shader, const char* block_name, struct gfx_sharedbuffer* shared_buff);

/* create a custum cblock without gpu_buffer */
struct gfx_cblock* gfx_shader_create_cblockraw(struct allocator* alloc, const char* block_name,
        const struct gfx_constant_desc* constants, uint cnt);
struct gfx_cblock* gfx_shader_create_cblock_tbuffer(struct allocator* alloc,
    struct gfx_shader* shader, const char* tb_name, uint tb_size);
void gfx_shader_destroy_cblock(struct gfx_cblock* cblock);

void gfx_shader_bind(gfx_cmdqueue cmdqueue, struct gfx_shader* shader);
void gfx_shader_updatecblock(gfx_cmdqueue cmdqueue, struct gfx_cblock* cblock);
void gfx_shader_bindcblocks(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
		const struct gfx_cblock** cblocks, uint cblock_cnt);
void gfx_shader_bindcblock_shared(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
                                  const struct gfx_cblock* cblock,
                                  gfx_buffer buff, uint offset, uint size, uint idx);

void gfx_shader_bindsampler(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
		uint name_hash, gfx_sampler sampler);
void gfx_shader_bindtexture(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
		uint name_hash, gfx_texture tex);
void gfx_shader_bindsamplertexture(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, gfx_sampler sampler, gfx_texture tex);
void gfx_shader_bindconstants(gfx_cmdqueue cmdqueue, struct gfx_shader* shader);
void gfx_shader_bindcblock_tbuffer(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
    uint name_hash, const struct gfx_cblock* cblock);

/* constants */
int gfx_cb_isvalid(struct gfx_cblock* cb, uint name_hash);
void gfx_cb_set4m(struct gfx_cblock* cb, uint name_hash, const struct mat4f* m);
void gfx_cb_set3m(struct gfx_cblock* cb, uint name_hash, const struct mat3f* m);
void gfx_cb_set4f(struct gfx_cblock* cb, uint name_hash, const float* fv);
void gfx_cb_set3f(struct gfx_cblock* cb, uint name_hash, const float* fv);
void gfx_cb_set2f(struct gfx_cblock* cb, uint name_hash, const float* fv);
void gfx_cb_setf(struct gfx_cblock* cb, uint name_hash, float f);
void gfx_cb_set4i(struct gfx_cblock* cb, uint name_hash, const int* nv);
/* takes an array of integers (cnt = integer count not vec4f count) and put them into float4 reg */
void gfx_cb_set4ivn(struct gfx_cblock* cb, uint name_hash, const int* nv, uint cnt);
void gfx_cb_set3i(struct gfx_cblock* cb, uint name_hash, const int* nv);
void gfx_cb_set2i(struct gfx_cblock* cb, uint name_hash, const int* nv);
void gfx_cb_seti(struct gfx_cblock* cb, uint name_hash, int n);
void gfx_cb_setiv(struct gfx_cblock* cb, uint name_hash, const int* ns, uint cnt);
void gfx_cb_setui(struct gfx_cblock* cb, uint name_hash, uint n);
void gfx_cb_set3mv(struct gfx_cblock* cb, uint name_hash, const struct mat3f* mv, uint cnt);
void gfx_cb_set3mvp(struct gfx_cblock* cb, uint name_hash, const struct mat3f** mvp, uint cnt);
void gfx_cb_set4mv(struct gfx_cblock* cb, uint name_hash, const struct mat4f* mv, uint cnt);
void gfx_cb_set4fv(struct gfx_cblock* cb, uint name_hash, const struct vec4f* vv, uint cnt);
void gfx_cb_setfv(struct gfx_cblock* cb, uint name_hash, const float* fv, uint cnt);
void gfx_cb_setp(struct gfx_cblock* cb, uint name_hash, const void* sdata, uint size);
void gfx_cb_setpv(struct gfx_cblock* cb, uint name_hash, const void* sdata, uint size,
    uint cnt);
void gfx_cb_set3ui(struct gfx_cblock* cb, uint name_hash, const uint* nv);

/* _offset assigns, writes value to offset (in bytes) and sets end-offset internally
 * usually we use them we big undefined buffers like tbuffers */
void gfx_cb_setpv_offset(struct gfx_cblock* cb, uint name_hash, const void* sdata, uint size,
                         uint offset);
void gfx_cb_set3mv_offset(struct gfx_cblock* cb, uint name_hash, const struct mat3f* mats,
                          uint mat_cnt, uint offset);

/* sets end offset for cb, some CBs like texture buffers may need end offset for more optimized maps */
void gfx_cb_set_endoffset(struct gfx_cblock* cb, uint offset);

/* default uniforms/slow mode */
int gfx_shader_isvalidtex(struct gfx_shader* shader, uint name_hash);
int gfx_shader_isvalid(struct gfx_shader* shader, uint name_hash);
void gfx_shader_set4m(struct gfx_shader* shader, uint name_hash, const struct mat4f* m);
void gfx_shader_set3m(struct gfx_shader* shader, uint name_hash, const struct mat3f* m);
void gfx_shader_set4f(struct gfx_shader* shader, uint name_hash, const float* fv);
void gfx_shader_set3f(struct gfx_shader* shader, uint name_hash, const float* fv);
void gfx_shader_set2f(struct gfx_shader* shader, uint name_hash, const float* fv);
void gfx_shader_setf(struct gfx_shader* shader, uint name_hash, float f);
void gfx_shader_setfv(struct gfx_shader* shader, uint name_hash, const float* fv, uint cnt);
void gfx_shader_set4i(struct gfx_shader* shader, uint name_hash, const int* nv);
void gfx_shader_set3i(struct gfx_shader* shader, uint name_hash, const int* nv);
void gfx_shader_set2i(struct gfx_shader* shader, uint name_hash, const int* nv);
void gfx_shader_seti(struct gfx_shader* shader, uint name_hash, int n);
void gfx_shader_setui(struct gfx_shader* shader, uint name_hash, uint n);
void gfx_shader_set3mv(struct gfx_shader* shader, uint name_hash,
		const struct mat3f* mv, uint cnt);
void gfx_shader_set4mv(struct gfx_shader* shader, uint name_hash,
		const struct mat4f* mv, uint cnt);
void gfx_shader_set4fv(struct gfx_shader* shader, uint name_hash,
		const struct vec4f* vv, uint cnt);
void gfx_shader_set3ui(struct gfx_shader* shader, uint name_hash, const uint* nv);

#endif /* GFX_SHADER_H_ */
