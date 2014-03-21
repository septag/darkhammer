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

#include "dhcore/core.h"

#include "cmp-mgr.h"
#include "gfx-canvas.h"
#include "scene-mgr.h"

#include "components/cmp-bounds.h"
#include "components/cmp-xform.h"


/*************************************************************************************************
 * fwd declarations
 */
result_t cmp_bounds_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_bounds_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_bounds_update(cmp_t c, float dt, void* params);
void cmp_bounds_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
	    const struct gfx_view_params* params);

/*************************************************************************************************/
result_t cmp_bounds_register(struct allocator* alloc)
{
	struct cmp_createparams params;
	memset(&params, 0x00, sizeof(params));

	params.name = "bounds";
	params.stride = sizeof(struct cmp_bounds);
	params.initial_cnt = 500;
	params.grow_cnt = 100;
	params.values = cmp_bounds_values;
	params.value_cnt = CMP_VALUE_CNT(cmp_bounds_values);
	params.type = cmp_bounds_type;
	params.create_func = cmp_bounds_create;
	params.destroy_func = cmp_bounds_destroy;
	params.debug_func = cmp_bounds_debug;
	params.update_funcs[CMP_UPDATE_STAGE4] = cmp_bounds_update;

	return cmp_register_component(alloc, &params);
}

result_t cmp_bounds_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	struct cmp_bounds* b = (struct cmp_bounds*)data;
	aabb_setzero(&b->ws_aabb);
	host_obj->bounds_cmp = hdl;

    /* push into spatial structure of the scene */
    scn_push_spatial(host_obj->scene_id, hdl);
	return RET_OK;
}

void cmp_bounds_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
    /* pull from spatial structure */
    scn_pull_spatial(host_obj->scene_id, hdl);

	host_obj->bounds_cmp = INVALID_HANDLE;
}

void cmp_bounds_update(cmp_t c, float dt, void* params)
{
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
	for (uint i = 0; i < cnt; i++)	{
		const struct cmp_instance_desc* inst = updates[i];

		/* update bounding volume in world-space from transform component */
		struct cmp_bounds* b = (struct cmp_bounds*)inst->data;
		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(inst->host->xform_cmp);
		sphere_xform(&b->ws_s, &b->s, &xf->ws_mat);
		aabb_from_sphere(&b->ws_aabb, &b->ws_s);

		/* update spatial (update will happen on visible query - see scn-mgr.c) */
        scn_update_spatial(inst->host->scene_id, inst->host->bounds_cmp);
	}
}

void cmp_bounds_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
	    const struct gfx_view_params* params)
{
    struct cmp_bounds* b = (struct cmp_bounds*)data;
    gfx_canvas_setwireframe(TRUE, TRUE);
    gfx_canvas_setztest(FALSE);
    gfx_canvas_setfillcolor_solid(&g_color_white);
    gfx_canvas_boundsphere(&b->ws_s, &params->viewproj, &params->view, TRUE);
    gfx_canvas_setztest(TRUE);
}

result_t cmp_bounds_modify(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	if (obj->xform_cmp != INVALID_HANDLE)	{
		cmp_updateinstance(obj->xform_cmp);

		/* update bounding volume in world-space from transform component */
		struct cmp_bounds* b = (struct cmp_bounds*)data;
		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
		sphere_xform(&b->ws_s, &b->s, &xf->ws_mat);
		aabb_from_sphere(&b->ws_aabb, &b->ws_s);
	}
	return RET_OK;
}

struct vec4f* cmp_bounds_getfarcorner(struct vec4f* r, cmphandle_t bounds_hdl,
		const struct vec4f* cam_pos)
{
	float dist_min = FL32_MAX;
	uint farpt_idx = 0;
	struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(bounds_hdl);
	struct vec4f corners[8];
	aabb_getptarr(corners, &b->ws_aabb);

	for (uint i = 0; i < 8; i++)	{
		float dist = vec3_dot(&corners[i], cam_pos);
		if (dist < dist_min)	{
			dist_min = dist;
			farpt_idx = i;
		}
	}

	return vec4_setv(r, &corners[farpt_idx]);
}
