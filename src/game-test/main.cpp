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

/**
 * Sample application for simple engine init/render
 */

#include <stdio.h>

#include "dhcore/core.h"
#include "dhcore/pak-file.h"
#include "dhcore/timer.h"
#include "dhcore/vec-math.h"

#include "dhapp/app.h"
#include "dhapp/input.h"

#include "dheng/engine.h"
#include "dheng/camera.h"
#include "dheng/scene-mgr.h"
#include "dheng/script.h"
#include "dheng/gfx-canvas.h"
#include "dheng/gfx.h"
#include "dheng/world-mgr.h"
#include "dheng/debug-hud.h"
#include "dheng/res-mgr.h"

/*************************************************************************************************
 * Globals
 */
struct camera_fps g_cam;
uint g_scene = 0;
struct timer* g_timer = NULL;
struct pak_file g_media_pak;
reshandle_t g_tex = INVALID_HANDLE;

/**
 * set data directory :
 * Assuming the binary is in "/bin" directory
 * in DEBUG build, just set data directory as "..\"
 * in RELEASE build, first try to load "..\media.pak", if not found, set data dir as "..\"
 */
void set_datadir()
{
    const char* share_dir = eng_get_sharedir();
    ASSERT(share_dir);

    char data_path[DH_PATH_MAX];
    path_join(data_path, share_dir, "test-data", NULL);

    /* Set the second parameter (monitor) to TRUE, for hot-reloading of assets within that directory */
    fio_addvdir(data_path, FALSE);
}

bool_t load_props()
{
    set_datadir();  /* important for loading resources */

    struct vec4f pos;
    struct vec4f target;
    vec3_setf(&pos, 0.0f, 1.0f, -5.0f);
    vec3_setf(&target, 0.0f, 1.0f, 1.0f);
    cam_fps_init(&g_cam, &pos, &target, 0.1f, 1000.0f, math_torad(60.0f));

    g_timer = timer_createinstance(TRUE);

    /* setup test scene */
    g_scene = scn_create_scene("test");
    if (g_scene == 0)
        return FALSE;
    scn_setactive(g_scene);
    wld_set_cam(&g_cam.c);

    /* load sample script (which populates the scene) */
    sct_runfile("test9.lua");

    /*
    g_tex = rs_load_texture("textures/0_wood_planks_01.dds", 0, FALSE, 0);
    if (g_tex == INVALID_HANDLE)
        return FALSE;
        */

    return TRUE;
}

void unload_props()
{
    if (g_tex != INVALID_HANDLE)
        rs_unload(g_tex);

    if (g_scene != 0)
        scn_destroy_scene(g_scene);
    if (g_timer != NULL)    {
        timer_destroyinstance(g_timer);
        g_timer = NULL;
    }
}

/*************************************************************************************************
 * Callbacks
 */
void debug_view_callback(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params)
{
    struct mat3f center_mat;
    mat3_setidentity(&center_mat);
    gfx_canvas_setlinecolor(&g_color_white);
    gfx_canvas_grid(5.0f, 70.0f, params->cam);
    gfx_canvas_coords(&center_mat, &params->cam_pos, 1.0f);

    if (g_tex != INVALID_HANDLE)    {
        struct rect2di rc;
        rect2di_seti(&rc, 100, 100, 256, 256);
        gfx_canvas_bmp2d(rs_get_texture(g_tex), 256, 256, &rc, 0);
    }
}

void update_camera()
{
    float dt = g_timer->dt;

    static int prev_x;
    static int prev_y;

    struct vec2i mpos;
    input_mouse_getpos(&mpos);

    if (app_window_isactive() && !hud_console_isactive())   {
        /* alter camera movement speed */
        if (input_kb_getkey(INPUT_KEY_LSHIFT, FALSE) || input_kb_getkey(INPUT_KEY_RSHIFT, FALSE))
            cam_fps_set_movespeed(&g_cam, 0.3f*5.0f);
        else
            cam_fps_set_movespeed(&g_cam, 0.3f);

        /* rotation - with mouse */
        if (input_mouse_getkey(INPUT_MOUSEKEY_LEFT, FALSE))
            cam_fps_update(&g_cam, mpos.x - prev_x, mpos.y - prev_y, dt);
    }

    prev_x = mpos.x;
    prev_y = mpos.y;
}

void keypress_callback(char c, uint vkey)
{
    eng_send_guimsgs(c, vkey);
}

void update_callback()
{
    input_update();
    update_camera();
    eng_update();
    app_window_swapbuffers();
}

/* pause/resume engine, on app activation/deactivation */
void activate_callback(bool_t active)
{
    if (active)
        eng_resume();
    else
        eng_pause();
}

void resize_callback(uint width, uint height)
{
    app_window_resize(width, height);
    gfx_resize(width, height);
    cam_set_viewsize(&g_cam.c, (float)width, (float)height);
}

void mousedown_callback(int x, int y, enum app_mouse_key key)
{
    input_mouse_lockcursor(x, y);
}

void mouseup_callback(int x, int y, enum app_mouse_key key)
{
    input_mouse_unlockcursor();
}

/*************************************************************************************************/
int main(int argc, char** argv)
{
    result_t r;
    init_params* params = NULL;

    /* init core library */
    uint flags = CORE_INIT_ALL;
#if defined(_RETAIL_)
    BIT_REMOVE(flags, CORE_INIT_TRACEMEM);
#endif
    r = core_init(flags);

    /* After core init, you can set logging options */
    char logfile[DH_PATH_MAX];
    log_outputconsole(TRUE);
    log_outputfile(TRUE, path_join(logfile, util_getexedir(logfile), "log.txt", NULL));

    /* check arguments:
     * --display-modes: enumerate all gpus and display modes, print them as json
     */
    if (argc > 1 && str_isequal_nocase(argv[1], "--display-modes")) {
        char* json_modes = app_display_querymodes();
        if (json_modes != NULL) {
            puts(json_modes);
            app_display_freemodes(json_modes);
        }   else    {
            puts("Error: could not enumerate modes");
        }
        core_release(FALSE);
        return 0;
    }

    /* Initialize engine application framework, This is the first thing we should do */
    /* load config, and initialize application */
    params = app_config_default();
    if (params == NULL)
        err_sendtolog(TRUE);
    params->flags |= (ENG_FLAG_DEBUG | ENG_FLAG_DEV | ENG_FLAG_CONSOLE);
    params->gfx.flags |= (GFX_FLAG_DEBUG | GFX_FLAG_FXAA);
    app_config_addconsolecmd(params, "showfps");
    app_config_addconsolecmd(params, "showft");

    /* note: application name will also, be the name of the main window */
    r = app_init("darkHAMMER: test", params);
    if (IS_FAIL(r)) {
        puts(err_getstring());
        goto cleanup;
    }

    /* initialize engine */
    r = eng_init(params);
    if (IS_FAIL(r)) {
        err_sendtolog(FALSE);
        goto cleanup;
    }

    if (!load_props())  {
        err_sendtolog(FALSE);
        goto cleanup;
    }

    /* set callbacks */
    app_window_setupdatefn(update_callback);
    app_window_setkeypressfn(keypress_callback);
    app_window_setactivefn(activate_callback);
    app_window_setmousedownfn(mousedown_callback);
    app_window_setmouseupfn(mouseup_callback);
    gfx_set_debug_renderfunc(debug_view_callback);
    //gfx_set_gridcallback();

    /* initialize ok: show the main window */
    app_window_show();

    /* enter message loop and update engine */
    app_window_run();

cleanup:
    unload_props();

    if (params != NULL)
        app_config_unload(params);

    /* message loop finished, release application */
    eng_release();
    app_release();
#if defined(_RETAIL_)
    core_release(FALSE);
#else
    core_release(TRUE);
#endif
    return 0;
}
