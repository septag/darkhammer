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

#if defined(_SKIN_)

/* cbuffer for skinning: shared among all skinning shaders */
Buffer<float4> tb_skins;

struct skin_output_pnt
{
    float4 pos;
    float3 norm;
    float3 tangent;
    float3 binorm;
};

struct skin_output_pn
{
    float4 pos;
    float3 norm;
};

float4x3 get_bone(int inst_idx, int bone_idx)
{
	int offset = (bone_idx + inst_idx*_MAX_BONES_)*3;
	float4 col1 = tb_skins.Load(offset);
	float4 col2 = tb_skins.Load(offset + 1);
	float4 col3 = tb_skins.Load(offset + 2);
	return float4x3(
		float3(col1.x, col2.x, col3.x),
		float3(col1.y, col2.y, col3.y),
		float3(col1.z, col2.z, col3.z),
		float3(col1.w, col2.w, col3.w));
}

float4 skin_vertex_p(int inst_idx, int4 blend_idxs, float4 blend_weights, float4 pos)
{
	float4 r;
    r = float4(0, 0, 0, 1);

	for (int i = 0; i < 4; i++)	{
		float4x3 mat = get_bone(inst_idx, blend_idxs[i]);
		r.xyz += mul(pos, mat) * blend_weights[i];
	}

	return r;    
}


skin_output_pn skin_vertex_pn(int inst_idx, int4 blend_idxs, float4 blend_weights, float4 pos, 
	float3 norm)
{
	skin_output_pn r;

    r.pos = float4(0, 0, 0, 1);
    r.norm = float3(0, 0, 0);

	for (int i = 0; i < 4; i++) {
		float4x3 mat = get_bone(inst_idx, blend_idxs[i]);
		float w = blend_weights[i];
        float3x3 m3 = (float3x3)mat;

		r.pos.xyz += mul(pos, mat) * w;
		r.norm += mul(norm, m3) * w;
	}

	return r;    
}

skin_output_pnt skin_vertex_pnt(int inst_idx, int4 blend_idxs, float4 blend_weights, float4 pos,
    float3 norm, float3 tangent, float3 binorm)
{
	skin_output_pnt r;

    r.pos = float4(0, 0, 0, 1);
    r.norm = float3(0, 0, 0);
    r.tangent = float3(0, 0, 0);
    r.binorm = float3(0, 0, 0);

	for (int i = 0; i < 4; i++) {
		float4x3 mat = get_bone(inst_idx, blend_idxs[i]);
        float3x3 m3 = (float3x3)mat;
		float w = blend_weights[i];

		r.pos.xyz += mul(pos, mat) * w;
		r.norm += mul(norm, m3) * w;
		r.tangent += mul(tangent, m3) * w;
		r.binorm += mul(binorm, m3) * w;
	}

	return r;    
}

#endif
