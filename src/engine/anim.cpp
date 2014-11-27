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

#include "dheng/anim.h"

#include "dhcore/core.h"
#include "dhcore/file-io.h"
#include "dhcore/json.h"
#include "dhcore/vec-math.h"
#include "dhcore/hash-table.h"
#include "dhcore/stack-alloc.h"
#include "dhcore/task-mgr.h"

#include "share/h3d-types.h"
#include "share/mem-ids.h"

#include "dheng/res-mgr.h"
#include "dheng/cmp-mgr.h"
#include "dheng/gfx-model.h"
#include "dheng/gfx-canvas.h"

#include "dheng/components/cmp-xform.h"

using namespace dh;

// Layer blending callback type
typedef const Mat3* (*pfn_anim_layerblend)(Mat3* result, Mat3* src, Mat3* dest, float mask);

// animReel
class Reel : public animReel
{
public:
    enum class Flags : uint
    {
        LOOP = (1<<0),
        SCALE = (1<<1)
    };

    struct Clip
    {
        char name[32];
        int frame_start; /* must be < subclip->frame_end */
        int frame_end;   /* must be <= reel->frame_cnt */
        int looped;
        float duration;
    };

    struct Pose
    {
        Vec4 pos_scale;     // w = Uniform scale
        Quat rot;
    };

    struct Channel
    {
        Pose *poses;    // Count = pose_cnt
    };

    char _name[32];
    int _fps = 0;
    int _frame_cnt = 0;
    float _duration = 0.0f;
    float _frame_time = 0.0f;
    uint _flags = 0;   // Combination of Flags
    int _pose_cnt = 0;
    int _clip_cnt = 0;
    char *_binds = nullptr;   //Maps each joint/node to binded hierarchy nodes (Size=char(32)*pose_cnt)
    Channel *_channels = nullptr; // Count = frame_cnt
    Clip *_clips = nullptr;
    HashtableFixed<int, -1> _clips_tbl; // Key points to _clips array
    Allocator *_alloc = nullptr;

public:
    Reel() = default;

public:
    // Interited from animReel
    int find_clip(const char *name) const
    {
        return _clips_tbl.value(name);
    }

    ClipInfo clip_info(int index) const
    {
        ASSERT(index < _clip_cnt);
        const Reel::Clip &clip = _clips[index];
        ClipInfo info;
        info.name = clip.name;
        info.duration = clip.duration;
        info.looped = clip.looped;
        return info;
    }

    ReelInfo info() const
    {
        ReelInfo info;
        info.fps = _fps;
        info.clip_cnt = _clip_cnt;
        info.duration = _duration;
        info.frame_cnt = _frame_cnt;
        info.frame_time = _frame_time;
        info.pose_cnt = _pose_cnt;
        return info;
    }

    const char* pose_binding(int pose_index) const
    {
        ASSERT(pose_index < _pose_cnt);
        return _binds + pose_index*32;
    }

    void destroy()
    {
        mem_delete_alloc_aligned<Reel>(_alloc, this);
    }

public:
    static animReel* loadf(const char *h3da_filepath, Allocator *alloc, uint thread_id)
    {
        static const char *err_fmt = "Loading animation reel '%s' failed: %s";

        // Fetch temp allocator
        Allocator *tmp_alloc = tsk_get_tmpalloc(thread_id);
        A_PUSH(tmp_alloc);

        // Load file in memory
        File f = File::open_mem(h3da_filepath, tmp_alloc, MID_ANIM);
        if (!f.is_open())  {
            A_POP(tmp_alloc);
            err_printf(__FILE__, __LINE__, err_fmt, h3da_filepath, "Could not open file");
            return nullptr;
        }

        // Read header
        h3dHeader header;
        f.read(&header, sizeof(header), 1);
        if (header.sign != H3D_SIGN || header.type != h3dType::ANIM_REEL) {
            err_printf(__FILE__, __LINE__, err_fmt, h3da_filepath, "Invalid file format");
            A_POP(tmp_alloc);
            return nullptr;
        }
        if (header.version != H3D_VERSION_11)   {
            err_printf(__FILE__, __LINE__, err_fmt, h3da_filepath, "Invalid file version");
            A_POP(tmp_alloc);
            return nullptr;
        }

        // Animation Reel descriptor
        h3dAnim h3danim;
        f.seek(header.data_offset);
        f.read(&h3danim, sizeof(h3danim), 1);

        // Create stack allocator for faster allocations
        StackAlloc stack_mem;
        Allocator stack_alloc;
        size_t total_sz =
            sizeof(Reel) +
            32*h3danim.channel_cnt +
            sizeof(Reel::Channel)*h3danim.frame_cnt +
            sizeof(Reel::Clip)*h3danim.clip_cnt +
            sizeof(Reel::Pose)*h3danim.channel_cnt*h3danim.frame_cnt + 16 +
            HashtableFixed<int, -1>::estimate_size(h3danim.clip_cnt);

        if (IS_FAIL(stack_mem.create(total_sz, alloc, MID_GFX))) {
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            A_POP(tmp_alloc);
            return nullptr;
        }
        stack_mem.bindto(&stack_alloc);

        // Start allocating and loading data
        Reel *reel = mem_new_alloc<Reel>(&stack_alloc);

        char filename[64];
        str_safecpy(reel->_name, sizeof(reel->_name), path_getfilename(filename, h3da_filepath));
        reel->_fps = h3danim.fps;
        reel->_frame_cnt = h3danim.frame_cnt;
        reel->_frame_time = 1.0f / ((float)h3danim.fps);
        reel->_duration = reel->_frame_time * (float)h3danim.frame_cnt;
        reel->_pose_cnt = h3danim.channel_cnt;
        reel->_alloc = alloc;
        reel->_clip_cnt = h3danim.clip_cnt;
        if (h3danim.has_scale)
            BIT_ADD(reel->_flags, (uint)Reel::Flags::SCALE);

        // Channel data
        // Bind names is an array buffer with each item being 32-byte(char) wide
        reel->_binds = (char*)A_ALLOC(&stack_alloc, 32*h3danim.channel_cnt, 0);
        ASSERT(reel->_binds);
        memset(reel->_binds, 0x00, 32*h3danim.channel_cnt);

        int frame_cnt = h3danim.frame_cnt;

        // Channels are set of poses for each frame.
        // We allocate frame_cnt number of channels
        reel->_channels = (Reel::Channel*)A_ALLOC(&stack_alloc,
                                                       sizeof(Reel::Channel)*frame_cnt, 0);
        ASSERT(reel->_channels);
        memset(reel->_channels, 0x00, sizeof(Reel::Channel)*frame_cnt);

        Vec4 *pos_scale = (Vec4*)A_ALLOC(tmp_alloc, sizeof(Vec4)*frame_cnt, 0);
        Quat *rot = (Quat*)A_ALLOC(tmp_alloc, sizeof(Quat)*frame_cnt, 0);
        if (!pos_scale || !rot)   {
            reel->destroy();
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            A_POP(tmp_alloc);
            return nullptr;
        }

        // Create a big buffer for all poses in all frames and assign them to each channel
        uint8 *pos_buff = (uint8*)A_ALIGNED_ALLOC(&stack_alloc,
                                          sizeof(Reel::Pose)*h3danim.channel_cnt*frame_cnt, 0);
        ASSERT(pos_buff);

        for (int i = 0; i < frame_cnt; i++) {
            reel->_channels[i].poses =
                    (Reel::Pose*)(pos_buff + i*reel->_pose_cnt*sizeof(Reel::Pose));
        }

        for (int i = 0; i < h3danim.channel_cnt; i++)   {
            h3dAnimChannel h3dchannel;
            f.read(&h3dchannel, sizeof(h3dchannel), 1);

            strcpy(reel->_binds + i*32, h3dchannel.bindto);
            f.read(pos_scale, sizeof(Vec4), frame_cnt);
            f.read(rot, sizeof(Quat), frame_cnt);

            for (int k = 0; k < frame_cnt; k++)  {
                reel->_channels[k].poses[i].pos_scale = pos_scale[k];
                reel->_channels[k].poses[i].rot = rot[k];
            }
        }

        A_FREE(tmp_alloc, pos_scale);
        A_FREE(tmp_alloc, rot);

        // Clips
        ASSERT(h3danim.clip_cnt);
        reel->_clips = (Reel::Clip*)A_ALLOC(&stack_alloc,
                                                 sizeof(Reel::Clip)*reel->_clip_cnt, 0);
        ASSERT(reel->_clips);
        reel->_clips_tbl.create(reel->_clip_cnt, &stack_alloc);

        f.seek(h3danim.clips_offset);
        for (int i = 0; i < h3danim.clip_cnt; i++)   {
            h3dAnimClip h3dclip;
            Reel::Clip *subclip = &reel->_clips[i];
            f.read(&h3dclip, sizeof(h3dclip), 1);
            strcpy(subclip->name, h3dclip.name);
            subclip->frame_start = h3dclip.start;
            subclip->frame_end = h3dclip.end;
            subclip->looped = (bool)h3dclip.looped;
            subclip->duration = reel->_frame_time * (float)(h3dclip.end - h3dclip.start);

            reel->_clips_tbl.add(h3dclip.name, i);
        }

        A_POP(tmp_alloc);
        return (animReel*)reel;
    }

    int find_pose_binding(const char *name)
    {
        for (int i = 0, cnt = _pose_cnt; i < cnt; i++)  {
            const char *bindname = _binds + i*32;
            if (str_isequal(bindname, name))
                return i;
        }
        return -1;
    }

    int find_clip_hashed(uint name_hash) const
    {
        return _clips_tbl.value(name_hash);
    }
};

// CharController
class CharController : public animCharController
{
public:
    enum class LayerType : int
    {
        OVERRIDE = 0,
        ADDITIVE
    };

    struct Layer
    {
        char name[32];
        LayerType type;
        int state_cnt;
        int *states;
        int default_state;
        int bone_mask_cnt;
        char *bone_mask;    // Array of strings (joint names). Series of char[32]
    };

    struct Param
    {
        char name[32];
        animParam value;
    };

    enum class SequenceType : int
    {
        UNKNOWN = 0,
        CLIP,
        BLEND_TREE
    };

    struct Sequence
    {
        SequenceType type;
        int index;  // Index to clip or blend_tree
    };

    struct BlendTree
    {
        char name[32];
        int param;
        int child_cnt;
        float child_cnt_f;
        Sequence  *childs;
    };

    struct State
    {
        char name[32];
        float speed;
        int transition_cnt;
        int *transitions;
        Sequence seq;
    };

    enum class Predicate : int
    {
        UNKNOWN = 0,
        EQUAL,
        NOT,
        GREATER,
        LESS
    };

    enum class ConditionType : int
    {
        EXIT,   // Transition acts when reaches a certain Exit time
        PARAM   // Transition acts when custom paremeter meets it's condition
    };

    struct Condition
    {
        ConditionType type;
        int param;  // Index to parameter
        Predicate predicate;
        animParam value;    // Rerefence value to compare to parameter/exit time
    };

    struct ConditionGroup
    {
        int count;
        Condition *conditions;
    };

    struct Transition
    {
        float duration;
        int owner_state;    // Index to owner state
        int target_state;   // Index to target state
        int group_cnt;
        TransitionGroup *groups;
    };

    struct Clip
    {
        char name[32];
        uint name_hash;
    };

public:
    Allocator *_alloc = nullptr;
    char _reel_filepath[128];

    int _transition_cnt = 0;
    int _clip_cnt = 0;
    int _blendtree_cnt = 0;
    int _state_cnt = 0;
    int _param_cnt = 0;
    int _layer_cnt = 0;

    Transition *_transitions;
    Clip *_clips;
    BlendTree *_blendtrees;
    State *_states;
    Param *_params;
    Layer *_layers;

    HashtableFixed<int, -1> _params_tbl;    // ParamName->ParamIndex

public:
    // Inherited from animCharController
    void destroy();

public:
    static CharController* load_json(const char *json_filepath, Allocator *alloc, uint thread_id);
};

/*************************************************************************************************/
/* instance for each anim-controller */
struct anim_ctrl_param_inst
{
    enum anim_ctrl_paramtype type;
    union   {
        float f;
        int i;
        int b;
    } value;
};

struct anim_ctrl_layer_inst
{
    uint state_idx;   /* active state (=INVALID_INDEX if we are on state) */
    uint transition_idx;  /* active transition (=INVALID_INDEX if we are not on transition) */
    pfn_anim_layerblend blend_fn;

    uint8* buff; /* buffer for below allocations */
    struct anim_pose* poses;    /* temp storing final blended pose (cnt = pose_cnt of reel) */
    float* bone_mask;    /* bone-mask, multipliers for poses (cnt = pose_cnt of reel) */
};

struct anim_ctrl_clip_inst
{
    float start_tm;  /* global start time */
    float tm;    /* local time */
    float progress;  /* normalized progress (*N if looped) */
    float duration;
    int looped;
    uint rclip_idx;
};

struct anim_ctrl_blendtree_inst
{
    uint seq_a;   /* first sequence being played (=INVALID_INDEX if none) */
    uint seq_b;   /* second sequence being played (=INVALID_INDEX if none) */
    float blend;
    float progress;
};

struct anim_ctrl_transition_inst
{
    float start_tm; /* global start time */
    float blend; /* blend position [0~1] */
};

/* instance data holds the whole state of each anim-controller */
struct anim_ctrl_instance_data
{
    struct allocator* alloc;
    anim_ctrl owner;
    reshandle_t reel_hdl;

    float tm;    /* global time */
    float playrate;  /* playback rate (default=1) */

    uint layer_cnt;
    struct anim_ctrl_param_inst* params;
    struct anim_ctrl_layer_inst* layers;
    struct anim_ctrl_clip_inst* clips;
    struct anim_ctrl_blendtree_inst* blendtrees;
    struct anim_ctrl_transition_inst* transitions;
};

/*************************************************************************************************
 * fwd declarations
 */

/* animation controller - loading */
static void anim_ctrl_load_params(anim_ctrl ctrl, json_t jparams, struct allocator* alloc);
static void anim_ctrl_load_clips(anim_ctrl ctrl, json_t jclips, struct allocator* alloc);
static void anim_ctrl_load_states(anim_ctrl ctrl, json_t jstates, struct allocator* alloc);
static void anim_ctrl_load_layers(anim_ctrl ctrl, json_t jlayers, struct allocator* alloc);
static void anim_ctrl_load_blendtrees(anim_ctrl ctrl, json_t jblendtrees, struct allocator* alloc);
static void anim_ctrl_load_transitions(anim_ctrl ctrl, json_t jtransitions, struct allocator* alloc);
static void anim_ctrl_parse_group(struct allocator* alloc, struct anim_ctrl_transition_group* grp,
                           json_t jgrp);
static uint anim_ctrl_getcount(json_t jparent, const char* name);
static uint anim_ctrl_getcount_2nd(json_t jparent, const char* name0, const char* name1);
static uint anim_ctrl_getcount_3rd(json_t jparent, const char* name0, const char* name1,
                              const char* name2);

/* animation controller */
static void anim_ctrl_startstate(const anim_ctrl ctrl, anim_ctrl_inst inst, uint state_idx,
                          float start_tm);
static int anim_ctrl_checkstate(const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
                            uint layer_idx, uint state_idx, float tm);
static void anim_ctrl_updatetransition(struct anim_pose* poses,
                                const anim_ctrl ctrl, anim_ctrl_inst inst,
                                const anim_reel reel, uint layer_idx, uint transition_idx,
                                float tm, struct allocator* tmp_alloc);
static void anim_ctrl_startstate(const anim_ctrl ctrl, anim_ctrl_inst inst, uint state_idx,
                          float start_tm);
static int anim_ctrl_checktgroup(const anim_ctrl ctrl, anim_ctrl_inst inst,
                             const anim_reel reel, uint state_idx, uint layer_idx,
                             const struct anim_ctrl_transition_group* tgroup, float tm);
static void anim_ctrl_updatestate(struct anim_pose* poses, const anim_ctrl ctrl, anim_ctrl_inst inst,
                           const anim_reel reel, uint layer_idx, uint state_idx, float tm,
                           struct allocator* tmp_alloc);
static void anim_ctrl_starttransition(const anim_ctrl ctrl, anim_ctrl_inst inst,
                               const anim_reel reel, uint layer_idx, uint transition_idx,
                               float tm);
static float anim_ctrl_progress_state(const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
                              uint state_idx);
static float anim_ctrl_updateseq(struct anim_pose* poses,
                         const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
                         const struct anim_ctrl_sequence* seq, float tm, float playrate,
                         struct allocator* tmp_alloc);
static void anim_ctrl_calcpose(struct anim_pose* poses, const anim_reel reel, uint clip_idx, float tm);
static void anim_ctrl_blendpose(struct anim_pose* poses, const struct anim_pose* poses_a,
                         const struct anim_pose* poses_b, uint pose_cnt, float blend);
static void anim_ctrl_startseq(const anim_ctrl ctrl, anim_ctrl_inst inst,
                        const struct anim_ctrl_sequence* seq, float start_tm);
static void anim_ctrl_startclip(const anim_ctrl ctrl, anim_ctrl_inst inst, uint clip_idx, float start_tm);
static void anim_ctrl_startblendtree(const anim_ctrl ctrl, anim_ctrl_inst inst, uint blendtree_idx,
                              float start_tm);
static int anim_ctrl_checktgroup(const anim_ctrl ctrl, anim_ctrl_inst inst,
                             const anim_reel reel, uint state_idx, uint layer_idx,
                             const struct anim_ctrl_transition_group* tgroup, float tm);
static float anim_ctrl_updateclip(struct anim_pose* poses, const anim_ctrl ctrl,
                          anim_ctrl_inst inst, const anim_reel reel, uint clip_idx, float tm,
                          float playrate);
static float anim_ctrl_updateblendtree(struct anim_pose* poses,
                               const anim_ctrl ctrl, anim_ctrl_inst inst,
                               const anim_reel reel, uint blendtree_idx, float tm,
                               float playrate, struct allocator* tmp_alloc);

/*************************************************************************************************
 * inlines
 */
INLINE char* anim_get_bindname(char* binds, uint idx)
{
    return binds + idx*32;
}

INLINE enum anim_ctrl_sequencetype anim_ctrl_parse_seqtype(json_t jseq)
{
    char seq_type_s[32];
    strcpy(seq_type_s, json_gets_child(jseq, "type", ""));
    if (str_isequal(seq_type_s, "clip"))
        return ANIM_CTRL_SEQUENCE_CLIP;
    else if (str_isequal(seq_type_s, "blendtree"))
        return ANIM_CTRL_SEQUENCE_BLENDTREE;
    else
        return ANIM_CTRL_SEQUENCE_UNKNOWN;
}

INLINE enum anim_ctrl_layertype anim_ctrl_parse_layertype(json_t jtype)
{
    const char* layer_type_s = json_gets_child(jtype, "layer", "");
    if (str_isequal(layer_type_s, "override"))
        return ANIM_CTRL_LAYER_OVERRIDE;
    else if (str_isequal(layer_type_s, "additive"))
        return ANIM_CTRL_LAYER_ADDITIVE;
    else
        return ANIM_CTRL_LAYER_OVERRIDE;
}

INLINE enum anim_ctrl_tgrouptype anim_ctrl_parse_grptype(json_t jgrp)
{
    char type_s[32];
    strcpy(type_s, json_gets_child(jgrp, "type", ""));

    if (str_isequal(type_s, "exit"))
        return ANIM_CTRL_TGROUP_EXIT;
    else if (str_isequal(type_s, "param"))
        return ANIM_CTRL_TGROUP_PARAM;
    else
        return ANIM_CTRL_TGROUP_EXIT;
}

INLINE enum anim_predicate anim_ctrl_parse_grppred(json_t jgrp)
{
    char pred_s[32];
    strcpy(pred_s, json_gets_child(jgrp, "predicate", ""));

    if (str_isequal(pred_s, "=="))
        return ANIM_PREDICATE_EQUAL;
    else if (str_isequal(pred_s, "!="))
        return ANIM_PREDICATE_NOT;
    else if (str_isequal(pred_s, ">"))
        return ANIM_PREDICATE_GREATER;
    else if (str_isequal(pred_s, "<"))
        return ANIM_PREDICATE_LESS;
    else
        return ANIM_PREDICATE_UNKNOWN;
}

INLINE int anim_ctrl_testpredicate_f(enum anim_predicate pred, float value1, float value2)
{
    switch (pred)   {
    case ANIM_PREDICATE_EQUAL:
        return math_isequal(value1, value2);
    case ANIM_PREDICATE_GREATER:
        return value1 > (value2 + EPSILON);
    case ANIM_PREDICATE_LESS:
        return value1 < (value2 - EPSILON);
    case ANIM_PREDICATE_NOT:
        return !math_isequal(value1, value2);
    default:
        return FALSE;
    }
}

INLINE int anim_ctrl_testpredicate_n(enum anim_predicate pred, int value1, int value2)
{
    switch (pred)   {
    case ANIM_PREDICATE_EQUAL:
        return value1 == value2;
    case ANIM_PREDICATE_GREATER:
        return value1 > value2;
    case ANIM_PREDICATE_LESS:
        return value1 < value2 - EPSILON;
    case ANIM_PREDICATE_NOT:
        return value1 != value2;
    default:
        return FALSE;
    }
}

INLINE int anim_ctrl_testpredicate_b(int value1, int value2)
{
    return value1 == value2;
}

/*************************************************************************************************/
void anim_update_clip_hierarchal(const anim_reel reel, uint clip_idx, float t,
    const uint* bindmap, const cmphandle_t* xforms, uint frame_force_idx,
    const uint* root_idxs, uint root_idx_cnt, const Mat3* root_mat)
{
    uint frame_cnt, frame_idx;
    const struct anim_clip* subclip = &reel->clips[clip_idx];
    float ft = reel->ft;

    if (frame_force_idx == INVALID_INDEX)   {
        frame_cnt = subclip->frame_end - subclip->frame_start;
        frame_idx = clampui((uint)(t/ft), 0, frame_cnt-1);
    }   else    {
        frame_cnt = reel->frame_cnt;
        frame_idx = frame_force_idx;
    }

    uint nextframe_idx = (frame_idx + 1) % frame_cnt;

    /* interpolate between two frames (samples) by time
     * normalize between 0~1 */
    float ivalue = (t - (frame_idx * ft)) / ft;

    const struct anim_channel* sampl = &reel->channels[frame_idx + subclip->frame_start];
    const struct anim_channel* next_sampl = &reel->channels[nextframe_idx + subclip->frame_start];

    Mat3 xfm;
    mat3_set_ident(&xfm);

    for (uint i = 0, pose_cnt = reel->pose_cnt; i < pose_cnt; i++)    {
        struct vec3f pos_lerp;
        struct quat4f rot_slerp;

        vec3_lerp(&pos_lerp, &sampl->poses[i].pos_scale, &next_sampl->poses[i].pos_scale, ivalue);
        quat_slerp(&rot_slerp, &sampl->poses[i].rot, &next_sampl->poses[i].rot, ivalue);
        mat3_set_trans_rot(&xfm, &pos_lerp, &rot_slerp);

        cmphandle_t xfh = xforms[bindmap[i]];
        struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(xfh);
        mat3_setm(&xf->mat, &xfm);
    }

    for (uint i = 0; i < root_idx_cnt; i++)   {
        cmphandle_t xfh = xforms[root_idxs[i]];
        struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(xfh);
        mat3_mul(&xf->mat, &xf->mat, root_mat);
    }
}

void anim_update_clip_skeletal(const anim_reel reel, uint clip_idx, float t,
    const uint* bindmap, Mat3* joints, uint frame_force_idx,
    const uint* root_idxs, uint root_idx_cnt, const Mat3* root_mat)
{
    uint frame_cnt, frame_idx;
    const struct anim_clip* subclip = &reel->clips[clip_idx];
    float ft = reel->ft;

    if (frame_force_idx == INVALID_INDEX)   {
        frame_cnt = subclip->frame_end - subclip->frame_start;
        frame_idx = clampui((uint)(t/ft), 0, frame_cnt-1);
    }   else    {
        frame_cnt = reel->frame_cnt;
        frame_idx = frame_force_idx;
    }

    uint nextframe_idx = (frame_idx + 1) % frame_cnt;

    /* interpolate between two frames (samples) by time
     * normalize between 0~1 */
    float ivalue = (t - (frame_idx * ft)) / ft;

    const struct anim_channel* sampl = &reel->channels[frame_idx + subclip->frame_start];
    const struct anim_channel* next_sampl = &reel->channels[nextframe_idx + subclip->frame_start];

    Mat3 xfm;
    mat3_set_ident(&xfm);

    for (uint i = 0, pose_cnt = reel->pose_cnt; i < pose_cnt; i++)    {
        struct vec3f pos_lerp;
        struct quat4f rot_slerp;

        vec3_lerp(&pos_lerp, &sampl->poses[i].pos_scale, &next_sampl->poses[i].pos_scale, ivalue);
        quat_slerp(&rot_slerp, &sampl->poses[i].rot, &next_sampl->poses[i].rot, ivalue);
        mat3_set_trans_rot(&xfm, &pos_lerp, &rot_slerp);
        mat3_setm(&joints[bindmap[i]], &xfm);
    }

    for (uint i = 0; i < root_idx_cnt; i++)   {
        mat3_mul(&joints[root_idxs[i]], &joints[root_idxs[i]], root_mat);
    }
}

/*************************************************************************************************/
uint anim_ctrl_getcount(json_t jparent, const char* name)
{
    json_t j = json_getitem(jparent, name);
    if (j != nullptr)
        return json_getarr_count(j);
    else
        return 0;
}

uint anim_ctrl_getcount_2nd(json_t jparent, const char* name0, const char* name1)
{
    json_t j = json_getitem(jparent, name0);
    if (j != nullptr)  {
        uint cnt = 0;
        uint l1_cnt = json_getarr_count(j);
        for (uint i = 0; i < l1_cnt; i++)    {
            json_t j2 = json_getitem(json_getarr_item(j, i), name1);
            cnt += (j2 != nullptr) ? json_getarr_count(j2) : 0;
        }
        return cnt;
    }   else    {
        return 0;
    }
}

uint anim_ctrl_getcount_3rd(json_t jparent, const char* name0, const char* name1,
                              const char* name2)
{
    json_t j = json_getitem(jparent, name0);
    if (j != nullptr)  {
        uint cnt = 0;
        uint l1_cnt = json_getarr_count(j);
        for (uint i = 0; i < l1_cnt; i++)    {
            json_t j2 = json_getitem(json_getarr_item(j, i), name1);

            if (j2 != nullptr) {
                uint l2_cnt = json_getarr_count(j2);
                for (uint k = 0; k < l2_cnt; k++) {
                    json_t j3 = json_getitem(json_getarr_item(j2, k), name2);
                    cnt += (j3 != nullptr) ? json_getarr_count(j3) : 0;
                }
            }
        }
        return cnt;
    }   else    {
        return 0;
    }
}

anim_ctrl anim_ctrl_load(struct allocator* alloc, const char* janim_filepath, uint thread_id)
{
    struct allocator* tmp_alloc = tsk_get_tmpalloc(thread_id);
    A_SAVE(tmp_alloc);

    /* load JSON ctrl file */
    file_t f = fio_openmem(tmp_alloc, janim_filepath, FALSE, MID_ANIM);
    if (f == nullptr) {
        err_printf(__FILE__, __LINE__, "Loading ctrl-anim failed: Could not open file '%s'",
            janim_filepath);
        A_POP(tmp_alloc);
        return nullptr;
    }

    json_t jroot = json_parsefilef(f, tmp_alloc);
    fio_close(f);
    if (jroot == nullptr)  {
        err_printf(__FILE__, __LINE__, "Loading ctrl-anim failed: Invalid json '%s'",
            janim_filepath);
        A_POP(tmp_alloc);
        return nullptr;
    }

    /* calculate total size and create memory stack */
    struct stack_alloc stack_mem;
    struct allocator stack_alloc;

    uint total_tgroups = anim_ctrl_getcount_2nd(jroot, "transitions", "groups");
    uint total_tgroupitems = anim_ctrl_getcount_3rd(jroot, "transitions", "groups", "conditions");
    uint total_seqs = anim_ctrl_getcount_2nd(jroot, "blendtrees", "childs");
    uint total_idxs = anim_ctrl_getcount_2nd(jroot, "states", "transitions") +
        anim_ctrl_getcount_2nd(jroot, "layers", "states");
    uint total_bonemasks = anim_ctrl_getcount_2nd(jroot, "layers", "bone-mask");
    uint param_cnt = anim_ctrl_getcount(jroot, "params");
    size_t total_sz =
        sizeof(struct anim_ctrl_data) +
        hashtable_fixed_estimate_size(param_cnt) +
        param_cnt*sizeof(struct anim_ctrl_param) +
        anim_ctrl_getcount(jroot, "clips")*sizeof(struct anim_ctrl_clip) +
        anim_ctrl_getcount(jroot, "transitions")*sizeof(struct anim_ctrl_transition) +
        anim_ctrl_getcount(jroot, "blendtrees")*sizeof(struct anim_ctrl_blendtree) +
        anim_ctrl_getcount(jroot, "layers")*sizeof(struct anim_ctrl_layer) +
        anim_ctrl_getcount(jroot, "states")*sizeof(struct anim_ctrl_state) +
        total_tgroups*sizeof(struct anim_ctrl_transition_group) +
        total_tgroupitems*sizeof(struct anim_ctrl_transition_groupitem) +
        total_seqs*sizeof(struct anim_ctrl_sequence) +
        total_idxs*sizeof(uint) +
        total_bonemasks*32;
    if (IS_FAIL(mem_stack_create(alloc, &stack_mem, total_sz, MID_GFX)))    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        A_POP(tmp_alloc);
        return nullptr;
    }
    mem_stack_bindalloc(&stack_mem, &stack_alloc);

    /* create ctrl structure and zero memory */
    anim_ctrl ctrl = (struct anim_ctrl_data*)A_ALLOC(&stack_alloc, sizeof(struct anim_ctrl_data),
        MID_ANIM);
    ASSERT(ctrl);
    memset(ctrl, 0x00, sizeof(struct anim_ctrl_data));
    ctrl->alloc = alloc;

    /* animation reel resource */
    const char* reel_filepath = json_gets_child(jroot, "reel", "");
    if (reel_filepath[0] == 0)  {
        err_printf(__FILE__, __LINE__, "Loading ctrl-anim failed: empty reel file");
        anim_ctrl_unload(ctrl);
        A_POP(tmp_alloc);
        return nullptr;
    }
    str_safecpy(ctrl->reel_filepath, sizeof(ctrl->reel_filepath), reel_filepath);

    /* */
    anim_ctrl_load_params(ctrl, json_getitem(jroot, "params"), &stack_alloc);
    anim_ctrl_load_clips(ctrl, json_getitem(jroot, "clips"), &stack_alloc);
    anim_ctrl_load_transitions(ctrl, json_getitem(jroot, "transitions"), &stack_alloc);
    anim_ctrl_load_blendtrees(ctrl, json_getitem(jroot, "blendtrees"), &stack_alloc);
    anim_ctrl_load_states(ctrl, json_getitem(jroot, "states"), &stack_alloc);
    anim_ctrl_load_layers(ctrl, json_getitem(jroot, "layers"), &stack_alloc);

    json_destroy(jroot);
    A_POP(tmp_alloc);

    return ctrl;
}

void anim_ctrl_load_params(anim_ctrl ctrl, json_t jparams, struct allocator* alloc)
{
    if (jparams == nullptr)
        return;

    uint cnt = json_getarr_count(jparams);
    if (cnt == 0)
        return;
    ctrl->params = (struct anim_ctrl_param*)A_ALLOC(alloc, sizeof(struct anim_ctrl_param)*cnt,
        MID_ANIM);
    ASSERT(ctrl->params);

    hashtable_fixed_create(alloc, &ctrl->param_tbl, cnt, MID_ANIM);

    for (uint i = 0; i < cnt; i++)    {
        json_t jparam = json_getarr_item(jparams, i);
        struct anim_ctrl_param* param = &ctrl->params[i];

        strcpy(param->name, json_gets_child(jparam, "name", ""));
        hashtable_fixed_add(&ctrl->param_tbl, hash_str(param->name), i);

        char type[32];
        strcpy(type, json_gets_child(jparam, "type", "float"));
        if (str_isequal_nocase(type, "float")) {
            param->type = ANIM_CTRL_PARAM_FLOAT;
            param->value.f = json_getf_child(jparam, "value", 0.0f);
        }   else if (str_isequal_nocase(type, "int"))  {
            param->type = ANIM_CTRL_PARAM_INT;
            param->value.i = json_geti_child(jparam, "value", 0);
        }   else if (str_isequal_nocase(type, "bool")) {
            param->type = ANIM_CTRL_PARAM_BOOLEAN;
            param->value.b = json_getb_child(jparam, "value", FALSE);
        }   else    {
            param->type = ANIM_CTRL_PARAM_FLOAT;
            param->value.f = 0.0f;
        }
    }

    ctrl->param_cnt = cnt;
}

void anim_ctrl_load_clips(anim_ctrl ctrl, json_t jclips, struct allocator* alloc)
{
    if (jclips == nullptr)
        return;

    uint cnt = json_getarr_count(jclips);
    if (cnt == 0)
        return;

    ctrl->clips = (struct anim_ctrl_clip*)A_ALLOC(alloc, sizeof(struct anim_ctrl_clip)*cnt,
        MID_ANIM);
    ASSERT(ctrl->clips);

    for (uint i = 0; i < cnt; i++)    {
        json_t jclip = json_getarr_item(jclips, i);
        struct anim_ctrl_clip* clip = &ctrl->clips[i];
        strcpy(clip->name, json_gets_child(jclip, "name", ""));
        clip->name_hash = hash_str(clip->name);
    }

    ctrl->clip_cnt = cnt;
}

void anim_ctrl_load_transitions(anim_ctrl ctrl, json_t jtransitions, struct allocator* alloc)
{
    if (jtransitions == nullptr)
        return;

    uint cnt = json_getarr_count(jtransitions);
    if (cnt == 0)
        return;

    ctrl->transitions = (struct anim_ctrl_transition*)
        A_ALLOC(alloc, sizeof(struct anim_ctrl_transition)*cnt, MID_ANIM);
    ASSERT(ctrl->transitions);
    memset(ctrl->transitions, 0x00, sizeof(struct anim_ctrl_transition)*cnt);

    for (uint i = 0; i < cnt; i++)    {
        json_t jtrans = json_getarr_item(jtransitions, i);
        struct anim_ctrl_transition* trans = &ctrl->transitions[i];

        trans->duration = json_getf_child(jtrans, "duration", 0.0f);
        trans->owner_state_idx = json_geti_child(jtrans, "owner", INVALID_INDEX);
        trans->target_state_idx = json_geti_child(jtrans, "target", INVALID_INDEX);

        /* groups */
        json_t jgroups = json_getitem(jtrans, "groups");
        if (jgroups != nullptr)    {
            uint group_cnt = json_getarr_count(jgroups);
            if (group_cnt > 0)  {
                trans->groups = (struct anim_ctrl_transition_group*)A_ALLOC(alloc,
                    sizeof(struct anim_ctrl_transition_group)*group_cnt, MID_GFX);
                ASSERT(trans->groups);
                memset(trans->groups, 0x00, sizeof(struct anim_ctrl_transition_group)*group_cnt);

                for (uint k = 0; k < group_cnt; k++)  {
                    anim_ctrl_parse_group(alloc, &trans->groups[k], json_getarr_item(jgroups, k));
                    trans->group_cnt ++;
                }   /* endfor: groups */
            }
        }

        ctrl->transition_cnt ++;
    }
}

void anim_ctrl_parse_group(struct allocator* alloc, struct anim_ctrl_transition_group* grp,
                           json_t jgrp)
{
    json_t jconds = json_getitem(jgrp, "conditions");
    if (jconds != nullptr) {
        uint cnt = json_getarr_count(jconds);
        grp->item_cnt = cnt;
        if (cnt == 0)
            return;

        grp->items = (struct anim_ctrl_transition_groupitem*)
            A_ALLOC(alloc, sizeof(struct anim_ctrl_transition_groupitem)*cnt, MID_ANIM);
        ASSERT(grp->items);

        for (uint i = 0; i < cnt; i++)    {
            struct anim_ctrl_transition_groupitem* item = &grp->items[i];
            json_t jitem = json_getarr_item(jconds, i);

            item->type = anim_ctrl_parse_grptype(jitem);
            item->param_idx = json_geti_child(jitem, "param", INVALID_INDEX);
            item->predicate = anim_ctrl_parse_grppred(jitem);

            const char* value_type = json_gets_child(jitem, "value-type", "float");
            if (str_isequal_nocase(value_type, "bool"))
                item->value.b = json_getb_child(jitem, "value", FALSE);
            else if (str_isequal_nocase(value_type, "int"))
                item->value.i = json_geti_child(jitem, "value", 0);
            else if (str_isequal_nocase(value_type, "float"))
                item->value.f = json_getf_child(jitem, "value", 0.0f);
        }
    }   else    {
        grp->item_cnt = 0;
        grp->items = nullptr;
    }
}


void anim_ctrl_load_blendtrees(anim_ctrl ctrl, json_t jblendtrees, struct allocator* alloc)
{
    if (jblendtrees == nullptr)
        return;

    uint cnt = json_getarr_count(jblendtrees);
    if (cnt == 0)
        return;

    ctrl->blendtrees = (struct anim_ctrl_blendtree*)A_ALLOC(alloc,
        sizeof(struct anim_ctrl_blendtree)*cnt, MID_ANIM);
    ASSERT(ctrl->blendtrees);
    memset(ctrl->blendtrees, 0x00, sizeof(struct anim_ctrl_blendtree)*cnt);

    for (uint i = 0; i < cnt; i++)    {
        json_t jbt = json_getarr_item(jblendtrees, i);
        struct anim_ctrl_blendtree* bt = &ctrl->blendtrees[i];
        strcpy(bt->name, json_gets_child(jbt, "name", ""));

        bt->param_idx = json_geti_child(jbt, "param", INVALID_INDEX);

        /* childs */
        json_t jchilds = json_getitem(jbt, "childs");
        if (jchilds != nullptr)    {
            uint child_cnt = json_getarr_count(jchilds);
            if (child_cnt > 0)  {
                bt->child_seqs = (struct anim_ctrl_sequence*)A_ALLOC(alloc,
                    sizeof(struct anim_ctrl_sequence)*child_cnt, MID_ANIM);
                ASSERT(bt->child_seqs);

                for (uint k = 0; k < child_cnt; k++)  {
                    json_t jseq = json_getarr_item(jchilds, k);
                    struct anim_ctrl_sequence* seq = &bt->child_seqs[k];
                    seq->idx = json_geti_child(jseq, "id", INVALID_INDEX);
                    seq->type = anim_ctrl_parse_seqtype(jseq);

                    bt->child_seq_cnt ++;
                }
                bt->child_cnt_f = (float)bt->child_seq_cnt;
            }
        }

        ctrl->blendtree_cnt ++;
    }
}


void anim_ctrl_load_states(anim_ctrl ctrl, json_t jstates, struct allocator* alloc)
{
    if (jstates == nullptr)
        return;

    uint cnt = json_getarr_count(jstates);
    if (cnt == 0)
        return;
    ctrl->states = (struct anim_ctrl_state*)A_ALLOC(alloc, sizeof(struct anim_ctrl_state)*cnt,
        MID_ANIM);
    ASSERT(ctrl->states);
    memset(ctrl->states, 0x00, sizeof(struct anim_ctrl_state)*cnt);

    for (uint i = 0; i < cnt; i++)    {
        json_t jstate = json_getarr_item(jstates, i);
        struct anim_ctrl_state* state = &ctrl->states[i];

        strcpy(state->name, json_gets_child(jstate, "name", ""));
        state->speed = json_getf_child(jstate, "speed", 1.0f);

        /* sequence */
        json_t jseq = json_getitem(jstate, "sequence");
        if (jseq != nullptr)   {
            state->seq.type = anim_ctrl_parse_seqtype(jseq);
            state->seq.idx = json_geti_child(jseq, "id", INVALID_INDEX);
        }

        /* transitions */
        json_t jtrans = json_getitem(jstate, "transitions");
        if (jtrans != nullptr) {
            state->transition_cnt = json_getarr_count(jtrans);
            if (state->transition_cnt > 0) {
                state->transitions = (uint*)A_ALLOC(alloc,
                    sizeof(uint)*state->transition_cnt, MID_ANIM);
                ASSERT(state->transitions);

                for (uint k = 0; k < state->transition_cnt; k++)
                    state->transitions[k] = json_geti(json_getarr_item(jtrans, k));
            }
        }

        ctrl->state_cnt ++;
    }
}

void anim_ctrl_load_layers(anim_ctrl ctrl, json_t jlayers, struct allocator* alloc)
{
    if (jlayers == nullptr)
        return;

    uint cnt = json_getarr_count(jlayers);
    if (cnt == 0)
        return;
    ctrl->layers = (struct anim_ctrl_layer*)A_ALLOC(alloc, sizeof(struct anim_ctrl_layer)*cnt,
        MID_ANIM);
    ASSERT(ctrl->layers);
    memset(ctrl->layers, 0x00, sizeof(struct anim_ctrl_layer)*cnt);

    for (uint i = 0; i < cnt; i++)    {
        json_t jlayer = json_getarr_item(jlayers, i);
        struct anim_ctrl_layer* layer = &ctrl->layers[i];

        strcpy(layer->name, json_gets_child(jlayer, "name", ""));
        layer->default_state_idx = json_geti_child(jlayer, "default", INVALID_INDEX);

        layer->type = anim_ctrl_parse_layertype(jlayer);

        /* states */
        json_t jstates = json_getitem(jlayer, "states");
        if (jstates != nullptr)    {
            layer->state_cnt = json_getarr_count(jstates);
            if (layer->state_cnt != 0)  {
                layer->states = (uint*)A_ALLOC(alloc, sizeof(uint)*layer->state_cnt,
                    MID_ANIM);
                ASSERT(layer->states);
                for (uint k = 0; k < layer->state_cnt; k++)
                    layer->states[k] = json_geti(json_getarr_item(jstates, k));
            }
        }

        /* bone-mask */
        json_t jbonemask = json_getitem(jlayer, "bone-mask");
        if (jbonemask != nullptr)  {
            layer->bone_mask_cnt = json_getarr_count(jbonemask);
            if (layer->bone_mask_cnt != 0)  {
                layer->bone_mask = (char*)A_ALLOC(alloc, 32*layer->bone_mask_cnt, MID_ANIM);
                ASSERT(layer->bone_mask);
                for (uint k = 0; k < layer->bone_mask_cnt; k++)
                    strcpy(layer->bone_mask + 32*k, json_gets(json_getarr_item(jbonemask, k)));
            }
        }

        ctrl->layer_cnt ++;
    }
}

void anim_ctrl_unload(anim_ctrl ctrl)
{
    A_ALIGNED_FREE(ctrl->alloc, ctrl);
}

/*************************************************************************************************/
/* note: time (tm) parameter should be global and handled by an external global timer */
void anim_ctrl_update(const anim_ctrl ctrl, anim_ctrl_inst inst, float tm,
                      struct allocator* tmp_alloc)
{
    const anim_reel reel = rs_get_animreel(inst->reel_hdl);
    if (reel == nullptr)
        return;

    for (uint i = 0, cnt = ctrl->layer_cnt; i < cnt; i++) {
        struct anim_ctrl_layer_inst* ilayer = &inst->layers[i];

        /* update current state */
        struct anim_pose* rposes = ilayer->poses;
        if (ilayer->state_idx != INVALID_INDEX)   {
            if (anim_ctrl_checkstate(ctrl, inst, reel, i, ilayer->state_idx, tm))  {
                anim_ctrl_update(ctrl, inst, tm, tmp_alloc);
                return;
            }
            anim_ctrl_updatestate(rposes, ctrl, inst, reel, i, ilayer->state_idx, tm, tmp_alloc);
        }   else if (ilayer->transition_idx != INVALID_INDEX) {
            anim_ctrl_updatetransition(rposes, ctrl, inst, reel, i, ilayer->transition_idx, tm,
                tmp_alloc);
        }   else    {
            /* we have no state, go to default state */
            ilayer->state_idx = ctrl->layers[i].default_state_idx;
            ilayer->transition_idx = INVALID_INDEX;
            anim_ctrl_startstate(ctrl, inst, ilayer->state_idx, tm);
            anim_ctrl_updatestate(rposes, ctrl, inst, reel, i, ilayer->state_idx, tm, tmp_alloc);
        }
    }

    inst->tm = tm;
}

int anim_ctrl_checkstate(const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
    uint layer_idx, uint state_idx, float tm)
{
    const struct anim_ctrl_state* cstate = &ctrl->states[state_idx];
    int condition_meet = FALSE;

    for (uint i = 0, cnt = cstate->transition_cnt; i < cnt; i++)  {
        struct anim_ctrl_transition* trans = &ctrl->transitions[cstate->transitions[i]];
        for (uint k = 0; k < trans->group_cnt; k++)   {
            const struct anim_ctrl_transition_group* tgroup = &trans->groups[k];
            condition_meet |= anim_ctrl_checktgroup(ctrl, inst, reel, state_idx, layer_idx, tgroup,
                tm);
        }

        if (condition_meet) {
            uint trans_idx = cstate->transitions[i];
            if (inst->layers[layer_idx].transition_idx != trans_idx)
                anim_ctrl_starttransition(ctrl, inst, reel, layer_idx, cstate->transitions[i], tm);
            break;
        }
    }

    return condition_meet;
}

int anim_ctrl_checktgroup(const anim_ctrl ctrl, anim_ctrl_inst inst,
                             const anim_reel reel, uint state_idx, uint layer_idx,
                             const struct anim_ctrl_transition_group* tgroup, float tm)
{
    int condition = TRUE;
    for (uint i = 0; i < tgroup->item_cnt && condition; i++)   {
        const struct anim_ctrl_transition_groupitem* item = &tgroup->items[i];

        if (item->type == ANIM_CTRL_TGROUP_EXIT)    {
            float k = anim_ctrl_progress_state(ctrl, inst, reel, state_idx);
            condition &= anim_ctrl_testpredicate_f(item->predicate, k, item->value.f);
        }    else if (item->type == ANIM_CTRL_TGROUP_PARAM) {
            struct anim_ctrl_param_inst* param_i = &inst->params[item->param_idx];
            switch (param_i->type)    {
            case ANIM_CTRL_PARAM_BOOLEAN:
                condition &= anim_ctrl_testpredicate_b(param_i->value.b, item->value.b);
                break;
            case ANIM_CTRL_PARAM_FLOAT:
                condition &= anim_ctrl_testpredicate_f(item->predicate, param_i->value.f,
                    item->value.f);
                break;
            case ANIM_CTRL_PARAM_INT:
                condition &= anim_ctrl_testpredicate_n(item->predicate, param_i->value.i,
                    item->value.i);
                break;
            default:
                break;
            }
        }
    }
    return condition;
}

void anim_ctrl_starttransition(const anim_ctrl ctrl, anim_ctrl_inst inst,
                               const anim_reel reel, uint layer_idx, uint transition_idx,
                               float tm)
{
    const struct anim_ctrl_transition* trans = &ctrl->transitions[transition_idx];
    struct anim_ctrl_transition_inst* itrans = &inst->transitions[transition_idx];
    struct anim_ctrl_layer_inst* ilayer = &inst->layers[layer_idx];

    ilayer->state_idx = INVALID_INDEX;
    ilayer->transition_idx = transition_idx;

    itrans->start_tm = tm;
    itrans->blend = 0.0;

    anim_ctrl_startstate(ctrl, inst, trans->target_state_idx, tm);
}

void anim_ctrl_updatestate(struct anim_pose* poses, const anim_ctrl ctrl, anim_ctrl_inst inst,
                           const anim_reel reel, uint layer_idx, uint state_idx, float tm,
                           struct allocator* tmp_alloc)
{
    const struct anim_ctrl_state* cstate = &ctrl->states[state_idx];

    float progress = anim_ctrl_updateseq(poses, ctrl, inst, reel, &cstate->seq, tm, inst->playrate,
        tmp_alloc);

    /* only update progress for blendtrees because they are recursive */
    if (cstate->seq.type == ANIM_CTRL_SEQUENCE_BLENDTREE)
        inst->blendtrees[cstate->seq.idx].progress = progress;
}

/* returns progress */
float anim_ctrl_updateseq(struct anim_pose* poses,
                         const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
                         const struct anim_ctrl_sequence* seq, float tm, float playrate,
                         struct allocator* tmp_alloc)
{
    if (seq->type == ANIM_CTRL_SEQUENCE_CLIP)
        return anim_ctrl_updateclip(poses, ctrl, inst, reel, seq->idx, tm, playrate);
    else if (seq->type == ANIM_CTRL_SEQUENCE_BLENDTREE)
        return anim_ctrl_updateblendtree(poses, ctrl, inst, reel, seq->idx, tm, playrate, tmp_alloc);
    return 0.0f;
}

/* returns progress */
float anim_ctrl_updateclip(struct anim_pose* poses, const anim_ctrl ctrl,
                          anim_ctrl_inst inst, const anim_reel reel, uint clip_idx, float tm,
                          float playrate)
{
    struct anim_ctrl_clip_inst* iclip = &inst->clips[clip_idx];

    float tm_raw = playrate*(tm - iclip->start_tm);
    float progress = tm_raw / iclip->duration;

    /* calculate local clip time */
    if (iclip->looped)   {
        iclip->tm = fmodf(tm_raw, iclip->duration);
        iclip->progress = progress;
    }   else    {
        iclip->tm = clampf(tm_raw, 0.0f, iclip->duration);
        iclip->progress = clampf(progress, 0.0f, 1.0f);
    }

    /* interpolate frames */
    anim_ctrl_calcpose(poses, reel, iclip->rclip_idx, iclip->tm);

    return iclip->progress;
}

void anim_ctrl_calcpose(struct anim_pose* poses, const anim_reel reel, uint clip_idx, float tm)
{
    const struct anim_clip* clip = &reel->clips[clip_idx];
    float ft = reel->ft;

    uint frame_cnt = clip->frame_end - clip->frame_start;
    uint frame_idx = clampui((uint)(tm/ft), 0, frame_cnt - 1);
    uint frame_next_idx = (frame_idx + 1) % frame_cnt;

    float interpolate = (tm - (frame_idx*ft)) / ft;

    const struct anim_channel* frame_a = &reel->channels[frame_idx + clip->frame_start];
    const struct anim_channel* frame_b = &reel->channels[frame_next_idx + clip->frame_start];

    anim_ctrl_blendpose(poses, frame_a->poses, frame_b->poses, reel->pose_cnt, interpolate);
}

void anim_ctrl_blendpose(struct anim_pose* poses, const struct anim_pose* poses_a,
                         const struct anim_pose* poses_b, uint pose_cnt, float blend)
{
    for (uint i = 0; i < pose_cnt; i++)   {
        vec3_lerp(&poses[i].pos_scale, &poses_a[i].pos_scale, &poses_b[i].pos_scale, blend);
        quat_slerp(&poses[i].rot, &poses_a[i].rot, &poses_b[i].rot, blend);
    }
}

/* returns progress */
float anim_ctrl_updateblendtree(struct anim_pose* poses,
    const anim_ctrl ctrl, anim_ctrl_inst inst,
    const anim_reel reel, uint blendtree_idx, float tm,
    float playrate, struct allocator* tmp_alloc)
{
    const struct anim_ctrl_blendtree* bt = &ctrl->blendtrees[blendtree_idx];
    struct anim_ctrl_blendtree_inst* ibt = &inst->blendtrees[blendtree_idx];

    struct anim_ctrl_param_inst* param_i = &inst->params[bt->param_idx];
    ASSERT(param_i->type == ANIM_CTRL_PARAM_FLOAT);

    float f = clampf(param_i->value.f, 0.0f, 1.0f);    /* must be normalized */

    /* detect which childs we have to blend */
    float progress = f*(bt->child_cnt_f - 1.0f);
    float idx_f = floorf(progress);
    float blend = progress - idx_f;

    uint idx = (uint)idx_f;
    uint idx2 = minui(idx + 1, bt->child_seq_cnt - 1);

    /* updat instance */
    ibt->seq_a = idx;
    ibt->seq_b = idx2;

    ibt->blend = blend;

    /* calculate */
    if (idx != idx2)    {
        const struct anim_ctrl_sequence* seq_a = &bt->child_seqs[idx];
        const struct anim_ctrl_sequence* seq_b = &bt->child_seqs[idx2];

        uint pose_cnt = reel->pose_cnt;
        struct anim_pose* poses_a = (struct anim_pose*)A_ALIGNED_ALLOC(tmp_alloc,
            sizeof(struct anim_pose)*pose_cnt, MID_ANIM);
        struct anim_pose* poses_b = (struct anim_pose*)A_ALIGNED_ALLOC(tmp_alloc,
            sizeof(struct anim_pose)*pose_cnt, MID_ANIM);
        ASSERT(poses_a);
        ASSERT(poses_b);

        float progress_a = anim_ctrl_updateseq(poses_a, ctrl, inst, reel, seq_a, tm, playrate,
            tmp_alloc);
        float progress_b = anim_ctrl_updateseq(poses_b, ctrl, inst, reel, seq_b, tm, playrate,
            tmp_alloc);

        A_ALIGNED_FREE(tmp_alloc, poses_a);
        A_ALIGNED_FREE(tmp_alloc, poses_b);

        /* blend two sequences */
        anim_ctrl_blendpose(poses, poses_a, poses_b, pose_cnt, blend);
        progress = (1.0f - blend)*progress_a + blend*progress_b;
    }    else   {
        const struct anim_ctrl_sequence* seq = &bt->child_seqs[idx];
        progress = anim_ctrl_updateseq(poses, ctrl, inst, reel, seq, tm, playrate, tmp_alloc);
    }

    return progress;
}


void anim_ctrl_updatetransition(struct anim_pose* poses,
                                const anim_ctrl ctrl, anim_ctrl_inst inst,
                                const anim_reel reel, uint layer_idx, uint transition_idx,
                                float tm, struct allocator* tmp_alloc)
{
    const struct anim_ctrl_transition* trans = &ctrl->transitions[transition_idx];
    struct anim_ctrl_transition_inst* itrans = &inst->transitions[transition_idx];
    struct anim_ctrl_layer_inst* ilayer = &inst->layers[layer_idx];
    ASSERT(trans->owner_state_idx != INVALID_INDEX);
    ASSERT(trans->target_state_idx != INVALID_INDEX);

    float elapsed = inst->playrate*(tm - itrans->start_tm);   /* local elapsed time */
    float blend = clampf(elapsed / trans->duration, 0.0f, 1.0f);
    itrans->blend = blend;

    if (blend == 1.0f)  {
        /* finish transition and move on to target state */
        ilayer->state_idx = trans->target_state_idx;
        ilayer->transition_idx = INVALID_INDEX;
        anim_ctrl_updatestate(poses, ctrl, inst, reel, layer_idx, trans->target_state_idx, tm,
            tmp_alloc);
    }   else {
        /* do blending of two states */
        uint pose_cnt = reel->pose_cnt;
        struct anim_pose* poses_a = (struct anim_pose*)A_ALIGNED_ALLOC(tmp_alloc,
            sizeof(struct anim_pose)*pose_cnt, MID_ANIM);
        struct anim_pose* poses_b = (struct anim_pose*)A_ALIGNED_ALLOC(tmp_alloc,
            sizeof(struct anim_pose)*pose_cnt, MID_ANIM);
        ASSERT(poses_a);
        ASSERT(poses_b);

        anim_ctrl_updatestate(poses_a, ctrl, inst, reel, layer_idx, trans->owner_state_idx, tm,
            tmp_alloc);
        anim_ctrl_updatestate(poses_b, ctrl, inst, reel, layer_idx, trans->target_state_idx, tm,
            tmp_alloc);

        A_ALIGNED_FREE(tmp_alloc, poses_a);
        A_ALIGNED_FREE(tmp_alloc, poses_b);

        anim_ctrl_blendpose(poses, poses_a, poses_b, pose_cnt, blend);
    }
}

float anim_ctrl_progress_state(const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
    uint state_idx)
{
    const struct anim_ctrl_state* cstate = &ctrl->states[state_idx];
    const struct anim_ctrl_sequence* seq = &cstate->seq;

    if (seq->type == ANIM_CTRL_SEQUENCE_CLIP)
        return minf(inst->clips[seq->idx].progress, 1.0f);
    else if (seq->type == ANIM_CTRL_SEQUENCE_BLENDTREE)
        return minf(inst->blendtrees[seq->idx].progress, 1.0f);

    return 0.0f;
}

void anim_ctrl_startstate(const anim_ctrl ctrl, anim_ctrl_inst inst, uint state_idx,
                          float start_tm)
{
    struct anim_ctrl_state* cstate = &ctrl->states[state_idx];
    anim_ctrl_startseq(ctrl, inst, &cstate->seq, start_tm);
}

void anim_ctrl_startseq(const anim_ctrl ctrl, anim_ctrl_inst inst,
                        const struct anim_ctrl_sequence* seq, float start_tm)
{
    if (seq->type == ANIM_CTRL_SEQUENCE_CLIP)
        anim_ctrl_startclip(ctrl, inst, seq->idx, start_tm);
    else if (seq->type == ANIM_CTRL_SEQUENCE_BLENDTREE)
        anim_ctrl_startblendtree(ctrl, inst, seq->idx, start_tm);
}

void anim_ctrl_startclip(const anim_ctrl ctrl, anim_ctrl_inst inst, uint clip_idx, float start_tm)
{
    struct anim_ctrl_clip_inst* iclip = &inst->clips[clip_idx];
    iclip->start_tm = start_tm;
    iclip->progress = 0.0f;
}

void anim_ctrl_startblendtree(const anim_ctrl ctrl, anim_ctrl_inst inst, uint blendtree_idx,
                              float start_tm)
{
    struct anim_ctrl_blendtree_inst* ibt = &inst->blendtrees[blendtree_idx];
    ibt->seq_a = INVALID_INDEX;
    ibt->seq_b = INVALID_INDEX;
    ibt->progress = 0.0f;

    /* recurse for child blendtrees */
    const struct anim_ctrl_blendtree* bt = &ctrl->blendtrees[blendtree_idx];
    for (uint i = 0, cnt = bt->child_seq_cnt; i < cnt; i++)
        anim_ctrl_startseq(ctrl, inst, &bt->child_seqs[i], start_tm);
}

/*************************************************************************************************/
void anim_ctrl_debug(anim_ctrl ctrl, anim_ctrl_inst inst)
{
    char msg[128];
    const int lh = 15;
    int x = 10;
    int y = 100;

    sprintf(msg, "time: %.3f", inst->tm);
    gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;

    strcpy(msg, "params:");
    gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;
    for (uint i = 0; i < ctrl->param_cnt; i++)    {
        sprintf(msg, "  name: %s", ctrl->params[i].name);
        gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;
        switch (inst->params[i].type)   {
        case ANIM_CTRL_PARAM_BOOLEAN:
            sprintf(msg, "  value (bool): %d", inst->params[i].value.b);
            break;
        case ANIM_CTRL_PARAM_FLOAT:
            sprintf(msg, "  value (float): %.3f", inst->params[i].value.f);
            break;
        case ANIM_CTRL_PARAM_INT:
            sprintf(msg, "  value (int): %d", inst->params[i].value.i);
            break;
        default:
            break;
        }
        gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;
    }

    for (uint i = 0; i < ctrl->layer_cnt; i++)    {
        struct anim_ctrl_layer* l = &ctrl->layers[i];
        struct anim_ctrl_layer_inst* li = &inst->layers[i];

        sprintf(msg, "layer: %s", l->name);
        gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;

        /* state */
        if (li->state_idx != INVALID_INDEX)  {
            struct anim_ctrl_state* state = &ctrl->states[li->state_idx];
            sprintf(msg, "  state: %s", state->name);
            gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;

            if (state->seq.type == ANIM_CTRL_SEQUENCE_CLIP) {
                sprintf(msg, "    clip: %s, %.3f", ctrl->clips[state->seq.idx].name,
                    inst->clips[state->seq.idx].progress);
            }   else if (state->seq.type == ANIM_CTRL_SEQUENCE_BLENDTREE)   {
                sprintf(msg, "    blendtree: %s (%d, %d, blend=%.2f, progress=%.2f)",
                    ctrl->blendtrees[state->seq.idx].name,
                    inst->blendtrees[state->seq.idx].seq_a,
                    inst->blendtrees[state->seq.idx].seq_b,
                    inst->blendtrees[state->seq.idx].blend,
                    inst->blendtrees[state->seq.idx].progress);
            }
            gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;
        }   else if (li->transition_idx != INVALID_INDEX)    {
            struct anim_ctrl_transition* trans = &ctrl->transitions[li->transition_idx];
            sprintf(msg, "  transition: %d", li->transition_idx);
            gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;
            sprintf(msg, "    duration: %.2f", trans->duration);
            gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;
            sprintf(msg, "    blend: %.2f", inst->transitions[li->transition_idx].blend);
            gfx_canvas_text2dpt(msg, x, y, 0);  y += lh;
        }
    }
}

enum anim_ctrl_paramtype anim_ctrl_get_paramtype(anim_ctrl ctrl, anim_ctrl_inst inst,
    const char* name)
{
    struct hashtable_item* item = hashtable_fixed_find(&ctrl->param_tbl, hash_str(name));
    if (item != nullptr)
        return inst->params[item->value].type;
    return ANIM_CTRL_PARAM_UNKNOWN;
}

float anim_ctrl_get_paramf(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name)
{
    struct hashtable_item* item = hashtable_fixed_find(&ctrl->param_tbl, hash_str(name));
    if (item != nullptr)   {
        ASSERT(inst->params[item->value].type == ANIM_CTRL_PARAM_FLOAT);
        return inst->params[item->value].value.f;
    }
    return 0.0f;
}

void anim_ctrl_set_paramf(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name, float value)
{
    struct hashtable_item* item = hashtable_fixed_find(&ctrl->param_tbl, hash_str(name));
    if (item != nullptr)   {
        ASSERT(inst->params[item->value].type == ANIM_CTRL_PARAM_FLOAT);
        inst->params[item->value].value.f = value;
    }
}

int anim_ctrl_get_paramb(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name)
{
    struct hashtable_item* item = hashtable_fixed_find(&ctrl->param_tbl, hash_str(name));
    if (item != nullptr)   {
        ASSERT(inst->params[item->value].type == ANIM_CTRL_PARAM_BOOLEAN);
        return inst->params[item->value].value.b;
    }
    return FALSE;
}

void anim_ctrl_set_paramb(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name, int value)
{
    struct hashtable_item* item = hashtable_fixed_find(&ctrl->param_tbl, hash_str(name));
    if (item != nullptr)   {
        ASSERT(inst->params[item->value].type == ANIM_CTRL_PARAM_BOOLEAN);
        inst->params[item->value].value.b = value;
    }
}

int anim_ctrl_get_parami(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name)
{
    struct hashtable_item* item = hashtable_fixed_find(&ctrl->param_tbl, hash_str(name));
    if (item != nullptr)   {
        ASSERT(inst->params[item->value].type == ANIM_CTRL_PARAM_INT);
        return inst->params[item->value].value.i;
    }
    return FALSE;
}

void anim_ctrl_set_parami(anim_ctrl ctrl, anim_ctrl_inst inst, const char* name, int value)
{
    struct hashtable_item* item = hashtable_fixed_find(&ctrl->param_tbl, hash_str(name));
    if (item != nullptr)   {
        ASSERT(inst->params[item->value].type == ANIM_CTRL_PARAM_INT);
        inst->params[item->value].value.i = value;
    }
}

void anim_ctrl_setupclip(const anim_ctrl ctrl, const anim_ctrl_inst inst, const anim_reel reel,
                         uint clip_idx)
{
    const struct anim_ctrl_clip* cclip = &ctrl->clips[clip_idx];
    struct anim_ctrl_clip_inst* iclip = &inst->clips[clip_idx];

    uint rclip_idx = anim_findclip_hashed(reel, cclip->name_hash);
    if (rclip_idx == INVALID_INDEX) {
        iclip->rclip_idx = INVALID_INDEX;
        return;
    }

    const struct anim_clip* rclip = &reel->clips[rclip_idx];
    iclip->duration = rclip->duration;
    iclip->looped = rclip->looped;
    iclip->rclip_idx = rclip_idx;
}

result_t anim_ctrl_bindreel(anim_ctrl_inst inst, const anim_reel reel)
{
    /* calcualte clip durations */
    const anim_ctrl ctrl = inst->owner;

    /* clips */
    for (uint i = 0; i < ctrl->clip_cnt; i++)
        anim_ctrl_setupclip(ctrl, inst, reel, i);

    /* layers */
    for (uint i = 0; i < ctrl->layer_cnt; i++)    {
        struct anim_ctrl_layer_inst* ilayer = &inst->layers[i];
        if (ilayer->buff != nullptr)
            A_ALIGNED_FREE(inst->alloc, ilayer->buff);
        size_t sz = (sizeof(struct anim_pose) + sizeof(float)) * reel->pose_cnt;
        uint8* buff = (uint8*)A_ALIGNED_ALLOC(inst->alloc, sz, MID_ANIM);
        if (buff == nullptr)
            return RET_OUTOFMEMORY;
        memset(buff, 0x00, sz);

        ilayer->buff = buff;
        ilayer->poses = (struct anim_pose*)buff;
        buff += sizeof(struct anim_pose)*reel->pose_cnt;

        /* construct bone-mask, bone-mask is an array of multipliers that applies to final result */
        ilayer->bone_mask = (float*)buff;
        const struct anim_ctrl_layer* layer = &ctrl->layers[i];

        if (i != 0 || layer->bone_mask_cnt != 0) {
            for (uint k = 0; k < layer->bone_mask_cnt; k++)   {
                const char* mask_name = layer->bone_mask + k*32;
                /* find the mask_name in reel's bindings */
                uint pose_idx = anim_find_posebinding(reel, mask_name);
                if (pose_idx != INVALID_INDEX)
                    ilayer->bone_mask[pose_idx] = 1.0f;
            }
        }   else    {
            /* main (first one) layer always applies to full body */
            for (uint k = 0; k < reel->pose_cnt; k++)
                ilayer->bone_mask[k] = 1.0f;
        }
    }

    return RET_OK;
}

void anim_ctrl_unbindreel(anim_ctrl_inst inst)
{
    for (uint i = 0; i < inst->layer_cnt; i++)    {
        struct anim_ctrl_layer_inst* ilayer = &inst->layers[i];
        if (ilayer->buff != nullptr)   {
            A_ALIGNED_FREE(inst->alloc, ilayer->buff);
            ilayer->buff = nullptr;
            ilayer->poses = nullptr;
            ilayer->bone_mask = nullptr;
        }
    }
}

anim_ctrl_inst anim_ctrl_createinstance(struct allocator* alloc, const anim_ctrl ctrl)
{
    /* calculate bytes needed to create the whole instance data */
    size_t bytes =
        sizeof(struct anim_ctrl_instance_data) +
        ctrl->param_cnt*sizeof(struct anim_ctrl_param_inst) +
        ctrl->blendtree_cnt*sizeof(struct anim_ctrl_blendtree_inst) +
        ctrl->clip_cnt*sizeof(struct anim_ctrl_clip_inst) +
        ctrl->layer_cnt*sizeof(struct anim_ctrl_layer_inst) +
        ctrl->transition_cnt*sizeof(struct anim_ctrl_transition_inst);

    uint8* buff = (uint8*)A_ALIGNED_ALLOC(alloc, bytes, MID_ANIM);
    if (buff == nullptr)   {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return nullptr;
    }
    memset(buff, 0x00, bytes);
    struct anim_ctrl_instance_data* inst = (struct anim_ctrl_instance_data*)buff;

    inst->alloc = alloc;
    inst->owner = ctrl;
    inst->playrate = 1.0f;
    buff += sizeof(struct anim_ctrl_instance_data);

    /* load animation reel */
    inst->reel_hdl = rs_load_animreel(ctrl->reel_filepath, 0);
    if (inst->reel_hdl == INVALID_HANDLE)   {
        err_printf(__FILE__, __LINE__, "Creating anim-ctrl instance failed: could not load resource"
            " '%s'", ctrl->reel_filepath);
        A_FREE(alloc, inst);
        return nullptr;
    }

    if (ctrl->param_cnt > 0)    {
        inst->params = (struct anim_ctrl_param_inst*)buff;
        for (uint i = 0; i < ctrl->param_cnt; i++)    {
            struct anim_ctrl_param_inst* param = &inst->params[i];
            param->type = ctrl->params[i].type;
            param->value.i = ctrl->params[i].value.i;
        }
        buff += sizeof(struct anim_ctrl_param_inst)*ctrl->param_cnt;
    }

    if (ctrl->layer_cnt > 0)    {
        inst->layers = (struct anim_ctrl_layer_inst*)buff;
        for (uint i = 0; i < ctrl->layer_cnt; i++)    {
            struct anim_ctrl_layer_inst* layer = &inst->layers[i];
            layer->state_idx = INVALID_INDEX;
            layer->transition_idx = INVALID_INDEX;
            switch (ctrl->layers[i].type)   {
                case ANIM_CTRL_LAYER_OVERRIDE:
                layer->blend_fn = anim_ctrl_layer_override;
                break;
                case ANIM_CTRL_LAYER_ADDITIVE:
                layer->blend_fn = anim_ctrl_layer_additive;
                break;
            }
        }
        buff += sizeof(struct anim_ctrl_layer_inst)*ctrl->layer_cnt;
        inst->layer_cnt = ctrl->layer_cnt;
    }

    if (ctrl->clip_cnt > 0) {
        inst->clips = (struct anim_ctrl_clip_inst*)buff;
        for (uint i = 0; i < ctrl->clip_cnt; i++) {
            struct anim_ctrl_clip_inst* clip = &inst->clips[i];
            clip->start_tm = 0.0f;
            clip->tm = 0.0f;
        }
        buff += sizeof(struct anim_ctrl_clip_inst)*ctrl->clip_cnt;
    }

    if (ctrl->blendtree_cnt > 0)    {
        inst->blendtrees = (struct anim_ctrl_blendtree_inst*)buff;
        for (uint i = 0; i < ctrl->blendtree_cnt; i++)    {
            struct anim_ctrl_blendtree_inst* bt = &inst->blendtrees[i];
            bt->seq_a = INVALID_INDEX;
            bt->seq_b = INVALID_INDEX;
            bt->blend = 0.0f;
        }
        buff += sizeof(struct anim_ctrl_blendtree_inst)*ctrl->blendtree_cnt;
    }

    if (ctrl->transition_cnt > 0)   {
        inst->transitions = (struct anim_ctrl_transition_inst*)buff;
        for (uint i = 0; i < ctrl->transition_cnt; i++)   {
            struct anim_ctrl_transition_inst* trans = &inst->transitions[i];
            trans->blend = 0.0f;
            trans->start_tm = 0.0f;
        }
        buff += sizeof(struct anim_ctrl_transition_inst)*ctrl->transition_cnt;
    }

    anim_reel reel = rs_get_animreel(inst->reel_hdl);
    if (reel == nullptr)
        return inst;

    if (IS_FAIL(anim_ctrl_bindreel(inst, reel)))    {
        anim_ctrl_destroyinstance(inst);
        return nullptr;
    }

    return inst;
}

void anim_ctrl_destroyinstance(anim_ctrl_inst inst)
{
    anim_ctrl_unbindreel(inst);

    if (inst->reel_hdl != INVALID_HANDLE)
        rs_unload(inst->reel_hdl);

    A_ALIGNED_FREE(inst->alloc, inst);
}

result_t anim_ctrl_set_reel(anim_ctrl_inst inst, reshandle_t reel_hdl)
{
    ASSERT(reel_hdl != INVALID_HANDLE);

    anim_ctrl_unbindreel(inst);
    anim_reel reel = rs_get_animreel(reel_hdl);
    if (reel != nullptr)   {
        if (IS_FAIL(anim_ctrl_bindreel(inst, reel)))
            return RET_FAIL;
    }

    inst->reel_hdl = reel_hdl;
    return RET_OK;
}

void anim_ctrl_fetchresult_hierarchal(const anim_ctrl_inst inst, const uint* bindmap,
                                      const cmphandle_t* xforms, const uint* root_idxs,
                                      uint root_idx_cnt, const Mat3* root_mat)
{
    const anim_reel reel = rs_get_animreel(inst->reel_hdl);
    if (reel == nullptr)
        return;

    uint pose_cnt = reel->pose_cnt;
    uint layer_cnt = inst->layer_cnt;

    Mat3 mat;
    Mat3 mat_tmp;

    for (uint i = 0; i < pose_cnt; i++)   {
        memset(&mat, 0x00, sizeof(mat));

        /* add layer matrices for each pose */
        for (uint k = 0; k < layer_cnt; k++)  {
            struct anim_pose* poses = inst->layers[k].poses;
            mat3_set_trans_rot(&mat_tmp, &poses[i].pos_scale, &poses[i].rot);
            inst->layers[k].blend_fn(&mat, &mat_tmp, &mat, inst->layers[k].bone_mask[i]);
        }

        cmphandle_t xfh = xforms[bindmap[i]];
        struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(xfh);
        mat3_setm(&xf->mat, &mat);
    }

    for (uint i = 0; i < root_idx_cnt; i++)   {
        cmphandle_t xfh = xforms[root_idxs[i]];
        struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(xfh);
        mat3_mul(&xf->mat, &xf->mat, root_mat);
    }
}

void anim_ctrl_fetchresult_skeletal(const anim_ctrl_inst inst, const uint* bindmap,
    Mat3* joints, const uint* root_idxs, uint root_idx_cnt, const Mat3* root_mat)
{
    const anim_reel reel = rs_get_animreel(inst->reel_hdl);
    if (reel == nullptr)
        return;

    uint pose_cnt = reel->pose_cnt;
    uint layer_cnt = inst->layer_cnt;

    Mat3 mat;
    Mat3 mat_tmp;

    for (uint i = 0; i < pose_cnt; i++)   {
        memset(&mat, 0x00, sizeof(mat));

        /* add layer matrices for each pose */
        for (uint k = 0; k < layer_cnt; k++)  {
            struct anim_pose* poses = inst->layers[k].poses;
            mat3_set_trans_rot(&mat_tmp, &poses[i].pos_scale, &poses[i].rot);
            inst->layers[k].blend_fn(&mat, &mat_tmp, &mat, inst->layers[k].bone_mask[i]);
        }

        mat3_setm(&joints[bindmap[i]], &mat);
    }

    for (uint i = 0; i < root_idx_cnt; i++)   {
        mat3_mul(&joints[root_idxs[i]], &joints[root_idxs[i]], root_mat);
    }
}

const Mat3* anim_ctrl_layer_override(Mat3* result, Mat3* src,
    Mat3* dest, float mask)
{
    return mat3_setm(result, mask == 0.0f ? dest : src);
}

const Mat3* anim_ctrl_layer_additive(Mat3* result, Mat3* src,
    Mat3* dest, float mask)
{
    return mat3_add(result, dest, mat3_muls(src, src, mask));
}

reshandle_t anim_ctrl_get_reel(const anim_ctrl_inst inst)
{
    return inst->reel_hdl;
}

int anim_ctrl_get_curstate(anim_ctrl ctrl, anim_ctrl_inst inst, const char* layer_name, 
    char* state, float* progress)
{
    /* find layer */
    for (uint i = 0; i < ctrl->layer_cnt; i++)  {
        if (str_isequal(ctrl->layers[i].name, layer_name))  {
            struct anim_ctrl_layer_inst* ilayer = &inst->layers[i];
            if (ilayer->state_idx != INVALID_INDEX) {
                uint idx = ilayer->state_idx;
                float p = 0.0f;
                if (ctrl->states[idx].seq.type == ANIM_CTRL_SEQUENCE_CLIP)
                    p = inst->clips[ctrl->states[idx].seq.idx].progress;
                else if (ctrl->states[idx].seq.type == ANIM_CTRL_SEQUENCE_BLENDTREE)
                    p = inst->blendtrees[ctrl->states[idx].seq.idx].progress;

                if (progress)
                    *progress = p;

                strcpy(state, ctrl->states[idx].name);

                return TRUE;
            }

            break;
        }
    }
    return FALSE;
}

int anim_ctrl_get_curtransition(anim_ctrl ctrl, anim_ctrl_inst inst, const char* layer_name, 
    char* state_a, char* state_b, OUT OPTIONAL float* progress)
{
    for (uint i = 0; i < ctrl->layer_cnt; i++)  {
        if (str_isequal(ctrl->layers[i].name, layer_name))  {
            struct anim_ctrl_layer_inst* ilayer = &inst->layers[i];
            if (ilayer->transition_idx != INVALID_INDEX) {
                uint idx = ilayer->transition_idx;
                if (progress)
                    *progress = inst->transitions[idx].blend;

                if (ctrl->transitions[idx].owner_state_idx != INVALID_INDEX)
                    strcpy(state_a, ctrl->states[ctrl->transitions[idx].owner_state_idx].name);
                if (ctrl->transitions[idx].target_state_idx != INVALID_INDEX)
                    strcpy(state_b, ctrl->states[ctrl->transitions[idx].target_state_idx].name);

                return TRUE;
            }

            break;
        }
    }
    return FALSE;
}
