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

#include "dhcore/core.h"
#include "dhcore/variant.h"
#include "dhcore/task-mgr.h"

#include "phx.h"
#include "phx-device.h"
#include "cmp-mgr.h"
#include "engine.h"
#include "dhapp/init-params.h"
#include "mem-ids.h"
#include "scene-mgr.h"

#include "components/cmp-xform.h"

#define STEP_LEN (1.0f/60.0f)

/*************************************************************************************************
 * types
 */
struct phx_mgr
{
    struct phx_params params;
    float steptm;
    float last_steptm;
    uint active_scene;
    phx_rigid_st rigid_debugplane;
};

/*************************************************************************************************
 * globals
 */
static struct phx_mgr g_phx;

/*************************************************************************************************/
void phx_zero()
{
    phx_zerodev();
    memset(&g_phx, 0x00, sizeof(g_phx));
}

result_t phx_init(const struct init_params* params)
{
    result_t r;

    log_print(LOG_TEXT, "init physics ...");
    memcpy(&g_phx.params, &params->phx, sizeof(struct phx_params));

    r = phx_initdev(params);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "phx: init phx-device failed");
        return RET_FAIL;
    }

    return RET_OK;
}

void phx_release()
{
    if (g_phx.rigid_debugplane != NULL)
        phx_destroy_rigid(g_phx.rigid_debugplane);

    phx_releasedev();
    phx_zero();
    log_printf(LOG_TEXT, "physics released.");
}

void phx_update_xforms(int simulated)
{
    static const float steptm_max = STEP_LEN;
    uint scene_id = g_phx.active_scene;
    uint cnt;

    if (scene_id == 0)
        return;

    float steptm = g_phx.steptm;
    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    A_SAVE(tmp_alloc);

    struct phx_active_transform* xfs = phx_scene_activexforms(scene_id, tmp_alloc, &cnt);
    if (xfs == NULL)    {
        A_LOAD(tmp_alloc);
        return;
    }

    struct mat3f mworld;
    struct mat3f mworld_inv;

    /* we have to interpolate, because stepping time is less that expected maximum step */
    if (steptm <= steptm_max)   {
        struct vec3f trans;
        struct vec3f rot;
        struct quat4f qrot;

        /* interpolate transforms by their linear/angular velocities */
        for (uint i = 0; i < cnt; i++)    {
            struct cmp_obj* obj = xfs[i].obj;
            struct cmp_xform* cxf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);

            /* if frame is simulated, update velocities of transform components */
            if (simulated)  {
                vec3_setv(&cxf->vel_lin, &xfs[i].vel_lin);
                vec3_setv(&cxf->vel_ang, &xfs[i].vel_ang);
            }

            /* interpolate */
            vec3_add(&trans, vec3_muls(&trans, &cxf->vel_lin, steptm), &xfs[i].xform_ws.p);
            vec3_muls(&rot, &cxf->vel_ang, steptm);
            quat_mul(&qrot, &xfs[i].xform_ws.q, quat_fromeuler(&qrot, rot.x, rot.y, rot.z));

            /* make matrix and calculate local-mat for xform component if required */
            if (cxf->parent_hdl == INVALID_HANDLE)  {
                mat3_set_trans_rot(&cxf->mat, &trans, &qrot);
            }   else    {
                struct cmp_xform* cxf_parent = (struct cmp_xform*)cmp_getinstancedata(cxf->parent_hdl);
                mat3_set_trans_rot(&cxf->mat, &trans, &qrot);
                mat3_invrt(&mworld_inv, &cxf_parent->ws_mat);
                mat3_mul(&cxf->mat, &mworld, &mworld_inv);
            }

            cmp_updateinstance(obj->xform_cmp);
        }
    }   else    {
        /* no interpolation */
        for (uint i = 0; i < cnt; i++)    {
            struct cmp_obj* obj = xfs[i].obj;
            struct cmp_xform* cxf = (struct cmp_xform*)cmp_getinstancedata(obj->xform_cmp);

            /* make matrix and calculate local-mat for xform component if required */
            if (cxf->parent_hdl == INVALID_HANDLE)  {
                xform3d_getmat(&cxf->mat, &xfs[i].xform_ws);
            }   else    {
                struct cmp_xform* cxf_parent = (struct cmp_xform*)cmp_getinstancedata(cxf->parent_hdl);
                xform3d_getmat(&mworld, &xfs[i].xform_ws);
                mat3_invrt(&mworld_inv, &cxf_parent->ws_mat);
                mat3_mul(&cxf->mat, &mworld, &mworld_inv);
            }

            cmp_updateinstance(obj->xform_cmp);
        }
    }

    A_LOAD(tmp_alloc);
}

int phx_update_sim(float dt)
{
    static const float step = STEP_LEN;
    uint scene_id = g_phx.active_scene;

    if (scene_id == 0)
        return FALSE;

    g_phx.last_steptm = g_phx.steptm;
    g_phx.steptm += dt;
    if (g_phx.steptm < step)
        return FALSE;

    g_phx.steptm -= step;
    phx_scene_simulate(scene_id, step);

    /* dt is too large, so we need more accuracy, repeat simulation */
    uint cnt = 0;
    while (g_phx.steptm > step && cnt < g_phx.params.substeps_max)  {
        phx_scene_wait(scene_id);
        phx_scene_simulate(scene_id, step);
        g_phx.steptm -= step;
        cnt ++;
    }

    return TRUE;
}

void phx_setactive(uint scene_id)
{
    g_phx.active_scene = scene_id;
}

uint phx_getactive()
{
    return g_phx.active_scene;
}

void phx_wait()
{
    uint scene_id = g_phx.active_scene;

    if (scene_id != 0)
        phx_scene_wait(scene_id);
}

void phx_create_debugplane(float friction, float restitution)
{
    if (g_phx.rigid_debugplane != NULL)
        return;

    struct xform3d xf;
    struct quat4f qr;
    phx_mtl mtl = phx_create_mtl(friction, friction, restitution);
    ASSERT(mtl);

    g_phx.rigid_debugplane = phx_create_rigid_st(xform3d_setidentity(&xf));
    if (g_phx.rigid_debugplane != NULL) {
        phx_create_planeshape(g_phx.rigid_debugplane, mtl, xform3d_setpq(&xf, &g_vec3_zero,
            quat_fromaxis(&qr, &g_vec3_unitz, PI*0.5f)));
        phx_scene_addactor(phx_getactive(), g_phx.rigid_debugplane);
    }
    phx_destroy_mtl(mtl);   /* decrease ref-count */
}


void phx_setgravity_callback(const struct variant* v, void* param)
{
    struct vec3f g;
    uint scene_id = scn_getactive();
    if (scene_id != 0)
        phx_scene_setgravity(scn_getphxscene(scene_id), vec3_setf(&g, v->fs[0], v->fs[1], v->fs[2]));
}
