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

/* input/output */
in vec2 vso_coord;
out vec4 pso_color;

/* textures */
uniform sampler2D s_depth;

#if defined(_EXTRA_)
uniform sampler2D s_depth_ext;
#endif

/* uniforms */
uniform vec2 c_camprops; /* x = near, y = far */

void main()
{
    float depth_zbuff = textureLod(s_depth, vec2(vso_coord.x, 1.0 - vso_coord.y), 0).x;
    float depth = (2.0f * c_camprops.x)/(c_camprops.y  + c_camprops.x - 
        depth_zbuff*(c_camprops.y - c_camprops.x));
    vec4 color = vec4(depth, depth, depth, 1);

#if defined(_EXTRA_)
    float depth_zbuff_ext = textureLod(s_depth_ext, vec2(vso_coord.x, 1.0 - vso_coord.y), 0).x;
    float depth_ext = (2.0f * c_camprops.x)/(c_camprops.y  + c_camprops.x - 
        depth_zbuff_ext*(c_camprops.y - c_camprops.x));
    if (depth_ext < depth)
        color *= vec4(depth_ext, 0, 0, 1);
#endif

    pso_color = color;
}
