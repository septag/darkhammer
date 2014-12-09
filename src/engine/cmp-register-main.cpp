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

#include "dheng/engine.h"
#include "dheng/components/cmp-xform.h"
#include "dheng/components/cmp-bounds.h"
#include "dheng/components/cmp-model.h"
#include "dheng/components/cmp-light.h"
#include "dheng/components/cmp-lodmodel.h"
#include "dheng/components/cmp-rbody.h"
#include "dheng/components/cmp-trigger.h"
#include "dheng/components/cmp-attachdock.h"
#include "dheng/components/cmp-attachment.h"
#include "dheng/components/cmp-anim.h"
#include "dheng/components/cmp-animchar.h"
#include "dheng/components/cmp-camera.h"

result_t cmp_register_main_components()
{
    result_t r;

    struct allocator* alloc = mem_heap();

    r = cmp_xform_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_bounds_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_attachdock_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_attachment_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_model_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_anim_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_animchar_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_light_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_lodmodel_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    /* physics related */
    if (!BIT_CHECK(eng_get_params()->flags, static_cast<uint>(appEngineFlags::DISABLE_PHYSICS)))  {
        r = cmp_rbody_register(alloc);
        if (IS_FAIL(r))
            return RET_FAIL;
    }

    r = cmp_trigger_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    r = cmp_camera_register(alloc);
    if (IS_FAIL(r))
        return RET_FAIL;

    return RET_OK;
}

void cmp_setcommonhdl(struct cmp_obj* obj, cmphandle_t hdl, cmptype_t type)
{
    if (type == cmp_xform_type)
        obj->xform_cmp = hdl;
    else if (type == cmp_bounds_type)
        obj->bounds_cmp = hdl;
    else if (type == cmp_model_type)    {
        obj->model_cmp = hdl;
        obj->model_shadow_cmp = hdl;
    }
    else if (type == cmp_rbody_type)
        obj->rbody_cmp = hdl;
    else if (type == cmp_trigger_type)
        obj->trigger_cmp = hdl;
    else if (type == cmp_attachdock_type)
        obj->attachdock_cmp = hdl;
    else if (type == cmp_attachment_type)
        obj->attach_cmp = hdl;
    else if (type == cmp_animchar_type)
        obj->animchar_cmp = hdl;
}
