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

#include "dhcore/core.h"
#include "dhcore/vec-math.h"
#include "dhcore/file-io.h"
#include "dhcore/stack-alloc.h"
#include "dhcore/task-mgr.h"

#include "phx-prefab.h"
#include "phx-device.h"
#include "h3d-types.h"
#include "mem-ids.h"
#include "engine.h"
#include "gfx-device.h"

/*************************************************************************************************
 * types
 */
struct phx_mtl_data
{
    float friction_st;
    float friction_dyn;
    float restitution;
};

struct phx_shape_data
{
    enum phx_obj_type type; /* one of shape types */
    struct xform3d local_pose;
    uint mtl_cnt;
    bool_t ccd;

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
    };

    uint* mtl_ids;
};

struct phx_rigid_data
{
    char name[32];
    enum phx_obj_type type; /* one of rigid object types */
    float mass;
    struct xform3d cmass;
    uint shape_cnt;
    float lin_damping;
    float ang_damping;
    int positer_cnt_min;
    int veliter_cnt_min;
    bool_t gravity_disable;

    struct phx_shape_data* shapes;
};

struct phx_prefab_data
{
    struct allocator* alloc;
    uint mtl_cnt;
    uint mesh_cnt;

    phx_mtl* mtls;
    phx_obj* meshes;    /* convex/trimesh objects */
    struct phx_rigid_data* rigid;
};

/*************************************************************************************************
 * fwd declarations
 */
bool_t phx_prefab_loadshape(struct phx_shape_data* shape, file_t f, struct allocator* alloc);
struct phx_rigid_data* phx_prefab_loadrigid(file_t f, struct allocator* alloc);
phx_obj phx_prefab_loadmesh(file_t f, bool_t gpu_mesh, struct allocator* tmp_alloc, uint thread_id);
phx_mtl phx_prefab_loadmtl(file_t f);

void phx_prefab_destroyshape(struct phx_shape_data* shape, struct allocator* alloc);

/*************************************************************************************************/
phx_prefab phx_prefab_load(const char* h3dp_filepath, struct allocator* alloc, uint thread_id)
{
    struct allocator* tmp_alloc = tsk_get_tmpalloc(thread_id);
    A_SAVE(tmp_alloc);

    file_t f = fio_openmem(tmp_alloc, h3dp_filepath, FALSE, MID_PHX);
    if (f == NULL)  {
        err_printf(__FILE__, __LINE__, "load phx-prefab failed: could not open '%s'", h3dp_filepath);
        return NULL;
    }

    /* header */
    struct h3d_header header;
    fio_read(f, &header, sizeof(header), 1);
    if (header.sign != H3D_SIGN || header.version != H3D_VERSION_12)    {
        err_printf(__FILE__, __LINE__, "load phx-prefab failed: unsupported file '%s'",
            h3dp_filepath);
        fio_close(f);
        A_LOAD(tmp_alloc);
        return NULL;
    }

    if (header.type != H3D_PHX) {
        err_printf(__FILE__, __LINE__, "load phx-prefab failed: invalid h3d file-type '%s'",
            h3dp_filepath);
        fio_close(f);
        A_LOAD(tmp_alloc);
        return NULL;
    }
    fio_seek(f, SEEK_MODE_START, header.data_offset);

    /* descriptor */
    struct h3d_phx h3ddesc;
    fio_read(f, &h3ddesc, sizeof(h3ddesc), 1);

    struct stack_alloc stack_mem;
    struct allocator stack_alloc;
    size_t total_sz =
        sizeof(struct phx_prefab_data) +
        sizeof(phx_mtl)*h3ddesc.mtl_cnt +
        sizeof(phx_obj)*h3ddesc.geo_cnt +
        sizeof(struct phx_rigid_data)*h3ddesc.rigid_cnt +
        sizeof(struct phx_shape_data)*h3ddesc.total_shapes +
        sizeof(uint)*h3ddesc.total_shape_mtls;
    if (IS_FAIL(mem_stack_create(alloc, &stack_mem, total_sz, MID_GFX))) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        A_LOAD(tmp_alloc);
        return NULL;
    }
    mem_stack_bindalloc(&stack_mem, &stack_alloc);

    /* */
    struct phx_prefab_data* prefab = (struct phx_prefab_data*)
        A_ALLOC(&stack_alloc, sizeof(struct phx_prefab_data), MID_PHX);
    ASSERT(prefab);
    memset(prefab, 0x00, sizeof(struct phx_prefab_data));
    prefab->alloc = alloc;

    if (h3ddesc.mtl_cnt > 0)    {
        prefab->mtls = (phx_mtl*)A_ALLOC(&stack_alloc, sizeof(phx_mtl)*h3ddesc.mtl_cnt, MID_PHX);
        ASSERT(prefab->mtls);
        memset(prefab->mtls, 0x00, sizeof(phx_mtl)*h3ddesc.mtl_cnt);
        prefab->mtl_cnt = h3ddesc.mtl_cnt;
    }

    if (h3ddesc.geo_cnt > 0)    {
        prefab->meshes = (phx_obj*)A_ALLOC(&stack_alloc, sizeof(phx_obj)*h3ddesc.geo_cnt, MID_PHX);
        ASSERT(prefab->meshes);
        memset(prefab->meshes, 0x00, sizeof(phx_obj)*h3ddesc.geo_cnt);
        prefab->mesh_cnt = h3ddesc.geo_cnt;
    }

    /* materials */
    for (uint i = 0; i < h3ddesc.mtl_cnt; i++)    {
        prefab->mtls[i] = phx_prefab_loadmtl(f);
        if (prefab->mtls[i] == NULL)    {
            fio_close(f);
            phx_prefab_unload(prefab);
            err_print(__FILE__, __LINE__, "load phx-prefab failed: could not create materials");
            A_LOAD(tmp_alloc);
            return NULL;
        }
    }

    /* meshes (geos) */
    bool_t gpu_mesh = BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DEV);
    for (uint i = 0; i < h3ddesc.geo_cnt; i++)    {
        prefab->meshes[i] = phx_prefab_loadmesh(f, gpu_mesh, tmp_alloc, thread_id);
        if (prefab->meshes[i] == NULL)  {
            fio_close(f);
            phx_prefab_unload(prefab);
            err_print(__FILE__, __LINE__, "load phx-prefab failed: could not create meshes");
            A_LOAD(tmp_alloc);
            return NULL;
        }
    }

    /* rigid */
    ASSERT(h3ddesc.rigid_cnt == 0 || h3ddesc.rigid_cnt == 1);
    if (h3ddesc.rigid_cnt > 0)  {
        prefab->rigid = phx_prefab_loadrigid(f, &stack_alloc);
        if (prefab->rigid == NULL)  {
            phx_prefab_unload(prefab);
            fio_close(f);
            err_print(__FILE__, __LINE__, "load phx-prefab failed: could not create rigid body");
            A_LOAD(tmp_alloc);
            return NULL;
        }
    }

    fio_close(f);
    A_LOAD(tmp_alloc);

    gfx_delayed_waitforobjects(thread_id);
    gfx_delayed_fillobjects(thread_id);
    return prefab;
}

void phx_prefab_unload(phx_prefab prefab)
{
    struct phx_prefab_data* pr = (struct phx_prefab_data*)prefab;
    struct allocator* alloc = pr->alloc;

    if (pr->mtls != NULL)   {
        for (uint i = 0; i < pr->mtl_cnt; i++)    {
            if (pr->mtls[i] != NULL)
                phx_destroy_mtl(pr->mtls[i]);
        }
    }

    if (pr->meshes != NULL) {
        for (uint i = 0; i < pr->mtl_cnt; i++)    {
            if (pr->meshes[i] != NULL && pr->meshes[i]->type == PHX_OBJ_CONVEXMESH)  {
                phx_destroy_convexmesh(pr->meshes[i]);
            }   else if (pr->meshes[i] != NULL && pr->meshes[i]->type == PHX_OBJ_TRIANGLEMESH) {
                phx_destroy_trimesh(pr->meshes[i]);
            }
        }
    }

    A_ALIGNED_FREE(alloc, pr);
}

phx_obj phx_prefab_loadmesh(file_t f, bool_t gpu_mesh, struct allocator* tmp_alloc, uint thread_id)
{
    struct h3d_phx_geo h3dgeo;

    fio_read(f, &h3dgeo, sizeof(h3dgeo), 1);
    ASSERT(h3dgeo.size > 0);
    void* buff = A_ALLOC(tmp_alloc, h3dgeo.size, MID_PHX);
    if (buff == NULL)
        return NULL;
    fio_read(f, buff, h3dgeo.size, 1);

    phx_obj mesh = NULL;
    switch ((enum h3d_phx_geotype)h3dgeo.type)    {
    case H3D_PHX_GEO_CONVEX:
        mesh = phx_create_convexmesh(buff, gpu_mesh, tmp_alloc, thread_id);
        break;
    case H3D_PHX_GEO_TRI:
        mesh = phx_create_trimesh(buff, gpu_mesh, tmp_alloc, thread_id);
        break;
    }
    A_FREE(tmp_alloc, buff);
    return mesh;
}

phx_mtl phx_prefab_loadmtl(file_t f)
{
    struct h3d_phx_mtl h3dmtl;

    fio_read(f, &h3dmtl, sizeof(h3dmtl), 1);
    return phx_create_mtl(h3dmtl.friction_st, h3dmtl.friction_dyn, h3dmtl.restitution);
}

struct phx_rigid_data* phx_prefab_loadrigid(file_t f, struct allocator* alloc)
{
    struct h3d_phx_rigid h3drigid;
    fio_read(f, &h3drigid, sizeof(h3drigid), 1);

    struct phx_rigid_data* rigid = (struct phx_rigid_data*)A_ALLOC(alloc,
        sizeof(struct phx_rigid_data), MID_PHX);
    ASSERT(rigid);
    memset(rigid, 0x00, sizeof(struct phx_rigid_data));

    /* rigid props */
    switch ((enum h3d_phx_rigidtype)h3drigid.type)  {
    case H3D_PHX_RIGID_ST:
        rigid->type = PHX_OBJ_RIGID_ST;
        break;
    case H3D_PHX_RIGID_DYN:
        rigid->type = PHX_OBJ_RIGID_DYN;
        break;
    }
    strcpy(rigid->name, h3drigid.name);
    xform3d_setf_raw(&rigid->cmass,
        h3drigid.cmass_pos[0], h3drigid.cmass_pos[1], h3drigid.cmass_pos[2],
        h3drigid.cmass_rot[0], h3drigid.cmass_rot[1], h3drigid.cmass_rot[2],
        h3drigid.cmass_rot[3]);
    rigid->mass = h3drigid.mass;
    rigid->lin_damping = h3drigid.lin_damping;
    rigid->ang_damping = h3drigid.ang_damping;
    rigid->gravity_disable = h3drigid.gravity_disable;
    rigid->positer_cnt_min = h3drigid.positer_cnt_min;
    rigid->veliter_cnt_min = h3drigid.veliter_cnt_min;
    rigid->shape_cnt = h3drigid.shape_cnt;

    /* shapes */
    rigid->shapes = (struct phx_shape_data*)
        A_ALLOC(alloc, sizeof(struct phx_shape_data)*h3drigid.shape_cnt, MID_PHX);
    ASSERT(rigid->shapes);
    rigid->shape_cnt = h3drigid.shape_cnt;
    memset(rigid->shapes, 0x00, sizeof(struct phx_shape_data)*h3drigid.shape_cnt);

    bool_t r = TRUE;
    for (uint i = 0; i < h3drigid.shape_cnt; i++)
        r &= phx_prefab_loadshape(&rigid->shapes[i], f, alloc);

    if (!r)
        return NULL;

    return rigid;
}

bool_t phx_prefab_loadshape(struct phx_shape_data* shape, file_t f, struct allocator* alloc)
{
    struct h3d_phx_shape h3dshape;

    fio_read(f, &h3dshape, sizeof(h3dshape), 1);

    shape->ccd = h3dshape.ccd;
    xform3d_setf_raw(&shape->local_pose,
        h3dshape.local_pos[0], h3dshape.local_pos[1], h3dshape.local_pos[2],
        h3dshape.local_rot[0], h3dshape.local_rot[1], h3dshape.local_rot[2],
        h3dshape.local_rot[3]);
    shape->mtl_ids = (uint*)A_ALLOC(alloc, sizeof(uint)*h3dshape.mtl_cnt, MID_PHX);
    ASSERT(shape->mtl_ids);
    shape->mtl_cnt = h3dshape.mtl_cnt;
    fio_read(f, shape->mtl_ids, sizeof(uint), h3dshape.mtl_cnt);

    switch ((enum h3d_phx_shapetype)h3dshape.type)  {
    case H3D_PHX_SHAPE_BOX:
        shape->type = PHX_OBJ_SHAPE_BOX;
        shape->box.hx = h3dshape.box.hx;
        shape->box.hy = h3dshape.box.hy;
        shape->box.hz = h3dshape.box.hz;
        break;
    case H3D_PHX_SHAPE_SPHERE:
        shape->type = PHX_OBJ_SHAPE_SPHERE;
        shape->sphere.radius = h3dshape.sphere.radius;
        break;
    case H3D_PHX_SHAPE_CAPSULE:
        shape->type = PHX_OBJ_SHAPE_CAPSULE;
        shape->capsule.radius = h3dshape.capsule.radius;
        shape->capsule.half_height = h3dshape.capsule.half_height;
        break;
    case H3D_PHX_SHAPE_CONVEX:
        shape->type = PHX_OBJ_SHAPE_CONVEX;
        shape->mesh.id = h3dshape.mesh.id;
        break;
    case H3D_PHX_SHAPE_TRI:
        shape->type = PHX_OBJ_SHAPE_TRI;
        shape->mesh.id = h3dshape.mesh.id;
        break;
    }
    return TRUE;
}

phx_obj phx_createinstance(phx_prefab prefab, struct xform3d* init_pose)
{
    ASSERT(prefab);
    struct phx_prefab_data* p = (struct phx_prefab_data*)prefab;

    phx_obj obj = NULL;
    /* create rigid body */
    if (p->rigid != NULL)   {
        if (p->rigid->type == PHX_OBJ_RIGID_ST)
            obj = phx_create_rigid_st(init_pose);
        else
            obj = phx_create_rigid_dyn(init_pose);

        if (obj == NULL)    {
            log_printf(LOG_WARNING, "phx: creating rigid body instance from '%s' failed",
                p->rigid->name);
            return NULL;
        }

        /* shapes */
        for (uint i = 0; i < p->rigid->shape_cnt; i++)    {
            struct phx_shape_data* shape_desc = &p->rigid->shapes[i];
            phx_mtl mtls[PHX_CHILD_MAX];
            uint mtl_cnt = minui(shape_desc->mtl_cnt, PHX_CHILD_MAX-1);
            for (uint i = 0; i < mtl_cnt; i++)
                mtls[i] = p->mtls[shape_desc->mtl_ids[i]];
            phx_obj shape = NULL;

            switch (shape_desc->type)   {
            case PHX_OBJ_SHAPE_BOX:
                shape = phx_create_boxshape(obj, shape_desc->box.hx, shape_desc->box.hy,
                    shape_desc->box.hz, mtls, mtl_cnt, &shape_desc->local_pose);
                break;
            case PHX_OBJ_SHAPE_SPHERE:
                shape = phx_create_sphereshape(obj, shape_desc->sphere.radius, mtls,
                    mtl_cnt, &shape_desc->local_pose);
                break;
            case PHX_OBJ_SHAPE_CAPSULE:
                shape = phx_create_capsuleshape(obj, shape_desc->capsule.radius,
                    shape_desc->capsule.half_height, mtls, mtl_cnt, &shape_desc->local_pose);
                break;
            case PHX_OBJ_SHAPE_CONVEX:
                shape = phx_create_convexshape(obj, p->meshes[shape_desc->mesh.id], mtls,
                    mtl_cnt, &shape_desc->local_pose);
                break;
            case PHX_OBJ_SHAPE_TRI:
                shape = phx_create_trishape(obj, p->meshes[shape_desc->mesh.id], mtls,
                    mtl_cnt, &shape_desc->local_pose);
                break;
            default:
            	break;
            }

            if (shape == NULL)  {
                log_printf(LOG_WARNING, "phx: creating rigid body shapes from '%s' failed",
                    p->rigid->name);
                phx_destroy_rigid(obj);
                return NULL;
            }

            if (shape_desc->ccd)
                phx_shape_setccd(shape, TRUE);
        }

        /* rigid body props */
        if (p->rigid->type == PHX_OBJ_RIGID_DYN)    {
            phx_rigid_setmass(obj, p->rigid->mass, &p->rigid->cmass.p);
            phx_rigid_setdamping(obj, p->rigid->lin_damping, p->rigid->ang_damping);
            phx_rigid_setsolveritercnt(obj, p->rigid->positer_cnt_min, p->rigid->veliter_cnt_min);
            phx_rigid_enablegravity(obj, !p->rigid->gravity_disable);
        }
    }

    return obj;
}
