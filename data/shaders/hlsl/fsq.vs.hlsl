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

/**
 * used for fullscreen quad rendering
 * all shaders that are postfx (or use some kind of fullscreen effect) should use this vertex-shader
 * and output should always be like 'vso' (pos, coord)
 */

struct vsi
{
    float4 pos : POSITION;
    float2 coord : TEXCOORD0;
};

struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
};


vso main(vsi i)
{
    vso o;
    o.pos = i.pos;
    o.coord = i.coord;
    return o;
}
