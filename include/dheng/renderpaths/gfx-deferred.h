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

#ifndef __GFXDEFERRED_H__
#define __GFXDEFERRED_H__

#include "../gfx-types.h"

struct gfx_rpath_result;
enum cmp_obj_type;
struct gfx_batch_item;

enum gfx_deferred_preview_mode
{
    GFX_DEFERRED_PREVIEW_ALBEDO = 0,
    GFX_DEFERRED_PREVIEW_SPECULAR = 1,
    GFX_DEFERRED_PREVIEW_NORMALS = 2,
    GFX_DEFERRED_PREVIEW_NORMALS_NOMAP = 3,
    GFX_DEFERRED_PREVIEW_DEPTH = 4,
    GFX_DEFERRED_PREVIEW_MTL = 5,
    GFX_DEFERRED_PREVIEW_GLOSS = 6,
    GFX_DEFERRED_PREVIEW_NONE = 0xffffffff
};

/* callbacks */
uint gfx_deferred_getshader(enum cmp_obj_type obj_type, uint rpath_flags);
result_t gfx_deferred_init(uint width, uint height);
void gfx_deferred_release();
void gfx_deferred_render(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
        const struct gfx_view_params* params, struct gfx_batch_item* batch_items, uint batch_cnt,
        void* userdata, OUT struct gfx_rpath_result* result);
result_t gfx_deferred_resize(uint width, uint height);

/* misc */
void gfx_deferred_setpreview(enum gfx_deferred_preview_mode mode);

#endif /* __GFXDEFERRED_H__ */
