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

in vec2 vso_coord;
in vec3 vso_viewray;

out vec4 pso_color;

#if defined(_VIEWNORMALS_)
/* normals view */
uniform sampler2D s_viewmap;

void main()
{
    vec2 norm_enc = textureLod(s_viewmap, vso_coord, 0).rg;
    vec3 norm = normal_decode_spheremap(norm_enc);
    pso_color = vec4(norm, 1.0f);
}
#elif defined(_VIEWALBEDO_)
/* albedo view */
uniform sampler2D s_viewmap;

void main()
{
    vec3 albedo = textureLod(s_viewmap, vso_coord, 0).rgb;
    pso_color = vec4(albedo, 1.0f);
}
#elif defined(_VIEWSPECULARMUL_)
/* specular multiplier */
uniform sampler2D s_viewmap;

void main()
{
    float spec = textureLod(s_viewmap, vso_coord, 0).a;
    pso_color = vec4(spec, spec, spec, 1.0f);
}
#elif defined(_VIEWNORMALSNOMAP_)
/* normal w/o mapping */
uniform sampler2D s_viewmap;

void main()
{
    vec2 norm_enc = textureLod(s_viewmap, vso_coord, 0).rg;
    vec3 norm = normal_decode_spheremap(norm_enc);
    pso_color = vec4(norm, 1.0f);
}
#elif defined(_VIEWDEPTH_)
/* normal w/o mapping */
uniform sampler2D s_viewmap;
uniform vec2 c_camprops;  /* x: near, y:far */

void main()
{
    /* NOTE: depth buffer fetch in opengl is between [-1 ~ 1] */
    float depth_zbuff = textureLod(s_viewmap, vso_coord, 0).r*2.0f - 1.0f;
    float depth = depth_linear(depth_zbuff, c_camprops.x, c_camprops.y);
    depth *= (c_camprops.y*0.01f);
    pso_color = vec4(depth, depth, depth, 1.0f);
}
#elif defined(_VIEWMTL_)
/* mtl preview with random generated colors */
uniform usampler2D s_viewmap;
uniform float c_mtlmax;

void main()
{
    ivec2 coord2d = ivec2(gl_FragCoord.xy);

    uvec2 mtl_enc = texelFetch(s_viewmap, coord2d, 0).xy;
    uint mtl_idx;
    float gloss;
    mtl_decode(mtl_enc, mtl_idx, gloss);
    float fmtlidx = float(mtl_idx);
    float value = fmtlidx/c_mtlmax;
    pso_color = color_generate(value);
}
#elif defined(_VIEWGLOSS_)
uniform usampler2D s_viewmap;

void main()
{
    ivec2 coord2d = ivec2(gl_FragCoord.xy);

    uvec2 mtl_enc = texelFetch(s_viewmap, coord2d, 0).xy;
    float gloss = float(mtl_enc.y) / UINT16_MAX;
    pso_color = vec4(gloss, gloss, gloss, 1);
}
#else
#error "no valid preprocessor is set"
#endif
