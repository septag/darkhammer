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
uniform sampler2D s_tex;

/* uniforms */
uniform vec2 c_texelsize; /* source texture texel-size */

/* constants */
const vec2 g_kernel[4] = vec2[](
    vec2(-0.5f, -0.5f),
    vec2(0.5f, -0.5f),
    vec2(-0.5f, 0.5f),
    vec2(0.5f, 0.5f)
);

/* */
void main()
{
    float depth_max = 0;
    vec4 final_tex;
    
    for (int i = 0; i < 4; i++) {
        vec2 coord = vso_coord + g_kernel[i]*c_texelsize;
        float depth = textureLod(s_depth, coord, 0).x;

        if (depth > depth_max)   {
            depth_max = depth;
            final_tex = textureLod(s_tex, coord, 0);
        }
    }

    pso_color = final_tex;
    gl_FragDepth = depth_max;
}
