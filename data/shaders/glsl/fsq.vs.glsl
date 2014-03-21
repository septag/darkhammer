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

/**
 * used for fullscreen quad rendering
 * all shaders that are postfx (or use some kind of fullscreen effect) should use this vertex-shader
 * and output should always be like 'vso' (pos, coord)
 */

layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord;

out vec2 vso_coord;

void main()
{
    gl_Position = vsi_pos;
    vso_coord = vec2(vsi_coord.x, 1 - vsi_coord.y);
}
