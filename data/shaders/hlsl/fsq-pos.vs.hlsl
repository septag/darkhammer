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

 /**
 * used for fullscreen quad rendering plus it passes information for position reconstruction
 */

struct vsi
{
    float4 pos : POSITION;
    float2 coord : TEXCOORD0;
};

struct vso
{
    float4 pos : SV_Position;
    float2 coord : TEXCOORD0;
    float3 viewray : TEXCOORD1;
};

/* projection params */
float4 c_projparams;
/* camera far distance */
float c_camfar;

vso main(vsi i)
{
    vso o;
    o.pos = i.pos;
    o.coord = i.coord;

    /* input is four corners of full-screen rect (in proj-space). convert them to view */
    float3 pos_vs = pos_proj_toview(i.pos.xyz, c_projparams, c_camfar);

    /* in pixel shader, linear_depth(view-depth) is multiplied by this value 
     * so after we multiply lin-depth (0~camfar) into viewray, z will be view.z, and 
     * xy=(lindepth/viewray.z)*viewray.xy */
    o.viewray = float3(pos_vs.xy / pos_vs.z, 1.0f);

    return o;
}
