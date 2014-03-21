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

#include "components/cmp-trigger.h"
#include "components/cmp-xform.h"

#include "phx-device.h"
#include "scene-mgr.h"
#include "cmp-mgr.h"
#include "gfx-canvas.h"
#include "script.h"

/*************************************************************************************************
 * fwd declare
 */
result_t cmp_trigger_create(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_trigger_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_trigger_update(cmp_t c, float dt, void* param);
void cmp_trigger_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params);
phx_obj cmp_trigger_createrbody(struct cmp_obj* obj, const struct vec3f* box, bool_t is_static,
    const struct xform3d* xf, const struct xform3d* shape_xf);

void cmp_trigger_callback(phx_obj trigger, phx_obj other, enum phx_trigger_state state,
    void* param);


/*************************************************************************************************/
result_t cmp_trigger_register(struct allocator* alloc)
{
    struct cmp_createparams params;
    memset(&params, 0x00, sizeof(params));

    params.name = "trigger";
    params.stride = sizeof(struct cmp_trigger);
    params.create_func = cmp_trigger_create;
    params.destroy_func = cmp_trigger_destroy;
    params.update_funcs[CMP_UPDATE_STAGE4] = cmp_trigger_update;
    params.grow_cnt = 300;
    params.initial_cnt = 300;
    params.values = cmp_trigger_values;
    params.value_cnt = CMP_VALUE_CNT(cmp_trigger_values);
    params.type = cmp_trigger_type;
    params.debug_func = cmp_trigger_debug;
    return cmp_register_component(alloc, &params);
}

result_t cmp_trigger_create(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    struct cmp_trigger* trigger = (struct cmp_trigger*)data;

    vec3_setf(&trigger->box, 1.0f, 1.0f, 1.0f);

    /* create default box */
    struct xform3d xf;
    if (obj->xform_cmp != INVALID_HANDLE)   {
        struct cmp_xform* cxf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
        xform3d_frommat3(&xf, &cxf->ws_mat);
    }   else    {
        xform3d_setidentity(&xf);
    }

    struct xform3d shape_xf;
    vec3_setzero(&trigger->local_pos);
    quat_setidentity(&trigger->local_rot);

    trigger->rbody = cmp_trigger_createrbody(obj, &trigger->box, FALSE, &xf,
        xform3d_setpq(&shape_xf, &trigger->local_pos, &trigger->local_rot));
    if (trigger->rbody == NULL)
        return RET_FAIL;

    trigger->collision_filter = 0xFFFFFFFF;

    /* add to physics scene */
    trigger->px_sceneid = scn_getphxscene(obj->scene_id);
    if (trigger->px_sceneid != 0)
        phx_scene_addactor(trigger->px_sceneid, trigger->rbody);

    if (obj != NULL)
        obj->trigger_cmp = hdl;

    return RET_OK;
}

void cmp_trigger_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    struct cmp_trigger* trigger = (struct cmp_trigger*)data;

    if (trigger->rbody != NULL) {
        /* unregister event */
        if (trigger->trigger_fn != NULL)
            phx_trigger_unregister(trigger->px_sceneid, trigger->rbody);

        if (trigger->px_sceneid != 0)
            phx_scene_removeactor(trigger->px_sceneid, trigger->rbody);

        phx_destroy_rigid(trigger->rbody);
        trigger->rbody = NULL;
    }

    /* is it from the script ? */
    if (trigger->param != NULL)
        sct_removetrigger((struct sct_trigger_event*)trigger->param);

    if (obj != NULL)
        obj->trigger_cmp = INVALID_HANDLE;
}

phx_obj cmp_trigger_createrbody(struct cmp_obj* obj, const struct vec3f* box, bool_t is_static,
    const struct xform3d* xf, const struct xform3d* shape_xf)
{
    phx_obj rbody = NULL;
    if (!is_static) {
        rbody = phx_create_rigid_dyn(xf);
        phx_rigid_enablegravity(rbody, FALSE);
    }   else    {
        rbody = phx_create_rigid_st(xf);
    }

    if (rbody == NULL)
        return NULL;

    phx_mtl mtl = phx_create_mtl(0.5f, 0.5f, 0.1f);
    ASSERT(mtl);

    /* box shape */
    phx_shape_box boxshape = phx_create_boxshape(rbody, box->x*0.5f, box->y*0.5f, box->z*0.5f,
        &mtl, 1, shape_xf);
    phx_destroy_mtl(mtl);
    if (boxshape == NULL)    {
        phx_destroy_rigid(rbody);
        return NULL;
    }

    /* set trigger shape(s) */
    phx_shape_settrigger(boxshape, TRUE);

    /* connect rbody user-data to host (for xform update) */
    rbody->user_ptr = obj;

    return rbody;
}

result_t cmp_trigger_modifybox(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_trigger* trigger = (struct cmp_trigger*)data;

    if (trigger->rbody == NULL) {
        log_print(LOG_WARNING, "modify-trigger failed: trigger object is not created");
        return RET_FAIL;
    }

    ASSERT(trigger->rbody->child_cnt == 1);
    phx_shape_modify_box(trigger->rbody->childs[0], trigger->box.x*0.5f, trigger->box.y*0.5f,
        trigger->box.z*0.5f);

    return RET_OK;
}

result_t cmp_trigger_modifylocalpos(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_trigger* trigger = (struct cmp_trigger*)data;
    if (trigger->rbody == NULL) {
        log_print(LOG_WARNING, "modify-trigger failed: trigger object is not created");
        return RET_FAIL;
    }

    struct xform3d shape_xf;
    ASSERT(trigger->rbody->child_cnt == 1);
    phx_shape_setpose(trigger->rbody->childs[0], xform3d_setpq(&shape_xf, &trigger->local_pos,
        &trigger->local_rot));

    return RET_OK;
}

result_t cmp_trigger_modifylocalrot(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_trigger* trigger = (struct cmp_trigger*)data;
    if (trigger->rbody == NULL) {
        log_print(LOG_WARNING, "modify-trigger failed: trigger object is not created");
        return RET_FAIL;
    }

    struct xform3d shape_xf;
    ASSERT(trigger->rbody->child_cnt == 1);
    phx_shape_setpose(trigger->rbody->childs[0], xform3d_setpq(&shape_xf, &trigger->local_pos,
        &trigger->local_rot));

    return RET_OK;
}

result_t cmp_trigger_modifystatic(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_trigger* trigger = (struct cmp_trigger*)data;

    if (trigger->rbody == NULL) {
        log_print(LOG_WARNING, "modify-trigger failed: trigger object is not created");
        return RET_FAIL;
    }

    struct xform3d xf;
    struct xform3d shape_xf;
    if (obj->xform_cmp != INVALID_HANDLE)   {
        struct cmp_xform* cxf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
        xform3d_frommat3(&xf, &cxf->ws_mat);
    }   else    {
        xform3d_setidentity(&xf);
    }


    /* remove from physics scene and recreate trigger object */
    if (trigger->px_sceneid != 0)   {
        /* remove trigger event */
        if (trigger->trigger_fn != NULL)
            phx_trigger_unregister(trigger->px_sceneid, trigger->rbody);

        phx_scene_removeactor(trigger->px_sceneid, trigger->rbody);
    }

    phx_destroy_rigid(trigger->rbody);
    trigger->rbody = cmp_trigger_createrbody(obj, &trigger->box, trigger->is_static, &xf,
        xform3d_setpq(&shape_xf, &trigger->local_pos, &trigger->local_rot));
    if (trigger->rbody == NULL) {
        log_print(LOG_WARNING, "modify-trigger failed: could not create trigger object");
        return RET_FAIL;
    }

    /* push back to physics scene */
    if (trigger->px_sceneid != 0)  {
        phx_scene_addactor(trigger->px_sceneid, trigger->rbody);

        if (trigger->trigger_fn != NULL)    {
            phx_trigger_register(trigger->px_sceneid, trigger->rbody, cmp_trigger_callback,
                trigger->param);
        }
    }

    return RET_OK;
}

result_t cmp_trigger_modifycolfilter(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    return RET_OK;
}

void cmp_trigger_update(cmp_t c, float dt, void* param)
{
    struct xform3d xf;
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
    for (uint i = 0; i < cnt; i++)	{
        const struct cmp_instance_desc* inst = updates[i];
        struct cmp_trigger* trigger = (struct cmp_trigger*)inst->data;
        /* assign kinamatic transform */
        if (!trigger->is_static == 0)   {
            struct cmp_xform* cxf = (struct cmp_xform*)cmp_getinstancedata(inst->host->xform_cmp);
            phx_rigid_setxform_raw(trigger->rbody, xform3d_frommat3(&xf, &cxf->ws_mat));
        }
    }
}

void cmp_trigger_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params)
{
    gfx_canvas_setalpha(0.4f);

    struct cmp_trigger* trigger = (struct cmp_trigger*)data;
    if (trigger->rbody != NULL)  {
        struct color clr;
        if (!trigger->triggered)
            color_setc(&clr, &g_color_green);
        else
            color_setc(&clr, &g_color_red);

        phx_draw_rigid(trigger->rbody, &clr);
    }

    gfx_canvas_setalpha(1.0f);
}

void cmp_trigger_register_callback(cmphandle_t trigger_cmp, pfn_cmp_trigger_callback trigger_fn,
    void* param)
{
    ASSERT(trigger_cmp != INVALID_HANDLE);

    struct cmp_trigger* trigger = (struct cmp_trigger*)cmp_getinstancedata(trigger_cmp);
    if (trigger->rbody != NULL && trigger->px_sceneid != 0) {
        phx_trigger_register(trigger->px_sceneid, trigger->rbody, cmp_trigger_callback, param);
        trigger->trigger_fn = trigger_fn;
        trigger->param = param;
    }
}

void cmp_trigger_callback(phx_obj trigger, phx_obj other, enum phx_trigger_state state,
    void* param)
{
    struct cmp_obj* trigger_obj = (struct cmp_obj*)trigger->user_ptr;
    struct cmp_obj* other_obj = (struct cmp_obj*)trigger->user_ptr;

    ASSERT(trigger_obj->trigger_cmp != INVALID_HANDLE);

    struct cmp_trigger* data = (struct cmp_trigger*)cmp_getinstancedata(trigger_obj->trigger_cmp);
    ASSERT(data->trigger_fn);

    /* call trigger callback */
    data->triggered = (state == PHX_TRIGGER_IN);
    data->trigger_fn(trigger_obj, other_obj, state, param);
}

void cmp_trigger_unregister_callback(cmphandle_t trigger_cmp)
{
    struct cmp_trigger* trigger = (struct cmp_trigger*)cmp_getinstancedata(trigger_cmp);
    if (trigger->rbody != NULL && trigger->px_sceneid != 0) {
        phx_trigger_unregister(trigger->px_sceneid, trigger->rbody);
        trigger->trigger_fn = NULL;
        trigger->param = NULL;
    }
}
