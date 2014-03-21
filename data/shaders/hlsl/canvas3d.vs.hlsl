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
    float2 coord : TEXCOORD0;
};

struct vso
{
    float4 pos : SV_Position;
#if defined(_TEXTURE_)
    float2 coord : TEXCOORD0;
#endif
};

float4x4 c_viewproj;
float4x3 c_world;

vso main(vsi input)
{
    vso output;
    float4 pos = float4(mul(input.pos, c_world), 1);
    output.pos = mul(pos, c_viewproj);
#if defined(_TEXTURE_)
    output.coord = input.coord;
#endif
    return output;
}
