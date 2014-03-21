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

Texture2D<float> t_lum;
SamplerState s_lum;

/* uniforms */
float c_midgrey;

/* constants */
static const float3 g_lumweights = float3(0.299f, 0.587f, 0.114f);
static const float3 g_blueshift = float3(1.05f, 0.97f, 1.27f);

/* filmic tonemapping 
 * @param clr input color must be in linear-space
 * @return gamma-space tonemapped color value
 */
float3 tonemap_filmic(float3 clr, float exposure)
{
	float3 r = clr * exposure;
	float3 x = max(0.0f, r - 0.004f);
	return (x*(6.2f*x+0.5f))/(x*(6.2f*x+1.7f)+0.06f);
}

/* returns gamma-space color */
float4 main(vso input) : SV_Target0
{
    float lum = t_lum.SampleLevel(s_lum, float2(0.5f, 0.5f), 0);
    float3 clr = t_tex.SampleLevel(s_tex, input.coord, 0).xyz;

    /* blueshift */
#if defined(_BLUESHIFT_)
    float blueshift_t = saturate(1.0f - (lum + 1.5f)/4.1f);
    float3 rod_clr = dot(clr, g_lumweights) * g_blueshift;
    clr = lerp(clr, rod_clr, blueshift_t);
#endif

    /* tonemap (with luminance in alpha) */
    float exposure = c_midgrey/(lum + 0.001f);
    float3 color = tonemap_filmic(clr, exposure);
    return float4(color, dot(color, g_lumweights));
}
