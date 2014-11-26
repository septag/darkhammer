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

/* defines maximum number of vertex-buffers and their IDs
 * we call them input-element-id
 * for each element-id we have a vertex-buffer in geometry data
 */
enum gfx_input_element_id
{
	GFX_INPUTELEMENT_ID_POSITION = 0,   /* float4: POSITION */
	GFX_INPUTELEMENT_ID_NORMAL, /* float3: NORMAL */
	GFX_INPUTELEMENT_ID_TEXCOORD0, /* float2: TEXCOORD0 */
    GFX_INPUTELEMENT_ID_TANGENT, /* float3: TANGENT */
    GFX_INPUTELEMENT_ID_BINORMAL, /* float3: BINORMAL */
    GFX_INPUTELEMENT_ID_BLENDINDEX, /* int4: BLENDINDEX */
    GFX_INPUTELEMENT_ID_BLENDWEIGHT, /* float4: BLENDWEIGHT */
	GFX_INPUTELEMENT_ID_TEXCOORD1, /* float2: TEXCOORD1 */
	GFX_INPUTELEMENT_ID_TEXCOORD2, /* float4: TEXCOORD2 */
	GFX_INPUTELEMENT_ID_TEXCOORD3, /* float4: TEXCOORD3 */
	GFX_INPUTELEMENT_ID_COLOR, /* float4: COLOR */
    GFX_INPUTELEMENT_ID_CNT /* don't use this, just for count */
};

#endif /* GFX_INPUT_TYPES_H_ */
