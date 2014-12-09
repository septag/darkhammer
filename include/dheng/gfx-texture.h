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

#ifndef GFX_TEXTURE_H_
#define GFX_TEXTURE_H_

#include "dhcore/types.h"
#include "gfx-types.h"
#include "dhcore/allocator.h"
#include "dhcore/file-io.h"

gfx_texture gfx_texture_loaddds(const char* dds_filepath, uint first_mipidx,
		int srgb, uint thread_id);

uint gfx_texture_getbpp(gfxFormat fmt);


#endif /* GFX_TEXTURE_H_ */
