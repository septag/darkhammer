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

#if defined(_SKIN_)

uniform samplerBuffer tb_skins;

struct skin_output_pnt
{
    vec4 pos;
    vec3 norm;
    vec3 tangent;
    vec3 binorm;
};

struct skin_output_pn
{
    vec4 pos;
    vec3 norm;
};

mat3x4 get_bone(int inst_idx, int bone_idx)
{
	mat3x4 r;
	int offset = (bone_idx + inst_idx*_MAX_BONES_)*3;
	r[0] = texelFetch(tb_skins, offset);
	r[1] = texelFetch(tb_skins, offset + 1);
	r[2] = texelFetch(tb_skins, offset + 2);
	return r;
}

vec4 skin_vertex_p(int inst_idx, ivec4 blend_idxs, vec4 blend_weights, vec4 pos)
{
	vec4 r = vec4(0, 0, 0, 1);
	for (int i = 0; i < 4; i++)	{
		mat3x4 m = get_bone(inst_idx, blend_idxs[i]);
		r.xyz += (pos * m) * blend_weights[i];
	}

	return r;    
}

skin_output_pn skin_vertex_pn(int inst_idx, ivec4 blend_idxs, vec4 blend_weights, vec4 pos, 
	vec3 norm)
{
	skin_output_pn r;

    r.pos = vec4(0, 0, 0, 1);
    r.norm = vec3(0, 0, 0);

	for (int i = 0; i < 4; i++) {
		mat3x4 m = get_bone(inst_idx, blend_idxs[i]);
		float w = blend_weights[i];

		r.pos.xyz += (pos * m) * w;
		r.norm += (norm * mat3(m)) * w;
	}

	return r;    
}

skin_output_pnt skin_vertex_pnt(int inst_idx, ivec4 blend_idxs, vec4 blend_weights, vec4 pos,
    vec3 norm, vec3 tangent, vec3 binorm)
{
	skin_output_pnt r;

    r.pos = vec4(0, 0, 0, 1);
    r.norm = vec3(0, 0, 0);
    r.tangent = vec3(0, 0, 0);
    r.binorm = vec3(0, 0, 0);

	for (int i = 0; i < 4; i++) {
		mat3x4 m = get_bone(inst_idx, blend_idxs[i]);
        mat3 m3 = mat3(m);
		float w = blend_weights[i];

		r.pos.xyz += (pos * m) * w;
		r.norm += (norm * m3) * w;
		r.tangent += (tangent * m3) * w;
		r.binorm += (binorm * m3) * w;
	}

	return r;    
}

#endif
