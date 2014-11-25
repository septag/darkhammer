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
#include "dhcore/hash.h"
#include "dhcore/task-mgr.h"

#include "anim.h"
#include "cmp-mgr.h"
#include "res-mgr.h"
#include "gfx-model.h"
#include "anim.h"
#include "mem-ids.h"
#include "engine.h"

#include "components/cmp-anim.h"
#include "components/cmp-model.h"

/*************************************************************************************************
 * fwd declarations
 */
result_t cmp_anim_create(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_anim_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_anim_update(cmp_t c, float dt, void* param);
void cmp_anim_destroydata(struct cmp_obj* obj, struct cmp_anim* data, cmphandle_t hdl,
    int release_mesh);
result_t cmp_anim_createhierarchy(struct cmp_anim* a, struct cmp_model* m, struct gfx_model* gmodel);
void cmp_anim_destroybind(struct cmp_anim* a);
result_t cmp_anim_createskeleton(struct cmp_anim* a, struct cmp_model* m, struct gfx_model* gmodel,
    uint geo_idx);

void cmp_anim_clearinstances(reshandle_t clip_hdl, const struct cmp_instance_desc** insts,
    uint cnt);
void cmp_anim_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
    reshandle_t clip_hdl, const struct cmp_instance_desc** insts, uint cnt);
void cmp_anim_destroyinstances(reshandle_t clip_hdl, const struct cmp_instance_desc** insts,
    uint cnt);

/*************************************************************************************************/
result_t cmp_anim_register(struct allocator* alloc)
{
    struct cmp_createparams params;
    memset(&params, 0x00, sizeof(params));

    params.name = "anim";
    params.stride = sizeof(struct cmp_anim);
    params.create_func = cmp_anim_create;
    params.destroy_func = cmp_anim_destroy;
    //params.debug_func = cmp_anim_debug;
    params.grow_cnt = 300;
    params.initial_cnt = 300;
    params.values = cmp_anim_values;
    params.value_cnt = CMP_VALUE_CNT(cmp_anim_values);
    params.type = cmp_anim_type;
    params.update_funcs[CMP_UPDATE_STAGE1] = cmp_anim_update;
    params.flags = CMP_FLAG_ALWAYSUPDATE;
    return cmp_register_component(alloc, &params);
}

result_t cmp_anim_create(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    struct cmp_anim* anim = (struct cmp_anim*)data;
    anim->play_rate = 1.0f;
    anim->clip_hdl = INVALID_HANDLE;
    anim->clip_id = 0;
    anim->frame_idx = INVALID_INDEX;

    return RET_OK;
}

void cmp_anim_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    cmp_anim_destroydata(obj, (struct cmp_anim*)data, hdl, TRUE);
}

void cmp_anim_update(cmp_t c, float dt, void* param)
{
    uint cnt;
    struct anim_reel_desc desc;
    struct anim_clip_desc clip_desc;

    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
    for (uint i = 0; i < cnt; i++)	{
        const struct cmp_instance_desc* inst = updates[i];
        struct cmp_anim* a = (struct cmp_anim*)inst->data;
#if !defined(_RETAIL_)
        if (a->clip_hdl != INVALID_HANDLE)   {
#endif
            anim_reel reel = rs_get_animreel(a->clip_hdl);
            if (reel == NULL)
                goto anim_reel_proceed;

            float t = a->t;

            anim_get_desc(&desc, reel);
            anim_get_clipdesc(&clip_desc, reel, a->clip_id);

#if !defined(_RETAIL_)
            if (a->frame_idx == INVALID_INDEX)  {
#endif
                if (clip_desc.looped)    {
                    t = fmodf(t, clip_desc.duration);
                }   else if (t > clip_desc.duration)    {
                    a->t = 0.0f;
                    cmp_anim_stop(inst->hdl);
                    continue;
                }
#if !defined(_RETAIL_)
            }   else    {
                t = ((float)a->frame_idx)*desc.ft;
            }
#endif

            if (a->pose != NULL)    {
                anim_update_clip_skeletal(reel, a->clip_id, t, a->bindmap, a->pose->mats,
                    a->frame_idx, a->root_idxs, a->root_cnt, &a->root_mat);
            }   else if (a->xform_hdls != NULL) {
                anim_update_clip_hierarchal(reel, a->clip_id, t, a->bindmap, a->xform_hdls,
                    a->frame_idx, a->root_idxs, a->root_cnt, &a->root_mat);
            }

            if (inst->host->model_cmp != INVALID_HANDLE)
                cmp_updateinstance(inst->host->model_cmp);

#if !defined(_RETAIL_)
        }
#endif
anim_reel_proceed:
        a->t += dt * a->play_rate;
    }
}

result_t cmp_anim_modify(struct cmp_obj* obj, struct allocator* alloc, struct allocator* tmp_alloc,
    void* data, cmphandle_t cur_hdl)
{
    result_t r;
    struct cmp_anim* a = (struct cmp_anim*)data;

    uint filehash = hash_str(a->filepath);
    int reload = (a->filepathhash == filehash);
    cmp_anim_destroydata(obj, a, cur_hdl, !reload);
    a->filepathhash = filehash;

    a->alloc = alloc;
    if (str_isempty(a->filepath))
        return RET_OK;

    if (!reload)
        a->clip_hdl = rs_load_animreel(a->filepath, 0);

    if (a->clip_hdl == INVALID_HANDLE)
        return RET_FAIL;

    /* bind */
    r = cmp_anim_bind(obj, a, alloc, tmp_alloc, cur_hdl);
    if (IS_FAIL(r)) {
        err_sendtolog(TRUE);
        log_print(LOG_WARNING, "binding anim-set failed");
        return RET_FAIL;
    }

    return RET_OK;
}

/* bind animation to object, by evaluating object's model component
 * - obj->model_cmp is invalid: wait to model to update (will be called from cmp_model)
 * - obj->model_cmp is skinned: bind skeleton, find first geometry that is skinned, bind to that
 * - obj->model_cmp is not skinned: bind hierarchal animation
 */
result_t cmp_anim_bind(struct cmp_obj* obj, void* data, struct allocator* alloc,
    struct allocator* tmp_alloc, reshandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)data;

     if (obj->model_cmp == INVALID_HANDLE)
        return RET_OK;

    struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);
    if (m->model_hdl == INVALID_HANDLE)
        return RET_OK;

    /* choose between skinned geometry and hierarchal */
    struct gfx_model* gmodel = rs_get_model(m->model_hdl);
    if (gmodel == NULL)
        return RET_OK;

    uint geo_idx = INVALID_INDEX;
    for (uint i = 0; i < gmodel->geo_cnt; i++)    {
        if (gmodel->geos[i].skeleton != NULL)   {
            geo_idx = i;
            break;
        }
    }

    result_t r = RET_FAIL;
    cmp_anim_destroybind(a);
    if (geo_idx != INVALID_INDEX)   {
        r = cmp_anim_createskeleton(a, m, gmodel, geo_idx);
    }    else   {
        r = cmp_anim_createhierarchy(a, m, gmodel);
    }

    if (IS_OK(r))   {
        cmp_anim_play(hdl);
    }

    return r;
}

/* doesn't actually bind anything, just assigns model pose/xforms to the component */
result_t cmp_anim_bind_noalloc(struct cmp_obj* obj, cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);

    if (obj->model_cmp == INVALID_HANDLE)
        return RET_OK;

    struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);
    if (m->model_hdl == INVALID_HANDLE)
        return RET_OK;

    /* choose between skinned geometry and hierarchal */
    struct gfx_model* gmodel = rs_get_model(m->model_hdl);
    if (gmodel == NULL)
        return RET_OK;

    uint geo_idx = INVALID_INDEX;
    for (uint i = 0; i < gmodel->geo_cnt; i++)    {
        if (gmodel->geos[i].skeleton != NULL)   {
            geo_idx = i;
            break;
        }
    }

    a->pose = NULL;
    a->xform_hdls = NULL;
    if (geo_idx != INVALID_INDEX)   {
        a->pose = m->model_inst->poses[geo_idx];
    }   else    {
        a->xform_hdls = m->xforms;
    }

    return RET_OK;
}


result_t cmp_anim_createskeleton(struct cmp_anim* a, struct cmp_model* m, struct gfx_model* gmodel,
                                 uint geo_idx)
{
    uint root_idx = 0;
    uint root_nodes[CMP_MESH_XFORM_MAX];

    struct gfx_model_skeleton* sk = gmodel->geos[geo_idx].skeleton;
    ASSERT(sk);

    anim_reel reel = rs_get_animreel(a->clip_hdl);
    if (reel == NULL)
        return RET_OK;

    struct anim_reel_desc desc;
    anim_get_desc(&desc, reel);
    a->bindmap = (uint*)A_ALLOC(a->alloc, sizeof(uint)*desc.pose_cnt, MID_ANIM);
    if (a->bindmap == NULL)    {
        cmp_anim_destroybind(a);
        return RET_OUTOFMEMORY;
    }

    a->pose = m->model_inst->poses[geo_idx];
    ASSERT(a->pose);

    for (uint i = 0, cnt = desc.pose_cnt; i < cnt; i++)   {
        const char* bind_name = anim_get_posebinding(reel, i);
        uint idx = gfx_model_findjoint(sk, bind_name);
        if (idx == INVALID_INDEX) {
            log_printf(LOG_WARNING, "Mapping model '%s' to animation '%s' failed", m->filepath,
                a->filepath);
            cmp_anim_destroybind(a);
            return RET_FAIL;
        }

        /* collect root joints */
        if (sk->joints[idx].parent_id == INVALID_INDEX) {
            ASSERT(root_idx < CMP_MESH_XFORM_MAX);
            root_nodes[root_idx++] = idx;
        }

        a->bindmap[i] = idx;
    }

    /* setup root matrix and root indexes */
    mat3_setm(&a->root_mat, &sk->joints_rootmat);
    if (root_idx > 0)   {
        a->root_idxs = (uint*)A_ALLOC(a->alloc, sizeof(uint)*root_idx, MID_ANIM);
        if (a->root_idxs == NULL)   {
            cmp_anim_destroybind(a);
            return RET_OUTOFMEMORY;
        }
        for (uint i = 0; i < root_idx; i++)
            a->root_idxs[i] = root_nodes[i];

        a->root_cnt = root_idx;
    }

    return RET_OK;
}

result_t cmp_anim_createhierarchy(struct cmp_anim* a, struct cmp_model* m, struct gfx_model* gmodel)
{
    uint root_idx = 0;
    uint root_nodes[CMP_MESH_XFORM_MAX];

    a->xform_hdls = m->xforms;

    /* bind maps */
    anim_reel reel = rs_get_animreel(a->clip_hdl);
    if (reel == NULL)
        return RET_OK;

    struct anim_reel_desc desc;
    anim_get_desc(&desc, reel);
    a->bindmap = (uint*)A_ALLOC(a->alloc, sizeof(uint)*desc.pose_cnt, MID_ANIM);
    if (a->bindmap == NULL)    {
        cmp_anim_destroybind(a);
        return RET_OUTOFMEMORY;
    }

    for (uint i = 0; i < desc.pose_cnt; i++)   {
        const char* bind_name = anim_get_posebinding(reel, i);
        uint idx = gfx_model_findnode(gmodel, bind_name);

        if (idx == INVALID_INDEX) {
            log_printf(LOG_WARNING, "Mapping model '%s' to animation '%s' failed", m->filepath,
                a->filepath);
            cmp_anim_destroybind(a);
            return RET_FAIL;
        }

        a->bindmap[i] = idx;

        /* collect 1st level child nodes (as root nodes) */
        if (gmodel->nodes[idx].parent_id != INVALID_INDEX &&
            gmodel->nodes[gmodel->nodes[idx].parent_id].parent_id == INVALID_INDEX)
        {
            ASSERT(root_idx < CMP_MESH_XFORM_MAX);
            root_nodes[root_idx++] = idx;
        }
    } /* foreach: pose */

    /* setup root matrix and root indexes */
    mat3_setm(&a->root_mat, &gmodel->root_mat);
    if (root_idx > 0)   {
        a->root_idxs = (uint*)A_ALLOC(a->alloc, sizeof(uint)*root_idx, MID_ANIM);
        if (a->root_idxs == NULL)   {
            cmp_anim_destroybind(a);
            return RET_OUTOFMEMORY;
        }
        for (uint i = 0; i < root_idx; i++)
            a->root_idxs[i] = root_nodes[i];

        a->root_cnt = root_idx;
    }

    return RET_OK;
}

void cmp_anim_unbind(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    cmp_anim_destroybind(a);
}

void cmp_anim_destroybind(struct cmp_anim* a)
{
    if (a->alloc != NULL)    {
        if (a->bindmap != NULL) {
            A_FREE(a->alloc, a->bindmap);
        }

        if (a->root_idxs != NULL)   {
            A_FREE(a->alloc, a->root_idxs);
        }
    }

    a->root_idxs = NULL;
    a->pose = NULL;
    a->bindmap = NULL;
    a->root_cnt = 0;
}

void cmp_anim_play(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    a->frame_idx = INVALID_INDEX;
    a->playing = TRUE;

    cmp_updateinstance(hdl);
}

void cmp_anim_stop(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    a->playing = FALSE;

    cmp_updateinstance_reset(hdl);
}

void cmp_anim_destroydata(struct cmp_obj* obj, struct cmp_anim* data, cmphandle_t hdl,
    int release_clip)
{
    cmp_anim_stop(hdl);

    cmp_anim_destroybind(data);

    if (release_clip && data->clip_hdl != INVALID_HANDLE)   {
        rs_unload(data->clip_hdl);
        data->filepathhash = 0;
        data->filepath[0] = 0;
        data->clip_hdl = INVALID_HANDLE;
    }

    data->alloc = NULL;
}

void cmp_anim_reload(const char* filepath, reshandle_t hdl, int manual)
{
    uint cnt;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    cmp_t c = cmp_findtype(cmp_anim_type);
    const struct cmp_instance_desc** insts = cmp_get_allinstances(c, &cnt, tmp_alloc);
    if (insts != NULL)  {
        /* reload model and refresh all model component data */
        cmp_anim_destroyinstances(hdl, insts, cnt);
        reshandle_t nhdl;
        if (!manual)
            nhdl = rs_load_animreel(filepath, RS_LOAD_REFRESH);
        else
            nhdl = hdl;

        if (nhdl != INVALID_HANDLE)
            cmp_anim_rebuildhinstances(eng_get_dataalloc(), tmp_alloc, hdl, insts, cnt);
        else
            cmp_anim_clearinstances(hdl, insts, cnt);  /* this happens when model-reload fails,
                                                          we have to invalidate handles */

        A_FREE(tmp_alloc, insts);
    }
}

void cmp_anim_destroyinstances(reshandle_t clip_hdl, const struct cmp_instance_desc** insts,
    uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        const struct cmp_anim* a = (const struct cmp_anim*)inst->data;
        if (clip_hdl == a->clip_hdl)
            cmp_anim_destroydata(inst->host, (struct cmp_anim*)inst->data, inst->hdl, FALSE);
    }
}

void cmp_anim_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
    reshandle_t clip_hdl, const struct cmp_instance_desc** insts, uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        const struct cmp_anim* a = (const struct cmp_anim*)inst->data;
        if (clip_hdl == a->clip_hdl)  {
            cmp_anim_modify(inst->host, alloc, tmp_alloc, inst->data, inst->hdl);
        }
    }
}

void cmp_anim_clearinstances(reshandle_t clip_hdl, const struct cmp_instance_desc** insts,
    uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        struct cmp_anim* a = (struct cmp_anim*)inst->data;
        if (clip_hdl == a->clip_hdl)
            a->clip_hdl = INVALID_HANDLE;
    }
}

result_t cmp_anim_modify_clip(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)data;
    if (a->clip_hdl != INVALID_HANDLE)   {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)   {
            a->clip_id = 0;
            a->t = 0.0f;
            return RET_OK;
        }

        uint clip_idx = anim_findclip(reel, a->clip_name);
        if (clip_idx != INVALID_INDEX)   {
            a->clip_id = clip_idx;
        } else  {
            a->clip_id = 0;
            log_printf(LOG_WARNING, "Animation clip '%s' does not exist in '%s'", a->clip_name,
                a->filepath);
        }
    }
    a->t = 0.0f;
    return RET_OK;
}

uint cmp_anim_getframecnt(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)
            return 0;

        struct anim_reel_desc desc;
        anim_get_desc(&desc, reel);
        return desc.frame_cnt;
    }   else    {
        return 0;
    }
}

result_t cmp_anim_modify_frame(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)data;
    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)   {
            a->frame_idx = INVALID_INDEX;
            return RET_OK;
        }

        struct anim_reel_desc desc;
        anim_get_desc(&desc, reel);

        a->frame_idx = clampui(a->frame_idx, 0, desc.frame_cnt-1);
        a->playing = FALSE;
        cmp_updateinstance(cur_hdl);
    }  else {
        a->frame_idx = INVALID_INDEX;
    }
    return RET_OK;
}

int cmp_anim_isplaying(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    return a->playing;
}

uint cmp_anim_getcurframe(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    if (a->frame_idx != INVALID_INDEX)
        return a->frame_idx;

    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)
            return 0;

        struct anim_reel_desc desc;
        anim_get_desc(&desc, reel);
        return clampui((uint)(a->t/desc.ft), 0, desc.frame_cnt-1);
    }  else {
        return 0;
    }
}

uint cmp_anim_getfps(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)
            return 0;

        struct anim_reel_desc desc;
        anim_get_desc(&desc, reel);
        return desc.fps;
    }   else    {
        return 0;
    }
}

uint cmp_anim_getclipcnt(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)
            return 0;

        struct anim_reel_desc desc;
        anim_get_desc(&desc, reel);
        return desc.clip_cnt;
    }    else   {
        return 0;
    }
}

const char* cmp_anim_getclipname(cmphandle_t hdl, uint clip_idx)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)
            return "";

        struct anim_clip_desc clip_desc;
        anim_get_clipdesc(&clip_desc, reel, clip_idx);
        return clip_desc.name;
    }    else   {
        return "";
    }
}

uint cmp_anim_getbonecnt(cmphandle_t hdl)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)
            return 0;
        struct anim_reel_desc rdesc;
        anim_get_desc(&rdesc, reel);
        return rdesc.pose_cnt;
    }

    return 0;
}

const char* cmp_anim_getbonename(cmphandle_t hdl, uint bone_idx)
{
    struct cmp_anim* a = (struct cmp_anim*)cmp_getinstancedata(hdl);
    if (a->clip_hdl != INVALID_HANDLE)  {
        anim_reel reel = rs_get_animreel(a->clip_hdl);
        if (reel == NULL)
            return 0;
        return anim_get_posebinding(reel, bone_idx);
    }
    return "";
}
