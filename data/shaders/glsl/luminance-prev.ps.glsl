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

/* */
void main()
{
    float lum = exp(textureLod(s_tex, vso_coord, 0).x);
    pso_color = vec4(lum, lum, lum, 1);
}

