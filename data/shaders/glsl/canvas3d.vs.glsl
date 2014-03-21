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

layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord;

#if defined(_TEXTURE_)
out vec2 vso_coord;
#endif

uniform mat4 c_viewproj;
uniform mat3x4 c_world;

void main()
{
    vec4 pos = vec4(vsi_pos * c_world, 1);
    gl_Position = pos * c_viewproj;
#if defined(_TEXTURE_)
    vso_coord = vsi_coord;
#endif
}

