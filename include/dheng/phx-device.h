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

#ifndef __PHXDEVICE_H__
#define __PHXDEVICE_H__

#include "phx-types.h"
#include "dhcore/prims.h"
#include "dhcore/color.h"
#include "engine-api.h"
#include "dhapp/init-params.h"

_EXTERN_BEGIN_

void phx_zerodev();
result_t phx_initdev(const struct init_params* params);
void phx_releasedev();

/* scene */
uint phx_create_scene(const struct vec3f* gravity, OPTIONAL const struct phx_scene_limits* limits);
void phx_destroy_scene(uint scene_id);
void phx_scene_getstats(uint scene_id, struct phx_scene_stats* stats);
void phx_scene_setgravity(uint scene_id, const struct vec3f* gravity);
struct vec3f* phx_scene_getgravity(uint scene_id, struct vec3f* gravity);
_EXTERN_ void phx_scene_addactor(uint scene_id, phx_obj rigid_obj);
void phx_scene_removeactor(uint scene_id, phx_obj rigid_obj);
void phx_scene_wait(uint scene_id);
void phx_scene_flush(uint scene_id);
bool_t phx_scene_check(uint scene_id);
void phx_scene_simulate(uint scene_id, float dt);
struct phx_active_transform* phx_scene_activexforms(uint scene_id, struct allocator* alloc,
    OUT uint* xform_cnt);

/* debugging: must happen between canvas3d->begin/end */
void phx_draw_rigid(phx_obj obj, const struct color* clr);

void phx_scene_setxform(uint scene_id, phx_obj obj, const struct xform3d* xf);
void phx_scene_setxform3m(uint scene_id, phx_obj obj, const struct mat3f* mat);

/* objects */
phx_rigid_st phx_create_rigid_st(const struct xform3d* pose);
phx_rigid_dyn phx_create_rigid_dyn(const struct xform3d* pose);
void phx_destroy_rigid(phx_obj rigid_obj);

phx_mtl phx_create_mtl(float friction_st, float friction_dyn, float restitution);
void phx_destroy_mtl(phx_mtl mtl);

phx_trimesh phx_create_trimesh(const void* data, bool_t make_gpugeo, struct allocator* tmp_alloc,
                               uint thread_id);
void phx_destroy_trimesh(phx_trimesh tri);

phx_convexmesh phx_create_convexmesh(const void* data, bool_t make_gpugeo,
    struct allocator* tmp_alloc, uint thread_id);
void phx_destroy_convexmesh(phx_convexmesh convex);

/* shapes */
phx_shape_box phx_create_boxshape(phx_obj rigid_obj,
    float hx, float hy, float hz, phx_mtl* mtls, uint mtl_cnt, const struct xform3d* localxf);
void phx_destroy_boxshape(phx_shape_box box);

phx_shape_sphere phx_create_sphereshape(phx_obj rigid_obj,
    float radius, phx_mtl* mtls, uint mtl_cnt, const struct xform3d* localxf);
void phx_destroy_sphereshape(phx_shape_sphere sphere);

phx_shape_capsule phx_create_capsuleshape(phx_obj rigid_obj,
    float radius, float half_height, phx_mtl* mtls, uint mtl_cnt,
    const struct xform3d* localxf);
void phx_destroy_capsuleshape(phx_shape_capsule capsule);

phx_shape_plane phx_create_planeshape(phx_obj rigid_obj, phx_mtl mtl, const struct xform3d* localxf);
void phx_destroy_planeshape(phx_shape_plane plane);

phx_shape_convex phx_create_convexshape(phx_obj rigid_obj,
    phx_convexmesh convexmesh, phx_mtl* mtls, uint mtl_cnt, const struct xform3d* localxf);
void phx_destroy_convexshape(phx_shape_convex cmshape);

phx_shape_tri phx_create_trishape(phx_rigid_st rigid_st,
    phx_trimesh trimesh, phx_mtl* mtls, uint mtl_cnt, const struct xform3d* localxf);
void phx_destroy_trishape(phx_shape_tri tmshape);

/* rigid */
void phx_rigid_setmass(phx_obj rigid_obj, float mass, OPTIONAL const struct vec3f* local_cmass);
void phx_rigid_setdensity(phx_obj rigid_obj, float density, const struct vec3f* local_cmass);
void phx_rigid_applyforce(phx_obj rigid_obj, const struct vec3f* force, enum phx_force_mode mode,
    bool_t wakeup, OPTIONAL const struct vec3f* pos);
void phx_rigid_applyforce_localpos(phx_obj rigid_obj, const struct vec3f* force,
    enum phx_force_mode mode, bool_t wakeup, OPTIONAL const struct vec3f* local_pos);
void phx_rigid_applylocalforce_localpos(phx_obj rigid_obj, const struct vec3f* local_force,
    enum phx_force_mode mode, bool_t wakeup, OPTIONAL const struct vec3f* local_pos);

void phx_rigid_applytorque(phx_obj rigid_obj, const struct vec3f* torque, enum phx_force_mode mode,
    bool_t wakeup);
void phx_rigid_clearforce(phx_obj rigid_obj, enum phx_force_mode mode, bool_t wakeup);
void phx_rigid_cleartorque(phx_obj rigid_obj, enum phx_force_mode mode, bool_t wakeup);
void phx_rigid_freeze(phx_obj rigid_obj, bool_t wakeup);
void phx_rigid_setkinematic(phx_obj rigid_obj, bool_t enable);
void phx_rigid_setkinamatic_xform(phx_obj rigid_obj, const struct xform3d* xf);
void phx_rigid_setkinamatic_xform3m(phx_obj rigid_obj, const struct mat3f* mat);
void phx_rigid_setdamping(phx_obj rigid_obj, float lin_damping, float ang_damping);
void phx_rigid_setsolveritercnt(phx_obj rigid_obj, uint8 positer_min, uint8 veliter_min);
void phx_rigid_enablegravity(phx_obj rigid_obj, bool_t enable);
void phx_rigid_setxform(uint scene_id, phx_obj rigid_obj, const struct xform3d* xf);
void phx_rigid_setxform_raw(phx_obj rigid_obj, const struct xform3d* xf);
void phx_rigid_setvelocity(phx_obj rigid_obj, const struct vec3f* vel_lin,
    const struct vec3f* vel_ang, bool_t wakeup);

/* shape */
void phx_shape_settrigger(phx_obj shape, bool_t trigger);
void phx_shape_modify_box(phx_shape_box box, float hx, float hy, float hz);
void phx_shape_setccd(phx_obj shape, bool_t enable);
void phx_shape_setpose(phx_obj shape, const struct xform3d* pose);

/* trigger */
void phx_trigger_register(uint scene_id, phx_obj trigger, pfn_trigger_callback trigger_fn,
    void* param);
void phx_trigger_unregister(uint scene_id, phx_obj trigger);

/* misc */
void phx_set_userdata(phx_obj obj, void* data);
void phx_getmemstats(struct phx_memstats* stats);

_EXTERN_END_

#endif /* __PHXDEVICE_H__ */
