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

#ifndef GFX_TYPES_GL_H_
#define GFX_TYPES_GL_H_

#include "dhcore/types.h"

#if defined(_MSVC_)
#pragma warning(disable: 663)
#endif

/* api specific flag differences */
#define GFX_BMP2D_EXTRAFLAG GFX_BMP2D_FLIPY

/* compressed formats
 * http://www.opengl.org/registry/specs/EXT/texture_compression_rgtc.txt
 * http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt
 * http://www.opengl.org/registry/specs/EXT/texture_sRGB.txt
 * */
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C

#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E

#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F

#define GL_COMPRESSED_RED_RGTC1_EXT 0x8DBB
#define GL_COMPRESSED_SIGNED_RED_RGTC1_EXT 0x8DBC

#define GL_COMPRESSED_RED_GREEN_RGTC2_EXT 0x8DBD
#define GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT 0x8DBE

/**************************************************************************
 * ENUMS
 */

enum class gfxBlendMode : uint
{
    ZERO              = 0, /*GL_ZERO*/
    ONE               = 1, /*GL_ONE*/
    SRC_COLOR         = 0x0300, /*GL_SRC_COLOR*/
    INV_SRC_COLOR     = 0x0301, /*GL_ONE_MINUS_SRC_COLOR*/
    SRC_ALPHA         = 0x0302, /*GL_SRC_ALPHA*/
    INV_SRC_ALPHA     = 0x0303, /*GL_ONE_MINUS_SRC_ALPHA*/
    DEST_ALPHA        = 0x0304, /*GL_DST_ALPHA*/
    INV_DEST_ALPHA    = 0x0305, /*GL_ONE_MINUS_DST_ALPHA*/
    DEST_COLOR        = 0x0306, /*GL_DST_COLOR*/
    INV_DEST_COLOR    = 0x0307, /*GL_ONE_MINUS_DST_COLOR*/
    SRC_ALPHA_SAT     = 0x0308, /*GL_SRC_ALPHA_SATURATE*/
    CONSTANT_COLOR    = 0x8001, /*GL_CONSTANT_COLOR*/
    INV_CONSTANT_COLOR= 0x8001, /*GL_ONE_MINUS_CONSTANT_COLOR*/
    SRC1_COLOR        = 0x88F9, /*GL_SRC1_COLOR*/
    INV_SRC1_COLOR    = 0x88FA, /*GL_ONE_MINUS_SRC1_COLOR*/
    SRC1_ALPHA        = 0x8589, /*GL_SOURCE1_ALPHA*/
    INV_SRC1_ALPHA    = 0x88FB, /*GL_ONE_MINUS_SRC1_ALPHA*/
    UNKNOWN           = 0xff
};

enum class gfxBlendOp : uint
{
    ADD            = 0x8006, /*GL_FUNC_ADD*/
    SUBTRACT       = 0x800A, /*GL_FUNC_SUBTRACT*/
    REV_SUBTRACT   = 0x800B, /*GL_FUNC_REVERSE_SUBTRACT*/
    MIN            = 0x8007, /*GL_MIN*/
    MAX            = 0x8008, /*GL_MAX*/
    UNKNOWN        = 0xff
};

enum class gfxClearFlag : uint
{
    DEPTH = 0x00000100,  /*GL_CLEAR_DEPTH_BIT*/
    STENCIL = 0x00000400, /*GL_CLEAR_STENCIL_BIT*/
    COLOR = 0x00004000 /*GL_CLEAR_COLOR_BIT*/
};

enum class gfxCmpFunc : uint
{
    OFF = 0,
    NEVER = 0x0200, /*GL_NEVER*/
    LESS = 0x0201, /*GL_LESS*/
    EQUAL = 0x0202, /*GL_EQUAL*/
    LESS_EQUAL = 0x0203, /*GL_LEQUAL*/
    GREATER = 0x0203, /*GL_GREATER*/
    NOT_EQUAL = 0x0205, /*GL_NOTEQUAL*/
    GREATER_EQUAL = 0x0206, /*GL_GEQUAL*/
    ALWAYS = 0x0207, /*GL_ALWAYS*/
    UNKNOWN = 0xff
};

enum class gfxCullMode : uint
{
    NONE = 0,
    FRONT = 0x0404, /*GL_FRONT​*/
    BACK = 0x0405, /*GL_BACK*/
    UNKNOWN = 0xff
};

enum class gfxFillMode : uint
{
    WIREFRAME = 0x1B01, /*GL_LINE*/
    SOLID = 0x1B02, /*GL_FILL*/
    UNKNOWN = 0xff
};

enum class gfxFilterMode : uint
{
    UNKNOWN = 0,
    NEAREST = 0x2600, /*GL_NEAREST*/
    LINEAR = 0x2601 /*GL_LINEAR*/
};

enum class gfxStencilOp : uint
{
    KEEP = 0x1E00, /*GL_KEEP*/
    ZERO = 0, /*GL_ZERO*/
    REPLACE = 0x1E01, /*GL_REPLACE*/
    INCR_SAT = 0x1E02, /*GL_INCR*/
    DECR_SAT = 0x1E03, /*GL_DECR*/
    INVERT = 0x150A, /*GL_INVERT*/
    INCR = 0x8507, /*GL_INCR_WRAP*/
    DECR = 0x8508, /*GL_DECR_WRAP*/
    UNKNOWN = 0xff
};

enum class gfxAddressMode : uint
{
    UNKNOWN = 0,
    WRAP = 0x2901, /*GL_REPEAT*/
    MIRROR = 0x8370, /*GL_MIRRORED_REPEAT*/
    CLAMP = 0x812F, /*GL_CLAMP_TO_EDGE*/
    BORDER = 0x812D /*GL_CLAMP_TO_BORDER*/
};

enum class gfxMapMode : uint
{
    READ = 0x0001, /*GL_MAP_READ_BIT*/
    WRITE = 0x0002, /*GL_MAP_WRITE_BIT*/
    READ_WRITE = 0x0003, /*(GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)*/
    WRITE_DISCARD = 0x000A, /*(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)*/
    WRITE_DISCARDRANGE = 0x0006, /*(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT)*/
};

enum class gfxMemHint : uint
{
    STATIC = 0x88E4, /*GL_STATIC_DRAW*/
    DYNAMIC = 0x88E8, /*GL_DYNAMIC_DRAW*/
    READ = 0x88E9 /*GL_DYNAMIC_READ*/
};

enum class gfxInputElemFormat : uint
{
    FLOAT = 0x1406, /* GL_FLOAT */
    UINT = 0x1405, /* GL_UNSIGNED_INT */
    INT = 0x1404 /* GL_INT */
};

enum class gfxBufferType : uint
{
    VERTEX = 0x8892, /*GL_ARRAY_BUFFER*/
    INDEX = 0x8893, /*GL_ELEMENT_ARRAY_BUFFER*/
    SHADER_STORAGE = 0x90D2, /*GL_SHADER_STORAGE_BUFFER*/
    SHADER_TEXTURE = 0x8C2A, /*GL_TEXTURE_BUFFER*/
    STREAM_OUT = 0x8C8E, /*GL_TRANSFORM_FEEDBACK_BUFFER*/
    CONSTANT = 0x8A11 /*GL_UNIFORM_BUFFER*/
};

enum class gfxUniformType : uint
{
    UNKNOWN = 0,
    FLOAT = 0x1406, /*GL_FLOAT*/
    FLOAT2 = 0x8B50, /*GL_FLOAT_VEC2*/
    FLOAT3 = 0x8B51, /*GL_FLOAT_VEC3*/
    FLOAT4 = 0x8B52, /*GL_FLOAT_VEC4*/
    INT = 0x1404, /*GL_INT*/
    INT2 = 0x8B53, /*GL_INT_VEC2*/
    INT3 = 0x8B54, /*GL_INT_VEC3*/
    INT4 = 0x8B55, /*GL_INT_VEC4*/
    UINT = 0x1405, /*GL_UNSIGNED_INT*/
    MAT4x3 = 0x8B68, /*GL_FLOAT_MAT3x4*/
    MAT4x4 = 0x8B5C, /*GL_FLOAT_MAT*/
    STRUCT /* not existed in GL (have to assign manually) */
};

enum class gfxFormat : uint
{
    UNKNOWN = 0,
    BC1 = GL_COMPRESSED_RGB_S3TC_DXT1_EXT,	/* DXTC1 (RGB) */
    BC1_SRGB = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,
    BC2 = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, /* DXTC3 (RGBA) */
    BC2_SRGB = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
    BC3 = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, /* DXTC5 (RGBA) */
    BC3_SRGB = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
    BC4 = GL_COMPRESSED_RED_RGTC1_EXT, /* TC-RED */
    BC4_SNORM = GL_COMPRESSED_SIGNED_RED_RGTC1_EXT,
    BC5 = GL_COMPRESSED_RED_GREEN_RGTC2_EXT, /* TC-RG */
    BC5_SNORM = GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT,
    RGBA_UNORM = 0x1908, /*GL_RGBA*/ /* 32bit */
    RGBA_UNORM_SRGB = 0x8C43, /*GL_SRGB8_ALPHA8*/ /* 32bit */
    RGB_UNORM = 0x8C41, /*GL_SRGB8*/ /* 24bit */
    R32G32B32A32_FLOAT = 0x8814, /*GL_RGBA32F,*/
    R32G32B32A32_UINT = 0x8D7C, /*GL_RGBA8UI,*/
    R32G32B32A32_SINT = 0x8D8E, /*GL_RGBA8I,*/
    R32G32B32_FLOAT = 0x8815, /*GL_RGB32F,*/
    R32G32B32_UINT = 0x8D71, /*GL_RGB32UI,*/
    R32G32B32_SINT = 0x8D83, /*GL_RGB32I,*/
    R16G16B16A16_FLOAT = 0x881A, /*GL_RGBA16F,*/
    R16G16B16A16_UNORM = 0x805B, /*GL_RGBA16,*/
    R16G16B16A16_UINT = 0x8D76, /*GL_RGBA16UI,*/
    R16G16B16A16_SINT = 0x8D88, /*GL_RGBA16I,*/
    R16G16B16A16_SNORM = 0x8F9B, /*GL_RGBA16_SNORM,*/
    R32G32_FLOAT = 0x8230, /*GL_RG32F,*/
    R32G32_UINT = 0x823C, /*GL_RG32UI,*/
    R32G32_SINT = 0x823B, /*GL_RG32I,*/
    R10G10B10A2_UNORM = 0x8059, /*GL_RGB10_A2,*/
    R10G10B10A2_UINT = 0x906F, /*GL_RGB10_A2UI,*/
    R11G11B10_FLOAT =  0x8C3A, /*GL_R11F_G11F_B10F,*/
    R16G16_FLOAT = 0x822F, /*GL_RG16F,*/
    R16G16_UNORM = 0x822C, /*GL_RG16,*/
    R16G16_UINT = 0x823A, /*GL_RG16UI,*/
    R16G16_SNORM = 0x8F99, /*GL_RG16_SNORM,*/
    R16G16_SINT = 0x8239, /*GL_RG16I,*/
    R32_FLOAT = 0x822E, /*GL_R32F,*/
    R32_UINT = 0x8236, /*GL_R32UI,*/
    R32_SINT = 0x8235, /*GL_R32I,*/
    R8G8_UNORM = 0x822B, /*GL_RG8,*/
    R8G8_UINT = 0x8238, /*GL_RG8UI,*/
    R8G8_SNORM = 0x8F95, /*GL_RG8_SNORM,*/
    R8G8_SINT = 0x8237, /*GL_RG8I,*/
    R16_FLOAT = 0x822D, /*GL_R16F,*/
    R16_UNORM = 0x822A, /*GL_R16,*/
    R16_UINT = 0x8234, /*GL_R16UI,*/
    R16_SNORM = 0x8F98, /*GL_R16_SNORM,*/
    R16_SINT = 0x8233, /*GL_R16I,*/
    R8_UNORM = 0x8229, /*GL_R8,*/
    R8_UINT = 0x8232, /*GL_R8UI,*/
    R8_SNORM = 0x8F94, /*GL_R8_SNORM,*/
    R8_SINT = 0x8231, /*GL_R8I*/
    DEPTH24_STENCIL8 = 0x88F0, /*GL_DEPTH24_STENCIL8*/
    DEPTH32 = 0x81A7, /*GL_DEPTH_COMPONENT32*/
    DEPTH16 = 0x81A5 /*GL_DEPTH_COMPONENT16​​*/
};

enum class gfxTextureType : uint
{
    TEX_1D = 0x0DE0, /*GL_TEXTURE_1D*/
    TEX_2D = 0x0DE1, /*GL_TEXTURE_2D*/
    TEX_3D = 0x806F, /*GL_TEXTURE_3D*/
    TEX_CUBE = 0x8513, /*GL_TEXTURE_CUBE_MAP*/
    TEX_1D_ARRAY = 0x8C18, /*GL_TEXTURE_1D_ARRAY*/
    TEX_2D_ARRAY = 0x8C1A, /*GL_TEXTURE_2D_ARRAY*/
    TEX_CUBE_ARRAY = 0x9009 /*GL_TEXTURE_CUBE_MAP_ARRAY*/
};

enum class gfxPrimitiveType : uint
{
    POINT_LIST = 0x0000, /*GL_POINTS*/
    LINE_LIST = 0x0001, /*GL_LINES*/
    LINE_STRIP = 0x0003, /*GL_LINE_STRIP*/
    TRIANGLE_LIST = 0x0004, /*GL_TRIANGLES*/
    TRIANGLE_STRIP = 0x0005, /*GL_TRIANGLE_STRIP*/
    LINELIST_ADJ = 0x000A, /*GL_LINES_ADJACENCY*/
    LINESTRIP_ADJ = 0x000B, /*GL_LINE_STRIP_ADJACENCY*/
    TRIANGLELIST_ADJ = 0x000C, /*GL_TRIANGLES_ADJACENCY*/
    TRIANGLESTRIP_ADJ = 0x000D /*GL_TRIANGLE_STRIP_ADJACENCY*/
};

enum class gfxIndexType : uint
{
    UNKNOWN = 0,
    UINT32 = 0x1405, /*GL_UNSIGNED_INT*/
    UINT16 = 0x1403, /*GL_UNSIGNED_SHORT*/
};

struct gfx_program_bin_desc
{
    const void *data;
    uint size;
    uint fmt;
};

#endif /* GFX_TYPES_GL_H_ */
