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
 * used for fullscreen quad rendering plus it passes information for position reconstruction
 */

layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord;

out vec2 vso_coord;
out vec3 vso_viewray;

uniform vec4 c_projparams;
uniform float c_camfar;

void main()
{
    /* input is four corners of full-screen rect (in proj-space). convert them to view */
    vec3 pos_vs = pos_proj_toview(vsi_pos.xyz, c_projparams, c_camfar);

    /* in pixel shader, linear_depth(view-depth) is multiplied by this value 
     * so after we multiply lin-depth (0~camfar) into viewray, z will be view.z, and 
     * xy=(lindepth/viewray.z)*viewray.xy */
    vso_viewray = vec3(pos_vs.xy/pos_vs.z, 1.0f);

    gl_Position = vsi_pos;
    vso_coord = vec2(vsi_coord.x, 1 - vsi_coord.y);
}
