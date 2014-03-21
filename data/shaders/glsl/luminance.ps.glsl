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

#define EPSILON 0.00001f

/* input/output */
in vec2 vso_coord;

layout(location=0) out float pso_lum;
#if defined(_BRIGHTPASS_)
layout(location=1) out vec4 pso_bright;
#endif

/* textures */
uniform sampler2D s_tex; /* must be linear sampler */

#if defined(_BRIGHTPASS_)
/* adapted luminance (of the previous frame) */
uniform sampler2D s_lum;
#endif

/* uniforms */
uniform vec2 c_texelsize;
uniform float c_midgrey;

/* constants */
const vec2 g_kernel[4] = vec2[](
    vec2(-1.0f, -1.0f),
    vec2(1.0f, -1.0f),
    vec2(-1.0f, 1.0f),
    vec2(1.0f, 1.0f)
);

const vec3 g_lumweights = vec3(0.299f, 0.587f, 0.114f);
#if defined(_BRIGHTPASS_)
const vec3 g_bright_threshold = vec3(2.0f, 2.0f, 2.0f);
const vec3 g_bright_offset = vec3(5.0f, 5.0f, 5.0f);
#endif

/* */
void main()
{
    float llum = 0; /* log-luminance */
#if defined(_BRIGHTPASS_)
    vec3 final_color = vec3(0, 0, 0);
#endif

    for (int i = 0; i < 4; i++) {
        vec3 color = textureLod(s_tex, vso_coord + g_kernel[i]*c_texelsize, 0).xyz;
        llum += log(dot(color, g_lumweights) + EPSILON);
#if defined(_BRIGHTPASS_)
        final_color += color;
#endif
    }

    pso_lum = llum*0.25f;

#if defined(_BRIGHTPASS_)
    float lum_final = textureLod(s_lum, vec2(0.5f, 0.5f), 0).x;
    final_color *= vec3(0.25f, 0.25f, 0.25f);
    float exposure = c_midgrey / (lum_final + 0.001f);
    final_color *= vec3(exposure, exposure, exposure);
    final_color -= g_bright_threshold;
    final_color = max(final_color, vec3(0, 0, 0));
    final_color /= (g_bright_offset + final_color);
    pso_bright = vec4(final_color, 1);
#endif
}
