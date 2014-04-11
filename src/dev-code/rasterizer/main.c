#include "GLEW/glew.h"

#include <stdio.h>
#include "dhcore/core.h"
#include "dhcore/color.h"
#include "dhcore/prims.h"
#include "dhcore/timer.h"

#if defined(_WIN_)
#include "dhcore/win.h"
#endif

#include "raster.h"

#define WIDTH 256
#define HEIGHT 256

#if defined(_WIN_)
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x0001
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif

/* globals */
HWND g_hwnd = NULL;
int g_active = FALSE;
HDC g_hdc = NULL;
HGLRC g_glc = NULL;
GLuint g_tex = 0;
GLuint g_prog = 0;
GLuint g_fsbuff[2] = {0, 0};
GLuint g_vao = 0;
void* g_buffer = NULL;

#define CAM_NEAR 0.2f
#define CAM_FAR 100.0f

/* fwd declare */
void main_loop();

static const char* g_vs_code =
    "#version 330\n"
    "in vec4 vsi_pos;"
    "in vec2 vsi_coord;"
    "out vec2 vso_coord;"
    "void main() {"
    "gl_Position = vsi_pos;"
    "vso_coord = vec2(vsi_coord.x, vsi_coord.y);"
    "}";
static const char* g_ps_code =
    "#version 330\n"
    "uniform sampler2D s_tex;"
    "uniform float c_near;"
    "uniform float c_far;"
    "in vec2 vso_coord;"
    "out vec4 pso_color;"
    "void main() {"
    "float depth_zbuff = texture(s_tex, vso_coord).x;"
    "float depth = (2.0f * c_near)/(c_far + c_near - depth_zbuff*(c_far-c_near));"
    "pso_color = vec4(depth, depth, depth, 1);"
    "}";
/*
static const char* g_ps_code =
    "#version 330\n"
    "uniform sampler2D s_tex;"
    "in vec2 vso_coord;"
    "out vec4 pso_color;"
    "void main() {"
    "float depth = texture(s_tex, vso_coord).x;"
    "pso_color = depth != 1.0f ? vec4(1.0f, 0.0f, 0.0f, 1.0f) : vec4(0, 0, 0, 1);"
    "}";
    */
/*
static const char* g_ps_code =
    "#version 330\n"
    "uniform sampler2D s_tex;"
    "in vec2 vso_coord;"
    "out vec4 pso_color;"
    "void main() {"
    "pso_color = texture(s_tex, vso_coord);"
    "}";
    */
/* */
static LRESULT CALLBACK msg_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)    {
    case WM_DESTROY:
        PostQuitMessage(0);        /* quit application loop */
        break;
    case WM_ACTIVATEAPP:
        g_active = wparam ? TRUE : FALSE;
        break;
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    return 0;
}


void APIENTRY gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei vs_len, const GLchar* message, GLvoid* userParam)
{
    printf(message);
    printf("\n");
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

result_t win_initapp(HINSTANCE hinst)
{
    ASSERT(hinst);

    /* register window class */
    WNDCLASSEX wndcls;
    memset(&wndcls, 0x00, sizeof(WNDCLASSEX));
    wndcls.cbSize =  sizeof(WNDCLASSEX);
    wndcls.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndcls.lpfnWndProc = msg_callback;
    wndcls.cbClsExtra = 0;
    wndcls.cbWndExtra = 0;
    wndcls.hInstance = hinst;
    wndcls.hIcon = NULL;
    wndcls.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndcls.hbrBackground = GetStockObject(WHITE_BRUSH);
    wndcls.lpszMenuName = NULL;
    wndcls.lpszClassName = "rasterizer";
    wndcls.hIconSm = NULL;
    if (!RegisterClassEx(&wndcls))  {
        err_print(__FILE__, __LINE__, "win-app init failed");
        return RET_FAIL;
    }

    /* create window */
    RECT wndrc = {0, 0, WIDTH, HEIGHT};
    uint x;
    uint y;

    AdjustWindowRect(&wndrc, WS_OVERLAPPEDWINDOW, FALSE);
    calc_screenpos(wndrc.right-wndrc.left, wndrc.bottom-wndrc.top, &x, &y);
    g_hwnd = CreateWindow("rasterizer",
        "rasterizer",
        WS_OVERLAPPEDWINDOW,
        x, y,
        wndrc.right - wndrc.left,
        wndrc.bottom - wndrc.top,
        NULL, NULL,
        hinst,
        NULL);
    if (g_hwnd == NULL)  {
        err_print(__FILE__, __LINE__, "win-app init failed: could not create window");
        return RET_FAIL;
    }
    ShowWindow(g_hwnd, SW_SHOW);
    return RET_OK;
}

void win_updateapp()
{
    MSG msg;
    int quit = FALSE;
    int have_msg = FALSE;

    memset(&msg, 0x00, sizeof(MSG));

    while (!quit)        {
        /* In Active mode, Peek Incomming Messages
         * In Non-Active mode, Wait for Incoming Messages */
        if (g_active)        {
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

        main_loop();
    }
}

void win_releaseapp()
{
    if (g_hwnd != NULL && IsWindow(g_hwnd))
        DestroyWindow(g_hwnd);
}

result_t init_gl_win(HINSTANCE hinst, HWND hwnd)
{
    ASSERT(IsWindow(hwnd));

    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0x00, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pfd_fmt = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pfd_fmt, &pfd);

    /* */
    HGLRC cur_glc = wglGetCurrentContext();
    if (cur_glc == NULL)    {
        cur_glc = wglCreateContext(hdc);
        if (cur_glc == NULL)    {
            ReleaseDC(hwnd, hdc);
            return RET_FAIL;
        }
        wglMakeCurrent(hdc, cur_glc);
    }

    /* get opengl version */
	struct version_info v;
	struct version_info max_v;

	/* get supported version from hardware */
    HMODULE libgl = LoadLibrary("opengl32.dll");
    PFNGLGETINTEGERVPROC getIntegerv = (PFNGLGETINTEGERVPROC)GetProcAddress(libgl, "glGetIntegerv");
    if (getIntegerv == NULL)
    	getIntegerv = (PFNGLGETINTEGERVPROC)wglGetProcAddress("glGetIntegerv");

	getIntegerv(GL_MAJOR_VERSION, &max_v.major);
	getIntegerv(GL_MINOR_VERSION, &max_v.minor);

    /* get CreateContextAttribsARB from opengl */
    typedef HGLRC (APIENTRY* pfn_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
    pfn_wglCreateContextAttribsARB wglCreateContextAttribsARB = (pfn_wglCreateContextAttribsARB)
        wglGetProcAddress("wglCreateContextAttribsARB");
    if (wglCreateContextAttribsARB == NULL) {
        wglCreateContextAttribsARB = (pfn_wglCreateContextAttribsARB)GetProcAddress(libgl,
            "wglCreateContextAttribsARB");
    }

    FreeLibrary(libgl);
    if (wglCreateContextAttribsARB == NULL) {
        log_print(LOG_WARNING, "could not get GL context (ARB), switching to traditional context.");
        g_glc = cur_glc;
        return RET_OK;
    }

    const int glc_atts[] = {
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
        WGL_CONTEXT_MAJOR_VERSION_ARB, max_v.major,
        WGL_CONTEXT_MINOR_VERSION_ARB, max_v.minor,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    g_glc = wglCreateContextAttribsARB(hdc, NULL, glc_atts);
    if (g_glc == NULL)    {
        log_print(LOG_WARNING, "could not get GL context (ARB), switching to traditional context.");
        g_glc = cur_glc;
    }	else	{
    	wglMakeCurrent(hdc, NULL);
    	wglDeleteContext(cur_glc);
    }

    wglMakeCurrent(hdc, g_glc);
    g_hdc = hdc;

    /* init functions */
    GLenum glew_ret;
    if ((glew_ret = glewInit()) != GLEW_OK)   {
        err_printf(__FILE__, __LINE__, "could not init glew: %s", glewGetErrorString(glew_ret));
        return RET_FAIL;
    }

    log_printf(LOG_INFO, "OpenGL init: %s, version: %s, GLSL %s",
        glGetString(GL_RENDERER), glGetString(GL_VERSION),
        glGetString(GL_SHADING_LANGUAGE_VERSION));
    glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE,
        0, NULL, GL_FALSE);
    glDebugMessageCallbackARB(gl_debug_callback, NULL);
    return RET_OK;
}

void release_gl_win(HDC hdc, HGLRC glc)
{
    if (glc != NULL)
        wglDeleteContext(glc);

    if (hdc != NULL)
        ReleaseDC(g_hwnd, hdc);
}

result_t init_prefabs()
{
    /* buffer */
    g_buffer = ALIGNED_ALLOC(WIDTH*HEIGHT*4, 0);
    if (g_buffer == NULL)
        return RET_OUTOFMEMORY;
    memset(g_buffer, 0x00, WIDTH*HEIGHT*4);

    /* gl texture */
    ASSERT(g_tex == 0);
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_buffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, WIDTH, HEIGHT, 0, GL_RED, GL_FLOAT, g_buffer);
    if (glGetError() != GL_NO_ERROR)
        return RET_FAIL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* shader for preview */
    GLint status;
    g_prog = glCreateProgram();
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLint vs_len[] = {strlen(g_vs_code)};
    glShaderSource(vs, 1, &g_vs_code, vs_len);
    glCompileShader(vs);

    GLint ps_len[] = {strlen(g_ps_code)};
    GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(ps, 1, &g_ps_code, ps_len);
    glCompileShader(ps);

    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, ps);

    glBindAttribLocation(g_prog, 0, "vsi_pos");
    glBindAttribLocation(g_prog, 1, "vsi_coord");

    glLinkProgram(g_prog);
    glGetProgramiv(g_prog, GL_LINK_STATUS, &status);
    glDeleteShader(vs);
    glDeleteShader(ps);
    if (status == GL_FALSE) {
        log_print(LOG_ERROR, "could not link program");
        return RET_FAIL;
    }

    /* fullscreen quad buffers */
    const struct vec3f pos[] = {
        {1.0f, 1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f, 1.0f},
        {-1.0f, -1.0f, 1.0f, 1.0f}
    };

    const struct vec2f coords[] = {
        {1.0f, 0.0f},
        {0.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };

    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(2, g_fsbuff);
    glBindBuffer(GL_ARRAY_BUFFER, g_fsbuff[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(struct vec3f)*4, pos, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(struct vec3f), NULL);

    glBindBuffer(GL_ARRAY_BUFFER, g_fsbuff[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(struct vec2f)*4, coords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vec2f), NULL);

    glBindVertexArray(0);

    return RET_OK;
}

void release_prefabs()
{
    if (g_fsbuff[0] != 0 || g_fsbuff[1] != 0)   {
        glDeleteBuffers(2, g_fsbuff);
    }

    if (g_prog != 0)    {
        glDeleteProgram(g_prog);
        g_prog = 0;
    }

    if (g_tex != 0) {
        glDeleteTextures(1, &g_tex);
        g_tex = 0;
    }

    if (g_buffer != NULL)
        ALIGNED_FREE(g_buffer);
}

int main()
{
    result_t r;
    r = core_init(TRUE);
    if (IS_FAIL(r)) {
        printf("core init failed\n");
        return -1;
    }

    log_outputconsole(TRUE);

    HINSTANCE hinst = GetModuleHandle(NULL);
    r = win_initapp(hinst);
    if (IS_FAIL(r)) {
        err_sendtolog(FALSE);
        return -1;
    }

    r = init_gl_win(hinst, g_hwnd);
    if (IS_FAIL(r)) {
        printf("init GL error\n");
        err_sendtolog(FALSE);
        return -1;
    }

    r = init_prefabs();
    if (IS_FAIL(r)) {
        printf("init prefabs failed\n");
        return -1;
    }

    win_updateapp();

    release_prefabs();
    release_gl_win(g_hdc, g_glc);
    win_releaseapp();
    core_release(TRUE);
    return 0;
}

void clear_buffer()
{
    float* buff = (float*)g_buffer;
    int size = WIDTH*HEIGHT;
    for (int i = 0; i < size; i++)
        buff[i] = 1.0f;
}

void update_texture()
{
    glBindTexture(GL_TEXTURE_2D, g_tex);
    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, g_buffer);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RED, GL_FLOAT, g_buffer);
    ASSERT(glGetError() == GL_NO_ERROR);
}


void draw_preview()
{
    glUseProgram(g_prog);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glUniform1i(glGetUniformLocation(g_prog, "s_tex"), 0);
    glUniform1f(glGetUniformLocation(g_prog, "c_near"), CAM_NEAR);
    glUniform1f(glGetUniformLocation(g_prog, "c_far"), CAM_FAR);

    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void draw_test()
{
    struct color c;
    color_setf(&c, 0.0f, 0.0f, 1.0f, 1.0f);
    uint cc = color_rgba_uint(color_swizzle_abgr(&c, &c));

    uint* buff = g_buffer;
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            buff[x + y*WIDTH] = cc;
        }
    }
}

void main_loop()
{
    static float rot = 0.0f;
    struct color tri_c;
    color_setf(&tri_c, 1.0f, 0.0f, 0.0f, 1.0f);

    struct vec3f v1;
    struct vec3f v2;
    struct vec3f v3;
    vec3_setf(&v1, 1.0f, 0.5f, 0.0f);
    vec3_setf(&v2, 0.4f, 0.65f, 0.3f);
    vec3_norm(&v1, &v1);
    vec3_norm(&v2, &v2);
    vec3_muls(&v3, vec3_add(&v3, &v1, &v2), 0.5f);
    float l = vec3_len(&v3);

    rs_setrendertarget(g_buffer, WIDTH, HEIGHT);
    rs_setviewport(0, 0, WIDTH, HEIGHT);
    clear_buffer();

    /* world */
    rot += 0.0005f;
    struct mat3f world;
    struct mat4f proj;
    struct mat3f view;

    mat3_setidentity(&world);
    mat3_set_roteuler(&world, 0.0f, rot, 0.0f);

    /* projection */
    float aspect = (float)WIDTH/(float)HEIGHT;
    float xscale = 1.0f / tan(math_torad(60.0f)*0.5f);
    float yscale = aspect * xscale;
    float zf = CAM_NEAR;
    float zn = CAM_FAR;
    mat4_setf(&proj,
        xscale,    0.0f,       0.0f,           0.0f,
        0.0f,      yscale,     0.0f,           0.0f,
        0.0f,      0.0f,       zf/(zf-zn),     1.0f,
        0.0f,      0.0f,       zn*zf/(zn-zf),  0.0f);

    /* view */
    struct vec3f right;
    struct vec3f up;
    struct vec3f look;
    struct vec3f pos;
    vec3_setf(&right, 1.0f, 0.0f, 0.0f);
    vec3_setf(&look, 0.0f, 0.0f, 1.0f);
    vec3_setf(&up, 0.0f, 1.0f, 0.0f);
    vec3_setf(&pos, 0.0f, 0.0f, -0.5f);
    mat3_setf(&view,
        right.x, up.x, look.x,
        right.y, up.y, look.y,
        right.z, up.z, look.z,
        -vec3_dot(&right, &pos),
        -vec3_dot(&up, &pos),
        -vec3_dot(&look, &pos));

    /* combine */
    struct mat4f viewproj;
    mat3_mul4(&viewproj, &view, &proj);

    /* triangle */
    const struct vec3f tri[] = {
        {0.0f, 1.0f, 0.0f, 1.0f},
        {-1.0f, -1.0f, 0.0f, 1.0f},
        {1.0f, -1.0f, 0.0f, 1.0f}
    };
    struct vec3f rtri[3];
    struct vec2i tri2d[3];

    rs_transform_verts(rtri, tri, 3, &world, &viewproj);
    for (int i = 0; i < 3; i++)
        vec2i_seti(&tri2d[i], (int)rtri[i].x, (int)rtri[i].y);

    uint64 start = timer_querytick();
    //rs_drawtri2d_2(&tri2d[0], &tri2d[1], &tri2d[2], color_rgba_uint(&tri_c));
    //rs_drawtri2d_3(&tri2d[0], &tri2d[1], &tri2d[2], color_rgba_uint(&tri_c));
    //float cnt = rs_testtri(&rtri[0], &rtri[1], &rtri[2]);
    //printf("%.1f pixels written: %.5f\n", cnt, timer_calctm(start, timer_querytick()));
    rs_drawtri(&rtri[0], &rtri[1], &rtri[2]);
    update_texture();

    draw_preview();

    if (g_hdc != NULL)
        SwapBuffers(g_hdc);
}
