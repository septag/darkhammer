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

/* inputs */
in vec2 vso_coord;

/* outputs */
out vec4 pso_color;

/* textures */
uniform sampler2D s_depth;
uniform sampler2D s_norm;
uniform sampler2D s_noise;

/* uniforms */
uniform vec4 c_params;    /* x = radius, y=bias, z=scale, w=intensity */
uniform vec4 c_rtvsz; /* (x,y) = render-target size, (w,z) = noise texture size */
uniform vec4 c_projparams;

/* constants */
const vec2 g_kernel[8] = vec2[](
    vec2(1, 0),
    vec2(-1, 0),
    vec2(0, 1),
    vec2(0, -1),
    vec2(1, 1),
    vec2(-1, -1),
    vec2(1, -1),
    vec2(-1, 1)
);

const vec2 g_value1 = vec2(0.707f, -0.707f);
const vec2 g_value2 = vec2(0.707f, 0.707f);

/* */
vec2 get_rand(vec2 coord)
{
    vec2 xy = textureLod(s_noise, c_rtvsz.xy*coord/c_rtvsz.zw, 0).xy*2 - 1;
    return normalize(xy);
}

vec3 calc_vspos_bycoord(vec2 coord, vec4 projparams, float depth)
{
    vec2 projpos = coord*2 - 1;
    float depth_lin = depth_linear_p(depth, projparams);
    return vec3((projpos.xy/projparams.xy)*depth_lin, depth_lin);
}

/* main calculation function */
float calc_ambient_t(vec2 coord, vec3 occludee_pos, vec3 occludee_norm, float depth)
{
    float bias = c_params.y;
    float scale = c_params.z;
    float intensity = c_params.w;

    vec3 dv = calc_vspos_bycoord(coord, c_projparams, depth) - occludee_pos;
    float  d = length(dv);
    vec3 v = dv/d;

    return max(0, dot(occludee_norm, v)-bias) * (1.0f/(1.0f+d*scale)) * intensity;
}


float calc_ao(vec2 coord, vec2 offset, vec3 occludee_pos, vec3 occludee_norm)
{
    vec2 coord_offset = coord + offset;
    float depth = textureLod(s_depth, coord_offset, 0).x*2 - 1;
    return calc_ambient_t(coord_offset, occludee_pos, occludee_norm, depth);
}

void main()
{
    float depth = textureLod(s_depth, vso_coord, 0).x*2 - 1;
    if (depth == 1.0)
        discard;

    vec2 norm = textureLod(s_norm, vso_coord, 0).xy;
    vec3 occludee_norm = normal_decode_spheremap(norm);
    vec3 occludee_pos = calc_vspos_bycoord(vso_coord, c_projparams, depth);
    vec2 rand = get_rand(vso_coord);

    float ao = 0;
    float radius = clamp(c_params.x/occludee_pos.z, 0.02f, 5.0f);
    vec2 coord1;
    vec2 coord2 = vec2(0, 0);

    for (int i = 0; i < _PASSCNT_; i++)  {
        coord1 = reflect(g_kernel[i], rand)*radius;
        /* coord2 = vec2(dot(coord1, g_value1), dot(coord2, g_value2)); */

        ao += calc_ao(vso_coord, coord1*0.25f, occludee_pos, occludee_norm);
        ao += calc_ao(vso_coord, coord1*0.5f, occludee_pos, occludee_norm);
        ao += calc_ao(vso_coord, coord1*0.75f, occludee_pos, occludee_norm);
        ao += calc_ao(vso_coord, coord1, occludee_pos, occludee_norm);
    }

    ao = 1 - ao/(_PASSCNT_*4);
    pso_color = vec4(ao, ao, ao, 1);
}

