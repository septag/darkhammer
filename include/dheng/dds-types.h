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

#ifndef __DDSTYPES_H__
#define __DDSTYPES_H__

#include "dhcore/types.h"
#include "gfx-types.h"

#define DDS_MAGIC		0x20534444	/* "DDS " */

#define DDS_FLAG_CAPS			0x1
#define DDS_FLAG_HEIGHT			0x2
#define DDS_FLAG_WIDTH			0x4
#define DDS_FLAG_PITCH			0x8
#define DDS_FLAG_PIXELFORMAT	0x1000
#define DDS_FLAG_MIPMAPCOUNT	0x20000
#define DDS_FLAG_LINEARSIZE		0x80000
#define DDS_FLAG_DEPTH			0x800000

#define DDS_FOURCC      0x00000004  /* DDPF_FOURCC */
#define DDS_RGB         0x00000040  /* DDPF_RGB */
#define DDS_RGBA        0x00000041  /* DDPF_RGB | DDPF_ALPHAPIXELS */
#define DDS_LUMINANCE   0x00020000  /* DDPF_LUMINANCE */

#define DDS_ALPHA						0x00000002  /* DDPF_ALPHA */
#define DDS_HEADER_FLAGS_TEXTURE        0x00001007  /* DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT */
#define DDS_HEADER_FLAGS_MIPMAP         0x00020000  /* DDSD_MIPMAPCOUNT */
#define DDS_HEADER_FLAGS_VOLUME         0x00800000  /* DDSD_DEPTH */
#define DDS_HEADER_FLAGS_PITCH          0x00000008  /* DDSD_PITCH */
#define DDS_HEADER_FLAGS_LINEARSIZE     0x00080000  /* DDSD_LINEARSIZE */

#define DDS_SURFACE_FLAGS_TEXTURE 0x00001000 /* DDSCAPS_TEXTURE */
#define DDS_SURFACE_FLAGS_MIPMAP  0x00400008 /* DDSCAPS_COMPLEX | DDSCAPS_MIPMAP */
#define DDS_SURFACE_FLAGS_CUBEMAP 0x00000008 /* DDSCAPS_COMPLEX */

#define DDS_CAPS_COMPLEX	  0x00000008
#define DDS_CAPS2_VOLUME	  0x00200000
#define DDS_CUBEMAP_POSITIVEX 0x00000600 /* DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX */
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 /* DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX */
#define DDS_CUBEMAP_POSITIVEY 0x00001200 /* DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY */
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 /* DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY */
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 /* DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ */
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 /* DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ */
#define DDS_CUBEMAP_ALLFACES (DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                              DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                              DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ)

#define DDS_FLAGS_VOLUME 0x00200000 /* DDSCAPS2_VOLUME */

#define DDS_FMT_A16B16G16R16		36
#define DDS_FMT_Q16W16V16U16		110
#define DDS_FMT_R16F				111
#define DDS_FMT_G16R16F				112
#define DDS_FMT_A16B16G16R16F		113
#define DDS_FMT_R32F				114
#define DDS_FMT_G32R32F				115
#define DDS_FMT_A32B32G32R32F		116

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                      \
    ((uint)(uint8)(ch0) | ((uint)(uint8)(ch1) << 8) |   \
    ((uint)(uint8)(ch2) << 16) | ((uint)(uint8)(ch3) << 24 ))

/* structures */
#pragma pack(push, 1)
struct _GCCPACKED_ dds_pixel_fmt
{
    uint size;
    uint flags;
    uint fourcc;
    uint rgb_bitcnt;
    uint r_bitmask;
    uint g_bitmask;
    uint b_bitmask;
    uint a_bitmask;
};

const struct dds_pixel_fmt PF_DXT1 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('D','X','T','1'), 0, 0, 0, 0, 0 };

const struct dds_pixel_fmt PF_DXT2 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('D','X','T','2'), 0, 0, 0, 0, 0 };

const struct dds_pixel_fmt PF_DXT3 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('D','X','T','3'), 0, 0, 0, 0, 0 };

const struct dds_pixel_fmt PF_DXT4 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('D','X','T','4'), 0, 0, 0, 0, 0 };

const struct dds_pixel_fmt PF_DXT5 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('D','X','T','5'), 0, 0, 0, 0, 0 };

const struct dds_pixel_fmt PF_ATI1 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '1'), 0, 0, 0, 0, 0 };

const struct dds_pixel_fmt PF_ATI2 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '2'), 0, 0, 0, 0, 0 };

const struct dds_pixel_fmt PF_A8R8G8B8 =
{ sizeof(struct dds_pixel_fmt), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

const struct dds_pixel_fmt PF_A1R5G5B5 =
{ sizeof(struct dds_pixel_fmt), DDS_RGBA, 0, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000 };

const struct dds_pixel_fmt PF_A4R4G4B4 =
{ sizeof(struct dds_pixel_fmt), DDS_RGBA, 0, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000 };

const struct dds_pixel_fmt PF_R8G8B8 =
{ sizeof(struct dds_pixel_fmt), DDS_RGB, 0, 24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 };

const struct dds_pixel_fmt PF_R5G6B5 =
{ sizeof(struct dds_pixel_fmt), DDS_RGB, 0, 16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000 };

/* This indicates the DDS_HEADER_DXT10 extension is present (the format is in dxgiFormat) */
const struct dds_pixel_fmt PF_DX10 =
{ sizeof(struct dds_pixel_fmt), DDS_FOURCC, MAKEFOURCC('D','X','1','0'), 0, 0, 0, 0, 0 };

struct dds_header
{
    uint size;
    uint flags;
    uint height;
    uint width;
    uint pitch_linearsize;
    uint depth; /* only if DDS_HEADER_FLAGS_VOLUME is set in HeaderFlags */
    uint mip_cnt;
    uint reserved1[11];
    struct dds_pixel_fmt ddspf;
    uint caps1;
    uint caps2;
    uint reserved2[3];
};

#pragma pack(pop)

#endif /* DDSTYPES_H */
