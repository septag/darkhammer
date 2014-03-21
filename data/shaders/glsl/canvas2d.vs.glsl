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

/* note: input position data should be in viewport space (pixels) */

/* vertex inputs */
layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_COLOR) in vec4 vsi_color;
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord;

/* vertex outputs */
out vso_t {
    vec4 color;
    vec2 coord;
} vso;

uniform vec2 c_rtsz;

void main()
{
//
    /* transform from clip-space (viewport) into proj-space */
    gl_Position = vec4(
        (vsi_pos.x / c_rtsz.x)*2 - 1,
        1 - (vsi_pos.y / c_rtsz.y)*2,
        0.0f, 1.0f);

    /* */
    vso.color = vsi_color;
    vso.coord = vec2(vsi_coord.x, -vsi_coord.y);
}

