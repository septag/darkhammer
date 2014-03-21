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
#include "dhcore/color.h"
#include "dhcore/task-mgr.h"

#include "components/cmp-rbody.h"
#include "components/cmp-xform.h"

#include "phx-prefab.h"
#include "res-mgr.h"
#include "phx-device.h"
#include "engine.h"
#include "cmp-mgr.h"
#include "scene-mgr.h"
#include "gfx-canvas.h"

/*************************************************************************************************
 * fwd declare
 */
result_t cmp_rbody_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_rbody_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_rbody_update(cmp_t c, float dt, void* param);
void cmp_rbody_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params);
void cmp_rbody_destroydata(struct cmp_obj* host_obj, struct cmp_rbody* data, cmphandle_t hdl,
    bool_t release_phx);

void cmp_rbody_destroyinstances(reshandle_t prefab_hdl, const struct cmp_instance_desc** insts,
    uint cnt);
void cmp_rbody_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
    reshandle_t prefab_hdl, const struct cmp_instance_desc** insts, uint cnt);
void cmp_rbody_clearinstances(reshandle_t prefab_hdl, const struct cmp_instance_desc** insts,
    uint cnt);

/*************************************************************************************************/
result_t cmp_rbody_register(struct allocator* alloc)
{
    struct cmp_createparams params;
    memset(&params, 0x00, sizeof(params));

    params.name = "rbody";
    params.stride = sizeof(struct cmp_rbody);
    params.create_func = cmp_rbody_create;
    params.destroy_func = cmp_rbody_destroy;
    params.update_funcs[CMP_UPDATE_STAGE4] = cmp_rbody_update;
    params.grow_cnt = 300;
    params.initial_cnt = 300;
    params.values = cmp_rbody_values;
    params.value_cnt = CMP_VALUE_CNT(cmp_rbody_values);
    params.type = cmp_rbody_type;
    params.debug_func = cmp_rbody_debug;
    return cmp_register_component(alloc, &params);
}

result_t cmp_rbody_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
    struct cmp_rbody* rb = (struct cmp_rbody*)data;
    rb->prefab_hdl = INVALID_HANDLE;
    rb->collision_filter = 0xFFFFFFFF;

    if (host_obj != NULL)
        host_obj->rbody_cmp = hdl;
    return RET_OK;
}

void cmp_rbody_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
    cmp_rbody_destroydata(host_obj, (struct cmp_rbody*)data, hdl, TRUE);
    if (host_obj != NULL)
        host_obj->rbody_cmp = INVALID_HANDLE;
}

result_t cmp_rbody_modifyfile(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_rbody* rb = (struct cmp_rbody*)data;
    uint filehash = hash_str(rb->filepath);
    bool_t reload = (rb->filepath_hash == filehash);
    rb->filepath_hash = filehash;

    /* destroy data before loading anything */
    cmp_rbody_destroydata(obj, (struct cmp_rbody*)data, cur_hdl, !reload);

    if (str_isempty(rb->filepath))
        return RET_OK;

    if (!reload)
        rb->prefab_hdl = rs_load_phxprefab(rb->filepath, 0);

    if (rb->prefab_hdl == INVALID_HANDLE)
        return RET_FAIL;

    /* actor transform (world-space) is obtained from xform component */
    phx_prefab prefab = rs_get_phxprefab(rb->prefab_hdl);
    if (prefab == NULL)
        return RET_OK;

    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    struct xform3d pose;
    xform3d_frommat3(&pose, &xf->ws_mat);
    rb->rbody = phx_createinstance(prefab, &pose);
    if (rb->rbody == NULL)  {
        log_print(LOG_WARNING, "modify-rbody failed: could not create rbody instance");
        cmp_rbody_destroydata(obj, (struct cmp_rbody*)data, cur_hdl, TRUE);
        return RET_FAIL;
    }

    /* connect rbody user-data to host (for xform update) */
    rb->rbody->user_ptr = obj;

    /* add rigid to physics scene */
    uint px_sceneid = scn_getphxscene(obj->scene_id);
    if (px_sceneid != 0)
        phx_scene_addactor(px_sceneid, rb->rbody);
    rb->px_sceneid = px_sceneid;
    return RET_OK;
}

result_t cmp_rbody_modifykinematic(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_rbody* rb = (struct cmp_rbody*)data;
    if (rb->rbody != NULL)  {
        if (rb->rbody->type == PHX_OBJ_RIGID_ST)
            rb->kinamtic = FALSE;
        else
            phx_rigid_setkinematic(rb->rbody, rb->kinamtic);
    }

    return RET_OK;
}

result_t cmp_rbody_modifydgravity(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_rbody* rb = (struct cmp_rbody*)data;
    if (rb->rbody != NULL)  {
        if (rb->rbody->type == PHX_OBJ_RIGID_ST)
            rb->disable_gravity = FALSE;
        else
            phx_rigid_enablegravity(rb->rbody, !rb->disable_gravity);
    }

    return RET_OK;
}

result_t cmp_rbody_modifycolfilter(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    return RET_OK;
}

void cmp_rbody_destroydata(struct cmp_obj* host_obj, struct cmp_rbody* data, cmphandle_t hdl,
    bool_t release_phx)
{
    if (data->rbody != NULL)    {
        if (data->px_sceneid != 0)
            phx_scene_removeactor(data->px_sceneid, data->rbody);
        phx_destroy_rigid(data->rbody);
        data->rbody = NULL;
    }

    if (release_phx && data->prefab_hdl != INVALID_HANDLE)    {
        rs_unload(data->prefab_hdl);
        data->filepath[0] = 0;
        data->prefab_hdl = INVALID_HANDLE;
        data->filepath_hash = 0;
    }
}

void cmp_rbody_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params)
{
    gfx_canvas_setalpha(0.5f);

    struct cmp_rbody* rb = (struct cmp_rbody*)data;
    if (rb->rbody != NULL)  {
        struct color clr;
        if (rb->rbody->type == PHX_OBJ_RIGID_ST)
            color_setc(&clr, &g_color_blue);
        else
            color_setc(&clr, &g_color_yellow);

        phx_draw_rigid(rb->rbody, &clr);
    }

    gfx_canvas_setalpha(1.0f);
}

void cmp_rbody_update(cmp_t c, float dt, void* param)
{
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
    for (uint i = 0; i < cnt; i++)	{
        const struct cmp_instance_desc* inst = updates[i];
        struct cmp_rbody* rb = (struct cmp_rbody*)inst->data;
        /* assign kinamatic transform */
        if (rb->kinamtic)   {
            struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(inst->host->xform_cmp);
            phx_rigid_setkinamatic_xform3m(rb->rbody, &xf->ws_mat);
        }
    }
}

void cmp_rbody_reload(const char* filepath, reshandle_t hdl, bool_t manual)
{
    uint cnt;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);

    cmp_t c = cmp_findtype(cmp_rbody_type);
    const struct cmp_instance_desc** insts = cmp_get_allinstances(c, &cnt, tmp_alloc);

    /* reload model and refresh all model component data */
    cmp_rbody_destroyinstances(hdl, insts, cnt);
    reshandle_t nhdl;
    if (!manual)
        nhdl = rs_load_phxprefab(filepath, RS_LOAD_REFRESH);
    else
        nhdl = hdl;

    if (nhdl != INVALID_HANDLE)
        cmp_rbody_rebuildhinstances(eng_get_dataalloc(), tmp_alloc, hdl, insts, cnt);
    else
        cmp_rbody_clearinstances(hdl, insts, cnt);  /* this happens when model-reload fails,
                                                     * we have to invalidate handles */
}

void cmp_rbody_destroyinstances(reshandle_t prefab_hdl, const struct cmp_instance_desc** insts,
    uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        struct cmp_rbody* rb = (struct cmp_rbody*)inst->data;
        if (prefab_hdl == rb->prefab_hdl)
            cmp_rbody_destroydata(inst->host, rb, inst->hdl, FALSE);
    }
}

void cmp_rbody_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
    reshandle_t prefab_hdl, const struct cmp_instance_desc** insts, uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        struct cmp_rbody* rb = (struct cmp_rbody*)inst->data;
        if (prefab_hdl == rb->prefab_hdl)  {
            cmp_rbody_modifyfile(inst->host, alloc, tmp_alloc, rb, inst->hdl);
        }
    }
}

void cmp_rbody_clearinstances(reshandle_t prefab_hdl, const struct cmp_instance_desc** insts,
    uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        struct cmp_rbody* rb = (struct cmp_rbody*)inst->data;
        if (prefab_hdl == rb->prefab_hdl)  {
            rb->prefab_hdl = INVALID_HANDLE;
            rb->filepath_hash = 0;
            rb->filepath[0] = 0;
        }
    }
}
