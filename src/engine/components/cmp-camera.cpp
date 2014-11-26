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
#include "components/cmp-camera.h"
#include "components/cmp-xform.h"

#include "cmp-mgr.h"
#include "gfx-canvas.h"
#include "world-mgr.h"

/*************************************************************************************************
 * fwd declarations
 */
result_t cmp_camera_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_camera_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_camera_update(cmp_t c, float dt, void* params);
void cmp_camera_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
	    const struct gfx_view_params* params);

/* globals */
struct cmp_obj* g_active_camobj = NULL;

/*************************************************************************************************/
result_t cmp_camera_register(struct allocator* alloc)
{
	struct cmp_createparams params;
	memset(&params, 0x00, sizeof(params));

	params.name = "Camera";
	params.stride = sizeof(struct cmp_camera);
	params.initial_cnt = 10;
	params.grow_cnt = 10;
	params.values = cmp_camera_values;
	params.value_cnt = CMP_VALUE_CNT(cmp_camera_values);
	params.type = cmp_camera_type;
	params.create_func = cmp_camera_create;
	params.destroy_func = cmp_camera_destroy;
	params.debug_func = cmp_camera_debug;
	params.update_funcs[CMP_UPDATE_STAGE4] = cmp_camera_update;

	return cmp_register_component(alloc, &params);
}

result_t cmp_camera_modifytype(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	struct cmp_camera* cam = (struct cmp_camera*)data;
	if (cam->type == CMP_CAMERA_ROLLPITCH_CONSTRAINED)	{
		cam_update(&cam->c);

		/* save pitch/yaw of the transform */
		if (obj->xform_cmp != INVALID_HANDLE)	{
			float roll;
			struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
			mat3_get_roteuler(&cam->c.pitch_cur, &cam->c.yaw_cur, &roll, &xf->mat);
		}

	    cam_set_pitchconst(&cam->c, TRUE, cam->pitch_min, cam->pitch_max);
	}	else	{
	    cam_set_pitchconst(&cam->c, FALSE, 0.0f, 0.0f);
	}

	cmp_updateinstance(cur_hdl);
	return RET_OK;
}

result_t cmp_camera_modifyproj(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	struct cmp_camera* cam = (struct cmp_camera*)data;
	cam->c.fov = math_torad(cam->fov);
	cam->c.ffar = cam->ffar;
	cam->c.fnear = cam->fnear;
	cmp_updateinstance(cur_hdl);
	return RET_OK;
}

result_t cmp_camera_modifyconstraint(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	struct cmp_camera* cam = (struct cmp_camera*)data;
	if (cam->type == CMP_CAMERA_ROLLPITCH_CONSTRAINED)	{
		if (cam->pitch_max < cam->pitch_min)
			cam->pitch_max = cam->pitch_min;
	    cam_set_pitchconst(&cam->c, TRUE, math_torad(cam->pitch_min), math_torad(cam->pitch_max));
	}
	return RET_OK;
}

result_t cmp_camera_modifybindpath(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	/* TODO */
	return RET_OK;
}

result_t cmp_camera_modifyactive(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_camera* cam = (struct cmp_camera*)data;

    if (cam->active)    {
	    wld_set_cam(&cam->c);
        if (g_active_camobj != NULL)    {
            cmphandle_t prev_hdl = cmp_findinstance(g_active_camobj->chain, cmp_camera_type);
            if (prev_hdl != INVALID_HANDLE)
                ((struct cmp_camera*)cmp_getinstancedata(prev_hdl))->active = FALSE;
        }

        g_active_camobj = obj;
    }

	return RET_OK;
}

result_t cmp_camera_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	/* default Camera */
	struct cmp_camera* cam = (struct cmp_camera*)data;
    cam->type = CMP_CAMERA_ROLLPITCH_CONSTRAINED;
    cam->fov = math_torad(60.0f);
    cam->fnear = 0.5f;
    cam->ffar = 1000.0f;
    cam->pitch_max = PI_HALF - EPSILON - math_torad(5.0f);
    cam->pitch_min = -PI_HALF + EPSILON + math_torad(5.0f);
    cam_init(&cam->c, &g_vec3_zero, &g_vec3_unitz, cam->fnear, cam->ffar, cam->fov);
    cam_set_pitchconst(&cam->c, TRUE, cam->pitch_min, cam->pitch_max);

    if (host_obj->xform_cmp != INVALID_HANDLE)	{
		float roll;
		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(host_obj->xform_cmp);
		mat3_get_roteuler(&cam->c.pitch_cur, &cam->c.yaw_cur, &roll, &xf->mat);
    }

	return RET_OK;
}

void cmp_camera_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	if (host_obj == g_active_camobj)    {
        struct cmp_camera* c = (struct cmp_camera*)data;
        if (c->active)
            wld_set_cam(NULL);
        c->active = FALSE;
    }
}

void cmp_camera_update(cmp_t c, float dt, void* params)
{
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);

	for (uint i = 0; i < cnt; i++)	{
		const struct cmp_instance_desc* inst = updates[i];
		struct cmp_camera* cam = (struct cmp_camera*)inst->data;
		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(inst->host->xform_cmp);

		/* update Camera from transform component */
		quat_frommat3(&cam->c.rot, &xf->ws_mat);
		mat3_get_trans(&cam->c.pos, &xf->ws_mat);

		float roll;
		mat3_get_roteuler(&cam->c.pitch_cur, &cam->c.yaw_cur, &roll, &xf->mat);

		cam_update(&cam->c);
	}
}

void cmp_camera_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
	    const struct gfx_view_params* params)
{
	gfx_canvas_cam(&((struct cmp_camera*)data)->c, &params->cam_pos, &params->viewproj, TRUE);
}
