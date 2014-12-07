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

    void update_nodes(int clip, float t, const int* bindmap, const cmphandle_t *xforms,
                      int frame_force, const int *root_idxs, int root_idx_cnt, const Mat3 *root_mat)
    {
        int frame_cnt, frame_idx;
        const Reel::Clip *subclip = &reel->_clips[clip];
        float ft = reel->ft;

        if (frame_force == -1)   {
            frame_cnt = subclip->frame_end - subclip->frame_start;
            frame_idx = tclamp<int>((int)(t/ft), 0, frame_cnt-1);
        }   else    {
            frame_cnt = reel->frame_cnt;
            frame_idx = frame_force;
        }

        int nextframe_idx = (frame_idx + 1) % frame_cnt;

        // Interpolate between two frames and normalize (0-1)
        float ivalue = (t - (frame_idx * ft)) / ft;

        Reel::Channel *sampl = &reel->_channels[frame_idx + subclip->frame_start];
        Reel::Channel *next_sampl = &reel->_channels[nextframe_idx + subclip->frame_start];

        Mat3 xfm(Mat3::Ident);

        for (int i = 0, pose_cnt = reel->pose_cnt; i < pose_cnt; i++)    {
            Vec3 pos = Vec3::lerp(sampl->poses[i].pos_scale, next_sampl->poses[i].pos_scale, ivalue);
            Quat rot = Quat::slerp(sampl->poses[i].rot, next_sampl->poses[i].rot, ivalue);

            xfm.set_rotation_quat(rot);
            xfm.set_translation(pos);

            cmphandle_t xfh = xforms[bindmap[i]];
            cmp_xform *xf = (cmp_xform*)cmp_getinstancedata(xfh);

            mat3_setm(&xf->mat, &xfm);
        }

        for (int i = 0; i < root_idx_cnt; i++)   {
            cmphandle_t xfh = xforms[root_idxs[i]];
            struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(xfh);
            mat3_mul(&xf->mat, &xf->mat, root_mat);
        }
    }

    void update_skeletal(int clip, float t, const int* bindmap, Mat3 *joints, int frame_force,
                         const int *root_idxs, int root_idx_cnt, const Mat3 *root_mat)
    {
        int frame_cnt, frame_idx;
        const Reel::Clip *subclip = &reel->_clips[clip];
        float ft = reel->ft;

        if (frame_force == INVALID_INDEX)   {
            frame_cnt = subclip->frame_end - subclip->frame_start;
            frame_idx = tclamp<int>((int)(t/ft), 0, frame_cnt-1);
        }   else    {
            frame_cnt = reel->frame_cnt;
            frame_idx = frame_force;
        }

        int nextframe_idx = (frame_idx + 1) % frame_cnt;
        float ivalue = (t - (frame_idx * ft)) / ft;

        Reel::Channel *sampl = &reel->_channels[frame_idx + subclip->frame_start];
        Reel::Channel *next_sampl = &reel->_channels[nextframe_idx + subclip->frame_start];

        Mat3 xfm(Mat3::Ident);

        for (int i = 0, pose_cnt = reel->pose_cnt; i < pose_cnt; i++)    {
            Vec3 pos = Vec3::lerp(sampl->poses[i].pos_scale, next_sampl->poses[i].pos_scale, ivalue);
            Quat rot = Quat::slerp(sampl->poses[i].rot, next_sampl->poses[i].rot, ivalue);

            xfm.set_translation(pos);
            xfm.set_rotation_quat(rot);

            mat3_setm(&joints[bindmap[i]], &xfm);
        }

        for (int i = 0; i < root_idx_cnt; i++)
            mat3_mul(&joints[root_idxs[i]], &joints[root_idxs[i]], root_mat);
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
        ConditionGroup *groups;
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
    void destroy()
    {
        mem_delete_alloc_aligned<CharController>(_alloc, this);
    }

private:
    static int jloader_getcount(JNode jparent, const char *name)
    {
        JNode j = jparent.child(name);
        if (j.is_valid())        return json_getarr_count(j);
        else                     return 0;
    }

    static int jloader_getcount_2nd(JNode jparent, const char *name0, const char *name1)
    {
        JNode j = jparent.child(name0);
        if (j.is_valid())  {
            int cnt = 0;
            for (int i = 0, l1_cnt = j.array_item_count(); i < l1_cnt; i++)    {
                JNode j2 = j.array_item(i).child(name1);
                cnt += j2.is_valid() ? j2.array_item_count() : 0;
            }
            return cnt;
        }   else    {
            return 0;
        }
    }

    static int jloader_getcount_3rd(JNode jparent, const char *name0, const char *name1, 
                                    const char *name2)
    {
        JNode j = jparent.child(name0);
        if (j.is_valid())  {
            int cnt = 0;
            for (int i = 0, l1_cnt = j.array_item_count(); i < l1_cnt; i++)    {
                JNode j2 = j.array_item(i).child(name1);
                if (j2.is_valid()) {
                    for (int k = 0, l2_cnt = j2.array_item_count(); k < l2_cnt; k++) {
                        JNode j3 = j2.array_item(k).child(name2);
                        cnt += j3.is_valid() ? j3.array_item_count() : 0;
                    }
                }
            }
            return cnt;
        }   else    {
            return 0;
        }
    }

    static void jloader_load_params(CharController *ctrl, JNode jparams, Allocator *alloc)
    {
        if (!jparams.is_valid())
            return;

        int cnt = jparams.array_item_count();
        if (cnt == 0)
            return;
        ctrl->_params = (struct Param*)A_ALLOC(alloc, sizeof(Param)*cnt, MID_ANIM);
        ASSERT(ctrl->_params);
        memset(ctrl->_params, 0x00, sizeof(Param)*cnt);

        ctrl->_params_tbl.create(cnt, alloc);

        for (int i = 0; i < cnt; i++)    {
            JNode jparam = jparams.array_item(i);
            Param *param = &ctrl->_params[i];

            str_safecpy(param->name, sizeof(param->name), jparam.child_str("name"));
            ctrl->_params_tbl.add(param->name, i);

            const char *type = jparam.child_str("type", "float");
            if (str_isequal_nocase(type, "float"))
                param->value = jparam.child_float("value");
            else if (str_isequal_nocase(type, "int"))
                param->value = jparam.child_int("value");
            else if (str_isequal_nocase(type, "bool"))
                param->value = jparam.child_bool("value");
            else
                param->value = 0.0f;
        }

        ctrl->_param_cnt = cnt;
    }

    static void jloader_load_clips(CharController *ctrl, JNode jclips, Allocator *alloc)
    {
        if (!jclips.is_valid())
            return;

        int cnt = jclips.array_item_count();
        if (cnt == 0)
            return;

        ctrl->_clips = (Clip*)A_ALLOC(alloc, sizeof(Clip)*cnt, MID_ANIM);
        ASSERT(ctrl->_clips);
        memset(ctrl->_clips, 0x00, sizeof(Clip)*cnt);

        for (int i = 0; i < cnt; i++)    {
            JNode jclip = jclips.array_item(i);
            Clip *clip = &ctrl->_clips[i];
            str_safecpy(clip->name, sizeof(clip->name), jclip.child_str("name"));
            clip->name_hash = hash_str(clip->name);
        }

        ctrl->_clip_cnt = cnt;
    }

    static SequenceType jloader_parse_seqtype(JNode jseq)
    {
        const char *seq_type_s = jseq.child_str("type");
        if (str_isequal(seq_type_s, "clip"))
            return SequenceType::CLIP;
        else if (str_isequal(seq_type_s, "blendtree"))
            return SequenceType::BLEND_TREE;
        else
            return SequenceType::UNKNOWN;
    }

    static LayerType jloader_parse_layertype(JNode jtype)
    {
        const char *layer_type_s = jtype.child_str("layer");
        if (str_isequal(layer_type_s, "override"))
            return LayerType::OVERRIDE;
        else if (str_isequal(layer_type_s, "additive"))
            return LayerType::ADDITIVE;
        else
            return LayerType::OVERRIDE;
    }

    static ConditionType jloader_parse_grptype(JNode jgrp)
    {
        const char *type_s = jgrp.child_str("type");

        if (str_isequal(type_s, "exit"))
            return ConditionType::EXIT;
        else if (str_isequal(type_s, "param"))
            return ConditionType::PARAM;
        else
            return ConditionType::EXIT;
    }

    static Predicate jloader_parse_grppred(JNode jgrp)
    {
        const char *pred_s = jgrp.child_str("predicate");

        if (str_isequal(pred_s, "=="))
            return Predicate::EQUAL;
        else if (str_isequal(pred_s, "!="))
            return Predicate::NOT;
        else if (str_isequal(pred_s, ">"))
            return Predicate::GREATER;
        else if (str_isequal(pred_s, "<"))
            return Predicate::LESS;
        else
            return Predicate::UNKNOWN;
    }

    static void json_parse_condgroup(JNode jgrp, ConditionGroup *grp, Allocator* alloc)
    {
        ASSERT(jgrp.is_valid());

        JNode jconds = jgrp.child("conditions");
        if (jconds.is_valid()) {
            int cnt = jconds.array_item_count();
            grp->count = cnt;
            if (cnt == 0)
                return;

            grp->conditions = (Condition*)A_ALLOC(alloc, sizeof(Condition)*cnt, MID_ANIM);
            ASSERT(grp->conditions);

            for (int i = 0; i < cnt; i++)    {
                Condition *item = &grp->conditions[i];
                JNode jitem = jconds.array_item(i);

                item->type = jloader_parse_grptype(jitem);
                item->param = jitem.child_int("param", -1);
                item->predicate = jloader_parse_grppred(jitem);

                const char *value_type = jitem.child_str("value-type", "float");
                if (str_isequal_nocase(value_type, "bool"))
                    item->value = jitem.child_bool("value");
                else if (str_isequal_nocase(value_type, "int"))
                    item->value = jitem.child_int("value");
                else if (str_isequal_nocase(value_type, "float"))
                    item->value = jitem.child_float("value");
            }
        }   else    {
            grp->count = 0;
            grp->conditions = nullptr;
        }
    }

    static void jloader_load_transitions(CharController *ctrl, JNode jtransitions, Allocator *alloc)
    {
        if (!jtransitions.is_valid())
            return;

        int cnt = jtransitions.array_item_count();
        if (cnt == 0)   
            return;

        ctrl->_transitions = (Transition*)A_ALLOC(alloc, sizeof(Transition)*cnt, MID_ANIM);
        ASSERT(ctrl->_transitions);
        memset(ctrl->_transitions, 0x00, sizeof(Transition)*cnt);

        for (int i = 0; i < cnt; i++)    {
            JNode jtrans = jtransitions.array_item(i);
            Transition *trans = &ctrl->_transitions[i];

            trans->duration = jtrans.child_float("duration");
            trans->owner_state = jtrans.child_int("owner", -1);
            trans->target_state = jtrans.child_int("target", -1);

            // Condition groups
            JNode jgroups = jtrans.child("groups");
            if (jgroups.is_valid())    {
                int group_cnt = jgroups.array_item_count();
                if (group_cnt)  {
                    trans->groups = (ConditionGroup*)A_ALLOC(alloc, 
                        sizeof(ConditionGroup)*group_cnt, MID_GFX);
                    ASSERT(trans->groups);
                    memset(trans->groups, 0x00, sizeof(ConditionGroup)*group_cnt);

                    for (int k = 0; k < group_cnt; k++)  {
                        json_parse_condgroup(jgroups.array_item(k), &trans->groups[k], alloc);
                        trans->group_cnt ++;
                    }  
                }
            }

            ctrl->_transition_cnt ++;
        }
    }

    static void jloader_load_blendtrees(CharController *ctrl, JNode jblendtrees, Allocator* alloc)
    {
        if (!jblendtrees.is_valid())
            return;

        int cnt = jblendtrees.array_item_count();
        if (!cnt)
            return;

        ctrl->_blendtrees = (BlendTree*)A_ALLOC(alloc, sizeof(BlendTree)*cnt, MID_ANIM);
        ASSERT(ctrl->_blendtrees);
        memset(ctrl->_blendtrees, 0x00, sizeof(BlendTree)*cnt);

        for (int i = 0; i < cnt; i++)    {
            JNode jbt = jblendtrees.array_item(i);
            BlendTree *bt = &ctrl->_blendtrees[i];
            str_safecpy(bt->name, sizeof(bt->name), jbt.child_str("name"));

            bt->param = jbt.child_int("param", -1);

            // Children
            JNode jchilds = jbt.child("childs");
            if (jchilds.is_valid())    {
                int child_cnt = jchilds.array_item_count();
                if (child_cnt)  {
                    bt->childs = (Sequence*)A_ALLOC(alloc, sizeof(Sequence)*child_cnt, MID_ANIM);
                    ASSERT(bt->childs);

                    for (int k = 0; k < child_cnt; k++)  {
                        JNode jseq = jchilds.array_item(k);
                        Sequence *seq = &bt->childs[k];
                        seq->index = jseq.child_int("id", -1);
                        seq->type = jloader_parse_seqtype(jseq);

                        bt->child_cnt ++;
                    }
                    bt->child_cnt_f = (float)bt->child_cnt;
                }
            }

            ctrl->_blendtree_cnt++;
        }
    }


    static void jloader_load_states(CharController *ctrl, JNode jstates, Allocator *alloc)
    {
        if (!jstates.is_valid())
            return;

        int cnt = jstates.array_item_count();
        if (cnt == 0)
            return;
        ctrl->_states = (State*)A_ALLOC(alloc, sizeof(State)*cnt, MID_ANIM);
        ASSERT(ctrl->_states);
        memset(ctrl->_states, 0x00, sizeof(State)*cnt);

        for (int i = 0; i < cnt; i++)    {
            JNode jstate = jstates.array_item(i);
            State *state = &ctrl->_states[i];

            str_safecpy(state->name, sizeof(state->name), jstate.child_str("name"));
            state->speed = jstate.child_float("speed", 1.0f);

            // Sequence
            JNode jseq = jstate.child("sequence");
            if (jseq.is_valid())   {
                state->seq.type = jloader_parse_seqtype(jseq);
                state->seq.index = jseq.child_int("id", -1);
            }

            // Transitions
            JNode jtrans = jstate.child("transitions");
            if (jtrans.is_valid()) {
                state->transition_cnt = jtrans.array_item_count();
                if (state->transition_cnt)   {
                    state->transitions = (int*)A_ALLOC(alloc, sizeof(int)*state->transition_cnt, 0);
                    ASSERT(state->transitions);
                    for (int k = 0; k < state->transition_cnt; k++)
                        state->transitions[k] = jtrans.array_item(k).to_int();
                }
            }

            ctrl->_state_cnt ++;
        }
    }

    static void jloader_load_layers(CharController *ctrl, JNode jlayers, Allocator *alloc)
    {
        if (!jlayers.is_valid())
            return;

        int cnt = jlayers.array_item_count();
        if (cnt == 0)
            return;
        ctrl->_layers = (Layer*)A_ALLOC(alloc, sizeof(Layer)*cnt, MID_ANIM);
        ASSERT(ctrl->_layers);
        memset(ctrl->_layers, 0x00, sizeof(Layer)*cnt);

        for (int i = 0; i < cnt; i++)    {
            JNode jlayer = jlayers.array_item(i);
            Layer *layer = &ctrl->_layers[i];

            str_safecpy(layer->name, sizeof(layer->name), jlayer.child_str("name"));
            layer->default_state = jlayer.child_int("default", -1);
            layer->type = jloader_parse_layertype(jlayer);

            // States
            JNode jstates = jlayer.child("states");
            if (jstates.is_valid())    {
                layer->state_cnt = jstates.array_item_count();
                if (layer->state_cnt)  {
                    layer->states = (int*)A_ALLOC(alloc, sizeof(uint)*layer->state_cnt, 0);
                    ASSERT(layer->states);
                    for (int k = 0; k < layer->state_cnt; k++)
                        layer->states[k] = jstates.array_item(k).to_int();
                }
            }

            // Bone mask
            JNode jbonemask = jlayer.child("bone-mask");
            if (jbonemask.is_valid())  {
                layer->bone_mask_cnt = jbonemask.array_item_count();
                if (layer->bone_mask_cnt)  {
                    layer->bone_mask = (char*)A_ALLOC(alloc, 32*layer->bone_mask_cnt, 0);
                    ASSERT(layer->bone_mask);
                    for (int k = 0; k < layer->bone_mask_cnt; k++)
                        str_safecpy(layer->bone_mask+32*k, 32, jbonemask.array_item(k).to_str());
                }
            }

            ctrl->_layer_cnt++;
        }
    }

public:
    static CharController* load_json(const char *json_filepath, Allocator *alloc, uint thread_id)
    {
        static const char *err_fmt = "Loading animation controller '%s' failed: %s";

        Allocator *tmp_alloc = tsk_get_tmpalloc(thread_id);
        A_PUSH(tmp_alloc);

        // Load JSON
        File f = File::open_mem(json_filepath, tmp_alloc, MID_ANIM);
        if (!f.is_open())   {
            err_printf(__FILE__, __LINE__, err_fmt, json_filepath, "File not found");
            A_POP(tmp_alloc);
            return nullptr;
        }
    
        JNode jroot = JNode(json_parsefilef(f, tmp_alloc));
        f.close();

        if (!jroot.is_valid())  {
            err_printf(__FILE__, __LINE__, err_fmt, json_filepath, "Invalid JSON format");
            A_POP(tmp_alloc);
            return nullptr;
        }

        // Calculate the total size of memory stack, so we can allocate all at once
        StackAlloc stack_mem;
        Allocator stack_alloc;

        int total_tgroups = jloader_getcount_2nd(jroot, "transitions", "groups");
        int total_tgroupitems = jloader_getcount_3rd(jroot, "transitions", "groups", "conditions");
        int total_seqs = jloader_getcount_2nd(jroot, "blendtrees", "childs");
        int total_idxs = jloader_getcount_2nd(jroot, "states", "transitions") +
                         jloader_getcount_2nd(jroot, "layers", "states");
        int total_bonemasks = jloader_getcount_2nd(jroot, "layers", "bone-mask");
        int param_cnt = jloader_getcount(jroot, "params");
        size_t total_sz =
            sizeof(CharController) +
            HashtableFixed<int, -1>::estimate_size(param_cnt) +
            param_cnt*sizeof(CharController) +
            jloader_getcount(jroot, "clips")*sizeof(Clip) +
            jloader_getcount(jroot, "transitions")*sizeof(Transition) +
            jloader_getcount(jroot, "blendtrees")*sizeof(BlendTree) +
            jloader_getcount(jroot, "layers")*sizeof(Layer) +
            jloader_getcount(jroot, "states")*sizeof(State) +
            total_tgroups*sizeof(ConditionGroup) +
            total_tgroupitems*sizeof(Condition) +
            total_seqs*sizeof(Sequence) +
            total_idxs*sizeof(int) +
            total_bonemasks*32;
        if (IS_FAIL(stack_mem.create(total_sz, alloc, MID_GFX)))    {
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            A_POP(tmp_alloc);
            return nullptr;
        }
        stack_mem.bindto(&stack_alloc);

        // Create 
        CharController *ctrl = mem_new_alloc<CharController>(&stack_alloc);
        ASSERT(ctrl);
        ctrl->_alloc = alloc;

        // Load Animation Reel
        const char *reel_filepath = jroot.child_str("reel");
        if (str_isempty(reel_filepath))  {
            err_printf(__FILE__, __LINE__, err_fmt, json_filepath, "Could not load AnimReel");
            ctrl->destroy();
            A_POP(tmp_alloc);
            return nullptr;
        }
        str_safecpy(ctrl->_reel_filepath, sizeof(ctrl->_reel_filepath), reel_filepath);

        jloader_load_params(ctrl, json_getitem(jroot, "params"), &stack_alloc);
        jloader_load_clips(ctrl, json_getitem(jroot, "clips"), &stack_alloc);
        jloader_load_transitions(ctrl, json_getitem(jroot, "transitions"), &stack_alloc);
        jloader_load_blendtrees(ctrl, json_getitem(jroot, "blendtrees"), &stack_alloc);
        jloader_load_states(ctrl, json_getitem(jroot, "states"), &stack_alloc);
        jloader_load_layers(ctrl, json_getitem(jroot, "layers"), &stack_alloc);

        jroot.destroy();
        A_POP(tmp_alloc);

        return ctrl;
    }
};

// CharControllerInst
class CharControllerInst : public animCharController
{
public:
    typedef const Mat3* (*FnLayerBlend)(Mat3* result, Mat3* src, Mat3* dest, float mask);

    struct LayerInst
    {
        int state;
        int transition;
        FnLayerBlend blend_fn;
        uint8 *buff;
        Reel::Pose *poses;
        float *bone_mask;
    };

    struct ClipInst
    {
        float start_tm; // Global start time
        float tm;       // Local time
        float progress; // Normalized progress (*N-times if looped)
        float duration;
        bool looped;
        int rclip;      // Index to clip
    };

    struct BlendTreeInst
    {
        int seq_a;
        int seq_b;
        float blend;
        float progress;
    };

    struct TransitionInst
    {
        float start_tm;
        float blend;
    };

public:
    Allocator *_alloc = nullptr;
    CharController *_ctrl = nullptr;
    reshandle_t _reel_hdl = INVALID_HANDLE;
    float _tm = 0.0f;   // Global time
    float _playrate = 1.0f;
    int layer_cnt = 0;

    animParam *_params = nullptr;
    LayerInst *_layers = nullptr;
    ClipInst *_clips = nullptr;
    BlendTreeInst *_blendtrees = nullptr;
    TransitionInst *_transitions = nullptr;

public:
    CharControllerInst() = default;

public:
    // Inherited from animCharControllerInst
    void debug();
    void destroy();

    animParam param(const char *name) const;
    void set_param(const char *name, const animParam param);

    reshandle_t reel() const;
    void set_reel(reshandle_t reel_hdl);

    void update(float tm, Allocator *tmp_alloc = mem_heap());

    bool debug_state(const char *layer, char *state, size_t state_sz, float *progress);
    bool debug_transition(const char *layer,
                          char *state_A, size_t state_A_sz,
                          char *state_B, size_t state_B_sz,
                          float *progress = nullptr);

private:
    bool test_predicate(CharController::Predicate pred, float value1, float value2)
    {
        switch (pred)   {
        case CharController::Predicate::EQUAL:
            return math_isequal(value1, value2);
        case CharController::Predicate::GREATER:
            return value1 > (value2 + EPSILON);
        case CharController::Predicate::LESS:
            return value1 < (value2 - EPSILON);
        case CharController::Predicate::NOT:
            return !math_isequal(value1, value2);
        default:
            return false;
        }
    }

    bool test_predicate(CharController::Predicate pred, int value1, int value2)
    {
        switch (pred)   {
        case CharController::Predicate::EQUAL:
            return value1 == value2;
        case CharController::Predicate::GREATER:
            return value1 > value2;
        case CharController::Predicate::LESS:
            return value1 < value2 - EPSILON;
        case CharController::Predicate::NOT:
            return value1 != value2;
        default:
            return false;
        }
    }

    bool test_predicate(bool value1, bool value2)
    {
        return value1 == value2;
    }

    result_t bindto(Reel *reel)
    {
        const CharController *ctrl = inst->_ctrl;

        // Setup clip instances
        for (int i = 0; i < ctrl->_clip_cnt; i++)   {
            const CharController::Clip *cclip = &ctrl->_clips[i];
            ClipInst *iclip = &inst->_clips[i];

            iclip->rclip = reel->find_clip_hashed(cclip->name_hash);
            if (iclip->rclip == -1)
                continue;

            Reel::Clip *rclip = &reel->_clips[iclip->rclip];
            iclip->duration = rclip->duration;
            iclip->looped = rclip->looped;
        }

        // Setup layers
        for (int i = 0; i < ctrl->_layer_cnt; i++)    {
            LayerInst *ilayer = &inst->_layers[i];
            if (ilayer->buff)
                A_ALIGNED_FREE(inst->alloc, ilayer->buff);
            size_t sz = (sizeof(Reel::Pose) + sizeof(float)) * reel->_pose_cnt;
            uint8 *buff = (uint8*)A_ALIGNED_ALLOC(inst->alloc, sz, MID_ANIM);
            if (buff == nullptr)
                return RET_OUTOFMEMORY;
            memset(buff, 0x00, sz);

            ilayer->buff = buff;
            ilayer->poses = (Reel::Pose*)buff;
            buff += sizeof(Reel::Pose)*reel->_pose_cnt;

            // Bone-Mask: It's an array of weights for each joint
            ilayer->bone_mask = (float*)buff;
            CharController::Layer *layer = &ctrl->_layers[i];

            if (i || layer->bone_mask_cnt) {
                for (int k = 0; k < layer->bone_mask_cnt; k++)   {
                    const char *mask_name = layer->bone_mask + k*32;

                    // Find mask name in bindings
                    int pose_idx = reel->find_pose_binding(mask_name);
                    if (pose_idx != -1)
                        ilayer->bone_mask[pose_idx] = 1.0f;
                }
            }   else    {
                // First layer always applies to full skeleton
                for (int k = 0; k < reel->_pose_cnt; k++)
                    ilayer->bone_mask[k] = 1.0f;
            }
        }

        return RET_OK;
    }

    static CharControllerInst* create(const CharController *ctrl, Allocator *alloc)
    {
        static const char *err_fmt = "Creating CharController instance failed: %s";

        // Calculate total bytes to create in one allocation
        size_t bytes =
            sizeof(struct CharControllerInst) +
            ctrl->_param_cnt*sizeof(animParam) +
            ctrl->_blendtree_cnt*sizeof(BlendTreeInst) +
            ctrl->_clip_cnt*sizeof(ClipInst) +
            ctrl->_layer_cnt*sizeof(LayerInst) +
            ctrl->_transition_cnt*sizeof(TransitionInst);

        StackAlloc stack_mem;
        Allocator stack_alloc;
        if (IS_FAIL(stack_mem.create(bytes, alloc, MID_ANIM)))   {
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            return nullptr;
        }
        stack_mem.bindto(&stack_alloc);

        CharControllerInst *inst = mem_new_alloc<CharControllerInst>(&stack_alloc);
        inst->_alloc = alloc;
        inst->_ctrl = ctrl;

        // Load animation reel
        inst->_reel_hdl = rs_load_animreel(ctrl->_reel_filepath, 0);
        if (inst->_reel_hdl == INVALID_HANDLE)   {
            err_printf(__FILE__, __LINE__, err_fmt, "Could not load animation reel");
            stack_mem.destroy();
            return nullptr;
        }

        if (ctrl->_param_cnt)    {
            inst->_params = (animParam*)A_ALLOC(&stack_alloc, sizeof(animParam)*ctrl->_param_cnt, 0);
            for (int i = 0; i < ctrl->_param_cnt; i++)
                inst->params[i] = ctrl->_params[i].value;
        }

        if (ctrl->_layer_cnt)    {
            inst->_layers = (LayerInst*)A_ALLOC(&stack_alloc, sizeof(LayerInst)*ctrl->_layer_cnt, 0);
            memset(inst->_layers, 0x00, sizeof(LayerInst)*ctrl->_layer_cnt);
            for (int i = 0; i < ctrl->_layer_cnt; i++)    {
                LayerInst *layer = &inst->_layers[i];
                layer->state = -1;
                layer->transition = -1;
                switch (ctrl->layers[i].type)   {
                case CharController::LayerType::OVERRIDE:
                    layer->blend_fn = layer_override;
                    break;
                case CharController::LayerType::ADDITIVE:
                    layer->blend_fn = layer_additive;
                    break;
                }
            }
            inst->_layer_cnt = ctrl->_layer_cnt;
        }

        if (ctrl->_clip_cnt) {
            inst->_clips = (ClipInst*)A_ALLOC(&stack_alloc, sizeof(ClipInst)*ctrl->_clip_cnt, 0);
            memset(inst->_clips, 0x00, sizeof(ClipInst)*ctrl->_clip_cnt);
        }

        if (ctrl->_blendtree_cnt)    {
            inst->_blendtrees = (BlendTreeInst*)A_ALLOC(&stack_alloc,
                                                        sizeof(BlendTreeInst)*ctrl->_blendtree_cnt, 0);
            memset(inst->_blendtrees, 0x00, sizeof(BlendTreeInst)*ctrl->_blendtree_cnt);
            for (int i = 0; i < ctrl->_blendtree_cnt; i++)    {
                BlendTreeInst *bt = &inst->blendtrees[i];
                bt->seq_a = -1;
                bt->seq_b = -1;
            }
        }

        if (ctrl->_transition_cnt)   {
            ctrl->_transitions = (TransitionInst*)A_ALLOC(&stack_alloc,
                                                          sizeof(TransitionInst)*ctrl->_transition_cnt,
                                                          0);
            memset(inst->_transitions, 0x00, sizeof(TransitionInst)*ctrl->_transition_cnt);
        }

        Reel *reel = rs_get_animreel(inst->_reel_hdl);
        if (reel == nullptr)
            return inst;

        if (IS_FAIL(inst->bindto(reel)))    {
            inst->destroy();
            err_printf(__FILE__, __LINE__, err_fmt, "Could not bind to Reel");
            return nullptr;
        }

        return inst;
    }

    // Updates
    float progress_state(int state_idx)
    {
        const CharController::Sequence &seq = _ctrl->_states[state_idx].seq;

        if (seq.type == CharController::SequenceType::CLIP)
            return tmin<float>(inst->clips[seq.index].progress, 1.0f);
        else if (seq->type == CharController::SequenceType::BLEND_TREE)
            return tmin<float>(inst->blendtrees[seq.index].progress, 1.0f);
        return 0.0f;
    }

    bool check_condition_group(int state_idx, const CharController::ConditionGroup &tgroup)
    {
        bool condition = true;
        for (int i = 0; i < tgroup.count && condition; i++)   {
            const CharController::Condition &item = tgroup.conditions[i];

            if (item.type == CharController::ConditionType::EXIT)    {
                float k = progress_state(state_idx);
                condition &= test_predicate(item->predicate, k, (float)item.value);
            }    else if (item->type == CharController::ConditionType::PARAM) {
                const animParam &iparam = _params[item.param];
                switch (iparam.type())    {
                case animParam::Type::BOOL:
                    condition &= test_predicate((bool)iparam, (bool)item.value);
                    break;
                case animParam::Type::FLOAT:
                    condition &= test_predicate(item.predicate, (float)iparam, (float)item.value);
                    break;
                case animParam::Type::INT:
                    condition &= test_predicate(item->predicate, (int)iparam, (int)item.value);
                    break;
                default:
                    break;
                }
            }
        }
        return condition;
    }

    void start_clip(int clip_idx, float start_tm)
    {
        ClipInst *iclip = &_clips[clip_idx];
        iclip->start_tm = start_tm;
        iclip->progress = 0.0f;
    }

    void start_blendtree(int blendtree_idx, float start_tm)
    {
        BlendTreeInst *ibt = &_blendtrees[blendtree_idx];
        ibt->seq_a = -1;
        ibt->seq_b = -1;
        ibt->progress = 0.0f;

        // Recurse for child nodes
        const CharController::BlendTree &bt = _ctrl->_blendtrees[blendtree_idx];
        for (int i = 0, cnt = bt->child_cnt; i < cnt; i++)  {
            const CharController &seq = bt.childs[i];
            if (seq.type == CharController::SequenceType::CLIP)
                start_clip(seq.index, start_tm);
            else if (seq.type == CharController::SequenceType::BLEND_TREE)
                start_blendtree(seq.index, start_tm);
        }
    }

    void start_sequence(const CharController::Sequence &seq, float start_tm)
    {
        if (seq.type == CharController::SequenceType::CLIP)
            start_clip(seq.index, start_tm);
        else if (seq.type == CharController::SequenceType::BLEND_TREE)
            start_blendtree(seq.index, start_tm);
    }

    void start_state(int state_idx, float start_tm)
    {
        start_sequence(_ctrl->_states[state_idx].seq, start_tm);
    }

    void start_transition(int layer_idx, int transition_idx, float tm)
    {
        const CharController::Transition &trans = _ctrl->_transitions[transition_idx];
        TransitionInst *itrans = &_transitions[transition_idx];
        LayerInst *ilayer = &_layers[layer_idx];

        ilayer->state = -1;
        ilayer->transition = transition_idx;

        itrans->start_tm = tm;
        itrans->blend = 0.0;

        start_state(trans.target_state, tm);
    }

    bool check_state(int layer_idx, int state_idx, float tm)
    {
        CharController *ctrl = _ctrl;
        const CharController::State &cstate = ctrl->_states[state_idx];
        bool condition_meet = false;

        for (int i = 0, cnt = cstate.transition_cnt; i < cnt; i++)  {
            const CharController::Transition &trans = ctrl->_transitions[cstate.transitions[i]];
            for (int k = 0; k < trans.group_cnt; k++)
                condition_meet |= check_condition_group(state_idx, layer_idx, trans.groups[k]);

            if (condition_meet) {
                int trans_idx = cstate.transitions[i];
                if (_layers[layer_idx].transition != trans_idx)
                    start_transition(layer_idx, cstate.transitions[i], tm);
                break;
            }
        }

        return condition_meet;
    }

    void blend_pose(Reel::Pose *poses, const Reel::Pose *poses_a, const Reel::Pose *poses_b,
                    int pose_cnt, float blend)
    {
        for (int i = 0; i < pose_cnt; i++)   {
            poses[i].pos_scale = Vec4::lerp(poses_a[i].pos_scale, poses_b[i].pos_scale, blend);
            poses[i].rot = Quat::slerp(poses_a[i].rot, poses_b[i].rot, blend);
        }
    }

    void calculate_pose(Reel::Pose *poses, const Reel *reel, int clip_idx, float tm)
    {
        const Reel::Clip &clip = reel->_clips[clip_idx];
        float ft = reel->ft;

        int frame_cnt = clip.frame_end - clip.frame_start;
        int frame_idx = tclamp<float>((int)(tm/ft), 0, frame_cnt - 1);
        int frame_next_idx = (frame_idx + 1) % frame_cnt;

        float interpolate = (tm - (frame_idx*ft)) / ft;

        const Reel::Channel &frame_a = reel->_channels[frame_idx + clip.frame_start];
        const Reel::Channel &frame_b = reel->_channels[frame_next_idx + clip.frame_start];

        blend_pose(poses, frame_a.poses, frame_b.poses, reel->_pose_cnt, interpolate);
    }

    // Returns progress
    float update_clip(Reel::Pose *poses, const Reel *reel, int clip_idx, float tm, float playrate)
    {
        const ClipInst *iclip = &_clips[clip_idx];

        float tm_raw = playrate * (tm - iclip->start_tm);
        float progress = tm_raw/iclip->duration;

        // Local clip time
        if (iclip->looped)   {
            iclip->tm = fmodf(tm_raw, iclip->duration);
            iclip->progress = progress;
        }   else    {
            iclip->tm = tclamp<float>(tm_raw, 0.0f, iclip->duration);
            iclip->progress = tclamp<float>(progress, 0.0f, 1.0f);
        }

        // Interpolate and calculate pose
        calculate_pose(poses, reel, iclip->rclip_idx, iclip->tm);

        return iclip->progress;
    }

    float update_sequence(Reel::Pose *poses, const Reel *reel,
                          const CharController::Sequence &seq, float tm, float playrate,
                          Allocator *tmp_alloc);

    // Returns progress
    float update_blendtree(Reel::Pose *poses, const Reel *reel, int blendtree_idx,
                           float tm, float playrate, Allocator *tmp_alloc)
    {
        const CharController::BlendTree &bt = _ctrl->_blendtrees[blendtree_idx];
        BlendTreeInst *ibt = &_blendtrees[blendtree_idx];

        animParam *iparam = &_params[bt.param];
        ASSERT(iparam->type() == animParam::Type::FLOAT);

        float f = tclamp<float>(*iparam, 0.0f, 1.0f); // Make it normalized

        // Detect which children we have to blend
        float progress = f*(bt.child_cnt_f - 1.0f);
        float idx_f = floorf(progress);
        float blend = progress - idx_f;

        int idx = (int)idx_f;
        int idx2 = tmin<int>(idx + 1, bt.child_cnt - 1);

        // Update instance
        ibt->seq_a = idx;
        ibt->seq_b = idx2;
        ibt->blend = blend;

        // Calculate
        if (idx != idx2)    {
            const CharController::Sequence &seq_a = bt.childs[idx];
            const CharController::Sequence &seq_b = bt.childs[idx2];

            int pose_cnt = reel->_pose_cnt;
            Reel::Pose *poses_a = (Reel::Pose*)A_ALIGNED_ALLOC(tmp_alloc,
                                                               sizeof(Reel::Pose)*pose_cnt, 0);
            Reel::Pose *poses_b = (Reel::Pose*)A_ALIGNED_ALLOC(tmp_alloc,
                                                               sizeof(struct anim_pose)*pose_cnt, 0);
            ASSERT(poses_a);
            ASSERT(poses_b);

            float progress_a = update_sequence(poses_a, reel, seq_a, tm, playrate, tmp_alloc);
            float progress_b = update_sequence(poses_b, reel, seq_b, tm, playrate, tmp_alloc);

            // Blend two sequences
            blend_pose(poses, poses_a, poses_b, pose_cnt, blend);

            A_ALIGNED_FREE(tmp_alloc, poses_a);
            A_ALIGNED_FREE(tmp_alloc, poses_b);

            progress = (1.0f - blend)*progress_a + blend*progress_b;
        }    else   {
            const CharController::Sequence &seq = bt.childs[idx];
            progress = update_sequence(poses, reel, seq, tm, playrate, tmp_alloc);
        }

        return progress;
    }

    // Returns progress
    float update_sequence(Reel::Pose *poses, const Reel *reel,
                          const CharController::Sequence &seq, float tm, float playrate,
                          Allocator *tmp_alloc)
    {
        if (seq.type == CharController::SequenceType::CLIP)
            return update_clip(poses, reel, seq.index, tm, playrate);
        else if (seq->type == CharController::SequenceType::BLEND_TREE)
            return update_blendtree(poses, reel, seq.index, tm, playrate, tmp_alloc);
        return 0.0f;
    }

    void update_state(Reel::Pose *poses, const Reel *reel, int state_idx, float tm,
                      Allocator *tmp_alloc)
    {
        const CharController::State &cstate = _ctrl->_states[state_idx];

        float progress = update_sequence(poses, reel, cstate.seq, tm, _playrate, tmp_alloc);

        /* only update progress for blendtrees because they are recursive */
        if (cstate.seq.type == CharController::SequenceType::BLEND_TREE)
            _blendtrees[cstate.seq.index].progress = progress;
    }


    void update(float tm, Allocator *tmp_alloc)
    {
        const CharController *ctrl = _ctrl;
        const Reel *reel = rs_get_animreel(inst->reel_hdl);
        if (reel == nullptr)
            return;

        for (int i = 0, cnt = ctrl->_layer_cnt; i < cnt; i++) {
            LayerInst *ilayer = &inst->layers[i];

            // Update state
            Reel::Pose *rposes = ilayer->poses;
            if (ilayer->state != -1)   {
                if (check_state(reel, i, ilayer->state, tm))  {
                    update(inst, tm, tmp_alloc);
                    return;
                }
                anim_ctrl_updatestate(rposes, ctrl, inst, reel, i, ilayer->state, tm, tmp_alloc);
            }   else if (ilayer->transition != -1) {
                anim_ctrl_updatetransition(rposes, ctrl, inst, reel, i, ilayer->transition, tm,
                    tmp_alloc);
            }   else    {
                /* we have no state, go to default state */
                ilayer->state = ctrl->layers[i].default_state_idx;
                ilayer->transition = -1;
                start_state(ctrl, inst, ilayer->state, tm);
                anim_ctrl_updatestate(rposes, ctrl, inst, reel, i, ilayer->state, tm, tmp_alloc);
            }
        }

        inst->tm = tm;
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

            blend_pose(poses, poses_a, poses_b, pose_cnt, blend);
        }
    }
};

/* animation controller */
static void start_state(const anim_ctrl ctrl, anim_ctrl_inst inst, uint state_idx,
                          float start_tm);
static int anim_ctrl_checkstate(const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
                            uint layer_idx, uint state_idx, float tm);
static void anim_ctrl_updatetransition(struct anim_pose* poses,
                                const anim_ctrl ctrl, anim_ctrl_inst inst,
                                const anim_reel reel, uint layer_idx, uint transition_idx,
                                float tm, struct allocator* tmp_alloc);
static void start_state(const anim_ctrl ctrl, anim_ctrl_inst inst, uint state_idx,
                          float start_tm);
static int check_condition_group(const anim_ctrl ctrl, anim_ctrl_inst inst,
                             const anim_reel reel, uint state_idx, uint layer_idx,
                             const struct anim_ctrl_transition_group* tgroup, float tm);
static void anim_ctrl_updatestate(struct anim_pose* poses, const anim_ctrl ctrl, anim_ctrl_inst inst,
                           const anim_reel reel, uint layer_idx, uint state_idx, float tm,
                           struct allocator* tmp_alloc);
static void start_transition(const anim_ctrl ctrl, anim_ctrl_inst inst,
                               const anim_reel reel, uint layer_idx, uint transition_idx,
                               float tm);
static float progress_state(const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
                              uint state_idx);
static float update_sequence(struct anim_pose* poses,
                         const anim_ctrl ctrl, anim_ctrl_inst inst, const anim_reel reel,
                         const struct anim_ctrl_sequence* seq, float tm, float playrate,
                         struct allocator* tmp_alloc);
static void calculate_pose(struct anim_pose* poses, const anim_reel reel, uint clip_idx, float tm);
static void blend_pose(struct anim_pose* poses, const struct anim_pose* poses_a,
                         const struct anim_pose* poses_b, uint pose_cnt, float blend);
static void start_sequence(const anim_ctrl ctrl, anim_ctrl_inst inst,
                        const struct anim_ctrl_sequence* seq, float start_tm);
static void start_clip(const anim_ctrl ctrl, anim_ctrl_inst inst, uint clip_idx, float start_tm);
static void start_blendtree(const anim_ctrl ctrl, anim_ctrl_inst inst, uint blendtree_idx,
                              float start_tm);
static int check_condition_group(const anim_ctrl ctrl, anim_ctrl_inst inst,
                             const anim_reel reel, uint state_idx, uint layer_idx,
                             const struct anim_ctrl_transition_group* tgroup, float tm);
static float update_clip(struct anim_pose* poses, const anim_ctrl ctrl,
                          anim_ctrl_inst inst, const anim_reel reel, uint clip_idx, float tm,
                          float playrate);
static float update_blendtree(struct anim_pose* poses,
                               const anim_ctrl ctrl, anim_ctrl_inst inst,
                               const anim_reel reel, uint blendtree_idx, float tm,
                               float playrate, struct allocator* tmp_alloc);


/*************************************************************************************************/
/* note: time (tm) parameter should be global and handled by an external global timer */

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
