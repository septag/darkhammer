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

/* texture */
Texture2D<float4> t_billboard;
SamplerState s_billboard;

struct gso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
    float4 color : TEXCOORD1;
};

/* */
float4 main(gso input) : SV_Target0
{
    return t_billboard.Sample(s_billboard, input.coord) * input.color;
}
