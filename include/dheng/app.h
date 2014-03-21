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
 * @defgroup app Application framework
 * Application frame is a set of function and callbacks that you can use to initialize 3d device,
 * and also create a window that engine renders to it.
 */

/*
 * Contributors: Davide Bacchet
 */


#ifndef __APP_H__
#define __APP_H__

#include "dhcore/types.h"
#include "engine-api.h"
#include "init-params.h"

/* fwd */
struct gfx_device_info;

/* */
typedef void* app_t;
typedef void* wnd_t;

enum app_mouse_key
{
    APP_MOUSEKEY_UNKNOWN = 0,
    APP_MOUSEKEY_LEFT,
    APP_MOUSEKEY_RIGHT,
    APP_MOUSEKEY_MIDDLE
};

/* event callback definitions */
/**
 * @ingroup app
 */
typedef void (*pfn_app_create)(const char* wnd_name);
/**
 * @ingroup app
 */
typedef void (*pfn_app_destroy)(const char* wnd_name);
/**
 * @ingroup app
 */
typedef void (*pfn_app_resize)(const char* wnd_name, uint width, uint height);
/**
 * @ingroup app
 */
typedef void (*pfn_app_active)(const char* wnd_name, bool_t active);
/**
 * @ingroup app
 */
typedef void (*pfn_app_keypress)(const char* wnd_name, char charcode, uint vkeycode);
/**
 * @ingroup app
 */
typedef void (*pfn_app_mousedown)(const char* wnd_name, int x, int y, enum app_mouse_key key);
/**
 * @ingroup app
 */
typedef void (*pfn_app_mouseup)(const char* wnd_name, int x, int y, enum app_mouse_key key);
/**
 * @ingroup app
 */
typedef void (*pfn_app_mousemove)(const char* wnd_name, int x, int y);
/**
 * @ingroup app
 */
typedef void (*pfn_app_update)();


/**
 * Loads configurations from json file
 * @param cfg_jsonfile JSON configuration file
 * @return init_params structure, or NULL if error occured during json load
 * @see app_unload_config
 * @see init_params
 * @ingroup app
 */
ENGINE_API struct init_params* app_load_config(const char* cfg_jsonfile);

/**
 * Loads default configuration, use this function to get a valid configuration structure and then
 * override the stuff you want.
 * @return A valid init_params structures
 * @see app_unload_config
 * @see init_params
 * @ingroup app
 */
ENGINE_API struct init_params* app_defaultconfig();

/**
 * Adds console command to config
 * @ingroup app
 */
ENGINE_API void app_config_add_consolecmd(struct init_params* cfg, const char* cmd);

/**
 * Unloads configuration structure from memory
 * @param cfg Valid configuration structure, must not be NULL
 * @see app_load_config
 * @see app_defaultconfig
 * @see init_params
 * @ingroup app
 */
ENGINE_API void app_unload_config(struct init_params* cfg);

/**
 * Receives JSON data string of supported display modes for the device\n
 * Applications can parse the json string and choose a proper resolution for app initialization
 * @return Null-terminated JSON string, consisting of all adapters and their display modes
 * @see app_free_displaymodes
 * @see app_load_config
 * @see app_defaultconfig
 * @see init_params
 * @ingroup app
 */
ENGINE_API char* app_query_displaymodes();

/**
 * Frees json string returned by @e app_query_displaymodes
 * @see app_query_displaymodes
 * @ingroup app
 */
ENGINE_API void app_free_displaymodes(char* dispmodes);

/**
 * Initializes application and 3D device
 * @param name Name of the application, also works for titlebar of the possible created window
 * @param params init params structure, required for engine and app initialization
 * @param wnd_override Optional parameter that you can set a valid window handle to skip window
 * creation, so your own custom window handles drawing and window events. This parameter can be NULL.
 * @see init_params
 * @ingroup app
 */
ENGINE_API result_t app_init(const char* name, const struct init_params* params,
    OPTIONAL wnd_t wnd_override);

/**
 * Releases the application and 3D device, must be called after engine is released
 * @see app_init
 * @ingroup app
 */
ENGINE_API void app_release();

/**
 * Enters application's event loop and updates until exit signal is called.\n
 * If you have overrided window on @e app_init, there is no need to call this function
 * @see app_init
 * @ingroup app
 */
ENGINE_API void app_update();

/**
 * Adjuts application window, to match the client size.\n
 * For example if you set 800x600, the client area of the window will be that exact size.
 * @ingroup app
 */
ENGINE_API void app_readjust(uint client_width, uint client_height);

/**
 * Sets if application should always be active and running, no matter if it loses focus or not
 * @ingroup app
 */
ENGINE_API void app_set_alwaysactive(bool_t active);

/**
 * Changes render target of the application, render target for the application is the actual target
 * window that you want to render to. You can have multiple windows and draw to them different stuff.
 * @param wnd_name Name of the target window, set this parameter to NULL if you want to draw to the
 * main window created with @e app_init
 * @see app_add_rendertarget
 * @see app_init
 * @ingroup app
 */
ENGINE_API void app_set_rendertarget(OPTIONAL const char* wnd_name);

/**
 * Swaps render buffers and presents the render data to the window.\n
 * Use this function to manually present renders to your own custom windows.
 * @ingroup app
 */
ENGINE_API void app_swapbuffers();

/**
 * Clears render target (target window)
 * @ingroup app
 */
ENGINE_API void app_clear_rendertarget(const float color[4], float depth, uint8 stencil, uint flags);

/**
 * Adds a new render target to your. This function is useful for creating editor and tools, in which
 * you want to render to multiple views.
 * @param wnd_name Window name (alias), future calls to change targets, will require this name
 * @param wnd Valid window handle, This is an OS dependent value
 * @param width Width of the render-target in pixels
 * @param height Height of the render-target in pixels
 * @see app_set_rendertarget
 * @ingroup app
 */
ENGINE_API result_t app_add_rendertarget(const char* wnd_name, OPTIONAL wnd_t wnd,
                                         uint width, uint height);

/**
 * Removes added render target (and it's window)
 * @param wnd_name Window name that used to create the render-target view
 * @see app_add_rendertarget
 * @ingroup app
 */
ENGINE_API void app_remove_rendertarget(const char* wnd_name);

/**
 * Returns TRUE if application has focus
 * @ingroup app
 */
ENGINE_API bool_t app_isactive();

/**
 * Returns applications main render-target width, in pixels
 * @ingroup app
 */
ENGINE_API uint app_get_wndwidth();

/**
 * Returns applications main render-target height, in pixels
 * @ingroup app
 */
ENGINE_API uint app_get_wndheight();

/**
 * Sets @e create callback function. Triggers when application's window is created.\n
 * Doesn't work for overrided windows.
 * @ingroup app
 */
ENGINE_API void app_set_createfunc(pfn_app_create fn);

/**
 * Sets @e destroy callback function. Triggers when application's main window is destroyed.\n
 * Doesn't work for overrided windows.
 * @ingroup app
 */
ENGINE_API void app_set_destroyfunc(pfn_app_destroy fn);

/**
 * Sets @e resize callback function. Triggers when application's main window is resized.\n
 * Doesn't work for overrided windows
 * @ingroup app
 */
ENGINE_API void app_set_resizefunc(pfn_app_resize fn);

/**
 * Sets @e active callback function, Triggers when application's main window loses or gains focus.\n
 * Doesn't work for overrided windows
 * @ingroup app
 */
ENGINE_API void app_set_activefunc(pfn_app_active fn);

/**
 * Sets @e keypress callback function, Triggers when application's main window receives keyboard input
 * Doesn't work for overrided windows
 * @ingroup app
 */
ENGINE_API void app_set_keypressfunc(pfn_app_keypress fn);

/**
 * Sets @e mouse key down callback function
 * Doesn't work for overrided windows
 * @ingroup app
 */
ENGINE_API void app_set_mousedownfunc(pfn_app_mousedown fn);

/**
 * Sets @e mouse key up function
 * Doesn't work for overrided windows
 * @ingroup app
 */
ENGINE_API void app_set_mouseupfunc(pfn_app_mouseup fn);

/**
 * Sets @e mouse move function
 * Doesn't work for overrided windows
 * @ingroup app
 */
ENGINE_API void app_set_mousemovefunc(pfn_app_mousemove fn);

/**
 * Sets @e update callback function, Triggers when application needs update during main event loop.\n
 * This function is called almost on every frame, so you can do any extra updates and syncs in this
 * callback.\n
 * Doesn't work for overrided windows, applications which override the window should have their own
 * update function
 * @ingroup app
 */
ENGINE_API void app_set_updatefunc(pfn_app_update fn);

/**
 * Returns name string for graphics device and initialized driver
 * @ingroup app
 */
ENGINE_API const char* app_get_gfxdriverstr();

/**
 * Fetches GPU info
 * @see gfx_device_info
 * @ingroup app
 */
ENGINE_API void app_get_gfxinfo(struct gfx_device_info* info);

/**
 * Returns @e actual supported graphics hardware level
 * @see gfx_hwver
 * @ingroup app
 */
ENGINE_API enum gfx_hwver app_get_gfxver();


ENGINE_API void* app_get_mainwnd();
ENGINE_API void* app_get_mainctx();

/**
 * Shows application window. \n
 * @param wnd_name Valid window name, set this parameter to NULL to show main window
 * @ingroup app
 */
ENGINE_API void app_show_window(OPTIONAL const char* wnd_name);

/**
 * Hides application window. \n
 * @param wnd_name Valid window name, set this parameter to NULL to hide main window
 * @ingroup app
 */
ENGINE_API void app_hide_window(OPTIONAL const char* wnd_name);

/**
 * Returns application's name
 * @see app_init
 * @ingroup app
 */
ENGINE_API const char* app_get_name();

/**
 * Manually resizes the window and internal buffers.\n
 * This function should be used with applications that overrided windows. If app framework owns the
 * window, then you shouldn't call this as it is called internally by app's window event handlers.
 * @param name Name (alias) of the target window. Set this parameter to NULL if you want to resize the main window
 * @param width Width of the window client area, in pixels
 * @param height Height of the window client area, in pixels
 * @return RET_OK if process if successful, or RET_FAIL on error
 * @see app_init
 * @ingroup app
 */
ENGINE_API result_t app_resize_window(OPTIONAL const char* name, uint width, uint height);

#endif /* __APP_H__ */
