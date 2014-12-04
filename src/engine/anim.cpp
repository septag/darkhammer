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
