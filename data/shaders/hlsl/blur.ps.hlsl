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

/* textures */
Texture2D<float4> t_tex;
SamplerState s_tex;

/* uniforms */
float4 c_kernel[_KERNELSIZE_];  /* xy: coord-offset, z:weight */
static const int kernel_half = _KERNELSIZE_/2;

/* */
float4 main(vso input) : SV_Target0
{
    float4 color = 0;

    [unroll]
    for (int i = -kernel_half; i <= kernel_half; i++)   {
        int idx = i + kernel_half;
        color += t_tex.SampleLevel(s_tex, input.coord + c_kernel[idx].xy, 0) * c_kernel[idx].z;
    }

    return float4(color.xyz, 1);
}
