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

#include "components/cmp-attachdock.h"
#include "components/cmp-attachment.h"
#include "components/cmp-model.h"
#include "components/cmp-xform.h"

#include "cmp-mgr.h"
#include "gfx-canvas.h"

/*************************************************************************************************
 * fwd declaration
 */
void cmp_attachdock_destroydata(struct cmp_obj* obj, struct cmp_attachdock* data, cmphandle_t hdl);
result_t cmp_attachdock_create(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_attachdock_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl);
void cmp_attachdock_update(cmp_t c, float dt, void* params);
void cmp_attachdock_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params);


/*************************************************************************************************/
result_t cmp_attachdock_register(struct allocator* alloc)
{
    struct cmp_createparams params;
    memset(&params, 0x00, sizeof(params));

    params.name = "attachdock";
    params.flags = CMP_FLAG_DEFERREDMODIFY;
    params.stride = sizeof(struct cmp_attachdock);
    params.initial_cnt = 100;
    params.grow_cnt = 100;
    params.values = cmp_attachdock_values;
    params.value_cnt = CMP_VALUE_CNT(cmp_attachdock_values);
    params.type = cmp_attachdock_type;
    params.create_func = cmp_attachdock_create;
    params.destroy_func = cmp_attachdock_destroy;
    params.debug_func = cmp_attachdock_debug;
    params.update_funcs[CMP_UPDATE_STAGE2] = cmp_attachdock_update; /* dependency update (attachments) */

    return cmp_register_component(alloc, &params);
}

result_t cmp_attachdock_create(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)data;
    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        attdock->docks[i].xform_hdl = INVALID_HANDLE;
        attdock->docks[i].attachment_hdl = INVALID_HANDLE;
    }

    if (obj != NULL)
        obj->attachdock_cmp = hdl;

    return RET_OK;
}

void cmp_attachdock_destroy(struct cmp_obj* obj, void* data, cmphandle_t hdl)
{
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)data;
    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        if (attdock->docks[i].attachment_hdl != INVALID_HANDLE)
            cmp_attachment_unlink(attdock->docks[i].attachment_hdl);
    }

    cmp_attachdock_destroydata(obj, attdock, hdl);

    if (obj != NULL)
        obj->attachdock_cmp = INVALID_HANDLE;
}

result_t cmp_attachdock_modify(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)data;
    cmp_attachdock_destroydata(obj, attdock, cur_hdl);

    if (obj->model_cmp == INVALID_HANDLE)
        return RET_FAIL;

    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        if (str_isempty(attdock->bindto[i]))
            continue;

        uint name_hash = hash_str(attdock->bindto[i]);
        attdock->bindto_hashes[i] = name_hash;

        cmphandle_t xform_hdl = cmp_model_findnode(obj->model_cmp, name_hash);
        if (xform_hdl != INVALID_HANDLE)    {
            attdock->docks[i].type = CMP_ATTACHDOCK_NORMAL;
            attdock->docks[i].xform_hdl = xform_hdl;
        }

        /* rebind the attachment */
        cmphandle_t att_hdl = attdock->docks[i].attachment_hdl;
        if (att_hdl != INVALID_HANDLE)  {
            cmp_attachment_modifydockslot(cmp_getinstancehost(att_hdl), NULL, NULL,
                cmp_getinstancedata(att_hdl), att_hdl);
        }
    }

    return RET_OK;
}

void cmp_attachdock_clear(cmphandle_t attdock_hdl)
{
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)cmp_getinstancedata(attdock_hdl);

    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        attdock->docks[i].type = CMP_ATTACHDOCK_NONE;
        attdock->docks[i].xform_hdl = INVALID_HANDLE;
    }
}

void cmp_attachdock_refresh(cmphandle_t attdock_hdl)
{
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)cmp_getinstancedata(attdock_hdl);
    struct cmp_obj* obj = cmp_getinstancehost(attdock_hdl);

    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        cmphandle_t xform_hdl = cmp_model_findnode(obj->model_cmp, attdock->bindto_hashes[i]);
        if (xform_hdl != INVALID_HANDLE)    {
            attdock->docks[i].type = CMP_ATTACHDOCK_NORMAL;
            attdock->docks[i].xform_hdl = xform_hdl;
        }   else    {
            attdock->docks[i].type = CMP_ATTACHDOCK_NONE;
            attdock->docks[i].xform_hdl = INVALID_HANDLE;
        }

        /* rebind the attachment */
        cmphandle_t att_hdl = attdock->docks[i].attachment_hdl;
        if (att_hdl != INVALID_HANDLE)  {
            cmp_attachment_modifydockslot(cmp_getinstancehost(att_hdl), NULL, NULL,
                cmp_getinstancedata(att_hdl), att_hdl);
        }
    }
}


void cmp_attachdock_destroydata(struct cmp_obj* obj, struct cmp_attachdock* data, cmphandle_t hdl)
{
    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        if (data->docks[i].type == CMP_ATTACHDOCK_BONE)
            cmp_destroy_instance(data->docks[i].xform_hdl);

        data->docks[i].type = CMP_ATTACHDOCK_NONE;
        data->docks[i].xform_hdl = INVALID_HANDLE;
    }
}

/* attachment dependency update */
void cmp_attachdock_update(cmp_t c, float dt, void* params)
{
    uint cnt;
    const struct cmp_instance_desc** insts = cmp_get_updateinstances(c, &cnt);
    for (uint i = 0; i < cnt; i++)    {
        const struct cmp_instance_desc* inst = insts[i];
        struct cmp_attachdock* attdock = (struct cmp_attachdock*)inst->data;
        for (uint k = 0; k < CMP_ATTACHDOCK_MAX; k++) {
            cmphandle_t attach_hdl = attdock->docks[k].attachment_hdl;
            if (attach_hdl != INVALID_HANDLE)
                cmp_updateinstance(attach_hdl);
        }
    }
}

void cmp_attachdock_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params)
{
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)data;

    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        if (!str_isempty(attdock->bindto[i]) && attdock->docks[i].type != CMP_ATTACHDOCK_NONE)  {
            struct cmp_xform* cxf = (struct cmp_xform*)
                cmp_getinstancedata(attdock->docks[i].xform_hdl);
            struct vec3f pos;
            mat3_get_transv(&pos, &cxf->ws_mat);

            gfx_canvas_setztest(TRUE);
            gfx_canvas_text3d(attdock->bindto[i], &pos, &params->viewproj);
            gfx_canvas_setztest(FALSE);
            gfx_canvas_coords(&cxf->ws_mat, &params->cam_pos, 0.2f);
        }
    }

    gfx_canvas_setztest(TRUE);
}

void cmp_attachdock_unlinkall(cmphandle_t attdock_hdl)
{
    struct cmp_attachdock* attdock = (struct cmp_attachdock*)cmp_getinstancedata(attdock_hdl);
    for (uint i = 0; i < CMP_ATTACHDOCK_MAX; i++) {
        if (attdock->docks[i].attachment_hdl != INVALID_HANDLE)
            cmp_attachment_unlink(attdock->docks[i].attachment_hdl);
    }
}
