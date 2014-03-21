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

struct pso
{
    float4 color : SV_Target0;
    float depth : SV_Depth;
};

/* textures */
Texture2D<float> t_depth;
SamplerState s_depth;

Texture2D t_tex;
SamplerState s_tex;

/* uniforms */
float2 c_texelsize; /* source texture texel-size */

/* */
static const float2 g_kernel[4] = {
    float2(-0.5f, -0.5f),
    float2(0.5f, -0.5f),
    float2(-0.5f, 0.5f),
    float2(0.5f, 0.5f)
};

pso main(vso input)
{
    float depth_max = 0;
    float4 final_tex;
    
    [unroll]
    for (int i = 0; i < 4; i++) {
        float2 coord = input.coord + g_kernel[i]*c_texelsize;
        float depth = t_depth.SampleLevel(s_depth, coord, 0);

        [flatten]
        if (depth > depth_max)   {
            depth_max = depth;
            final_tex = t_tex.SampleLevel(s_tex, coord, 0);
        }
    }

    pso o;
    o.color = final_tex;
    o.depth = depth_max;
    return o;
}
