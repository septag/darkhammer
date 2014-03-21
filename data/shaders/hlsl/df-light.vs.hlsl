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

struct vsi
{
    uint instance_idx : SV_InstanceID;
    float4 pos : POSITION;
    float2 coord : TEXCOORD0;
};

struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
    float3 viewray : TEXCOORD1;
    nointerpolation uint tile_id : TEXCOORD2;
};

/* uniforms */
float4 c_projparams;
float c_camfar;
float2 c_rtsz; /* x:rt-width, y:rt-height */
uint3 c_grid;  /* x:col-cnt ,y: row-cnt, z:cell-size*/
uint c_celloffset;

vso main(vsi input)
{
    vso o;

    /* input position is in screen-space */
    /* instance_idx is the actual tile_id */
    uint tile_id = input.instance_idx + c_celloffset;
    uint x = tile_id % c_grid.x;
    uint y = (tile_id - x) / c_grid.x;
    uint2 offset = uint2(x*c_grid.z, y*c_grid.z);

    /* convert to projection */
    float3 pos_prj = float3(
        ((input.pos.x + (float)offset.x)/ c_rtsz.x)*2 - 1,
        1 - ((input.pos.y + (float)offset.y)/ c_rtsz.y)*2,
        1.0f);

    /* calculate view-ray */
    float3 pos_vs = pos_proj_toview(pos_prj.xyz, c_projparams, c_camfar);
    o.viewray = float3(pos_vs.xy / pos_vs.z, 1.0f);

    /* calculate coords */
    float2 coord = input.coord + float2(float(x)/c_rtsz.x, float(y)/c_rtsz.y);

    o.pos = float4(pos_prj, 1.0f);
    o.coord = coord;
    o.tile_id = input.instance_idx;
    return o;
}


