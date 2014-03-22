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
#include "dhcore/task-mgr.h"

#include "components/cmp-model.h"
#include "components/cmp-xform.h"
#include "components/cmp-bounds.h"
#include "components/cmp-attachdock.h"
#include "components/cmp-animchar.h"
#include "components/cmp-anim.h"

#include "res-mgr.h"
#include "gfx-model.h"
#include "cmp-mgr.h"
#include "gfx-canvas.h"
#include "engine.h"

/*************************************************************************************************
 * fwd declarations
 */
void cmp_model_destroydata(struct cmp_obj* host_obj, struct cmp_model* data, cmphandle_t hdl,
		bool_t release_mesh);

result_t cmp_model_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_model_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl);
void cmp_model_update(cmp_t c, float dt, void* param);
void cmp_model_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
		const struct gfx_view_params* params);
/* refresh all model data that their resource matches 'model_hdl': used in res-mgr for hot-loading */
void cmp_model_destroyinstances(reshandle_t model_hdl, const struct cmp_instance_desc** insts,
    uint cnt);
void cmp_model_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
    reshandle_t model_hdl, const struct cmp_instance_desc** insts, uint cnt);
void cmp_model_clearinstances(reshandle_t model_hdl, const struct cmp_instance_desc** insts,
    uint cnt, bool_t reset_hdl);
void cmp_model_drawpose(const struct gfx_model_geo* geo, const struct gfx_model_posegpu* pose,
                        const struct gfx_view_params* params, const struct mat3f* world_mat,
                        float scale);
void cmp_model_drawbone(const struct mat3f* j0, const struct mat3f* j1, float level, float scale);

/*************************************************************************************************/
result_t cmp_model_register(struct allocator* alloc)
{
	struct cmp_createparams params;
	memset(&params, 0x00, sizeof(params));

	params.name = "model";
	params.stride = sizeof(struct cmp_model);
	params.create_func = cmp_model_create;
	params.destroy_func = cmp_model_destroy;
	params.debug_func = cmp_model_debug;
	params.grow_cnt = 300;
	params.initial_cnt = 300;
	params.values = cmp_model_values;
	params.value_cnt = CMP_VALUE_CNT(cmp_model_values);
	params.type = cmp_model_type;
	params.update_funcs[CMP_UPDATE_STAGE2] = cmp_model_update;
	return cmp_register_component(alloc, &params);
}

result_t cmp_model_create(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	struct cmp_model* m = (struct cmp_model*)data;
	m->model_hdl = INVALID_HANDLE;
    if (host_obj != NULL)   {
	    host_obj->model_cmp = hdl;
        host_obj->model_shadow_cmp = hdl;
    }
	return RET_OK;
}

void cmp_model_destroy(struct cmp_obj* host_obj, void* data, cmphandle_t hdl)
{
	cmp_model_destroydata(host_obj, (struct cmp_model*)data, hdl, TRUE);
    if (host_obj != NULL)   {
	    host_obj->model_cmp = INVALID_HANDLE;
        host_obj->model_shadow_cmp = INVALID_HANDLE;
    }
}

/* update node xforms and sknning matrices */
void cmp_model_update(cmp_t c, float dt, void* param)
{
    uint cnt;
    const struct cmp_instance_desc** updates = cmp_get_updateinstances(c, &cnt);
	for (uint i = 0; i < cnt; i++)	{
		const struct cmp_instance_desc* inst = updates[i];
		struct cmp_model* m = (struct cmp_model*)inst->data;
		for (uint k = 1; k < m->xform_cnt; k++)
			cmp_updateinstance(m->xforms[k]);

        /* skinning */
        const struct gfx_model_instance* mi = m->model_inst;
#ifndef _RETAIL_
        if (m->model_inst == NULL)
            continue;
#endif
        for (uint k = 0; k < mi->pose_cnt; k++)   {
            if (mi->poses[k] != NULL)
                gfx_model_update_skin(mi->poses[k]);
        }
	}
}

void cmp_model_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
		const struct gfx_view_params* params)
{
	struct cmp_model* m = (struct cmp_model*)data;

    if (m->model_hdl == INVALID_HANDLE)
        return;
    struct gfx_model* gmodel = rs_get_model(m->model_hdl);
    if (gmodel == NULL)
        return;

	gfx_canvas_setztest(TRUE);
	gfx_canvas_setwireframe(TRUE, TRUE);

	uint tri_cnt = 0;
	uint vert_cnt = 0;
	for (uint i = 0; i < gmodel->node_cnt; i++)	{
		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(m->xforms[i]);
		struct vec4f node_pos;
		mat3_get_trans(&node_pos, &xf->ws_mat);

		gfx_canvas_text3d(gmodel->nodes[i].name, &node_pos, &params->viewproj);
        gfx_canvas_setztest(FALSE);
        gfx_canvas_coords(&xf->ws_mat, &params->cam_pos, 0.2f);
		if (gmodel->nodes[i].mesh_id != INVALID_INDEX)	{
            struct gfx_model_mesh* gmesh = &gmodel->meshes[gmodel->nodes[i].mesh_id];
            struct gfx_model_geo* geo = &gmodel->geos[gmesh->geo_id];
            /* either draw skeleton or wireframe mesh */
            if (geo->skeleton != NULL) {
                struct gfx_model_instance* inst = m->model_inst;
                gfx_canvas_setztest(FALSE);
                float scale =  maxf(aabb_getwidth(&gmodel->bb),
                    maxf(aabb_getheight(&gmodel->bb), aabb_getdepth(&gmodel->bb)));
                scale *= 0.05f;
                cmp_model_drawpose(geo, inst->poses[gmesh->geo_id], params, &xf->ws_mat, scale);
                gfx_canvas_setztest(TRUE);
            }   else    {
                gfx_canvas_setfillcolor_solid(&g_color_yellow);
                gfx_canvas_geo(geo, &xf->ws_mat);
            }
            tri_cnt += geo->tri_cnt;
            vert_cnt += geo->vert_cnt;
		}
	}

	/* model info */
	char info[64];
	struct vec4f farpt;
    gfx_canvas_settextcolor(&g_color_grey);
	cmp_bounds_getfarcorner(&farpt, obj->bounds_cmp, &params->cam_pos);
	sprintf(info, "node_cnt: %d\ntri_cnt: %d\nvertex_cnt: %d", gmodel->node_cnt,
			tri_cnt, vert_cnt);
	gfx_canvas_text3dmultiline(info, &farpt, &params->viewproj);

    gfx_canvas_setfillcolor_solid(&g_color_white);
    gfx_canvas_settextcolor(&g_color_white);
    gfx_canvas_setwireframe(FALSE, TRUE);
}

float cmp_model_calc_jointlevel(struct gfx_model_skeleton* sk, uint joint_idx)
{
    struct gfx_model_joint* joint = &sk->joints[joint_idx];
    float level = 1.0f;

    while (joint->parent_id != INVALID_INDEX)   {
        level += 1.0f;
        joint = &sk->joints[joint->parent_id];
    }

    return level;
}

struct mat3f* cmp_model_calc_jointmat(struct mat3f* rm, struct gfx_model_skeleton* sk,
    uint joint_idx, const struct mat3f* mats, const struct mat3f* world_mat)
{
    /* calculate top-level matrix */
    struct gfx_model_joint* joint = &sk->joints[joint_idx];

    mat3_setm(rm, &mats[joint_idx]);
    while (joint->parent_id != INVALID_INDEX)	{
        mat3_mul(rm, rm, &mats[joint->parent_id]);
        joint = &sk->joints[joint->parent_id];
    }

    return mat3_mul(rm, rm, world_mat);
}

result_t cmp_model_modify(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl)
{
	struct cmp_model* m = (struct cmp_model*)data;
	uint filehash = hash_str(m->filepath);
	bool_t reload = (m->filepath_hash == filehash);
    m->filepath_hash = filehash;

	/* destroy data before loading anything */
	cmp_model_destroydata(obj, (struct cmp_model*)data, cur_hdl, !reload);

	/* ignore if mesh file is blank */
	if (str_isempty(m->filepath))   {
        if (obj->attachdock_cmp != INVALID_HANDLE)
            cmp_attachdock_clear(obj->attachdock_cmp);
		return RET_OK;
    }

    if (!reload)
	    m->model_hdl = rs_load_model(m->filepath, 0);

	if (m->model_hdl == INVALID_HANDLE)
		return RET_FAIL;

	/* create and add cmp_xform(s) for each node in the mesh */
	cmphandle_t xform_hdl;
    struct gfx_model* gmodel = rs_get_model(m->model_hdl);

    /* multithread loads, may have gmodel = NULL */
    if (gmodel == NULL)
        return RET_OK;

	/* root xform */
	if ((xform_hdl = obj->xform_cmp) != INVALID_HANDLE)	{
		cmp_updateinstance(xform_hdl);
	}	else	{
		log_print(LOG_WARNING, "modify-model failed: xform component is required");
		return RET_FAIL;
	}

	m->xforms[0] = xform_hdl;
	m->xform_cnt = gmodel->node_cnt;

    /* submesh transform components
     * they are only modified by internal engine components
     * so they are managed by model component */
	for (uint i = 1, xform_cnt = gmodel->node_cnt; i < xform_cnt; i++)	{
		m->xforms[i] = cmp_create_instance(cmp_findtype(cmp_xform_type), obj,
            CMP_INSTANCEFLAG_INDIRECTHOST, cur_hdl, (uint)offsetof(struct cmp_model, xforms[i]));
		if (m->xforms[i] == INVALID_HANDLE)	{
			log_print(LOG_WARNING, "modify-model failed: xform component creation failed");
			return RET_FAIL;
		}

		struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(m->xforms[i]);
		mat3_setm(&xf->mat, &gmodel->nodes[i].local_mat);
		xf->parent_hdl = m->xforms[gmodel->nodes[i].parent_id];
	}

	/* bounds component is required */
    if (!BIT_CHECK(m->flags, CMP_MODELFLAG_NOBOUNDUPDATE))  {
	    if (obj->bounds_cmp == INVALID_HANDLE)	{
		    cmp_model_destroydata(obj, (struct cmp_model*)data, cur_hdl, TRUE);
		    log_print(LOG_WARNING, "modify-model failed: bounds component is missing");
		    return RET_FAIL;
	    }

	    /* update local bounds from mesh data */
	    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(obj->bounds_cmp);
	    sphere_from_aabb(&b->s, &gmodel->bb);
	    cmp_updateinstance(obj->bounds_cmp);
    }

	/* create model instance */
    m->model_inst = gfx_model_createinstance(alloc, tmp_alloc, m->model_hdl);
    if (m->model_inst == NULL)  {
        cmp_model_destroydata(obj, (struct cmp_model*)data, cur_hdl, TRUE);
        log_print(LOG_WARNING, "modify-model failed: could not create model instance data");
        return RET_FAIL;
    }

    /* refresh dependencies */
    /* current dependencies: attachdock, anim, animchar */
    if (obj->animchar_cmp != INVALID_HANDLE)    {
        cmp_animchar_bind(obj, cmp_getinstancedata(obj->animchar_cmp), alloc, tmp_alloc,
            obj->animchar_cmp);
    }

    cmphandle_t anim_cmp = cmp_findinstance(obj->chain, cmp_anim_type);
    if (anim_cmp != INVALID_HANDLE) {
        cmp_anim_bind(obj, cmp_getinstancedata(anim_cmp), alloc, tmp_alloc, anim_cmp);
    }

    if (obj->attachdock_cmp != INVALID_HANDLE)  {
        cmp_attachdock_refresh(obj->attachdock_cmp);
    }

    return RET_OK;
}

void cmp_model_destroydata(struct cmp_obj* host_obj, struct cmp_model* data, cmphandle_t hdl,
		bool_t release_mesh)
{
	/* destroy cmp_xform(s) except the first one which belongs to the host object */
    for (uint i = 0; i < data->xform_cnt; i++)
        data->xforms[i] = INVALID_HANDLE;

    if (data->model_inst != NULL)   {
        gfx_model_destroyinstance(data->model_inst);
        data->model_inst = NULL;
    }

	data->xform_cnt = 0;
	if (release_mesh && data->model_hdl != INVALID_HANDLE)	{
		rs_unload(data->model_hdl);
		data->filepath_hash = 0;
        data->filepath[0] = 0;
        data->model_hdl = INVALID_HANDLE;
	}
}

void cmp_model_reload(const char* filepath, reshandle_t hdl, bool_t manual)
{
    reshandle_t nhdl;
    uint cnt;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    cmp_t c = cmp_findtype(cmp_model_type);
    const struct cmp_instance_desc** insts = cmp_get_allinstances(c, &cnt, tmp_alloc);

    /* reload model and refresh all model component data */
    cmp_model_destroyinstances(hdl, insts, cnt);

    if (!manual)
        nhdl = rs_load_model(filepath, RS_LOAD_REFRESH);
    else
        nhdl = hdl;

    if (nhdl != INVALID_HANDLE) {
        if (rs_get_model(nhdl) != NULL)
            cmp_model_rebuildhinstances(eng_get_dataalloc(), tmp_alloc, hdl, insts, cnt);
        else
            cmp_model_clearinstances(hdl, insts, cnt, FALSE);
    }    else   {
        cmp_model_clearinstances(hdl, insts, cnt, TRUE);  /* this happens when model-reload fails
                                                             we have to invalidate handles */
    }

    A_FREE(tmp_alloc, insts);
}

void cmp_model_destroyinstances(reshandle_t model_hdl, const struct cmp_instance_desc** insts,
    uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        const struct cmp_model* m = (const struct cmp_model*)inst->data;
        if (model_hdl == m->model_hdl)
            cmp_model_destroydata(inst->host, (struct cmp_model*)inst->data, inst->hdl, FALSE);
    }
}

void cmp_model_rebuildhinstances(struct allocator* alloc, struct allocator* tmp_alloc,
    reshandle_t model_hdl, const struct cmp_instance_desc** insts, uint cnt)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        const struct cmp_model* m = (const struct cmp_model*)inst->data;
        if (model_hdl == m->model_hdl)  {
            cmp_model_modify(inst->host, alloc, tmp_alloc, inst->data, inst->hdl);
        }
    }
}

void cmp_model_clearinstances(reshandle_t model_hdl, const struct cmp_instance_desc** insts,
    uint cnt, bool_t reset_hdl)
{
    for (uint i = 0; i < cnt; i++) {
        const struct cmp_instance_desc* inst = insts[i];
        const struct cmp_model* m = (const struct cmp_model*)inst->data;
        if (model_hdl == m->model_hdl)  {
            struct cmp_model* data = (struct cmp_model*)inst->data;

            if (reset_hdl)
                data->model_hdl = INVALID_HANDLE;

            /* clear dependencies */
            /* current dependencies: attachdock, anim, animchar */
            struct cmp_obj* obj = inst->host;
            if (obj->animchar_cmp != INVALID_HANDLE)
                cmp_animchar_unbind(obj->animchar_cmp);

            cmphandle_t anim_cmp = cmp_findinstance(obj->chain, cmp_anim_type);
            if (anim_cmp != INVALID_HANDLE)
                cmp_anim_unbind(anim_cmp);

            if (obj->attachdock_cmp != INVALID_HANDLE)
                cmp_attachdock_clear(obj->attachdock_cmp);
        }
    }
}

cmphandle_t cmp_model_findnode(cmphandle_t model_hdl, uint name_hash)
{
    struct cmp_model* cm = (struct cmp_model*)cmp_getinstancedata(model_hdl);
#if !defined(_RETAIL_)
    if (cm->model_hdl != INVALID_HANDLE)    {
#endif
        struct gfx_model* m = rs_get_model(cm->model_hdl);
        if (m == NULL)
            return INVALID_HANDLE;

        for (uint i = 0, cnt = m->node_cnt; i < cnt; i++) {
            if (m->nodes[i].name_hash == name_hash)
                return cm->xforms[i];
        }

#if !defined(_RETAIL_)
    }
#endif

    return INVALID_HANDLE;
}

void cmp_model_drawpose(const struct gfx_model_geo* geo, const struct gfx_model_posegpu* pose,
                        const struct gfx_view_params* params, const struct mat3f* world_mat,
                        float scale)
{
    gfx_canvas_setwireframe(FALSE, TRUE);
    gfx_canvas_setlinecolor(&g_color_yellow);
    gfx_canvas_setfillcolor_solid(&g_color_red);
    gfx_canvas_settextcolor(&g_color_grey);

    struct mat3f j1m;
    struct mat3f j2m;
    struct gfx_model_skeleton* sk = geo->skeleton;
    for (uint i = 0; i < sk->joint_cnt; i++)  {
        cmp_model_calc_jointmat(&j1m, sk, i, pose->mats, world_mat);

        /* find child for the current joint and draw the bone */
        bool_t nochild = FALSE;
        for (uint k = 0; k < sk->joint_cnt; k++)  {
            struct gfx_model_joint* joint2 = &sk->joints[k];
            if (joint2->parent_id == i) {
                cmp_model_calc_jointmat(&j2m, sk, k, pose->mats, world_mat);
                float level = cmp_model_calc_jointlevel(sk, i);
                cmp_model_drawbone(&j1m, &j2m, level, scale);
                /*gfx_canvas_coords(&j1m, &params->cam_pos, 0.1f);*/
                nochild = TRUE;
                break;
            }
        }

        if (!nochild)    {
            struct sphere s;
            float level = cmp_model_calc_jointlevel(sk, i);
            sphere_setf(&s, 0.0f, 0.0f, 0.0f, scale * (1.0f / level));
            gfx_canvas_sphere(&s, &j1m, GFX_SPHERE_LOW);
            /*gfx_canvas_coords(&j1m, &params->cam_pos, 0.1f);*/
        }
    }
    gfx_canvas_setwireframe(TRUE, TRUE);
}

void cmp_model_drawbone(const struct mat3f* j0, const struct mat3f* j1, float level, float scale)
{
    struct sphere s;
    sphere_setf(&s, 0.0f, 0.0f, 0.0f, scale * (1.0f/level));

    struct vec3f p0;
    struct vec3f p1;
    mat3_get_trans(&p0, j0);
    mat3_get_trans(&p1, j1);

    gfx_canvas_line3d(&p0, &p1);

    gfx_canvas_sphere(&s, j0, GFX_SPHERE_LOW);
    gfx_canvas_sphere(&s, j1, GFX_SPHERE_LOW);
}

