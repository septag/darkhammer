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

/* output */
out vec4 pso_color;

/* textures */
uniform sampler2D s_depth;
uniform sampler2D s_norm;
uniform sampler2D s_depth_hires;
uniform sampler2D s_norm_hires;
uniform sampler2D s_tex;

/* uniforms */
uniform vec2 c_texelsize;
uniform vec4 c_projparams;

/* constants */
const vec2 g_kernel[4] = vec2[](
    vec2(0.0f, 1.0f),
    vec2(1.0f, 0.0f),
    vec2(-1.0f, 0.0f),
    vec2(0.0, -1.0f)
);

const float g_epsilon = 0.0001f;

/* */
vec3 calc_norm(vec2 norm_xy)
{
    return normal_decode_spheremap(norm_xy);
}

float calc_lineardepth(float depth, vec4 projparams)
{
    return depth_linear_p(depth, projparams);
}

void main()
{
    int i;
    vec2 coords[4];
    for (i = 0; i < 4; i++)
        coords[i] = vso_coord + c_texelsize*g_kernel[i];

    /* normal weights */
    float norm_weights[4];
    vec3 norm_hires = calc_norm(textureLod(s_norm_hires, vso_coord, 0).xy);
    for (i = 0; i < 4; i++) {
        vec3 norm_coarse = calc_norm(textureLod(s_norm, coords[i], 0).xy);
        /* amplify dot-product for norm(x)norm_hires */
        norm_weights[i] = pow(abs(dot(norm_coarse, norm_hires)), 32);
    }

    /* depth weights */
    float depth_weights[4];
    float depth_hires = calc_lineardepth(textureLod(s_depth_hires, vso_coord, 0).x*2 - 1,
        c_projparams);
    for (i = 0; i < 4; i++) {
        float depth_coarse = calc_lineardepth(textureLod(s_depth, coords[i], 0).x*2 - 1, 
            c_projparams);
        depth_weights[i] = 1.0f / (g_epsilon + abs(depth_hires-depth_coarse));
    }

    /* we have the weights, final color evaluation */
    vec3 color_t = vec3(0, 0, 0);
    float weight_sum = 0;
    for (i = 0; i < 4; i++) {
        float weight = norm_weights[i] * depth_weights[i];
        color_t += textureLod(s_tex, coords[i], 0).xyz*weight;
        weight_sum += weight;
    }
    color_t /= weight_sum;
    pso_color = vec4(color_t, 1);
}
