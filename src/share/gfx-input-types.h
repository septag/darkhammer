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

#ifndef GFX_INPUT_TYPES_H_
#define GFX_INPUT_TYPES_H_

// Vertex input element IDs
enum class gfxInputElemId : uint
{
	POSITION = 0,   // float4
	NORMAL,         // float3
	TEXCOORD0,      // float2
    TANGENT,        // float3
    BINORMAL,       // float3
    BLENDINDEX,     // int4
    BLENDWEIGHT,    // float4
	TEXCOORD1,      // float2
	TEXCOORD2,      // float4
	TEXCOORD3,      // float4
	COLOR,          // float4
    COUNT           // Don't use it. just for array counting
};

#endif /* GFX_INPUT_TYPES_H_ */
