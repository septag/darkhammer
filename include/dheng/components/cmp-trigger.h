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

#ifndef __CMPTRIGGER_H__
#define __CMPTRIGGER_H__

#include "dhcore/vec-math.h"
#include "../cmp-types.h"
#include "../phx-types.h"

/* types */
typedef void (*pfn_cmp_trigger_callback)(struct cmp_obj* trigger_obj, struct cmp_obj* other_obj,
    enum phx_trigger_state state, void* param);

/* */
struct cmp_trigger
{
    /* interface */
    struct vec3f box;
    struct vec3f local_pos;
    struct quat4f local_rot;
    bool_t is_static;
    uint collision_filter;

    /* internal */
    phx_obj rbody;
    uint px_sceneid;  /* owner scene */
    bool_t triggered;
    pfn_cmp_trigger_callback trigger_fn;
    void* param;
};

ENGINE_API result_t cmp_trigger_modifybox(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_trigger_modifylocalpos(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_trigger_modifylocalrot(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_trigger_modifystatic(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_trigger_modifycolfilter(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_trigger_values[] = {
    {"box", CMP_VALUE_FLOAT3, offsetof(struct cmp_trigger, box), sizeof(struct vec3f), 1,
    cmp_trigger_modifybox, ""},
    {"local_pos", CMP_VALUE_FLOAT3, offsetof(struct cmp_trigger, local_pos), sizeof(struct vec3f),
    1, cmp_trigger_modifylocalpos, ""},
    {"local_rot", CMP_VALUE_FLOAT4, offsetof(struct cmp_trigger, local_rot), sizeof(struct quat4f),
    1, cmp_trigger_modifylocalrot, ""},
    {"static", CMP_VALUE_BOOL, offsetof(struct cmp_trigger, is_static), sizeof(bool_t), 1,
    cmp_trigger_modifystatic, ""},
    {"collision_filter", CMP_VALUE_UINT, offsetof(struct cmp_trigger, collision_filter),
    sizeof(uint), 1, cmp_trigger_modifycolfilter, ""}
};
static const uint16 cmp_trigger_type = 0x6dcb;

/* */
result_t cmp_trigger_register(struct allocator* alloc);

/* proxy for trigger events */
ENGINE_API void cmp_trigger_register_callback(cmphandle_t trigger_cmp,
                                              pfn_cmp_trigger_callback trigger_fn, void* param);
ENGINE_API void cmp_trigger_unregister_callback(cmphandle_t trigger_cmp);

#endif /* __CMPTRIGGER_H__ */
