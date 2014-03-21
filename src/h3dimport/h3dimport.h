/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
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


#ifndef H3DIMPORT_H_
#define H3DIMPORT_H_

#include "dhcore/types.h"
#include "dheng/h3d-types.h"

struct import_params_texture
{
	bool_t fast_mode;
	bool_t force_dxt3;
};

enum import_type
{
    IMPORT_UNKNOWN = 0,
    IMPORT_MODEL,
    IMPORT_ANIM,
    IMPORT_TEXTURE,
    IMPORT_PHX
};

enum coord_type
{
    COORD_NONE = 0,
    COORD_RH_ZUP,
    COORD_RH_GL
};

struct import_params
{
    enum import_type type;
	char name[32];
	char in_filepath[DH_PATH_MAX];
	char out_filepath[DH_PATH_MAX];
	char texture_dir[DH_PATH_MAX];
	char texture_dir_alias[DH_PATH_MAX];
    char clips_json_filepath[DH_PATH_MAX];
	bool_t verbose;
	bool_t calc_tangents;
    bool_t list;    /* list can work for models/physics files */
    bool_t list_mtls;
    bool_t toff;    /* texture-compression off */
    float scale;
    enum coord_type coord;
	struct import_params_texture tex_params;
    uint anim_fps;
    enum h3d_texture_type ttype;    /* texture type for texture only conversion */
    char occ_name[32];  /* empty if we have no occluder */
};

#endif /* H3DIMPORT_H_ */
