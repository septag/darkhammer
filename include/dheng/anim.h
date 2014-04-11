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

#ifndef __ANIM_H__
#define __ANIM_H__

#include "dhcore/types.h"
#include "cmp-types.h"
#include "engine-api.h"

/* typedefs */
struct anim_ctrl_data;
struct anim_ctrl_instance_data;
struct anim_reel_data;
struct gfx_model_posegpu;

typedef struct anim_ctrl_data* anim_ctrl;
typedef struct anim_ctrl_instance_data* anim_ctrl_inst;
typedef struct anim_reel_data* anim_reel;

/* types */
enum anim_ctrl_paramtype
{
    ANIM_CTRL_PARAM_UNKNOWN = 0,
    ANIM_CTRL_PARAM_INT = 1,
    ANIM_CTRL_PARAM_FLOAT = 2,
    ANIM_CTRL_PARAM_BOOLEAN = 3
};

struct anim_reel_desc
{
    uint fps;
    uint frame_cnt;
    float duration;
    float ft;
    uint pose_cnt;
    uint clip_cnt;
};

struct anim_clip_desc
{
    const char* name;
    int looped;
    float duration;
};


/* animation controller API */
anim_ctrl anim_ctrl_load(struct allocator* alloc, const char* janim_filepath,
                         uint thread_id);
void anim_ctrl_unload(anim_ctrl ctrl);
void anim_ctrl_update(const anim_ctrl ctrl, anim_ctrl_inst inst, float tm,
                      struct allocator* tmp_alloc);
void anim_ctrl_debug(const anim_ctrl ctrl, anim_ctrl_inst inst);
anim_ctrl_inst anim_ctrl_createinstance(struct allocator* alloc, const anim_ctrl ctrl);
void anim_ctrl_destroyinstance(anim_ctrl_inst inst);

ENGINE_API enum anim_ctrl_paramtype anim_ctrl_get_paramtype(anim_ctrl ctrl, anim_ctrl_inst inst,
  const char* name);
ENGINE_API float anim_ctrl_get_paramf(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name);
ENGINE_API void anim_ctrl_set_paramf(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name,
                                     float value);
ENGINE_API int anim_ctrl_get_paramb(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name);
ENGINE_API void anim_ctrl_set_paramb(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name,
                                     int value);
ENGINE_API int anim_ctrl_get_parami(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name);
ENGINE_API void anim_ctrl_set_parami(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name,
  int value);

void anim_ctrl_fetchresult_hierarchal(const anim_ctrl_inst inst, const uint* bindmap,
                                      const cmphandle_t* xforms,
                                      const uint* root_idxs, uint root_idx_cnt,
                                      const struct mat3f* root_mat);
void anim_ctrl_fetchresult_skeletal(const anim_ctrl_inst inst, const uint* bindmap,
                                    struct mat3f* joints, const uint* root_idxs,
                                    uint root_idx_cnt, const struct mat3f* root_mat);
reshandle_t anim_ctrl_get_reel(const anim_ctrl_inst inst);
result_t anim_ctrl_set_reel(anim_ctrl_inst inst, reshandle_t reel_hdl);

/* animation reel API */
anim_reel anim_load(struct allocator* alloc, const char* h3da_filepath, uint thread_id);
void anim_unload(anim_reel reel);

void anim_update_clip_hierarchal(const anim_reel reel, uint clip_idx, float t,
    const uint* bindmap, const cmphandle_t* xforms, uint frame_force_idx,
    const uint* root_idxs, uint root_idx_cnt, const struct mat3f* root_mat);
void anim_update_clip_skeletal(const anim_reel reel, uint clip_idx, float t,
    const uint* bindmap, struct mat3f* joints, uint frame_force_idx,
    const uint* root_idxs, uint root_idx_cnt, const struct mat3f* root_mat);

uint anim_findclip(const anim_reel reel, const char* name);
void anim_get_clipdesc(struct anim_clip_desc* desc, const anim_reel reel, uint clip_idx);
void anim_get_desc(struct anim_reel_desc* desc, const anim_reel reel);
const char* anim_get_posebinding(const anim_reel reel, uint pose_idx);

/* debugging */
int anim_ctrl_get_curstate(anim_ctrl ctrl, anim_ctrl_inst inst, const char* layer_name, 
  OUT char* state, OUT OPTIONAL float* progress);
int anim_ctrl_get_curtransition(anim_ctrl ctrl, anim_ctrl_inst inst, const char* layer_name, 
  OUT char* state_a, OUT char* state_b, OUT OPTIONAL float* progress);

#endif /* __ANIM_H__ */
