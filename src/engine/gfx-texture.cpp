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

#if defined(_GL_)
#include "GL/glew.h"
#endif

#include "dhcore/core.h"
#include "dhcore/task-mgr.h"

#include "gfx-texture.h"

#include "dds-types.h"
#include "mem-ids.h"
#include "gfx-device.h"

#include <math.h>

#define ISBITMASK(r,g,b,a) (pf->r_bitmask == r && pf->g_bitmask == g && \
                            pf->b_bitmask == b && pf->a_bitmask == a)

/*************************************************************************************************
 * forward
 */
gfx_texture dds_create_texture(struct allocator* tmp_alloc, uint first_mipidx, int srgb,
		struct dds_header* header, uint8* bits, uint size, uint thread_id);
enum gfxFormat dds_get_format(const struct dds_pixel_fmt* pf);
int dds_is_argb(const struct dds_pixel_fmt* pf);
void dds_get_surfaceinfo(uint width, uint height, enum gfxFormat fmt,
		OUT uint* size, OUT uint* rowsize, OUT uint* rowcnt);
uint gfx_texture_getbpp(enum gfxFormat fmt);
enum gfxFormat dds_conv_tosrgb(enum gfxFormat fmt);

/*************************************************************************************************/
gfx_texture gfx_texture_loaddds(const char* dds_filepath, uint first_mipidx,
		int srgb, uint thread_id)
{
	/* TODO: get tmp allocator based on thread_id */
	struct allocator* tmp_alloc = tsk_get_tmpalloc(thread_id);
	A_SAVE(tmp_alloc);

	file_t f = fio_openmem(tmp_alloc, dds_filepath, FALSE, MID_GFX);
	if (f == NULL)	{
        A_LOAD(tmp_alloc);
		err_printf(__FILE__, __LINE__, "load dds '%s' failed: could not open file", dds_filepath);
		return NULL;
	}

    /* header */
    uint sign;
    struct dds_header header;

    fio_read(f, &sign, sizeof(sign), 1);
    if( sign != DDS_MAGIC )		{
        A_LOAD(tmp_alloc);
        err_printf(__FILE__, __LINE__, "load dds '%s' failed: invalid file format", dds_filepath);
        return NULL;
    }

    fio_read(f, &header, sizeof(header), 1);
    if (header.size != sizeof(header) || header.ddspf.size != sizeof(struct dds_pixel_fmt))	{
        A_LOAD(tmp_alloc);
        err_printf(__FILE__, __LINE__, "load dds '%s' failed: invalid header", dds_filepath);
        return NULL;
    }

    /* Check for DX10 DDS, they are not supported because of crappy
     *  api-specific properties of these files */
    if(BIT_CHECK(header.ddspf.flags, DDS_FOURCC) &&
       MAKEFOURCC('D', 'X', '1', '0') == header.ddspf.fourcc)
    {
        A_LOAD(tmp_alloc);
        err_printf(__FILE__, __LINE__, "load dds '%s' failed:"
        		"dx10 specific textures are not supported", dds_filepath);
        return NULL;
    }

    /* detach memory and read bits */
    size_t cur_offset = fio_getpos(f);
    size_t bits_size = fio_getsize(f) - cur_offset;
    size_t file_size;
    uint8* file_data = (uint8*)fio_detachmem(f, &file_size, NULL);
    fio_close(f);

	gfx_texture tex = dds_create_texture(tmp_alloc, first_mipidx, srgb, &header,
        file_data + cur_offset, (uint)bits_size, thread_id);

	A_FREE(tmp_alloc, file_data);
	A_LOAD(tmp_alloc);

    if (thread_id != 0 && tex != NULL)    {
        gfx_delayed_waitforobjects(thread_id);
        gfx_delayed_fillobjects(thread_id);
    }

	return tex;
}

gfx_texture dds_create_texture(struct allocator* tmp_alloc, uint first_mipidx, int srgb,
		struct dds_header* header, uint8* bits, uint size, uint thread_id)
{
	uint width = header->width;
	uint height = header->height;
	uint depth = header->depth;
	uint mip_cnt = maxui(1, header->mip_cnt);
	uint first_mip = minui(first_mipidx, mip_cnt-1);
	uint target_mip_cnt = mip_cnt - first_mipidx;
	const uint cubemap_all = DDS_CUBEMAP_ALLFACES;
	enum gfxTextureType type;

	/* Which type of texture ? */
	if (BIT_CHECK(header->flags, DDS_FLAG_WIDTH) && !BIT_CHECK(header->flags, DDS_FLAG_HEIGHT))	{
		/* has width, no height: tex1d */
		type = gfxTextureType::TEX_1D;
	}    else if (BIT_CHECK(header->caps1, DDS_CAPS_COMPLEX) &&
				  BIT_CHECK(header->flags, DDS_FLAG_DEPTH) &&
				  BIT_CHECK(header->caps2, DDS_CAPS2_VOLUME))
	{
		type = gfxTextureType::TEX_3D;
	}    else    {
		/* determine cubemap, and set 6 surface arrays for texture */
		if (BIT_CHECK(header->caps1, DDS_CAPS_COMPLEX) && BIT_CHECK(header->caps2, cubemap_all))  {
			type = gfxTextureType::TEX_CUBE;
		}	else	{
			type = gfxTextureType::TEX_2D;
		}
	}

	/* Which format and bpp ? */
	enum gfxFormat surface_fmt = dds_get_format(&header->ddspf);
	if (surface_fmt == gfxFormat::UNKNOWN)        {
		/* check if we have d3d9 formats, like ARGB */
		if (dds_is_argb(&header->ddspf))    {
			/* swizzle to match engine's textures */
			surface_fmt = gfxFormat::RGBA_UNORM;
			for (uint i = 0; i < size; i += 4)    {
				uint8 tmpb = bits[i];
				bits[i] = bits[i+2];
				bits[i+2] = tmpb;
			}
		}
	}

	/* unsupported format */
	if (surface_fmt == gfxFormat::UNKNOWN)
		return NULL;

	/* Create Texture objects and it's mips */
	uint array_size = (type == gfxTextureType::TEX_CUBE) ? 6 : 1;
	uint subres_cnt = target_mip_cnt * array_size;
	struct gfx_subresource_data* data = (struct gfx_subresource_data*)A_ALLOC(tmp_alloc,
		sizeof(struct gfx_subresource_data)*subres_cnt, MID_GFX);
	if (data == NULL)
		return NULL;

	/* for each array, generate mips (how it is ordered in the resource)
	 * ArrayItem1 (mip1, mip2, ...), ArrayItem2 (mip1, mip2, ...)
	 * cubemap order:
	 * positive x, negative x, positive y, negative y, positive z, negative z
	 */
	uint bytes_cnt = 0;
	uint rowbyte_cnt = 0;
	uint row_cnt = 0;
	uint idx = 0;
	uint8* src_bits = bits;
    uint actual_size = 0;
	for (uint i = 0; i < array_size; i++)	{
		uint mip_idx = 0;

		for (uint k = 0; k  < mip_cnt; k++)        {
            int w = maxi(1, (int)floorf(width/powf(2.0f, (float)k)));
            int h = maxi(1, (int)floorf(height/powf(2.0f, (float)k)));

            dds_get_surfaceinfo(w, h, surface_fmt, &bytes_cnt, &rowbyte_cnt, &row_cnt);

			if (mip_idx >= first_mip)    {
				data[idx].p = src_bits;
				data[idx].size = bytes_cnt;
				data[idx].pitch_row = rowbyte_cnt;
				data[idx].pitch_slice = 0;
				idx ++;
			}

			mip_idx ++;
			src_bits += bytes_cnt;
            actual_size += bytes_cnt;
		}
	}

	/* shrink width, height to match first_mip */
    int w = maxi(1, (int)floorf(width/powf(2.0f, (float)first_mipidx)));
    int h = maxi(1, (int)floorf(height/powf(2.0f, (float)first_mipidx)));

	/* gfx texture creation */
	gfx_texture tex = gfx_create_texture(type,
			w, h, depth,
			srgb ? dds_conv_tosrgb(surface_fmt) : surface_fmt ,
			target_mip_cnt,
			1,
            actual_size,
			data,
            gfxMemHint::STATIC, thread_id);

	A_FREE(tmp_alloc, data);
	return tex;
}

enum gfxFormat dds_get_format(const struct dds_pixel_fmt* pf)
{
    if (BIT_CHECK(pf->flags, DDS_RGB))    {
        switch (pf->rgb_bitcnt)    {
        case 32:
            /* GFX_FORMAT_B8G8R8A8_UNORM_SRGB & GFX_FORMAT_B8G8R8X8_UNORM_SRGB should be
             * written using the DX10 extended header instead since these formats require
             * DXGI 1.1
             *
             * This code will use the fallback to swizzle RGB to BGR in memory for standard
             * DDS files which works on 10 and 10.1 devices with WDDM 1.0 drivers
             *
             * NOTE: We don't use GFX_FORMAT_B8G8R8X8_UNORM or GFX_FORMAT_B8G8R8X8_UNORM
             * here because they were defined for DXGI 1.0 but were not required for D3D10/10.1
             */
            if (ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0xff000000))
                return gfxFormat::RGBA_UNORM;
            if (ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0x00000000))
                return gfxFormat::RGBA_UNORM; /* No GFX_FORMAT_X8B8G8R8 in DXGI */
            if (ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000))
                return gfxFormat::R10G10B10A2_UNORM;
            if (ISBITMASK(0x0000ffff,0xffff0000,0x00000000,0x00000000))
                return gfxFormat::R16G16_UNORM;
            if (ISBITMASK(0xffffffff,0x00000000,0x00000000,0x00000000))
                return gfxFormat::R32_FLOAT; /* D3DX writes this out as a FourCC of 114 */
            break;

        case 24:
            /* No 24bpp DXGI formats */
            break;

        case 16:
            /* 5:5:5 & 5:6:5 formats are defined for DXGI, but are deprecated for D3D10+ */
        	break;
        }
    }    else if (BIT_CHECK(pf->flags, DDS_LUMINANCE))    {
        if (8 == pf->rgb_bitcnt)   {
            if (ISBITMASK(0x000000ff,0x00000000,0x00000000,0x00000000))
                return gfxFormat::R8_UNORM; /* D3DX10/11 writes this out as DX10 extension */
        }
        if (16 == pf->rgb_bitcnt)     {
            if (ISBITMASK(0x0000ffff,0x00000000,0x00000000,0x00000000))
                return gfxFormat::R16_UNORM; /* D3DX10/11 writes this out as DX10 extension (not supported) */
            if (ISBITMASK(0x000000ff,0x00000000,0x00000000,0x0000ff00))
                return gfxFormat::R8G8_UNORM; /* D3DX10/11 writes this out as DX10 extension (not suppoerted) */
        }
    }	else if (BIT_CHECK(pf->flags, DDS_FOURCC))    {
        if (MAKEFOURCC('D', 'X', 'T', '1') == pf->fourcc)   return gfxFormat::BC1;
        if (MAKEFOURCC('D', 'X', 'T', '3') == pf->fourcc)   return gfxFormat::BC2;
        if (MAKEFOURCC('D', 'X', 'T', '5') == pf->fourcc)   return gfxFormat::BC3;
        if (MAKEFOURCC('A', 'T', 'I', '1') == pf->fourcc)   return gfxFormat::BC4;
        if (MAKEFOURCC('A', 'T', 'I', '2') == pf->fourcc)   return gfxFormat::BC5;

        /* Check for D3DFORMAT enums being set here */
        switch (pf->fourcc)     {
        case DDS_FMT_A16B16G16R16:  return gfxFormat::R16G16B16A16_UNORM;
        case DDS_FMT_Q16W16V16U16:  return gfxFormat::R16G16B16A16_SNORM;
        case DDS_FMT_R16F:          return gfxFormat::R16_FLOAT;
        case DDS_FMT_G16R16F:       return gfxFormat::R16G16_FLOAT;
        case DDS_FMT_A16B16G16R16F: return gfxFormat::R16G16B16A16_FLOAT;
        case DDS_FMT_R32F:          return gfxFormat::R32_FLOAT;
        case DDS_FMT_G32R32F:       return gfxFormat::R32G32_FLOAT;
        case DDS_FMT_A32B32G32R32F: return gfxFormat::R32G32B32A32_FLOAT;
        }
    }

    return gfxFormat::UNKNOWN;
}


int dds_is_argb(const struct dds_pixel_fmt* pf)
{
    if (BIT_CHECK(pf->flags, DDS_RGB) && pf->rgb_bitcnt == 32)    {
        if (ISBITMASK(0x00ff0000,0x0000ff00,0x000000ff,0xff000000))
        	return TRUE;
        if (ISBITMASK(0x00ff0000,0x0000ff00,0x000000ff,0x00000000))
        	return TRUE;
    }
    return FALSE;
}

void dds_get_surfaceinfo(uint width, uint height, enum gfxFormat fmt,
		OUT uint* size, OUT uint* rowsize, OUT uint* rowcnt)
{
    uint bytes_cnt = 0;
    uint rowbytes_cnt = 0;
    uint row_cnt = 0;

    int bc = TRUE;	/* block-compression */
    int bytes_per_block = 16;
    switch (fmt)
    {
    case gfxFormat::BC1:
    case gfxFormat::BC1_SRGB:
    case gfxFormat::BC4:
    case gfxFormat::BC4_SNORM:
        bytes_per_block = 8;
        break;

    case gfxFormat::BC2:
    case gfxFormat::BC2_SRGB:
    case gfxFormat::BC3:
    case gfxFormat::BC3_SRGB:
    case gfxFormat::BC5:
    case gfxFormat::BC5_SNORM:
    /*
    case GFX_FORMAT_BC6H_TYPELESS:
    case GFX_FORMAT_BC6H_UF16:
    case GFX_FORMAT_BC6H_SF16:
    case GFX_FORMAT_BC7_TYPELESS:
    case GFX_FORMAT_BC7_UNORM:
    case GFX_FORMAT_BC7_UNORM_SRGB:
    */
        break;

    default:
        bc = FALSE;
        break;
    }

    if (bc)    {
        uint blocks_wide = 0;
        uint blocks_high = 0;
        if (width > 0)      blocks_wide = maxui(1, width / 4);
        if (height > 0)     blocks_high = maxui(1, height / 4);
        rowbytes_cnt = blocks_wide * bytes_per_block;
        row_cnt = blocks_high;
    }    else    {
        uint bpp = gfx_texture_getbpp(fmt);
        rowbytes_cnt = (width * bpp + 7) / 8; /* round up to nearest byte */
        row_cnt = height;
    }
    bytes_cnt = rowbytes_cnt * row_cnt;

    *size = bytes_cnt;
    *rowsize = rowbytes_cnt;
    *rowcnt = row_cnt;
}

uint gfx_texture_getbpp(enum gfxFormat fmt)
{
    switch (fmt)
    {
    case gfxFormat::R32G32B32A32_FLOAT:
    case gfxFormat::R32G32B32A32_UINT:
    case gfxFormat::R32G32B32A32_SINT:
        return 128;

    case gfxFormat::R32G32B32_FLOAT:
    case gfxFormat::R32G32B32_UINT:
    case gfxFormat::R32G32B32_SINT:
        return 96;

    case gfxFormat::R16G16B16A16_FLOAT:
    case gfxFormat::R16G16B16A16_UNORM:
    case gfxFormat::R16G16B16A16_UINT:
    case gfxFormat::R16G16B16A16_SNORM:
    case gfxFormat::R16G16B16A16_SINT:
    case gfxFormat::R32G32_FLOAT:
    case gfxFormat::R32G32_UINT:
    case gfxFormat::R32G32_SINT:
        return 64;

    case gfxFormat::R10G10B10A2_UNORM:
    case gfxFormat::R10G10B10A2_UINT:
    case gfxFormat::R11G11B10_FLOAT:
    case gfxFormat::RGBA_UNORM:
    case gfxFormat::RGBA_UNORM_SRGB:
    case gfxFormat::R16G16_FLOAT:
    case gfxFormat::R16G16_UNORM:
    case gfxFormat::R16G16_UINT:
    case gfxFormat::R16G16_SNORM:
    case gfxFormat::R16G16_SINT:
    case gfxFormat::R32_FLOAT:
    case gfxFormat::R32_UINT:
    case gfxFormat::R32_SINT:
    case gfxFormat::DEPTH24_STENCIL8:
    case gfxFormat::DEPTH32:
        return 32;

    case gfxFormat::R8G8_UNORM:
    case gfxFormat::R8G8_UINT:
    case gfxFormat::R8G8_SNORM:
    case gfxFormat::R8G8_SINT:
    case gfxFormat::R16_FLOAT:
    case gfxFormat::R16_UNORM:
    case gfxFormat::R16_UINT:
    case gfxFormat::R16_SNORM:
    case gfxFormat::R16_SINT:
    case gfxFormat::DEPTH16:
        return 16;

    case gfxFormat::R8_UNORM:
    case gfxFormat::R8_UINT:
    case gfxFormat::R8_SNORM:
    case gfxFormat::R8_SINT:
        return 8;

    case gfxFormat::BC1:
    case gfxFormat::BC1_SRGB:
        return 4;

    case gfxFormat::BC2:
    case gfxFormat::BC2_SRGB:
    case gfxFormat::BC3:
    case gfxFormat::BC3_SRGB:
    case gfxFormat::BC4:
    case gfxFormat::BC4_SNORM:
    case gfxFormat::BC5:
    case gfxFormat::BC5_SNORM:
    /*
    case GFX_FORMAT_BC6H_TYPELESS:
    case GFX_FORMAT_BC6H_UF16:
    case GFX_FORMAT_BC6H_SF16:
    case GFX_FORMAT_BC7_TYPELESS:
    case GFX_FORMAT_BC7_UNORM:
    case GFX_FORMAT_BC7_UNORM_SRGB:
    */
        return 8;

    default:
    	return 0;
    }
}

enum gfxFormat dds_conv_tosrgb(enum gfxFormat fmt)
{
    switch (fmt)        {
    case gfxFormat::BC1:      	return gfxFormat::BC1_SRGB;
    case gfxFormat::BC2:      	return gfxFormat::BC2_SRGB;
    case gfxFormat::BC3:      	return gfxFormat::BC3_SRGB;
    case gfxFormat::RGBA_UNORM: return gfxFormat::RGBA_UNORM_SRGB;
    default:                    return fmt;
    }
}
