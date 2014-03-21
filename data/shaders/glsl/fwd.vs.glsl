
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

#ifndef _MAX_INSTANCES_
#error "_MAX_INSTANCES is not defined"
#endif
 
layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_NORMAL) in vec3 vsi_norm;
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord0;
 
out vec3 vso_norm_ws;
out vec2 vso_coord0;
 
layout(std140) uniform cb_frame
{ 
   mat4 c_viewproj;
};

layout(std140) uniform cb_xforms
{
    mat3x4 c_mats[_MAX_INSTANCES_];
};

void main()
{
   vec4 pos_ws = vec4(vsi_pos * c_mats[gl_InstanceID], 1);
   
   vso_norm_ws = vsi_norm * mat3(c_mats[gl_InstanceID]);
   vso_coord0 = vec2(vsi_coord0.x, -vsi_coord0.y);
   gl_Position = pos_ws * c_viewproj;
}
 
 
 
 
