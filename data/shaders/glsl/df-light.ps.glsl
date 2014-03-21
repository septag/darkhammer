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

/* inputs */
in vec2 vso_coord;
in vec3 vso_viewray;
#if defined(_LOCAL_LIGHTING_)
flat in uint vso_tile_id;
#endif

/* outputs */
out vec4 pso_color;

/* global constants */
uniform vec4 c_projparams;
uniform float c_global_ambient;

/* material */
struct mtl
{
    vec4 ambient_clr;
    vec4 diff_clr;
    vec4 spec_clr;
    vec4 emissive_clr;
    vec4 props;    /* x = spec_exp(glossiness), y = spec_intensity, z=reflectance */
};

/* materials */
uniform samplerBuffer tb_mtls;

/* material fetch */
mtl get_mtl(uint idx)
{
    mtl m;
    int offset = int(idx*uint(5));
    m.ambient_clr = texelFetch(tb_mtls, offset);
    m.diff_clr = texelFetch(tb_mtls, offset + 1);
    m.spec_clr = texelFetch(tb_mtls, offset + 2);
    m.emissive_clr = texelFetch(tb_mtls, offset + 3);
    m.props = texelFetch(tb_mtls, offset + 4);
    return m;
}

#if defined(_LOCAL_LIGHTING_)

#define LIGHT_TYPE_POINT 2.0f
#define LIGHT_TYPE_SPOT 3.0f

/* light */
struct local_light
{
    vec4 type;   /* type.x: point=2, spot=3 */
    vec4 pos_vs; /* position (view-space) */
    vec4 atten;  /* attenuations (x=near, y=far, z=cos(narrow), w=cos(wide) */
    vec4 dir_vs; /* direction (view-space) */
    vec4 color;  /* linear space color (premultiplied), a=intensity*/
};

struct local_light_tile
{
    uvec4 lightcnt;  /* x = cnt */
    uvec4 lightidxs[_MAX_LIGHT_INDEXES_/4];
};

layout(std140) uniform cb_light
{
    local_light_tile c_tiles[_MAX_TILES_];
};

uniform samplerBuffer tb_lights;
#elif defined(_SUN_LIGHTING_)
uniform vec4 c_ambient_sky;
uniform vec4 c_ambient_ground;
uniform float c_ambient_intensity;
uniform vec3 c_skydir_vs;     /* direction of the sky (view-space) */

uniform vec3 c_lightdirinv_vs;   /* inverse direction (view-space) */
uniform vec4 c_lightcolor;    /* linear space color (premultiplied)) */
#endif

/* textures */
uniform sampler2D s_depth;
uniform sampler2D s_norm;
uniform sampler2D s_albedo;
uniform usampler2D s_mtl;
#if defined(_SUN_LIGHTING_)
uniform sampler2D s_shadows;
uniform sampler2D s_ssao;
#endif

/* functions */
vec3 calc_lit(vec3 diff_clr_over_pi, vec3 spec_clr, vec3 lv, vec3 vv, 
    vec3 norm, float gloss, vec4 light_clr, float vis_coeff, float spec_coeff)
{
    /* Microfacet BRDF Torrance-Sparrow (Lazarov11)
     * f(l, v) = c_diff/PI + f(v, h)*G(l, v, h)*D(h)/4(n.l)(n.v)
     */
    /* calculate base values (light-vector, half-vector, n_dot_l) */
    vec3 hv = normalize(vv + lv);
    float n_dot_l = max(0, dot(norm, lv));

    /* reflectance (specular) */
    vec3 fresnel = calc_fresnel_schlick(hv, vv, spec_clr);
    float vis_t = calc_vis_term(n_dot_l, norm, vv, gloss, vis_coeff);
    float blinnphong_t = calc_blinnphong_term(hv, norm, gloss, spec_coeff);
    vec3 spec_refl = fresnel*(vis_t*blinnphong_t);

    return (diff_clr_over_pi + spec_refl)*light_clr.rgb*n_dot_l;
}

#if defined(_LOCAL_LIGHTING_)
/* local light fetch */
local_light get_locallight(uint idx)
{
    local_light l;
    int offset = int(idx)*5;
    l.type = texelFetch(tb_lights, offset);
    l.pos_vs = texelFetch(tb_lights, offset + 1);
    l.atten = texelFetch(tb_lights, offset + 2);
    l.dir_vs = texelFetch(tb_lights, offset + 3);
    l.color = texelFetch(tb_lights, offset + 4);
    return l;
}

float calc_dist_atten(vec3 lv, float anear, float afar, float intensity, out float lv_len)
{
    lv_len = length(lv);
    float d = lv_len - anear;
    float t = clamp((d - anear)/(afar - anear), 0, 1);
    float att = pow(1-t, (1/intensity)*4.0f);
    return att;
}

float calc_angle_atten(vec3 ldir, vec3 lv, float anarrow, float awide)
{
    return smoothstep(awide, anarrow, dot(ldir, -lv));
}
#endif

void main()
{
    ivec2 coord2d = ivec2(gl_FragCoord.xy);

    /* reconstruct position */
    /* NOTE: depth buffer fetch in opengl is between [-1, 1] so we have to convert it to [0, 1] */
    float depth = texelFetch(s_depth, coord2d, 0).x*2.0 - 1.0f;
    if (depth == 1.0)
        discard;

    float depth_vs = c_projparams.w / (depth - c_projparams.z);    /* view depth */
    vec3 pos_vs = depth_vs * vso_viewray;

    /* material */
    uvec2 mtl_enc = texelFetch(s_mtl, coord2d, 0).xy;
    uint mtl_idx;
    float gloss;
    mtl_decode(mtl_enc, mtl_idx, gloss);
    mtl m = get_mtl(mtl_idx);

    /* reconstruct normal */
    vec2 norm_enc = texelFetch(s_norm, coord2d, 0).xy;
    vec3 norm_vs = normal_decode_spheremap(norm_enc);

    /* albedo */
    vec4 g1 = texelFetch(s_albedo, coord2d, 0);
    vec3 diff_albedo = g1.rgb * m.diff_clr.rgb;

    /* specular color */
    vec3 spec_clr = g1.a * m.spec_clr.rgb;

    /* prepare lighting coeffs */
    float vis_coeff = calc_vis_coeff(gloss);
    vec3 diff_clr = calc_diffuse(diff_albedo);
    float spec_coeff = calc_spec_coeff(gloss);
    vec3 vv = -normalize(pos_vs);/* view-vector */

#if defined(_SUN_LIGHTING_)
    /* light-vector */
    vec3 lv = c_lightdirinv_vs;

    vec3 lit_clr = calc_lit(
        diff_clr,
        spec_clr,
        lv, vv, norm_vs,
        gloss,
        c_lightcolor,
        vis_coeff, 
        spec_coeff);

    /* shadows/ssao */
    vec4 shadow = texelFetch(s_shadows, coord2d, 0);
    vec4 ssao = texelFetch(s_ssao, coord2d, 0);

    /* ambient */
    vec3 ambient = calc_ambient_spherical(norm_vs, c_skydir_vs, c_ambient_sky.xyz, 
        c_ambient_ground.xyz) * diff_albedo;
    ambient *= c_ambient_intensity;
    ambient *= ssao.xyz;
    
    /* final lit (considering shadows and ambient) */
    lit_clr *= shadow.xyz;
    lit_clr += ambient;
#elif defined(_LOCAL_LIGHTING_)
    vec3 lit_clr = vec3(0, 0, 0);

    local_light_tile tile = c_tiles[vso_tile_id];
    for (uint i = uint(0); i < tile.lightcnt.x; i++)    {
        uint vidx = i/uint(4);
        uint subidx = i % uint(4);
        uint lightidx = tile.lightidxs[vidx][subidx];
        local_light light = get_locallight(lightidx);

        /* light-vector */
        vec3 lv = light.pos_vs.xyz - pos_vs;

        /* attenuation */
        float lv_len;
        float atten = calc_dist_atten(lv, light.atten.x, light.atten.y, light.color.a, lv_len);

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

    pso_color = vec4(lit_clr, 1);    
}
