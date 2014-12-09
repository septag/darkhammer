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

/*!
 * \brief Animation Parameter: This is actually a Three type variant (Int,Bool,Float)
 * That can be fetched and set by user for animControllerInst to control animation flow and logic
 */
class animParam
{
public:
    enum class Type : int
    {
        UNKNOWN = 0,
        INT,
        FLOAT,
        BOOL
    };

private:
    Type _type = Type::UNKNOWN;
    union   {
        int i = 0;
        float f;
        bool b;
    } _values;

public:
    animParam() = default;
    animParam(int i)
    {
        _values.i = i;
        _type = Type::INT;
    }

    animParam(float f)
    {
        _values.f = f;
        _type = Type::FLOAT;
    }

    animParam(bool b)
    {
        _values.b = b;
        _type = Type::BOOL;
    }

    Type type() const   {   return _type;   }
    operator int() const    {   return _values.i;   }
    operator float() const  {   return _values.f;   }
    operator bool() const   {   return _values.b;   }

    animParam& operator=(int i)
    {
        _values.i = i;
        _type = Type::INT;
        return *this;
    }

    animParam& operator=(bool b)
    {
        _values.b = b;
        _type = Type::BOOL;
        return *this;
    }

    animParam& operator=(float f)
    {
        _values.f = f;
        _type = Type::FLOAT;
        return *this;
    }
};

/*!
 * \brief animCharController: Used as a resource to load reference character controller data
 */
class animCharController
{
public:
    virtual void destroy() = 0;
};

/*!
 * \brief animCharControllerInst: Main controller class that can be used by engine user
 * To control Character animation
 */
class animCharControllerInst
{
public:
    virtual void debug() = 0;
    virtual void destroy() = 0;

    // Params
    virtual animParam param(const char *name) const = 0;
    virtual void set_param(const char *name, const animParam param) = 0;

    // Reel Accessor
    virtual reshandle_t reel() const = 0;
    virtual result_t set_reel(reshandle_t reel_hdl) = 0;

    // Update
    virtual void update(float tm, Allocator *tmp_alloc = mem_heap()) = 0;

    // Debug
    virtual bool debug_state(const char *layer, OUT char *state, size_t state_sz,
                             OUT float *progress = nullptr) = 0;
    virtual bool debug_transition(const char *layer,
                                  OUT char *state_A, size_t state_A_sz,
                                  OUT char *state_B, size_t state_B_sz,
                                  OUT float *progress = nullptr) = 0;
};

/*!
 * \brief animReel: Used as resource inside engine. Contains a collection of animation clips
 * Data is reference binding points for animation keys. And animation keyframes for each joint
 */
class animReel
{
public:
    struct ClipInfo
    {
        const char *name;
        bool looped;
        float duration;
    };

    struct ReelInfo
    {
        int fps;
        int frame_cnt;
        float duration;
        float frame_time;
        int pose_cnt;
        int clip_cnt;
    };

public:
    // Get Data
    virtual int find_clip(const char *name) const = 0;
    virtual ClipInfo clip_info(int index) const = 0;
    virtual ReelInfo info() const = 0;
    virtual const char* pose_binding(int pose_index) const = 0;

    // Destroy
    virtual void destroy() = 0;
};

// Animation Controller Loads
animCharController* anim_charctrl_loadj(const char *json_animctrl,
                                        Allocator *alloc = mem_heap(),
                                        uint thread_id = 0);

animCharControllerInst* anim_charctrl_create_instance(const animCharController *ctrl,
                                                      Allocator *alloc = mem_heap());

// Animation Reel Loads
animReel* anim_reel_loadf(const char *h3da_filepath, Allocator *alloc = mem_heap(),
                          uint thread_id = 0);


#if 0
void anim_ctrl_fetchresult_hierarchal(const anim_ctrl_inst inst, const uint* bindmap,
                                      const cmphandle_t* xforms,
                                      const uint* root_idxs, uint root_idx_cnt,
                                      const struct mat3f* root_mat);
void anim_ctrl_fetchresult_skeletal(const anim_ctrl_inst inst, const uint* bindmap,
                                    struct mat3f* joints, const uint* root_idxs,
                                    uint root_idx_cnt, const struct mat3f* root_mat);

void anim_update_clip_hierarchal(const anim_reel reel, uint clip_idx, float t,
    const uint* bindmap, const cmphandle_t* xforms, uint frame_force_idx,
    const uint* root_idxs, uint root_idx_cnt, const struct mat3f* root_mat);
void anim_update_clip_skeletal(const anim_reel reel, uint clip_idx, float t,
    const uint* bindmap, struct mat3f* joints, uint frame_force_idx,
    const uint* root_idxs, uint root_idx_cnt, const struct mat3f* root_mat);
#endif

#endif /* __ANIM_H__ */
