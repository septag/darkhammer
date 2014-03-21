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

#include "mem-ids.h"
#include "res-mgr.h"
#include "cmp-mgr.h"
#include "gfx-model.h"
#include "engine.h"
#include "components/cmp-animchar.h"
#include "components/cmp-model.h"

/*************************************************************************************************
 * fwd declarations
 */
result_t cmp_animchar_create(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_animchar_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_animchar_update(cmp_t c, float dt, void* param);
void cmp_animchar_destroydata(struct cmp_obj* obj, struct cmp_animchar* data, cmphandle_t hdl,
                              bool_t release_ctrl);
result_t cmp_animchar_createhierarchy(struct cmp_animchar* ch, struct cmp_model* m,
    struct gfx_model* gmodel);
void cmp_animchar_destroybind(struct cmp_animchar* ch);
void cmp_animchar_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
                        const struct gfx_view_params* params);

void cmp_animchar_clearinstances(reshandle_t hdl, const struct cmp_instance_desc** insts,
                                 uint cnt);
void cmp_animchar_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
                                    reshandle_t hdl, const struct cmp_instance_desc** insts,
                                    uint cnt);
void cmp_animchar_destroyinstances(reshandle_t hdl, const struct cmp_instance_desc** insts,
                                   uint cnt);

result_t cmp_animchar_createskeleton(struct cmp_animchar* ch, struct cmp_model* m,
    struct gfx_model* gmodel, uint geo_idx);

/*************************************************************************************************/
result_t cmp_animchar_register(struct allocator* alloc)
{
    struct cmp_createparams params;
    memset(&params, 0x00, sizeof(params));

    params.name = "animchar";
    params.stride = sizeof(struct cmp_animchar);
    params.create_func = cmp_animchar_create;
    params.destroy_func = cmp_animchar_destroy;
    params.debug_func = cmp_animchar_debug;
    params.grow_cnt = 300;
    params.initial_cnt = 300;
    params.values = cmp_animchar_values;
    params.value_cnt = CMP_VALUE_CNT(cmp_animchar_values);
    params.type = cmp_animchar_type;
    params.update_funcs[CMP_UPDATE_STAGE1] = cmp_animchar_update;
    params.flags = CMP_FLAG_ALWAYSUPDATE;
    return cmp_register_component(alloc, &params);
}

result_t cmp_animchar_create(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    struct cmp_animchar* ch = (struct cmp_animchar*)data;
    ch->ctrl_hdl = INVALID_HANDLE;

    obj->animchar_cmp = hdl;
    return RET_OK;
}

void cmp_animchar_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    cmp_animchar_destroydata(obj, (struct cmp_animchar*)data, hdl, TRUE);

    obj->animchar_cmp = INVALID_HANDLE;
}

void cmp_animchar_update(cmp_t c, float dt, void* param)
{
    static float tm = 0.0f;
    uint cnt;

    tm += dt;

    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);

    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
    for (uint i = 0; i < cnt; i++)    {
        const struct cmp_instance_desc* inst = updates[i];
        struct cmp_animchar* ch = (struct cmp_animchar*)inst->data;
        if (ch->ctrl_hdl != INVALID_HANDLE) {
            anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
            if (ctrl == NULL)
                continue;

            anim_ctrl_update(ctrl, ch->inst, tm, tmp_alloc);

            if (ch->pose != NULL)  {
                anim_ctrl_fetchresult_skeletal(ch->inst, ch->bindmap, ch->pose->mats,
                    ch->root_idxs, ch->root_cnt, &ch->root_mat);
            }   else    {
                anim_ctrl_fetchresult_hierarchal(ch->inst, ch->bindmap, ch->xform_hdls,
                    ch->root_idxs, ch->root_cnt, &ch->root_mat);
            }

            if (inst->host->model_cmp != INVALID_HANDLE)
                cmp_updateinstance(inst->host->model_cmp);
        }
    }
}

result_t cmp_animchar_modify(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t hdl)
{
    result_t r;
    struct cmp_animchar* ch = (struct cmp_animchar*)data;

    uint filehash = hash_str(ch->filepath);
    bool_t reload = (ch->filepathhash == filehash);
    cmp_animchar_destroydata(obj, ch, hdl, !reload);
    ch->filepathhash = filehash;

    ch->alloc = alloc;
    if (str_isempty(ch->filepath))
        return RET_OK;

    if (!reload)
        ch->ctrl_hdl = rs_load_animctrl(ch->filepath, 0);

    if (ch->ctrl_hdl == INVALID_HANDLE)
        return RET_FAIL;

    anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
    if (ctrl == NULL)
        return RET_OK;

    /* create instance */
    ch->inst = anim_ctrl_createinstance(alloc, ctrl);
    if (ch->inst == NULL)   {
        err_sendtolog(TRUE);
        return RET_FAIL;
    }

    /* create bind data from existing object's model */
    r = cmp_animchar_bind(obj, ch, alloc, tmp_alloc, hdl);
    if (IS_FAIL(r)) {
        err_sendtolog(TRUE);
        log_print(LOG_WARNING, "binding anim-ctrl failed");
        return RET_FAIL;
    }

    return RET_OK;
}

/* bind animation to object, by evaluating object's model component
 * - obj->model_cmp is invalid: wait to model to update (will be called from cmp_model)
 * - obj->model_cmp is skinned: bind skeleton, find first geometry that is skinned, bind to that
 * - obj->model_cmp is not skinned: bind hierarchal animation
 */
result_t cmp_animchar_bind(struct cmp_obj* obj, void* data, struct allocator* alloc,
    struct allocator* tmp_alloc, reshandle_t hdl)
{
    struct cmp_animchar* ch = (struct cmp_animchar*)data;

    if (obj->model_cmp == INVALID_HANDLE)
        return RET_OK;

    struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);
    if (m->model_hdl == INVALID_HANDLE)
        return RET_OK;

    /* choose between skinned and hierarchal
     * try to find an skinned mesh within the model, if not found, try hierarchal */
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
    cmp_animchar_destroybind(ch);
    if (ch->inst == NULL)
        return RET_OK;

    if (geo_idx != INVALID_INDEX)   {
        r = cmp_animchar_createskeleton(ch, m, gmodel, geo_idx);
    }   else    {
        r = cmp_animchar_createhierarchy(ch, m, gmodel);
    }

    return r;
}

/* doesn't actually bind anything, just assigns model pose/xforms to the component */
result_t cmp_animchar_bind_noalloc(struct cmp_obj* obj, cmphandle_t hdl)
{
    struct cmp_animchar* ch = (struct cmp_animchar*)cmp_getinstancedata(hdl);

    if (obj->model_cmp == INVALID_HANDLE)
        return RET_OK;

    struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);
    if (m->model_hdl ==INVALID_HANDLE)
        return RET_OK;

    /* choose between skinned and hierarchal
     * try to find an skinned mesh within the model, if not found, try hierarchal */
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

    ch->pose = NULL;
    ch->xform_hdls = NULL;
    if (geo_idx != INVALID_INDEX)   {
        ch->pose = m->model_inst->poses[geo_idx];
    }   else    {
        ch->xform_hdls = m->xforms;
    }

    return RET_OK;
}


result_t cmp_animchar_createskeleton(struct cmp_animchar* ch, struct cmp_model* m,
    struct gfx_model* gmodel, uint geo_idx)
{
    uint root_idx = 0;
    uint root_nodes[CMP_MESH_XFORM_MAX];
    struct gfx_model_skeleton* sk = gmodel->geos[geo_idx].skeleton;
    ASSERT(sk);

    reshandle_t reel_hdl = anim_ctrl_get_reel(ch->inst);
    if (reel_hdl == INVALID_HANDLE)
        return RET_FAIL;
    anim_reel reel = rs_get_animreel(reel_hdl);
    if (reel == NULL)
        return RET_OK;

    struct anim_reel_desc desc;
    anim_get_desc(&desc, reel);
    ch->bindmap = (uint*)A_ALLOC(ch->alloc, sizeof(uint)*desc.pose_cnt, MID_ANIM);
    if (ch->bindmap == NULL)    {
        cmp_animchar_destroybind(ch);
        return RET_OUTOFMEMORY;
    }

    ch->pose = m->model_inst->poses[geo_idx];
    ASSERT(ch->pose);

    for (uint i = 0, cnt = desc.pose_cnt; i < cnt; i++)   {
        const char* bind_name = anim_get_posebinding(reel, i);
        uint idx = gfx_model_findjoint(sk, bind_name);
        if (idx == INVALID_INDEX) {
            log_printf(LOG_WARNING, "Mapping model '%s' to animation '%s' failed", m->filepath,
                ch->filepath);
            cmp_animchar_destroybind(ch);
            return RET_FAIL;
        }

        ch->bindmap[i] = idx;

        /* collect root joints */
        if (sk->joints[idx].parent_id == INVALID_INDEX) {
            ASSERT(root_idx < CMP_MESH_XFORM_MAX);
            root_nodes[root_idx++] = idx;
        }
    }

    /* setup root matrix and root indexes */
    mat3_setm(&ch->root_mat, &sk->joints_rootmat);
    if (root_idx > 0)   {
        ch->root_idxs = (uint*)A_ALLOC(ch->alloc, sizeof(uint)*root_idx, MID_ANIM);
        if (ch->root_idxs == NULL)   {
            cmp_animchar_destroybind(ch);
            return RET_OUTOFMEMORY;
        }
        for (uint i = 0; i < root_idx; i++)
            ch->root_idxs[i] = root_nodes[i];

        ch->root_cnt = root_idx;
    }

    return RET_OK;
}

result_t cmp_animchar_createhierarchy(struct cmp_animchar* ch, struct cmp_model* m,
    struct gfx_model* gmodel)
{
    uint root_idx = 0;
    uint root_nodes[CMP_MESH_XFORM_MAX];

    reshandle_t reel_hdl = anim_ctrl_get_reel(ch->inst);
    if (reel_hdl == INVALID_HANDLE)
        return RET_FAIL;

    anim_reel reel = rs_get_animreel(reel_hdl);
    ch->xform_hdls = m->xforms;

    /* bind maps */
    struct anim_reel_desc desc;
    anim_get_desc(&desc, reel);
    ch->bindmap = (uint*)A_ALLOC(ch->alloc, sizeof(uint)*desc.pose_cnt, MID_ANIM);
    if (ch->bindmap == NULL)    {
        cmp_animchar_destroybind(ch);
        return RET_OUTOFMEMORY;
    }

    for (uint i = 0; i < desc.pose_cnt; i++)   {
        const char* bind_name = anim_get_posebinding(reel, i);
        uint idx = gfx_model_findnode(gmodel, bind_name);

        if (idx == INVALID_INDEX) {
            log_printf(LOG_WARNING, "Mapping model '%s' to animation '%s' failed", m->filepath,
                ch->filepath);
            cmp_animchar_destroybind(ch);
            return RET_FAIL;
        }

        ch->bindmap[i] = idx;

        /* collect 1st level child nodes (as root nodes) */
        if (gmodel->nodes[idx].parent_id != INVALID_INDEX &&
            gmodel->nodes[gmodel->nodes[idx].parent_id].parent_id == INVALID_INDEX)
        {
            ASSERT(root_idx < CMP_MESH_XFORM_MAX);
            root_nodes[root_idx++] = idx;
        }
    } /* foreach: pose */

    /* setup root matrix and root indexes */
    mat3_setm(&ch->root_mat, &gmodel->root_mat);
    if (root_idx > 0)   {
        ch->root_idxs = (uint*)A_ALLOC(ch->alloc, sizeof(uint)*root_idx, MID_ANIM);
        if (ch->root_idxs == NULL)   {
            cmp_animchar_destroybind(ch);
            return RET_OUTOFMEMORY;
        }
        for (uint i = 0; i < root_idx; i++)
            ch->root_idxs[i] = root_nodes[i];

        ch->root_cnt = root_idx;
    }

    return RET_OK;

}

void cmp_animchar_unbind(cmphandle_t hdl)
{
    struct cmp_animchar* ch = (struct cmp_animchar*)cmp_getinstancedata(hdl);
    cmp_animchar_destroybind(ch);
}

void cmp_animchar_destroybind(struct cmp_animchar* ch)
{
    if (ch->alloc != NULL)    {
        if (ch->bindmap != NULL)    {
            A_FREE(ch->alloc, ch->bindmap);
        }

        if (ch->root_idxs)  {
            A_FREE(ch->alloc, ch->root_idxs);
        }
    }

    ch->root_idxs = NULL;
    ch->pose = NULL;
    ch->bindmap = NULL;
    ch->root_cnt = 0;
}

void cmp_animchar_destroydata(struct cmp_obj* obj, struct cmp_animchar* data, cmphandle_t hdl,
                              bool_t release_ctrl)
{
    cmp_animchar_destroybind(data);

    if (data->inst != NULL) {
        anim_ctrl_destroyinstance(data->inst);
        data->inst = NULL;
    }

    if (release_ctrl && data->ctrl_hdl != INVALID_HANDLE)   {
        rs_unload(data->ctrl_hdl);
        data->filepathhash = 0;
        data->filepath[0] = 0;
        data->ctrl_hdl = INVALID_HANDLE;
    }

    data->alloc = NULL;
}

void cmp_animchar_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
                        const struct gfx_view_params* params)
{
    struct cmp_animchar* ch = (struct cmp_animchar*)data;
    if (ch->ctrl_hdl != INVALID_HANDLE && ch->inst != NULL)
        anim_ctrl_debug(rs_get_animctrl(ch->ctrl_hdl), ch->inst);
}

/*************************************************************************************************
 * reload
 */
void cmp_animchar_reelchanged(reshandle_t reel_hdl)
{
    uint cnt;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);

    cmp_t c = cmp_findtype(cmp_animchar_type);
    const struct cmp_instance_desc** insts = cmp_get_allinstances(c, &cnt, tmp_alloc);

    /* rebind reel to instances with the same reel handle */
    for (uint i = 0; i < cnt; i++)    {
        const struct cmp_instance_desc* inst = insts[i];
        struct cmp_animchar* ch = (struct cmp_animchar*)inst->data;
        if (ch->inst == NULL)
            continue;

        if (reel_hdl == anim_ctrl_get_reel(ch->inst))   {
            anim_ctrl_set_reel(ch->inst, reel_hdl);
            cmp_animchar_destroybind(ch);
            cmp_animchar_bind(inst->host, ch, NULL, tmp_alloc, inst->hdl);
        }
    }
}

void cmp_animchar_reload(const char* filepath, reshandle_t hdl, bool_t manual)
{
    uint cnt;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    cmp_t c = cmp_findtype(cmp_animchar_type);
    const struct cmp_instance_desc** insts = cmp_get_allinstances(c, &cnt, tmp_alloc);

    cmp_animchar_destroyinstances(hdl, insts, cnt);

    reshandle_t nhdl;
    if (!manual)
        nhdl = rs_load_animctrl(filepath, RS_LOAD_REFRESH);
    else
        nhdl = hdl;

    if (nhdl != INVALID_HANDLE)
        cmp_animchar_rebuildhinstances(eng_get_dataalloc(), tmp_alloc, hdl, insts, cnt);
    else
        cmp_animchar_clearinstances(hdl, insts, cnt);

    A_FREE(tmp_alloc, insts);
}

void cmp_animchar_clearinstances(reshandle_t hdl, const struct cmp_instance_desc** insts,
                                 uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        struct cmp_animchar* ch = (struct cmp_animchar*)inst->data;
        if (hdl == ch->ctrl_hdl)
            ch->ctrl_hdl = INVALID_HANDLE;
    }
}

void cmp_animchar_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
                                    reshandle_t hdl, const struct cmp_instance_desc** insts,
                                    uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        const struct cmp_animchar* ch = (const struct cmp_animchar*)inst->data;
        if (hdl == ch->ctrl_hdl)  {
            cmp_animchar_modify(inst->host, alloc, tmp_alloc, inst->data, inst->hdl);
        }
    }
}

void cmp_animchar_destroyinstances(reshandle_t hdl, const struct cmp_instance_desc** insts,
                                   uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        const struct cmp_animchar* ch = (const struct cmp_animchar*)inst->data;
        if (hdl == ch->ctrl_hdl)
            cmp_animchar_destroydata(inst->host, (struct cmp_animchar*)inst->data, inst->hdl, FALSE);
    }
}

void cmp_animchar_setparamb(cmphandle_t hdl, const char* name, bool_t value)
{
    const struct cmp_animchar* ch = (const struct cmp_animchar*)cmp_getinstancedata(hdl);
    if (ch->inst != NULL && ch->ctrl_hdl != INVALID_HANDLE) {
        anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
        if (ctrl != NULL)
            anim_ctrl_set_paramb(ctrl, ch->inst, name, value);
    }
}

void cmp_animchar_setparami(cmphandle_t hdl, const char* name, int value)
{
    const struct cmp_animchar* ch = (const struct cmp_animchar*)cmp_getinstancedata(hdl);
    if (ch->inst != NULL && ch->ctrl_hdl != INVALID_HANDLE) {
        anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
        if (ctrl != NULL)
            anim_ctrl_set_parami(ctrl, ch->inst, name, value);
    }
}

void cmp_animchar_setparamf(cmphandle_t hdl, const char* name, float value)
{
    const struct cmp_animchar* ch = (const struct cmp_animchar*)cmp_getinstancedata(hdl);
    if (ch->inst != NULL && ch->ctrl_hdl != INVALID_HANDLE) {
        anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
        if (ctrl != NULL)
            anim_ctrl_set_paramf(ctrl, ch->inst, name, value);
    }
}

float cmp_animchar_getparamf(cmphandle_t hdl, const char* name)
{
    const struct cmp_animchar* ch = (const struct cmp_animchar*)cmp_getinstancedata(hdl);
    if (ch->inst != NULL && ch->ctrl_hdl != INVALID_HANDLE) {
        anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
        if (ctrl != NULL)
            return anim_ctrl_get_paramf(ctrl, ch->inst, name);
    }
    return 0.0f;
}

int cmp_animchar_getparami(cmphandle_t hdl, const char* name)
{
    const struct cmp_animchar* ch = (const struct cmp_animchar*)cmp_getinstancedata(hdl);
    if (ch->inst != NULL && ch->ctrl_hdl != INVALID_HANDLE) {
        anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
        if (ctrl != NULL)
            return anim_ctrl_get_parami(ctrl, ch->inst, name);
    }
    return 0;
}

bool_t cmp_animchar_getparamb(cmphandle_t hdl, const char* name)
{
    const struct cmp_animchar* ch = (const struct cmp_animchar*)cmp_getinstancedata(hdl);
    if (ch->inst != NULL && ch->ctrl_hdl != INVALID_HANDLE) {
        anim_ctrl ctrl = rs_get_animctrl(ch->ctrl_hdl);
        if (ctrl != NULL)
            return anim_ctrl_get_paramb(ctrl, ch->inst, name);
    }
    return FALSE;
}

