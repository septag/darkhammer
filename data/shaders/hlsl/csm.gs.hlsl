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
    float4 pos0 : POSITION0;
    float4 pos1 : POSITION1;
    float4 pos2 : POSITION2;

#if defined(_ALPHAMAP_)
    float2 coord : TEXCOORD0;
#endif
};

struct gso
{
    uint rt_idx : SV_RenderTargetArrayIndex;
    float4 pos : SV_Position;
    
#if defined(_ALPHAMAP_)
    float2 coord : TEXCOORD0;
#endif
};

cbuffer cb_frame_gs
{
    float4 c_cascade_planes[4*_CASCADE_CNT_];
};

/* returns false if it's not intersected */
uint test_tri_singleplane(float4 v0, float4 v1, float4 v2, float4 plane)
{
    float i1 = dot(v0.xyz, plane.xyz) + plane.w;
    float i2 = dot(v1.xyz, plane.xyz) + plane.w;
    float i3 = dot(v2.xyz, plane.xyz) + plane.w;
    return (i1 >= 0) | (i2 >= 0) | (i3 >= 0);
}

uint test_tri_planes(float4 plane1, float4 plane2, float4 plane3, float4 plane4, 
    float4 v0, float4 v1, float4 v2)
{
    uint t1 = test_tri_singleplane(v0, v1, v2, plane1);
    uint t2 = test_tri_singleplane(v0, v1, v2, plane2);
    uint t3 = test_tri_singleplane(v0, v1, v2, plane3);
    uint t4 = test_tri_singleplane(v0, v1, v2, plane4);
    return (t1 & t2 & t3 & t4);
}

[maxvertexcount(9)]
void main(triangle vso i[3], inout TriangleStream<gso> tris)
{
    /* generate 3 triangles for each input trinagle and send them to 3 views (cascade) */
    gso o[3];

#if defined(_ALPHAMAP_)
    o[0].coord = i[0].coord;
    o[1].coord = i[1].coord;
    o[2].coord = i[2].coord;
#endif

    /* tri #1 -> cascade 1 */
    if (test_tri_planes(c_cascade_planes[0], c_cascade_planes[1], c_cascade_planes[2],
        c_cascade_planes[3], i[0].pos0, i[1].pos0, i[2].pos0))
    {
        o[0].rt_idx = 0;
        o[1].rt_idx = 0;
        o[2].rt_idx = 0;
        o[0].pos = i[0].pos0;
        o[1].pos = i[1].pos0;
        o[2].pos = i[2].pos0;
        tris.Append(o[0]);
        tris.Append(o[1]);
        tris.Append(o[2]);
        tris.RestartStrip();
    }

    /* tri #2 -> cascade 2 */
    if (test_tri_planes(c_cascade_planes[4], c_cascade_planes[5], c_cascade_planes[6],
        c_cascade_planes[7], i[0].pos1, i[1].pos1, i[2].pos1))
    {
        o[0].rt_idx = 1;
        o[1].rt_idx = 1;
        o[2].rt_idx = 1;
        o[0].pos = i[0].pos1;
        o[1].pos = i[1].pos1;
        o[2].pos = i[2].pos1;
        tris.Append(o[0]);
        tris.Append(o[1]);
        tris.Append(o[2]);
        tris.RestartStrip();
    }

    /* tri #3 -> cascade 3 */
    if (test_tri_planes(c_cascade_planes[8], c_cascade_planes[9], c_cascade_planes[10],
        c_cascade_planes[11], i[0].pos2, i[1].pos2, i[2].pos2))
    {
        o[0].rt_idx = 2;
        o[1].rt_idx = 2;
        o[2].rt_idx = 2;
        o[0].pos = i[0].pos2;
        o[1].pos = i[1].pos2;
        o[2].pos = i[2].pos2;
        tris.Append(o[0]);
        tris.Append(o[1]);
        tris.Append(o[2]);
        tris.RestartStrip();
    }
}


