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
#include "dhcore/prims.h"

#include "components/cmp-light.h"
#include "components/cmp-xform.h"
#include "components/cmp-bounds.h"

#include "cmp-mgr.h"
#include "gfx-canvas.h"
#include "lod-scheme.h"

#define LIGHT_FADE_RANGE 5.0f

/*************************************************************************************************
 * fwd declarations
 */
void cmp_light_calcbounds(struct cmp_obj* obj, struct cmp_light* light);
result_t cmp_light_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_light_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_light_update(cmp_t c, float dt, void* param);
void cmp_light_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
		const struct gfx_view_params* params);

void light_calcspotbounds(struct sphere* s, struct cmp_light* light);
void light_calcpointbounds(struct sphere* s, struct cmp_light* light);

/*************************************************************************************************/
result_t cmp_light_register(struct allocator* alloc)
{
	struct cmp_createparams params;
	memset(&params, 0x00, sizeof(params));

	params.name = "light";
	params.stride = sizeof(struct cmp_light);
	params.initial_cnt = 100;
	params.grow_cnt = 50;
	params.values = cmp_light_values;
	params.value_cnt = CMP_VALUE_CNT(cmp_light_values);
	params.type = cmp_light_type;
	params.create_func = cmp_light_create;
	params.destroy_func = cmp_light_destroy;
	params.debug_func = cmp_light_debug;
	params.update_funcs[CMP_UPDATE_STAGE4] = cmp_light_update;

	return cmp_register_component(alloc, &params);
}

result_t cmp_light_modifytype(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	struct cmp_light* l = (struct cmp_light*)data;
	cmp_light_calcbounds(obj, l);
	ASSERT(obj->bounds_cmp != INVALID_HANDLE);
	cmp_updateinstance(obj->bounds_cmp);
	return RET_OK;
}

result_t cmp_light_modifycolor(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	struct cmp_light* l = (struct cmp_light*)data;
	color_tolinear(&l->color_lin, &l->color);
	return RET_OK;
}

result_t cmp_light_modifyatten(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	struct cmp_light* l = (struct cmp_light*)data;
	l->atten_near = clampf(l->atten_near, 0.0f, l->atten_far);
	l->atten_far = clampf(l->atten_far, l->atten_near, 1000.0f);
	l->atten_narrow = clampf(l->atten_narrow, 0.0f, l->atten_wide);
	l->atten_wide = clampf(l->atten_wide, l->atten_narrow, 0.785f);

	cmp_light_calcbounds(obj, l);
	cmp_updateinstance(cur_hdl);
	return RET_OK;
}

result_t cmp_light_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	/* defaults */
	struct cmp_light* l = (struct cmp_light*)data;
	l->type = CMP_LIGHT_POINT;
	vec3_setzero(&l->pos);
	l->intensity = 1.0f;
	color_setc(&l->color, &g_color_white);
	color_tolinear(&l->color_lin, &l->color);
	l->atten_near = 0.5f;
	l->atten_far = 2.5f;
	l->atten_narrow = PI*0.25f*0.3f;
	l->atten_wide = PI*0.25f;
	cmp_light_calcbounds(host_obj, l);
    strcpy(l->lod_scheme_name, "default");
    l->scheme_id = lod_findmodelscheme(l->lod_scheme_name);
    ASSERT(l->scheme_id != 0);

	return RET_OK;
}

void cmp_light_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{

}

void cmp_light_update(cmp_t c, float dt, void* param)
{
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
	for (uint i = 0; i < cnt; i++)	{
		const struct cmp_instance_desc* inst = updates[i];
		struct cmp_light* l = (struct cmp_light*)inst->data;

		/* get transform data and update direction and position */
		ASSERT(inst->host->xform_cmp != INVALID_HANDLE);
		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(inst->host->xform_cmp);
		mat3_get_zaxis(&l->dir, &xf->ws_mat);
		mat3_get_trans(&l->pos, &xf->ws_mat);
        vec3_norm(&l->dir, &l->dir);
	}
}

void cmp_light_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
		const struct gfx_view_params* params)
{
	struct cmp_light* l = (struct cmp_light*)data;
	gfx_canvas_setztest(TRUE);
	gfx_canvas_setfillcolor_solid(&g_color_yellow);
	gfx_canvas_setwireframe(TRUE, FALSE);

	switch (l->type)	{
	case CMP_LIGHT_POINT:	{
		float atten[] = {l->atten_near, l->atten_far};
		gfx_canvas_light_pt(&l->pos, atten);
	}
		break;
	case CMP_LIGHT_SPOT:	{
		float atten[] = {l->atten_near, l->atten_far, l->atten_narrow, l->atten_wide};
		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);
		gfx_canvas_light_spot(&xf->ws_mat, atten);
	}
		break;
	default:
		break;
	}
}

/* update bounds component of the light based on light properties */
void cmp_light_calcbounds(struct cmp_obj* obj, struct cmp_light* light)
{
	struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(obj->bounds_cmp);
	switch (light->type)	{
	case CMP_LIGHT_POINT:
        light_calcpointbounds(&b->s, light);
		break;
    case CMP_LIGHT_SPOT:
        light_calcspotbounds(&b->s, light);
		break;
	}

	/* modify some value in the light component to refresh the parent */
	light->color_lin.a = !light->color_lin.a;
}

/* calculate spot-light sphere bounds in local-space, based on light properties */
void light_calcspotbounds(struct sphere* s, struct cmp_light* light)
{
    struct vec4f centerpt;
    struct vec4f endpt;
    vec3_muls(&centerpt, &g_vec3_unitz, light->atten_far);
    vec3_add(&endpt, &centerpt,
        vec3_muls(&endpt, &g_vec3_unitz, light->atten_far - light->atten_near));
    float r1 = light->atten_near * tanf(light->atten_narrow);
    float r2 = light->atten_far * tanf(light->atten_far);

    struct sphere s1;
    struct sphere s2;
    struct vec4f tmp;
    sphere_circum(&s1,
        vec3_add(&tmp, &centerpt, vec3_muls(&tmp, &g_vec3_unitx, r1)),
        vec3_sub(&tmp, &centerpt, vec3_muls(&tmp, &g_vec3_unitx, r1)),
        vec3_add(&tmp, &endpt, vec3_muls(&tmp, &g_vec3_unity, r2)),
        vec3_sub(&tmp, &endpt, vec3_muls(&tmp, &g_vec3_unity, r2)));
    sphere_circum(&s2,
        vec3_add(&tmp, &centerpt, vec3_muls(&tmp, &g_vec3_unity, r1)),
        vec3_sub(&tmp, &centerpt, vec3_muls(&tmp, &g_vec3_unity, r1)),
        vec3_add(&tmp, &endpt, vec3_muls(&tmp, &g_vec3_unitx, r2)),
        vec3_sub(&tmp, &endpt, vec3_muls(&tmp, &g_vec3_unitx, r2)));

    sphere_merge(s, &s1, &s2);
    s->r += 0.01f;
}

/* calculate point-light sphere bounds in local-space, by it's light properties */
void light_calcpointbounds(struct sphere* s, struct cmp_light* light)
{
    sphere_setf(s, 0.0f, 0.0f, 0.0f, light->atten_far);
}

result_t cmp_light_modifylod(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
    struct cmp_light* l = (struct cmp_light*)data;
    uint id = lod_findmodelscheme(l->lod_scheme_name);
    if (id == 0) {
        log_printf(LOG_WARNING, "light: lod-scheme '%s' does not exist", l->lod_scheme_name);
        return RET_FAIL;
    }

    l->scheme_id = id;
    return RET_OK;
}

bool_t cmp_light_applylod(cmphandle_t light_hdl, const struct vec3f* campos, OUT float* intensity)
{
    struct cmp_obj* host = cmp_getinstancehost(light_hdl);
    struct cmp_light* light = (struct cmp_light*)cmp_getinstancedata(light_hdl);
    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(host->bounds_cmp);
    const struct lod_light_scheme* scheme = lod_getlightscheme(light->scheme_id);

    /* calculate distance factors to the bounds of object */
    struct vec3f d;
    vec3_setf(&d, campos->x - b->ws_s.x, campos->y - b->ws_s.y, campos->z - b->ws_s.z);
    float r = b->ws_s.r;
    float dot_d = vec3_dot(&d, &d);

    /* test high detail */
    float l = scheme->vis_range + r;
    if (dot_d < l*l)    {
        *intensity = 1.0f;
        return TRUE;
    }

    /* not visible, calculate fading value (intensity multiplier) */
    /* dist = l + (1-t)*LIGHT_FADE_RANGE
     * t = 1 - ((dist - l)/LIGHT_FADE_RANGE)
     */
    float dist = clampf(sqrtf(dot_d), l, l + LIGHT_FADE_RANGE);
    *intensity = 1.0f - (dist - l)/LIGHT_FADE_RANGE;
    return (*intensity > EPSILON);
}
