/***********************************************************************************
 *
 * Copyright (c) 2012, Sepehr Taghdisian
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 **********************************************************************************/

#ifndef __GFXTYPES_D3D_H__
#define __GFXTYPES_D3D__H__

#include "dhcore/types.h"

#if defined(_MSVC_)
#pragma warning(disable: 663)
#endif

/* api specific flag differences */
/* extra flag for flipping imasges, automatically applied to draw 2d bitmap flags */
#define GFX_BMP2D_EXTRAFLAG 0

/**************************************************************************
 * ENUMS
 */
enum class gfxBlendMode : uint
{
    ZERO              = 1,    /* D3D11_BLEND_ZERO */
    ONE               = 2,    /* D3D11_BLEND_ONE  */
    SRC_COLOR         = 3,    /* D3D11_BLEND_SRC_COLOR  */
    INV_SRC_COLOR     = 4,    /* D3D11_BLEND_INV_SRC_COLOR */
    SRC_ALPHA         = 5,    /* D3D11_BLEND_SRC_ALPHA  */
    INV_SRC_ALPHA     = 6,    /* D3D11_BLEND_INV_SRC_ALPHA */
    DEST_ALPHA        = 7,    /* D3D11_BLEND_DEST_ALPHA */
    INV_DEST_ALPHA    = 8,    /* D3D11_BLEND_INV_DEST_ALPHA */
    DEST_COLOR        = 9,    /* D3D11_BLEND_DEST_COLOR */
    INV_DEST_COLOR    = 10,   /* D3D11_BLEND_INV_DEST_COLOR */
    SRC_ALPHA_SAT     = 11,   /* D3D11_BLEND_SRC_ALPHA_SAT */
    CONSTANT_COLOR    = 14,   /* D3D11_BLEND_BLEND_FACTOR */
    INV_CONSTANT_COLOR= 15,   /* D3D11_BLEND_INV_BLEND_FACTOR */
    SRC1_COLOR        = 16,   /* D3D11_BLEND_SRC1_COLOR */
    INV_SRC1_COLOR    = 17,   /* D3D11_BLEND_INV_SRC1_COLOR */
    SRC1_ALPHA        = 18,   /* D3D11_BLEND_SRC1_ALPHA */
    INV_SRC1_ALPHA    = 19,   /* D3D11_BLEND_INV_SRC1_ALPHA */
    UNKNOWN           = 0xff
};

enum class gfxBlendOp : uint
{
    ADD            = 1, /* D3D11_BLEND_OP_ADD */
    SUBTRACT       = 2, /* D3D11_BLEND_OP_SUBTRACT */
    REV_SUBTRACT   = 3, /* D3D11_BLEND_OP_REV_SUBTRACT */
    MIN            = 4, /* D3D11_BLEND_OP_MIN */
    MAX            = 5, /* D3D11_BLEND_OP_MAX */
    UNKNOWN        = 0xff
};

enum class gfxClearFlag : uint
{
    DEPTH = 0x1,  /* D3D11_CLEAR_DEPTH */
    STENCIL = 0x2, /* D3D11_CLEAR_STENCIL */
    COLOR = 0x4
};

enum class gfxCmpFunc : uint
{
	OFF = 0, /* */
    NEVER = 1, /* D3D11_COMPARISON_NEVER */
    LESS = 2, /* D3D11_COMPARISON_LESS */
    EQUAL = 3, /* D3D11_COMPARISON_EQUAL */
    LESS_EQUAL = 4, /* D3D11_COMPARISON_LESS_EQUAL */
    GREATER = 5, /* D3D11_COMPARISON_GREATER */
    NOT_EQUAL = 6, /* D3D11_COMPARISON_NOT_EQUAL */
    GREATER_EQUAL = 7, /* D3D11_COMPARISON_GREATER_EQUAL  */
    ALWAYS = 8, /* D3D11_COMPARISON_ALWAYS */
    UNKNOWN = 0xff
};

enum class gfxCullMode : uint
{
    NONE = 1, /* D3D11_CULL_NONE */
    FRONT = 2, /* D3D11_CULL_FRONT */
    BACK = 3, /* D3D11_CULL_BACK */
    UNKNOWN = 0xff
};

enum class gfxFillMode : uint
{
    WIREFRAME = 2, /* D3D11_FILL_WIREFRAME */
    SOLID = 3, /* D3D11_FILL_SOLID */
    UNKNOWN = 0xff
};

enum class gfxFilterMode : uint
{
    UNKNOWN = 0,
    NEAREST,
    LINEAR
};

enum class gfxStencilOp : uint
{
    KEEP = 1, /* D3D11_STENCIL_OP_KEEP */
    ZERO = 2, /* D3D11_STENCIL_OP_ZERO */
    REPLACE = 3, /* D3D11_STENCIL_OP_REPLACE */
    INCR_SAT = 4, /* D3D11_STENCIL_OP_INCR_SAT */
    DECR_SAT = 5, /* D3D11_STENCIL_OP_DECR_SAT */
    INVERT = 6, /* D3D11_STENCIL_OP_INVERT */
    INCR = 7, /* D3D11_STENCIL_OP_INCR */
    DECR = 8, /* D3D11_STENCIL_OP_DECR */
    UNKNOWN = 0xff
};

enum class gfxAddressMode : uint
{
    UNKNOWN = 0, /* */
    WRAP = 1, /* D3D11_TEXTURE_ADDRESS_WRAP */
    MIRROR = 2, /* D3D11_TEXTURE_ADDRESS_MIRROR */
    CLAMP = 3, /* D3D11_TEXTURE_ADDRESS_CLAMP */
    BORDER = 4 /* D3D11_TEXTURE_ADDRESS_BORDER */
};

enum gfxPrimitiveType
{
    POINT_LIST = 1, /* */
    LINE_LIST = 2, /* D3D11_PRIMITIVE_TOPOLOGY_LINELIST */
    LINE_STRIP = 3, /* D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP */
    TRIANGLE_LIST = 4, /* D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST */
    TRIANGLE_STRIP = 5, /* D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP */
    LINELIST_ADJ = 10, /* D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ */
    LINESTRIP_ADJ = 11, /* D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ */
    TRIANGLELIST_ADJ = 12, /* D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ */
    TRIANGLESTRIP_ADJ = 13 /* D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ */
};

enum class gfxFormat : uint
{
    UNKNOWN = 0,
    R32G32B32A32_FLOAT  = 2,
    R32G32B32A32_UINT = 3,
    R32G32B32A32_SINT = 4,
    R32G32B32_FLOAT = 6,
    R32G32B32_UINT = 7,
    R32G32B32_SINT = 8,
    R16G16B16A16_FLOAT = 10,
    R16G16B16A16_UNORM = 11,
    R16G16B16A16_UINT = 12,
    R16G16B16A16_SNORM = 13,
    R16G16B16A16_SINT = 14,
    R32G32_FLOAT = 16,
    R32G32_UINT = 17,
    R32G32_SINT = 18,
    R10G10B10A2_UNORM = 24,
    R10G10B10A2_UINT = 25,
    R11G11B10_FLOAT = 26,
    RGBA_UNORM = 28, /* DXGI_FORMAT_R8G8B8A8_UNORM */
    RGBA_UNORM_SRGB = 29, /* DXGI_FORMAT_R8G8B8A8_UNORM_SRGB */
    RGBA_SNORM = 31,
    R16G16_FLOAT = 34,
    R16G16_UNORM = 35,
    R16G16_UINT = 36,
    R16G16_SNORM = 37,
    R16G16_SINT = 38,
    R32_FLOAT = 41,
    R32_UINT = 42,
    R32_SINT = 43,
    R8G8_UNORM = 49,
    R8G8_UINT = 50,
    R8G8_SNORM = 51,
    R8G8_SINT = 52,
    R16_FLOAT = 54,
    R16_UNORM = 56,
    R16_UINT = 57,
    R16_SNORM = 58,
    R16_SINT = 59,
    R8_UNORM = 61,
    R8_UINT = 62,
    R8_SNORM = 63,
    R8_SINT = 64,
    BC1 = 71,
    BC1_SRGB = 72,
    BC2 = 74,
    BC2_SRGB = 75,
    BC3 = 77,
    BC3_SRGB = 78,
    BC4 = 80,
    BC4_SNORM = 81,
    BC5 = 83,
    BC5_SNORM = 84,
    DEPTH24_STENCIL8 = 45, /*DXGI_FORMAT_D24_UNORM_S8_UINT*/
    DEPTH32 = 40, /*DXGI_FORMAT_D32_FLOAT*/
    DEPTH16 = 55 /*DXGI_FORMAT_D16_UNORM*/
};

enum class gfxTextureType : uint
{
    TEX_1D,
    TEX_2D,
    TEX_3D,
    TEX_CUBE,
    TEX_1D_ARRAY,
    TEX_2D_ARRAY,
    TEX_CUBE_ARRAY
};

enum class gfxBufferType : uint
{
    VERTEX = 0x1L, /*D3D11_BIND_VERTEX_BUFFER*/
    INDEX = 0x2L, /*D3D11_BIND_INDEX_BUFFER*/
    SHADER_STORAGE = 0x100L,
    SHADER_TEXTURE = 0x200L,  /* TBUFFER */
    STREAM_OUT = 0x10L, /*D3D11_BIND_STREAM_OUTPUT*/
    CONSTANT = 0x4L /*D3D11_BIND_CONSTANT_BUFFER*/
};

enum class gfxMapMode : uint
{
    READ = 1, /*D3D11_MAP_READ*/
    WRITE = 2, /*D3D11_MAP_WRITE*/
    READ_WRITE = 3, /*D3D11_MAP_READ_WRITE*/
    WRITE_DISCARD = 4, /*D3D11_MAP_WRITE_DISCARD*/
    WRITE_DISCARDRANGE = 5 /*D3D11_MAP_WRITE_NO_OVERWRITE*/
};

enum class gfxMemHint : uint
{
    STATIC = 0, /*D3D11_USAGE_DEFAULT*/
    DYNAMIC = 2, /*D3D11_USAGE_DYNAMIC*/
    READ = 3 /*D3D11_USAGE_STAGING*/
};

enum class gfxInputElemFormat : uint
{
    FLOAT,
    UINT,
    INT
};

enum class gfxIndexType : uint
{
    UNKNOWN = 0,
    UINT32 = 42, /*DXGI_FORMAT_R32_UINT*/
    UINT16 = 57, /*DXGI_FORMAT_R16_UINT*/
};

enum class gfxUniformType : uint
{
    UNKNOWN = 0,
    FLOAT,
    FLOAT2,
    FLOAT3,
    FLOAT4,
    INT,
    INT2,
    INT3,
    INT4,
    BOOL,
    UINT,
    MAT4x3,
    MAT4x4,
    STRUCT
};

struct gfx_input_element_binding;
struct gfx_program_bin_desc
{
    uint vs_sz;
    uint ps_sz;
    uint gs_sz;

    const void *vs;
    const void *gs;
    const void *ps;

    uint input_cnt;
    const struct gfx_input_element_binding *inputs;
};

#endif /* __GFXTYPES_D3D_H__ */
