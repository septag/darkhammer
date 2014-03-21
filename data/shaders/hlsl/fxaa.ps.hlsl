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
/** 
 * parameters description:
 * QUALITY_SUBPIX : 
 * Choose the amount of sub-pixel aliasing removal. 
 * 		1.00 - upper limit (softer)
 * 		0.75 - default amount of filtering
 * 		0.50 - lower limit (sharper, less sub-pixel aliasing removal)
 * 		0.25 - almost off
 * 		0.00 - completely off
 * QUALITY_EDGE_THRESHOLD :
 * The minimum amount of local contrast required to apply algorithm.
 * 		0.333 - too little (faster)
 * 		0.250 - low quality
 * 		0.166 - default
 * 		0.125 - high quality 
 * 		0.063 - overkill (slower)
 * QUALITY_EDGE_THRESHOLD_MIN
 * Trims the algorithm from processing darks.
 * 		0.0833 - upper limit (default, the start of visible unfiltered edges)
 * 		0.0625 - high quality (faster)
 * 		0.0312 - visible limit (slower)
 */
#define QUALITY_SUBPIX 0.75f
#define QUALITY_EDGE_THRESHOLD 0.166f
#define QUALITY_EDGE_THRESHOLD_MIN 0.0833f

struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
};

/* textures */
Texture2D<float4> t_rgbl;
SamplerState s_rgbl;

/* uniforms */
float2 c_texelsize;

/* */
float4 main(vso input) : SV_Target0
{
    FxaaTex t;
    t.smpl = s_rgbl;
    t.tex = t_rgbl; /* should be in gamma-space */
    return FxaaPixelShader(
        input.coord, 
        0,  /* not-used */
        t,
        t, t, /* not-used */
        c_texelsize,
        0, 0, 0, /* not-used */
        QUALITY_SUBPIX,
        QUALITY_EDGE_THRESHOLD,
        QUALITY_EDGE_THRESHOLD_MIN,
        0, 0, 0, 0);
}
