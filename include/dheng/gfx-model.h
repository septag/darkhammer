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

#ifndef GFX_MODEL_H_
#define GFX_MODEL_H_

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "dhcore/prims.h"
#include "dhcore/color.h"
#include "dhcore/allocator.h"
#include "gfx-types.h"
#include "engine-api.h"

#define GFX_MODEL_MAX_MAPS	7
#define GFX_MODEL_BUFFER_BASE 0
#define GFX_MODEL_BUFFER_SKIN 1
#define GFX_MODEL_BUFFER_NMAP 2
#define GFX_MODEL_BUFFER_EXTRA 3
#define GFX_MODEL_BUFFER_CNT 4

/* fwd */
struct gfx_cblock;
struct file_mgr;
struct gfx_shader;

/* */
enum gfx_model_maptype
{
	GFX_MODEL_DIFFUSEMAP = 0,   /* specular map is embeded in diffuse-map */
	GFX_MODEL_NORMALMAP = 1,
	GFX_MODEL_GLOSSMAP = 2,
    GFX_MODEL_ALPHAMAP = 3,
	GFX_MODEL_EMISSIVEMAP = 4,
	GFX_MODEL_REFLECTIONMAP = 5
};

struct gfx_model_geosubset
{
	uint ib_idx;
	uint idx_cnt;
};

struct ALIGN16 gfx_model_joint
{
	char name[32];
    uint name_hash;
	struct mat3f offset_mat;
	uint parent_id;
};

struct gfx_model_skeleton
{
	uint joint_cnt;
	uint bones_pervertex_max;
    struct mat3f joints_rootmat;
	struct gfx_model_joint* joints;
	struct mat3f* init_pose;	/* count = joint_cnt */
};

struct gfx_model_geo
{
	uint vert_id_cnt;	/* number of vertex-buffers (ids) */
	uint vert_cnt;
	uint tri_cnt;
	uint subset_cnt;
	enum gfxIndexType ib_type;
	uint vert_ids[GFX_INPUTELEMENT_ID_CNT];

	/* renderables */
	gfx_buffer vbuffers[GFX_MODEL_BUFFER_CNT];
	gfx_buffer ibuffer;
	gfx_inputlayout inputlayout;

	/* subsets */
	struct gfx_model_geosubset* subsets;

	/* skeletal data */
	struct gfx_model_skeleton* skeleton;
};

struct gfx_model_map
{
	enum gfx_model_maptype type;
	char filepath[DH_PATH_MAX];
};

enum gfx_model_mtl_flag
{
	GFX_MODEL_MTLFLAG_MIRROR = (1<<0),
	GFX_MODEL_MTLFLAG_WATER = (1<<1),
	GFX_MODEL_MTLFLAG_TRANSPARENT = (1<<2),
    GFX_MODEL_MTLFLAG_DOUBLESIDED = (1<<3)
};

/* used as reference material to create/instance gpu materials */
struct gfx_model_mtl
{
	uint flags;   /* gfx_model_mtl_flag combination */

    struct color ambient;
	struct color diffuse;
	struct color specular;
	struct color emissive;

	float spec_exp;
	float spec_intensity;
	float opacity;

	uint map_cnt;
	struct gfx_model_map* maps;
};

/* instanced for each mesh, used in render pipeline */
struct gfx_model_posegpu
{
    const struct gfx_model_skeleton* skeleton;
	uint mat_cnt; /* this is actually the number of joints */
	struct mat3f* mats; /* final joint mats */
    struct mat3f* offset_mats; /* will be copied from skeleton data */
    struct mat3f* skin_mats;    /* result of multiplying 'mats' into skeleton's offset_mat */
};

/* instanced for each mesh, used in render pipeline */
struct gfx_model_mtlgpu
{
	reshandle_t textures[GFX_MODEL_MAX_MAPS];
	struct gfx_cblock* cb; /* mtl cblock */
	struct gfx_renderpass_item passes[GFX_RENDERPASS_MAX];
	int invalidate_cb; /* indicates that the data inside 'cb' is changed */
};

struct gfx_model_submesh
{
	uint subset_id;
	uint mtl_id;
	uint offset_idx;	/* global index (for addressing in unique-ids, see gfx_model_instance) */
};

struct gfx_model_mesh
{
	uint geo_id;
	uint submesh_cnt;
	struct gfx_model_submesh* submeshes;
};

struct ALIGN16 gfx_model_node
{
	char name[32];
    uint name_hash;
	uint mesh_id;
	uint parent_id;
	uint child_cnt;
	struct mat3f local_mat;
	struct aabb bb;
	uint* child_ids;
};

struct gfx_model_occ
{
    char name[32];
    uint tri_cnt;
    uint vert_cnt;
    uint16* indexes;
    struct vec3f* poss;
};

/* reference model data, used in loading */
struct gfx_model
{
	uint mesh_cnt;
	uint geo_cnt;
	uint mtl_cnt;
	uint node_cnt;
	uint renderable_cnt;

    struct mat3f root_mat;

	struct gfx_model_node* nodes;
	struct gfx_model_mesh* meshes;
	struct gfx_model_mtl* mtls;
	struct gfx_model_geo* geos;
    struct gfx_model_occ* occ;  /* =NULL if there is no occluder */

	uint* renderable_idxs;	/* indexes to renderable nodes */
	struct allocator* alloc;
	struct aabb bb;
};

/* instanced for each model (objects in scene), used for rendering */
struct gfx_model_instance
{
	reshandle_t model;	/* reference to model handle */
    uint pose_cnt;
	struct gfx_model_mtlgpu** mtls;	/* material instances, count = model->mtl_cnt */
	struct gfx_model_posegpu** poses; /* poses for each skinned geo or NULL, count=model->geo_cnt */
	uint* unique_ids;	/* count=mesh count(x)each mesh submesh count: unique ids r used in batcher for instancing*/
    int* alpha_flags;    /* count = renderable-node-count: indicates that each node has some kind of alpha */
	struct allocator* alloc;
};

/* API */
struct gfx_model* gfx_model_load(struct allocator* alloc, const char* h3dm_filepath,
    uint thread_id);
void gfx_model_unload(struct gfx_model* model);

struct gfx_model_instance* gfx_model_createinstance(struct allocator* alloc,
		struct allocator* tmp_alloc, reshandle_t model);
void gfx_model_destroyinstance(struct gfx_model_instance* inst);

uint gfx_model_findnode(const struct gfx_model* model, const char* name);
uint gfx_model_findjoint(const struct gfx_model_skeleton* skeleton, const char* name);

/* put values of mtl inside model into gpu-mtls of instance (no buffer updates)
 * this function should only be called if material props are changed
 */
void gfx_model_updatemtls(struct gfx_model_instance* inst);

/* put textures and constant buffers of material into gpu pipeline
 * this function should be called before submitting model to the gpu for draw
 */
void gfx_model_setmtl(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
		struct gfx_model_instance* inst, uint mtl_id);

void gfx_model_update_skin(struct gfx_model_posegpu* pose);

#endif /* GFX_MODEL_H_ */
