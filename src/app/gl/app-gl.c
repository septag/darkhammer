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

/**
 * Additional code: Davide Bacchet
 */

#if defined(_GL_)

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#if defined(_WIN_)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif defined(_LINUX_)
#define GLFW_EXPOSE_NATIVE_GLX
#define GLFW_EXPOSE_NATIVE_X11
#include <X11/extensions/Xrandr.h>
#elif defined(_OSX_)
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#endif

#include "GLFW/glfw3native.h"

#include "dhcore/core.h"
#include "dhcore/json.h"

#include "init-params.h"
#include "app.h"
#include "input.h"

#define DEFAULT_WIDTH   1280
#define DEFAULT_HEIGHT  720

/*************************************************************************************************
 * types
 */
struct app_gl
{
    char name[32];
    uint width;
    uint height;
    uint refresh_rate;
    int active;
    int always_active;
    int init;

    GLFWwindow* wnd;

    /* callbacks */
    pfn_app_create create_fn;
    pfn_app_destroy destroy_fn;
    pfn_app_resize resize_fn;
    pfn_app_active active_fn;
    pfn_app_keypress keypress_fn;
    pfn_app_update update_fn;
    pfn_app_mousedown mousedown_fn;
    pfn_app_mouseup mouseup_fn;
    pfn_app_mousemove mousemove_fn;

    uint last_key;
};

/*************************************************************************************************
 * globals
 */
struct app_gl* g_app = NULL;

/*************************************************************************************************
 * callbacks for glfw
 */
void glfw_window_resize(GLFWwindow* wnd, int w, int h);
void glfw_window_focus(GLFWwindow* wnd, int focused);
void glfw_window_close(GLFWwindow* wnd);
void glfw_error_callback(int error, const char* desc);
void glfw_window_char(GLFWwindow* wnd, unsigned int ch);
void glfw_window_keypress(GLFWwindow* wnd, int key, int scancode, int action, int mods);
void glfw_window_mousebtn(GLFWwindow* wnd, int button, int action, int mods);
void glfw_window_mousepos(GLFWwindow* wnd, double xpos, double ypos);

/* fwd */
struct app_wnd* app_find_window(const char* wnd_name);
struct app_wnd* app_find_window_byhdl(GLFWwindow* w);
struct app_wnd* app_add_window(const char* name, uint width, uint height, uint refresh_rate,
    int fullscreen, wnd_t wnd_override);
void app_remove_window(const char* name);

/*************************************************************************************************
 * inlines
 */
INLINE void app_convert_gfxver(appGfxDeviceVersion hwver, OUT int* major, OUT int* minor)
{
    switch (hwver)	{
    case appGfxDeviceVersion::GL3_2:
        *major = 3;
        *minor = 2;
        break;
    case appGfxDeviceVersion::GL3_3:
        *major = 3;
        *minor = 3;
        break;
    case appGfxDeviceVersion::GL4_0:
        *major = 4;
        *minor = 0;
        break;
    case appGfxDeviceVersion::GL4_1:
        *major = 4;
        *minor = 1;
        break;
    case appGfxDeviceVersion::GL4_2:
        *major = 4;
        *minor = 2;
        break;
    case appGfxDeviceVersion::GL4_3:
        *major = 4;
        *minor = 3;
        break;
    case appGfxDeviceVersion::GL4_4:
        *major = 4;
        *minor = 4;
        break;
    default:
        *major = INT32_MAX;
        *minor = INT32_MAX;
    }
}

/*************************************************************************************************/
result_t app_init(const char* name, const struct appInitParams* params)
{
    ASSERT(g_app == NULL);
    if (g_app != NULL)  {
        err_print(__FILE__, __LINE__, "application already initialized");
        return RET_FAIL;
    }

    /* create application */
    log_print(LOG_TEXT, "init OpenGL app ...");

    struct app_gl* app = (struct app_gl*)ALLOC(sizeof(struct app_gl), 0);
    ASSERT(app);
    memset(app, 0x00, sizeof(struct app_gl));
    g_app = app;

    input_zero();

    str_safecpy(app->name, sizeof(app->name), name);

    uint width = params->gfx.width;
    uint height = params->gfx.height;
    if (width == 0)
        width = DEFAULT_WIDTH;
    if (height == 0)
        height = DEFAULT_HEIGHT;

    /* initialize glfw */
    if (!glfwInit())    {
        err_print(__FILE__, __LINE__, "gl-app init failed: could not init glfw");
        return RET_FAIL;
    }

    /* create main window */
    int major, minor;
    glfwSetErrorCallback(glfw_error_callback);
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

    /* construct GL version for context creation */
    app_convert_gfxver(params->gfx.hwver, &major, &minor);
    if (major != INT32_MAX && minor != INT32_MAX)   {
        // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
    }
    if (BIT_CHECK(params->gfx.flags, appGfxFlags::DEBUG))
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    log_print(LOG_INFO, "  init OpenGL context and window ...");

    GLFWmonitor* mon = BIT_CHECK(params->gfx.flags, appGfxFlags::FULLSCREEN) ? 
        glfwGetPrimaryMonitor() : NULL;

    GLFWwindow* wnd = glfwCreateWindow((int)width, (int)height, name, mon, NULL);
    if (wnd == NULL) {
        err_print(__FILE__, __LINE__, "create window/context failed");
        return RET_FAIL;
    }
    app->wnd = wnd;

    /* callbacks */
    glfwSetFramebufferSizeCallback(wnd, glfw_window_resize);
    glfwSetWindowCloseCallback(wnd, glfw_window_close);
    glfwSetWindowFocusCallback(wnd, glfw_window_focus);
    glfwSetCharCallback(wnd, glfw_window_char);
    glfwSetKeyCallback(wnd, glfw_window_keypress);
    glfwSetMouseButtonCallback(wnd, glfw_window_mousebtn);
    glfwSetCursorPosCallback(wnd, glfw_window_mousepos);

    glfwMakeContextCurrent(wnd);
    glfwSwapInterval(BIT_CHECK(params->gfx.flags, appGfxFlags::VSYNC) ? 1 : 0);

    str_safecpy(app->name, sizeof(app->name), name);
    g_app->wnd = wnd;
    app->width = width;
    app->height = height;
    app->always_active = TRUE;
    app->refresh_rate = params->gfx.refresh_rate;

    /* initialize input system */
    input_init();

    app->init = TRUE;
    return RET_OK;
}

void app_release()
{
    if (g_app == NULL)
        return;

    struct app_gl* app = g_app;

    input_release();

    if (app->wnd != NULL)
        glfwDestroyWindow(app->wnd);
    glfwTerminate();
    FREE(app);
    g_app = NULL;
}

void app_window_run()
{
    struct app_gl* app = g_app;
    ASSERT(g_app->wnd);

    while (!glfwWindowShouldClose(g_app->wnd))   {
        if (app->active || app->always_active)
            glfwPollEvents();
        else
            glfwWaitEvents();

        if (g_app->update_fn)
            g_app->update_fn();
    }
}

void app_window_readjust(uint client_width, uint client_height)
{
    ASSERT(g_app);
    ASSERT(g_app->wnd);

    glfwSetWindowSize(g_app->wnd, client_width, client_height);
}

void app_window_alwaysactive(int active)
{
    g_app->always_active = active;
}

result_t app_window_resize(uint width, uint height)
{
    struct app_gl* app = g_app;
    ASSERT(app);
    if (!app->init)
        return RET_FAIL;

    app->width = width;
    app->height = height;

    return RET_OK;
}

void app_window_swapbuffers()
{
    glfwSwapBuffers(g_app->wnd);
}

int app_window_isactive()
{
    return g_app->active;
}

uint app_window_getwidth()
{
    return g_app->width;
}

uint app_window_getheight()
{
    return g_app->height;
}

void app_window_setcreatefn(pfn_app_create fn)
{
    ASSERT(g_app);
    g_app->create_fn = fn;
}

void app_window_setdestroyfn(pfn_app_destroy fn)
{
    ASSERT(g_app);
    g_app->destroy_fn = fn;
}

void app_window_setresizefn(pfn_app_resize fn)
{
    ASSERT(g_app);
    g_app->resize_fn = fn;
}

void app_window_setactivefn(pfn_app_active fn)
{
    ASSERT(g_app);
    g_app->active_fn = fn;
}

void app_window_setkeypressfn(pfn_app_keypress fn)
{
    ASSERT(g_app);
    g_app->keypress_fn = fn;
}

void app_window_setupdatefn(pfn_app_update fn)
{
    ASSERT(g_app);
    g_app->update_fn = fn;
}

void app_window_setmousedownfn(pfn_app_mousedown fn)
{
    ASSERT(g_app);
    g_app->mousedown_fn = fn;
}

void app_window_setmouseupfn(pfn_app_mouseup fn)
{
    ASSERT(g_app);
    g_app->mouseup_fn = fn;
}

void app_window_setmousemovefn(pfn_app_mousemove fn)
{
    ASSERT(g_app);
    g_app->mousemove_fn = fn;
}

void* app_gfx_getcontext()
{
    return NULL;
}

void app_window_show()
{
    ASSERT(g_app);
    ASSERT(g_app->wnd);
    glfwShowWindow(g_app->wnd);
}

void app_window_hide()
{
    ASSERT(g_app);
    ASSERT(g_app->wnd);

    glfwHideWindow(g_app->wnd);
}

const char* app_getname()
{
    return g_app->name;
}

void glfw_error_callback(int error, const char* desc)
{
    err_printf(__FILE__, __LINE__, "GLFW: (code: %d) %s", error, desc);
}

/*************************************************************************************************/
void glfw_window_resize(GLFWwindow* wnd, int width, int height)
{
    app_window_resize((uint)width, (uint)height);

    if (g_app->resize_fn != NULL)
        g_app->resize_fn((uint)width, (uint)height);
}

void glfw_window_focus(GLFWwindow* wnd, int focused)
{
    g_app->active = focused;

    if (g_app->active_fn != NULL)
        g_app->active_fn(focused);
}

void glfw_window_close(GLFWwindow* wnd)
{
    if (g_app->destroy_fn != NULL)
        g_app->destroy_fn();
}

void glfw_window_char(GLFWwindow* wnd, unsigned int ch)
{
    if (g_app->keypress_fn != NULL) {
        g_app->keypress_fn(ch & 0xff, g_app->last_key);
        g_app->last_key = 0;
    }
}

void glfw_window_keypress(GLFWwindow* wnd, int key, int scancode, int action, int mods)
{
    g_app->last_key = (uint)key;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        glfw_window_char(wnd, 0);
}

void glfw_window_mousebtn(GLFWwindow* wnd, int button, int action, int mods)
{
    appMouseKey mousekey;

    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        mousekey = appMouseKey::LEFT;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        mousekey = appMouseKey::RIGHT;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        mousekey = appMouseKey::MIDDLE;
        break;
    default:
        mousekey = appMouseKey::UNKNOWN;
        break;
    }

    double xd, yd;
    glfwGetCursorPos(wnd, &xd, &yd);
    if (action == GLFW_PRESS && g_app->mousedown_fn != NULL)
        g_app->mousedown_fn((int)xd, (int)yd, mousekey);
    else if (action == GLFW_RELEASE && g_app->mouseup_fn != NULL)
        g_app->mouseup_fn((int)xd, (int)yd, mousekey);
}

void glfw_window_mousepos(GLFWwindow* wnd, double xpos, double ypos)
{
    ASSERT(g_app);
    if (g_app->mousemove_fn != NULL)
        g_app->mousemove_fn((int)xpos, (int)ypos);
}

char* app_display_querymodes()
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
        return NULL;

    uint adapter_id = 0;
    size_t outsz;

    /* start json data (adapter array) */
    json_t jroot = json_create_arr();

    /* read adapters */
    while (adapter_id == 0)  {
        json_t jadapter = json_create_obj();
        json_additem_toarr(jroot, jadapter);

        json_additem_toobj(jadapter, "name", json_create_str("Graphics Card #1"));
        json_additem_toobj(jadapter, "id", json_create_num(0));

        /* enumerate monitors */
        json_t joutputs = json_create_arr();
        json_additem_toobj(jadapter, "monitors", joutputs);
        int output_cnt = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&output_cnt);
        for (int i = 0; i < output_cnt; i++)    {
            json_t joutput = json_create_obj();
            json_additem_toarr(joutputs, joutput);

            json_additem_toobj(joutput, "id", json_create_num((fl64)i));
            json_additem_toobj(joutput, "name", json_create_str(glfwGetMonitorName(monitors[i])));

            /* enumerate modes */
            json_t jmodes = json_create_arr();
            json_additem_toobj(joutput, "modes", jmodes);

            int mode_cnt;
            const GLFWvidmode* modes = glfwGetVideoModes(monitors[i], &mode_cnt);
            for (int i = 0; i < mode_cnt; i++)   {
                json_t jmode = json_create_obj();
                json_additem_toobj(jmode, "width", json_create_num((fl64)modes[i].width));
                json_additem_toobj(jmode, "height", json_create_num((fl64)modes[i].height));
                json_additem_toobj(jmode, "refresh-rate",
                    json_create_num((fl64)modes[i].refreshRate));
                json_additem_toarr(jmodes, jmode);
            }
        }

        adapter_id ++;
    }

    char* r = json_savetobuffer(jroot, &outsz, FALSE);
    json_destroy(jroot);

    return r;
}

void app_display_freemodes(char* dispmodes)
{
    ASSERT(dispmodes);
    json_deletebuffer(dispmodes);
}

wnd_t app_window_gethandle()
{
    ASSERT(g_app);
    ASSERT(g_app->wnd);
#if defined(_LINUX_)
    return glfwGetX11Window(g_app->wnd);
#elif defined(_WIN_)
    return glfwGetWin32Window(g_app->wnd);
#elif defined(_OSX_)
    return glfwGetCocoaWindow(g_app->wnd);
#endif
}

GLFWwindow* app_window_getplatform_w()
{
    ASSERT(g_app);
    return g_app->wnd;
}

#endif /* _GL_ */
