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

#include "components/cmp-xform.h"
#include "components/cmp-light.h"
#include "components/cmp-camera.h"
#include "components/cmp-model.h"
#include "components/cmp-rbody.h"
#include "components/cmp-trigger.h"
#include "components/cmp-attachdock.h"

#include "cmp-mgr.h"
#include "gfx-canvas.h"
#include "phx-device.h"

/*************************************************************************************************
 * fwd declarations
 */
result_t cmp_xform_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_xform_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_xform_update1(cmp_t c, float dt, void* params);
void cmp_xform_update2(cmp_t c, float dt, void* params);
void cmp_xform_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
	    const struct gfx_view_params* params);

void cmp_xform_setm(struct cmp_obj* obj, const struct mat3f* mat);
void cmp_xform_setx(struct cmp_obj* obj, const struct xform3d* xform);
void cmp_xform_updatedeps(struct cmp_obj* obj, uint flags);

/*************************************************************************************************/
result_t cmp_xform_register(struct allocator* alloc)
{
	struct cmp_createparams params;
	memset(&params, 0x00, sizeof(params));

	params.name = "transform";
	params.stride = sizeof(struct cmp_xform);
	params.initial_cnt = 500;
	params.grow_cnt = 500;
	params.values = cmp_xform_values;
	params.value_cnt = CMP_VALUE_CNT(cmp_xform_values);
	params.type = cmp_xform_type;
	params.create_func = cmp_xform_create;
	params.destroy_func = cmp_xform_destroy;
	params.debug_func = cmp_xform_debug;
	params.update_funcs[CMP_UPDATE_STAGE2] = cmp_xform_update1; /* dependency update (mesh, bounds)*/
	params.update_funcs[CMP_UPDATE_STAGE3] = cmp_xform_update2; /* world-space calc */

	return cmp_register_component(alloc, &params);
}


result_t cmp_xform_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	struct cmp_xform* xf = (struct cmp_xform*)data;
	mat3_setidentity(&xf->mat);
	vec3_setzero(&xf->vel_lin);
	vec3_setzero(&xf->vel_ang);
	mat3_setidentity(&xf->ws_mat);
	xf->parent_hdl = INVALID_HANDLE;
	if (!BIT_CHECK(cmp_getinstanceflags(hdl), CMP_INSTANCEFLAG_INDIRECTHOST))
		host_obj->xform_cmp = hdl;
	return RET_OK;
}

void cmp_xform_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	if (!BIT_CHECK(cmp_getinstanceflags(hdl), CMP_INSTANCEFLAG_INDIRECTHOST))	{
		/* because transform component is very basic and can be used in the other components ..
		 * we should invalidate it on them too
		 */

		/* used in mesh ? */
		if (host_obj->model_cmp != INVALID_HANDLE)	{
			struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(host_obj->model_cmp);
			m->xforms[0] = INVALID_HANDLE;
		}

		/* used in attach dock ? */
		if (host_obj->attachdock_cmp != INVALID_HANDLE)
            cmp_attachdock_unlinkall(host_obj->attachdock_cmp);

		host_obj->xform_cmp = INVALID_HANDLE;
	}
}

result_t cmp_xform_modify(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	cmp_xform_setm(obj, &((struct cmp_xform*)data)->mat);
	return RET_OK;
}

void cmp_xform_update1(cmp_t c, float dt, void* params)
{
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
	for (uint i = 0; i < cnt; i++)	{
		const struct cmp_instance_desc* inst = updates[i];
		cmp_xform_updatedeps(inst->host, inst->flags);
	}
}

void cmp_xform_updatedeps(struct cmp_obj* obj, uint flags)
{
	cmphandle_t dep_hdl;
	if (!BIT_CHECK(flags, CMP_INSTANCEFLAG_INDIRECTHOST))	{
		if ((dep_hdl = obj->bounds_cmp) != INVALID_HANDLE)
			cmp_updateinstance(dep_hdl);
		if ((dep_hdl = obj->model_cmp) != INVALID_HANDLE)
			cmp_updateinstance(dep_hdl);
		if ((dep_hdl = obj->rbody_cmp) != INVALID_HANDLE)
			cmp_updateinstance(dep_hdl);
        if ((dep_hdl = obj->trigger_cmp) != INVALID_HANDLE)
            cmp_updateinstance(dep_hdl);
        if ((dep_hdl = obj->attachdock_cmp) != INVALID_HANDLE)
            cmp_updateinstance(dep_hdl);

		switch (obj->type)	{
		case CMP_OBJTYPE_LIGHT:
			cmp_updateinstance(cmp_findinstance(obj->chain, cmp_light_type));
			break;
		case CMP_OBJTYPE_CAMERA:
			cmp_updateinstance(cmp_findinstance(obj->chain, cmp_camera_type));
			break;
		default:
			break;
		}
	}
}

void cmp_xform_update2(cmp_t c, float dt, void* params)
{
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
	for (uint i = 0; i < cnt; i++)	{
		const struct cmp_instance_desc* inst = updates[i];
		struct cmp_xform* xf = (struct cmp_xform*)inst->data;

		/* calculate world matrix */
		struct mat3f wsmat;
		cmphandle_t parent_hdl = xf->parent_hdl;
		mat3_setm(&wsmat, &xf->mat);
		while (parent_hdl != INVALID_HANDLE)	{
			struct cmp_xform* parent_xf = (struct cmp_xform*)cmp_getinstancedata(parent_hdl);
			mat3_mul(&wsmat, &wsmat, &parent_xf->mat);
			parent_hdl = parent_xf->parent_hdl;
		}
		mat3_setm(&xf->ws_mat, &wsmat);
	}
}

void cmp_xform_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
	    const struct gfx_view_params* params)
{
	struct cmp_xform* xf = (struct cmp_xform*)data;
    gfx_canvas_setztest(FALSE);
	gfx_canvas_coords(&xf->ws_mat, &params->cam_pos, 0.3f);
}

void cmp_xform_setx(struct cmp_obj* obj, const struct xform3d* xform)
{
	struct mat3f mat;
	xform3d_getmat(&mat, xform);
	cmp_xform_setm(obj, &mat);
}

void cmp_xform_setm(struct cmp_obj* obj, const struct mat3f* mat)
{
	struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);

    /* update world-mat */
	struct mat3f ws_mat;
	mat3_setm(&xf->mat, mat);
	mat3_setm(&ws_mat, mat);
	cmphandle_t parent_hdl = xf->parent_hdl;
	while (parent_hdl != INVALID_HANDLE)	{
		struct cmp_xform* parent_xf = (struct cmp_xform*)cmp_getinstancedata(parent_hdl);
		mat3_mul(&ws_mat, &ws_mat, &parent_xf->mat);
		parent_hdl = parent_xf->parent_hdl;
	}
	mat3_setm(&xf->ws_mat, &ws_mat);

	/* apply to rigid body */
	if (obj->rbody_cmp != INVALID_HANDLE)	{
        struct cmp_rbody* rb = (struct cmp_rbody*)cmp_getinstancedata(obj->rbody_cmp);
        struct xform3d tmp_xf;
        if (rb->rbody != NULL)
            phx_rigid_setxform(rb->px_sceneid, rb->rbody, xform3d_frommat3(&tmp_xf, &ws_mat));
	}

    /* apply to trigger */
    cmphandle_t trigger_cmp = obj->trigger_cmp;
    if (trigger_cmp != INVALID_HANDLE)  {
        struct cmp_trigger* trigger = (struct cmp_trigger*)cmp_getinstancedata(trigger_cmp);
        struct xform3d tmp_xf;
        phx_rigid_setxform(trigger->px_sceneid, trigger->rbody, xform3d_frommat3(&tmp_xf, &ws_mat));
    }

	cmp_updateinstance(obj->xform_cmp);
}

void cmp_xform_setpos(struct cmp_obj* obj, const struct vec3f* pos)
{
    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    mat3_set_trans(&xf->mat, pos);
    cmp_xform_setm(obj, &xf->mat);
}

void cmp_xform_setposf(struct cmp_obj* obj, float x, float y, float z)
{
    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    mat3_set_transf(&xf->mat, x, y, z);
    cmp_xform_setm(obj, &xf->mat);
}

void cmp_xform_setrot(struct cmp_obj* obj, float rx, float ry, float rz)
{
    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    mat3_set_roteuler(&xf->mat, rx, ry, rz);
    cmp_xform_setm(obj, &xf->mat);
}

void cmp_xform_setrot_deg(struct cmp_obj* obj, float rx_deg, float ry_deg, float rz_deg)
{
    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    mat3_set_roteuler(&xf->mat, math_torad(rx_deg), math_torad(ry_deg), math_torad(rz_deg));
    cmp_xform_setm(obj, &xf->mat);
}

void cmp_xform_setrot_quat(struct cmp_obj* obj, const struct quat4f* q)
{
    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    mat3_set_rotquat(&xf->mat, q);
    cmp_xform_setm(obj, &xf->mat);
}

struct vec3f* cmp_xform_getpos(struct cmp_obj* obj, OUT struct vec3f* pos)
{
    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    return mat3_get_trans(pos, &xf->mat);
}

struct quat4f* cmp_xform_getrot(struct cmp_obj* obj, OUT struct quat4f* q)
{
    struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
    return mat3_get_rotquat(q, &xf->mat);
}
