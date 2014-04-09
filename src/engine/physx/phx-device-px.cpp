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

#include <PxPhysicsAPI.h>
#include <extensions/PxExtensionsAPI.h>

#include "dhcore/core.h"
#include "dhcore/freelist-alloc.h"
#include "dhcore/array.h"
#include "dhcore/pool-alloc.h"
#include "dhcore/mt.h"
#include "dhcore/hash-table.h"
#include "dhcore/hash.h"
#include "dhcore/hwinfo.h"

#include "phx-device.h"
#include "mem-ids.h"
#include "engine.h"
#include "gfx-device.h"
#include "gfx-canvas.h"
#include "dhapp/init-params.h"
#include "gfx-cmdqueue.h"
#include "gfx.h"

#define PHX_DEFAULT_MEMSIZE (32*1024*1024)
#define PHX_DEFAULT_BLOCKSIZE 200
#define PHX_DEFAULT_SCRATCHSIZE (512*1024)

using namespace physx;

/*************************************************************************************************
 * fwd declarations
 */
struct phx_scene_data;
struct phx_geo_gpu* phx_creategeo(const struct vec3f* verts, uint vert_cnt,
    const uint16* indexes, uint tri_cnt, uint thread_id);
void phx_destroygeo(struct phx_geo_gpu* geo);
void phx_destroy_scenedata(struct phx_scene_data* sdata);
void phx_trigger_call(struct phx_scene_data* s, phx_obj trigger, phx_obj other,
    enum phx_trigger_state state);

/*************************************************************************************************
 * class overrides
 */
inline void* operator new(size_t sz, void* ptr)
{
    return ptr;
}

inline void operator delete(void* ptr)
{
}

/* Physx allocators */
/* freelist (dynamic but fixed total size) */
class phx_allocator_fs : public PxAllocatorCallback
{
private:
    struct freelist_alloc_ts* mfreelist;

public:
    phx_allocator_fs(struct freelist_alloc_ts* alloc) : mfreelist(alloc)
    {
    }

    virtual ~phx_allocator_fs()
    {
    }

    /* from PxAllocatorCallback */
    void* allocate(size_t sz, const char* type_name, const char* filename, int line)
    {
        void* p = mem_freelist_alignedalloc_ts(mfreelist, sz, 16, MID_PHX);
        ASSERT(p);
        return p;
    }

    void deallocate(void* ptr)
    {
        if (ptr)    {
            mem_freelist_alignedfree_ts(mfreelist, ptr);
        }
    }
};

/* heap (no limitation, but slow) */
class phx_allocator_heap : public PxAllocatorCallback
{
public:
    phx_allocator_heap()
    {
    }

    virtual ~phx_allocator_heap()
    {
    }

    /* from PxAllocatorCallback */
    void* allocate(size_t sz, const char* type_name, const char* filename, int line)
    {
        void* p = mem_alignedalloc(sz, 16, filename, line, MID_PHX);
        ASSERT(p);
        return p;
    }

    void deallocate(void* ptr)
    {
        if (ptr)    {
            mem_alignedfree(ptr);
        }
    }
};


/* logging errors within Physx */
class phx_error : public PxErrorCallback
{
public:
    /* from PxErrorCallback */
    void reportError(PxErrorCode::Enum code, const char* msg, const char* filename, int line)
    {
        enum log_type type;
        switch (code)   {
        case PxErrorCode::eINVALID_PARAMETER:
        case PxErrorCode::eINVALID_OPERATION:
        case PxErrorCode::eOUT_OF_MEMORY:
            type = LOG_ERROR;
            break;

        case PxErrorCode::eDEBUG_INFO:
            type = LOG_INFO;
            break;

        case PxErrorCode::eDEBUG_WARNING:
            type = LOG_WARNING;
            break;

        default:
            type = LOG_ERROR;
            break;
        }

        log_printf(type, "Physx: %s (%s@%d)", msg, filename, line);
    }
};

/* Physx streaming from memory */
class phx_stream : public PxInputStream
{
private:
    mutable const uint8* mbuff;

public:
    phx_stream(const void* buff) : mbuff((const uint8*)buff) {}

    PxU32 read(void* dest, PxU32 cnt)
    {
        memcpy(dest, mbuff, cnt);
        mbuff += cnt;
        return cnt;
    }
};

INLINE enum phx_trigger_state phx_get_triggertype(PxPairFlag::Enum type)
{
    switch (type)   {
    case PxPairFlag::eNOTIFY_TOUCH_FOUND:
        return PHX_TRIGGER_IN;
    case PxPairFlag::eNOTIFY_TOUCH_LOST:
        return PHX_TRIGGER_OUT;
    default:
        return PHX_TRIGGER_UNKNOWN;
    }
}

/* event handler */
class phx_event_handler : public PxSimulationEventCallback
{
private:
    struct phx_scene_data* ms;

public:
    phx_event_handler(struct phx_scene_data* s) : ms(s) {}
    void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count)    {}
    void onWake(PxActor** actors, PxU32 count)    {}
    void onSleep(PxActor** actors, PxU32 count)    {}
    void onContact(const PxContactPairHeader& pair_header, const PxContactPair* pairs,
        PxU32 pair_cnt)  {}

    void onTrigger(PxTriggerPair* pairs, PxU32 count)
    {
        for (uint i = 0; i < count; i++)  {
            if (pairs[i].flags &
                (PxTriggerPairFlag::eDELETED_SHAPE_TRIGGER |
                 PxTriggerPairFlag::eDELETED_SHAPE_OTHER))
            {
                continue;
            }

            PxActor* other = &pairs[i].otherShape->getActor();
            PxActor* trigger = &pairs[i].triggerShape->getActor();

            phx_trigger_call(ms, (phx_obj)trigger->userData, (phx_obj)other->userData,
                phx_get_triggertype(pairs[i].status));
        }
    }
};

/*************************************************************************************************
 * types
 */
struct phx_event
{
    pfn_trigger_callback trigger_fn;
    void* param;
};

struct phx_scene_data
{
    PxScene* s;
    class phx_event_handler* event_handler;
    struct pool_alloc event_pool;   /* item: phx_event */
    struct hashtable_open trigger_tbl;  /* key: hash(trigger_obj), value: phx_event* */
};

struct phx_geo_gpu
{
    uint vert_cnt;
    uint tri_cnt;
    gfx_buffer ib;
    gfx_buffer vb_pos;
    gfx_inputlayout il;
};

struct phx_device
{
    struct array scenes;    /* item: phx_scene_data* */
    struct pool_alloc obj_pool;
    class PxAllocatorCallback* alloc;
    class phx_error err;
    struct freelist_alloc_ts fs_mem;
    class PxFoundation* base;
    class PxPhysics* sdk;
    class PxProfileZoneManager* profiler;
    class PxDefaultCpuDispatcher* dispatcher;
    void* scratch_buff;
    size_t scratch_sz;
    bool_t ext_init;
    bool_t optimize_mem;
};

/*************************************************************************************************
 * globals
 */
static struct phx_device g_phxdev;

/*************************************************************************************************
 * inlines
 */
INLINE struct xform3d* phx_toxf(struct xform3d* xf, const PxTransform& xf_px)
{
    return xform3d_setf_raw(xf, xf_px.p.x, xf_px.p.y, xf_px.p.z,
        xf_px.q.x, xf_px.q.y, xf_px.q.z, xf_px.q.w);
}

INLINE PxTransform phx_xftopx(const struct xform3d* xf)
{
    return PxTransform(PxVec3(xf->p.x, xf->p.y, xf->p.z),
        PxQuat(xf->q.x, xf->q.y, xf->q.z, xf->q.w));
}

INLINE struct mat3f* phx_tomat3(struct mat3f* rm, const PxTransform& xf)
{
    struct quat4f q;
    mat3_set_rotquat(rm, quat_setf(&q, xf.q.x, xf.q.y, xf.q.z, xf.q.w));
    mat3_set_transf(rm, xf.p.x, xf.p.y, xf.p.z);

    return rm;
}

INLINE PxTransform phx_mat3topx(const struct mat3f* m)
{
    PxTransform xf;
    struct quat4f q;
    quat_frommat3(&q, m);
    mat3_get_transf(&xf.p.x, &xf.p.y, &xf.p.z, m);
    xf.q.x = q.x;
    xf.q.y = q.y;
    xf.q.z = q.z;
    xf.q.w = q.w;
    return xf;
}

INLINE PxVec3 phx_vec3topx(const struct vec3f* v)
{
    return PxVec3(v->x, v->y, v->z);
}

INLINE struct vec3f* phx_tovec3(struct vec3f* rv, const PxVec3& v)
{
    return vec3_setf(rv, v.x, v.y, v.z);
}

INLINE PxBounds3 phx_aabbtopx(const struct aabb* bb)
{
    return PxBounds3(phx_vec3topx(&bb->minpt), phx_vec3topx(&bb->maxpt));
}

INLINE PxQuat phx_quat4topx(const struct quat4f* q)
{
    return PxQuat(q->x, q->y, q->z, q->w);
}

INLINE struct phx_scene_data** phx_getfreescene_slot(uint* pid)
{
    struct phx_scene_data** pscenes = (struct phx_scene_data**)g_phxdev.scenes.buffer;
    for (uint i = 0; i < g_phxdev.scenes.item_cnt; i++)  {
        if (pscenes[i] == NULL) {
            *pid = i + 1;
            return pscenes + i;
        }
    }

    *pid = g_phxdev.scenes.item_cnt + 1;
    return (struct phx_scene_data**)arr_add(&g_phxdev.scenes);
}

INLINE PxScene* phx_getscene(uint id)
{
    return ((struct phx_scene_data**)g_phxdev.scenes.buffer)[id-1]->s;
}

INLINE phx_obj phx_createobj(uptr_t api_obj, enum phx_obj_type type)
{
    phx_obj obj = (phx_obj)mem_pool_alloc(&g_phxdev.obj_pool);
    ASSERT(obj);
    memset(obj, 0x00, sizeof(struct phx_obj_data));

    obj->type = type;
    obj->api_obj = api_obj;
    obj->ref_cnt = 1;
    return obj;
}

INLINE void phx_destroyobj(phx_obj obj)
{
    obj->type = PHX_OBJ_NULL;
    mem_pool_free(&g_phxdev.obj_pool, obj);
}

INLINE PxForceMode::Enum phx_fmodetopx(enum phx_force_mode mode)
{
    switch (mode)   {
    case PHX_FORCE_NORMAL:
        return PxForceMode::eFORCE;
    case PHX_FORCE_IMPULSE:
        return PxForceMode::eIMPULSE;
    case PHX_FORCE_VELOCITY:
        return PxForceMode::eVELOCITY_CHANGE;
    case PHX_FORCE_ACCELERATION:
        return PxForceMode::eACCELERATION;
    default:
        return PxForceMode::eFORCE;
    }
}

/*************************************************************************************************/
void phx_zerodev()
{
    memset(&g_phxdev, 0x00, sizeof(g_phxdev));
}

result_t phx_initdev(const struct init_params* params)
{
    result_t r;

    const struct phx_params* pparams = &params->phx;

    r = arr_create(mem_heap(), &g_phxdev.scenes, sizeof(struct phx_scene_data*), 5, 5, MID_PHX);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    r = mem_pool_create(mem_heap(), &g_phxdev.obj_pool, sizeof(struct phx_obj_data),
        PHX_DEFAULT_BLOCKSIZE, MID_PHX);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    /* scratch buffer */
    size_t scratch_sz = pparams->scratch_sz != 0 ? ((size_t)pparams->scratch_sz*1024) :
        PHX_DEFAULT_SCRATCHSIZE;
    if (scratch_sz % (16*1024) != 0)    {
        scratch_sz = (scratch_sz/(16*1024) + 1)*(16*1024);
    }
    g_phxdev.scratch_buff = ALIGNED_ALLOC(scratch_sz, MID_PHX);
    g_phxdev.scratch_sz = scratch_sz;
    if (g_phxdev.scratch_buff == NULL)  {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    /* allocator */
    if (BIT_CHECK(params->flags, ENG_FLAG_OPTIMIZEMEMORY))  {
        r = mem_freelist_create_ts(mem_heap(), &g_phxdev.fs_mem,
            pparams->mem_sz != 0 ? (pparams->mem_sz*1024) : PHX_DEFAULT_MEMSIZE, MID_PHX);
        if (IS_FAIL(r)) {
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            return RET_OUTOFMEMORY;
        }

        void* alloc_ptr = ALLOC(sizeof(class phx_allocator_fs), MID_PHX);
        g_phxdev.alloc = (PxAllocatorCallback*)new(alloc_ptr) phx_allocator_fs(&g_phxdev.fs_mem);
        g_phxdev.optimize_mem = TRUE;
    }   else    {
        void* alloc_ptr = ALLOC(sizeof(class phx_allocator_heap), MID_PHX);
        g_phxdev.alloc = (PxAllocatorCallback*)new(alloc_ptr) phx_allocator_heap();
    }

    g_phxdev.base = PxCreateFoundation(PX_PHYSICS_VERSION, *g_phxdev.alloc, g_phxdev.err);
    if (g_phxdev.base == NULL) {
        err_print(__FILE__, __LINE__, "phx-device init failed: could not create Physx foundation");
        return RET_FAIL;
    }

    if (BIT_CHECK(pparams->flags, PHX_FLAG_PROFILE)) {
        g_phxdev.profiler = &PxProfileZoneManager::createProfileZoneManager(g_phxdev.base);
    }

    g_phxdev.sdk = PxCreatePhysics(PX_PHYSICS_VERSION, *g_phxdev.base, PxTolerancesScale(),
        BIT_CHECK(pparams->flags, PHX_FLAG_TRACKMEM) ? true : false, g_phxdev.profiler);
    if (g_phxdev.sdk == NULL)  {
        err_print(__FILE__, __LINE__, "phx-device init failed: could not create Physx SDK");
        return RET_FAIL;
    }

    if (!PxInitExtensions(*g_phxdev.sdk))  {
        err_print(__FILE__, __LINE__, "phx-device init failed: could not create Physx extensions");
        return RET_FAIL;
    }
    g_phxdev.ext_init = TRUE;

    /* dispatch through cpu cores */
    const struct hwinfo* hinfo = eng_get_hwinfo();
    int core_cnt = clampi((int)hinfo->cpu_core_cnt - 2, 1, 4);
    g_phxdev.dispatcher = PxDefaultCpuDispatcherCreate((PxU32)core_cnt);
    if (g_phxdev.dispatcher == NULL)   {
        err_print(__FILE__, __LINE__, "phx-device init failed: could not create Physx dispatcher");
        return RET_FAIL;
    }

    log_print(LOG_INFO, "phx-device: Physix v3.2 initialized.");
    return RET_OK;
}

void phx_releasedev()
{
    /* release un-released scenes */
    for (uint i = 0; i < g_phxdev.scenes.item_cnt; i++)  {
        struct phx_scene_data* s = ((struct phx_scene_data**)g_phxdev.scenes.buffer)[i];
        if (s != NULL)  {
            log_printf(LOG_INFO, "phx-device: releasing scene id %d", i+1);
            phx_destroy_scenedata(s);
        }
    }

    if (g_phxdev.dispatcher != NULL)
        g_phxdev.dispatcher->release();

    if (g_phxdev.ext_init)
        PxCloseExtensions();

    if (g_phxdev.sdk != NULL)
        g_phxdev.sdk->release();

    if (g_phxdev.profiler != NULL)
        g_phxdev.profiler->release();

    if (g_phxdev.base != NULL)
        g_phxdev.base->release();

    if (g_phxdev.alloc != NULL) {
        g_phxdev.alloc->~PxAllocatorCallback();
        FREE(g_phxdev.alloc);
    }

    uint leak_cnt = mem_freelist_getleaks_ts(&g_phxdev.fs_mem, NULL);
    if (leak_cnt > 0)   {
        void** ptrs = (void**)ALLOC(sizeof(void*)*leak_cnt, MID_PHX);
        ASSERT(ptrs);
        log_printf(LOG_WARNING, "phx-device: total %d mem-leaks found:", leak_cnt);
        mem_freelist_getleaks_ts(&g_phxdev.fs_mem, ptrs);
        for (uint i = 0; i < leak_cnt; i++)
            log_printf(LOG_INFO, "  0x%x", (uptr_t)ptrs[i]);
    }

    uint obj_leak_cnt = mem_pool_getleaks(&g_phxdev.obj_pool);
    if (obj_leak_cnt > 0)
        log_printf(LOG_WARNING, "phx-device total %d object leaks found", obj_leak_cnt);

    if (g_phxdev.scratch_buff != NULL)
        ALIGNED_FREE(g_phxdev.scratch_buff);
    if (g_phxdev.optimize_mem)
        mem_freelist_destroy_ts(&g_phxdev.fs_mem);
    mem_pool_destroy(&g_phxdev.obj_pool);
    arr_destroy(&g_phxdev.scenes);
}

uint phx_create_scene(const vec3f* gravity, OPTIONAL const struct phx_scene_limits* limits)
{
    result_t r;

    /* create scene data */
    struct phx_scene_data* s = (struct phx_scene_data*)ALLOC(sizeof(struct phx_scene_data), MID_PHX);
    if (s == NULL)  {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return 0;
    }
    memset(s, 0x00, sizeof(struct phx_scene_data));

    /* Physx scene */
    PxSceneDesc sdesc(g_phxdev.sdk->getTolerancesScale());
    sdesc.gravity = phx_vec3topx(gravity);

    if (limits != NULL) {
        sdesc.limits.maxNbActors = limits->actors_max;
        sdesc.limits.maxNbBodies = limits->rigid_max;
        sdesc.limits.maxNbConstraints = limits->constraints_max;
        sdesc.limits.maxNbDynamicShapes = limits->shapes_dyn_max;
        sdesc.limits.maxNbStaticShapes = limits->shapes_st_max;
    }   else    {
        sdesc.limits.setToDefault();
    }

    sdesc.flags = PxSceneFlag::eENABLE_ACTIVETRANSFORMS;

    /* assign dispatchers */
    if (sdesc.cpuDispatcher == NULL)
        sdesc.cpuDispatcher = g_phxdev.dispatcher;

    if (sdesc.filterShader == NULL)
        sdesc.filterShader = PxDefaultSimulationFilterShader;

    void* ebuff = ALLOC(sizeof(class phx_event_handler), MID_PHX);
    ASSERT(ebuff);
    class phx_event_handler* e = new(ebuff) phx_event_handler(s);
    sdesc.simulationEventCallback = e;
    s->event_handler = e;

    /* create physics scene */
    PxScene* pxscene = g_phxdev.sdk->createScene(sdesc);
    if (pxscene == NULL)    {
        err_print(__FILE__, __LINE__, "phx-device: creating scene failed");
        return 0;
    }
    s->s = pxscene;

    /* containers */
    r = mem_pool_create(mem_heap(), &s->event_pool, sizeof(struct phx_event), 200, MID_PHX);
    if (IS_FAIL(r)) {
        phx_destroy_scenedata(s);
        err_printn(__FILE__, __LINE__, r);
        return 0;
    }

    r = hashtable_open_create(mem_heap(), &s->trigger_tbl, 200, 200, MID_PHX);
    if (IS_FAIL(r)) {
        phx_destroy_scenedata(s);
        err_printn(__FILE__, __LINE__, r);
        return 0;
    }

    uint id;
    struct phx_scene_data** ptr_scene = phx_getfreescene_slot(&id);
    if (ptr_scene == NULL)  {
        phx_destroy_scenedata(s);
        err_print(__FILE__, __LINE__, "phx-device: not enough memory");
        return 0;
    }
    *ptr_scene = s;

    return id;
}

void phx_destroy_scenedata(struct phx_scene_data* sdata)
{
    hashtable_open_destroy(&sdata->trigger_tbl);
    mem_pool_destroy(&sdata->event_pool);

    if (sdata->s != NULL)   {
        sdata->s->fetchResults(true);
        sdata->s->release();
    }

    if (sdata->event_handler != NULL)   {
        sdata->event_handler->~phx_event_handler();
        FREE(sdata->event_handler);
    }

    FREE(sdata);
}

void phx_destroy_scene(uint scene_id)
{
    ASSERT(scene_id > 0);
    ASSERT(scene_id <= g_phxdev.scenes.item_cnt);

    struct phx_scene_data** pscenes = (struct phx_scene_data**)g_phxdev.scenes.buffer;
    if (pscenes[scene_id-1] != NULL)    {
        phx_destroy_scenedata(pscenes[scene_id-1]);
        pscenes[scene_id-1] = NULL;
    }
}

void phx_scene_getstats(uint scene_id, struct phx_scene_stats* stats)
{
    memset(stats, 0x00, sizeof(struct phx_scene_stats));
    PxSimulationStatistics pstats;

    PxScene* s = phx_getscene(scene_id);
    s->getSimulationStatistics(pstats);

    stats->st_cnt = pstats.numStaticBodies;
    stats->dyn_cnt = pstats.numDynamicBodies;
    stats->active_constaint_cnt = pstats.numActiveConstraints;
    stats->active_kinamatic_cnt = pstats.numActiveKinematicBodies;
    stats->active_dyn_cnt = pstats.numActiveDynamicBodies;
}

void phx_scene_setgravity(uint scene_id, const struct vec3f* gravity)
{
    PxScene* s = phx_getscene(scene_id);
    s->setGravity(phx_vec3topx(gravity));
}

struct vec3f* phx_scene_getgravity(uint scene_id, struct vec3f* gravity)
{
    return phx_tovec3(gravity, phx_getscene(scene_id)->getGravity());
}

void phx_scene_addactor(uint scene_id, phx_obj rigid_obj)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_ST || rigid_obj->type == PHX_OBJ_RIGID_DYN);

    phx_getscene(scene_id)->addActor(*((PxActor*)rigid_obj->api_obj));
}

void phx_scene_removeactor(uint scene_id, phx_obj rigid_obj)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_ST || rigid_obj->type == PHX_OBJ_RIGID_DYN);

    phx_getscene(scene_id)->removeActor(*((PxActor*)rigid_obj->api_obj));
}

void phx_scene_flush(uint scene_id)
{
    PxScene* s = phx_getscene(scene_id);
    s->fetchResults(true);
    s->flush();
}

void phx_scene_wait(uint scene_id)
{
    phx_getscene(scene_id)->fetchResults(true);
}

bool_t phx_scene_check(uint scene_id)
{
    return phx_getscene(scene_id)->checkResults() ? TRUE : FALSE;
}

void phx_scene_simulate(uint scene_id, float dt)
{
    phx_getscene(scene_id)->simulate(dt, NULL, g_phxdev.scratch_buff, 
        (int)g_phxdev.scratch_sz, true);
}

struct phx_active_transform* phx_scene_activexforms(uint scene_id, struct allocator* alloc,
    OUT uint* xform_cnt)
{
    PxU32 pxform_cnt;
    PxScene* s = phx_getscene(scene_id);

    PxActiveTransform* pxforms = s->getActiveTransforms(pxform_cnt);
    if (pxform_cnt == 0)
        return NULL;

    struct phx_active_transform* xfs = (struct phx_active_transform*)A_ALIGNED_ALLOC(alloc,
        sizeof(struct phx_active_transform)*pxform_cnt, MID_PHX);
    if (xfs == NULL)
        return NULL;
    memset(xfs, 0x00, sizeof(struct phx_active_transform)*pxform_cnt);

    for (uint i = 0; i < pxform_cnt; i++) {
        struct phx_active_transform* xf = &xfs[i];

        phx_obj actor = (phx_obj)pxforms[i].userData;
        xf->obj = (struct cmp_obj*)actor->user_ptr;
        phx_toxf(&xf->xform_ws, pxforms[i].actor2World);

        /* if object is rigid body, then velocity vectors are updated, fill them too */
        PxRigidBody* rbody = pxforms[i].actor->isRigidBody();
        if (rbody != NULL)  {
            phx_tovec3(&xf->vel_lin, rbody->getLinearVelocity());
            phx_tovec3(&xf->vel_ang, rbody->getAngularVelocity());
        }
    }

    *xform_cnt = pxform_cnt;
    return xfs;
}

phx_mtl phx_create_mtl(float friction_st, float friction_dyn, float restitution)
{
    PxMaterial* pxmtl = g_phxdev.sdk->createMaterial(friction_st, friction_dyn, restitution);
    if (pxmtl == NULL)
        return NULL;

    return phx_createobj((uptr_t)pxmtl, PHX_OBJ_MTL);
}

void phx_destroy_mtl(phx_mtl mtl)
{
    mtl->ref_cnt --;
    if (mtl->ref_cnt == 0)  {
        ((PxMaterial*)mtl->api_obj)->release();
        phx_destroyobj(mtl);
    }
}

phx_rigid_st phx_create_rigid_st(const struct xform3d* pose)
{
    PxRigidStatic* pxrigid = g_phxdev.sdk->createRigidStatic(phx_xftopx(pose));
    if (pxrigid == NULL)
        return NULL;

    phx_rigid_st obj = phx_createobj((uptr_t)pxrigid, PHX_OBJ_RIGID_ST);
    ((PxActor*)obj->api_obj)->userData = obj;   /* make two-way relationship with Px object */
    return obj;
}

phx_rigid_dyn phx_create_rigid_dyn(const struct xform3d* pose)
{
    PxRigidDynamic* pxrigid = g_phxdev.sdk->createRigidDynamic(phx_xftopx(pose));
    if (pxrigid == NULL)
        return NULL;

    phx_rigid_dyn obj = phx_createobj((uptr_t)pxrigid, PHX_OBJ_RIGID_DYN);
    ((PxActor*)obj->api_obj)->userData = obj;   /* make two-way relationship with Px object */
    return obj;
}

void phx_destroy_rigid(phx_obj rigid_obj)
{
    /* children (shapes) */
    for (uint i = 0; i < rigid_obj->child_cnt; i++)   {
        phx_obj childobj = rigid_obj->childs[i];
        switch (childobj->type)   {
        case PHX_OBJ_SHAPE_BOX:
            phx_destroy_boxshape(childobj);
            break;
        case PHX_OBJ_SHAPE_TRI:
            phx_destroy_trishape(childobj);
            break;
        case PHX_OBJ_SHAPE_CAPSULE:
            phx_destroy_capsuleshape(childobj);
            break;
        case PHX_OBJ_SHAPE_SPHERE:
            phx_destroy_sphereshape(childobj);
            break;
        case PHX_OBJ_SHAPE_CONVEX:
            phx_destroy_convexshape(childobj);
            break;
        case PHX_OBJ_SHAPE_PLANE:
            phx_destroy_planeshape(childobj);
            break;
        case PHX_OBJ_SHAPE_HEIGHTFIELD:
        default:
            ASSERT(0);
        }
    }

    ((PxActor*)rigid_obj->api_obj)->release();
    phx_destroyobj(rigid_obj);
}

phx_shape_box phx_create_boxshape(phx_obj rigid_obj,
    float hx, float hy, float hz, phx_mtl* mtls, uint mtl_cnt,
    const struct xform3d* localxf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_ST || rigid_obj->type == PHX_OBJ_RIGID_DYN);

    PxMaterial* pxmtls[PHX_CHILD_MAX];
    mtl_cnt = minui(PHX_CHILD_MAX, mtl_cnt);

    for (uint i = 0; i < mtl_cnt; i++)
        pxmtls[i] = (PxMaterial*)mtls[i]->api_obj;

    PxShape* pxshape = ((PxRigidActor*)rigid_obj->api_obj)->createShape(
        PxBoxGeometry(hx, hy, hz), pxmtls, mtl_cnt, phx_xftopx(localxf));

    if (pxshape == NULL)
        return NULL;

    /* create object and assign materials to it's children */
    phx_shape_box shape = phx_createobj((uptr_t)pxshape, PHX_OBJ_SHAPE_BOX);
    for (uint i = 0; i < mtl_cnt; i++)    {
        shape->childs[i] = mtls[i];
        mtls[i]->ref_cnt ++;
    }
    shape->child_cnt = mtl_cnt;

    /* add shape to rigid object children */
    if (rigid_obj->child_cnt == PHX_CHILD_MAX)  {
        phx_destroy_boxshape(shape);
        return NULL;
    }
    rigid_obj->childs[rigid_obj->child_cnt++] = shape;

    return shape;
}

void phx_destroy_boxshape(phx_shape_box box)
{
    ASSERT(box->type == PHX_OBJ_SHAPE_BOX);
    /* release children */
    for (uint i = 0; i < box->child_cnt; i++) {
        ASSERT(box->childs[i]->type == PHX_OBJ_MTL);
        phx_destroy_mtl(box->childs[i]);
    }

    /* */
    phx_destroyobj(box);
}

phx_shape_sphere phx_create_sphereshape(phx_obj rigid_obj,
    float radius, phx_mtl* mtls, uint mtl_cnt, const struct xform3d* localxf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_ST || rigid_obj->type == PHX_OBJ_RIGID_DYN);

    PxMaterial* pxmtls[PHX_CHILD_MAX];
    mtl_cnt = minui(PHX_CHILD_MAX, mtl_cnt);

    for (uint i = 0; i < mtl_cnt; i++)
        pxmtls[i] = (PxMaterial*)mtls[i]->api_obj;

    PxShape* pxshape = ((PxRigidActor*)rigid_obj->api_obj)->createShape(
        PxSphereGeometry(radius), pxmtls, mtl_cnt, phx_xftopx(localxf));

    if (pxshape == NULL)
        return NULL;

    /* create object and assign materials to it's children */
    phx_shape_sphere shape = phx_createobj((uptr_t)pxshape, PHX_OBJ_SHAPE_SPHERE);
    for (uint i = 0; i < mtl_cnt; i++)    {
        shape->childs[i] = mtls[i];
        mtls[i]->ref_cnt ++;
    }
    shape->child_cnt = mtl_cnt;

    /* add shape to rigid object children */
    if (rigid_obj->child_cnt == PHX_CHILD_MAX)  {
        phx_destroy_boxshape(shape);
        return NULL;
    }
    rigid_obj->childs[rigid_obj->child_cnt++] = shape;

    return shape;
}

void phx_destroy_sphereshape(phx_shape_sphere sphere)
{
    ASSERT(sphere->type == PHX_OBJ_SHAPE_SPHERE);
    /* release children */
    for (uint i = 0; i < sphere->child_cnt; i++) {
        ASSERT(sphere->childs[i]->type == PHX_OBJ_MTL);
        phx_destroy_mtl(sphere->childs[i]);
    }

    /* */
    phx_destroyobj(sphere);
}

phx_shape_capsule phx_create_capsuleshape(phx_obj rigid_obj,
    float radius, float half_height, phx_mtl* mtls, uint mtl_cnt,
    const struct xform3d* localxf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_ST || rigid_obj->type == PHX_OBJ_RIGID_DYN);

    PxMaterial* pxmtls[PHX_CHILD_MAX];
    mtl_cnt = minui(PHX_CHILD_MAX, mtl_cnt);

    for (uint i = 0; i < mtl_cnt; i++)
        pxmtls[i] = (PxMaterial*)mtls[i]->api_obj;

    PxShape* pxshape = ((PxRigidActor*)rigid_obj->api_obj)->createShape(
        PxCapsuleGeometry(radius, half_height), pxmtls, mtl_cnt, phx_xftopx(localxf));

    if (pxshape == NULL)
        return NULL;

    /* create object and assign materials to it's children */
    phx_shape_capsule shape = phx_createobj((uptr_t)pxshape, PHX_OBJ_SHAPE_CAPSULE);
    for (uint i = 0; i < mtl_cnt; i++)    {
        shape->childs[i] = mtls[i];
        mtls[i]->ref_cnt ++;
    }
    shape->child_cnt = mtl_cnt;

    /* add shape to rigid object children */
    if (rigid_obj->child_cnt == PHX_CHILD_MAX)  {
        phx_destroy_boxshape(shape);
        return NULL;
    }
    rigid_obj->childs[rigid_obj->child_cnt++] = shape;

    return shape;
}

void phx_destroy_capsuleshape(phx_shape_capsule capsule)
{
    ASSERT(capsule->type == PHX_OBJ_SHAPE_CAPSULE);
    /* release children */
    for (uint i = 0; i < capsule->child_cnt; i++) {
        ASSERT(capsule->childs[i]->type == PHX_OBJ_MTL);
        phx_destroy_mtl(capsule->childs[i]);
    }

    /* */
    phx_destroyobj(capsule);
}

phx_shape_plane phx_create_planeshape(phx_obj rigid_obj, phx_mtl mtl, const struct xform3d* localxf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_ST);

    PxShape* pxshape = ((PxRigidActor*)rigid_obj->api_obj)->createShape(
        PxPlaneGeometry(), *((PxMaterial*)mtl->api_obj), phx_xftopx(localxf));

    if (pxshape == NULL)
        return NULL;

    /* create object and assign materials to it's children */
    phx_shape_plane shape = phx_createobj((uptr_t)pxshape, PHX_OBJ_SHAPE_PLANE);
    mtl->ref_cnt ++;
    shape->childs[0] = mtl;
    shape->child_cnt = 1;

    /* add shape to rigid object children */
    ASSERT(rigid_obj->child_cnt == 0);
    rigid_obj->childs[rigid_obj->child_cnt++] = shape;

    return shape;
}

void phx_destroy_planeshape(phx_shape_plane plane)
{
    ASSERT(plane->type == PHX_OBJ_SHAPE_PLANE);
    ASSERT(plane->child_cnt == 1);
    ASSERT(plane->childs[0]->type == PHX_OBJ_MTL);

    phx_destroy_mtl(plane->childs[0]);

    /* */
    phx_destroyobj(plane);
}

phx_shape_convex phx_create_convexshape(phx_obj rigid_obj,
    phx_convexmesh convexmesh, phx_mtl* mtls, uint mtl_cnt, const struct xform3d* localxf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_ST || rigid_obj->type == PHX_OBJ_RIGID_DYN);

    PxMaterial* pxmtls[PHX_CHILD_MAX];
    mtl_cnt = minui(PHX_CHILD_MAX-1, mtl_cnt);

    for (uint i = 0; i < mtl_cnt; i++)
        pxmtls[i] = (PxMaterial*)mtls[i]->api_obj;

    PxShape* pxshape = ((PxRigidActor*)rigid_obj->api_obj)->createShape(
        PxConvexMeshGeometry((PxConvexMesh*)convexmesh->api_obj, PxMeshScale()), pxmtls, mtl_cnt,
        phx_xftopx(localxf));

    if (pxshape == NULL)
        return NULL;

    /* create object and assign materials to it's children */
    phx_shape_convex shape = phx_createobj((uptr_t)pxshape, PHX_OBJ_SHAPE_CONVEX);
    for (uint i = 0; i < mtl_cnt; i++)    {
        shape->childs[i] = mtls[i];
        mtls[i]->ref_cnt ++;
    }
    shape->child_cnt = mtl_cnt;

    /* add convexmesh to shape's children */
    convexmesh->ref_cnt ++;
    shape->childs[shape->child_cnt++] = convexmesh;

    /* add shape to rigid object children */
    if (rigid_obj->child_cnt == PHX_CHILD_MAX)  {
        phx_destroy_boxshape(shape);
        return NULL;
    }
    rigid_obj->childs[rigid_obj->child_cnt++] = shape;

    pxshape->userData = convexmesh->user_ptr;   /* assign geo_gpu of mesh to the shape for debug */
    return shape;
}

void phx_destroy_convexshape(phx_shape_convex cmshape)
{
    ASSERT(cmshape->type == PHX_OBJ_SHAPE_CONVEX);

    /* release children */
    for (uint i = 0; i < cmshape->child_cnt-1; i++)
        phx_destroy_mtl(cmshape->childs[i]);

    phx_destroy_convexmesh(cmshape->childs[cmshape->child_cnt-1]);

    phx_destroyobj(cmshape);
}

phx_shape_tri phx_create_trishape(phx_rigid_st rigid_st,
    phx_trimesh trimesh, phx_mtl* mtls, uint mtl_cnt, const struct xform3d* localxf)
{
    ASSERT(rigid_st->type == PHX_OBJ_RIGID_ST || rigid_st->type == PHX_OBJ_RIGID_DYN);

    PxMaterial* pxmtls[PHX_CHILD_MAX];
    mtl_cnt = minui(PHX_CHILD_MAX-1, mtl_cnt);

    for (uint i = 0; i < mtl_cnt; i++)
        pxmtls[i] = (PxMaterial*)mtls[i]->api_obj;

    PxShape* pxshape = ((PxRigidActor*)rigid_st->api_obj)->createShape(
        PxTriangleMeshGeometry((PxTriangleMesh*)trimesh->api_obj), pxmtls, mtl_cnt,
        phx_xftopx(localxf));

    if (pxshape == NULL)
        return NULL;

    /* create object and assign materials to it's children */
    phx_shape_tri shape = phx_createobj((uptr_t)pxshape, PHX_OBJ_SHAPE_TRI);
    for (uint i = 0; i < mtl_cnt; i++)    {
        shape->childs[i] = mtls[i];
        mtls[i]->ref_cnt ++;
    }
    shape->child_cnt = mtl_cnt;

    /* add trimesh to shape's children */
    trimesh->ref_cnt ++;
    shape->childs[shape->child_cnt++] = trimesh;

    /* add shape to rigid object children */
    if (rigid_st->child_cnt == PHX_CHILD_MAX)  {
        phx_destroy_boxshape(shape);
        return NULL;
    }
    rigid_st->childs[rigid_st->child_cnt++] = shape;

    pxshape->userData = trimesh->user_ptr;   /* assign geo_gpu of mesh to the shape for debug */
    return shape;
}

void phx_destroy_trishape(phx_shape_tri tmshape)
{
    ASSERT(tmshape->type == PHX_OBJ_SHAPE_TRI);

    /* release children */
    for (uint i = 0; i < tmshape->child_cnt-1; i++)
        phx_destroy_mtl(tmshape->childs[i]);

    phx_destroy_trimesh(tmshape->childs[tmshape->child_cnt-1]);

    phx_destroyobj(tmshape);
}

phx_convexmesh phx_create_convexmesh(const void* data, bool_t make_gpugeo,
    struct allocator* tmp_alloc, uint thread_id)
{
	phx_stream stream(data);
    PxConvexMesh* pxconvex = g_phxdev.sdk->createConvexMesh(static_cast<PxInputStream&>(stream));
    if (pxconvex == NULL)
        return NULL;

    struct phx_geo_gpu* geo = NULL;
    if (make_gpugeo)    {
        PxHullPolygon pdata;
        uint tri_cnt = 0;
        uint poly_cnt = pxconvex->getNbPolygons();
        uint vert_cnt = pxconvex->getNbVertices();

        for (uint i = 0; i < poly_cnt; i++)   {
            pxconvex->getPolygonData(i, pdata);
            tri_cnt += (pdata.mNbVerts - 2);
        }

        struct vec3f* poss = (struct vec3f*)A_ALLOC(tmp_alloc, sizeof(struct vec3f)*vert_cnt,
            MID_PHX);
        uint16* indexes = (uint16*)A_ALLOC(tmp_alloc, sizeof(uint16)*tri_cnt*3, MID_PHX);
        if (indexes == NULL || poss == NULL)    {
            pxconvex->release();
            return NULL;
        }

        /* build position vertices */
        const PxVec3* src_verts = pxconvex->getVertices();
        for (uint i = 0; i < vert_cnt; i++)
            vec3_setf(&poss[i], src_verts[i].x, src_verts[i].y, src_verts[i].z);

        /* build indexes */
        uint tri_offset = 0;
        const uint8* src_idxs = pxconvex->getIndexBuffer();
        for (uint i = 0; i < poly_cnt; i++)   {
            pxconvex->getPolygonData(i, pdata);
            uint prev_idx1 = 1;
            uint prev_idx2 = 2;

            for (int k = 0; k < (pdata.mNbVerts - 2); k++) {
                uint idx = tri_offset * 3;

                indexes[idx] = src_idxs[pdata.mIndexBase];
                indexes[idx + 1] = src_idxs[pdata.mIndexBase + prev_idx1];
                indexes[idx + 2] = src_idxs[pdata.mIndexBase + prev_idx2];

                prev_idx1 = prev_idx2;
                prev_idx2 = prev_idx1 + 1;
                tri_offset ++;
            }
        }

        geo = phx_creategeo(poss, vert_cnt, indexes, tri_cnt, thread_id);
        A_FREE(tmp_alloc, indexes);
        A_FREE(tmp_alloc, poss);

        if (geo == NULL)    {
            log_print(LOG_WARNING, "phx-device: could not create geometry representation for"
                " convex mesh");
        }
    }

    phx_convexmesh convex = phx_createobj((uptr_t)pxconvex, PHX_OBJ_CONVEXMESH);
    convex->user_ptr = geo; /* assign graphic geo as user_ptr */
    return convex;
}

void phx_destroy_convexmesh(phx_convexmesh convex)
{
    convex->ref_cnt --;
    if (convex->ref_cnt == 0)   {
        struct phx_geo_gpu* geo = (struct phx_geo_gpu*)convex->user_ptr;
        if (geo != NULL)
            phx_destroygeo(geo);

        ((PxConvexMesh*)convex->api_obj)->release();
        phx_destroyobj(convex);
    }
}

phx_trimesh phx_create_trimesh(const void* data, bool_t make_gpugeo, struct allocator* tmp_alloc,
                               uint thread_id)
{
	phx_stream stream(data);
    PxTriangleMesh* pxtri = g_phxdev.sdk->createTriangleMesh(static_cast<PxInputStream&>(stream));
    if (pxtri == NULL)
        return NULL;

    struct phx_geo_gpu* geo = NULL;
    if (make_gpugeo)    {
        uint vert_cnt = pxtri->getNbVertices();
        ASSERT(pxtri->has16BitTriangleIndices());

        struct vec3f* poss = (struct vec3f*)A_ALLOC(tmp_alloc, sizeof(struct vec3f)*vert_cnt,
            MID_PHX);
        if (poss == NULL)   {
            pxtri->release();
            return NULL;
        }

        /* build position vertices */
        const PxVec3* src_verts = pxtri->getVertices();
        for (uint i = 0; i < vert_cnt; i++)
            vec3_setf(&poss[i], src_verts[i].x, src_verts[i].y, src_verts[i].z);

        geo = phx_creategeo(poss, vert_cnt, (const uint16*)pxtri->getTriangles(),
            pxtri->getNbTriangles(), thread_id);
        A_FREE(tmp_alloc, poss);

        if (geo == NULL)    {
            log_print(LOG_WARNING, "phx-device: could not create geometry representation for"
                " triangle mesh");
        }
    }

    phx_trimesh tri = phx_createobj((uptr_t)pxtri, PHX_OBJ_TRIANGLEMESH);
    tri->user_ptr = geo; /* assign graphic geo as user_ptr */
    return tri;
}

void phx_destroy_trimesh(phx_trimesh tri)
{
    tri->ref_cnt --;
    if (tri->ref_cnt == 0)   {
        struct phx_geo_gpu* geo = (struct phx_geo_gpu*)tri->user_ptr;
        if (geo != NULL)
            phx_destroygeo(geo);

        ((PxTriangleMesh*)tri->api_obj)->release();
        phx_destroyobj(tri);
    }
}

struct phx_geo_gpu* phx_creategeo(const struct vec3f* verts, uint vert_cnt,
    const uint16* indexes, uint tri_cnt, uint thread_id)
{
    struct phx_geo_gpu* geo = (struct phx_geo_gpu*)ALLOC(sizeof(struct phx_geo_gpu), MID_PHX);
    if (geo == NULL)
        return NULL;

    memset(geo, 0x00, sizeof(struct phx_geo_gpu));

    geo->tri_cnt = tri_cnt;
    geo->vert_cnt = vert_cnt;

    geo->vb_pos = gfx_create_buffer(GFX_BUFFER_VERTEX, GFX_MEMHINT_STATIC,
        sizeof(struct vec3f)*vert_cnt, verts, thread_id);
    geo->ib = gfx_create_buffer(GFX_BUFFER_INDEX, GFX_MEMHINT_STATIC,
        sizeof(uint16)*tri_cnt*3, indexes, thread_id);
    if (geo->vb_pos == NULL || geo->ib == NULL)  {
        phx_destroygeo(geo);
        return NULL;
    }

    const struct gfx_input_element_binding inputs[] = {
        {GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0, GFX_INPUT_OFFSET_PACKED}
    };

    const struct gfx_input_vbuff_desc vbuffs[] = {
        {sizeof(struct vec3f), geo->vb_pos}
    };

    geo->il = gfx_create_inputlayout(vbuffs, GFX_INPUTVB_GETCNT(vbuffs),
        inputs, GFX_INPUT_GETCNT(inputs), geo->ib, GFX_INDEX_UINT16, thread_id);

    return geo;
}

void phx_destroygeo(struct phx_geo_gpu* geo)
{
    if (geo->il != NULL)
        gfx_destroy_inputlayout(geo->il);

    if (geo->vb_pos != NULL)
        gfx_destroy_buffer(geo->vb_pos);

    if (geo->ib != NULL)
        gfx_destroy_buffer(geo->ib);

    FREE(geo);
}

/*************************************************************************************************/
void phx_rigid_setmass(phx_obj rigid_obj, float mass, OPTIONAL const struct vec3f* local_cmass)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);
    PxVec3 cmass;
    PxRigidBodyExt::setMassAndUpdateInertia(*((PxRigidBody*)rigid_obj->api_obj), mass,
        local_cmass != NULL ? &(cmass = phx_vec3topx(local_cmass)) : NULL);
}

void phx_rigid_setdensity(phx_obj rigid_obj, float density, const struct vec3f* local_cmass)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);
    PxVec3 cmass;
    PxRigidBodyExt::updateMassAndInertia(*((PxRigidBody*)rigid_obj->api_obj), density,
        &(cmass = phx_vec3topx(local_cmass)));
}

void phx_rigid_applyforce(phx_obj rigid_obj, const struct vec3f* force, enum phx_force_mode mode,
    bool_t wakeup, OPTIONAL const struct vec3f* pos)
{
    ((PxRigidBody*)rigid_obj->api_obj)->addForce(phx_vec3topx(force), phx_fmodetopx(mode),
        wakeup != FALSE);
}

void phx_rigid_applyforce_localpos(phx_obj rigid_obj, const struct vec3f* force,
    enum phx_force_mode mode, bool_t wakeup, OPTIONAL const struct vec3f* local_pos)
{
    PxRigidBodyExt::addForceAtLocalPos(*((PxRigidBody*)rigid_obj->api_obj),
        phx_vec3topx(force), phx_vec3topx(local_pos), phx_fmodetopx(mode), wakeup != FALSE);
}

void phx_rigid_applylocalforce_localpos(phx_obj rigid_obj, const struct vec3f* local_force,
    enum phx_force_mode mode, bool_t wakeup, OPTIONAL const struct vec3f* local_pos)
{
    PxRigidBodyExt::addLocalForceAtLocalPos(*((PxRigidBody*)rigid_obj->api_obj),
        phx_vec3topx(local_force), phx_vec3topx(local_pos), phx_fmodetopx(mode), wakeup != FALSE);
}

void phx_rigid_applytorque(phx_obj rigid_obj, const struct vec3f* torque, enum phx_force_mode mode,
    bool_t wakeup)
{
    ((PxRigidBody*)rigid_obj->api_obj)->addTorque(phx_vec3topx(torque), phx_fmodetopx(mode),
        wakeup != FALSE);
}

void phx_rigid_clearforce(phx_obj rigid_obj, enum phx_force_mode mode, bool_t wakeup)
{
    ((PxRigidBody*)rigid_obj->api_obj)->clearForce(phx_fmodetopx(mode), wakeup != FALSE);
}

void phx_rigid_cleartorque(phx_obj rigid_obj, enum phx_force_mode mode, bool_t wakeup)
{
    ((PxRigidBody*)rigid_obj->api_obj)->clearTorque(phx_fmodetopx(mode), wakeup != FALSE);
}

void phx_rigid_freeze(phx_obj rigid_obj, bool_t wakeup)
{
    /* clear any velocities that rigid object has */
    PxRigidBody* rbody = (PxRigidBody*)rigid_obj->api_obj;
    rbody->setAngularVelocity(PxVec3(0.0f, 0.0f, 0.0f), wakeup != FALSE);
    rbody->setLinearVelocity(PxVec3(0.0f, 0.0f, 0.0f), wakeup != FALSE);
}

void phx_rigid_setkinematic(phx_obj rigid_obj, bool_t enable)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);

    ((PxRigidDynamic*)rigid_obj->api_obj)->setRigidDynamicFlag(PxRigidDynamicFlag::eKINEMATIC,
        enable != FALSE);
}

void phx_rigid_setkinamatic_xform(phx_obj rigid_obj, const struct xform3d* xf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);

    ((PxRigidDynamic*)rigid_obj->api_obj)->setKinematicTarget(phx_xftopx(xf));
}

void phx_rigid_setkinamatic_xform3m(phx_obj rigid_obj, const struct mat3f* mat)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);

    ((PxRigidDynamic*)rigid_obj->api_obj)->setKinematicTarget(phx_mat3topx(mat));
}

void phx_rigid_setdamping(phx_obj rigid_obj, float lin_damping, float ang_damping)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);

    PxRigidDynamic* rbody = (PxRigidDynamic*)rigid_obj->api_obj;
    rbody->setLinearDamping(lin_damping);
    rbody->setAngularDamping(ang_damping);
}

void phx_rigid_setsolveritercnt(phx_obj rigid_obj, uint8 positer_min, uint8 veliter_min)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);
    ((PxRigidDynamic*)rigid_obj->api_obj)->setSolverIterationCounts(positer_min, veliter_min);
}

void phx_rigid_enablegravity(phx_obj rigid_obj, bool_t enable)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);
    ((PxRigidDynamic*)rigid_obj->api_obj)->setActorFlag(PxActorFlag::eDISABLE_GRAVITY,
        (bool)!enable);
}

void phx_set_userdata(phx_obj obj, void* data)
{
    switch (obj->type)  {
    case PHX_OBJ_RIGID_DYN:
    case PHX_OBJ_RIGID_ST:
        ((PxActor*)obj->api_obj)->userData = data;
        break;
    case PHX_OBJ_MTL:
        ((PxMaterial*)obj->api_obj)->userData = data;
        break;
    case PHX_OBJ_SHAPE_BOX:
    case PHX_OBJ_SHAPE_TRI:
    case PHX_OBJ_SHAPE_CAPSULE:
    case PHX_OBJ_SHAPE_SPHERE:
    case PHX_OBJ_SHAPE_CONVEX:
    case PHX_OBJ_SHAPE_PLANE:
    case PHX_OBJ_SHAPE_HEIGHTFIELD:
        ((PxShape*)obj->api_obj)->userData = data;
    default:
        ASSERT(0);
    }
}

void phx_draw_rigid(phx_obj obj, const struct color* clr)
{
    ASSERT(obj->type == PHX_OBJ_RIGID_DYN || obj->type == PHX_OBJ_RIGID_ST);

    struct mat3f world;
    struct mat3f mfinal;

    PxRigidActor* rbody = (PxRigidActor*)obj->api_obj;
    phx_tomat3(&world, rbody->getGlobalPose());
    gfx_canvas_setfillcolor_solid(clr);

    for (uint i = 0; i < obj->child_cnt; i++) {
        phx_obj child = obj->childs[i];
        switch (child->type)    {
        case PHX_OBJ_SHAPE_SPHERE:
            {
                PxSphereGeometry pxsphere;
                struct sphere s;
                mat3_mul(&mfinal, phx_tomat3(&mfinal, ((PxShape*)child->api_obj)->getLocalPose()),
                    &world);
                ((PxShape*)child->api_obj)->getSphereGeometry(pxsphere);
                gfx_canvas_sphere(sphere_setf(&s, 0.0f, 0.0f, 0.0f, pxsphere.radius), &mfinal,
                    GFX_SPHERE_MEDIUM);
            }
            break;
        case PHX_OBJ_SHAPE_CAPSULE:
            {
                PxCapsuleGeometry pxcapsule;
                mat3_mul(&mfinal, phx_tomat3(&mfinal, ((PxShape*)child->api_obj)->getLocalPose()),
                    &world);
                ((PxShape*)child->api_obj)->getCapsuleGeometry(pxcapsule);
                gfx_canvas_capsule(pxcapsule.radius, pxcapsule.halfHeight, &mfinal);
            }
            break;
        case PHX_OBJ_SHAPE_BOX:
            {
                PxBoxGeometry pxbox;
                struct aabb bb;
                mat3_mul(&mfinal, phx_tomat3(&mfinal, ((PxShape*)child->api_obj)->getLocalPose()),
                    &world);
                ((PxShape*)child->api_obj)->getBoxGeometry(pxbox);
                gfx_canvas_box(aabb_setf(&bb, -pxbox.halfExtents.x, -pxbox.halfExtents.y,
                    -pxbox.halfExtents.z, pxbox.halfExtents.x, pxbox.halfExtents.y,
                    pxbox.halfExtents.z), &mfinal);
            }
            break;
        case PHX_OBJ_SHAPE_CONVEX:
        case PHX_OBJ_SHAPE_TRI:
            if (((PxShape*)child->api_obj)->userData != NULL)   {
                struct phx_geo_gpu* geo = (struct phx_geo_gpu*)((PxShape*)child->api_obj)->userData;
                mat3_mul(&mfinal, phx_tomat3(&mfinal, ((PxShape*)child->api_obj)->getLocalPose()),
                    &world);
                gfx_canvas_georaw(geo->il, &mfinal, clr, geo->tri_cnt, GFX_INDEX_UINT16);
            }
            break;

        default:
        	break;
        }
    }
}

void phx_getmemstats(struct phx_memstats* stats)
{
    if (g_phxdev.optimize_mem)  {
        stats->buff_alloc = g_phxdev.fs_mem.fl.alloc_size;
        stats->buff_max = g_phxdev.fs_mem.fl.size;
    }   else    {
        stats->buff_alloc = mem_sizebyid(MID_PHX);
        stats->buff_max = 0;
    }
}

void phx_shape_setccd(phx_obj shape, bool_t enable)
{
    PxShape* pxshape = (PxShape*)shape->api_obj;
    pxshape->setFlag(PxShapeFlag::eUSE_SWEPT_BOUNDS, enable != FALSE);
}

/* NOTE: this call is not performance friendly, don't call it in tight loop */
void phx_rigid_setxform(uint scene_id, phx_obj rigid_obj, const struct xform3d* xf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN || rigid_obj->type == PHX_OBJ_RIGID_ST);

    /* transform on object (especially statics) are expensive, don't do un-neseccery transforms */
    PxTransform px_xf = phx_xftopx(xf);
    PxRigidActor* rigid = (PxRigidActor*)rigid_obj->api_obj;
    PxTransform px_prevxf = rigid->getGlobalPose();

    if (math_isequal(px_xf.p.x, px_prevxf.p.x) || math_isequal(px_xf.p.y, px_prevxf.p.y) ||
        math_isequal(px_xf.p.z, px_prevxf.p.z) || math_isequal(px_xf.q.x, px_prevxf.q.x) ||
        math_isequal(px_xf.q.y, px_prevxf.q.y) || math_isequal(px_xf.q.z, px_prevxf.q.z) ||
        math_isequal(px_xf.q.w, px_prevxf.q.w))
    {
        if (rigid_obj->type == PHX_OBJ_RIGID_ST)   {
            PxScene* s = phx_getscene(scene_id);
            s->removeActor(*rigid);
            rigid->setGlobalPose(px_xf);
            s->addActor(*rigid);
        }   else    {
            rigid->setGlobalPose(px_xf);
        }
    }

}

void phx_rigid_setxform_raw(phx_obj rigid_obj, const struct xform3d* xf)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);

    PxRigidActor* rigid = (PxRigidActor*)rigid_obj->api_obj;
    rigid->setGlobalPose(phx_xftopx(xf));
}

void phx_rigid_setvelocity(phx_obj rigid_obj, const struct vec3f* vel_lin,
    const struct vec3f* vel_ang, bool_t wakeup)
{
    ASSERT(rigid_obj->type == PHX_OBJ_RIGID_DYN);

    PxRigidDynamic* rigid = (PxRigidDynamic*)rigid_obj->api_obj;
    rigid->setAngularVelocity(phx_vec3topx(vel_ang), false);
    rigid->setLinearVelocity(phx_vec3topx(vel_lin), wakeup != FALSE);
}

void phx_shape_settrigger(phx_obj shape, bool_t trigger)
{
    PxShape* pxshape = (PxShape*)shape->api_obj;
    pxshape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, trigger == FALSE);
    pxshape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, trigger != FALSE);
}

void phx_shape_modify_box(phx_shape_box box, float hx, float hy, float hz)
{
    ASSERT(box->type == PHX_OBJ_SHAPE_BOX);

    PxShape* pxshape = (PxShape*)box->api_obj;
    pxshape->setGeometry(PxBoxGeometry(hx, hy, hz));
}

void phx_shape_setpose(phx_obj shape, const struct xform3d* xf)
{
    PxShape* pxshape = (PxShape*)shape->api_obj;
    pxshape->setLocalPose(phx_xftopx(xf));
}

void phx_trigger_call(struct phx_scene_data* s, phx_obj trigger, phx_obj other,
    enum phx_trigger_state state)
{
    uint hash = hash_u64((uint64)trigger);
    struct hashtable_item* item = hashtable_open_find(&s->trigger_tbl, hash);
    if (item != NULL)   {
        struct phx_event* e = (struct phx_event*)item->value;
        e->trigger_fn(trigger, other, state, e->param);
    }
}

void phx_trigger_register(uint scene_id, phx_obj trigger, pfn_trigger_callback trigger_fn,
    void* param)
{
    ASSERT(scene_id != 0);
    ASSERT(trigger_fn);

    struct phx_scene_data* s = ((struct phx_scene_data**)g_phxdev.scenes.buffer)[scene_id-1];

    uint hash = hash_u64((uint64)trigger);
    struct hashtable_item* item = hashtable_open_find(&s->trigger_tbl, hash);
    if (item == NULL)   {
        struct phx_event* e = (struct phx_event*)mem_pool_alloc(&s->event_pool);
        ASSERT(e);
        e->trigger_fn = trigger_fn;
        e->param = param;

        hashtable_open_add(&s->trigger_tbl, hash, (uint64)e);
    }   else    {
        struct phx_event* e = (struct phx_event*)item->value;
        e->param = param;
        e->trigger_fn = trigger_fn;
    }
}

void phx_trigger_unregister(uint scene_id, phx_obj trigger)
{
    ASSERT(scene_id != 0);

    struct phx_scene_data* s = ((struct phx_scene_data**)g_phxdev.scenes.buffer)[scene_id-1];
    uint hash = hash_u64((uint64)trigger);
    struct hashtable_item* item = hashtable_open_find(&s->trigger_tbl, hash);
    if (item != NULL)   {
        struct phx_event* e = (struct phx_event*)item->value;
        mem_pool_free(&s->event_pool, e);
        hashtable_open_remove(&s->trigger_tbl, item);
    }
}

