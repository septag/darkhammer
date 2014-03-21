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

/* input */
struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
    float3 norm_vs : TEXCOORD1;

#if defined(_NORMALMAP_)
    float3 tangent_vs : TEXCOORD2;
    float3 binorm_vs : TEXCOORD3;
#endif
};

/* output */
struct pso
{
    float4 g1 : SV_Target0;  /* albedo(rgb) + specular multiplier (a) */
    float2 g2 : SV_Target1;  /* normal (with mapping) encoded (rg) */
    uint2 g3 : SV_Target2;    /* material buffer */
    float4 g4 : SV_Target3;  /* normal (w/o mapping) encoded (rg) */
};

/* constants */
uint c_mtlidx;
float c_gloss;  /* normalized [0, 1] */

/* textures */
#if defined(_DIFFUSEMAP_)
SamplerState s_mtl_diffusemap;
Texture2D<float4> t_mtl_diffusemap;
#endif

#if defined(_NORMALMAP_)
SamplerState s_mtl_normalmap;
Texture2D<float2> t_mtl_normalmap;
#endif

#if defined(_ALPHAMAP_)
SamplerState s_mtl_alphamap;
Texture2D<float> t_mtl_alphamap;
#endif

#if defined(_GLOSSMAP_)
SamplerState s_mtl_glossmap;
Texture2D<float> t_mtl_glossmap;
#endif

pso main(vso i)
{
    pso o;

#if defined(_ALPHAMAP_)
    clip(t_mtl_alphamap.Sample(s_mtl_alphamap, i.coord) - 0.5f);
#endif
    
#if defined(_DIFFUSEMAP_)
    o.g1 = t_mtl_diffusemap.Sample(s_mtl_diffusemap, i.coord);
#else
    o.g1 = float4(1.0f, 1.0f, 1.0f, 1.0f);
#endif

#if defined(_NORMALMAP_)
    float3 norm_ts = float3(t_mtl_normalmap.Sample(s_mtl_normalmap, i.coord)*2-1, 0);
    norm_ts.z = sqrt(1.0f - dot(norm_ts.xy, norm_ts.xy)); /* calc 3rd component from the first two */
    /* transform to view-space */
    float3 norm_vs = normalize(norm_ts.x*i.tangent_vs + norm_ts.y*i.binorm_vs + norm_ts.z*i.norm_vs);
    /* encode */
    o.g2 = normal_encode_spheremap(norm_vs);
    o.g4 = float4(normal_encode_spheremap(normalize(i.norm_vs)), 0, 0);
#else
    float3 norm_vs = normalize(i.norm_vs);
    o.g2 = normal_encode_spheremap(norm_vs);
    o.g4 = float4(o.g2, 0, 0);
#endif

    /* material */
#if defined(_GLOSSMAP_)
    float gloss = t_mtl_glossmap.Sample(s_mtl_glossmap, i.coord) * c_gloss;
#else
    float gloss = c_gloss;
#endif

    o.g3 = mtl_encode(c_mtlidx, gloss);
    
    return o;
}


