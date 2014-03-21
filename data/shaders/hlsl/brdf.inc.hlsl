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

/* calculations for Microfacet BRDF Torrance-Sparrow (Lazarov11)
 * f(l, v) = f(v, h)*G(l, v, h)*D(h)/4(n.l)(n.v)
 * F(v, h) = schlick fresnel (http://en.wikipedia.org/wiki/Schlick's_approximation)
 * (1/4)*D(h) = normalized blinn-phong
 * V(l, v, h) = visiblity term G(l, v, h)/(n.l)*(n.v) - Schick-Smith
 */

static const float PI = 3.14159265f;
static const float PI_OVER_2 = PI/2.0f;
static const float PI_OVER_4 = PI/4.0f;
static const float PI_MUL_8 = PI*8.0f;

float3 calc_fresnel_schlick(float3 hv, float3 vv, float3 spec_clr)
{
    float f_base = 1.0f - dot(hv, vv);
    float f_exp = f_base*f_base*f_base*f_base*f_base;
    return spec_clr + (1 - spec_clr)*f_exp;
}

/* gloss should be [1, 8192] */
/* spec_coeff is for specular normalization */
float calc_blinnphong_term(float3 hv, float3 norm, float gloss, float spec_coeff)
{
    return spec_coeff * pow(max(0, dot(hv, norm)), gloss);
}

float calc_vis_term(float n_dot_l, float3 norm, float3 vv, float gloss, float vis_coeff)
{
    float k = 1 - vis_coeff;
    float inv_v = (n_dot_l*k + vis_coeff)*(max(0, dot(norm, vv))*k + vis_coeff);
    return 1.0f/inv_v;
}

float3 calc_ambient_spherical(float3 norm, float3 skyv, float3 ambient_sky, float3 ambient_ground)
{
    /* hemispheric ambient */
    float hem_f = dot(norm, skyv)*0.5f + 0.5f;
    return lerp(ambient_ground, ambient_sky, hem_f);
}

float calc_spec_coeff(float gloss)
{
    return ((gloss + 2.0f)/PI_MUL_8);
}

float3 calc_diffuse(float3 albedo)
{
    return albedo/PI;
}

float calc_vis_coeff(float gloss)
{
    return 1.0f / sqrt(PI_OVER_4*gloss + PI_OVER_2);
}

