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

#define EPSILON 0.00001f

struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
};

struct pso
{
    float lum : SV_Target0;
#if defined(_BRIGHTPASS_)
    float4 bright : SV_Target1;
#endif
};

/* textures */
Texture2D<float4> t_tex;
SamplerState s_tex; /* must be linear sampler */

#if defined(_BRIGHTPASS_)
/* adapted luminance (of the previous frame) */
Texture2D<float> t_lum;
SamplerState s_lum;
#endif

/* uniforms */
float2 c_texelsize;
float c_midgrey;

/* constants */
static const float2 g_kernel[4] = {
    float2(-1.0f, -1.0f),
    float2(1.0f, -1.0f),
    float2(-1.0f, 1.0f),
    float2(1.0f, 1.0f)
};

static const float3 g_lumweights = float3(0.299f, 0.587f, 0.114f);
#if defined(_BRIGHTPASS_)
static const float3 g_bright_threshold = float3(2.0f, 2.0f, 2.0f);
static const float3 g_bright_offset = float3(5.0f, 5.0f, 5.0f);
#endif

/* */
pso main(vso input)
{
    pso o;

    float llum = 0; /* log-luminance */
#if defined(_BRIGHTPASS_)
    float3 final_color = 0;
#endif

    [unroll]
    for (int i = 0; i < 4; i++) {
        float3 color = t_tex.SampleLevel(s_tex, input.coord + g_kernel[i]*c_texelsize, 0).xyz;
        llum += log(dot(color, g_lumweights) + EPSILON);
#if defined(_BRIGHTPASS_)
        final_color += color;
#endif
    }

    o.lum = llum*0.25f;

#if defined(_BRIGHTPASS_)
    float lum_final = t_lum.SampleLevel(s_lum, float2(0.5f, 0.5f), 0);
    final_color *= 0.25f;
    final_color *= (c_midgrey / (lum_final + 0.001f));
    final_color -= g_bright_threshold;
    final_color = max(final_color, 0.0f);
    final_color /= (g_bright_offset + final_color);
    o.bright = float4(final_color, 1);
#endif

    return o;
}
