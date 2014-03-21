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

layout(points) in;
layout(triangle_strip, max_vertices=6) out;

/* verts/output */
in vso
{
    vec4 pos_ws;
    vec4 coord;
    vec4 billboard;
    vec4 color;
} verts[];

out gso
{
    vec2 coord;
    vec4 color;
} o;

/* cbuffers */
layout(std140) uniform cb_frame
{
    mat3x4 c_viewinv;
    mat4 c_viewproj;
};

/* constants */
const vec2 xforms[6] = vec2[](
    vec2(-1.0f, 1.0f),    /* top-left */
    vec2(1.0f, 1.0f),     /* top-right */
    vec2(1.0f, -1.0f),    /* bottom-right */
    vec2(1.0f, -1.0f),    /* bottom-right */
    vec2(-1.0f, -1.0f),   /* bottom-left */
    vec2(-1.0f, 1.0f)     /* top-left */
);

void main()
{
    int i;
    vec3 xaxis = vec3(c_viewinv[0].x, c_viewinv[1].x, c_viewinv[2].x);
    vec3 yaxis = vec3(c_viewinv[0].y, c_viewinv[1].y, c_viewinv[2].y);
    vec4 rcoord = verts[0].coord;
    vec2 coords[6] = vec2[](
        vec2(rcoord.x, rcoord.y), /* top-left */
        vec2(rcoord.z, rcoord.y), /* top-right */
        vec2(rcoord.z, rcoord.w), /* bottom-right */
        vec2(rcoord.z, rcoord.w), /* bottom-right */
        vec2(rcoord.x, rcoord.w), /* bottom-left */
        vec2(rcoord.x, rcoord.y)  /* top-left */
    );

    /* scale axises to billboard size (half-sizes) */
    vec3 xaxis_scaled = xaxis*verts[0].billboard.x;
    vec3 yaxis_scaled = yaxis*verts[0].billboard.y;
    o.color = verts[0].color;

    /* tri #1 */
    for (i = 2; i >= 0; i--)    {
        vec3 pos = verts[0].pos_ws.xyz + xforms[i].x*xaxis_scaled + xforms[i].y*yaxis_scaled;
        gl_Position = vec4(pos, 1) * c_viewproj;
        o.coord = coords[i];
        EmitVertex();
    }
    EndPrimitive();

    /* tri #2 */
    for (i = 5; i >= 3; i--)    {
        vec3 pos = verts[0].pos_ws.xyz + xforms[i].x*xaxis_scaled + xforms[i].y*yaxis_scaled;
        gl_Position = vec4(pos, 1) * c_viewproj;
        o.coord = coords[i];
        EmitVertex();
    }
    EndPrimitive();
}
