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

struct vso
{
    float4 pos : SV_Position;
#if defined(_TEXTURE_)
    float2 coord : TEXCOORD0;
#endif
};

float4 c_color;

/* textures */
Texture2D<float4> t_tex;
SamplerState s_tex;

float4 main(vso input) : SV_target0
{
#if defined(_TEXTURE_)
    return t_tex.Sample(s_tex, input.coord)*c_color;
#else
    return c_color;
#endif
}
