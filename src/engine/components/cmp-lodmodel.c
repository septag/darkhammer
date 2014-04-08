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

#include "dhcore/core.h"

#include "components/cmp-lodmodel.h"
#include "components/cmp-model.h"
#include "components/cmp-bounds.h"
#include "components/cmp-xform.h"
#include "components/cmp-attachdock.h"
#include "components/cmp-anim.h"
#include "components/cmp-animchar.h"

#include "cmp-mgr.h"
#include "lod-scheme.h"
#include "camera.h"
#include "gfx-model.h"

#define LOD_INDEX_HIGH 0
#define LOD_INDEX_MED 1
#define LOD_INDEX_LOW 2

/*************************************************************************************************
 * fwd declarations
 */
result_t cmp_lodmodel_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_lodmodel_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
result_t lodmodel_loadmodel(struct cmp_obj* obj, cmphandle_t model_hdl, const char* filepath,
    struct allocator* alloc, struct allocator* tmp_alloc, bool_t update_bounds);
void lodmodel_buildidxs(struct cmp_lodmodel* lodmodel);
cmphandle_t lodmodel_switchmodel(cmphandle_t cur_hdl, cmphandle_t new_hdl, bool_t* is_changed);
result_t lodmodel_loadmodel(struct cmp_obj* obj, cmphandle_t model_hdl, const char* filepath,
    struct allocator* alloc, struct allocator* tmp_alloc, bool_t update_bounds);
void cmp_lodmodel_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
        const struct gfx_view_params* params);
void cmp_lodmodel_updatedeps(struct cmp_obj* obj);

/*************************************************************************************************
 * inlines
 */
INLINE bool_t lodmodel_checkmodel(struct cmp_lodmodel* lodmodel, uint lod_idx)
{
    return (lodmodel->models[LOD_INDEX_LOW] != INVALID_HANDLE) &&
        (((struct cmp_model*)cmp_getinstancedata(lodmodel->models[LOD_INDEX_LOW]))->model_hdl !=
        INVALID_HANDLE);
}

/*************************************************************************************************/
result_t cmp_lodmodel_register(struct allocator* alloc)
{
    struct cmp_createparams params;
    memset(&params, 0x00, sizeof(params));

    params.name = "lod-model";
    params.stride = sizeof(struct cmp_lodmodel);
    params.create_func = cmp_lodmodel_create;
    params.destroy_func = cmp_lodmodel_destroy;
    params.debug_func = cmp_lodmodel_debug;
    params.grow_cnt = 300;
    params.initial_cnt = 300;
    params.values = cmp_lodmodel_values;
    params.value_cnt = CMP_VALUE_CNT(cmp_lodmodel_values);
    params.type = cmp_lodmodel_type;
    return cmp_register_component(alloc, &params);
}

result_t cmp_lodmodel_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
    /* object should not have model attached */
    if (cmp_findinstance(host_obj->chain, cmp_model_type) != INVALID_HANDLE)    {
        log_printf(LOG_WARNING, "creating lod-model failed: object '%s' already assigned a model",
            host_obj->name);
        return RET_FAIL;
    }

    struct cmp_lodmodel* m = (struct cmp_lodmodel*)data;
    for (uint i = 0; i < CMP_LOD_MODELS_MAX; i++) {
        m->models[i] = INVALID_HANDLE;
        m->lod_idxs[i] = 0;
    }
    strcpy(m->scheme_name, "default");
    m->scheme_id = lod_findmodelscheme(m->scheme_name);

    host_obj->model_cmp = INVALID_HANDLE;
    host_obj->model_shadow_cmp = INVALID_HANDLE;
    return RET_OK;
}

void cmp_lodmodel_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)data;
    for (uint i = 0; i < CMP_LOD_MODELS_MAX; i++) {
        m->models[i] = INVALID_HANDLE;
        m->lod_idxs[i] = 0;
    }
    host_obj->model_cmp = INVALID_HANDLE;
    host_obj->model_shadow_cmp = INVALID_HANDLE;
}

bool_t cmp_lodmodel_applylod(cmphandle_t lodmdl_hdl, const struct vec3f* campos)
{
    struct cmp_obj* host = cmp_getinstancehost(lodmdl_hdl);
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)cmp_getinstancedata(lodmdl_hdl);
    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(host->bounds_cmp);
    const struct lod_model_scheme* scheme = lod_getmodelscheme(m->scheme_id);

    /* calculate distance factors to the bounds of object */
    struct vec3f d;
    vec3_setf(&d, campos->x - b->ws_s.x, campos->y - b->ws_s.y, campos->z - b->ws_s.z);
    float r = b->ws_s.r;
    float dot_d = vec3_dot(&d, &d);

    bool_t changed;

    /* test high detail */
    float l = scheme->high_range + r;
    if (dot_d < l*l)    {
        host->model_cmp = lodmodel_switchmodel(host->model_cmp,
            m->models[m->lod_idxs[LOD_INDEX_HIGH]], &changed);
        if (changed)
            cmp_lodmodel_updatedeps(host);
        return TRUE;
    }

    /* test medium detail */
    l = scheme->medium_range + r;
    if (dot_d < l*l)    {
        host->model_cmp = lodmodel_switchmodel(host->model_cmp,
            m->models[m->lod_idxs[LOD_INDEX_MED]], &changed);
        if (changed)
            cmp_lodmodel_updatedeps(host);
        return TRUE;
    }

    /* test low detail */
    l = scheme->low_range + r;
    if (dot_d < l*l)    {
        host->model_cmp = lodmodel_switchmodel(host->model_cmp,
            m->models[m->lod_idxs[LOD_INDEX_LOW]], &changed);
        if (changed)
            cmp_lodmodel_updatedeps(host);
        return TRUE;
    }   else    {
        return FALSE;   /* too far, doesn't get rendered anymore */
    }
}

/* same as normal applylod, but uses 1 level lower LOD for every range (faster for shadows) */
bool_t cmp_lodmodel_applylod_shadow(cmphandle_t lodmdl_hdl, const struct vec3f* campos)
{
    struct cmp_obj* host = cmp_getinstancehost(lodmdl_hdl);
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)cmp_getinstancedata(lodmdl_hdl);
    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(host->bounds_cmp);
    const struct lod_model_scheme* scheme = lod_getmodelscheme(m->scheme_id);

    /* calculate distance factors to the bounds of object */
    struct vec3f d;
    vec3_setf(&d, campos->x - b->ws_s.x, campos->y - b->ws_s.y, campos->z - b->ws_s.z);
    float r = b->ws_s.r;
    float dot_d = vec3_dot(&d, &d);

    /* test high detail */
    bool_t changed;
    float l = scheme->high_range + r;
    if (dot_d < l*l)    {
        host->model_shadow_cmp = lodmodel_switchmodel(host->model_shadow_cmp,
            m->models[m->lod_idxs[LOD_INDEX_MED]], &changed);
        return TRUE;
    }

    /* test medium detail */
    l = scheme->medium_range + r;
    if (dot_d < l*l)    {
        host->model_shadow_cmp = lodmodel_switchmodel(host->model_shadow_cmp,
            m->models[m->lod_idxs[LOD_INDEX_LOW]], &changed);
        return TRUE;
    }

    /* test low detail */
    l = scheme->low_range + r;
    if (dot_d < l*l)    {
        host->model_shadow_cmp = lodmodel_switchmodel(host->model_shadow_cmp,
            m->models[m->lod_idxs[LOD_INDEX_LOW]], &changed);
        return TRUE;
    }   else    {
        return FALSE;   /* too far, doesn't get rendered anymore */
    }
}


cmphandle_t lodmodel_switchmodel(cmphandle_t cur_hdl, cmphandle_t new_hdl, bool_t* is_changed)
{
    if (cur_hdl == new_hdl) {
        *is_changed = FALSE;
        return cur_hdl;
    }

    *is_changed = TRUE;

    struct cmp_model* cur = (struct cmp_model*)cmp_getinstancedata(cur_hdl);
    struct cmp_model* n = (struct cmp_model*)cmp_getinstancedata(new_hdl);

    /* copy current transforms into new one */
    uint xf_max = minui(cur->xform_cnt, n->xform_cnt);
    for (uint i = 1; i < xf_max; i++) {
        struct cmp_xform* xfc = (struct cmp_xform*)cmp_getinstancedata(cur->xforms[i]);
        struct cmp_xform* xfn = (struct cmp_xform*)cmp_getinstancedata(n->xforms[i]);
        mat3_setm(&xfn->mat, &xfc->mat);
        vec3_setv(&xfn->vel_lin, &xfc->vel_lin);
        vec3_setv(&xfn->vel_ang, &xfc->vel_ang);
    }

    /* copy current poses into new one */
    struct gfx_model_instance* cur_inst = cur->model_inst;
    ASSERT(cur_inst);
    struct gfx_model_instance* inst = n->model_inst;
    ASSERT(inst);
    uint pose_max = minui(inst->pose_cnt, cur_inst->pose_cnt);
    for (uint i = 0; i < pose_max; i++)   {
        if (inst->poses[i] != NULL) {
            memcpy(inst->poses[i]->mats, cur_inst->poses[i]->mats,
                sizeof(struct mat3f)*minui(inst->poses[i]->mat_cnt, cur_inst->poses[i]->mat_cnt));
        }
    }

    cmp_updateinstance(new_hdl);
    return new_hdl;
}

void cmp_lodmodel_updatedeps(struct cmp_obj* obj)
{
    if (obj->attachdock_cmp != INVALID_HANDLE)
        cmp_attachdock_refresh(obj->attachdock_cmp);

    cmphandle_t anim_cmp = cmp_findinstance(obj->chain, cmp_anim_type);
    if (anim_cmp != INVALID_HANDLE)
        cmp_anim_bind_noalloc(obj, anim_cmp);

    if (obj->animchar_cmp != INVALID_HANDLE)
        cmp_animchar_bind_noalloc(obj, obj->animchar_cmp);
}

result_t cmp_lodmodel_modify_hi(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)data;
    if (m->models[LOD_INDEX_HIGH] == INVALID_HANDLE) {
        m->models[LOD_INDEX_HIGH] = cmp_create_instance(cmp_findtype(cmp_model_type), obj,
            CMP_INSTANCEFLAG_INDIRECTHOST,
            cur_hdl, (uint)offsetof(struct cmp_lodmodel, models[LOD_INDEX_HIGH]));
    }

    if (m->models[LOD_INDEX_HIGH] == INVALID_HANDLE)
        return RET_FAIL;
    obj->model_cmp = m->models[LOD_INDEX_HIGH];
    obj->model_shadow_cmp = m->models[LOD_INDEX_HIGH];
    return lodmodel_loadmodel(obj, m->models[LOD_INDEX_HIGH], m->filepath_hi, alloc,
        tmp_alloc, TRUE);
}

result_t cmp_lodmodel_modify_md(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)data;
    if (m->models[LOD_INDEX_MED] == INVALID_HANDLE) {
        m->models[LOD_INDEX_MED] = cmp_create_instance(cmp_findtype(cmp_model_type), NULL,
            CMP_INSTANCEFLAG_INDIRECTHOST,
            cur_hdl, (uint)offsetof(struct cmp_lodmodel, models[LOD_INDEX_MED]));
    }

    if (m->models[LOD_INDEX_MED] == INVALID_HANDLE)
        return RET_FAIL;
    result_t r = lodmodel_loadmodel(obj, m->models[LOD_INDEX_MED], m->filepath_md, alloc, tmp_alloc,
        FALSE);
    if (IS_OK(r))
        lodmodel_buildidxs(m);
    return r;
}

result_t cmp_lodmodel_modify_lo(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)data;
    if (m->models[LOD_INDEX_LOW] == INVALID_HANDLE) {
        m->models[LOD_INDEX_LOW] = cmp_create_instance(cmp_findtype(cmp_model_type), NULL,
            CMP_INSTANCEFLAG_INDIRECTHOST,
            cur_hdl, (uint)offsetof(struct cmp_lodmodel, models[LOD_INDEX_LOW]));
    }

    if (m->models[LOD_INDEX_LOW] == INVALID_HANDLE)
        return RET_FAIL;
    result_t r = lodmodel_loadmodel(obj, m->models[LOD_INDEX_LOW], m->filepath_lo, alloc, tmp_alloc,
        FALSE);
    if (IS_OK(r))
        lodmodel_buildidxs(m);
    return r;
}

void lodmodel_buildidxs(struct cmp_lodmodel* lodmodel)
{
    if (lodmodel_checkmodel(lodmodel, LOD_INDEX_LOW))    {
        lodmodel->lod_idxs[LOD_INDEX_LOW] = LOD_INDEX_LOW;
    }   else if (lodmodel_checkmodel(lodmodel, LOD_INDEX_MED))   {
        lodmodel->lod_idxs[LOD_INDEX_LOW] = LOD_INDEX_MED;
    }   else    {
        lodmodel->lod_idxs[LOD_INDEX_LOW] = LOD_INDEX_HIGH;
    }

    if (lodmodel_checkmodel(lodmodel, LOD_INDEX_MED))   {
        lodmodel->lod_idxs[LOD_INDEX_MED] = LOD_INDEX_MED;
    }   else    {
        lodmodel->lod_idxs[LOD_INDEX_MED] = LOD_INDEX_HIGH;
    }

    lodmodel->lod_idxs[LOD_INDEX_HIGH] = LOD_INDEX_HIGH;
}

result_t lodmodel_loadmodel(struct cmp_obj* obj, cmphandle_t model_hdl, const char* filepath,
    struct allocator* alloc, struct allocator* tmp_alloc, bool_t update_bounds)
{
    struct cmp_model* mdl = (struct cmp_model*)cmp_getinstancedata(model_hdl);
    strcpy(mdl->filepath, filepath);
    if (!update_bounds)
        BIT_ADD(mdl->flags, CMP_MODELFLAG_NOBOUNDUPDATE);
    BIT_ADD(mdl->flags, CMP_MODELFLAG_ISLOD);
    return cmp_model_modify(obj, alloc, tmp_alloc, mdl, model_hdl);
}

result_t cmp_lodmodel_modify_shadows(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)data;
    for (uint i = 0; i < CMP_LOD_MODELS_MAX; i++) {
        if (m->models[i] != INVALID_HANDLE)
            cmp_value_setb(m->models[i], "exclude_shadows", m->exclude_shadows);
    }
    return RET_OK;
}

result_t cmp_lodmodel_modify_scheme(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_lodmodel* m = (struct cmp_lodmodel*)data;
    uint id = lod_findmodelscheme(m->scheme_name);
    if (id == 0) {
        log_printf(LOG_WARNING, "lod-model: lod-scheme '%s' does not exist", m->scheme_name);
        return RET_FAIL;
    }

    m->scheme_id = id;
    return RET_OK;
}

void cmp_lodmodel_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
        const struct gfx_view_params* params)
{
    if (obj->model_cmp != INVALID_HANDLE)
        cmp_model_debug(obj, cmp_getinstancedata(obj->model_cmp), obj->model_cmp, dt, params);
}
