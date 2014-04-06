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

#ifndef __CMPANIMCHAR_H__
#define __CMPANIMCHAR_H__

#include "dhcore/types.h"
#include "cmp-types.h"
#include "anim.h"

/* */
struct cmp_animchar
{
    /* interface */
    char filepath[128];

    /* internal */
    reshandle_t ctrl_hdl;   /* animation controller resource */
    anim_ctrl_inst inst;    /* animation controller instance */

    uint* bindmap;    /* maps animation poses (count = pose_cnt) to target resource */

    struct mat3f root_mat;  /* root matrix which must be applied to root node after anim update */
    uint root_cnt;
    uint* root_idxs;    /* index of the root nodes (in xform_hdls or pose) */

    struct gfx_model_posegpu* pose; /* pointer to gpupose of the binded skeletal model */
    const cmphandle_t* xform_hdls; /* xform component handles for hierarchal animation */
    uint xform_cnt;   /* xform_hdls count */

    uint filepathhash;        /* filehash to keep track of possible reloading */
    struct allocator* alloc;    /* we have dynamic allocation within this component */
};

/*************************************************************************************************/
ENGINE_API result_t cmp_animchar_modify(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t hdl);

/* descriptors */
static const struct cmp_value cmp_animchar_values[] = {
    {"filepath", CMP_VALUE_STRING, offsetof(struct cmp_animchar, filepath), 128, 1,
        cmp_animchar_modify, "customdlg; filepicker; filter=*.json;"}
};
static const uint16 cmp_animchar_type = 0x99e4;

/* */
result_t cmp_animchar_register(struct allocator* alloc);

void cmp_animchar_reload(const char* filepath, reshandle_t hdl, bool_t manual);
void cmp_animchar_reelchanged(reshandle_t reel_hdl);
result_t cmp_animchar_bind(struct cmp_obj* obj, void* data, struct allocator* alloc,
    struct allocator* tmp_alloc, reshandle_t hdl);
result_t cmp_animchar_bind_noalloc(struct cmp_obj* obj, cmphandle_t hdl);

void cmp_animchar_unbind(cmphandle_t hdl);

/* parameter manipulation */
ENGINE_API void cmp_animchar_setparamb(cmphandle_t hdl, const char* name, bool_t value);
ENGINE_API void cmp_animchar_setparami(cmphandle_t hdl, const char* name, int value);
ENGINE_API void cmp_animchar_setparamf(cmphandle_t hdl, const char* name, float value);
ENGINE_API float cmp_animchar_getparamf(cmphandle_t hdl, const char* name);
ENGINE_API int cmp_animchar_getparami(cmphandle_t hdl, const char* name);
ENGINE_API bool_t cmp_animchar_getparamb(cmphandle_t hdl, const char* name);
ENGINE_API enum anim_ctrl_paramtype cmp_animchar_getparamtype(cmphandle_t hdl, const char* name);

/* debugging */
ENGINE_API bool_t cmp_animchar_get_curstate(cmphandle_t hdl, const char* layer_name, 
    OUT char* state, OPTIONAL OUT float* progress);
ENGINE_API bool_t cmp_animchar_get_curtransition(cmphandle_t hdl, const char* layer_name, 
    OUT char* state_a, OUT char* state_b, OPTIONAL OUT float* progress);


#endif /* __CMPANIMCHAR_H__ */