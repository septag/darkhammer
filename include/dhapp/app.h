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
#include "app-api.h"
#include "init-params.h"

/* fwd */
struct gfx_device_info;

/* platform dependent types */
#if defined(_WIN_)
#include "dhcore/win.h"
typedef HWND wnd_t;
#elif defined(_LINUX_)
#include "X11/Xlib.h"
typedef Window wnd_t;
#elif defined(_OSX_)
  #if defined(__OBJC__)
    #import <Cocoa/Cocoa.h>
  #else
    typedef void* id;
  #endif
typedef id wnd_t;
#else
  #error "specific window type is not implemented for this platform"
#endif

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
typedef void (*pfn_app_create)();
/**
 * @ingroup app
 */
typedef void (*pfn_app_destroy)();
/**
 * @ingroup app
 */
typedef void (*pfn_app_resize)(uint width, uint height);
/**
 * @ingroup app
 */
typedef void (*pfn_app_active)(int active);
/**
 * @ingroup app
 */
typedef void (*pfn_app_keypress)(char charcode, uint vkeycode);
/**
 * @ingroup app
 */
typedef void (*pfn_app_mousedown)(int x, int y, enum app_mouse_key key);
/**
 * @ingroup app
 */
typedef void (*pfn_app_mouseup)(int x, int y, enum app_mouse_key key);
/**
 * @ingroup app
 */
typedef void (*pfn_app_mousemove)(int x, int y);
/**
 * @ingroup app
 */
typedef void (*pfn_app_update)();


/**
 * Loads configurations from json file
 * @param cfg_jsonfile JSON configuration file
 * @return init_params structure, or NULL if error occured during json load
 * @see app_config_unload
 * @see init_params
 * @ingroup app
 */
APP_API struct init_params* app_config_load(const char* cfg_jsonfile);

/**
 * Loads default configuration, use this function to get a valid configuration structure and then
 * override the stuff you want.
 * @return A valid init_params structures
 * @see app_config_unload
 * @see init_params
 * @ingroup app
 */
APP_API struct init_params* app_config_default();

/**
 * Adds console command to config
 * @ingroup app
 */
APP_API void app_config_addconsolecmd(struct init_params* cfg, const char* cmd);

/**
 * Unloads configuration structure from memory
 * @param cfg Valid configuration structure, must not be NULL
 * @see app_config_load
 * @see app_config_default
 * @see init_params
 * @ingroup app
 */
APP_API void app_config_unload(struct init_params* cfg);

/**
 * Receives JSON data string of supported display modes for the device\n
 * Applications can parse the json string and choose a proper resolution for app initialization
 * @return Null-terminated JSON string, consisting of all adapters and their display modes
 * @see app_display_freemodes
 * @see app_config_load
 * @see app_config_default
 * @see init_params
 * @ingroup app
 */
APP_API char* app_display_querymodes();

/**
 * Frees json string returned by @e app_display_querymodes
 * @see app_display_querymodes
 * @ingroup app
 */
APP_API void app_display_freemodes(char* dispmodes);

/**
 * Initializes application and 3D device
 * @param name Name of the application, also works for titlebar of the possible created window
 * @param params init params structure, required for engine and app initialization
 * @param wnd_override Optional parameter that you can set a valid window handle to skip window
 * creation, so your own custom window handles drawing and window events. This parameter can be NULL.
 * @see init_params
 * @ingroup app
 */
APP_API result_t app_init(const char* name, const struct init_params* params);

/**
 * Releases the application and 3D device, must be called after engine is released
 * @see app_init
 * @ingroup app
 */
APP_API void app_release();

/**
 * Enters application's event loop and updates until exit signal is called.\n
 * If you have overrided window on @e app_init, there is no need to call this function
 * @see app_init
 * @ingroup app
 */
APP_API void app_window_run();

/**
 * Adjuts application window, to match the client size.\n
 * For example if you set 800x600, the client area of the window will be that exact size.
 * @ingroup app
 */
APP_API void app_window_readjust(uint client_width, uint client_height);

/**
 * Sets if application should always be active and running, no matter if it loses focus or not
 * @ingroup app
 */
APP_API void app_window_alwaysactive(int active);

/**
 * Swaps render buffers and presents the render data to the window.\n
 * Use this function to manually present renders to your own custom windows.
 * @ingroup app
 */
APP_API void app_window_swapbuffers();

/**
 * Returns TRUE if application has focus
 * @ingroup app
 */
APP_API int app_window_isactive();

/**
 * Returns applications main render-target width, in pixels
 * @ingroup app
 */
APP_API uint app_window_getwidth();

/**
 * Returns applications main render-target height, in pixels
 * @ingroup app
 */
APP_API uint app_window_getheight();

/**
 * Sets @e create callback function. Triggers when application's window is created.\n
 * Doesn't work for overrided windows.
 * @ingroup app
 */
APP_API void app_window_setcreatefn(pfn_app_create fn);

/**
 * Sets @e destroy callback function. Triggers when application's main window is destroyed.\n
 * Doesn't work for overrided windows.
 * @ingroup app
 */
APP_API void app_window_setdestroyfn(pfn_app_destroy fn);

/**
 * Sets @e resize callback function. Triggers when application's main window is resized.\n
 * Doesn't work for overrided windows
 * @ingroup app
 */
APP_API void app_window_setresizefn(pfn_app_resize fn);

/**
 * Sets @e active callback function, Triggers when application's main window loses or gains focus.\n
 * Doesn't work for overrided windows
 * @ingroup app
 */
APP_API void app_window_setactivefn(pfn_app_active fn);

/**
 * Sets @e keypress callback function, Triggers when application's main window receives keyboard input
 * Doesn't work for overrided windows
 * @ingroup app
 */
APP_API void app_window_setkeypressfn(pfn_app_keypress fn);

/**
 * Sets @e mouse key down callback function
 * Doesn't work for overrided windows
 * @ingroup app
 */
APP_API void app_window_setmousedownfn(pfn_app_mousedown fn);

/**
 * Sets @e mouse key up function
 * Doesn't work for overrided windows
 * @ingroup app
 */
APP_API void app_window_setmouseupfn(pfn_app_mouseup fn);

/**
 * Sets @e mouse move function
 * Doesn't work for overrided windows
 * @ingroup app
 */
APP_API void app_window_setmousemovefn(pfn_app_mousemove fn);

/**
 * Sets @e update callback function, Triggers when application needs update during main event loop.\n
 * This function is called almost on every frame, so you can do any extra updates and syncs in this
 * callback.\n
 * Doesn't work for overrided windows, applications which override the window should have their own
 * update function
 * @ingroup app
 */
APP_API void app_window_setupdatefn(pfn_app_update fn);


APP_API wnd_t app_window_gethandle();

/**
 * Shows application window. \n
 * @param wnd_name Valid window name, set this parameter to NULL to show main window
 * @ingroup app
 */
APP_API void app_window_show();

/**
 * Hides application window. \n
 * @param wnd_name Valid window name, set this parameter to NULL to hide main window
 * @ingroup app
 */
APP_API void app_window_hide();

/**
 * Returns application's name
 * @see app_init
 * @ingroup app
 */
APP_API const char* app_getname();

/**
 * Manually resizes the window and internal buffers.\n
 * This function should be used with applications that overrided windows. If app framework owns the
 * window, then you shouldn't call this as it is called internally by app's window event handlers.
 * @param width Width of the window client area, in pixels
 * @param height Height of the window client area, in pixels
 * @return RET_OK if process if successful, or RET_FAIL on error
 * @see app_init
 * @ingroup app
 */
APP_API result_t app_window_resize(uint width, uint height);

/* Direct3D specific stuff */
#ifdef _D3D_
#include <dxgi.h>
#include <d3d11.h>
APP_API result_t app_d3d_initdev(wnd_t hwnd, const char* name, const struct init_params* params);
APP_API void app_d3d_getswapchain_buffers(OUT ID3D11Texture2D** pbackbuff, 
                                          OUT ID3D11Texture2D** pdepthbuff);
APP_API void app_d3d_getswapchain_views(OUT ID3D11RenderTargetView** prtv, 
                                        OUT ID3D11DepthStencilView** pdsv);
APP_API ID3D11Device* app_d3d_getdevice();
APP_API ID3D11DeviceContext* app_d3d_getcontext();
APP_API IDXGIAdapter* app_d3d_getadapter();
APP_API enum gfx_hwver app_d3d_getver();
#endif

#endif /* __APP_H__ */
