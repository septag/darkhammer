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

/* Natively create Win32 Window app (for d3d use) */

#include "app.h"

#if defined(_WIN_) && defined(_D3D_)
#if defined(_MSVC_)
/* win8.1 sdk inculdes some d3d types that triggeres "redifinition" warnings with external DX-SDK */
#pragma warning(disable: 4005)
#endif

#include "dhcore/win.h"
#include <dxgi.h>
#include <D3Dcommon.h>
//#include <d3d11_1.h>
#include <d3d11.h>
#include <time.h>

#include "dhcore/core.h"
#include "dhcore/linked-list.h"
#include "engine.h"
#include "gfx.h"

#define DEFAULT_WIDTH   1280
#define DEFAULT_HEIGHT  720
#define BACKBUFFER_COUNT 2
#define DEFAULT_DISPLAY_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM

#if !defined(RELEASE)
#define RELEASE(x)  if ((x) != NULL)  {   (x)->Release();    (x) = NULL;   }
#endif

/*************************************************************************************************
 * types
 */
struct app_swapchain
{
    char name[32];
    HWND hwnd;
    uint width;
    uint height;
    bool_t vsync;
    bool_t fullscreen;
    uint refresh_rate;

    IDXGISwapChain* sc;
    ID3D11Texture2D* backbuff;
    ID3D11RenderTargetView* rtv;
    ID3D11Texture2D* depthbuff;
    ID3D11DepthStencilView* dsv;

    struct linked_list lnode;
};

struct app_win
{
    char name[32];
    HWND hwnd;  /* main window */
    HINSTANCE pinst; /* program instance */
    uint width;
    uint height;
    bool_t active;
    bool_t always_active;
    bool_t init;
    bool_t d3d_dbg;
    bool_t wnd_override;
    uint refresh_rate;

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

    /* dx stuff */
    IDXGIFactory* dxgi_factory;
    IDXGIAdapter* dxgi_adapter;
    ID3D11Device* dev;
    ID3D11DeviceContext* main_ctx;
    struct linked_list* swapchains;
    enum gfx_hwver d3dver;
    struct app_swapchain* render_target;    /* current render target swapchain */
};

/* global */
struct app_win* g_app = NULL;

/* fwd */
void calc_screenpos(uint width, uint height, uint* px, uint* py);
result_t app_init_dxgi(const struct init_params* params,
    OUT IDXGIFactory** pfactory,
    OUT IDXGIAdapter** padapter,
    OUT ID3D11Device** pdev,
    OUT ID3D11DeviceContext** pcontext,
    OUT enum gfx_hwver* pver);
void app_release_dxgi();
result_t app_add_swapchain(const char* name, HWND hwnd, uint width, uint height,
    uint refresh_rate, bool_t fullscreen, bool_t vsync);
void app_remove_swapchain(const char* name);
const char* app_get_wndname(HWND hwnd);
struct app_swapchain* app_find_swapchain(const char* wnd_name);

/* inlines */
INLINE const char* app_get_hwverstr(enum gfx_hwver hwver)
{
    switch (hwver)  {
    case GFX_HWVER_D3D10_0:
        return "10.0";
    case GFX_HWVER_D3D10_1:
        return "10.1";
    case GFX_HWVER_D3D11_0:
        return "11.0";
    case GFX_HWVER_D3D11_1:
        return "11.1";
    }
    return "";
}

/*************************************************************************************************/
static LRESULT CALLBACK msg_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ASSERT(g_app);

    switch (msg)    {
    case WM_CREATE:
        if (g_app->create_fn != NULL)
            g_app->create_fn(app_get_wndname(hwnd));
        break;

    case WM_DESTROY:
        if (g_app->destroy_fn != NULL)
            g_app->destroy_fn(app_get_wndname(hwnd));
        PostQuitMessage(0);        /* quit application loop */
        break;

    case WM_SIZE:
        {
            if (wparam == SIZE_MINIMIZED)
                break;
            uint width = LOWORD(lparam);
            uint height = HIWORD(lparam);
            ASSERT(width != 0);
            ASSERT(height != 0);
            app_resize_window(app_get_wndname(hwnd), width, height);
            if (g_app->resize_fn != NULL)
                g_app->resize_fn(app_get_wndname(hwnd), width, height);
        }
        break;

    case WM_ACTIVATEAPP:
        g_app->active = wparam ? TRUE : FALSE;
        if (g_app->active_fn != NULL)
            g_app->active_fn(app_get_wndname(hwnd), wparam ? TRUE : FALSE);
        break;

    case WM_KEYDOWN:
        if (g_app->keypress_fn != NULL)
            g_app->keypress_fn(app_get_wndname(hwnd), 0, (uint)wparam);
        break;

    case WM_CHAR:
        if (g_app->keypress_fn != NULL)
            g_app->keypress_fn(app_get_wndname(hwnd), (char)wparam, -1);
        break;

    case WM_LBUTTONDOWN:
        if (g_app->mousedown_fn != NULL)    {
            g_app->mousedown_fn(app_get_wndname(hwnd), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                APP_MOUSEKEY_LEFT);
        }
        break;
    case WM_LBUTTONUP:
        if (g_app->mousedown_fn != NULL)    {
            g_app->mouseup_fn(app_get_wndname(hwnd), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                APP_MOUSEKEY_LEFT);
        }
        break;
    case WM_RBUTTONDOWN:
        if (g_app->mousedown_fn != NULL)    {
            g_app->mousedown_fn(app_get_wndname(hwnd), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                APP_MOUSEKEY_RIGHT);
        }
        break;
    case WM_RBUTTONUP:
        if (g_app->mousedown_fn != NULL)    {
            g_app->mouseup_fn(app_get_wndname(hwnd), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                APP_MOUSEKEY_RIGHT);
        }
        break;
    case WM_MBUTTONDOWN:
        if (g_app->mousedown_fn != NULL)    {
            g_app->mousedown_fn(app_get_wndname(hwnd), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                APP_MOUSEKEY_MIDDLE);
        }
        break;
    case WM_MBUTTONUP:
        if (g_app->mousedown_fn != NULL)    {
            g_app->mouseup_fn(app_get_wndname(hwnd), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                APP_MOUSEKEY_MIDDLE);
        }
        break;

    case WM_MOUSEMOVE:
        if (g_app->mousemove_fn != NULL)
            g_app->mousemove_fn(app_get_wndname(hwnd), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        break;
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    return 0;
}

result_t app_init(const char* name, const struct init_params* params, OPTIONAL wnd_t wnd_override)
{
    ASSERT(g_app == NULL);
    if (g_app != NULL)  {
        err_print(__FILE__, __LINE__, "application already initialized");
        return RET_FAIL;
    }

    result_t r;

    eng_zero();

    log_print(LOG_TEXT, "init app (d3d) ...");
    /* create application */
    struct app_win* app = (struct app_win*)ALLOC(sizeof(struct app_win), 0);
    ASSERT(app);
    memset(app, 0x00, sizeof(struct app_win));

    g_app = app;
    str_safecpy(app->name, sizeof(app->name), name);

    uint width = params->gfx.width;
    uint height = params->gfx.height;
    if (width == 0)
        width = DEFAULT_WIDTH;
    if (height == 0)
        height = DEFAULT_HEIGHT;

    HINSTANCE myinst = GetModuleHandle(NULL);

    if (wnd_override == NULL)   {
        /* register window class */
        WNDCLASSEX wndcls;
        memset(&wndcls, 0x00, sizeof(WNDCLASSEX));
        wndcls.cbSize =  sizeof(WNDCLASSEX);
        wndcls.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wndcls.lpfnWndProc = msg_callback;
        wndcls.cbClsExtra = 0;
        wndcls.cbWndExtra = 0;
        wndcls.hInstance = myinst;
        wndcls.hIcon = NULL;
        wndcls.hCursor = LoadCursor(NULL, IDC_ARROW);
        wndcls.hbrBackground = (HBRUSH)(GetStockObject(WHITE_BRUSH));
        wndcls.lpszMenuName = NULL;
        wndcls.lpszClassName = app->name;
        wndcls.hIconSm = NULL;
        if (!RegisterClassEx(&wndcls))  {
            err_print(__FILE__, __LINE__, "win-app init failed");
            return RET_FAIL;
        }

        /* create window */
        RECT wndrc = {0, 0, width, height};
        uint x;
        uint y;

        AdjustWindowRect(&wndrc, WS_OVERLAPPEDWINDOW, FALSE);
        calc_screenpos(wndrc.right-wndrc.left, wndrc.bottom-wndrc.top, &x, &y);
        app->hwnd = CreateWindow(app->name,
                                 name,
                                 WS_OVERLAPPEDWINDOW,
                                 x, y,
                                 wndrc.right - wndrc.left,
                                 wndrc.bottom - wndrc.top,
                                 NULL, NULL,
                                 myinst,
                                 NULL);
        if (app->hwnd == NULL)  {
            err_print(__FILE__, __LINE__, "win-app init failed: could not create window");
            return RET_FAIL;
        }
    }   else    {
        HWND hwnd = (HWND)wnd_override;
        if (!IsWindow(hwnd))    {
            err_print(__FILE__, __LINE__, "win-app init failed: invalid window override handle");
            return RET_FAIL;
        }
        app->hwnd = hwnd;
        app->wnd_override = TRUE;
    }

    /* init DXGI */
    r = app_init_dxgi(params, &app->dxgi_factory, &app->dxgi_adapter, &app->dev, &app->main_ctx,
        &app->d3dver);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "win-app init failed: init DXGI failed");
        app_release();
        return RET_FAIL;
    }
    app->d3d_dbg = BIT_CHECK(params->gfx.flags, GFX_FLAG_DEBUG);
    app->pinst = myinst;
    app->width = width;
    app->height = height;
    app->always_active = TRUE;
    app->refresh_rate = params->gfx.refresh_rate;

    /* create default swapchain */
    r = app_add_swapchain(app->name, app->hwnd, width, height, params->gfx.refresh_rate,
        BIT_CHECK(params->gfx.flags, GFX_FLAG_FULLSCREEN),
        BIT_CHECK(params->gfx.flags, GFX_FLAG_VSYNC));
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "win-app init failed: add swapchain failed");
        app_release();
        return RET_FAIL;
    }

    /* set as default render-target */
    app_set_rendertarget(app->name);
    app->init = TRUE;

    return RET_OK;
}

void calc_screenpos(uint width, uint height, uint* px, uint* py)
{
    /* Returns the position where window will be at the center of the screen */
    HWND desktop_wnd = GetDesktopWindow();
    if (desktop_wnd != NULL && IsWindow(desktop_wnd))        {
        RECT rc;
        GetWindowRect(desktop_wnd, &rc);
        LONG bg_width = rc.right - rc.left;
        LONG bg_height = rc.bottom - rc.top;

        *px = rc.left + (bg_width/2) - (width/2);
        *py = rc.top + (bg_height/2) - (height/2);
    }
}

void app_release()
{
    if (g_app == NULL)
        return;

    struct app_win* wapp = g_app;

    /* remove swap chains */
    struct linked_list* lnode = wapp->swapchains;
    while (lnode != NULL)   {
        struct linked_list* next = lnode->next;
        app_remove_swapchain(((struct app_swapchain*)lnode->data)->name);
        lnode = next;
    }

    /* release main dx-sdk objects */
    app_release_dxgi();

    if (!wapp->wnd_override)    {
        if (wapp->hwnd != NULL && IsWindow(wapp->hwnd))
            DestroyWindow(wapp->hwnd);
        UnregisterClass(wapp->name, wapp->pinst);
    }

    FREE(wapp);
    g_app = NULL;
}

void app_update()
{
    ASSERT(g_app);
    struct app_win* wapp = g_app;
    if (wapp->wnd_override)
        return;

    MSG msg;
    bool_t quit = FALSE;
    bool_t have_msg = FALSE;

    memset(&msg, 0x00, sizeof(MSG));

    while (!quit)        {
        /* In Active mode, Peek Incomming Messages
         * In Non-Active mode, Wait/Block for Incoming Messages */
        if (wapp->active || wapp->always_active)        {
            have_msg = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
            quit = (msg.message == WM_QUIT);
        }    else    {
            quit = !GetMessage(&msg, NULL, 0, 0);
            have_msg = TRUE;
        }

        /* If we have a Message, Process It */
        if (have_msg)        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            have_msg = FALSE;
        }

        /* run Program Loop */
        if (wapp->update_fn != NULL)
            wapp->update_fn();
    }
}

void app_readjust(uint client_width, uint client_height)
{
    ASSERT(g_app);

    struct app_win* app = g_app;

    if (app->wnd_override)
        return;

    RECT wnd_rect = {0, 0,
        (client_width!=0) ? client_width : app->width,
        (client_height!=0) ? client_height : app->height};
    uint xpos, ypos;

    AdjustWindowRect(&wnd_rect, WS_OVERLAPPEDWINDOW, FALSE);
    calc_screenpos(wnd_rect.right-wnd_rect.left, wnd_rect.bottom-wnd_rect.top, &xpos, &ypos);
    SetWindowPos(((struct app_win*)app)->hwnd, NULL, xpos, ypos,
        wnd_rect.right - wnd_rect.left, wnd_rect.bottom - wnd_rect.top, 0);

    app->width = client_width;
    app->height = client_height;
}


void app_set_alwaysactive(bool_t active)
{
    g_app->always_active = active;
}


uint app_get_wndwidth()
{
    return g_app->width;
}

uint app_get_wndheight()
{
    return g_app->height;
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

result_t app_init_dxgi(const struct init_params* params,
    OUT IDXGIFactory** pfactory,
    OUT IDXGIAdapter** padapter,
    OUT ID3D11Device** pdev,
    OUT ID3D11DeviceContext** pcontext,
    OUT enum gfx_hwver* pver)
{
    HRESULT dxhr;
    uint dev_flags = 0;
    D3D_FEATURE_LEVEL levels[3];
    uint levels_cnt;
    D3D_FEATURE_LEVEL ft_level;
    IDXGIFactory* factory;
    IDXGIAdapter* adapter;
    ID3D11Device* dev;
    ID3D11DeviceContext* ctx = NULL;
    enum gfx_hwver gfxver;

    log_print(LOG_INFO, "  init DXGI (D3D11) ...");

    dxhr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
    if (FAILED(dxhr))   {
        err_print(__FILE__, __LINE__, "gfx-device init failed: could not create dx factory");
        return RET_FAIL;
    }

    /* gfx dxgi_adapter */
    dxhr = factory->EnumAdapters(params->gfx.adapter_id, &adapter);
    if (dxhr == DXGI_ERROR_NOT_FOUND)   {
        factory->Release();
        err_print(__FILE__, __LINE__, "gfx-device init failed: specified dxgi_adapter-id not found");
        return RET_FAIL;
    }

    /* Flags */
    if (BIT_CHECK(params->gfx.flags, GFX_FLAG_DEBUG))
        BIT_ADD(dev_flags, D3D11_CREATE_DEVICE_DEBUG);

    /* Feature Levels */
    if (params->gfx.hwver == GFX_HWVER_UNKNOWN)   {
        levels[0] = D3D_FEATURE_LEVEL_11_0;
        levels[1] = D3D_FEATURE_LEVEL_10_1;
        levels[2] = D3D_FEATURE_LEVEL_10_0;
        levels_cnt = 3;
    }   else    {
        switch (params->gfx.hwver)   {
        case GFX_HWVER_D3D10_0:
            levels[0] = D3D_FEATURE_LEVEL_10_0;
            break;
        case GFX_HWVER_D3D10_1:
            levels[0] = D3D_FEATURE_LEVEL_10_1;
            break;
        case GFX_HWVER_D3D11_0:
        case GFX_HWVER_D3D11_1:
            levels[0] = D3D_FEATURE_LEVEL_11_0;
            break;
        }
        levels_cnt = 1;
    }

    /* create device */
    dxhr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, dev_flags,
        levels, levels_cnt, D3D11_SDK_VERSION, &dev, &ft_level, &ctx);
    if (FAILED(dxhr))   {
        err_print(__FILE__, __LINE__, "app init failed: could not create d3d device");
        return RET_FAIL;
    }

    switch (ft_level)   {
    case D3D_FEATURE_LEVEL_10_0:    gfxver = GFX_HWVER_D3D10_0;    break;
    case D3D_FEATURE_LEVEL_10_1:    gfxver = GFX_HWVER_D3D10_1;    break;
    case D3D_FEATURE_LEVEL_11_0:    gfxver = GFX_HWVER_D3D11_0;    break;
    //case D3D_FEATURE_LEVEL_11_1:    gfxver = GFX_HWVER_D3D11_1;    break;
    default:
        gfxver = GFX_HWVER_UNKNOWN;
        break;
    }

    /* elevate the API to d3d11.1 */
    /* fetch context (currently not working because I don't have win8 dev system) */
    /*
    ID3D11Device* dev1;
    dxhr = dev->QueryInterface(__uuidof(ID3D11Device), (void**)&dev1);
    if (FAILED(dxhr))   {
        err_print(__FILE__, __LINE__, "app init failed: could not create d3d11.1 device");
        return RET_FAIL;
    }
    dev->Release();

    ID3D11DeviceContext* ctx1;
    dev1->GetImmediateContext1(&ctx1);
    ctx->Release();
    */

    /* assign output values */
    *pfactory = factory;
    *padapter = adapter;
    *pdev = dev;
    *pcontext = ctx;
    *pver = gfxver;

    return RET_OK;
}

void app_release_dxgi()
{
    ASSERT(g_app);

    struct app_win* app = g_app;

    RELEASE(app->main_ctx);
    RELEASE(app->dev);
    RELEASE(app->dxgi_adapter);
    RELEASE(app->dxgi_factory);
}

result_t app_add_rendertarget(const char* wnd_name, wnd_t wnd, uint width, uint height)
{
    return app_add_swapchain(wnd_name, (HWND)wnd, width, height, g_app->refresh_rate, FALSE, FALSE);
}

result_t app_add_swapchain(const char* name, HWND hwnd, uint width, uint height,
    uint refresh_rate, bool_t fullscreen, bool_t vsync)
{
    ASSERT(g_app);

    HRESULT dxhr;
    IDXGISwapChain* swapchain;
    ID3D11Texture2D* backbuffer;
    ID3D11RenderTargetView* rtv;
    ID3D11Texture2D* depthstencil = NULL;
    ID3D11DepthStencilView* dsv = NULL;
    struct app_win* app = g_app;

    /* search in the current swapchain list */
    struct linked_list* lnode = app->swapchains;
    while (lnode)   {
        struct app_swapchain* sc = (struct app_swapchain*)lnode->data;
        if (str_isequal(name, sc->name))   {
            err_print(__FILE__, __LINE__, "swapchain creation failed: already exists");
            return RET_FAIL;
        }

        lnode = lnode->next;
    }

    DXGI_SWAP_CHAIN_DESC d3d_desc;
    memset(&d3d_desc, 0x00, sizeof(d3d_desc));

    if (width == 0 || height == 0)  {
        RECT wrc;
        ::GetClientRect(hwnd, &wrc);
        width = wrc.right - wrc.left;
        height = wrc.bottom - wrc.top;
    }

    d3d_desc.BufferDesc.Width = width;
    d3d_desc.BufferDesc.Height = height;
    d3d_desc.BufferDesc.RefreshRate.Numerator = refresh_rate;
    d3d_desc.BufferDesc.RefreshRate.Denominator = 1;
    d3d_desc.BufferDesc.Format = DEFAULT_DISPLAY_FORMAT;
    d3d_desc.SampleDesc.Count = 1;
    d3d_desc.SampleDesc.Quality = 0;
    d3d_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d3d_desc.BufferCount = BACKBUFFER_COUNT;
    d3d_desc.Windowed = fullscreen ? FALSE : TRUE;
    d3d_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    d3d_desc.Flags = fullscreen ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;
    d3d_desc.OutputWindow = hwnd;

    dxhr = app->dxgi_factory->CreateSwapChain(app->dev, &d3d_desc, &swapchain);
    if (FAILED(dxhr))   {
        err_print(__FILE__, __LINE__, "swapchain creation failed");
        return NULL;
    }

    /* get BackBuffer and Create RenderTargetView from it */
    if (FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer)))    {
        swapchain->Release();
        return RET_FAIL;
    }

    if (FAILED(app->dev->CreateRenderTargetView(backbuffer, NULL, &rtv)))  {
        backbuffer->Release();
        swapchain->Release();
        return RET_FAIL;
    }

    /* depth/stencil */
    D3D11_TEXTURE2D_DESC ds_desc;
    memset(&ds_desc, 0x00, sizeof(ds_desc));
    ds_desc.ArraySize = 1;
    ds_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ds_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    ds_desc.MipLevels = 1;
    ds_desc.SampleDesc.Count = 1;
    ds_desc.Width = width;
    ds_desc.Height = height;

    if (FAILED(app->dev->CreateTexture2D(&ds_desc, NULL, &depthstencil)))  {
        backbuffer->Release();
        swapchain->Release();
        return RET_FAIL;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC ds_view;
    memset(&ds_view, 0x00, sizeof(ds_view));
    ds_view.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ds_view.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(app->dev->CreateDepthStencilView(depthstencil, &ds_view, &dsv)))    {
        depthstencil->Release();
        backbuffer->Release();
        swapchain->Release();
        return RET_FAIL;
    }


    struct app_swapchain* sc = (struct app_swapchain*)ALLOC(sizeof(struct app_swapchain), 0);
    ASSERT(sc);
    memset(sc, 0x00, sizeof(struct app_swapchain));
    str_safecpy(sc->name, sizeof(sc->name), name);
    sc->hwnd = hwnd;

    sc->sc = swapchain;
    sc->backbuff = backbuffer;
    sc->depthbuff = depthstencil;
    sc->dsv = dsv;
    sc->rtv = rtv;
    sc->width = width;
    sc->height = height;
    sc->vsync = vsync;
    sc->fullscreen = fullscreen;
    sc->refresh_rate = refresh_rate;

    list_add(&app->swapchains, &sc->lnode, sc);
    return RET_OK;
}

void app_remove_swapchain(const char* name)
{
    ASSERT(g_app);
    struct app_win* app = g_app;

    /* find swapchain in the list */
    struct linked_list* lnode = app->swapchains;
    while (lnode)   {
        struct app_swapchain* sc = (struct app_swapchain*)lnode->data;
        if (str_isequal(name, sc->name))   {
            if (sc->fullscreen)
                sc->sc->SetFullscreenState(FALSE, NULL);

            RELEASE(sc->rtv);
            RELEASE(sc->dsv);
            RELEASE(sc->backbuff);
            RELEASE(sc->depthbuff);
            RELEASE(sc->sc);

            list_remove(&app->swapchains, lnode);
            FREE(sc);
            break;
        }

        lnode = lnode->next;
    }

}

result_t app_resize_window(OPTIONAL const char* name, uint width, uint height)
{
    ASSERT(g_app);
    struct app_win* app = g_app;

    if (!app->init)
        return RET_FAIL;

    /* search for the specified swapchain and resize it */
    struct app_swapchain* rsc = (name != NULL) ? app_find_swapchain(name) : app->render_target;
    if (rsc == NULL)
        return RET_FAIL;

    /* re-create */
    RELEASE(rsc->dsv);
    RELEASE(rsc->depthbuff);
    RELEASE(rsc->rtv);
    RELEASE(rsc->backbuff);

    rsc->sc->ResizeBuffers(BACKBUFFER_COUNT, width, height, DEFAULT_DISPLAY_FORMAT, 0);

    /* get BackBuffer and Create RenderTargetView from it */
    if (FAILED(rsc->sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&rsc->backbuff)))  {
        log_print(LOG_WARNING, "resizing swapchain failed");
        return RET_FAIL;
    }

    if (FAILED(app->dev->CreateRenderTargetView(rsc->backbuff, NULL, &rsc->rtv)))  {
        log_print(LOG_WARNING, "resizing swapchain failed");
        return RET_FAIL;
    }

    /* depthstencil */
    D3D11_TEXTURE2D_DESC ds_desc;
    memset(&ds_desc, 0x00, sizeof(ds_desc));
    ds_desc.ArraySize = 1;
    ds_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ds_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    ds_desc.MipLevels = 1;
    ds_desc.SampleDesc.Count = 1;
    ds_desc.Width = width;
    ds_desc.Height = height;

    if (FAILED(app->dev->CreateTexture2D(&ds_desc, NULL, &rsc->depthbuff)))  {
        log_print(LOG_WARNING, "resizing swapchain failed");
        return RET_FAIL;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC ds_view;
    memset(&ds_view, 0x00, sizeof(ds_view));
    ds_view.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ds_view.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(app->dev->CreateDepthStencilView(rsc->depthbuff, &ds_view, &rsc->dsv)))    {
        log_print(LOG_WARNING, "resizing swapchain failed");
        return RET_FAIL;
    }

    rsc->width = width;
    rsc->height = height;

    /* for main window (this is where main render buffers are created), resize internal buffers */
    if (app->width != width || app->height != height)   {
        gfx_resize(width, height);
        app->width = width;
        app->height = height;
    }

    /* if the current (render target) swapchain is resized, update variables */
    if (rsc == app->render_target)
        gfx_set_rtvsize(width, height);

    return RET_OK;
}

const char* app_get_wndname(HWND hwnd)
{
    struct app_win* app = g_app;
    struct linked_list* lnode = app->swapchains;
    while (lnode)   {
        struct app_swapchain* sc = (struct app_swapchain*)lnode->data;
        if (sc->hwnd == hwnd)
            return sc->name;

        lnode = lnode->next;
    }
    return "";
}

struct app_swapchain* app_find_swapchain(const char* wnd_name)
{
    struct app_win* app = g_app;
    struct linked_list* lnode = app->swapchains;
    while (lnode)   {
        struct app_swapchain* sc = (struct app_swapchain*)lnode->data;
        if (str_isequal(wnd_name, sc->name))
            return sc;

        lnode = lnode->next;
    }
    return NULL;
}

void app_set_rendertarget(OPTIONAL const char* wnd_name)
{
    ASSERT(g_app);
    struct app_win* app = g_app;

    struct app_swapchain* sc;
    if (wnd_name == NULL)
        sc = app->render_target;
    else
        sc = app_find_swapchain(wnd_name);

    if (sc != NULL) {
        app->render_target = sc;
        app->main_ctx->OMSetRenderTargets(1, &sc->rtv, sc->dsv);

        /* reset rendering variables that depend on size */
        gfx_set_rtvsize(sc->width, sc->height);
    }
}

void app_swapbuffers()
{
    ASSERT(g_app);
    struct app_win* app = g_app;
    ASSERT(app->render_target);

    app->render_target->sc->Present(app->render_target->vsync ? 1 : 0, 0);
}

const char* app_get_gfxdriverstr()
{
    static char info[256];

    struct app_win* app = g_app;

    DXGI_ADAPTER_DESC desc;
    HRESULT dxhr = app->dxgi_adapter->GetDesc(&desc);
    if (FAILED(dxhr))
        return NULL;

    char gpu_desc[128];
    str_widetomb(gpu_desc, desc.Description, sizeof(gpu_desc));

    sprintf(info, "%s r%d v%s-%s %s",
        gpu_desc,
        desc.Revision,
        app_get_hwverstr(app->d3dver),
        app->d3d_dbg ? "dbg" : "rls",
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
    struct app_win* app = g_app;

    memset(info, 0x00, sizeof(struct gfx_device_info));

    if (app->dxgi_adapter == NULL || app->dev == NULL)
        return;

    DXGI_ADAPTER_DESC desc;
    HRESULT dxhr = app->dxgi_adapter->GetDesc(&desc);
    if (FAILED(dxhr))
        return;

    /* basic info */
    str_widetomb(info->desc, desc.Description, sizeof(info->desc));
    switch (desc.VendorId)        {
    case 0x1002:        info->vendor = GFX_GPU_ATI;     break;
    case 0x10DE:        info->vendor = GFX_GPU_NVIDIA;  break;
    case 0x163C:        info->vendor = GFX_GPU_INTEL;   break;
    default:            info->vendor = GFX_GPU_UNKNOWN; break;
    }
    info->mem_avail = desc.DedicatedVideoMemory / 1024;

    /* threading */
    D3D11_FEATURE_DATA_THREADING d3d_thr;
    if (FAILED(app->dev->CheckFeatureSupport(D3D11_FEATURE_THREADING, &d3d_thr, sizeof(d3d_thr))))
        return;
    info->threading.concurrent_cmdlist = d3d_thr.DriverCommandLists;
    info->threading.concurrent_create = d3d_thr.DriverConcurrentCreates;
}

enum gfx_hwver app_get_gfxver()
{
    ASSERT(g_app);

    return g_app->d3dver;
}

void* app_get_mainctx()
{
    ASSERT(g_app);
    return (void*)g_app->main_ctx;
}

ID3D11Texture2D* app_d3d_getbackbuff()
{
    ASSERT(g_app);
    ASSERT(g_app->render_target);

    return g_app->render_target->backbuff;
}

ID3D11Texture2D* app_d3d_getdepthbuff()
{
    ASSERT(g_app);
    ASSERT(g_app->render_target);

    return g_app->render_target->depthbuff;
}

ID3D11Device* app_d3d_getdev()
{
    ASSERT(g_app);
    return g_app->dev;
}

void app_show_window(OPTIONAL const char* wnd_name)
{
    struct app_swapchain* sc = wnd_name != NULL ? app_find_swapchain(wnd_name) :
        g_app->render_target;
    if (sc != NULL && !g_app->wnd_override)
        ShowWindow(sc->hwnd, SW_SHOW);
}

void app_hide_window(OPTIONAL const char* wnd_name)
{
    struct app_swapchain* sc = wnd_name != NULL ? app_find_swapchain(wnd_name) :
        g_app->render_target;
    if (sc != NULL && !g_app->wnd_override)
        ShowWindow(sc->hwnd, SW_HIDE);
}

const char* app_get_name()
{
    return g_app->name;
}

bool_t app_isactive()
{
    return g_app->active;
}

_EXTERN_ void* app_get_mainwnd()
{
    return g_app->hwnd;
}

void app_clear_rendertarget(const float color[4], float depth, uint8 stencil, uint flags)
{
    ASSERT(g_app);
    struct app_win* app = g_app;
    struct app_swapchain* sc = app->render_target;
    ASSERT(sc);

    if (BIT_CHECK(flags, GFX_CLEAR_DEPTH) || BIT_CHECK(flags, GFX_CLEAR_STENCIL))
        app->main_ctx->ClearDepthStencilView(sc->dsv, flags, depth, stencil);

    if (BIT_CHECK(flags, GFX_CLEAR_COLOR))
        app->main_ctx->ClearRenderTargetView(sc->rtv, color);
}

char* app_query_displaymodes()
{
    IDXGIFactory* factory;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory)))
        return NULL;

    IDXGIAdapter* adapter;
    uint adapter_id = 0;
    DXGI_ADAPTER_DESC desc;
    char gpu_desc[128];
    size_t outsz;

    /* start json data (adapter array) */
    json_t jroot = json_create_arr();

    /* read adapters */
    while (factory->EnumAdapters(adapter_id, &adapter) != DXGI_ERROR_NOT_FOUND)  {
        adapter->GetDesc(&desc);
        str_widetomb(gpu_desc, desc.Description, sizeof(gpu_desc));

        json_t jadapter = json_create_obj();
        json_additem_toarr(jroot, jadapter);

        json_additem_toobj(jadapter, "name", json_create_str(gpu_desc));
        json_additem_toobj(jadapter, "id", json_create_num((fl64)adapter_id));

        /* enumerate monitors */
        json_t joutputs = json_create_arr();
        json_additem_toobj(jadapter, "outputs", joutputs);

        IDXGIOutput* output;
        uint output_id = 0;

        while (adapter->EnumOutputs(output_id, &output) != DXGI_ERROR_NOT_FOUND)    {
            json_t joutput = json_create_obj();
            json_additem_toarr(joutputs, joutput);

            json_additem_toobj(joutput, "id", json_create_num((fl64)output_id));

            /* enumerate modes */
            json_t jmodes = json_create_arr();
            json_additem_toobj(joutput, "monitors", jmodes);

            uint mode_cnt;
            HRESULT hr = output->GetDisplayModeList(DEFAULT_DISPLAY_FORMAT, 0, &mode_cnt, NULL);
            if (SUCCEEDED(hr))  {
                DXGI_MODE_DESC* modes = (DXGI_MODE_DESC*)ALLOC(sizeof(DXGI_MODE_DESC) * mode_cnt, 0);
                ASSERT(modes);
                output->GetDisplayModeList(DEFAULT_DISPLAY_FORMAT, 0, &mode_cnt, modes);
                for (uint i = 0; i < mode_cnt; i++)   {
                    if (modes[i].RefreshRate.Denominator != 1)
                        continue;

                    json_t jmode = json_create_obj();
                    json_additem_toobj(jmode, "width", json_create_num((fl64)modes[i].Width));
                    json_additem_toobj(jmode, "height", json_create_num((fl64)modes[i].Height));
                    json_additem_toobj(jmode, "refresh-rate",
                        json_create_num((fl64)modes[i].RefreshRate.Numerator));
                    json_additem_toarr(jmodes, jmode);
                }
                FREE(modes);
            }

            output_id ++;
        }

        adapter->Release();
        adapter_id ++;
    }

    factory->Release();

    char* r = json_savetobuffer(jroot, &outsz, FALSE);
    json_destroy(jroot);

    return r;
}

void app_free_displaymodes(char* dispmodes)
{
    ASSERT(dispmodes);
    json_deletebuffer(dispmodes);
}

void app_remove_rendertarget(const char* wnd_name)
{
    app_remove_swapchain(wnd_name);
}

#endif /* _WIN_ && _D3D_ */
