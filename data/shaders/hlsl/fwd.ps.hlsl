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

struct vso
{
    float4 pos : SV_Position;
    float3 norm_ws : TEXCOORD0;
    float2 coord0 : TEXCOORD1;
};

cbuffer cb_mtl
{
    float4 c_mtl_ambientclr;
    float4 c_mtl_diffuseclr;
};

#if defined(_DIFFUSEMAP_)
SamplerState s_mtl_diffusemap;
Texture2D<float4> t_mtl_diffusemap;
#endif

float4 main(vso input) : SV_Target0
{
    float4 albedo = c_mtl_diffuseclr;

#if defined(_DIFFUSEMAP_)
    albedo *= t_mtl_diffusemap.Sample(s_mtl_diffusemap, input.coord0);
#endif

    float3 light_dir = normalize(float3(0.0f, -0.5f, 1.0f));

    float3 norm_ws = normalize(input.norm_ws);
    float diff_term = max(0, dot(-light_dir, norm_ws));

    float3 color = diff_term*albedo.rgb + c_mtl_ambientclr.rgb*albedo.rgb;
    return float4(color, 1);
}
