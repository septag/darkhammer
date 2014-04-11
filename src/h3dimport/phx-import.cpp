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

#include "PxPhysicsAPI.h"
#include "cooking/PxCooking.h"
#include "common/PxIO.h"

#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/file-io.h"
#include "dhcore/util.h"

#include "phx-import.h"
#include "h3dimport.h"
#include "math-conv.h"

#include "dheng/h3d-types.h"

#include "ezxml/ezxml.h"

#define NUMBER_DELIMITER "\n\t "

using namespace physx;

/*************************************************************************************************
 * types
 */
struct shape_ext
{
    struct h3d_phx_shape s;
    uint* mtl_ids;
};

struct rigid_ext
{
    struct h3d_phx_rigid r;
    struct shape_ext* shapes;
};

struct pgeo_ext
{
    struct h3d_phx_geo g;
    void* data;
};

struct phx_ext
{
    struct h3d_phx p;
    struct h3d_phx_mtl* mtls;
    struct pgeo_ext* geos;
    struct rigid_ext* rigid;
};

class import_phx_writer : public PxOutputStream
{
private:
    file_t mf;

public:
    import_phx_writer(file_t f) : mf(f) {}

    PxU32 write(const void* src, PxU32 cnt)
    {
        return (PxU32)fio_write(mf, src, 1, cnt);
    }
};

class import_phx_alloc : public PxAllocatorCallback
{
public:
    void* allocate(size_t size, const char* type, const char* filename, int line)
    {
        return mem_alignedalloc(size, 16, filename, line, 0);
    }

    void deallocate(void* ptr)
    {
        if (ptr != NULL)
            mem_alignedfree(ptr);
    }
};

class import_phx_log : public PxErrorCallback
{
    void reportError(PxErrorCode::Enum code, const char* msg, const char* filename, int line)
    {
        printf(TERM_BOLDGREY "Physx: %s\n" TERM_RESET, msg);
    }
};

/*************************************************************************************************
 * fwd declarations
 */
ezxml_t import_phx_findobj(ezxml_t xroot, const char* name);
struct rigid_ext* import_phx_createrigid(ezxml_t xrigid, ezxml_t xroot, struct array* geos,
    struct array* mtls);
int import_phx_createshape(struct shape_ext* shape, ezxml_t xshape, ezxml_t xroot,
    struct array* geos, struct array* mtls);
void import_phx_destroyshape(struct shape_ext* shape);
void import_phx_destroyrigid(struct rigid_ext* rigid);
int import_phx_createconvexmesh(struct pgeo_ext* geo, ezxml_t xcmesh, ezxml_t xroot);
int import_phx_createtrimesh(struct pgeo_ext* geo, ezxml_t xtrimesh, ezxml_t xroot);
void import_phx_getpose(OUT float pos[3], OUT float rot[4], ezxml_t xpose);
uint import_phx_createmtl(struct array* mtls, ezxml_t xmtlref, ezxml_t xroot);
int import_writephx(const char* filepath, const struct phx_ext* p);
void import_phx_transform_pose(INOUT float pos[3], INOUT float rot[4], const struct mat3f* mat);

/*************************************************************************************************
 * globals
 */
enum coord_type g_phx_coord;
PxCooking* g_pxcook = NULL;
struct mat3f g_root_mat;
struct mat3f g_phx_resize_mat;

/*************************************************************************************************
 * inlines
 */
INLINE void import_phx_convert3f(OUT float f[3])
{
    struct vec3f v;
    vec3_setf(&v, f[0], f[1], f[2]);
    import_convert_vec3(&v, &v, g_phx_coord);
    import_set3f(f, v.f);
}

/* returns INVALID_INDEX if not found */
INLINE uint import_phx_findmtl(const struct h3d_phx_mtl* mtl, const struct h3d_phx_mtl* mtls,
    uint mtl_cnt)
{
    for (uint i = 0; i < mtl_cnt; i++)    {
        if (mtls[i].friction_st == mtl->friction_st &&
            mtls[i].friction_dyn == mtl->friction_dyn &&
            mtls[i].restitution == mtl->restitution)
        {
            return i;
        }
    }
    return INVALID_INDEX;
}

/*************************************************************************************************/
int import_phx_list(const struct import_params* params)
{
    ezxml_t xroot = ezxml_parse_file(params->in_filepath);
    const char* err = ezxml_error(xroot);

    if (xroot == NULL || !str_isempty(err))    {
        ezxml_free(xroot);
        printf(TERM_BOLDRED "Error: RepX XML: %s\n" TERM_RESET,
            str_isempty(err) ? "File not found" : err);
        return FALSE;
    }

    if (!str_isequal(ezxml_name(xroot), "PhysX30Collection"))   {
        ezxml_free(xroot);
        printf(TERM_BOLDRED "Error: not a valid RepX file\n" TERM_RESET);
        return FALSE;
    }

    /* Rigid statics */
    ezxml_t xst = ezxml_child(xroot, "PxRigidStatic");
    while (xst != NULL)    {
        printf("%s\n", ezxml_txt(ezxml_child(xst, "Name")));
        xst = ezxml_next(xst);
    }

    /* Rigid dynamics */
    ezxml_t xdyn = ezxml_child(xroot, "PxRigidDynamic");
    while (xdyn != NULL)    {
        printf("%s\n", ezxml_txt(ezxml_child(xdyn, "Name")));
        xdyn = ezxml_next(xdyn);
    }

    ezxml_free(xroot);
    return TRUE;
}

int import_phx(const struct import_params* params)
{
    ezxml_t xroot = ezxml_parse_file(params->in_filepath);
    const char* err = ezxml_error(xroot);
    g_phx_coord = params->coord;
    mat3_setidentity(&g_root_mat);

    /* construct scale mat */
    mat3_setidentity(&g_phx_resize_mat);
    mat3_set_scalef(&g_phx_resize_mat, params->scale, params->scale, params->scale);

    if (xroot == NULL || !str_isempty(err))    {
        ezxml_free(xroot);
        printf(TERM_BOLDRED "Error: RepX XML: %s\n" TERM_RESET,
            str_isempty(err) ? "File not found" : err);
        return FALSE;
    }

    if (!str_isequal(ezxml_name(xroot), "PhysX30Collection"))   {
        ezxml_free(xroot);
        printf(TERM_BOLDRED "Error: not a valid RepX file\n" TERM_RESET);
        return FALSE;
    }

    /* find object in the RepX file */
    ezxml_t xobj = import_phx_findobj(xroot, params->name);
    if (xobj == NULL)   {
        ezxml_free(xroot);
        printf(TERM_BOLDRED "Error: object '%s' not found\n" TERM_RESET, params->name);
        return FALSE;
    }

    /* intialize physx */
    class import_phx_alloc alloc;
    class import_phx_log log;
    PxFoundation* pxbase = PxCreateFoundation(PX_PHYSICS_VERSION, alloc, log);
    if (pxbase == NULL) {
        ezxml_free(xroot);
        printf(TERM_BOLDRED "Error: could not initalize physx" TERM_RESET);
        return FALSE;
    }
    g_pxcook = PxCreateCooking(PX_PHYSICS_VERSION, *pxbase, PxCookingParams());
    if (g_pxcook == NULL)   {
        ezxml_free(xroot);
        pxbase->release();
        printf(TERM_BOLDRED "Error: could not initalize physx" TERM_RESET);
        return FALSE;
    }

    /* */
    struct phx_ext p;
    memset(&p, 0x00, sizeof(struct phx_ext));

    struct array geos, mtls;
    arr_create(mem_heap(), &geos, sizeof(struct pgeo_ext), 5, 5, 0);
    arr_create(mem_heap(), &mtls, sizeof(struct h3d_phx_mtl), 5, 5, 0);
    ASSERT(geos.buffer);
    ASSERT(mtls.buffer);
    int result = FALSE;

    /* determine the type of xobj */
    if (str_isequal(ezxml_name(xobj), "PxRigidStatic") ||
        str_isequal(ezxml_name(xobj), "PxRigidDynamic"))
    {
        p.rigid = import_phx_createrigid(xobj, xroot, &geos, &mtls);
        if (p.rigid == NULL)
            goto cleanup;
        p.p.rigid_cnt = 1;
    }

    p.geos = (struct pgeo_ext*)geos.buffer;
    p.mtls = (struct h3d_phx_mtl*)mtls.buffer;
    p.p.geo_cnt = geos.item_cnt;
    p.p.mtl_cnt = mtls.item_cnt;
    if (p.p.rigid_cnt > 0)    {
        p.p.total_shapes += p.rigid->r.shape_cnt;
        for (uint i = 0; i < p.rigid->r.shape_cnt; i++)
            p.p.total_shape_mtls += p.rigid->shapes[i].s.mtl_cnt;
    }

    result = import_writephx(params->out_filepath, &p);

    if (params->verbose)    {
        printf(TERM_WHITE);
        printf("Physics report:\n"
            "  geometry count: %d\n"
            "  material count: %d\n", p.p.geo_cnt, p.p.mtl_cnt);
        switch (p.rigid->r.type)    {
        case H3D_PHX_RIGID_DYN:
            printf( "  rigid-body: dynamic\n");
            break;
        case H3D_PHX_RIGID_ST:
            printf( "  rigid-body: static\n");
            break;
        }

        for (uint i = 0; i < geos.item_cnt; i++)  {
            struct pgeo_ext* geo = &((struct pgeo_ext*)geos.buffer)[i];
            switch (geo->g.type)    {
            case H3D_PHX_GEO_CONVEX:
                printf("  mesh #%d (convex): vertices = %d\n", i+1, geo->g.vert_cnt);
                break;
            case H3D_PHX_GEO_TRI:
                printf("  mesh #%d (tri): vertices = %d, tris = %d\n", i+1, geo->g.vert_cnt,
                    geo->g.tri_cnt);
                break;
            }
        }
        printf(TERM_RESET);
    }

    if (result)
        printf(TERM_BOLDGREEN "ok, saved: \"%s\".\n" TERM_RESET, params->out_filepath);

cleanup:
    /* cleanup */
    if (p.rigid != NULL)
        import_phx_destroyrigid(p.rigid);

    for (uint i = 0; i < geos.item_cnt; i++)  {
        struct pgeo_ext* geo = &((struct pgeo_ext*)geos.buffer)[i];
        if (geo->data != NULL)
            FREE(geo->data);
    }

    arr_destroy(&mtls);
    arr_destroy(&geos);
    ezxml_free(xroot);
    g_pxcook->release();
    pxbase->release();

    return result;
}

struct rigid_ext* import_phx_createrigid(ezxml_t xrigid, ezxml_t xroot, struct array* geos,
    struct array* mtls)
{
    struct rigid_ext* rigid = (struct rigid_ext*)ALLOC(sizeof(struct rigid_ext), 0);
    ASSERT(rigid);
    memset(rigid, 0x00, sizeof(struct rigid_ext));
    rigid->r.cmass_rot[3] = 1.0f;

    ezxml_t xpose = ezxml_child(xrigid, "GlobalPose");
    if (xpose != NULL)  {
        float pos[3];
        float quat[4];
        struct xform3d xf;
        import_phx_getpose(pos, quat, xpose);
        xform3d_setf_raw(&xf, pos[0], pos[1], pos[2], quat[0], quat[1], quat[2], quat[3]);
        xform3d_getmat(&g_root_mat, &xf);
        mat3_mul(&g_root_mat, &g_root_mat, &g_phx_resize_mat);
    }   else    {
        mat3_setidentity(&g_root_mat);
    }

    /* count shapes */
    uint shape_cnt = 0;
    ezxml_t xshapes = ezxml_child(xrigid, "Shapes");
    if (xshapes != NULL)    {
        ezxml_t xshape = ezxml_child(xshapes, "PxShape");
        while (xshape != NULL)  {
            shape_cnt ++;
            xshape = ezxml_next(xshape);
        }
    }

    if (shape_cnt == 0) {
        printf(TERM_BOLDRED "Error: No shapes for rigid body '%s'\n" TERM_RESET,
            ezxml_txt(ezxml_child(xrigid, "Name")));
        FREE(rigid);
        return NULL;
    }

    /* build and export shapes */
    rigid->shapes = (struct shape_ext*)ALLOC(sizeof(struct shape_ext)*shape_cnt, 0);
    ASSERT(rigid->shapes);
    memset(rigid->shapes, 0x00, sizeof(struct shape_ext)*shape_cnt);

    ezxml_t xshape = ezxml_child(xshapes, "PxShape");
    while (xshape != NULL)  {
        if (!import_phx_createshape(&rigid->shapes[rigid->r.shape_cnt], xshape, xroot, geos, mtls))
        {
            import_phx_destroyrigid(rigid);
            return NULL;
        }

        xshape = ezxml_next(xshape);
        rigid->r.shape_cnt ++;
    }

    /* rigid body properties */
    if (str_isequal(ezxml_name(xrigid), "PxRigidStatic"))
        rigid->r.type = H3D_PHX_RIGID_ST;
    else if (str_isequal(ezxml_name(xrigid), "PxRigidDynamic"))
        rigid->r.type = H3D_PHX_RIGID_DYN;

    const char* name = ezxml_txt(ezxml_child(xrigid, "Name"));
    str_safecpy(rigid->r.name, sizeof(rigid->r.name), name);
    rigid->r.mass = str_tofl32(ezxml_txt(ezxml_child(xrigid, "Mass")));
    ezxml_t xcmass = ezxml_child(xrigid, "CMassLocalPose");
    if (xcmass != NULL) {
        import_phx_getpose(rigid->r.cmass_pos, rigid->r.cmass_rot, xcmass);
        import_phx_transform_pose(rigid->r.cmass_pos, rigid->r.cmass_rot, &g_root_mat);
    }
    rigid->r.lin_damping = str_tofl32(ezxml_txt(ezxml_child(xrigid, "LinearDamping")));
    rigid->r.ang_damping = str_tofl32(ezxml_txt(ezxml_child(xrigid, "AngularDamping")));
    ezxml_t xsolver = ezxml_child(xrigid, "SolverIterationCounts");
    if (xsolver != NULL)    {
        rigid->r.positer_cnt_min = str_toint32(ezxml_txt(ezxml_child(xsolver, "minPositionIters")));
        rigid->r.veliter_cnt_min = str_toint32(ezxml_txt(ezxml_child(xsolver, "minVelocityIters")));
    }
    ezxml_t xflags = ezxml_child(xrigid, "ActorFlags");
    if (xflags != NULL) {
        rigid->r.gravity_disable = strstr(ezxml_txt(xflags), "eDISABLE_GRAVITY") != NULL;
    }

    return rigid;
}

ezxml_t import_phx_findobj(ezxml_t xroot, const char* name)
{
    /* Rigid statics */
    ezxml_t xst = ezxml_child(xroot, "PxRigidStatic");
    while (xst != NULL)    {
        if (str_isequal_nocase(name, ezxml_txt(ezxml_child(xst, "Name"))))
            return xst;
        xst = ezxml_next(xst);
    }

    /* Rigid dynamics */
    ezxml_t xdyn = ezxml_child(xroot, "PxRigidDynamic");
    while (xdyn != NULL)    {
        if (str_isequal_nocase(name, ezxml_txt(ezxml_child(xdyn, "Name"))))
            return xdyn;
        xdyn = ezxml_next(xdyn);
    }

    return NULL;
}

int import_phx_createshape(struct shape_ext* shape, ezxml_t xshape, ezxml_t xroot,
    struct array* geos, struct array* mtls)
{
    /* geometry */
    /* determine shape geometry */
    ezxml_t xgeo = ezxml_child(xshape, "Geometry");
    if (xgeo == NULL)
        return FALSE;
    shape->s.local_rot[3] = 1.0f;

    ezxml_t xgeodesc;
    if ((xgeodesc = ezxml_child(xgeo, "PxBoxGeometry")) != NULL)    {
        shape->s.type = H3D_PHX_SHAPE_BOX;
        sscanf(ezxml_txt(ezxml_child(xgeodesc, "HalfExtents")), "%f %f %f",
            &shape->s.box.hx, &shape->s.box.hy, &shape->s.box.hz);
        import_phx_convert3f(shape->s.f);
    }   else if ((xgeodesc = ezxml_child(xgeo, "PxSphereGeometry")) != NULL)    {
        shape->s.type = H3D_PHX_SHAPE_SPHERE;
        shape->s.sphere.radius = str_tofl32(ezxml_txt(ezxml_child(xgeodesc, "Radius")));
    }   else if ((xgeodesc = ezxml_child(xgeo, "PxCapsuleGeometry")) != NULL)   {
        shape->s.type = H3D_PHX_SHAPE_CAPSULE;
        shape->s.capsule.radius = str_tofl32(ezxml_txt(ezxml_child(xgeodesc, "Radius")));
        shape->s.capsule.half_height = str_tofl32(ezxml_txt(ezxml_child(xgeodesc, "HalfHeight")));
    }   else if ((xgeodesc = ezxml_child(xgeo, "PxConvexMeshGeometry")) != NULL)    {
        shape->s.type = H3D_PHX_SHAPE_CONVEX;
        struct pgeo_ext* geo = (struct pgeo_ext*)arr_add(geos);
        ASSERT(geo);
        if (!import_phx_createconvexmesh(geo, ezxml_child(xgeodesc, "ConvexMesh"), xroot))
            return FALSE;
        shape->s.mesh.id = geos->item_cnt - 1;
    }   else if ((xgeodesc = ezxml_child(xgeo, "PxTriangleMeshGeometry")) != NULL)  {
        shape->s.type = H3D_PHX_SHAPE_TRI;
        struct pgeo_ext* geo = (struct pgeo_ext*)arr_add(geos);
        ASSERT(geo);
        if (!import_phx_createtrimesh(geo, ezxml_child(xgeodesc, "TriangleMesh"), xroot))
            return FALSE;
        shape->s.mesh.id = geos->item_cnt - 1;
    }

    /* pose */
    ezxml_t xpose = ezxml_child(xshape, "LocalPose");
    if (xpose != NULL)  {
        import_phx_getpose(shape->s.local_pos, shape->s.local_rot, xpose);
        import_phx_transform_pose(shape->s.local_pos, shape->s.local_rot, &g_root_mat);
    }

    /* materials */
    ezxml_t xmtls = ezxml_child(xshape, "Materials");
    if (xmtls != NULL)  {
        uint mtl_cnt = 0;
        ezxml_t xmtlref = ezxml_child(xmtls, "PxMaterialRef");
        while (xmtlref != NULL) {
            mtl_cnt ++;
            xmtlref = ezxml_next(xmtlref);
        }

        shape->mtl_ids = (uint*)ALLOC(sizeof(uint)*mtl_cnt, 0);
        ASSERT(shape->mtl_ids);
        xmtlref = ezxml_child(xmtls, "PxMaterialRef");
        while (xmtlref != NULL) {
            shape->mtl_ids[shape->s.mtl_cnt++] = import_phx_createmtl(mtls, xmtlref, xroot);
            xmtlref = ezxml_next(xmtlref);
        }
    }

    /* properties */
    ezxml_t xflags = ezxml_child(xshape, "Flags");
    if (xflags != NULL) {
        shape->s.ccd = strstr(ezxml_txt(xflags), "eUSE_SWEPT_BOUNDS") != NULL;
    }

    return TRUE;
}

void import_phx_destroyshape(struct shape_ext* shape)
{
    if (shape->mtl_ids != NULL)
        FREE(shape->mtl_ids);
}

void import_phx_destroyrigid(struct rigid_ext* rigid)
{
    if (rigid->shapes)  {
        for (uint i = 0; i < rigid->r.shape_cnt; i++)
            import_phx_destroyshape(&rigid->shapes[i]);
        FREE(rigid->shapes);
    }
    FREE(rigid);
}

int import_phx_createconvexmesh(struct pgeo_ext* geo, ezxml_t xcmesh, ezxml_t xroot)
{
    /* search root for specific convesmesh ID */
    const char* cmesh_ref = ezxml_txt(xcmesh);

    ezxml_t xmeshdata = ezxml_child(xroot, "PxConvexMesh");
    while (xmeshdata != NULL)   {
        if (str_isequal(ezxml_txt(ezxml_child(xmeshdata, "Id")), cmesh_ref))
            break;
        xmeshdata = ezxml_next(xmeshdata);
    }

    if (xmeshdata == NULL)  {
        printf(TERM_BOLDRED "Error: mesh data '%s' not found in file\n" TERM_RESET, cmesh_ref);
        return FALSE;
    }

    /* parse points into a buffer */
    const char* pts_const = ezxml_txt(ezxml_child(xmeshdata, "points"));
    uint pts_sz = (uint)strlen(pts_const);
    char* pts_data = (char*)ALLOC(pts_sz + 1, 0);
    ASSERT(pts_data);
    memcpy(pts_data, pts_const, pts_sz);    pts_data[pts_sz] = 0;

    struct array pts;
    arr_create(mem_heap(), &pts, sizeof(float), 128, 128, 0);

    char* token = strtok(pts_data, NUMBER_DELIMITER);
    while (token != NULL)   {
        *((float*)arr_add(&pts)) = str_tofl32(token);
        token = strtok(NULL, NUMBER_DELIMITER);
    }

    /* convert coords */
    for (uint i = 0; i < pts.item_cnt; i+=3)   {
        float* pt = &((float*)pts.buffer)[i];
        import_phx_convert3f(pt);
    }

    /* create memory file and get ready to bake convex mesh */
    file_t f = fio_createmem(mem_heap(), "convexmesh", 0);
    ASSERT(f);

    /* cook conves mesh */
    PxConvexMeshDesc px_desc;
    px_desc.points.count = pts.item_cnt/3;
    px_desc.points.stride = sizeof(float)*3;
    px_desc.points.data = pts.buffer;
    px_desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
    import_phx_writer writer(f);
    if (!g_pxcook->cookConvexMesh(px_desc, static_cast<PxOutputStream&>(writer)))   {
        fio_close(f);
        arr_destroy(&pts);
        FREE(pts_data);
        return FALSE;
    }

    geo->g.type = H3D_PHX_GEO_CONVEX;
    geo->g.tri_cnt = 0;
    geo->g.vert_cnt = px_desc.points.count;
    size_t gsize;
    geo->data = fio_detachmem(f, &gsize, NULL);
    geo->g.size = (uint)gsize;

    fio_close(f);
    arr_destroy(&pts);
    FREE(pts_data);
    return TRUE;
}

int import_phx_createtrimesh(struct pgeo_ext* geo, ezxml_t xtrimesh, ezxml_t xroot)
{
    /* search root for specific convesmesh ID */
    const char* trimesh_ref = ezxml_txt(xtrimesh);

    ezxml_t xmeshdata = ezxml_child(xroot, "PxTriangleMesh");
    while (xmeshdata != NULL)   {
        if (str_isequal(ezxml_txt(ezxml_child(xmeshdata, "Id")), trimesh_ref))
            break;
        xmeshdata = ezxml_next(xmeshdata);
    }

    if (xmeshdata == NULL)  {
        printf(TERM_BOLDRED "Error: mesh data '%s' not found in file\n" TERM_RESET, trimesh_ref);
        return FALSE;
    }

    /* parse points into a buffer */
    const char* pts_const = ezxml_txt(ezxml_child(xmeshdata, "Points"));
    uint pts_sz = (uint)strlen(pts_const);
    char* pts_data = (char*)ALLOC(pts_sz + 1, 0);
    ASSERT(pts_data);
    memcpy(pts_data, pts_const, pts_sz);    pts_data[pts_sz] = 0;

    struct array pts;
    arr_create(mem_heap(), &pts, sizeof(float), 512, 512, 0);

    char* token = strtok(pts_data, NUMBER_DELIMITER);
    while (token != NULL)   {
        *((float*)arr_add(&pts)) = str_tofl32(token);
        token = strtok(NULL, NUMBER_DELIMITER);
    }

    /* convert coords */
    for (uint i = 0; i < pts.item_cnt; i+=3)   {
        float* pt = &((float*)pts.buffer)[i];
        import_phx_convert3f(pt);
    }

    /* parse indexes */
    struct array idxs;
    arr_create(mem_heap(), &idxs, sizeof(int16), 512, 512, 0);

    const char* idxs_const = ezxml_txt(ezxml_child(xmeshdata, "Triangles"));
    uint idxs_sz = (uint)strlen(idxs_const);
    char* idxs_data = (char*)ALLOC(idxs_sz + 1, 0);
    ASSERT(idxs_data);
    memcpy(idxs_data, idxs_const, idxs_sz);    idxs_data[idxs_sz] = 0;

    token = strtok(idxs_data, NUMBER_DELIMITER);
    while (token != NULL)   {
        *((int16*)arr_add(&idxs)) = (int16)str_toint32(token);
        token = strtok(NULL, NUMBER_DELIMITER);
    }

    /* convert triangle winding */
    for (uint i = 0; i < idxs.item_cnt; i+=3)   {
        int16* tri = &((int16*)idxs.buffer)[i];
        int16 tmp = tri[0];
        tri[0] = tri[2];
        tri[2] = tmp;
    }

    /* create memory file and get ready to bake convex mesh */
    file_t f = fio_createmem(mem_heap(), "trimesh", 0);
    ASSERT(f);

    /* cook conves mesh */
    PxTriangleMeshDesc px_desc;
    px_desc.points.count = pts.item_cnt/3;
    px_desc.points.stride = sizeof(float)*3;
    px_desc.points.data = pts.buffer;
    px_desc.triangles.count = idxs.item_cnt/3;
    px_desc.triangles.stride = sizeof(int16)*3;
    px_desc.triangles.data = idxs.buffer;
    px_desc.flags = PxMeshFlag::e16_BIT_INDICES;
    import_phx_writer writer(f);
    if (!g_pxcook->cookTriangleMesh(px_desc, static_cast<PxOutputStream&>(writer)))   {
        fio_close(f);
        arr_destroy(&pts);
        arr_destroy(&idxs);
        FREE(idxs_data);
        FREE(pts_data);
        return FALSE;
    }

    geo->g.type = H3D_PHX_GEO_TRI;
    geo->g.tri_cnt = px_desc.triangles.count;
    geo->g.vert_cnt = px_desc.points.count;
    size_t gsize;
    geo->data = fio_detachmem(f, &gsize, NULL);
    geo->g.size = (uint)gsize;

    fio_close(f);
    arr_destroy(&pts);
    arr_destroy(&idxs);
    FREE(idxs_data);
    FREE(pts_data);

    return TRUE;
}

void import_phx_getpose(OUT float pos[3], OUT float rot[4], ezxml_t xpose)
{
    const char* pose_str = ezxml_txt(xpose);
    sscanf(pose_str, "%f %f %f %f %f %f %f",
        &rot[0], &rot[1], &rot[2], &rot[3],
        &pos[0], &pos[1], &pos[2]);

    struct vec3f v;
    struct quat4f q;

    vec3_setf(&v, pos[0], pos[1], pos[2]);
    quat_setf(&q, rot[0], rot[1], rot[2], rot[3]);

    import_convert_vec3(&v, &v, g_phx_coord);
    import_convert_quat(&q, &q, g_phx_coord);

    import_set3f(pos, v.f);
    import_set4f(rot, q.f);
}

void import_phx_transform_pose(INOUT float pos[3], INOUT float rot[4], const struct mat3f* mat)
{
    struct xform3d xf;
    struct mat3f mymat;

    xform3d_setf_raw(&xf,
        pos[0], pos[1], pos[2],
        rot[0], rot[1], rot[2], rot[3]);
    xform3d_getmat(&mymat, &xf);
    mat3_mul(&mymat, &mymat, mat);
    xform3d_frommat3(&xf, &mymat);

    /* reset pos, and rot */
    pos[0] = xf.p.x;
    pos[1] = xf.p.y;
    pos[2] = xf.p.z;

    rot[0] = xf.q.x;
    rot[1] = xf.q.y;
    rot[2] = xf.q.z;
    rot[3] = xf.q.w;
}

uint import_phx_createmtl(struct array* mtls, ezxml_t xmtlref, ezxml_t xroot)
{
    /* get material id from ref, and find it in xml */
    const char* mtlref = ezxml_txt(xmtlref);

    ezxml_t xmtl = ezxml_child(xroot, "PxMaterial");
    while (xmtl != NULL)    {
        if (str_isequal(ezxml_txt(ezxml_child(xmtl, "Id")), mtlref))    {
            struct h3d_phx_mtl mtl;
            mtl.friction_st = str_tofl32(ezxml_txt(ezxml_child(xmtl, "StaticFriction")));
            mtl.friction_dyn = str_tofl32(ezxml_txt(ezxml_child(xmtl, "DynamicFriction")));
            mtl.restitution = str_tofl32(ezxml_txt(ezxml_child(xmtl, "Restitution")));

            uint mtl_id = import_phx_findmtl(&mtl, (const struct h3d_phx_mtl*)mtls->buffer,
                mtls->item_cnt);
            if (mtl_id == INVALID_INDEX)    {
                struct h3d_phx_mtl* pmtl = (struct h3d_phx_mtl*)arr_add(mtls);
                ASSERT(pmtl);
                memcpy(pmtl, &mtl, sizeof(mtl));
                mtl_id = mtls->item_cnt - 1;
            }
            return mtl_id;
        }
        xmtl = ezxml_next(xmtl);
    }

    printf(TERM_BOLDYELLOW "Warning: material ref '%s' not found in file\n" TERM_RESET, mtlref);
    return INVALID_INDEX;
}

int import_writephx(const char* filepath, const struct phx_ext* p)
{
    char filepath_tmp[DH_PATH_MAX];
    strcat(strcpy(filepath_tmp, filepath), ".tmp");
    FILE* f = fopen(filepath_tmp, "wb");
    if (f == NULL)  {
        printf(TERM_BOLDRED "Error: failed to open file '%s' for writing\n" TERM_RESET, filepath);
        return FALSE;
    }

    /* header */
    struct h3d_header header;
    header.sign = H3D_SIGN;
    header.type = H3D_PHX;
    header.version = H3D_VERSION_12;
    header.data_offset = sizeof(struct h3d_header);
    fwrite(&header, sizeof(header), 1, f);

    /* phx descriptor */
    fwrite(&p->p, sizeof(struct h3d_phx), 1, f);

    /* materials */
    fwrite(p->mtls, sizeof(struct h3d_phx_mtl), p->p.mtl_cnt, f);

    /* geos */
    for (uint i = 0; i < p->p.geo_cnt; i++)   {
        fwrite(&p->geos[i].g, sizeof(struct h3d_phx_geo), 1, f);
        fwrite(p->geos[i].data, p->geos[i].g.size, 1, f);
    }

    /* rigid(s) */
    if (p->rigid != NULL)   {
        fwrite(&p->rigid->r, sizeof(struct h3d_phx_rigid), 1, f);
        /* shapes */
        for (uint i = 0; i < p->rigid->r.shape_cnt; i++)  {
            fwrite(&p->rigid->shapes[i].s, sizeof(struct h3d_phx_shape), 1, f);
            /* mtl ids */
            fwrite(p->rigid->shapes[i].mtl_ids, sizeof(uint), p->rigid->shapes[i].s.mtl_cnt, f);
        }
    }

    fclose(f);
    return util_movefile(filepath, filepath_tmp);
}
