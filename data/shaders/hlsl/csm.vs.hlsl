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

struct vsi
{
    uint instance_idx : SV_InstanceID;
    float4 pos : POSITION;
	float3 norm : NORMAL;

#if defined(_ALPHAMAP_)
    float2 coord : TEXCOORD0;
#endif

#if defined(_SKIN_)
    int4 blend_idxs : BLENDINDICES;
    float4 blend_weights : BLENDWEIGHT;
#endif
};

struct vso
{
    float4 pos0 : POSITION0;
    float4 pos1 : POSITION1;
    float4 pos2 : POSITION2;

#if defined(_ALPHAMAP_)
    float2 coord : TEXCOORD0;
#endif
};

cbuffer cb_frame
{
	float4 c_texelsz;	/* x = texelsz */
	float4 c_fovfactors;	/* max(proj.11, proj[1].22) for each projection matrix */
	float4 c_lightdir;	
	float4x3 c_views[_CASCADE_CNT_];
    float4x4 c_cascade_mats[_CASCADE_CNT_];
};

cbuffer cb_xforms
{
    float4x3 c_mats[_MAX_INSTANCES_];
};

float4 apply_bias(float4 pos_ws, float3 norm_ws, float4x3 view, float fovfactor)
{
	float3 lv = c_lightdir.xyz;
	float4 pos_vs = float4(mul(pos_ws, view), 1);
	float texelsz = c_texelsz.x;
	texelsz *= abs(pos_vs.z) * fovfactor;

	float l_dot_n = dot(lv, norm_ws);
	float norm_offset_scale = saturate(1.0f - l_dot_n) * texelsz;
	float4 shadow_offset = float4(norm_ws*norm_offset_scale, 0);
	
	/* offset poistion (in world space) */
	pos_ws += shadow_offset;
	return pos_ws;
}

vso main(vsi i)
{
    vso o;

#if defined(_SKIN_)
    skin_output_pn pn = skin_vertex_pn(i.instance_idx, i.blend_idxs, i.blend_weights, i.pos, i.norm);
	float4 pos = pn.pos;
	float3 norm = pn.norm;
#else
    float4 pos = i.pos;
	float3 norm = i.norm;
#endif

    float4 pos_ws = float4(mul(pos, c_mats[i.instance_idx]), 1);
	float3 norm_ws = mul(float4(norm, 0), c_mats[i.instance_idx]);

    o.pos0 = mul(apply_bias(pos_ws, norm_ws, c_views[0], c_fovfactors[0]), c_cascade_mats[0]);
    o.pos1 = mul(apply_bias(pos_ws, norm_ws, c_views[1], c_fovfactors[1]), c_cascade_mats[1]);
    o.pos2 = mul(apply_bias(pos_ws, norm_ws, c_views[2], c_fovfactors[2]), c_cascade_mats[2]);

#if defined(_ALPHAMAP_)
    o.coord = i.coord;
#endif

    return o;
};




