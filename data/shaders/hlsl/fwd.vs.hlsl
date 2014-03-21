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

struct vsi
{
    float4 pos : POSITION;
    float3 norm : NORMAL;
    float2 coord0 : TEXCOORD0;
    uint instance_idx : SV_InstanceID;
};

struct vso
{
    float4 pos : SV_Position;
    float3 norm_ws : TEXCOORD0;
    float2 coord0 : TEXCOORD1;
};

cbuffer cb_frame
{
    float4x4 c_viewproj;
};


cbuffer cb_xforms
{
    float4x3 c_mats[_MAX_INSTANCES_];
};

vso main(vsi input)
{
    vso output;

    float4 pos_ws = float4(mul(input.pos, c_mats[input.instance_idx]), 1);

    output.pos = mul(pos_ws, c_viewproj);
    output.norm_ws = mul(input.norm, (float3x3)c_mats[input.instance_idx]);
    output.coord0 = float2(input.coord0.x, 1 - input.coord0.y);

    return output;
}
