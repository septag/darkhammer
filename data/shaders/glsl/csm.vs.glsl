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

/* inputs */
layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_NORMAL) in vec3 vsi_norm;
#if defined(_ALPHAMAP_)
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord;
#endif
#if defined(_SKIN_)
layout(location = INPUT_ID_BLENDINDEX) in ivec4 vsi_blend_idxs;
layout(location = INPUT_ID_BLENDWEIGHT) in vec4 vsi_blend_weights;
#endif

/* outputs */
out vso {
    vec4 pos0;
    vec4 pos1;
    vec4 pos2;
#if defined(_ALPHAMAP_)
    vec2 coord;
#endif
} o;

layout(std140) uniform cb_frame
{
	vec4 c_texelsz;	/* x = texelsz */
	vec4 c_fovfactors;	/* max(proj.11, proj[1].22) for each projection matrix */
	vec4 c_lightdir;	
	mat3x4 c_views[_CASCADE_CNT_];
    mat4 c_cascade_mats[_CASCADE_CNT_];
};

layout(std140) uniform cb_xforms
{
    mat3x4 c_mats[_MAX_INSTANCES_];
};

vec4 apply_bias(vec4 pos_ws, vec3 norm_ws, mat3x4 view, float fovfactor)
{
	vec3 lv = c_lightdir.xyz;
	vec4 pos_vs = vec4(pos_ws * view, 1);
	float texelsz = c_texelsz.x;
	texelsz *= abs(pos_vs.z) * fovfactor;

	float l_dot_n = dot(lv, norm_ws);
	float norm_offset_scale = clamp(1.0f - l_dot_n, 0.0f, 1.0f) * texelsz;
	vec4 shadow_offset = vec4(norm_ws*norm_offset_scale, 0);
	
	/* offset poistion (in world space) */
	pos_ws += shadow_offset;
	return pos_ws;
}

void main()
{
#if defined(_SKIN_)
    skin_output_pn pn = skin_vertex_pn(gl_InstanceID, vsi_blend_idxs, vsi_blend_weights, vsi_pos, 
		vsi_norm);
	vec4 pos = pn.pos;
	vec3 norm = pn.norm;
#else
    vec4 pos = vsi_pos;
	vec3 norm = vsi_norm;
#endif

    vec4 pos_ws = vec4(pos * c_mats[gl_InstanceID], 1);
	vec3 norm_ws = vec4(norm, 0) * c_mats[gl_InstanceID];

    o.pos0 = apply_bias(pos_ws, norm_ws, c_views[0], c_fovfactors[0]) * c_cascade_mats[0];
    o.pos1 = apply_bias(pos_ws, norm_ws, c_views[1], c_fovfactors[1]) * c_cascade_mats[1];
    o.pos2 = apply_bias(pos_ws, norm_ws, c_views[2], c_fovfactors[2]) * c_cascade_mats[2];

#if defined(_ALPHAMAP_)
    o.coord = vec2(vsi_coord.x, vsi_coord.y);
#endif
}




