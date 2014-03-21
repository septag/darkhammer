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
 * Note: every structure here should be 1-byte packed and using compiler variable primitives
 */

#ifndef H3D_TYPES_H_
#define H3D_TYPES_H_

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "dhcore/color.h"
#include "gfx-input-types.h"
#include "dhcore/prims.h"
#include "dhcore/color.h"

#define H3D_SIGN	0x683364	/*h3d*/
#define H3D_VERSION	0x312e30	/*1.0*/
#define H3D_VERSION_11 0x312e31 /*1.1*/
#define H3D_VERSION_12 0x312e32 /*1.2*/
#define H3D_VERSION_13 0x312e33 /*1.3*/
#pragma pack(push, 1)

enum h3d_type
{
	H3D_MESH = (1<<0),  /* h3dm files */
	H3D_ANIM = (1<<1),  /* h3da files */
    H3D_PHX = (1<<2)    /* h3dp files */
};

enum h3d_texture_type
{
	H3D_TEXTURE_DIFFUSE = 0,
	H3D_TEXTURE_NORMAL = 1,
	H3D_TEXTURE_GLOSS = 2,
	H3D_TEXTURE_ALPHAMAP = 3,
	H3D_TEXTURE_EMISSIVE = 4,
	H3D_TEXTURE_REFLECTION = 5
};

struct _GCCPACKED_ h3d_header
{
	uint sign;
	uint version;
	uint type;	/* enum h3d_type */
	uint data_offset;
};

/*************************************************************************************************
 * model
 */
struct _GCCPACKED_ h3d_geo_subset
{
	uint ib_idx;
	uint idx_cnt;
};

struct _GCCPACKED_ h3d_joint
{
	char name[32];
    float offset_mat[12];
	uint parent_idx;
};

struct _GCCPACKED_ h3d_geo
{
	uint vert_id_cnt;	/* number of vertex-ids (vertex-buffers) */
	uint vert_cnt;
	uint tri_cnt;
	uint subset_cnt;
	uint joint_cnt;
	uint bones_pervertex_max;
	bool_t ib_isui32;	/* index-buffer is 32bit integer ?*/
	uint vert_ids[GFX_INPUTELEMENT_ID_CNT];	/* type=gfx_input_element_id, count = vert_id_cnt */
    float joints_rootmat[12];

#if 0
    /* comes after in the h3d file (data) */
	struct h3d_geo_subset* subsets;
	void* indexes; /* can be uint16* or uint* */
	struct vec4f* poss;
	struct vec4f* norms;
	struct vec4f* tangents;
	struct vec4f* binormals;
	struct vec4f* blend_weights;
	struct vec4i* blend_indexes;
	struct vec2f* coords0;
	struct vec2f* coords1;
	struct vec2f* coords2;
	struct vec2f* coords3;
	struct color* colors;
	struct h3d_joint* joints;
	struct mat3f* init_pose;
#endif
};

struct _GCCPACKED_ h3d_texture
{
	uint type;	/* h3d_texture_type */
	char filepath[DH_PATH_MAX];
};

struct _GCCPACKED_ h3d_mtl
{
	float diffuse[3];
	float specular[3];
	float ambient[3];
    float emissive[3];
	float spec_exp;
	float spec_intensity;
	float opacity;
	uint texture_cnt;

#if 0
    /* comes after in the h3d file (data) */
	struct h3d_texture* textures;
#endif
};

struct _GCCPACKED_ h3d_submesh
{
	uint subset_idx;
	uint mtl_idx;
};

struct h3d_mesh
{
	uint geo_idx;
	uint submesh_cnt;

#if 0
    /* comes after in the h3d file (data) */
	struct h3d_submesh* submeshes;
#endif
};

struct _GCCPACKED_ h3d_node
{
	char name[32];
	uint mesh_idx;
	uint parent_idx;
	uint child_cnt;
    float local_xform[12];
    float bb_min[3];
    float bb_max[3];

#if 0
    /* comes after in the h3d file (data) */
	uint* child_idxs;
#endif
};

struct _GCCPACKED_ h3d_occ
{
    char name[32];
    uint vert_cnt;
    uint tri_cnt;

#if 0
    /* data which comes after in the file */
    void* indexes;
    struct vec4f* poss;
#endif
};

struct _GCCPACKED_ h3d_model
{
    uint node_cnt;
    uint mesh_cnt;
    uint geo_cnt;
    uint mtl_cnt;
    bool_t has_occ;

    uint total_childidxs; /* total child idxs of all nodes */
    uint total_submeshes; /* total count of submesheshes */
    uint total_geo_subsets;   /* total subsets of geos */
    uint total_joints;
    uint total_skeletons;
    uint total_maps;  /* total maps of all materials */
    uint occ_vert_cnt;
    uint occ_idx_cnt;
};

/*************************************************************************************************
 * anim
 */
struct _GCCPACKED_ h3d_anim_channel
{
    char bindto[32];
#if 0
    /* data comes after in the file */
    struct vec3f* poss;
    struct quat4f* rots;
#endif
};

struct _GCCPACKED_ h3d_anim_clip
{
    char name[32];
    uint start;
    uint end;
    bool_t looped;
};

struct _GCCPACKED_ h3d_anim
{
    uint fps;
    uint frame_cnt;
    uint channel_cnt;
    bool_t has_scale;
    uint clip_cnt;
    uint clips_offset;    /* points to end of anim data for clips table */

#if 0
    /* data comes after in the file */
    struct h3d_anim_channels* channels;
    struct h3d_anim_clip* clips;
#endif
};

/*************************************************************************************************
 * physics
 */
enum h3d_phx_rigidtype
{
    H3D_PHX_RIGID_ST = 1,
    H3D_PHX_RIGID_DYN = 2
};

enum h3d_phx_shapetype
{
    H3D_PHX_SHAPE_BOX = 1,
    H3D_PHX_SHAPE_SPHERE = 2,
    H3D_PHX_SHAPE_CAPSULE = 3,
    H3D_PHX_SHAPE_CONVEX = 4,
    H3D_PHX_SHAPE_TRI = 5
};

enum h3d_phx_geotype
{
    H3D_PHX_GEO_CONVEX = 1,
    H3D_PHX_GEO_TRI = 2
};

struct h3d_phx_mtl
{
    float friction_st;
    float friction_dyn;
    float restitution;
};

struct h3d_phx_geo
{
    uint type; /* h3d_phx_geotype */
    uint size;
    uint vert_cnt;
    uint tri_cnt;

#if 0
    /* comes after in file */
    void* data;
#endif
};

struct _GCCPACKED_ h3d_phx_shape
{
    uint type; /* h3d_phx_shapetype */
    float local_pos[3];
    float local_rot[4];
    uint mtl_cnt;
    bool_t ccd;

    /* shape descriptions */
    union   {
        struct {
            float hx;
            float hy;
            float hz;
        } box;

        struct {
            float radius;
        } sphere;

        struct {
            float radius;
            float half_height;
        } capsule;

        struct {
            uint id;  /* index to h3d_phx.geos */
        } mesh;

        float f[3];
    };

#if 0
    /* data comes after in the file */
    uint* mtl_ids;    /* index to h3d_phx.mtls */
#endif
};

struct _GCCPACKED_ h3d_phx_rigid
{
    char name[32];
    uint type; /* h3d_phx_rigidtype */
    float mass;
    float cmass_pos[3];  /* position of center-of-mass */
    float cmass_rot[4]; /* rotation of center-of-mass (quaternion) */
    uint shape_cnt;
    float lin_damping;
    float ang_damping;
    int positer_cnt_min;
    int veliter_cnt_min;
    bool_t gravity_disable;

#if 0
    /* data comes after in the file */
    struct h3d_phx_shape* shapes;
#endif
};

struct _GCCPACKED_ h3d_phx
{
    uint mtl_cnt;
    uint geo_cnt;
    uint rigid_cnt;

    uint total_shapes;
    uint total_shape_mtls;

#if 0
    /* dynamic data */
    struct h3d_phx_mtl* mtls;
    struct h3d_phx_geo* geos;
    struct h3d_phx_rigid* rigid;
#endif
};

#pragma pack(pop)

/**
 * vertex types
 */
struct ALIGN16 h3d_vertex_base
{
    struct vec3f pos;
    struct vec3f norm;
    struct vec2f coord;
};

struct ALIGN16 h3d_vertex_skin
{
    struct vec4i indices;
    struct vec4f weights;
};

struct ALIGN16 h3d_vertex_nmap
{
    struct vec3f tangent;
    struct vec3f binorm;
};

struct ALIGN16 h3d_vertex_extra
{
    struct vec2f coord2;
    struct color color;
};

#endif /* H3D_TYPES_H_ */
