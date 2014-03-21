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

struct vsi
{
    float4 pos : POSITION;
    float4 color : COLOR0;
    float2 coord0 : TEXCOORD0;
};

struct vso
{
    float4 pos : SV_Position;
    float4 color : COLOR0;
    float2 coord0 : TEXCOORD0;
};

float2 c_rtsz;

vso main(vsi input)
{
    vso output;
    output.pos = float4(
        (input.pos.x / c_rtsz.x)*2 - 1,
        1 - (input.pos.y / c_rtsz.y)*2,
        0.0f, 1.0f);
    output.color = input.color;
    output.coord0 = float2(input.coord0.x, 1 - input.coord0.y);
    return output;
}
