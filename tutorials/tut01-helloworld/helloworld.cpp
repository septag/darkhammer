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

#include "dhapp/input.h"
#include "dhapp/app.h"

#include "dheng/engine.h"
#include "dheng/gfx-canvas.h"
#include "dheng/camera.h"
#include "dheng/scene-mgr.h"
#include "dheng/gfx.h"
#include "dheng/world-mgr.h"
#include "dheng/debug-hud.h"

#include "helper-app.h"

#define APP_NAME "darkHAMMER: Tut01 - HelloWorld"
#define CAM_FOV 50.0f
#define CAM_FAR 1000.0f
#define CAM_NEAR 0.1f

/**
 * Globals
 */
camera g_cam;
uint g_scene = 0;
timer* g_timer = NULL;

/**
 * Initialize the scene
 * Set root data directory and load/init required assets
 */
bool_t init_scene()
{
    /* data root directory is "[tutorials]/data" */
    set_datadir();

    /* Initialize low-level camera */
    vec4f pos;
    vec4f target;
    vec3_setf(&pos, 0.0f, 1.0f, -5.0f);
    vec3_setf(&target, 0.0f, 1.0f, 1.0f);
    cam_init(&g_cam, &pos, &target, CAM_NEAR, CAM_FAR, math_torad(CAM_FOV));

    g_scene = scn_create_scene("main");
    if (g_scene == 0)   {
        err_print(__FILE__, __LINE__, "Initializing scene failed");
        return FALSE;
    }

    /* Activate the scene and the camera */
    scn_setactive(g_scene);
    wld_set_cam(&g_cam);

    /* create default timer */
    g_timer = timer_createinstance(TRUE);
    if (g_timer == NULL)    {
        err_print(__FILE__, __LINE__, "Could not create timer");
        return FALSE;
    }

    return TRUE;
}

/**
 * Release scene data
 */
void release_scene()
{
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
    gfx_canvas_coords(&ident, &params->cam_pos, 1.0f);

    /* Draw a Hello world text */
    gfx_canvas_text2dpt("Hello World", 10, 10, 0);
}

/**
 * Update camera: We are using the most low-level method of camera handling here
 */
void update_camera()
{
    /* store previous mouse position for delta */
    static int prev_x = 0;
    static int prev_y = 0;

    const float multiplier = 0.005f;

    float dt = g_timer->dt;
    float move = 5.0f * dt;

    struct vec2i mpos;
    input_mouse_getpos(&mpos);

    /* update only if application is active and mouse activity is enabled */
    if (app_window_isactive() && !hud_console_isactive())   {
        /* Move camera only if left mouse key is pressed inside the window */
        if (prev_x != 0 && prev_y != 0 && input_mouse_getkey(INPUT_MOUSEKEY_LEFT, FALSE))   {
            /* Camera Rotation (with mouse) */
            float dx = (float)(mpos.x - prev_x);
            float dy = (float)(mpos.y - prev_y);
            if (!math_iszero(dx))   cam_yaw(&g_cam, dx*multiplier);
            if (!math_iszero(dy))   cam_pitch(&g_cam, dy*multiplier);

            /* Camera movement (with keyboard) */
            if (input_kb_getkey(INPUT_KEY_LSHIFT, FALSE) || input_kb_getkey(INPUT_KEY_RSHIFT, FALSE))
                move *= 5.0f;
            if (input_kb_getkey(INPUT_KEY_W, FALSE) || input_kb_getkey(INPUT_KEY_UP, FALSE))
                cam_fwd(&g_cam, move);
            if (input_kb_getkey(INPUT_KEY_S, FALSE) || input_kb_getkey(INPUT_KEY_DOWN, FALSE))
                cam_fwd(&g_cam, -move);
            if (input_kb_getkey(INPUT_KEY_A, FALSE) || input_kb_getkey(INPUT_KEY_LEFT, FALSE))
                cam_strafe(&g_cam, -move);
            if (input_kb_getkey(INPUT_KEY_D, FALSE) || input_kb_getkey(INPUT_KEY_RIGHT, FALSE))
                cam_strafe(&g_cam, move);

            cam_update(&g_cam);
        }
    }

    prev_x = mpos.x;
    prev_y = mpos.y;
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
    update_camera(); /* update camera after input is refreshed */
    eng_update();   /* updates engine and renders the frame */
}

/**
 * Application activate/deactivate callback
 * Pause/Resume engine simulation
 */
void activate_callback(bool_t active)
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
    cam_set_viewsize(&g_cam, (float)width, (float)height);
}

/*************************************************************************************************/
int main(int argc, char** argv)
{
    result_t r;

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