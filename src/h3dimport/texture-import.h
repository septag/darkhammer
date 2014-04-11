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


#ifndef TEXTURE_IMPORT_H_
#define TEXTURE_IMPORT_H_

#include "dheng/h3d-types.h"
#include "h3dimport.h"

struct import_texture_info
{
	uint width;
	uint height;
	uint64 size;
};

_EXTERN_ int import_process_texture(const char* img_filepath, enum h3d_texture_type type,
	const struct import_params* params, char* img_filename, struct import_texture_info* info);
_EXTERN_ const char* import_get_textureusage(enum h3d_texture_type type);
_EXTERN_ int import_texture(const struct import_params* params);

#endif /* TEXTURE_IMPORT_H_ */
