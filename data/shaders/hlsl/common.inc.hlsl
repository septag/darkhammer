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

#define EPSILON 0.00001f

float3 color_tolinear(float3 clr)
{
    return clr*clr; /* should be pow(clr, 2.2), but for performance we just settle with 2.0 */
}

float3 color_togamma(float3 clr)
{
    return sqrt(clr);   /* should be pow(clr, 0.45), but for performance we just settle with sqr */
}

/* RGBM encoding in order to prevent float render-targets
 * reference: http://graphicrants.blogspot.com/2009/04/rgbm-color-encoding.html
 * it is recommended to convert colors to gamma-space before encoding
 */
float4 color_torgbm(float3 clr)
{
    float4 rgbm;
    clr *= 1.0 / 6.0;
    rgbm.a = saturate( max(max(clr.r, clr.g), max(clr.b, 1e-6)) );
    rgbm.a = ceil(rgbm.a * 255.0) / 255.0;
    rgbm.rgb = clr / rgbm.a;
    return rgbm;
}

/* RGBM decoding, see color_torgbm */
float3 color_fromrgbm(float4 rgbm)
{
    return 6.0f * rgbm.rgb * rgbm.a;
}

float4 color_hsv_to_rgb(float h, float s, float v)
{
    int h_i = (int)floor(h*6.0f);
    float f = h*6.0f - h_i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f*s);
    float t = v * (1.0f - (1.0f - f)*s);
    [flatten]
    switch (h_i)    {
        case 0: return float4(v, t, p, 1.0f);
        case 1: return float4(q, v, p, 1.0f);
        case 2: return float4(p, v, t, 1.0f);
        case 3: return float4(p, q, v, 1.0f);
        case 4: return float4(t, p, v, 1.0f);
        case 5: return float4(v, p, q, 1.0f);
    }

    return float4(0, 0, 0, 1.0f);
}

float4 color_generate(float seed)
{
    static const float golden_ratio = 0.618033988749895f;
    float i;
    float h = modf(seed + golden_ratio, i);
    return color_hsv_to_rgb(h, 0.5f, 0.95f);
}

/* checks if the point is in sphere 
 * @param sphere (xyz)=center, (w)=radius
*/
int pos_insphere(float3 pos, float4 sphere)
{
	float3 d = pos - sphere.xyz;
	return dot(d, d) - sphere.w*sphere.w < EPSILON;    
}

/* virtual cube-mapping: converts a tex-coord on a cube-atlas, and returns cube-map coord
 * @param coord coordinates on 2d map
 * @param map_idx map to which surface of the cube 
 */
float3 coord_maptocube(float2 coord, int map_idx)
{
	static const float3 offsets[6] = {
		float3(0.5f, 0.5f, 0.5f), float3(-0.5f, 0.5f, -0.5f),
		float3(-0.5f, 0.5f, -0.5f), float3(-0.5f, -0.5f, 0.5f),
		float3(-0.5f, 0.5f, 0.5f), float3(0.5f, 0.5f, -0.5f)
	};

	static const float3 mulx[6] = {
		float3(0.0f, 0.0f, -1.0f), float3(0.0f, 0.0f, 1.0f), float3(1.0f, 0.0f, 0.0f),
		float3(1.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f)
	};

	static const float3 muly[6] = {
		float3(0.0f, -1.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), float3(0.0f, 0.0f, 1.0f),
		float3(0.0f, 0.0f, -1.0f), float3(0.0f, -1.0f, 0.0f), float3(0.0f, -1.0f, 0.0f)
	};

	return offsets[map_idx] + mulx[map_idx]*coord.x + muly[map_idx]*coord.y;
}

