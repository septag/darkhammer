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

Texture2D<float4> t_alphamap;
SamplerState s_alphamap;

struct gso
{
    uint rt_idx : SV_RenderTargetArrayIndex;
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
};

float4 main(gso i) : SV_Target0
{
    float a = t_alphamap.Sample(s_alphamap, i.coord).a;
    clip(a - 0.95f);
    return float4(1, 1, 1, 1);
}
