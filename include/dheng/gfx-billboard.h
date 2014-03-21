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

#ifndef __GFXBILLBOARD_H__
#define __GFXBILLBOARD_H__

#include "gfx-types.h"

void gfx_blb_zero();
result_t gfx_blb_init();
void gfx_blb_release();

void gfx_blb_push(gfx_cmdqueue cmdqueue,
    const struct vec4f* pos, const struct color* color,
    float sx, float sy, gfx_texture tex,
    const struct vec4f* texcoord);
void gfx_blb_render(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);

#endif /* __GFX-BILLBOARD_H__ */
