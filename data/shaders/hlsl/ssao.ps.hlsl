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

/* inputs */
struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
};

/* textures */
Texture2D<float> t_depth;
SamplerState s_depth;

Texture2D<float4> t_norm;
SamplerState s_norm;

Texture2D<float4> t_noise;
SamplerState s_noise;

/* uniforms */
float4 c_params;    /* x = radius, y=bias, z=scale, w=intensity */
float4 c_rtvsz; /* (x,y) = render-target size, (w,z) = noise texture size */
float4 c_projparams;

/* constants */
static const float2 g_kernel[8] = {
    float2(1, 0),
    float2(-1, 0),
    float2(0, 1),
    float2(0, -1),
    float2(1, 1),
    float2(-1, -1),
    float2(1, -1),
    float2(-1, 1)
};

static const float2 g_value1 = float2(0.707f, -0.707f);
static const float2 g_value2 = float2(0.707f, 0.707f);


/* */
float2 get_rand(float2 coord)
{
    float2 xy = t_noise.SampleLevel(s_noise, c_rtvsz.xy*coord/c_rtvsz.zw, 0).xy;
    return normalize(xy*2 - 1);
}

float3 calc_vspos_bycoord(float2 coord, float4 projparams, float depth)
{
    float2 projpos = float2(coord.x*2 - 1, (1-coord.y)*2 - 1);
    float depth_lin = depth_linear_p(depth, projparams);
    return float3((projpos.xy/projparams.xy)*depth_lin, depth_lin);
    
}

/* main calculation function */
float calc_ambient_t(float2 coord, float3 occludee_pos, float3 occludee_norm, float depth)
{
    const float bias = c_params.y;
    const float scale = c_params.z;
    const float intensity = c_params.w;

    float3 dv = calc_vspos_bycoord(coord, c_projparams, depth) - occludee_pos;
    float  d = length(dv);
    float3 v = dv/d;

    return max(0, dot(occludee_norm, v)-bias) * (1.0f/(1.0f+d*scale)) * intensity;
}


float calc_ao(float2 coord, float2 offset, float3 occludee_pos, float3 occludee_norm)
{
    float2 coord_offset = coord + offset;
    float depth = t_depth.SampleLevel(s_depth, coord_offset, 0);
    return calc_ambient_t(coord_offset, occludee_pos, occludee_norm, depth);
}

float4 main(vso input) : SV_Target0
{
    float depth = t_depth.SampleLevel(s_depth, input.coord, 0);
    clip(0.999999f - depth);

    float2 norm = t_norm.SampleLevel(s_norm, input.coord, 0).xy;
    float3 occludee_norm = normal_decode_spheremap(norm);
    float3 occludee_pos = calc_vspos_bycoord(input.coord, c_projparams, depth);
    float2 rand = get_rand(input.coord);

    float ao = 0;
    float radius = clamp(c_params.x/occludee_pos.z, 0.02f, 5.0f);

    [unroll]
    for (int i = 0; i < _PASSCNT_; i++)  {
        float2 coord1 = reflect(g_kernel[i], rand)*radius;
        /* coord2 currently gives some extreme values which I haven't debugged yet */
        /* float2 coord2 = float2(dot(coord1, g_value1), dot(coord2, g_value2)); */

        ao += calc_ao(input.coord, coord1*0.25f, occludee_pos, occludee_norm);
        ao += calc_ao(input.coord, coord1*0.5f, occludee_pos, occludee_norm);
        ao += calc_ao(input.coord, coord1*0.75f, occludee_pos, occludee_norm);
        ao += calc_ao(input.coord, coord1, occludee_pos, occludee_norm);
    }

    ao = 1 - ao/(_PASSCNT_*4);
    return float4(ao.xxx, 1);
}

