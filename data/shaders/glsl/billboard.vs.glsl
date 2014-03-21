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
layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_TEXCOORD2) in vec4 vsi_coord; /* rectangular coord of the quad */
layout(location = INPUT_ID_TEXCOORD3) in vec4 vsi_billboard; /* x = half-width, y = half-height, z=rotation */
layout(location = INPUT_ID_COLOR) in vec4 vsi_color;

out vso {
	vec4 pos_ws;
	vec4 coord;
	vec4 billboard;
	vec4 color;
} o;

/* uniforms */
uniform mat3x4 c_world;

/* */
void main()
{
    o.pos_ws = vec4(vsi_pos * c_world, 1);
    o.coord = vsi_coord;
    o.billboard = vsi_billboard;
    o.color = vsi_color;
}
