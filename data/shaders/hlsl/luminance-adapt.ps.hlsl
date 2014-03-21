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
Texture2D<float> t_lum_target;
SamplerState s_lum_target;

Texture2D<float> t_lum_adapted;
SamplerState s_lum_adapted;

/* uniforms */
float c_elapsedtm;  /* in seconds */
float c_lastmip;
float2 c_lum_range;    /* x = lum_min, y = lum_max */

/* */
float4 main() : SV_Target0
{
    float lum_target = exp(t_lum_target.SampleLevel(s_lum_target, float2(0.5f, 0.5f), c_lastmip));
    lum_target = clamp(lum_target, c_lum_range.x, c_lum_range.y);
    float lum_adapt = t_lum_adapted.SampleLevel(s_lum_adapted, float2(0.5f, 0.5f), 0);
    /* move closer to target lumnance 2% each frame (30fps) */
    float lum_final = lum_adapt + (lum_target - lum_adapt)*(1 - pow(0.98f, 30*c_elapsedtm));
    return float4(lum_final.xxx, 1.0f);
}
