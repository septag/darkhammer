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

/* convert from projection to view-space 
 * @param proj_params represents paramters of projection matrix 
 * @param cam_far view camera far distance 
 */
float3 pos_proj_toview(float3 pos_ps, float4 proj_params, float cam_far)
{
    pos_ps *= cam_far;
    return float3(pos_ps.xy, pos_ps.z - proj_params.w)/proj_params.xyz;
}

/* converts from zbuffer depth (1/z) to linear depth (between 0~1) */
float depth_linear(float depth_zbuffer, float nnear, float nfar)
{
    return (2.0f * nnear)/(nfar + nnear - depth_zbuffer*(nfar-nnear));
}

float depth_linear_p(float depth_zbuff, float4 projparams)
{
    /* Zbuff = (Zv*proj.m33 + proj.m43)/Zv : resolve Zv */
    return projparams.w / (depth_zbuff - projparams.z);
}

#if 0
/* reference: http://aras-p.info/texts/CompactNormalStorage.html (method #4)
 * encodes 3-component norm into 2 components using sphere-map method
 */
float2 normal_encode_spheremap(float3 norm)
{
    float2 enc = normalize(norm.xy) * sqrt(norm.z*0.5f + 0.5f);
	return (enc*0.5 + 0.5f);
}

/* reference: http://aras-p.info/texts/CompactNormalStorage.html (method #4)
 * decodes 2-component encoded norm to 3-component norm vector using sphere-mapping
 * @see normal_encode_spheremap
 */
float3 normal_decode_spheremap(float2 norm_enc)
{
	float4 nn = float4(norm_enc, 0, 0)*float4(2, 2, 0, 0) + float4(-1, -1, 1, -1);
	float l = dot(nn.xyz, -nn.xyw);
	nn.z = -l;
	nn.xy *= sqrt(l);
	return (nn.xyz*2 + float3(0, 0, 1));
}
#endif

float2 normal_encode_spheremap(float3 norm)
{
	float f = sqrt(-8*norm.z + 8);
	return norm.xy/f + 0.5f;
}

float3 normal_decode_spheremap(float2 norm_enc)
{
	float3 n;
	float2 fenc = norm_enc * 4 - 2;
	float f = dot(fenc, fenc);
	float g = sqrt(1 - f/4);
	n.xy = fenc * g;
	n.z = -(1 - f/2);
    return n;
}

#define UINT16_MAX 65535.0f

/* encoding gloss/material-index
/* gloss = normalized [0, 1] */
uint2 mtl_encode(uint mtl_idx, float gloss)
{
    return uint2(mtl_idx, uint(gloss*UINT16_MAX));
}

/* input: encoded mtl buffer (RG_UINT16)
 * output: mtl_idx, gloss [1, 8192] */
void mtl_decode(uint2 mtl, out uint mtl_idx, out float gloss)
{
    mtl_idx = mtl.x;
    gloss = pow(2.0f, 13.0f*(float(mtl.y)/UINT16_MAX));
}

