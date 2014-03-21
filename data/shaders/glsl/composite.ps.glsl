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

uniform sampler2D s_color;

#if defined(_DEPTHBUFFER_)
uniform sampler2D s_depth;
#endif

/* multiply texture #1 */
#if defined(_ADD1_)
uniform sampler2D s_add1;
#endif

/* multiply texture #2 */
#if defined(_MUL1_)
uniform sampler2D s_mul1;
#endif

void main()
{
    vec4 color = textureLod(s_color, vso_coord, 0);

#if defined(_ADD1_)
    vec4 add1 = textureLod(s_add1, vso_coord, 0);
    color += add1;
#endif

#if defined(_MUL2_)
    vec4 mul1 = textureLod(s_add1, vso_coord, 0);
    color *= mul2;
#endif
    
#if defined(_DEPTHBUFFER_)
    gl_FragDepth = textureLod(s_depth, vso_coord, 0).x;
#endif

    pso_color = vec4(color.xyz, 1);
}

