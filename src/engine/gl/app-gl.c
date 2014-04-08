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

#include "app.h"

#if defined(_GL_)

#include "GL/glew.h"
#include "GLFWEXT/glfw3.h"

#include "dhcore/core.h"
#include "dhcore/linked-list.h"

#include "init-params.h"
#include "engine.h"
#include "gfx.h"

#define DEFAULT_WIDTH   1280
#define DEFAULT_HEIGHT  720

/* amd info */
#define VBO_FREE_MEMORY_ATI	0x87FB
#define TEXTURE_FREE_MEMORY_ATI 0x87FC
#define RENDERBUFFER_FREE_MEMORY_ATI 0x87FD
#define TOTAL_PHYSICAL_MEMORY_ATI 0x87FE

/* nvidia info */
#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049

/*************************************************************************************************
 * types
 */
struct app_wnd
{
    char name[32];
    GLFWwindow* w;
    GLFWcontext* ctx;

    uint width;
    uint height;
    uint refresh_rate;

    struct linked_list lnode;
};

struct app_gl
{
    char name[32];
    uint width;
    uint height;
    uint refresh_rate;
    bool_t active;
    bool_t always_active;
    bool_t init;
    enum gfx_hwver glver;
    struct app_wnd* main_wnd;
    struct app_wnd* render_target;

    struct linked_list* wnds;   /* linked-list for window list: data=app_wnd */

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
    bool_t fullscreen, wnd_t wnd_override);
void app_remove_window(const char* name);

void APIENTRY app_gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, GLvoid* user_param);
const char* app_gl_debug_getseverity(GLenum severity);
const char* app_gl_debug_getsource(GLenum source);
const char* app_gl_debug_gettype(GLenum type);

/*************************************************************************************************
 * inlines
 */
INLINE void app_convert_gfxver(enum gfx_hwver hwver, struct version_info* v)
{
    switch (hwver)	{
    case GFX_HWVER_GL3_2:
        v->major = 3;
        v->minor = 2;
        break;
    case GFX_HWVER_GL3_3:
        v->major = 3;
        v->minor = 3;
        break;
    case GFX_HWVER_GL4_0:
        v->major = 4;
        v->minor = 0;
        break;
    case GFX_HWVER_GL4_1:
        v->major = 4;
        v->minor = 1;
        break;
    case GFX_HWVER_GL4_2:
        v->major = 4;
        v->minor = 2;
        break;
    case GFX_HWVER_GL4_3:
        v->major = 4;
        v->minor = 3;
        break;
    case GFX_HWVER_GL4_4:
        v->major = 4;
        v->minor = 4;
        break;
    default:
        v->major = INT32_MAX;
        v->minor = INT32_MAX;
    }
}

INLINE enum gfx_hwver app_get_glver(const struct version_info* v)
{
    if (v->major == 3)	{
        if (v->minor == 2)
            return GFX_HWVER_GL3_2;
        else if (v->minor >= 3)
            return GFX_HWVER_GL3_3;
    }	else if (v->major == 4)	{
        if (v->minor==0)
            return GFX_HWVER_GL4_0;
        else if (v->minor==1)
            return GFX_HWVER_GL4_1;
        else if (v->minor >= 2)
            return GFX_HWVER_GL4_2;
    }

    return GFX_HWVER_UNKNOWN;
}

/*************************************************************************************************/
result_t app_init(const char* name, const struct init_params* params,
    OPTIONAL wnd_t wnd_override)
{
    ASSERT(g_app == NULL);

    eng_zero();

    /* create application */
    log_print(LOG_TEXT, "init OpenGL app ...");

    struct app_gl* app = (struct app_gl*)ALLOC(sizeof(struct app_gl), 0);
    ASSERT(app);
    memset(app, 0x00, sizeof(struct app_gl));
    g_app = app;

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
    struct version_info ver;
    glfwSetErrorCallback(glfw_error_callback);
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

    /* construct GL version for context creation */
    app_convert_gfxver(params->gfx.hwver, &ver);
    if (ver.major != INT32_MAX && ver.minor != INT32_MAX)   {
        // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, ver.major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, ver.minor);
    }
    if (BIT_CHECK(params->gfx.flags, GFX_FLAG_DEBUG))
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    log_print(LOG_INFO, "  init OpenGL ...");
    struct app_wnd* wnd = app_add_window(app->name, width, height, params->gfx.refresh_rate,
        BIT_CHECK(params->gfx.flags, GFX_FLAG_FULLSCREEN), wnd_override);
    if (wnd == NULL)    {
        err_print(__FILE__, __LINE__, "gl-app init failed: coult not create main context/window");
        return RET_FAIL;
    }
    app_set_rendertarget(wnd->name);
    glfwSwapInterval(BIT_CHECK(params->gfx.flags, GFX_FLAG_VSYNC) ? 1 : 0);

    /* init GL functions */
    GLenum glew_ret = glewInit();
    if (glew_ret != GLEW_OK)   {
        err_printf(__FILE__, __LINE__, "gl-app init failed: could not init GLEW: %s",
            glewGetString(glew_ret));
        return RET_FAIL;
    }

    /* recheck the version */
    struct version_info final_ver;
    glGetIntegerv(GL_MAJOR_VERSION, &final_ver.major);
    glGetIntegerv(GL_MINOR_VERSION, &final_ver.minor);

    if (final_ver.major < 3 || (final_ver.major == 3 && final_ver.minor < 2))    {
        err_printf(__FILE__, __LINE__, "gl-app init failed: OpenGL context version does not meet the"
            " requested requirements (GL ver: %d.%d)", final_ver.major, final_ver.minor);
        return RET_FAIL;
    }

    /* set debug callback */
    if (glfwExtensionSupported("GL_ARB_debug_output"))	{
        glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
        /* turn shader compiler errors off, I will catch them myself */
        glDebugMessageControlARB(GL_DEBUG_SOURCE_SHADER_COMPILER, GL_DONT_CARE, GL_DONT_CARE,
            0, NULL, GL_FALSE);
        /* turn API 'other' errors (they are just info for nvidia drivers) off, don't need them */
        glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE,
            0, NULL, GL_FALSE);
        glDebugMessageCallbackARB(app_gl_debug_callback, NULL);
    }

    app->glver = app_get_glver(&final_ver);
    app->main_wnd = wnd;
    app->init = TRUE;
    app->width = width;
    app->height = height;
    app->always_active = TRUE;
    app->refresh_rate = params->gfx.refresh_rate;

    return RET_OK;
}

void app_release()
{
    if (g_app == NULL)
        return;

    struct app_gl* app = g_app;

    /* destroy all windows/contexts */
    struct linked_list* lnode = app->wnds;
    while (lnode)   {
        struct linked_list* next = lnode->next;
        struct app_wnd* wnd = (struct app_wnd*)lnode->data;
        glfwDestroyWindow(wnd->w);
        FREE(wnd);
        lnode = next;
    }

    glfwTerminate();
    FREE(app);
    g_app = NULL;
}

void app_update()
{
    struct app_gl* app = g_app;
    ASSERT(app->main_wnd);

    while (!glfwWindowShouldClose(app->main_wnd->w))   {
        if (app->active || app->always_active)
            glfwPollEvents();
        else
            glfwWaitEvents();

        if (g_app->update_fn)
            g_app->update_fn();
    }
}

void app_readjust(uint client_width, uint client_height)
{
    ASSERT(g_app);
    ASSERT(g_app->main_wnd);

    glfwSetWindowSize(g_app->main_wnd->w, client_width, client_height);
}

void app_set_alwaysactive(bool_t active)
{
    g_app->always_active = active;
}

result_t app_resize_window(OPTIONAL const char* wnd_name, uint width, uint height)
{
    struct app_gl* app = g_app;

    if (!app->init)
        return RET_FAIL;

    /* for main window (this is where main render buffers are created), resize internal buffers */
    if (app->width != width || app->height != height)   {
        gfx_resize(width, height);
        app->width = width;
        app->height = height;
    }

    struct app_wnd* wnd = wnd_name != NULL ? app_find_window(wnd_name) : g_app->main_wnd;
    if (wnd != NULL)   {
        wnd->width = width;
        wnd->height = height;

        /* if the current (render target) swapchain is resized, update variables */
        if (wnd == app->render_target)
            gfx_set_rtvsize(width, height);
    }

    return RET_OK;
}

void app_set_rendertarget(OPTIONAL const char* wnd_name)
{
    g_app->render_target = wnd_name != NULL ? app_find_window(wnd_name) : g_app->main_wnd;
    glfwMakeContextCurrent(g_app->render_target->ctx);
}

void app_swapbuffers()
{
    ASSERT(g_app->render_target);
    glfwSwapBuffers(g_app->render_target->ctx);
}

void app_clear_rendertarget(const float color[4], float depth, uint8 stencil, uint flags)
{
    if (BIT_CHECK(flags, GFX_CLEAR_COLOR))
        glClearColor(color[0], color[1], color[2], color[3]);

    if (BIT_CHECK(flags, GFX_CLEAR_STENCIL))
        glClearStencil(stencil);

    if (BIT_CHECK(flags, GFX_CLEAR_DEPTH))
        glClearDepthf(depth);

    glClear(flags);
}

bool_t app_isactive()
{
    return g_app->active;
}

uint app_get_wndwidth()
{
    return g_app->render_target->width;
}

uint app_get_wndheight()
{
    return g_app->render_target->height;
}

void app_set_createfunc(pfn_app_create fn)
{
    ASSERT(g_app);
    g_app->create_fn = fn;
}

void app_set_destroyfunc(pfn_app_destroy fn)
{
    ASSERT(g_app);
    g_app->destroy_fn = fn;
}

void app_set_resizefunc(pfn_app_resize fn)
{
    ASSERT(g_app);
    g_app->resize_fn = fn;
}

void app_set_activefunc(pfn_app_active fn)
{
    ASSERT(g_app);
    g_app->active_fn = fn;
}

void app_set_keypressfunc(pfn_app_keypress fn)
{
    ASSERT(g_app);
    g_app->keypress_fn = fn;
}

void app_set_updatefunc(pfn_app_update fn)
{
    ASSERT(g_app);
    g_app->update_fn = fn;
}

void app_set_mousedownfunc(pfn_app_mousedown fn)
{
    ASSERT(g_app);
    g_app->mousedown_fn = fn;
}

void app_set_mouseupfunc(pfn_app_mouseup fn)
{
    ASSERT(g_app);
    g_app->mouseup_fn = fn;
}

void app_set_mousemovefunc(pfn_app_mousemove fn)
{
    ASSERT(g_app);
    g_app->mousemove_fn = fn;
}

const char* app_get_gfxdriverstr()
{
    static char info[256];
    sprintf(info, "%s %s %s", glGetString(GL_RENDERER), glGetString(GL_VERSION),
#if defined(_X64_)
        "x64"
#elif defined(_X86_)
        "x86"
#else
        "[]"
#endif
        );
    return info;
}

void app_get_gfxinfo(struct gfx_device_info* info)
{
    memset(info, 0x00, sizeof(struct gfx_device_info));

    const char* vendor = (const char*)glGetString(GL_VENDOR);
    if (strstr(vendor, "ATI"))
        info->vendor = GFX_GPU_ATI;
    else if (strstr(vendor, "NVIDIA"))
        info->vendor = GFX_GPU_NVIDIA;
    else if (strstr(vendor, "INTEL"))
        info->vendor = GFX_GPU_INTEL;
    else
        info->vendor = GFX_GPU_UNKNOWN;
    sprintf(info->desc, "%s, version: %s, GLSL: %s",
        glGetString(GL_RENDERER), glGetString(GL_VERSION),
        glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (glfwExtensionSupported("GL_ATI_meminfo"))	{
        GLint vbo_free = 0;
        glGetIntegerv(VBO_FREE_MEMORY_ATI, &vbo_free);
        info->mem_avail = vbo_free;
    } else if (glfwExtensionSupported("GL_NVX_gpu_memory_info"))	{
        glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &info->mem_avail);
    }
}

enum gfx_hwver app_get_gfxver()
{
    return g_app->glver;
}

void* app_get_mainctx()
{
    return NULL;
}

void app_show_window(OPTIONAL const char* wnd_name)
{
    struct app_wnd* wnd = wnd_name != NULL ? app_find_window(wnd_name) : g_app->main_wnd;
    if (wnd != NULL)
        glfwShowWindow(wnd->w);
}

void app_hide_window(OPTIONAL const char* wnd_name)
{
    struct app_wnd* wnd = wnd_name != NULL ? app_find_window(wnd_name) : g_app->main_wnd;
    if (wnd != NULL)
        glfwHideWindow(wnd->w);
}

const char* app_get_name()
{
    return g_app->name;
}

result_t app_add_rendertarget(const char* wnd_name, wnd_t wnd, uint width, uint height)
{
    struct app_wnd* w = app_add_window(wnd_name, width, height, g_app->refresh_rate, FALSE, wnd);
    if (w == NULL)
        return RET_FAIL;
    return RET_OK;
}

void app_remove_rendertarget(const char* wnd_name)
{
    app_remove_window(wnd_name);
}

struct app_wnd* app_add_window(const char* name, uint width, uint height, uint refresh_rate,
    bool_t fullscreen, wnd_t wnd_override)
{
    struct app_wnd* wnd = (struct app_wnd*)ALLOC(sizeof(struct app_wnd), 0);
    ASSERT(wnd);
    memset(wnd, 0x00, sizeof(struct app_wnd));

    GLFWmonitor* mon = fullscreen ? glfwGetPrimaryMonitor() : NULL;

    if (wnd_override == NULL)  {
        wnd->w = glfwCreateWindow((int)width, (int)height, name, mon,
            (g_app->main_wnd != NULL) ? g_app->main_wnd->ctx : NULL);
    }   else    {
        wnd->ctx = glfwCreateContext((void*)wnd_override, mon,
            (g_app->main_wnd != NULL) ? g_app->main_wnd->ctx : NULL);
    }
    if (wnd->w == NULL && wnd->ctx == NULL) {
        err_print(__FILE__, __LINE__, "create window/context failed");
        FREE(wnd);
        return NULL;
    }

    if (wnd->w != NULL) {
        wnd->ctx = glfwGetWindowContext(wnd->w);
        ASSERT(wnd->ctx);

        /* callbacks */
        glfwSetFramebufferSizeCallback(wnd->w, glfw_window_resize);
        glfwSetWindowCloseCallback(wnd->w, glfw_window_close);
        glfwSetWindowFocusCallback(wnd->w, glfw_window_focus);
        glfwSetCharCallback(wnd->w, glfw_window_char);
        glfwSetKeyCallback(wnd->w, glfw_window_keypress);
        glfwSetMouseButtonCallback(wnd->w, glfw_window_mousebtn);
        glfwSetCursorPosCallback(wnd->w, glfw_window_mousepos);
    }

    strcpy(wnd->name, name);
    wnd->width = width;
    wnd->height = height;
    wnd->refresh_rate = refresh_rate;

    if (fullscreen)
        g_app->active = TRUE;

    list_add(&g_app->wnds, &wnd->lnode, wnd);
    return wnd;
}

void app_remove_window(const char* name)
{
    ASSERT(g_app);
    struct app_gl* app = g_app;

    /* find swapchain in the list */
    struct linked_list* lnode = app->wnds;
    while (lnode)   {
        struct app_wnd* wnd = (struct app_wnd*)lnode->data;
        if (str_isequal(name, wnd->name))   {
            glfwDestroyWindow(wnd->w);
            list_remove(&app->wnds, lnode);
            FREE(wnd);
            break;
        }

        lnode = lnode->next;
    }
}

struct app_wnd* app_find_window(const char* wnd_name)
{
    struct app_gl* app = g_app;
    struct linked_list* lnode = app->wnds;
    while (lnode)   {
        struct app_wnd* w = (struct app_wnd*)lnode->data;
        if (str_isequal(wnd_name, w->name))
            return w;

        lnode = lnode->next;
    }
    return NULL;
}

void glfw_error_callback(int error, const char* desc)
{
    log_printf(LOG_WARNING, "glfw: (code: %d) %s", error, desc);
}

struct app_wnd* app_find_window_byhdl(GLFWwindow* w)
{
    struct app_gl* app = g_app;
    struct linked_list* lnode = app->wnds;
    while (lnode)   {
        struct app_wnd* wnd = (struct app_wnd*)lnode->data;
        if (wnd->w == w)
            return wnd;

        lnode = lnode->next;
    }
    return NULL;
}

void APIENTRY app_gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, GLvoid* user_param)
{
    printf("[Warning] OpenGL: %s (id: %d, source: %s, type: %s, severity: %s)\n", message,
        id, app_gl_debug_getsource(source), app_gl_debug_gettype(type),
        app_gl_debug_getseverity(severity));
}

const char* app_gl_debug_getseverity(GLenum severity)
{
    switch (severity)	{
    case GL_DEBUG_SEVERITY_HIGH_ARB:
        return "high";
    case GL_DEBUG_SEVERITY_MEDIUM_ARB:
        return "medium";
    case GL_DEBUG_SEVERITY_LOW_ARB:
        return "low";
    default:
        return "";
    }
}

const char* app_gl_debug_getsource(GLenum source)
{
    switch (source)	{
    case GL_DEBUG_SOURCE_API_ARB:
        return "api";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
        return "window-system";
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
        return "shader-compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
        return "3rdparty";
    case GL_DEBUG_SOURCE_APPLICATION_ARB:
        return "applcation";
    case GL_DEBUG_SOURCE_OTHER_ARB:
        return "other";
    default:
        return "";
    }
}

const char* app_gl_debug_gettype(GLenum type)
{
    switch (type)	{
    case GL_DEBUG_TYPE_ERROR_ARB:
        return "error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
        return "deprecated";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
        return "undefined";
    case GL_DEBUG_TYPE_PORTABILITY_ARB:
        return "portability";
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:
        return "performance";
    case GL_DEBUG_TYPE_OTHER_ARB:
        return "other";
    default:
        return "";
    }
}

/*************************************************************************************************/
void glfw_window_resize(GLFWwindow* wnd, int width, int height)
{
    struct app_wnd* w = app_find_window_byhdl(wnd);
    ASSERT(w);
    app_resize_window(w->name, (uint)width, (uint)height);

    if (g_app->resize_fn != NULL)
        g_app->resize_fn(w->name, (uint)width, (uint)height);
}

void glfw_window_focus(GLFWwindow* wnd, int focused)
{
    g_app->active = focused;

    if (g_app->active_fn != NULL)
        g_app->active_fn(app_find_window_byhdl(wnd)->name, focused);
}

void glfw_window_close(GLFWwindow* wnd)
{
    if (g_app->destroy_fn != NULL)
        g_app->destroy_fn(app_find_window_byhdl(wnd)->name);
}

void glfw_window_char(GLFWwindow* wnd, unsigned int ch)
{
    if (g_app->keypress_fn != NULL) {
        g_app->keypress_fn(app_find_window_byhdl(wnd)->name, ch & 0xff, g_app->last_key);
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
    enum app_mouse_key mousekey;

    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        mousekey = APP_MOUSEKEY_LEFT;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        mousekey = APP_MOUSEKEY_RIGHT;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        mousekey = APP_MOUSEKEY_MIDDLE;
        break;
    default:
        mousekey = APP_MOUSEKEY_UNKNOWN;
        break;
    }

    double xd, yd;
    glfwGetCursorPos(wnd, &xd, &yd);
    if (action == GLFW_PRESS && g_app->mousedown_fn != NULL)
        g_app->mousedown_fn(app_find_window_byhdl(wnd)->name, (int)xd, (int)yd, mousekey);
    else if (action == GLFW_RELEASE && g_app->mouseup_fn != NULL)
        g_app->mouseup_fn(app_find_window_byhdl(wnd)->name, (int)xd, (int)yd, mousekey);
}

void glfw_window_mousepos(GLFWwindow* wnd, double xpos, double ypos)
{
    ASSERT(g_app);
    if (g_app->mousemove_fn != NULL)
        g_app->mousemove_fn(app_find_window_byhdl(wnd)->name, (int)xpos, (int)ypos);
}

char* app_query_displaymodes()
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

void app_free_displaymodes(char* dispmodes)
{
    ASSERT(dispmodes);
    json_deletebuffer(dispmodes);
}

void* app_get_mainwnd()
{
    return g_app->main_wnd->w;
}

#endif /* _GL_ */
