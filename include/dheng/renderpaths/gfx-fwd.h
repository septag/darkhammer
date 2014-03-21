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


#ifndef GFX_FWD_H_
#define GFX_FWD_H_

struct gfx_rpath_result;

/**
 * forward rendering path
 */

uint gfx_fwd_getshader(enum cmp_obj_type obj_type, uint rpath_flags);
result_t gfx_fwd_init(uint width, uint height);
void gfx_fwd_release();
void gfx_fwd_render(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
		const struct gfx_view_params* params, struct gfx_batch_item* batch_items, uint batch_cnt,
		void* userdata, OUT struct gfx_rpath_result* result);
result_t gfx_fwd_resize(uint width, uint height);

#endif /* GFX_FWD_H_ */
