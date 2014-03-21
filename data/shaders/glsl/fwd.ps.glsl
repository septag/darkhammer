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

in vec3 vso_norm_ws;
in vec2 vso_coord0;
out vec4 pso_color;
    
layout(std140) uniform cb_mtl
{
   vec4 c_mtl_ambientclr; 
   vec4 c_mtl_diffuseclr;
};
 
#if defined(_DIFFUSEMAP_)
uniform sampler2D s_mtl_diffusemap;
#endif
  
void main()
{
   vec4 albedo = c_mtl_diffuseclr;
 
#if defined(_DIFFUSEMAP_)
   albedo *= texture(s_mtl_diffusemap, vso_coord0);
#endif
 
   vec3 light_dir = vec3(0.0f, -0.5f, 1.0f);
   light_dir = normalize(light_dir);
    
   vec3 norm_ws = normalize(vso_norm_ws);
    
   float diff_term = max(0, dot(-light_dir, norm_ws));
   vec3 color = diff_term*albedo.rgb + c_mtl_ambientclr.rgb*albedo.rgb;
   pso_color = vec4(color, 1);
}
 
