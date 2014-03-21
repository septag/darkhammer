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

#include <stdio.h>

#include "dhcore/core.h"
#include "dhcore/json.h"

#include "dheng/h3d-types.h"

#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/config.h"
#include "assimp/anim.h"

#include "model-import.h"
#include "math-conv.h"

#define DEFAULT_FPS 30

/*************************************************************************************************
 * types
 */
struct anim_channel_ext
{
    struct h3d_anim_channel c;
    struct vec4f* pos_scale;    /* w = uniform scale */
    struct quat4f* rot;
};

struct anim_ext
{
    struct h3d_anim a;
    struct anim_channel_ext* channels;
    struct h3d_anim_clip* clips;
};

/*************************************************************************************************
 * fwd declarations
 */
bool_t import_writeanim(const char* filepath, const struct anim_ext* anim);
struct h3d_anim_clip* import_loadclips(const char* json_filepath, uint frame_cnt,
    OUT uint* clip_cnt);
struct h3d_anim_clip* import_defaultclip(uint frame_cnt);

/*************************************************************************************************
 * Globals
 */
enum coord_type g_anim_coord;


/*************************************************************************************************/
bool_t import_anim(const struct import_params* params)
{
    uint flags = 0;
    if (params->coord == COORD_NONE)
        flags |= aiProcess_MakeLeftHanded;

    const struct aiScene* scene = aiImportFileEx(params->in_filepath, flags, NULL);

    if (scene == NULL)  {
        printf(TERM_BOLDRED "Error: (assimp) %s\n" TERM_RESET, aiGetErrorString());
        return FALSE;
    }

    if (scene->mNumAnimations == 0) {
        printf(TERM_BOLDRED "Error: no animation exist in the file '%s'" TERM_RESET,
            params->in_filepath);
        return FALSE;
    }

    g_anim_coord = params->coord;
    uint fps = params->anim_fps != 0 ? params->anim_fps : DEFAULT_FPS;
    uint channel_cnt = 0;
    uint frame_cnt = 0;
    bool_t has_scale = FALSE;

    /* channel count is the sum of all animation channels */
    /* frame_cnt is the maximum of all animation channel key counts */
    for (uint i = 0; i < scene->mNumAnimations; i++)  {
        const struct aiAnimation* anim = scene->mAnimations[i];
        channel_cnt += anim->mNumChannels;
        for (uint k = 0; k < anim->mNumChannels; k++) {
            frame_cnt = maxun(frame_cnt, maxun(anim->mChannels[k]->mNumPositionKeys,
                maxun(anim->mChannels[k]->mNumRotationKeys, anim->mChannels[k]->mNumScalingKeys)));
        }
    }

    if (channel_cnt == 0)   {
        printf(TERM_BOLDRED "Error: no animation channels exist in the file '%s'" TERM_RESET,
            params->in_filepath);
        return FALSE;
    }

    /* parse my data from assimp data */
    struct anim_ext h3danim;
    memset(&h3danim, 0x00, sizeof(h3danim));

    /* channels */
    struct anim_channel_ext* h3dchannels = (struct anim_channel_ext*)
        ALLOC(sizeof(struct anim_channel_ext)*channel_cnt, 0);
    ASSERT(h3dchannels != NULL);
    memset(h3dchannels, 0x00, sizeof(struct anim_channel_ext)*channel_cnt);

    uint channel_offset = 0;
    for (uint i = 0; i < scene->mNumAnimations; i++)  {
        struct aiAnimation* anim = scene->mAnimations[i];

        for (uint k = 0; k < anim->mNumChannels; k++) {
            struct aiNodeAnim* channel = anim->mChannels[k];
            struct anim_channel_ext* h3dchannel = &h3dchannels[k + channel_offset];

            str_safecpy(h3dchannel->c.bindto, sizeof(h3dchannel->c.bindto), channel->mNodeName.data);
            h3dchannel->pos_scale = (struct vec4f*)ALLOC(sizeof(struct vec4f)*frame_cnt, 0);
            h3dchannel->rot = (struct quat4f*)ALLOC(sizeof(struct quat4f)*frame_cnt, 0);
            ASSERT(h3dchannel->pos_scale && h3dchannel->rot);

            struct vec3f pos;
            struct quat4f quat;
            float scale = 1.0f;

            /* fill channel data */
            for (uint f = 0; f < frame_cnt; f++)  {
                if (f < channel->mNumPositionKeys)  {
                    vec3_setf(&pos,
                        channel->mPositionKeys[f].mValue.x,
                        channel->mPositionKeys[f].mValue.y,
                        channel->mPositionKeys[f].mValue.z);
                }

                if (f < channel->mNumRotationKeys)  {
                    quat_setf(&quat,
                        channel->mRotationKeys[f].mValue.x,
                        channel->mRotationKeys[f].mValue.y,
                        channel->mRotationKeys[f].mValue.z,
                        channel->mRotationKeys[f].mValue.w);
                }

                if (f < channel->mNumScalingKeys)   {
                    scale = (channel->mScalingKeys[f].mValue.x +
                        channel->mScalingKeys[f].mValue.y +
                        channel->mScalingKeys[f].mValue.z) / 3.0f;
                    has_scale |= !math_isequal(scale, 1.0f);
                }

                import_convert_vec3(&pos, &pos, g_anim_coord);
                import_convert_quat(&quat, &quat, g_anim_coord);

                vec4_setf(&h3dchannel->pos_scale[f], pos.x, pos.y, pos.z, scale);
                quat_setf(&h3dchannel->rot[f], quat.x, quat.y, quat.z, quat.w);
            }
        }

        channel_offset += anim->mNumChannels;
    }

    /* write to file */
    h3danim.a.channel_cnt = channel_cnt;
    h3danim.a.frame_cnt = frame_cnt;
    h3danim.a.has_scale = has_scale;
    h3danim.a.fps = fps;
    h3danim.channels = h3dchannels;

    /* parse json clip file */
    h3danim.clips = import_loadclips(params->clips_json_filepath, frame_cnt, &h3danim.a.clip_cnt);

    /* write */
    bool_t r = import_writeanim(params->out_filepath, &h3danim);

    /* report */
    if (r)  {
        printf(TERM_BOLDGREEN "ok, saved: \"%s\".\n" TERM_RESET, params->out_filepath);

        if (params->verbose)    {
            printf(TERM_WHITE);
            printf("Animation report:\n"
                "  animation count: %d\n"
                "  frame count: %d\n"
                "  channel count: %d\n"
                "  has_scale: %s\n"
                "  fps: %d\n",
                scene->mNumAnimations,
                frame_cnt,
                channel_cnt,
                has_scale ? "yes" : "no",
                fps);
            printf(TERM_RESET);
        }
    }

    /* cleanup */
    for (uint i = 0; i < h3danim.a.channel_cnt; i++)    {
        if (h3danim.channels[i].pos_scale != NULL)
            FREE(h3danim.channels[i].pos_scale);
        if (h3danim.channels[i].rot != NULL)
            FREE(h3danim.channels[i].rot);
    }
    FREE(h3danim.clips);
    FREE(h3danim.channels);
    aiReleaseImport(scene);

    return r;
}

bool_t import_writeanim(const char* filepath, const struct anim_ext* anim)
{
    /* write to temp file and move it later */
    char filepath_tmp[DH_PATH_MAX];
    strcat(strcpy(filepath_tmp, filepath), ".tmp");
    FILE* f = fopen(filepath_tmp, "wb");
    if (f == NULL)  {
        printf(TERM_BOLDRED "Error: failed to open file '%s' for writing\n" TERM_RESET, filepath);
        return FALSE;
    }

    /* header */
    struct h3d_header header;
    header.sign = H3D_SIGN;
    header.type = H3D_ANIM;
    header.version = H3D_VERSION_11;
    header.data_offset = sizeof(struct h3d_header);
    fwrite(&header, sizeof(header), 1, f);

    /* anim descriptor */
    struct h3d_anim a;
    memcpy(&a, &anim->a, sizeof(struct h3d_anim));
    /* we will write this section later */
    fseek(f, sizeof(a), SEEK_CUR);

    /* channels */
    for (uint i = 0; i < anim->a.channel_cnt; i++)    {
        /* channel descriptor */
        fwrite(&anim->channels[i].c, sizeof(struct h3d_anim_channel), 1, f);

        /* channel data */
        fwrite(anim->channels[i].pos_scale, sizeof(struct vec4f), anim->a.frame_cnt, f);
        fwrite(anim->channels[i].rot, sizeof(struct quat4f), anim->a.frame_cnt, f);
    }

    a.clips_offset = ftell(f);

    /* clips */
    fwrite(anim->clips, sizeof(struct h3d_anim_clip), a.clip_cnt, f);

    /* write animation data header */
    fseek(f, header.data_offset, SEEK_SET);
    fwrite(&a, sizeof(a), 1, f);

    fclose(f);

    /* move back the file */
    return util_movefile(filepath, filepath_tmp);
}

struct h3d_anim_clip* import_loadclips(const char* json_filepath, uint frame_cnt,
    OUT uint* clip_cnt)
{
    /* return default clips, which is the whole animation */
    if (str_isempty(json_filepath)) {
        *clip_cnt = 1;
        return import_defaultclip(frame_cnt);
    }

    char* json_data = util_readtextfile(json_filepath, mem_heap());
    if (json_data == NULL)  {
        printf(TERM_BOLDYELLOW "Warning: could not open JSON file '%s' for clips,"
            " reseting to default", json_filepath);
        *clip_cnt = 1;
        return import_defaultclip(frame_cnt);
    }

    json_t jroot = json_parsestring(json_data);
    FREE(json_data);
    if (jroot == NULL)  {
        printf(TERM_BOLDYELLOW "Warning: could not read JSON file '%s' for clips,"
            " reseting to default", json_filepath);
        *clip_cnt = 1;
        return import_defaultclip(frame_cnt);
    }

    /* */
    json_t jclips = jroot;
    uint cnt = json_getarr_count(jclips);
    if (cnt == 0)   {
        printf(TERM_BOLDYELLOW "Warning: no clip defined in JSON file '%s',"
            " switching to default", json_filepath);
        *clip_cnt = 1;
        return import_defaultclip(frame_cnt);
    }

    struct h3d_anim_clip* clips = (struct h3d_anim_clip*)ALLOC(sizeof(struct h3d_anim_clip)*cnt, 0);
    ASSERT(clips);

    for (uint i = 0; i < cnt; i++)    {
        json_t jclip = json_getarr_item(jclips, i);

        strcpy(clips[i].name, json_gets_child(jclip, "name", "[noname]"));
        clips[i].start = minun(json_geti_child(jclip, "start", 0), frame_cnt-1);
        clips[i].end = minun(json_geti_child(jclip, "end", frame_cnt), frame_cnt);
        clips[i].looped = json_getb_child(jclip, "looped", FALSE);
    }

    json_destroy(jroot);

    *clip_cnt = cnt;
    return clips;
}

struct h3d_anim_clip* import_defaultclip(uint frame_cnt)
{
    struct h3d_anim_clip* clips = (struct h3d_anim_clip*)ALLOC(sizeof(struct h3d_anim_clip), 0);
    ASSERT(clips);

    memset(clips, 0x00, sizeof(struct h3d_anim_clip));
    strcpy(clips[0].name, "main");
    clips[0].start = 0;
    clips[0].end = frame_cnt;
    clips[0].looped = FALSE;

    return clips;
}
