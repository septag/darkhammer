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
Texture2D<float> t_tex;
SamplerState s_tex;

/* */
float4 main(vso input) : SV_Target0
{
    return float4(exp(t_tex.SampleLevel(s_tex, input.coord, 0)).xxx, 1);
}

