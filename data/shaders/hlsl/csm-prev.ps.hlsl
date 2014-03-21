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

struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
};

struct pso
{
    float4 cs0 : SV_Target0;
    float4 cs1 : SV_Target1;
    float4 cs2 : SV_Target2;
};

/* textures */
#if !defined(_D3D10_)
Texture2DArray<float> t_shadowmap;
#else
TextureCube<float> t_shadowmap;
#endif

SamplerState s_shadowmap;

/* constants */
float4 c_orthoparams[_CASCADE_CNT_];
float c_max_far[_CASCADE_CNT_];

/* */
float4 get_view_depth(float4 orthoparams, float max_far, float2 coord, int map_idx)
{
#if !defined(_D3D10_)
    float depth = t_shadowmap.SampleLevel(s_shadowmap, float3(coord, map_idx), 0);
#else
    float depth = t_shadowmap.SampleLevel(s_shadowmap, coord_maptocube(coord, map_idx), 0);
#endif

    float depth_vs = (depth - orthoparams.w)/orthoparams.z;
    depth_vs = pow(abs(depth_vs/max_far), 10);
    return float4(depth_vs, depth_vs, depth_vs, 1);
}

pso main(vso input)
{
    float4 colors[_CASCADE_CNT_];
    [unroll]
    for (int i = 0; i < _CASCADE_CNT_; i++) {
        colors[i] = get_view_depth(c_orthoparams[i], c_max_far[i], input.coord, i);
    }

    pso o;
    o.cs0 = colors[0];
    o.cs1 = colors[1];
    o.cs2 = colors[2];
    return o;
}


