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
uniform sampler2D s_tex;

/* uniforms */
uniform vec4 c_kernel[_KERNELSIZE_];  /* xy: coord-offset, z:weight */
const int kernel_half = _KERNELSIZE_/2;

/* */
void main()
{
    vec4 color = vec4(0, 0, 0, 0);

    for (int i = -kernel_half; i <= kernel_half; i++)   {
        int idx = i + kernel_half;
        color += textureLod(s_tex, vso_coord + c_kernel[idx].xy, 0) * c_kernel[idx].z;
    }

    pso_color = vec4(color.xyz, 1);
}
