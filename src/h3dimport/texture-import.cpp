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

#include <stdio.h>
#include "dhcore/core.h"
#include "stb_image/stb_image.h"
#define NVTT_SHARED
#include "nvtt/nvtt.h"
#include "texture-import.h"

/*************************************************************************************************
 * inlines
 */
INLINE nvtt::Format import_get_nvttfmt(char* str_type, enum h3d_texture_type type, bool_t has_alpha,
		bool_t force_bc2)
{
	switch (type)	{
	case H3D_TEXTURE_GLOSS:
		strcpy(str_type, "ATI1N");
		return nvtt::Format_BC4;
	case H3D_TEXTURE_DIFFUSE:
		if (has_alpha && !force_bc2)	{
			strcpy(str_type, "DXTC5");
			return nvtt::Format_BC3;	/* for low-freq alpha */
		}	else if (has_alpha && force_bc2)	{
			strcpy(str_type, "DXTC3");
			return nvtt::Format_BC2; /* for hi-freq alpha */
		}	else	{
			strcpy(str_type, "DXTC1");
			return nvtt::Format_BC1;
		}
	case H3D_TEXTURE_NORMAL:
		strcpy(str_type, "ATI2N");
		return nvtt::Format_BC5;
	case H3D_TEXTURE_EMISSIVE:
		strcpy(str_type, "DXTC1");
		return nvtt::Format_BC1;
	case H3D_TEXTURE_REFLECTION:
		strcpy(str_type, "DXTC1");
		return nvtt::Format_BC1;
	case H3D_TEXTURE_ALPHAMAP:
		strcpy(str_type, "ATI1N");
		return nvtt::Format_BC4;
	default:
	return nvtt::Format_RGBA;
	}
}

/*************************************************************************************************
 * output handler
 */
struct tex_output_handler : public nvtt::OutputHandler
{
	tex_output_handler(const char* name, uint64 total_sz)
	{
		file = fopen(name, "wb");
		total = total_sz + 128;
		progress = 0;
		percent = 0;
	}

	virtual ~tex_output_handler()
	{
		if (file != NULL)
			fclose(file);
	}

	bool_t is_open() const
	{
		return (file != NULL);
	}

	uint64 get_filesize() const
	{
		return progress;
	}

	/* from nvtt::OutputHandler */
	virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel)
	{
	}

	virtual bool writeData(const void* data, int size)
	{
		fwrite(data, size, 1, file);
		progress += size;
#if !defined(_DEBUG_)
		uint p = (uint)((100 * progress)/total);
		if (p != percent)	{
			percent = p;
			printf("  %d%%\r", p);
			fflush(stdout);
		}
#endif
		return true;
	}


private:
	uint64 total;
	uint64 progress;
	uint percent;
	FILE* file;
};

/*************************************************************************************************/
bool_t import_process_texture(const char* img_filepath, enum h3d_texture_type type,
		const struct import_params* params, char* img_filename, struct import_texture_info* info)
{
	char img_ext[DH_PATH_MAX];
	char img_outpath[DH_PATH_MAX];

	/* construct output image path */
    path_join(img_outpath, params->texture_dir,
        strcat(path_getfilename(img_filename, img_filepath), ".dds"), NULL);

	/* if file extension is DDS, just copy it */
	path_getfileext(img_ext, img_filepath);
	if (str_isequal_nocase(img_ext, "dds"))	{
		printf(TERM_WHITE "  image %s (copy): %s ...\n" TERM_RESET,
				import_get_textureusage(type), img_filepath);
		return util_copyfile(img_outpath, img_filepath);
	}

	int width, height, n;
	/* load image and force conversion to 32bpp */
	stbi_uc* img_data = stbi_load(img_filepath, &width, &height, &n, 4);
	if (img_data == NULL)	{
		printf(TERM_BOLDYELLOW "Warning: loading source image '%s' failed\n" TERM_RESET,
				img_filepath);
		return FALSE;
	}

	/* swizzle R and B, because nvtt accept BGRA data */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char* pixel = img_data + (x + y*width)*4;
            unsigned char tmp = pixel[2];
            pixel[2] = pixel[0];
            pixel[0] = tmp;
        }
    }

    char str_type[32];
    nvtt::Format fmt = import_get_nvttfmt(str_type, type, n == 4, params->tex_params.force_dxt3);
	printf(TERM_WHITE "  image %s (%s): %s\n" TERM_RESET,
			import_get_textureusage(type), str_type, img_filepath);
	nvtt::Compressor compressor;
	nvtt::InputOptions input;
	nvtt::CompressionOptions compress;
	nvtt::OutputOptions output;

	input.setTextureLayout(nvtt::TextureType_2D, width, height);
	input.setMipmapData(img_data, width, height);
	input.setAlphaMode((n == 4) ? nvtt::AlphaMode_Transparency : nvtt::AlphaMode_None);
	input.setNormalMap((fmt == nvtt::Format_BC5) ? true : false);
	compress.setFormat(fmt);
	if (params->tex_params.fast_mode)
		compress.setQuality(nvtt::Quality_Fastest);
	else
		compress.setQuality(nvtt::Quality_Normal);

	/* open target file for writing */
    /* write to temp file */
    char img_outpath_tmp[DH_PATH_MAX];
    strcat(strcpy(img_outpath_tmp, img_outpath), ".tmp");
	uint64 estimate_sz = compressor.estimateSize(input, compress);
	struct tex_output_handler* output_hdl = new tex_output_handler(img_outpath_tmp, estimate_sz);
	if (!output_hdl->is_open())	{
		printf(TERM_BOLDYELLOW "Warning: target image '%s' could not be created\n" TERM_RESET,
				img_outpath);
		stbi_image_free(img_data);
		delete output_hdl;
		return FALSE;
	}

	output.setOutputHandler(output_hdl);
	bool r = compressor.process(input, compress, output);

	if (info != NULL)	{
		info->width = width;
		info->height = height;
		info->size = output_hdl->get_filesize();
	}

	delete output_hdl;
	stbi_image_free(img_data);

    /* move file */
    if (!r)	{
        printf(TERM_BOLDYELLOW "Warning: compressing source image '%s' failed\n" TERM_RESET,
            img_filepath);
        util_delfile(img_outpath_tmp);
    }   else    {
        util_movefile(img_outpath, img_outpath_tmp);
    }
	return r ? TRUE : FALSE;
}

const char* import_get_textureusage(enum h3d_texture_type type)
{
	switch (type)	{
	case H3D_TEXTURE_GLOSS:
		return "ambient";
	case H3D_TEXTURE_DIFFUSE:
		return "diffuse";
	case H3D_TEXTURE_NORMAL:
		return "normal";
	case H3D_TEXTURE_EMISSIVE:
		return "emissive";
	case H3D_TEXTURE_REFLECTION:
		return "reflection";
	case H3D_TEXTURE_ALPHAMAP:
		return "alpha";
	default:
		return "";
	}
}

bool_t import_texture(const struct import_params* params)
{
    struct import_texture_info info;
    char filename[128];

    if (import_process_texture(params->in_filepath, params->ttype, params, filename, &info)) {
        printf(TERM_BOLDWHITE "  ok: %s - %dkb (%dx%d)\n" TERM_RESET, filename,
            (int)(info.size/1024), info.width, info.height);
        return TRUE;
    }

    return FALSE;
}
