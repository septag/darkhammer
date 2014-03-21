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

#ifndef __CMPRBODY_H__
#define __CMPRBODY_H__

#include "dhcore/types.h"
#include "../cmp-types.h"
#include "../phx-types.h"

struct cmp_rbody
{
    /* interface */
    char filepath[128];
    bool_t kinamtic;
    bool_t disable_gravity;
    uint collision_filter;

    /* internal */
    phx_obj rbody;
    reshandle_t prefab_hdl;
    uint filepath_hash;
    uint px_sceneid;  /* owner scene */
};

ENGINE_API result_t cmp_rbody_modifyfile(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_rbody_modifykinematic(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_rbody_modifydgravity(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_rbody_modifycolfilter(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_rbody_values[] = {
    {"filepath", CMP_VALUE_STRING, offsetof(struct cmp_rbody, filepath), 128, 1,
    cmp_rbody_modifyfile, "customdlg; filepicker; filter=*.h3dp;"},
    {"kinematic", CMP_VALUE_BOOL, offsetof(struct cmp_rbody, kinamtic), sizeof(bool_t), 1,
    cmp_rbody_modifykinematic, ""},
    {"disable_gravity", CMP_VALUE_BOOL, offsetof(struct cmp_rbody, disable_gravity), sizeof(bool_t),
    1, cmp_rbody_modifydgravity, ""},
    {"collision_filter", CMP_VALUE_UINT, offsetof(struct cmp_rbody, collision_filter),
    sizeof(bool_t), 1, cmp_rbody_modifycolfilter, ""}
};
static const uint16 cmp_rbody_type = 0xbc2d;

result_t cmp_rbody_register(struct allocator* alloc);
void cmp_rbody_reload(const char* filepath, reshandle_t hdl, bool_t manual);

#endif /* __CMPRBODY_H__ */
