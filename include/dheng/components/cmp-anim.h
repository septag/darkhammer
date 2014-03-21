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

#ifndef __CMPANIM_H__
#define __CMPANIM_H__

#include "dhcore/types.h"
#include "../cmp-types.h"

/* fwd */
struct anim_set;

/* */
struct cmp_anim
{
    /* interface */
    char filepath[128];
    float play_rate;
    char clip_name[32];
    uint frame_idx;

    /* internal */
    reshandle_t clip_hdl;
    float t; /* elapsed time */
    uint clip_id;
    bool_t playing; /* keep play state */
    struct gfx_model_posegpu* pose; /* pointer to gpupose of the binded skeletal model */
    const cmphandle_t* xform_hdls; /* pointer to xform components, used for hierarchal animation */
    struct mat3f root_mat;  /* root matrix which must be applied to root node after anim update */
    uint xform_cnt;   /* count of mats or xform_hdls */
    uint root_cnt;
    uint* bindmap;  /* maps animation poses to joints in the target resource (count: reel->pose_cnt) */
    uint* root_idxs;    /* index of the root nodes (in xform_hdls or pose) */
    uint filepathhash;
    struct allocator* alloc;    /* we have dynamic allocation in this component */
};

ENGINE_API result_t cmp_anim_modify(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_anim_modify_clip(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_anim_modify_frame(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_anim_values[] = {
    {"filepath", CMP_VALUE_STRING, offsetof(struct cmp_anim, filepath), 128, 1, cmp_anim_modify,
        "customdlg; filepicker; filter=*.h3da;"},
    {"play_rate", CMP_VALUE_FLOAT, offsetof(struct cmp_anim, play_rate), sizeof(float), 1, NULL, ""},
    {"clip_name", CMP_VALUE_STRING, offsetof(struct cmp_anim, clip_name), 32, 1,
        cmp_anim_modify_clip, ""},
    {"frame_idx", CMP_VALUE_UINT, offsetof(struct cmp_anim, frame_idx), sizeof(uint), 1,
        cmp_anim_modify_frame, ""}
};
static const uint16 cmp_anim_type = 0x068b;

/* */
result_t cmp_anim_register(struct allocator* alloc);
void cmp_anim_reload(const char* filepath, reshandle_t hdl, bool_t manual);
result_t cmp_anim_bind(struct cmp_obj* obj, void* data, struct allocator* alloc,
    struct allocator* tmp_alloc, reshandle_t hdl);
void cmp_anim_unbind(cmphandle_t hdl);
result_t cmp_anim_bind_noalloc(struct cmp_obj* obj, cmphandle_t hdl);

ENGINE_API uint cmp_anim_getframecnt(cmphandle_t hdl);
ENGINE_API uint cmp_anim_getfps(cmphandle_t hdl);
ENGINE_API void cmp_anim_play(cmphandle_t hdl);
ENGINE_API void cmp_anim_stop(cmphandle_t hdl);
ENGINE_API bool_t cmp_anim_isplaying(cmphandle_t hdl);
ENGINE_API uint cmp_anim_getcurframe(cmphandle_t hdl);
ENGINE_API uint cmp_anim_getclipcnt(cmphandle_t hdl);
ENGINE_API const char* cmp_anim_getclipname(cmphandle_t hdl, uint clip_idx);
ENGINE_API uint cmp_anim_getbonecnt(cmphandle_t hdl);
ENGINE_API const char* cmp_anim_getbonename(cmphandle_t hdl, uint bone_idx);

#endif /* __CMPANIM_H__ */
