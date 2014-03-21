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
    float4 color : COLOR0;
    float2 coord0 : TEXCOORD0;
};

int c_type;
SamplerState s_bmp;
Texture2D<float4> t_bmp;

float4 main(vso input) : SV_Target0
{
    float4 color = input.color;
    [flatten]
    if (c_type == 1)
        color *= t_bmp.Sample(s_bmp, input.coord0);
    return color;
}
