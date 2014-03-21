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
    float3 viewray : TEXCOORD1;
#if defined(_LOCAL_LIGHTING_)
    nointerpolation uint tile_id : TEXCOORD2;
#endif
};

/* global constants */
float4 c_projparams;

#if defined(_SUN_LIGHTING_)
float4 c_ambient_sky;
float4 c_ambient_ground;
float c_ambient_intensity;
float3 c_skydir_vs;     /* direction of the sky (view-space) */

float3 c_lightdirinv_vs;   /* inverse direction (view-space) */
float4 c_lightcolor;    /* linear space color (premultiplied), a=intensity */
#endif

#if defined(_LOCAL_LIGHTING_)

#define LIGHT_TYPE_POINT 2.0f
#define LIGHT_TYPE_SPOT 3.0f

/* light */
struct local_light
{
    float4 type;   /* type.x: point=2, spot=3 */
    float4 pos_vs; /* position (view-space) */
    float4 atten;  /* attenuations (x=near, y=far, z=cos(narrow), w=cos(wide) */
    float4 dir_vs; /* direction (view-space) */
    float4 color;  /* linear space color (pre-multiplied) */
};

struct local_light_tile
{
    uint4 lightcnt;  /* x = cnt */
    uint4 lightidxs[_MAX_LIGHT_INDEXES_/4];
};

cbuffer cb_light
{
    local_light_tile c_tiles[_MAX_TILES_];
};

/* lights tbuffer (array of local_light) */
Buffer<float4> tb_lights;

/* local light fetch */
local_light get_locallight(uint idx)
{
    local_light l;
    int offset = int(idx)*5;
    l.type = tb_lights.Load(offset);
    l.pos_vs = tb_lights.Load(offset + 1);
    l.atten = tb_lights.Load(offset + 2);
    l.dir_vs = tb_lights.Load(offset + 3);
    l.color = tb_lights.Load(offset + 4);
    return l;
}
#endif

/* material */
struct mtl
{
    float4 ambient_clr;
    float4 diff_clr;
    float4 spec_clr;
    float4 emissive_clr;
    float4 props;       /* extra props */
};

/* materials tbuffer (array of mtl) */
Buffer<float4> tb_mtls;

/* material fetch */
mtl get_mtl(uint idx)
{
    mtl m;
    int offset = int(idx)*5;
    m.ambient_clr = tb_mtls.Load(offset);
    m.diff_clr = tb_mtls.Load(offset + 1);
    m.spec_clr = tb_mtls.Load(offset + 2);
    m.emissive_clr = tb_mtls.Load(offset + 3);
    m.props = tb_mtls.Load(offset + 4);
    return m;
}

/* textures */
Texture2D<float> s_depth;
Texture2D<float2> s_norm;
Texture2D<float4> s_albedo;
Texture2D<uint2> s_mtl;
#if defined(_SUN_LIGHTING_)
Texture2D<float4> s_shadows;
Texture2D<float4> s_ssao;
#endif

/* functions */
float3 calc_lit(float3 diff_clr_over_pi, float3 spec_clr, float3 lv, float3 vv, 
    float3 norm, float gloss, float4 light_clr, float vis_coeff, float spec_coeff)
{
    /* Microfacet BRDF Torrance-Sparrow (Lazarov11)
     * f(l, v) = c_diff/PI + f(v, h)*G(l, v, h)*D(h)/4(n.l)(n.v)
     */
    /* calculate base values (light-vector, half-vector, n_dot_l) */
    float3 hv = normalize(vv + lv);
    float n_dot_l = max(0, dot(norm, lv));

    /* reflectance (specular) */
    float3 fresnel = calc_fresnel_schlick(hv, vv, spec_clr);
    float vis_t = calc_vis_term(n_dot_l, norm, vv, gloss, vis_coeff);
    float blinnphong_t = calc_blinnphong_term(hv, norm, gloss, spec_coeff);
    float3 spec_refl = fresnel*(vis_t*blinnphong_t);

    return (diff_clr_over_pi + spec_refl)*light_clr.rgb*n_dot_l;
}

#if defined(_LOCAL_LIGHTING_)
float calc_dist_atten(float3 lv, float anear, float afar, float intensity, out float lv_len)
{
    lv_len = length(lv);
    float d = lv_len - anear;
    float t = saturate((d - anear)/(afar - anear));
    float att = pow(1-t, (1/intensity)*4.0f);
    return att;
}

float calc_angle_atten(float3 ldir, float3 lv, float anarrow, float awide)
{
    return smoothstep(awide, anarrow, dot(ldir, -lv));
}
#endif

float4 main(vso input) : SV_Target0
{
#if defined(_LOCAL_LIGHTING_)
    local_light_tile tile = c_tiles[input.tile_id];
    [flatten] 
    if (tile.lightcnt.x == 0)     
        discard;
#endif

    int3 coord2d = int3(input.pos.xy, 0);

    /* reconstruct position */
    float depth = s_depth.Load(coord2d);
    clip(0.999999f - depth);

    float depth_vs = c_projparams.w / (depth - c_projparams.z);    /* view depth */
    float3 pos_vs = depth_vs * input.viewray;

    /* material/gloss/a-term */
    uint2 mtl_enc = s_mtl.Load(coord2d);
    uint mtl_idx;
    float gloss;
    mtl_decode(mtl_enc, mtl_idx, gloss);
    mtl m = get_mtl(mtl_idx);

    /* reconstruct normal */
    float2 norm_enc = s_norm.Load(coord2d);
    float3 norm_vs = normal_decode_spheremap(norm_enc);

    /* albedo */
    float4 g1 = s_albedo.Load(coord2d);
    float3 diff_albedo = g1.rgb * m.diff_clr.rgb;

    /* specular color */
    float3 spec_clr = g1.a * m.spec_clr.rgb;

    /* prepare lighting coeffs */
    float vis_coeff = calc_vis_coeff(gloss);
    float3 diff_clr = calc_diffuse(diff_albedo);
    float spec_coeff = calc_spec_coeff(gloss);
    float3 vv = -normalize(pos_vs); /* view-vector */

#if defined(_SUN_LIGHTING_)
    /* light-vector */
    float3 lv = c_lightdirinv_vs;

    float3 lit_clr = calc_lit(
        diff_clr,
        spec_clr,
        lv, vv, norm_vs,
        gloss,
        c_lightcolor,
        vis_coeff, 
        spec_coeff);

    /* shadows/ssao */
    float4 shadow = s_shadows.Load(coord2d);
    float4 ssao = s_ssao.Load(coord2d);

    /* ambient */
    float3 ambient = calc_ambient_spherical(norm_vs, c_skydir_vs, c_ambient_sky.xyz, 
        c_ambient_ground.xyz) * diff_albedo;
    ambient *= c_ambient_intensity;
    ambient *= ssao.xyz;
    
    /* final lit (considering shadows and ambient) */
    lit_clr *= shadow.xyz;
    lit_clr += ambient;
#elif defined(_LOCAL_LIGHTING_)
    float3 lit_clr = float3(0, 0, 0);

    for (uint i = 0; i < tile.lightcnt.x; i++)    {
        uint vidx = i/4;
        uint subidx = i % 4;
        uint lightidx = tile.lightidxs[vidx][subidx];
        local_light light = get_locallight(lightidx);

        /* light-vector */
        float3 lv = light.pos_vs.xyz - pos_vs;

        /* attenuation */
        float lv_len;
        float atten = calc_dist_atten(lv, light.atten.x, light.atten.y, light.color.a, lv_len);

        [flatten]
        if (light.type.x == LIGHT_TYPE_SPOT)
            atten *= calc_angle_atten(light.dir_vs.xyz, lv, light.atten.z, light.atten.w);

        /* lit calc */
        lv /= lv_len;   /* normalize light-vect for light calc */
        lit_clr += atten * calc_lit(diff_clr,
            spec_clr,
            lv, vv, norm_vs,
            gloss, 
            light.color,
            vis_coeff, 
            spec_coeff);
    }
#endif

    return float4(lit_clr, 1);    
}
