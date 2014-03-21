/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
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

#ifndef __PHXTYPES_H__
#define __PHXTYPES_H__

#include "dhcore/types.h"
#include "dhcore/vec-math.h"

#define PHX_CHILD_MAX 16

/* fwd */
struct cmp_obj;

/* */
enum phx_heightfield_fmt
{
    PHX_HEIGHFIELD_FMT_S16TM = (1<<0) /* height: 16bits, Tess-mtl(8bits: 7bits mtl, 1bit tess) */
};

enum phx_heightfield_flags
{
    PHX_HEIGHTFIELD_FLAG_NOBOUNDARYEDGES = (1<<0) /* disable collision with boundary edges */
};

struct phx_heightfield_desc
{
    uint flags; /* phx_heightfield_flags */
    enum phx_heightfield_fmt fmt;
    uint row_cnt;
    uint col_cnt;
    float thickness;
    float convex_edge_threshold;
    uint sample_stride;
    const void* samples;
};

struct phx_scene_limits
{
    uint actors_max;
    uint rigid_max; /* static rigid bodies */
    uint shapes_st_max; /* static shapes */
    uint shapes_dyn_max; /* dynamic shapes */
    uint constraints_max;
};

struct phx_scene_stats
{
    uint active_constaint_cnt;
    uint active_dyn_cnt;
    uint active_kinamatic_cnt;
    uint st_cnt;
    uint dyn_cnt;
    uint axis_solver_constraints_cnt;
};

enum phx_force_mode
{
    PHX_FORCE_NORMAL,
    PHX_FORCE_IMPULSE,
    PHX_FORCE_VELOCITY,
    PHX_FORCE_ACCELERATION
};

enum phx_obj_type
{
    PHX_OBJ_NULL = 0,
    PHX_OBJ_COLLECTION,
    PHX_OBJ_RIGID_DYN,
    PHX_OBJ_RIGID_ST,
    PHX_OBJ_CONSTRAINT,
    PHX_OBJ_ARTICULATION,
    PHX_OBJ_AGGREGATE,
    PHX_OBJ_ATTACHMENT,
    PHX_OBJ_MTL,
    PHX_OBJ_TRIANGLEMESH,
    PHX_OBJ_CONVEXMESH,
    PHX_OBJ_HEIGHTFIELD,
    PHX_OBJ_SHAPE_BOX,
    PHX_OBJ_SHAPE_CAPSULE,
    PHX_OBJ_SHAPE_SPHERE,
    PHX_OBJ_SHAPE_PLANE,
    PHX_OBJ_SHAPE_CONVEX,
    PHX_OBJ_SHAPE_TRI,
    PHX_OBJ_SHAPE_HEIGHTFIELD
};

struct phx_obj_data
{
    uptr_t api_obj; /* API specific object */
    enum phx_obj_type type;
    uint ref_cnt; /* reference count is used for multiple ref objects like materials and meshes */
    void* user_ptr;
    uint child_cnt;
    struct phx_obj_data* childs[PHX_CHILD_MAX];
};

/* opaque types */
typedef struct phx_obj_data* phx_obj;
typedef struct phx_obj_data* phx_rigid_st;
typedef struct phx_obj_data* phx_rigid_dyn;
typedef struct phx_obj_data* phx_aggregate;
typedef struct phx_obj_data* phx_mtl;
typedef struct phx_obj_data* phx_trimesh;
typedef struct phx_obj_data* phx_convexmesh;
typedef struct phx_obj_data* phx_heightfield;
typedef struct phx_obj_data* phx_shape_box;
typedef struct phx_obj_data* phx_shape_sphere;
typedef struct phx_obj_data* phx_shape_capsule;
typedef struct phx_obj_data* phx_shape_convex;
typedef struct phx_obj_data* phx_shape_plane;
typedef struct phx_obj_data* phx_shape_tri;

/* */
struct ALIGN16 phx_active_transform
{
    struct cmp_obj* obj;  /* owner object */
    struct xform3d xform_ws;  /* world-space transform */
    struct vec3f vel_lin;  /* linear velocity: updated only when simulation is updated */
    struct vec3f vel_ang;  /* angulat velocity: updated only when simulation is updated */
};

struct phx_memstats
{
    size_t buff_max;
    size_t buff_alloc;
};

enum phx_trigger_state
{
    PHX_TRIGGER_UNKNOWN = 0,
    PHX_TRIGGER_IN = 1,
    PHX_TRIGGER_OUT = 2
};

typedef void (*pfn_trigger_callback)(phx_obj trigger, phx_obj other, enum phx_trigger_state state,
    void* param);

#endif /* __PHX-TYPES_H__ */
