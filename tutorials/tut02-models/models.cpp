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

#include <stdio.h>
#include "dhcore/core.h"
#include "dhcore/timer.h"

#include "dhapp/app.h"
#include "dheng/engine.h"
#include "dheng/gfx-canvas.h"
#include "dheng/camera.h"
#include "dheng/scene-mgr.h"
#include "dhapp/input.h"
#include "dheng/gfx.h"
#include "dheng/debug-hud.h"

#include "helper-app.h"

#define APP_NAME "darkHAMMER: Tut02 - Models"
#define CAM_FOV 50.0f
#define CAM_FAR 1000.0f
#define CAM_NEAR 0.1f

#include "dheng/scene-mgr.h"
#include "dheng/cmp-mgr.h"
#include "dheng/components/cmp-xform.h"
#include "dheng/phx.h"
#include "dheng/world-mgr.h"

/**
 * Globals
 */
camera_fps g_cam;
uint g_scene = 0;
timer* g_timer = NULL;

#define BARREL_COUNT 32
cmp_obj* g_obj_ground = NULL;
cmp_obj* g_obj_barrels[BARREL_COUNT];

/**
 * Load tutorial data and setup scene
 */
int tut02_load_data()
{
    result_t r;

    uint scene_id = scn_getactive();
    ASSERT(scene_id != 0);

    /* Create ground plane model */
    cmp_obj* obj = scn_create_obj(scene_id, "ground", CMP_OBJTYPE_MODEL);
    if (obj == NULL)
        return FALSE;
    r = cmp_value_sets(cmp_findinstance_inobj(obj, "model"), "filepath", "plane.h3dm");
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "Loading models failed");
        return FALSE;
    }
    g_obj_ground = obj;

    /* Create physics debug plane, acts as an infinite physics ground for rigid objects */
    phx_create_debugplane(0.5f, 0.5f);

    /* Setup barrels */
    for (uint i = 0; i < BARREL_COUNT; i++)   {
        char name[32];
        sprintf(name, "barrel-%d", i);
        cmp_obj* barrel = scn_create_obj(scene_id, name, CMP_OBJTYPE_MODEL);
        if (barrel == NULL)
            return FALSE;
        /* Load barrel.h3dm as model file for objects
         * We set/modify the "filepath" value of the "model" component of the object */
        r = cmp_value_sets(cmp_findinstance_inobj(barrel, "model"), "filepath", "barrel.h3dm");
        if (IS_FAIL(r)) {
            scn_destroy_obj(barrel);
            err_print(__FILE__, __LINE__, "Loading barrel models failed");
            return FALSE;
        }

        /* Create rigid body physics component for the object */
        cmphandle_t rbody_hdl = cmp_create_instance_forobj("rbody", barrel);
        if (rbody_hdl == INVALID_HANDLE)    {
            scn_destroy_obj(barrel);
            err_print(__FILE__, __LINE__, "Loading barrel models failed");
            return FALSE;
        }

        /* Load barrel.h3dp as physics object
         * We set/modify the "filepath" value of the "rbody" component of the object */
        r = cmp_value_sets(rbody_hdl, "filepath", "barrel.h3dp");
        if (IS_FAIL(r))    {
            scn_destroy_obj(barrel);
            err_print(__FILE__, __LINE__, "Loading barrel models failed");
            return FALSE;
        }

        /* Place barrel randomly in space */
        cmp_xform_setposf(barrel,
            rand_getf(-5.0f, 5.0f), rand_getf(10.0f, 16.0f), rand_getf(-5.0f, 5.0f));
        cmp_xform_setrot(barrel, rand_getf(0, PI), 0.0f, rand_getf(0, PI));

        g_obj_barrels[i] = barrel;
    }

    return TRUE;
}

/**
 * Unload this tutorial data
 */
void tut02_unload_data()
{
    /* Destroy ground plane */
    if (g_obj_ground != NULL)   {
        scn_destroy_obj(g_obj_ground);
        g_obj_ground = NULL;
    }

    /* Destroy barrels */
    for (uint i = 0; i < BARREL_COUNT; i++)   {
        if (g_obj_barrels[i] != NULL)   {
            scn_destroy_obj(g_obj_barrels[i]);
            g_obj_barrels[i] = NULL;
        }
    }
}

/**
 * Initialize the scene
 * Set root data directory and load/init required assets
 */
int init_scene()
{
    /* data root directory is "[tutorials]/data" */
    set_datadir();

    /* Initialize default FPS camera, FPS cameras automically handle input and smoothing */
    vec4f pos;
    vec4f target;
    vec3_setf(&pos, 0.0f, 1.0f, -5.0f);
    vec3_setf(&target, 0.0f, 1.0f, 1.0f);
    cam_fps_init(&g_cam, &pos, &target, CAM_NEAR, CAM_FAR, math_torad(CAM_FOV));

    g_scene = scn_create_scene("main");
    if (g_scene == 0)   {
        err_print(__FILE__, __LINE__, "Initializing scene failed");
        return FALSE;
    }

    /* Activate the scene and the camera */
    scn_setactive(g_scene);
    wld_set_cam(&g_cam.c);

    /* create default timer */
    g_timer = timer_createinstance(TRUE);
    if (g_timer == NULL)    {
        err_print(__FILE__, __LINE__, "Could not create timer");
        return FALSE;
    }

    set_datadir();

    return tut02_load_data();
}

/**
 * Release scene data
 */
void release_scene()
{
    tut02_unload_data();

    if (g_scene != 0)
        scn_destroy_scene(g_scene);

    if (g_timer != NULL)
        timer_destroyinstance(g_timer);
}

/**
 * View debug callback
 * We use this callback for simple canvas drawings
 */
void debug_view_callback(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params)
{
    /* Draw a white XZ plane grid */
    gfx_canvas_setlinecolor(&g_color_white);
    gfx_canvas_grid(5.0f, 70.0f, params->cam);

    /* Draw main axises on the origin */
    struct mat3f ident;
    mat3_setidentity(&ident);
}

/**
 * Update camera: We are using the most low-level method of camera handling here
 */
void update_camera()
{
    /* store previous mouse position for delta */
    static int prev_x = 0;
    static int prev_y = 0;

    float dt = g_timer->dt;

    struct vec2i mpos;
    input_mouse_getpos(&mpos);

    /* update only if application is active and mouse activity is enabled */
    if (app_window_isactive() && !hud_console_isactive())   {
        /* Move camera only if left mouse key is pressed inside the window */
        if (prev_x != 0 && prev_y != 0 && input_mouse_getkey(INPUT_MOUSEKEY_LEFT, FALSE))
        {
            int dx = mpos.x - prev_x;
            int dy = mpos.y - prev_y;
            cam_fps_update(&g_cam, dx, dy, dt);
        }
    }

    prev_x = mpos.x;
    prev_y = mpos.y;
}

/**
 * Modify sun light direction by pressing "1" and "2" keys
 */
void update_light()
{
    vec3f dir;
    uint sec_light = wld_find_section("light");
    uint w_ldir = wld_find_var(sec_light, "dir");
    vec3_setvp(&dir, wld_get_var(sec_light, w_ldir)->fs);

    mat3f m;
    mat3_setidentity(&m);

    /* Pressing "1" key rotates global light (sun light) around x-axis */
    if (input_kb_getkey(INPUT_KEY_1, FALSE)) {
        mat3_set_roteuler(&m, math_torad(0.5f), 0.0f, 0.0f);
        vec3_transformsr(&dir, &dir, &m);
    }

    /* Pressing "2" key rotates global light (sun light) around z-axis */
    if (input_kb_getkey(INPUT_KEY_2, FALSE)) {
        mat3_set_roteuler(&m, 0.0f, math_torad(0.5f), 0.0f);
        vec3_transformsr(&dir, &dir, &m);
    }

    struct variant dir_var;
    wld_set_var(sec_light, w_ldir, var_set3fv(&dir_var, dir.f));
}

/**
 * Window keypress callback: Send required keyboard messages to engine (for GUI handling)
 */
void keypress_callback(char c, uint vkey)
{
    eng_send_guimsgs(c, vkey);
}

/**
 * Update loop
 */
void update_callback()
{
    input_update(); /* updates input system */
    update_light();
    update_camera(); /* update camera after input is refreshed */
    eng_update();   /* updates engine and renders the frame */
}

/**
 * Application activate/deactivate callback
 * Pause/Resume engine simulation
 */
void activate_callback(int active)
{
    if (active)
        eng_resume();
    else
        eng_pause();
}

/**
 * Window resize callback
 * Resize the camera aspect
 */
void resize_callback(uint width, uint height)
{
    cam_set_viewsize(&g_cam.c, (float)width, (float)height);
}

/*************************************************************************************************/
int main(int argc, char** argv)
{
    result_t r;
    memset(g_obj_barrels, 0x00, sizeof(g_obj_barrels));

    /* Init core library */
    r = core_init(CORE_INIT_ALL);

    /* After core init, you can set logging options */
    log_outputconsole(TRUE);
    set_logfile();

    /* load config file (json) */
    init_params* params = app_config_default();
    if (params == NULL) {
        err_sendtolog(FALSE);
        core_release(FALSE);
        return -1;
    }

    /* Add our stuff to init params
     * Engine's data directory must be changed
     */
    params->flags |= (ENG_FLAG_CONSOLE | ENG_FLAG_DEV);

    /* Initialize application (graphics device and rendering window)
     * Application name will also, be the name of the main window */
    r = app_init(APP_NAME, params);
    if (IS_FAIL(r)) {
        err_sendtolog(FALSE);
        core_release(FALSE);
        app_config_unload(params);
        return -1;
    }

    /* Initialize engine */
    r = eng_init(params);

    /* init params isn't needed anymore */
    app_config_unload(params);
    if (IS_FAIL(r)) {
        err_sendtolog(FALSE);
        eng_release();
        app_release();
        core_release(TRUE);
        return -1;
    }

    if (!init_scene())  {
        err_sendtolog(FALSE);
        eng_release();
        app_release();
        core_release(TRUE);
        return -1;
    }

    /* Set application callbacks */
    app_window_setupdatefn(update_callback);
    app_window_setkeypressfn(keypress_callback);
    app_window_setactivefn(activate_callback);
    gfx_set_debug_renderfunc(debug_view_callback);

    /* Initialize ok: show the main window */
    app_window_show();

    /* Enter message loop and update engine */
    app_window_run();

    /* cleanup */
    release_scene();
    eng_release();
    app_release();
    core_release(TRUE);
    return 0;
}