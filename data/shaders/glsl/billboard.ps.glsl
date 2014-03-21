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
in gso
{
    vec2 coord;
    vec4 color;
} i;

out vec4 pso_color;

/* texture */
uniform sampler2D s_billboard;

/* */
void main()
{
    pso_color = texture(s_billboard, i.coord) * i.color;
}
