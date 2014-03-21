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
    float4 pos_ws : POSITION;
    float4 coord : TEXCOORD0;
    float4 billboard : TEXCOORD1;
    float4 color : TEXCOORD2;
};

struct gso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
    float4 color : TEXCOORD1;
};

/* cbuffers */
cbuffer cb_frame
{
    float4x3 c_viewinv;
    float4x4 c_viewproj;
};

/* constants */
static const float2 xforms[6] = {
    float2(-1.0f, 1.0f),	/* top-left */
    float2(1.0f, 1.0f),		/* top-right */
    float2(1.0f, -1.0f),	/* bottom-right */
    float2(1.0f, -1.0f),	/* bottom-right */
    float2(-1.0f, -1.0f),	/* bottom-left */
    float2(-1.0f, 1.0f)		/* top-left */
};

[maxvertexcount(6)]
void main(point vso input[1], inout TriangleStream<gso> tris)
{
    int i;
    gso output;
    float3 xaxis = c_viewinv[0];
    float3 yaxis = c_viewinv[1];
    float4 rcoord = input[0].coord;
    float2 coords[6] = {
        float2(rcoord.x, rcoord.y),	/* top-left */
        float2(rcoord.z, rcoord.y),	/* top-right */
        float2(rcoord.z, rcoord.w),	/* bottom-right */
        float2(rcoord.z, rcoord.w),	/* bottom-right */
        float2(rcoord.x, rcoord.w),	/* bottom-left */
        float2(rcoord.x, rcoord.y)	/* top-left */
    };

    /* scale axises to billboard size (half-sizes) */
    float3 xaxis_scaled = xaxis*input[0].billboard.x;
    float3 yaxis_scaled = yaxis*input[0].billboard.y;
    output.color = input[0].color;

    /* tri #1 */
    [unroll]
    for (i = 2; i >= 0; i--)    {
        float3 pos = input[0].pos_ws.xyz + xforms[i].x*xaxis_scaled + xforms[i].y*yaxis_scaled;
        output.pos = mul(float4(pos, 1), c_viewproj);
        output.coord = coords[i];
        tris.Append(output);
    }
    tris.RestartStrip();

    /* tri #2 */
    [unroll]
    for (i = 5; i >= 3; i--)    {
        float3 pos = input[0].pos_ws.xyz + xforms[i].x*xaxis_scaled + xforms[i].y*yaxis_scaled;
        output.pos = mul(float4(pos, 1), c_viewproj);
        output.coord = coords[i];
        tris.Append(output);
    }
    tris.RestartStrip();
}
