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

#ifndef _MAX_INSTANCES_
#error "_MAX_INSTANCES_ not defined"
#endif

/* input */
layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_NORMAL) in vec3 vsi_norm;
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord0;

#if defined(_NORMALMAP_)
layout(location = INPUT_ID_TANGENT) in vec3 vsi_tangent;
layout(location = INPUT_ID_BINORMAL) in vec3 vsi_binorm;
#endif

#if defined(_SKIN_)
layout(location = INPUT_ID_BLENDINDEX) in ivec4 vsi_blend_idxs;
layout(location = INPUT_ID_BLENDWEIGHT) in vec4 vsi_blend_weights;
#endif

/* output */
out vec2 vso_coord;
out vec3 vso_norm_vs;

#if defined(_NORMALMAP_)
out vec3 vso_tangent_vs;
out vec3 vso_binorm_vs;
#endif

layout(std140) uniform cb_frame
{
    mat3x4 c_view;
    mat4 c_viewproj;
};

layout(std140) uniform cb_xforms
{
    mat3x4 c_mats[_MAX_INSTANCES_];
};

void main() 
{
    /* skinning */
#if defined(_SKIN_)
    #if defined(_NORMALMAP_)
        skin_output_pnt s = skin_vertex_pnt(gl_InstanceID, vsi_blend_idxs, vsi_blend_weights, 
			vsi_pos, vsi_norm, vsi_tangent, vsi_binorm);    
        vec4 pos = s.pos;
        vec3 norm = s.norm;
        vec3 tangent = s.tangent;
        vec3 binorm = s.binorm;
    #else
        skin_output_pn s = skin_vertex_pn(gl_InstanceID, vsi_blend_idxs, vsi_blend_weights, vsi_pos, 
			vsi_norm);
        vec4 pos = s.pos;
        vec3 norm = s.norm;
    #endif
#else
    vec4 pos = vsi_pos;
    vec3 norm = vsi_norm;
    #if defined(_NORMALMAP_)
        vec3 tangent = vsi_tangent;
        vec3 binorm = vsi_binorm;
    #endif
#endif
    mat3 view3 = mat3(c_view);
    mat3 m3 = mat3(c_mats[gl_InstanceID]);

    /* position */
    vec4 pos_ws = vec4(pos * c_mats[gl_InstanceID], 1.0f);
    gl_Position = pos_ws * c_viewproj;

    /* normal */
    vec3 norm_ws = norm * m3;
    vso_norm_vs = norm_ws * view3;

	/* tangents */
#if defined(_NORMALMAP_)
    vec3 tangent_ws = tangent * m3;
    vso_tangent_vs = tangent_ws * view3;

    vec3 binorm_ws = binorm * m3;
    vso_binorm_vs = binorm_ws * view3;
#endif
    
    /* tex-coord */
    vso_coord = vec2(vsi_coord0.x, vsi_coord0.y);
}

