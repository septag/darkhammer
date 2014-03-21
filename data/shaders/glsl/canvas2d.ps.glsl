/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
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

/* vertex outputs */
in vso_t {
    vec4 color;
    vec2 coord;
} vso;

out vec4 pso_color;

uniform int c_type;
uniform sampler2D s_bmp;

void main()
{
    vec4 color = vso.color;
    
    if (c_type == 1)
        color *= texture(s_bmp, vso.coord);
    
    pso_color = color;
}
