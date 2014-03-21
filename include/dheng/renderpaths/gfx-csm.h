/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
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

#ifndef GFX_CSM_H_
#define GFX_CSM_H_

#include "../gfx-types.h"
#include "dhcore/prims.h"

/* fwd */
enum cmp_obj_type;
struct gfx_rpath_result;
struct gfx_batch_item;

/* callback implementations */
uint gfx_csm_getshader(enum cmp_obj_type obj_type, uint rpath_flags);
result_t gfx_csm_init(uint width, uint height);
void gfx_csm_release();
void gfx_csm_render(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
        const struct gfx_view_params* params, struct gfx_batch_item* batch_items, uint batch_cnt,
        void* userdata, OUT struct gfx_rpath_result* result);
result_t gfx_csm_resize(uint width, uint height);

/* internal use */
void gfx_csm_prepare(const struct gfx_view_params* params, const struct vec3f* light_dir,
    const struct aabb* world_bounds);

uint gfx_csm_get_cascadecnt();
const struct aabb* gfx_csm_get_frustumbounds();
const struct mat4f* gfx_csm_get_shadowmats();
gfx_texture gfx_csm_get_shadowtex();
const struct vec4f* gfx_csm_get_cascades(const struct mat3f* view);

#endif /* GFX_CSM_H_ */
