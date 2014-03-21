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

/* input */
in vec2 vso_coord;
in vec3 vso_norm_vs;

#if defined(_NORMALMAP_)
in vec3 vso_tangent_vs;
in vec3 vso_binorm_vs;
#endif

/* output */
layout(location=0) out vec4 pso_g1; /* albedo(rgb) + specular mul (a) */
layout(location=1) out vec2 pso_g2; /* normal (with mapping) encoded (rg) */
layout(location=2) out uvec2 pso_g3; /* material buffer */
layout(location=3) out vec4 pso_g4; /* normal (w/o mapping) encoded (rg) */

/* constants */
uniform uint c_mtlidx;
uniform float c_gloss;  /* normalized [0, 1] */

/* textures */
#if defined(_DIFFUSEMAP_)
uniform sampler2D s_mtl_diffusemap;
#endif

#if defined(_NORMALMAP_)
uniform sampler2D s_mtl_normalmap;
#endif

#if defined(_ALPHAMAP_)
uniform sampler2D s_mtl_alphamap;
#endif

#if defined(_GLOSSMAP_)
uniform sampler2D s_mtl_alphamap;
#endif

void main()
{
#if defined(_ALPHAMAP_)
    float alpha = texture(s_mtl_alphamap, vso_coord).x;
    if (alpha < 0.5f)
        discard;
#endif

#if defined(_DIFFUSEMAP_)
    pso_g1 = texture(s_mtl_diffusemap, vso_coord);
#else
    pso_g1 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
#endif

#if defined(_NORMALMAP_)
    vec3 norm_ts = vec3(texture(s_mtl_normalmap, vso_coord).xy*2.0f-1.0f, 0);
    norm_ts.z = sqrt(1.0f - dot(norm_ts.xy, norm_ts.xy)); /* calc 3rd component from the first two */
    /* transform to view-space */
    vec3 norm_vs = normalize(norm_ts.x*vso_tangent_vs + norm_ts.y*vso_binorm_vs + 
        norm_ts.z*vso_norm_vs);
    /* encode */
    pso_g2 = normal_encode_spheremap(norm_vs);
    pso_g4 = vec4(normal_encode_spheremap(normalize(vso_norm_vs)), 0, 0);
#else
    vec3 norm_vs = normalize(vso_norm_vs);
    pso_g2 = normal_encode_spheremap(norm_vs);
    pso_g4 = vec4(pso_g2, 0, 0);
#endif

    /* material */
#if defined(_GLOSSMAP_)
    float gloss = texture(s_mtl_glossmap, vso_coord).x * c_gloss;
#else
    float gloss = c_gloss;
#endif
    pso_g3 = mtl_encode(c_mtlidx, gloss);
}


