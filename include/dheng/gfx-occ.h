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

#ifndef __GFXOCC_H__
#define __GFXOCC_H__

struct gfx_model_occ;

/* occlusion culling */
#include "dhcore/types.h"
#include "dhcore/vec-math.h"

struct sphere;

/* */
void gfx_occ_zero();
result_t gfx_occ_init(uint width, uint height, uint cpu_caps);
void gfx_occ_release();
void gfx_occ_setviewport(int x, int y, int width, int height);
void gfx_occ_setmatrices(const struct mat4f* viewproj);
void gfx_occ_clear();
void gfx_occ_drawoccluder(struct allocator* tmp_alloc, struct gfx_model_occ* occ,
    const struct mat3f* world);
int gfx_occ_testbounds(const struct sphere* s, const struct vec3f* xaxis,
    const struct vec3f* yaxis, const struct vec3f* campos);
void gfx_occ_finish(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);
float gfx_occ_getfar();

#endif /* __GFX-OCC_H__ */
