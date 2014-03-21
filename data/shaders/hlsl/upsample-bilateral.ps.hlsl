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

Texture2D<float> t_depth_hires; /* 2x size of source */
SamplerState s_depth_hires;

Texture2D<float4> t_norm_hires; /* 2x size of source */
SamplerState s_norm_hires;

Texture2D<float4> t_tex;    /* texture to be upsampled */
SamplerState s_tex;

/* uniforms */
float2 c_texelsize;
float4 c_projparams;

/* constants */
static const float2 g_kernel[4] = {
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(-1.0f, 0.0f),
    float2(0.0, -1.0f)
};

static const float g_epsilon = 0.0001f;

/* */
float3 calc_norm(float2 norm_xy)
{
    return normal_decode_spheremap(norm_xy);
}

float calc_lineardepth(float depth, float4 projparams)
{
    return depth_linear_p(depth, projparams);
}

float4 main(vso input) : SV_Target0
{
    int i;
    float2 coords[4];
    [unroll]
    for (i = 0; i < 4; i++)
        coords[i] = input.coord + c_texelsize*g_kernel[i];

    /* normal weights */
    float norm_weights[4];
    float3 norm_hires = calc_norm(t_norm_hires.SampleLevel(s_norm_hires, input.coord, 0).xy);
    [unroll]
    for (i = 0; i < 4; i++) {
        float3 norm_coarse = calc_norm(t_norm.SampleLevel(s_norm, coords[i], 0).xy);
        /* amplify dot-product for norm(x)norm_hires */
        norm_weights[i] = pow(abs(dot(norm_coarse, norm_hires)), 32);
    }

    /* depth weights */
    float depth_weights[4];
    float depth_hires = calc_lineardepth(t_depth_hires.SampleLevel(s_depth_hires, input.coord, 0),
        c_projparams);
    [unroll]
    for (i = 0; i < 4; i++) {
        float depth_coarse = calc_lineardepth(t_depth.SampleLevel(s_depth, coords[i], 0), 
            c_projparams);
        depth_weights[i] = 1.0f / (g_epsilon + abs(depth_hires-depth_coarse));
    }

    /* we have the weights, final color evaluation */
    float3 color_t = 0;
    float weight_sum = 0;
    
    [unroll]        
    for (i = 0; i < 4; i++) {
        float weight = norm_weights[i] * depth_weights[i];
        color_t += t_tex.SampleLevel(s_tex, coords[i], 0).xyz*weight;
        weight_sum += weight;
    }
    color_t /= weight_sum;
    return float4(color_t, 1);
}
