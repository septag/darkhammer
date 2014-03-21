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

#ifndef _MAX_INSTANCES_
#error "_MAX_INSTANCES_ not defined"
#endif

/* input */
struct vsi
{
    uint instance_idx : SV_InstanceID;

    float4 pos : POSITION;
    float3 norm : NORMAL;
    float2 coord0 : TEXCOORD0;

#if defined(_NORMALMAP_)
    float3 tangent : TANGENT;
    float3 binorm : BINORMAL;
#endif

#if defined(_SKIN_)
    int4 blend_idxs : BLENDINDICES;
    float4 blend_weights : BLENDWEIGHT;
#endif
};

/* output */
struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
    float3 norm_vs : TEXCOORD1;

#if defined(_NORMALMAP_)
    float3 tangent_vs : TEXCOORD2;
    float3 binorm_vs : TEXCOORD3;
#endif
};

/* per-frame cbuffer */
cbuffer cb_frame
{
    float4x3 c_view;
    float4x4 c_viewproj;
};

/* transforms cbuffer */
cbuffer cb_xforms
{
    float4x3 c_mats[_MAX_INSTANCES_];
};


vso main(vsi i)
{
    vso o;

    /* skinning */
#if defined(_SKIN_)
    #if defined(_NORMALMAP_)
        skin_output_pnt s = skin_vertex_pnt(i.instance_idx, i.blend_idxs, i.blend_weights, i.pos, 
			i.norm, i.tangent, i.binorm);    
        float4 pos = s.pos;
        float3 norm = s.norm;
        float3 tangent = s.tangent;
        float3 binorm = s.binorm;
    #else
        skin_output_pn s = skin_vertex_pn(i.instance_idx, i.blend_idxs, i.blend_weights, i.pos, 
			i.norm);
        float4 pos = s.pos;
        float3 norm = s.norm;
    #endif
#else
    float4 pos = i.pos;
    float3 norm = i.norm;
    #if defined(_NORMALMAP_)
        float3 tangent = i.tangent;
        float3 binorm = i.binorm;
    #endif
#endif
    float3x3 m3 = (float3x3)c_mats[i.instance_idx];
    float3x3 view3 = (float3x3)c_view;

    /* position */
    float4 pos_ws = float4(mul(pos, c_mats[i.instance_idx]), 1.0f);
    o.pos = mul(pos_ws, c_viewproj);

    /* normal */
    float3 norm_ws = mul(norm, m3);
    o.norm_vs = mul(norm_ws, view3);

#if defined(_NORMALMAP_)
    float3 tangent_ws = mul(tangent, m3);
    o.tangent_vs = mul(tangent_ws, view3);

    float3 binorm_ws = mul(binorm, m3);
    o.binorm_vs = mul(binorm_ws, view3);
#endif
    
    /* tex-coord */
    o.coord = i.coord0;
    
    return o;
};


