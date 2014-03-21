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

in vec2 vso_coord;

out vec4 pso_color;

/* textures */
uniform sampler2D s_lum_target;
uniform sampler2D s_lum_adapted;

/* uniforms */
uniform float c_elapsedtm;  /* in seconds */
uniform float c_lastmip;
uniform vec2 c_lum_range;   /* x = lum_min, y = lum_max */

/* */
void main()
{
    float lum_target = exp(textureLod(s_lum_target, vec2(0.5f, 0.5f), c_lastmip).x);
    lum_target = clamp(lum_target, c_lum_range.x, c_lum_range.y);
    float lum_adapt = textureLod(s_lum_adapted, vec2(0.5f, 0.5f), 0).x;
    /* move closer to target lumnance 2% each frame (30fps) */
    float lum_final = lum_adapt + (lum_target - lum_adapt)*(1 - pow(0.98f, 30*c_elapsedtm));
    pso_color = vec4(lum_final, 0, 0, 0);
}
