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

struct pso
{
    float4 color : SV_Target0;
#if _DEPTHBUFFER_
    float depth : SV_Depth;
#endif
};

Texture2D<float4> t_color;
SamplerState s_color;

#if defined(_DEPTHBUFFER_)
Texture2D<float> t_depth;
SamplerState s_depth;
#endif

/* multiply texture #1 */
#if defined(_ADD1_)
Texture2D<float4> t_add1;
SamplerState s_add1;
#endif

/* multiply texture #2 */
#if defined(_MUL1_)
Texture2D<float4> t_mul1;
SamplerState s_mul1;
#endif

pso main(vso input)
{
    pso o;
    float4 color = t_color.SampleLevel(s_color, input.coord, 0);

#if defined(_ADD1_)
    float4 add1 = t_add1.SampleLevel(s_add1, input.coord, 0);
    color += add1;
#endif

#if defined(_MUL2_)
    float4 mul1 = t_add1.SampleLevel(s_add1, input.coord, 0);
    color *= mul2;
#endif
    
#if defined(_DEPTHBUFFER_)
    o.depth = t_depth.SampleLevel(s_depth, input.coord, 0);
#endif

    o.color = float4(color.xyz, 1);
    return o;
}

