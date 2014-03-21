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
layout(location=0) out vec4 pso_cs0;
layout(location=1) out vec4 pso_cs1;
layout(location=2) out vec4 pso_cs2;

/* textures */
#if !defined(_D3D10_)
uniform sampler2DArray s_shadowmap;
#else
uniform samplerCube s_shadowmap;
#endif

/* uniforms */
uniform vec4 c_orthoparams[_CASCADE_CNT_];
uniform float c_max_far[_CASCADE_CNT_];

/* */
vec4 get_view_depth(vec4 orthoparams, float max_far, vec2 coord, int map_idx)
{
#if !defined(_D3D10_)
    float depth = texture(s_shadowmap, vec3(coord, map_idx)).x;
#else
    float depth = texture(s_shadowmap, coord_maptocube(coord, map_idx)).x;
#endif

    float depth_vs = (depth - orthoparams.w)/orthoparams.z;
    depth_vs = pow(abs(depth_vs/max_far), 10);
    return vec4(depth_vs, depth_vs, depth_vs, 1);
}

void main()
{
    vec4 colors[_CASCADE_CNT_];
    for (int i = 0; i < _CASCADE_CNT_; i++) {
        colors[i] = get_view_depth(c_orthoparams[i], c_max_far[i], 
            vec2(vso_coord.x, vso_coord.y), i);
    }

    pso_cs0 = colors[0];
    pso_cs1 = colors[1];
    pso_cs2 = colors[2];
}


