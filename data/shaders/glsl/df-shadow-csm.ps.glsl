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

/* input */
in vec2 vso_coord;
in vec3 vso_viewray;

/* output */
out vec4 pso_color;

/* textures */
uniform sampler2D s_depth;
#if !defined(_D3D10_)
uniform sampler2DArrayShadow s_shadowmap;
#else
uniform samplerCubeShadow s_shadowmap;
#endif

/* constants */
const vec4 g_colors[4] = vec4[](
    vec4(1.0f, 0.0f, 0.0f, 1.0f),
    vec4(0.0f, 1.0f, 0.0f, 1.0f),
    vec4(0.0f, 0.0f, 1.0f, 1.0f),
    vec4(1.0f, 1.0f, 0.0f, 1.0f)
);

const vec2 g_kernel_poisson[4] = vec2[](
    vec2( -0.94201624f, -0.39906216f),
    vec2( 0.94558609f, -0.76890725f),
    vec2(-0.094184101f, -0.92938870f),
    vec2(0.34495938f, 0.29387760f)
);

const float g_split_weights[4] = float[](
    1.0f, 
    0.5f,
    0.2f,
    0.1f
);

/* uniforms */
uniform vec4 c_cascades_vs[_CASCADE_CNT_];   /* spheres for cascades */
uniform mat4 c_shadow_mats[_CASCADE_CNT_];
uniform vec4 c_projparams;

#if !defined(_D3D10_)
float shadow_1tap(vec3 shadow_vect, int map_idx)
{
    shadow_vect.y = 1 - shadow_vect.y;
    return texture(s_shadowmap, vec4(shadow_vect.xy, map_idx, shadow_vect.z*0.5f+0.5f));
}

float shadow_4taps(vec3 shadow_vect, int map_idx)
{
    const vec2 texel = vec2(1.0f/700.0f, 1.0f/700.0f);
    vec2 texel_size = texel*g_split_weights[map_idx];
    float occ = 0;
    shadow_vect.y = 1 - shadow_vect.y;
    shadow_vect.z = shadow_vect.z*0.5f+0.5f;

    for (int i = 0; i < 4; i++) {
        occ += texture(s_shadowmap, 
            vec4(shadow_vect.xy + g_kernel_poisson[i]*texel_size, map_idx, shadow_vect.z));
    }

    return occ*0.25f;
}
#else
float shadow_1tap(vec3 shadow_vect, int map_idx)
{
    shadow_vect.y = 1 - shadow_vect.y;

    if (min(shadow_vect.x, shadow_vect.y) > 0.0f && max(shadow_vect.x, shadow_vect.y) < 1.0f) {
        return texture(s_shadowmap, 
            vec4(coord_maptocube(shadow_vect.xy, map_idx), shadow_vect.z*0.5f+0.5f));
    }   else    {
        return 0.0f;
    }
}

float shadow_4taps(vec3 shadow_vect, int map_idx)
{
    const vec2 texel = vec2(1.0f/700.0f, 1.0f/700.0f);
    vec2 texel_size = texel*g_split_weights[map_idx];
    float occ = 0;
    shadow_vect.y = 1 - shadow_vect.y;
    shadow_vect.z = shadow_vect.z*0.5f+0.5f;

    for (int i = 0; i < 4; i++) {
        vec2 coord = shadow_vect.xy + g_kernel_poisson[i]*texel_size;
        if (min(coord.x, coord.y) > 0.0f && max(coord.x, coord.y) < 1.0f) {
            occ += texture(s_shadowmap, vec4(coord_maptocube(coord, map_idx), shadow_vect.z));
        }
    }

    return occ*0.25f;
}
#endif

void main()
{
    /* reconstruct position */
    float depth = textureLod(s_depth, vso_coord, 0).x*2.0f - 1.0f;
    float depth_vs = c_projparams.w / (depth - c_projparams.z);    /* view depth */
    vec3 pos_vs = depth_vs * vso_viewray;
    vec4 color = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    for (int i = 0; i < _CASCADE_CNT_; i++)  {
        if (pos_insphere(pos_vs, c_cascades_vs[i]))  {
            vec4 shadow = vec4(pos_vs, 1) * c_shadow_mats[i];
            float occ = shadow_4taps(shadow.xyz/shadow.w, i);
#if defined(_PREVIEW_)
            color = vec4(g_colors[i].xyz*occ, 1);
#else
            color = vec4(occ, occ, occ, 1);
#endif
            break;
        }
    }

    pso_color = color;
}
