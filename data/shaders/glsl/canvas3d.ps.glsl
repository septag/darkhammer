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

out vec4 pso_color;
#if defined(_TEXTURE_)
in vec2 vso_coord;
uniform sampler2D s_tex;
#endif

uniform vec4 c_color;

void main() 
{
#if defined(_TEXTURE_)
    pso_color = texture(s_tex, vso_coord);
#else
    pso_color = c_color;
#endif
}
