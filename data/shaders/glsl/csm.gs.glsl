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


layout(triangles) in;
layout(triangle_strip, max_vertices=9) out;

/* input */
in vso  {
    vec4 pos0;
    vec4 pos1;
    vec4 pos2;
#if defined(_ALPHAMAP_)
    vec2 coord;
#endif
} verts[];

/* output */
#if defined(_ALPHAMAP_)
out vec2 gso_coord;
#endif

layout(std140) uniform cb_frame_gs
{
    vec4 c_cascade_planes[4*_CASCADE_CNT_];
};

/* returns false if it's not intersected */
uint test_tri_singleplane(vec4 v0, vec4 v1, vec4 v2, vec4 plane)
{
    float i1 = dot(v0.xyz, plane.xyz) + plane.w;
    float i2 = dot(v1.xyz, plane.xyz) + plane.w;
    float i3 = dot(v2.xyz, plane.xyz) + plane.w;
    return uint(i1 >= 0) | uint(i2 >= 0) | uint(i3 >= 0);
}

bool test_tri_planes(vec4 plane1, vec4 plane2, vec4 plane3, vec4 plane4, 
    vec4 v0, vec4 v1, vec4 v2)
{
    uint t1 = test_tri_singleplane(v0, v1, v2, plane1);
    uint t2 = test_tri_singleplane(v0, v1, v2, plane2);
    uint t3 = test_tri_singleplane(v0, v1, v2, plane3);
    uint t4 = test_tri_singleplane(v0, v1, v2, plane4);
    return bool(t1 & t2 & t3 & t4);
}

/* */
void main()
{
    /* generate 3 triangles for each input trinagle and send them to 3 views (cascade) */
    /* tri#1 -> cascade #1 */
    if (test_tri_planes(c_cascade_planes[0], c_cascade_planes[1], c_cascade_planes[2],
        c_cascade_planes[3], verts[0].pos0, verts[1].pos0, verts[2].pos0))
    {
        gl_Layer = 0;
        for (int i = 0; i < 3; i++)    {
            gl_Position = verts[i].pos0;
#if defined(_ALPHAMAP_)
            gso_coord = verts[i].coord;
#endif
            EmitVertex();
        }
        EndPrimitive();
    }

    /* tri#2 -> cascade #2 */
    if (test_tri_planes(c_cascade_planes[4], c_cascade_planes[5], c_cascade_planes[6],
        c_cascade_planes[7], verts[0].pos1, verts[1].pos1, verts[2].pos1))
    {
        gl_Layer = 1;
        for (int i = 0; i < 3; i++)    {
            gl_Position = verts[i].pos1;
#if defined(_ALPHAMAP_)
            gso_coord = verts[i].coord;
#endif
            EmitVertex();
        }
        EndPrimitive();
    }

    /* tri #3 -> cascade #3 */
    if (test_tri_planes(c_cascade_planes[8], c_cascade_planes[9], c_cascade_planes[10],
        c_cascade_planes[11], verts[0].pos2, verts[1].pos2, verts[2].pos2))
    {    
        gl_Layer = 2;
        for (int i = 0; i < 3; i++)    {
            gl_Position = verts[i].pos2;
#if defined(_ALPHAMAP_)
            gso_coord = verts[i].coord;
#endif
            EmitVertex();
        }
        EndPrimitive();
    }
}


