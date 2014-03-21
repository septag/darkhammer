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
    float3 viewray : TEXCOORD1;
};

/* textures */
Texture2D<float> t_depth;
SamplerState s_depth;

#if !defined(_D3D10_)
Texture2DArray<float> t_shadowmap;
#else
TextureCube<float> t_shadowmap;
#endif
SamplerComparisonState s_shadowmap;

/* constants */
static const float4 g_colors[4] = {
    float4(1.0f, 0.0f, 0.0f, 1.0f),
    float4(0.0f, 1.0f, 0.0f, 1.0f),
    float4(0.0f, 0.0f, 1.0f, 1.0f),
    float4(1.0f, 1.0f, 0.0f, 1.0f)
};

static const float2 g_kernel_poisson[4] = {
    float2( -0.94201624f, -0.39906216f),
    float2( 0.94558609f, -0.76890725f),
    float2(-0.094184101f, -0.92938870f),
    float2(0.34495938f, 0.29387760f)
};

static const float g_split_weights[4] = {
    1.0f, 
    0.6f,
    0.3f,
    0.1f
};

/* uniforms */
float4 c_cascades_vs[_CASCADE_CNT_];   /* spheres for cascades */
float4x4 c_shadow_mats[_CASCADE_CNT_];
float4 c_projparams;

#if !defined(_D3D10_)
float shadow_1tap(float3 shadow_vect, int map_idx)
{
    return t_shadowmap.SampleCmpLevelZero(s_shadowmap, 
        float3(shadow_vect.xy, map_idx), shadow_vect.z);
}

float shadow_4taps(float3 shadow_vect, int map_idx)
{
    static const float2 texel = float2(1.0f/700.0f, 1.0f/700.0f);
    float2 texel_size = texel*g_split_weights[map_idx];
    float occ = 0;

    [unroll]
    for (int i = 0; i < 4; i++) {
        occ += t_shadowmap.SampleCmpLevelZero(s_shadowmap, 
            float3(shadow_vect.xy + g_kernel_poisson[i]*texel_size, map_idx), shadow_vect.z);
    }

    return occ*0.25f;
}
#else
float shadow_1tap(float3 shadow_vect, int map_idx)
{
    [flatten]
    if (min(shadow_vect.x, shadow_vect.y) > 0.0f && max(shadow_vect.x, shadow_vect.y) < 1.0f) {
        return t_shadowmap.SampleCmpLevelZero(s_shadowmap, coord_maptocube(shadow_vect.xy, map_idx),
            shadow_vect.z);
    }   else    {
        return 0;
    }
}

float shadow_4taps(float3 shadow_vect, int map_idx)
{
    static const float2 texel = float2(1.0f/700.0f, 1.0f/700.0f);
    float2 texel_size = texel*g_split_weights[map_idx];
    float occ = 0;

    [unroll]
    for (int i = 0; i < 4; i++) {
        float2 coord = shadow_vect.xy + g_kernel_poisson[i]*texel_size;
        [flatten]
        if (min(coord.x, coord.y) > 0.0f && max(coord.x, coord.y) < 1.0f) {
            occ += t_shadowmap.SampleCmpLevelZero(s_shadowmap, coord_maptocube(coord, map_idx), 
                shadow_vect.z);
        }
    }

    return occ*0.25f;
}
#endif

float4 main(vso i) : SV_Target0
{
    /* reconstruct position */
    float depth = t_depth.Sample(s_depth, i.coord);
    float depth_vs = c_projparams.w / (depth - c_projparams.z);    /* view depth */
    float3 pos_vs = depth_vs * i.viewray;
    float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);

    [unroll]
    for (int i = 0; i < _CASCADE_CNT_; i++)  {
        if (pos_insphere(pos_vs, c_cascades_vs[i]))  {
            float4 shadow = mul(float4(pos_vs, 1), c_shadow_mats[i]);
            float occ = shadow_4taps(shadow.xyz/shadow.w, i);
#if defined(_PREVIEW_)
            color = float4(g_colors[i].xyz*occ, 1);
#else
            color = float4(occ, occ, occ, 1);
#endif
            break;
        }
    }

    return color;
}
