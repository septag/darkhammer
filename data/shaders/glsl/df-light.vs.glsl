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

/* vertex-shader used for local-area deferred lighting */

/* input/output */
layout(location = INPUT_ID_POSITION) in vec4 vsi_pos;
layout(location = INPUT_ID_TEXCOORD0) in vec2 vsi_coord;

out vec2 vso_coord;
out vec3 vso_viewray;
flat out uint vso_tile_id;

/* uniforms */
uniform vec4 c_projparams;
uniform float c_camfar;
uniform vec2 c_rtsz; /* x:rt-width, y:rt-height */
uniform uvec3 c_grid;  /* x:col-cnt ,y: row-cnt, z:cell-size*/
uniform uint c_celloffset;

void main()
{
    /* input position is in screen-space */
    /* instance_idx is the actual tile_id */
    uint tile_id = uint(gl_InstanceID) + c_celloffset;
    uint x = tile_id % c_grid.x;
    uint y = (tile_id - x) / c_grid.x;
    uvec2 offset = uvec2(x*c_grid.z, y*c_grid.z);

    /* convert to projection */
    vec3 pos_prj = vec3(
        ((vsi_pos.x + offset.x)/ c_rtsz.x)*2 - 1,
        1 - ((vsi_pos.y + offset.y)/ c_rtsz.y)*2,
        1.0f);

    /* calculate view-ray */
    vec3 pos_vs = pos_proj_toview(pos_prj.xyz, c_projparams, c_camfar);
    vso_viewray = vec3(pos_vs.xy / pos_vs.z, 1.0f);

    /* calculate coords */
    vec2 coord = vsi_coord + vec2(float(x)/c_rtsz.x, float(y)/c_rtsz.y);

    gl_Position = vec4(pos_prj, 1.0f);
    vso_coord = coord;
    vso_tile_id = uint(gl_InstanceID);
}


