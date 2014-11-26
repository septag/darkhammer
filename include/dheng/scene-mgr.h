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

#ifndef __SCENEMGR_H__
#define __SCENEMGR_H__

#include "dhcore/types.h"
#include "dhcore/prims.h"

#include "cmp-types.h"
#include "engine-api.h"

#define SCENE_GLOBAL INVALID_INDEX

/* fwd*/
struct mat3f;
struct gfx_model;
struct gfx_model_instance;
struct gfx_shader;
struct Camera;
struct gfx_view_params;
struct variant;
struct gfx_model_posegpu;

/* types */
struct scn_render_model
{
	cmphandle_t model_hdl;
	int sun_shadows;	/* we have to render for sun shadow map? */
	struct gfx_model* gmodel;
    struct gfx_model_posegpu* pose; /* valid posegpu, for skinned meshes */
	struct gfx_model_instance* inst;
	uint mat_idx;
	uint bounds_idx;
	uint node_idx;	/* index to renderable node in gfx_model */
};

struct scn_render_light
{
    cmphandle_t light_hdl;
    uint bounds_idx;
    uint mat_idx;
    float intensity_mul;
};

struct scn_render_query
{
	uint obj_cnt;
	uint model_cnt;
    uint mat_cnt;
    uint light_cnt;

	struct mat3f* mats;	/* transform mats (referenced by 'scn_render_xxx' structures) */
	struct sphere* bounds;	/* bounding spehres (referenced by 'scn_render_xxx' structures) */
	struct scn_render_model* models;	/* renderable models (data is extracted from gfx_model) */
    struct scn_render_light* lights;    /* local area lights */
	struct allocator* alloc;
};

_EXTERN_BEGIN_

void scn_zero();
result_t scn_initmgr();
void scn_releasemgr();

ENGINE_API uint scn_create_scene(const char* name);
ENGINE_API void scn_destroy_scene(uint scene_id);
ENGINE_API uint scn_findscene(const char* name);

struct scn_render_query* scn_create_query(uint scene_id, struct allocator* alloc,
	const struct plane frust_planes[6], const struct gfx_view_params* params,uint flags);
struct scn_render_query* scn_create_query_csm(uint scene_id, struct allocator* alloc,
    const struct aabb* frust_bounds, const struct vec3f* dir_norm,
    const struct gfx_view_params* params);
struct scn_render_query* scn_create_query_sphere(uint scene_id, struct allocator* alloc,
    const struct sphere* sphere, const struct gfx_view_params* params);

void scn_destroy_query(struct scn_render_query* query);

void scn_create_csmquery();
void scn_destroy_csmquery();

void scn_update_spatial(uint scene_id, cmphandle_t bounds_hdl);
void scn_push_spatial(uint scene_id, cmphandle_t bounds_hdl);
void scn_pull_spatial(uint scene_id, cmphandle_t bounds_hdl);

uint scn_getphxscene(uint scene_id);

ENGINE_API struct cmp_obj* scn_create_obj(uint scene_id, const char* name, enum cmp_obj_type type);
ENGINE_API void scn_destroy_obj(struct cmp_obj* obj);

ENGINE_API uint scn_findobj(uint scene_id, const char* name);
ENGINE_API struct cmp_obj* scn_getobj(uint scene_id, uint obj_id);
ENGINE_API void scn_clear(uint scene_id);

ENGINE_API void scn_setsize(uint scene_id, const struct vec3f* minpt, const struct vec3f* maxpt);
ENGINE_API void scn_getsize(uint scene_id, OUT struct vec3f* minpt, OUT struct vec3f* maxpt);

ENGINE_API void scn_setactive(uint scene_id);
ENGINE_API uint scn_getactive();

ENGINE_API void scn_setcellsize(uint scene_id, float cell_size);
ENGINE_API float scn_getcellsize(uint scene_id);

_EXTERN_END_

#endif /* __SCENEMGR_H__ */
