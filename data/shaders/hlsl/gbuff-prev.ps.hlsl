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
    float2 coord : TEXCOORD0;
};


#if defined(_VIEWNORMALS_)
/* normals view */
SamplerState s_viewmap;
Texture2D<float2> t_viewmap;

float4 main(vso i) : SV_Target0
{
    float2 norm_enc = t_viewmap.Sample(s_viewmap, i.coord);
    float3 norm = normal_decode_spheremap(norm_enc);
    return float4(norm, 1.0f);
}
#elif defined(_VIEWALBEDO_)
/* albedo view */
SamplerState s_viewmap;
Texture2D<float4> t_viewmap;

float4 main(vso i) : SV_Target0
{
    float3 albedo = t_viewmap.Sample(s_viewmap, i.coord).rgb;
    return float4(sqrt(albedo), 1.0f);	/* roughly transform color from linear to gamma */
}
#elif defined(_VIEWSPECULARMUL_)
/* specular multiplier */
SamplerState s_viewmap;
Texture2D<float4> t_viewmap;

float4 main(vso i) : SV_Target0
{
    float spec = t_viewmap.Sample(s_viewmap, i.coord).a;
    return float4(spec, spec, spec, 1.0f);
}
#elif defined(_VIEWNORMALSNOMAP_)
/* normal w/o mapping */
SamplerState s_viewmap;
Texture2D<float4> t_viewmap;

float4 main(vso i) : SV_Target0
{
    float2 norm_enc = t_viewmap.Sample(s_viewmap, i.coord).xy;
    float3 norm = normal_decode_spheremap(norm_enc);
    return float4(norm, 1.0f);
}
#elif defined(_VIEWDEPTH_)
/* normal w/o mapping */
SamplerState s_viewmap;
Texture2D<float> t_viewmap;
float2 c_camprops;  /* x: near, y:far */

float4 main(vso i) : SV_Target0
{
    float depth_zbuff = t_viewmap.Sample(s_viewmap, i.coord);
    float depth = depth_linear(depth_zbuff, c_camprops.x, c_camprops.y);
    depth *= (c_camprops.y*0.01f);
    return float4(depth, depth, depth, 1.0f);
}
#elif defined(_VIEWMTL_)
/* mtl preview with random generated colors */
Texture2D<uint2> s_viewmap;
float c_mtlmax;

float4 main(vso i) : SV_Target0
{
    int3 coord2d = int3(i.pos.xy, 0);

    uint2 mtl_enc = s_viewmap.Load(coord2d);
    uint mtl_idx;
    float gloss;
    mtl_decode(mtl_enc, mtl_idx, gloss);
    float fmtlidx = (float)mtl_idx;
    float value = fmtlidx/c_mtlmax;
    return color_generate(value);
}
#elif defined(_VIEWGLOSS_)
Texture2D<uint2> s_viewmap;

float4 main(vso i) : SV_Target0
{
    int3 coord2d = int3(i.pos.xy, 0);

    uint2 mtl_enc = s_viewmap.Load(coord2d);
    float gloss = (float)mtl_enc.y / UINT16_MAX;
    return float4(gloss, gloss, gloss, 1);
}
#else
#error "no valid preprocessor is set"
#endif
