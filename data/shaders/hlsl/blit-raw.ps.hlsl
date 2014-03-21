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

struct pso
{
    float4 color : SV_Target0;
    float depth : SV_Depth;
};

Texture2D<float> s_depth;
Texture2D<float4> s_color;

pso main(vso i)
{
    pso o;

    int3 coord2d = int3(i.pos.xy, 0);

    o.color = s_color.Load(coord2d);
    o.depth = s_depth.Load(coord2d);

    return o;
}
