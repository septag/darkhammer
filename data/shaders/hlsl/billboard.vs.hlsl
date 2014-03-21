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

struct vsi
{
    float4 pos : POSITION;
    float4 coord : TEXCOORD2;   /* rectangular coord of the quad */
    float4 billboard : TEXCOORD3;   /* x = half-width, y = half-height, z=rotation */
    float4 color: COLOR0;
};

struct vso
{
    float4 pos_ws : POSITION;
    float4 coord : TEXCOORD0;
    float4 billboard : TEXCOORD1;
    float4 color : TEXCOORD2;
};

/* uniforms */
float4x3 c_world;

/* */
vso main(vsi input)
{
    vso o;
    o.pos_ws = float4(mul(input.pos, c_world), 1);
    o.coord = input.coord;
    o.billboard = input.billboard;
    o.color = input.color;
    return o;
}
