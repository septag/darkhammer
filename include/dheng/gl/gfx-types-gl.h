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

enum gfxBlendMode
{
    gfxBlendMode::ZERO              = 0, /*GL_ZERO*/
    gfxBlendMode::ONE               = 1, /*GL_ONE*/
    gfxBlendMode::SRC_COLOR         = 0x0300, /*GL_SRC_COLOR*/
    gfxBlendMode::INV_SRC_COLOR     = 0x0301, /*GL_ONE_MINUS_SRC_COLOR*/
    gfxBlendMode::SRC_ALPHA         = 0x0302, /*GL_SRC_ALPHA*/
    gfxBlendMode::INV_SRC_ALPHA     = 0x0303, /*GL_ONE_MINUS_SRC_ALPHA*/
    gfxBlendMode::DEST_ALPHA        = 0x0304, /*GL_DST_ALPHA*/
    gfxBlendMode::INV_DEST_ALPHA    = 0x0305, /*GL_ONE_MINUS_DST_ALPHA*/
    gfxBlendMode::DEST_COLOR        = 0x0306, /*GL_DST_COLOR*/
    gfxBlendMode::INV_DEST_COLOR    = 0x0307, /*GL_ONE_MINUS_DST_COLOR*/
    gfxBlendMode::SRC_ALPHA_SAT     = 0x0308, /*GL_SRC_ALPHA_SATURATE*/
    gfxBlendMode::CONSTANT_COLOR    = 0x8001, /*GL_CONSTANT_COLOR*/
    gfxBlendMode::INV_CONSTANT_COLOR= 0x8001, /*GL_ONE_MINUS_CONSTANT_COLOR*/
    gfxBlendMode::SRC1_COLOR        = 0x88F9, /*GL_SRC1_COLOR*/
    gfxBlendMode::INV_SRC1_COLOR    = 0x88FA, /*GL_ONE_MINUS_SRC1_COLOR*/
    gfxBlendMode::SRC1_ALPHA        = 0x8589, /*GL_SOURCE1_ALPHA*/
    gfxBlendMode::INV_SRC1_ALPHA    = 0x88FB, /*GL_ONE_MINUS_SRC1_ALPHA*/
    gfxBlendMode::UNKNOWN           = 0xff
};

enum gfxBlendOp
{
    gfxBlendOp::ADD            = 0x8006, /*GL_FUNC_ADD*/
    gfxBlendOp::SUBTRACT       = 0x800A, /*GL_FUNC_SUBTRACT*/
    gfxBlendOp::REV_SUBTRACT   = 0x800B, /*GL_FUNC_REVERSE_SUBTRACT*/
    gfxBlendOp::MIN            = 0x8007, /*GL_MIN*/
    gfxBlendOp::MAX            = 0x8008, /*GL_MAX*/
    gfxBlendOp::UNKNOWN        = 0xff
};

enum gfxClearFlag
{
    gfxClearFlag::DEPTH = 0x00000100,  /*GL_CLEAR_DEPTH_BIT*/
    gfxClearFlag::STENCIL = 0x00000400, /*GL_CLEAR_STENCIL_BIT*/
    gfxClearFlag::COLOR = 0x00004000 /*GL_CLEAR_COLOR_BIT*/
};

enum gfxCmpFunc
{
	gfxCmpFunc::OFF = 0,
    gfxCmpFunc::NEVER = 0x0200, /*GL_NEVER*/
    gfxCmpFunc::LESS = 0x0201, /*GL_LESS*/
    gfxCmpFunc::EQUAL = 0x0202, /*GL_EQUAL*/
    gfxCmpFunc::LESS_EQUAL = 0x0203, /*GL_LEQUAL*/
    gfxCmpFunc::GREATER = 0x0203, /*GL_GREATER*/
    gfxCmpFunc::NOT_EQUAL = 0x0205, /*GL_NOTEQUAL*/
    gfxCmpFunc::GREATER_EQUAL = 0x0206, /*GL_GEQUAL*/
    gfxCmpFunc::ALWAYS = 0x0207, /*GL_ALWAYS*/
    gfxCmpFunc::UNKNOWN = 0xff
};

enum gfxCullMode
{
    gfxCullMode::NONE = 0,
    gfxCullMode::FRONT = 0x0404, /*GL_FRONT​*/
    gfxCullMode::BACK = 0x0405, /*GL_BACK*/
    gfxCullMode::UNKNOWN = 0xff
};

enum gfxFillMode
{
    gfxFillMode::WIREFRAME = 0x1B01, /*GL_LINE*/
    gfxFillMode::SOLID = 0x1B02, /*GL_FILL*/
    gfxFillMode::UNKNOWN = 0xff
};

enum gfxFilterMode
{
    gfxFilterMode::UNKNOWN = 0,
    gfxFilterMode::NEAREST = 0x2600, /*GL_NEAREST*/
    gfxFilterMode::LINEAR = 0x2601 /*GL_LINEAR*/
};

enum gfxStencilOp
{
    gfxStencilOp::KEEP = 0x1E00, /*GL_KEEP*/
    gfxStencilOp::ZERO = 0, /*GL_ZERO*/
    gfxStencilOp::REPLACE = 0x1E01, /*GL_REPLACE*/
    gfxStencilOp::INCR_SAT = 0x1E02, /*GL_INCR*/
    gfxStencilOp::DECR_SAT = 0x1E03, /*GL_DECR*/
    gfxStencilOp::INVERT = 0x150A, /*GL_INVERT*/
    gfxStencilOp::INCR = 0x8507, /*GL_INCR_WRAP*/
    gfxStencilOp::DECR = 0x8508, /*GL_DECR_WRAP*/
    gfxStencilOp::UNKNOWN = 0xff
};

enum gfxAddressMode
{
    gfxAddressMode::UNKNOWN = 0,
    gfxAddressMode::WRAP = 0x2901, /*GL_REPEAT*/
    gfxAddressMode::MIRROR = 0x8370, /*GL_MIRRORED_REPEAT*/
    gfxAddressMode::CLAMP = 0x812F, /*GL_CLAMP_TO_EDGE*/
    gfxAddressMode::BORDER = 0x812D /*GL_CLAMP_TO_BORDER*/
};

enum gfxMapMode
{
    gfxMapMode::READ = 0x0001, /*GL_MAP_READ_BIT*/
    gfxMapMode::WRITE = 0x0002, /*GL_MAP_WRITE_BIT*/
    gfxMapMode::READ_WRITE = 0x0003, /*(GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)*/
    gfxMapMode::WRITE_DISCARD = 0x000A, /*(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)*/
    gfxMapMode::WRITE_DISCARDRANGE = 0x0006, /*(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT)*/
};

enum gfxMemHint
{
    gfxMemHint::STATIC = 0x88E4, /*GL_STATIC_DRAW*/
    gfxMemHint::DYNAMIC = 0x88E8, /*GL_DYNAMIC_DRAW*/
    gfxMemHint::READ = 0x88E9 /*GL_DYNAMIC_READ*/
};

enum gfxInputElemFormat
{
	gfxInputElemFormat::FLOAT = 0x1406, /* GL_FLOAT */
	gfxInputElemFormat::UINT = 0x1405, /* GL_UNSIGNED_INT */
	gfxInputElemFormat::INT = 0x1404 /* GL_INT */
};

enum gfxBufferType
{
    gfxBufferType::VERTEX = 0x8892, /*GL_ARRAY_BUFFER*/
    gfxBufferType::INDEX = 0x8893, /*GL_ELEMENT_ARRAY_BUFFER*/
    gfxBufferType::SHADER_STORAGE = 0x90D2, /*GL_SHADER_STORAGE_BUFFER*/
    gfxBufferType::SHADER_TEXTURE = 0x8C2A, /*GL_TEXTURE_BUFFER*/
    gfxBufferType::STREAM_OUT = 0x8C8E, /*GL_TRANSFORM_FEEDBACK_BUFFER*/
    gfxBufferType::CONSTANT = 0x8A11 /*GL_UNIFORM_BUFFER*/
};

enum gfxUniformType
{
	gfxUniformType::UNKNOWN = 0,
	gfxUniformType::FLOAT = 0x1406, /*GL_FLOAT*/
	gfxUniformType::FLOAT2 = 0x8B50, /*GL_FLOAT_VEC2*/
	gfxUniformType::FLOAT3 = 0x8B51, /*GL_FLOAT_VEC3*/
	gfxUniformType::FLOAT4 = 0x8B52, /*GL_FLOAT_VEC4*/
	gfxUniformType::INT = 0x1404, /*GL_INT*/
	gfxUniformType::INT2 = 0x8B53, /*GL_INT_VEC2*/
	gfxUniformType::INT3 = 0x8B54, /*GL_INT_VEC3*/
	gfxUniformType::INT4 = 0x8B55, /*GL_INT_VEC4*/
	gfxUniformType::UINT = 0x1405, /*GL_UNSIGNED_INT*/
	gfxUniformType::MAT4x3 = 0x8B68, /*GL_FLOAT_MAT3x4*/
	gfxUniformType::MAT4x4 = 0x8B5C, /*GL_FLOAT_MAT*/
    gfxUniformType::STRUCT /* not existed in GL (have to assign manually) */
};

enum gfxFormat
{
	gfxFormat::UNKNOWN = 0,
	gfxFormat::BC1 = GL_COMPRESSED_RGB_S3TC_DXT1_EXT,	/* DXTC1 (RGB) */
    gfxFormat::BC1_SRGB = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,
    gfxFormat::BC2 = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, /* DXTC3 (RGBA) */
    gfxFormat::BC2_SRGB = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
    gfxFormat::BC3 = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, /* DXTC5 (RGBA) */
    gfxFormat::BC3_SRGB = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
    gfxFormat::BC4 = GL_COMPRESSED_RED_RGTC1_EXT, /* TC-RED */
    gfxFormat::BC4_SNORM = GL_COMPRESSED_SIGNED_RED_RGTC1_EXT,
    gfxFormat::BC5 = GL_COMPRESSED_RED_GREEN_RGTC2_EXT, /* TC-RG */
    gfxFormat::BC5_SNORM = GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT,
    gfxFormat::RGBA_UNORM = 0x1908, /*GL_RGBA*/ /* 32bit */
    gfxFormat::RGBA_UNORM_SRGB = 0x8C43, /*GL_SRGB8_ALPHA8*/ /* 32bit */
    GFX_FORMAT_RGB_UNORM = 0x8C41, /*GL_SRGB8*/ /* 24bit */
    gfxFormat::R32G32B32A32_FLOAT = 0x8814, /*GL_RGBA32F,*/
    gfxFormat::R32G32B32A32_UINT = 0x8D7C, /*GL_RGBA8UI,*/
    gfxFormat::R32G32B32A32_SINT = 0x8D8E, /*GL_RGBA8I,*/
    gfxFormat::R32G32B32_FLOAT = 0x8815, /*GL_RGB32F,*/
    gfxFormat::R32G32B32_UINT = 0x8D71, /*GL_RGB32UI,*/
    gfxFormat::R32G32B32_SINT = 0x8D83, /*GL_RGB32I,*/
    gfxFormat::R16G16B16A16_FLOAT = 0x881A, /*GL_RGBA16F,*/
    gfxFormat::R16G16B16A16_UNORM = 0x805B, /*GL_RGBA16,*/
    gfxFormat::R16G16B16A16_UINT = 0x8D76, /*GL_RGBA16UI,*/
    gfxFormat::R16G16B16A16_SINT = 0x8D88, /*GL_RGBA16I,*/
    gfxFormat::R16G16B16A16_SNORM = 0x8F9B, /*GL_RGBA16_SNORM,*/
    gfxFormat::R32G32_FLOAT = 0x8230, /*GL_RG32F,*/
    gfxFormat::R32G32_UINT = 0x823C, /*GL_RG32UI,*/
    gfxFormat::R32G32_SINT = 0x823B, /*GL_RG32I,*/
    gfxFormat::R10G10B10A2_UNORM = 0x8059, /*GL_RGB10_A2,*/
    gfxFormat::R10G10B10A2_UINT = 0x906F, /*GL_RGB10_A2UI,*/
    gfxFormat::R11G11B10_FLOAT =  0x8C3A, /*GL_R11F_G11F_B10F,*/
    gfxFormat::R16G16_FLOAT = 0x822F, /*GL_RG16F,*/
    gfxFormat::R16G16_UNORM = 0x822C, /*GL_RG16,*/
    gfxFormat::R16G16_UINT = 0x823A, /*GL_RG16UI,*/
    gfxFormat::R16G16_SNORM = 0x8F99, /*GL_RG16_SNORM,*/
    gfxFormat::R16G16_SINT = 0x8239, /*GL_RG16I,*/
    gfxFormat::R32_FLOAT = 0x822E, /*GL_R32F,*/
    gfxFormat::R32_UINT = 0x8236, /*GL_R32UI,*/
    gfxFormat::R32_SINT = 0x8235, /*GL_R32I,*/
    gfxFormat::R8G8_UNORM = 0x822B, /*GL_RG8,*/
    gfxFormat::R8G8_UINT = 0x8238, /*GL_RG8UI,*/
    gfxFormat::R8G8_SNORM = 0x8F95, /*GL_RG8_SNORM,*/
    gfxFormat::R8G8_SINT = 0x8237, /*GL_RG8I,*/
    gfxFormat::R16_FLOAT = 0x822D, /*GL_R16F,*/
    gfxFormat::R16_UNORM = 0x822A, /*GL_R16,*/
    gfxFormat::R16_UINT = 0x8234, /*GL_R16UI,*/
    gfxFormat::R16_SNORM = 0x8F98, /*GL_R16_SNORM,*/
    gfxFormat::R16_SINT = 0x8233, /*GL_R16I,*/
    gfxFormat::R8_UNORM = 0x8229, /*GL_R8,*/
    gfxFormat::R8_UINT = 0x8232, /*GL_R8UI,*/
    gfxFormat::R8_SNORM = 0x8F94, /*GL_R8_SNORM,*/
    gfxFormat::R8_SINT = 0x8231, /*GL_R8I*/
    gfxFormat::DEPTH24_STENCIL8 = 0x88F0, /*GL_DEPTH24_STENCIL8*/
	gfxFormat::DEPTH32 = 0x81A7, /*GL_DEPTH_COMPONENT32*/
	gfxFormat::DEPTH16 = 0x81A5 /*GL_DEPTH_COMPONENT16​​*/
};

enum gfxTextureType
{
	gfxTextureType::TEX_1D = 0x0DE0, /*GL_TEXTURE_1D*/
	gfxTextureType::TEX_2D = 0x0DE1, /*GL_TEXTURE_2D*/
	gfxTextureType::TEX_3D = 0x806F, /*GL_TEXTURE_3D*/
	gfxTextureType::TEX_CUBE = 0x8513, /*GL_TEXTURE_CUBE_MAP*/
	gfxTextureType::TEX_1D_ARRAY = 0x8C18, /*GL_TEXTURE_1D_ARRAY*/
	gfxTextureType::TEX_2D_ARRAY = 0x8C1A, /*GL_TEXTURE_2D_ARRAY*/
	gfxTextureType::TEX_CUBE_ARRAY = 0x9009 /*GL_TEXTURE_CUBE_MAP_ARRAY*/
};

enum gfxPrimitiveType
{
    gfxPrimitiveType::POINT_LIST = 0x0000, /*GL_POINTS*/
    gfxPrimitiveType::LINE_LIST = 0x0001, /*GL_LINES*/
    gfxPrimitiveType::LINE_STRIP = 0x0003, /*GL_LINE_STRIP*/
    gfxPrimitiveType::TRIANGLE_LIST = 0x0004, /*GL_TRIANGLES*/
    gfxPrimitiveType::TRIANGLE_STRIP = 0x0005, /*GL_TRIANGLE_STRIP*/
    gfxPrimitiveType::LINELIST_ADJ = 0x000A, /*GL_LINES_ADJACENCY*/
    gfxPrimitiveType::LINESTRIP_ADJ = 0x000B, /*GL_LINE_STRIP_ADJACENCY*/
    gfxPrimitiveType::TRIANGLELIST_ADJ = 0x000C, /*GL_TRIANGLES_ADJACENCY*/
    gfxPrimitiveType::TRIANGLESTRIP_ADJ = 0x000D /*GL_TRIANGLE_STRIP_ADJACENCY*/
};

enum gfxIndexType
{
    gfxIndexType::UNKNOWN = 0,
	gfxIndexType::UINT32 = 0x1405, /*GL_UNSIGNED_INT*/
	gfxIndexType::UINT16 = 0x1403, /*GL_UNSIGNED_SHORT*/
};

struct gfx_program_bin_desc
{
    const void* data;
    uint size;
    uint fmt;
};

#endif /* GFX_TYPES_GL_H_ */
