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

uniform sampler2D s_alphamap;
in vec2 gso_coord;
out vec4 pso_color;

void main()
{
    float a = texture(s_alphamap, gso_coord).a;
    if (a < 1.0f)
        discard;
    pso_color = vec4(1, 1, 1, 1);
}
