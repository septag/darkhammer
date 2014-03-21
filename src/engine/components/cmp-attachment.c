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

#include "components/cmp-attachment.h"
#include "components/cmp-attachdock.h"
#include "components/cmp-xform.h"

#include "cmp-mgr.h"
#include "scene-mgr.h"
#include "gfx-canvas.h"

/*************************************************************************************************
 * fwd declarations
 */
void cmp_attachment_destroydata(struct cmp_obj* obj, struct cmp_attachment* data, cmphandle_t hdl);
result_t cmp_attachment_create(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_attachment_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_attachment_update(cmp_t c, float dt, void* params);
void cmp_attachment_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params);

/*************************************************************************************************/
result_t cmp_attachment_register(struct allocator* alloc)
{
    struct cmp_createparams params;
    memset(&params, 0x00, sizeof(params));

    params.name = "attachment";
    params.flags = CMP_FLAG_DEFERREDMODIFY;
    params.stride = sizeof(struct cmp_attachment);
    params.initial_cnt = 100;
    params.grow_cnt = 100;
    params.values = cmp_attachment_values;
    params.value_cnt = CMP_VALUE_CNT(cmp_attachment_values);
    params.type = cmp_attachment_type;
    params.create_func = cmp_attachment_create;
    params.destroy_func = cmp_attachment_destroy;
    params.debug_func = cmp_attachment_debug;
    params.update_funcs[CMP_UPDATE_STAGE2] = cmp_attachment_update; /* dependency update (xforms) */

    return cmp_register_component(alloc, &params);
}

void cmp_attachment_unlink(cmphandle_t attach_hdl)
{
    struct cmp_attachment* att = (struct cmp_attachment*)cmp_getinstancedata(attach_hdl);
    cmp_attachment_destroydata(cmp_getinstancehost(attach_hdl), att, attach_hdl);
    att->attachto[0] = 0;
    att->dock_slot = 0;
    att->target_obj_id = 0;
}

void cmp_attachment_destroydata(struct cmp_obj* obj, struct cmp_attachment* data, cmphandle_t hdl)
{
    if (data->dock_hdl != INVALID_HANDLE)   {
        ASSERT(data->dock_slot < CMP_ATTACHDOCK_MAX);

        struct cmp_attachdock* attdock = (struct cmp_attachdock*)cmp_getinstancedata(data->dock_hdl);
        struct cmp_attachdock_dockdesc* dock = &attdock->docks[data->dock_slot];

        if (obj->xform_cmp != INVALID_HANDLE)       {
            struct cmp_xform* cxf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);

            /* calculate world matrix and set it to current object's transform component */
            struct mat3f ws_mat;
            mat3_setm(&ws_mat, &cxf->mat);
            cmphandle_t parent = cxf->parent_hdl;
            while (parent != INVALID_HANDLE)    {
                struct cmp_xform* cxf_parent = (struct cmp_xform*)cmp_getinstancedata(parent);
                mat3_mul(&ws_mat, &ws_mat, &cxf_parent->mat);
                parent = cxf_parent->parent_hdl;
            }

            mat3_setm(&cxf->mat, &ws_mat);

            /* clear current object's transform parent */
            cxf->parent_hdl = INVALID_HANDLE;
            cmp_updateinstance(obj->xform_cmp);
        }

        dock->attachment_hdl = INVALID_HANDLE;
        data->dock_hdl = INVALID_HANDLE;
    }
}

result_t cmp_attachment_create(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    struct cmp_attachment* att = (struct cmp_attachment*)data;
    att->dock_hdl = INVALID_HANDLE;

    if (obj != NULL)
        obj->attach_cmp = hdl;

    return RET_OK;
}

void cmp_attachment_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    cmp_attachment_destroydata(obj, (struct cmp_attachment*)data, hdl);

    if (obj != NULL)
        obj->attach_cmp = INVALID_HANDLE;
}

void cmp_attachment_update(cmp_t c, float dt, void* params)
{
    uint cnt;
    const struct cmp_instance_desc** insts = cmp_get_updateinstances(c, &cnt);
    for (uint i = 0; i < cnt; i++)    {
        const struct cmp_instance_desc* inst = insts[i];

        cmp_updateinstance(inst->host->xform_cmp);

        /* because we have passes dep update of xform, we have to do it manually */
        cmp_xform_updatedeps(inst->host, inst->flags);
    }
}

result_t cmp_attachment_modifyattachto(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_attachment* att = (struct cmp_attachment*)data;

    cmp_attachment_destroydata(obj, att, cur_hdl);
    if (str_isempty(att->attachto))
        return RET_OK;

    uint target_id = scn_findobj(obj->scene_id, att->attachto);
    if (target_id == 0) {
        log_printf(LOG_WARNING, "cmp-attachment failed: could not find object '%s'", att->attachto);
        cmp_attachment_destroydata(obj, att, cur_hdl);
        att->attachto[0] = 0;
        return RET_FAIL;
    }

    struct cmp_obj* target = scn_getobj(obj->scene_id, target_id);
    cmphandle_t dock_hdl = target->attachdock_cmp;
    if (dock_hdl == INVALID_HANDLE) {
        log_printf(LOG_WARNING, "cmp-attachment failed: target '%s' does not have attachdock",
            att->attachto);
        cmp_attachment_destroydata(obj, att, cur_hdl);
        att->attachto[0] = 0;
        return RET_FAIL;
    }

    att->target_obj_id = target_id;
    att->dock_hdl = dock_hdl;

    return RET_OK;
}

result_t cmp_attachment_modifydockslot(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_attachment* att = (struct cmp_attachment*)data;
    if (att->dock_hdl == INVALID_HANDLE)
        return RET_FAIL;

    att->dock_slot = clampun(att->dock_slot, 0, CMP_ATTACHDOCK_MAX-1);

    struct cmp_obj* target = scn_getobj(obj->scene_id, att->target_obj_id);
    cmphandle_t dock_hdl = target->attachdock_cmp;

    /* modify 'parent' of the transform component and assign attachment to target dock */
    struct cmp_xform* cxf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)cmp_getinstancedata(dock_hdl);
    attdock->docks[att->dock_slot].attachment_hdl = cur_hdl;
    cxf->parent_hdl = attdock->docks[att->dock_slot].xform_hdl;
    mat3_setidentity(&cxf->mat);

    cmp_updateinstance(obj->xform_cmp);
    return RET_OK;
}


void cmp_attachment_attach(struct cmp_obj* obj, struct cmp_obj* target, const char* dockname)
{
    if (obj->attach_cmp != INVALID_HANDLE && target->attachdock_cmp != INVALID_HANDLE)  {
        struct cmp_attachment* att = (struct cmp_attachment*)cmp_getinstancedata(obj->attach_cmp);
        struct cmp_attachdock* attdock = (struct cmp_attachdock*)
            cmp_getinstancedata(target->attachdock_cmp);

        uint dockname_hash = hash_str(dockname);
        for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
            if (attdock->bindto_hashes[i] != dockname_hash)
                continue;
            strcpy(att->attachto, target->name);
            att->dock_slot = i;
            cmp_attachment_modifyattachto(obj, NULL, NULL, att, obj->attach_cmp);
            cmp_attachment_modifydockslot(obj, NULL, NULL, att, obj->attach_cmp);
            break;
        }
    }
}

void cmp_attachment_detach(struct cmp_obj* obj)
{
    if (obj->attach_cmp != INVALID_HANDLE)  {
        struct cmp_attachment* att = (struct cmp_attachment*)cmp_getinstancedata(obj->attach_cmp);
        cmp_attachment_destroydata(obj, att, obj->attach_cmp);
        att->attachto[0] = 0;
        att->dock_slot = 0;
        att->target_obj_id = 0;
    }
}


void cmp_attachment_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params)
{
    struct cmp_attachment* att = (struct cmp_attachment*)data;
    if (att->dock_hdl != INVALID_HANDLE)   {
        cmphandle_t src_xform = obj->xform_cmp;
        cmphandle_t dest_xform = cmp_getinstancehost(att->dock_hdl)->xform_cmp;
        if (dest_xform != INVALID_HANDLE)   {
            struct cmp_xform* src_cxf = (struct cmp_xform*)cmp_getinstancedata(src_xform);
            struct cmp_xform* dest_cxf = (struct cmp_xform*)cmp_getinstancedata(dest_xform);
            struct vec3f src_pos;
            struct vec3f dest_pos;

            gfx_canvas_setztest(FALSE);
            gfx_canvas_setlinecolor(&g_color_blue);
            gfx_canvas_arrow3d(
                mat3_get_transv(&src_pos, &src_cxf->ws_mat),
                mat3_get_transv(&dest_pos, &dest_cxf->ws_mat),
                &g_vec3_unity, 0.03f);
            gfx_canvas_setztest(TRUE);
        }
    }
}
