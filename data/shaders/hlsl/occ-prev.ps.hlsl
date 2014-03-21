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

struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
};

/* textures */
Texture2D<float> t_depth;
SamplerState s_depth;

#if defined(_EXTRA_)
Texture2D<float> t_depth_ext;
SamplerState s_depth_ext;
#endif

/* uniforms */
float2 c_camprops;  /* x = near, y = far */

/* */
float4 main(vso input) : SV_Target0
{
    float depth_zbuff = t_depth.SampleLevel(s_depth, input.coord, 0);
    float depth = (2.0f * c_camprops.x)/(c_camprops.y  + c_camprops.x - 
        depth_zbuff*(c_camprops.y - c_camprops.x));
    float4 color = float4(depth, depth, depth, 1);

#if defined(_EXTRA_)
    float depth_zbuff_ext = t_depth_ext.SampleLevel(s_depth_ext, input.coord, 0);
    float depth_ext = (2.0f * c_camprops.x)/(c_camprops.y  + c_camprops.x - 
        depth_zbuff_ext*(c_camprops.y - c_camprops.x));
    [flatten]
    if (depth_ext < depth)
        color *= float4(depth_ext, 0, 0, 1);
#endif

    return color;
}
