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
out vec4 pso_color;

/* textures */
uniform sampler2D s_tex;
uniform sampler2D s_lum;

/* uniforms */
uniform float c_midgrey;

/* constants */
const vec3 g_lumweights = vec3(0.299f, 0.587f, 0.114f);
const vec3 g_blueshift = vec3(1.05f, 0.97f, 1.27f);

/**
 * filmic tonemapping 
 * @param clr input color must be in linear-space
 * @return gamma-space tonemapped color value
 */
vec3 tonemap_filmic(vec3 clr, float exposure)
{
	vec3 r = clr * exposure;
	vec3 x = max(vec3(0.0f, 0.0f, 0.0f), r - vec3(0.004f, 0.004f, 0.004f));
	return (x*(vec3(6.2f, 6.2f, 6.2f)*x + vec3(0.5f, 0.5f, 0.5f))) /
	   (x*(vec3(6.2f, 6.2f, 6.2f)*x + vec3(1.7f, 1.7f, 1.7f))+ vec3(0.06f, 0.06f, 0.06f));
}

/* returns gamma-space color */
void main()
{
    float lum = textureLod(s_lum, vec2(0.5f, 0.5f), 0).x;
    vec3 clr = textureLod(s_tex, vso_coord, 0).xyz;

    /* blueshift */
#if defined(_BLUESHIFT_)
    float blueshift_t = clamp(1.0f - (lum + 1.5f)/4.1f, 0, 1);
    vec3 rod_clr = dot(clr, g_lumweights) * g_blueshift;
    clr = mix(clr, rod_clr, blueshift_t);
#endif

    /* tonemap */
    float exposure = c_midgrey/(lum + 0.001f);
    vec3 color = tonemap_filmic(clr, exposure);
    pso_color = vec4(color, dot(color, g_lumweights));
}
