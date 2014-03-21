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

#if defined(SWIG)
%module dheng

%{
#include "dhcore/core.h"
#include "dhcore/file-io.h"
#include "dhcore/hash.h"
#include "dhcore/vec-math.h"
#include "dhcore/std-math.h"
#include "engine.h"
#include "camera.h"
#include "scene-mgr.h"
#include "gfx.h"
#include "input.h"
#include "app.h"
#include "script.h"
#include "pybind/pyalloc.h"
#include "cmp-mgr.h"
#include "components/cmp-anim.h"
#include "world-mgr.h"
%}

%include "typemaps.i"

%import "../../../include/core/types.h"
#endif

/* core.h */
#define CORE_INIT_TRACEMEM (1<<0)
#define CORE_INIT_CRASHDUMP (1<<1)
#define CORE_INIT_LOGGER (1<<2)
#define CORE_INIT_ERRORS (1<<3)
#define CORE_INIT_JSON (1<<4)
#define CORE_INIT_FILEIO (1<<5)
#define CORE_INIT_TIMER (1<<6)
#define CORE_INIT_ALL 0xffffffff

result_t core_init(uint flags);
void core_release(bool_t report_leaks);

/* log.h */
enum log_type
{
    LOG_TEXT = 0,
    LOG_ERROR = 1,
    LOG_WARNING = 2,
    LOG_INFO = 3,
    LOG_LOAD = 4
};

result_t log_outputconsole(bool_t enable);
result_t log_outputfile(bool_t enable, const char* log_filepath);
result_t log_outputdebugger(bool_t enable);

/* error.h */
#define RET_OK 1
#define RET_FAIL 0

void err_print(const char* source, uint line, const char* text);
uint err_getcode();
void err_sendtolog(bool_t as_warning);
const char* err_getstring();
void err_clear();
bool_t err_haserrors();

/* std-math.h */
float math_torad(float deg);
float math_todeg(float rad);
bool_t math_isequal(float a, float b);

/* file-io.h */
void fio_addvdir(const char* directory, bool_t monitor);
void fio_clearvdirs();

/* hash.h */
uint hash_murmur32(const void* key, uint size_bytes, uint seed);

/* str.h */
void* str_toptr(const char* s);

/* vec-math.h */
struct vec2i
{
    int x;
    int y;
};

struct vec4f
{
    float x;
    float y;
    float z;
    float w;
};

struct vec4f* vec3_setf(struct vec4f* r, float x, float y, float z);

/* init-params.h */
enum msaa_mode
{
    MSAA_NONE = 0,
    MSAA_2X = 2,
    MSAA_4X = 4,
    MSAA_8X = 8
};

enum texture_quality
{
    TEXTURE_QUALITY_HIGH = 1,
    TEXTURE_QUALITY_NORMAL = 2,
    TEXTURE_QUALITY_LOW = 3,
    TEXTURE_QUALITY_HIGHEST = 0
};

enum texture_filter
{
    TEXTURE_FILTER_TRILINEAR = 0,
    TEXTURE_FILTER_BILINEAR,
    TEXTURE_FILTER_ANISO2X,
    TEXTURE_FILTER_ANISO4X,
    TEXTURE_FILTER_ANISO8X,
    TEXTURE_FILTER_ANISO16X
};

enum shading_quality
{
    SHADING_QUALITY_LOW = 2,
    SHADING_QUALITY_NORMAL = 1,
    SHADING_QUALITY_HIGH = 0
};

enum gfx_hwver
{
    GFX_HWVER_UNKNOWN = 0,
    GFX_HWVER_D3D11_0 = 3,
    GFX_HWVER_D3D10_1 = 2,
    GFX_HWVER_D3D10_0 = 1,
    GFX_HWVER_GL4_3 = 11,
    GFX_HWVER_GL4_2 = 10,
    GFX_HWVER_GL4_1 = 9,
    GFX_HWVER_GL4_0 = 8,
    GFX_HWVER_GL3_3 = 7,
    GFX_HWVER_GL3_2 = 6
};

enum gfx_flags
{
    GFX_FLAG_FULLSCREEN = (1<<0),
    GFX_FLAG_VSYNC = (1<<1),
    GFX_FLAG_DEBUG = (1<<2),
    GFX_FLAG_FXAA = (1<<3),
    GFX_FLAG_REBUILDSHADERS = (1<<4)
};

enum eng_flags
{
    ENG_FLAG_DEBUG = (1<<0),
    ENG_FLAG_DEV = (1<<1),
    ENG_FLAG_EDITOR = (1<<2),
    ENG_FLAG_CONSOLE = (1<<3),
    ENG_FLAG_DISABLEPHX = (1<<4),
    ENG_FLAG_DISABLEBGLOAD = (1<<6)
};

struct dev_params
{
    int fpsgraph_max;
    int ftgraph_max;
    int webserver_port;
    uint buffsize_data;
    uint buffsize_tmp;
};

struct gfx_params
{
    uint flags;
    enum msaa_mode msaa;
    enum texture_quality tex_quality;
    enum texture_filter tex_filter;
    enum shading_quality shading_quality;
    enum gfx_hwver hwver;
    uint adapter_id;
    uint width;
    uint height;
};

enum phx_flags
{
    PHX_FLAG_TRACKMEM = (1<<0),
    PHX_FLAG_PROFILE = (1<<1)
};

struct phx_params
{
    uint flags;
    uint mem_sz;
    uint substeps_max;
    uint scratch_sz;
};

struct sct_params
{
    uint mem_sz;
};

struct init_params
{
    uint flags;
    uint console_lines_max;

    struct gfx_params gfx;
    struct dev_params dev;
    struct phx_params phx;
    struct sct_params sct;

    const char* data_path;
};

/* app.h */
typedef void* wnd_t;

result_t app_init(const char* name, const struct init_params* params, wnd_t wnd_override);
void app_release();
struct init_params* app_load_config(const char* cfg_jsonfile);
struct init_params* app_defaultconfig();
char* app_query_displaymodes();
void app_free_displaymodes(char* dispmodes);
void app_swapbuffers();
void app_set_rendertarget(const char* wnd_name);
result_t app_resize_window(OPTIONAL const char* name, uint width, uint height);
result_t app_add_rendertarget(const char* wnd_name, wnd_t wnd, uint width, uint height);

/* engine.h */
result_t eng_init(const struct init_params* params);
void eng_release();
void eng_update();

/* camera.h */
struct camera
{
    struct vec4f look;
    struct vec4f pos;

    float fnear;
    float ffar;
    float fov;
    float aspect;
};

void cam_init(struct camera* cam, const struct vec4f* pos, const struct vec4f* lookat,
    float fnear, float ffar, float fov);
void cam_set_pitchconst(struct camera* cam, bool_t enable,
    float pitch_min, float pitch_max);
void cam_set_viewsize(struct camera* cam, float width, float height);
void cam_update(struct camera* cam);
void cam_pitch(struct camera* cam, float pitch);
void cam_yaw(struct camera* cam, float yaw);
void cam_roll(struct camera* cam, float roll);
void cam_fwd(struct camera* cam, float dz);
void cam_strafe(struct camera* cam, float dx);

/* cmp-mgr.h */
typedef uint64 cmphandle_t;
typedef uint16 cmptype_t;

enum cmp_obj_type
{
	CMP_OBJTYPE_UNKNOWN = 0,
	CMP_OBJTYPE_MODEL = (1<<0),
	CMP_OBJTYPE_PARTICLE = (1<<1),
	CMP_OBJTYPE_LIGHT = (1<<2),
	CMP_OBJTYPE_DECAL = (1<<3),
	CMP_OBJTYPE_CAMERA = (1<<4),
    CMP_OBJTYPE_TRIGGER = (1<<5)
};

struct cmp_obj
{
    char name[32];
    uint id;
    uint scene_id;
    enum cmp_obj_type type;
};

void cmp_destroy_instance(cmphandle_t hdl);
cmphandle_t cmp_create_instance_forobj(const char* cmpname, struct cmp_obj* obj);
cmphandle_t cmp_findinstance_inobj(struct cmp_obj* obj, const char* cmpname);
result_t cmp_value_sets(cmphandle_t hdl, const char* name, const char* value);
result_t cmp_value_setf(cmphandle_t hdl, const char* name, float value);
result_t cmp_value_seti(cmphandle_t hdl, const char* name, int value);
result_t cmp_value_setui(cmphandle_t hdl, const char* name, uint value);
result_t cmp_value_set4f(cmphandle_t hdl, const char* name, const float* value);
result_t cmp_value_set3f(cmphandle_t hdl, const char* name, const float* value);
result_t cmp_value_set2f(cmphandle_t hdl, const char* name, const float* value);
result_t cmp_value_setb(cmphandle_t hdl, const char* name, bool_t value);

/* scene-mgr.h */
uint scn_create_scene(const char* name);
void scn_destroy_scene(uint scene_id);
void scn_setactive(uint scene_id);
uint scn_getactive();
uint scn_findscene(const char* name);

struct cmp_obj* scn_create_obj(uint scene_id, const char* name, enum cmp_obj_type type);
void scn_destroy_obj(struct cmp_obj* obj);
uint scn_findobj(uint scene_id, const char* name);
struct cmp_obj* scn_getobj(uint scene_id, uint obj_id);

/* world-mgr.h */
void wld_set_cam(struct camera* cam);
struct camera* wld_get_cam();

/* gfx.h */
typedef void (*pfn_debug_render)(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);
void gfx_set_debug_renderfunc(pfn_debug_render fn);
%constant void gfx_render_grid(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);

/* input.h */
enum input_mouse_key
{
	INPUT_MOUSEKEY_LEFT = 0,
	INPUT_MOUSEKEY_RIGHT,
	INPUT_MOUSEKEY_MIDDLE,
	INPUT_MOUSEKEY_PGUP,
	INPUT_MOUSEKEY_PGDOWN
};

enum input_key	{
	INPUT_KEY_ESC = 0,
	INPUT_KEY_F1,
	INPUT_KEY_F2,
	INPUT_KEY_F3,
	INPUT_KEY_F4,
	INPUT_KEY_F5,
	INPUT_KEY_F6,
	INPUT_KEY_F7,
	INPUT_KEY_F8,
	INPUT_KEY_F9,
	INPUT_KEY_F10,
	INPUT_KEY_F11,
	INPUT_KEY_F12,
	INPUT_KEY_PRINTSCREEN,
	INPUT_KEY_BREAK,
	INPUT_KEY_TILDE,
	INPUT_KEY_1,
	INPUT_KEY_2,
	INPUT_KEY_3,
	INPUT_KEY_4,
	INPUT_KEY_5,
	INPUT_KEY_6,
	INPUT_KEY_7,
	INPUT_KEY_8,
	INPUT_KEY_9,
	INPUT_KEY_0,
	INPUT_KEY_DASH,
	INPUT_KEY_EQUAL,
	INPUT_KEY_BACKSPACE,
	INPUT_KEY_TAB,
	INPUT_KEY_Q,
	INPUT_KEY_W,
	INPUT_KEY_E,
	INPUT_KEY_R,
	INPUT_KEY_T,
	INPUT_KEY_Y,
	INPUT_KEY_U,
	INPUT_KEY_I,
	INPUT_KEY_O,
	INPUT_KEY_P,
	INPUT_KEY_BRACKET_OPEN,
	INPUT_KEY_BRACKET_CLOSE,
	INPUT_KEY_BACKSLASH,
	INPUT_KEY_CAPS,
	INPUT_KEY_A,
	INPUT_KEY_S,
	INPUT_KEY_D,
	INPUT_KEY_F,
	INPUT_KEY_G,
	INPUT_KEY_H,
	INPUT_KEY_J,
	INPUT_KEY_K,
	INPUT_KEY_L,
	INPUT_KEY_SEMICOLON,
	INPUT_KEY_QUOTE,
	INPUT_KEY_ENTER,
	INPUT_KEY_LSHIFT,
	INPUT_KEY_Z,
	INPUT_KEY_X,
	INPUT_KEY_C,
	INPUT_KEY_V,
	INPUT_KEY_B,
	INPUT_KEY_N,
	INPUT_KEY_M,
	INPUT_KEY_COMMA,
	INPUT_KEY_DOT,
	INPUT_KEY_SLASH,
	INPUT_KEY_RSHIFT,
	INPUT_KEY_LCTRL,
	INPUT_KEY_LALT,
	INPUT_KEY_SPACE,
	INPUT_KEY_RALT,
	INPUT_KEY_RCTRL,
	INPUT_KEY_DELETE,
	INPUT_KEY_INSERT,
	INPUT_KEY_HOME,
	INPUT_KEY_END,
	INPUT_KEY_PGUP,
	INPUT_KEY_PGDWN,
	INPUT_KEY_UP,
	INPUT_KEY_DOWN,
	INPUT_KEY_LEFT,
	INPUT_KEY_RIGHT,
	INPUT_KEY_NUM_SLASH,
	INPUT_KEY_NUM_MULTIPLY,
	INPUT_KEY_NUM_MINUS,
	INPUT_KEY_NUM_PLUS,
	INPUT_KEY_NUM_ENTER,
	INPUT_KEY_NUM_DOT,
	INPUT_KEY_NUM_1,
	INPUT_KEY_NUM_2,
	INPUT_KEY_NUM_3,
	INPUT_KEY_NUM_4,
	INPUT_KEY_NUM_5,
	INPUT_KEY_NUM_6,
	INPUT_KEY_NUM_7,
	INPUT_KEY_NUM_8,
	INPUT_KEY_NUM_9,
	INPUT_KEY_NUM_0,
	INPUT_KEY_NUM_LOCK,
	INPUT_KEY_CNT   /* count of input key enums */
};

struct vec2i* input_mouse_getpos(struct vec2i* pos);
bool_t input_mouse_getkey(enum input_mouse_key mkey, bool_t once);
bool_t input_kb_getkey(enum input_key key, bool_t once);
void input_update();

/* script.h */
result_t sct_runfile(const char* lua_filepath);

/* cmp-anim.h */
uint cmp_anim_getframecnt(cmphandle_t hdl);
void cmp_anim_play(cmphandle_t hdl);
void cmp_anim_stop(cmphandle_t hdl);
bool_t cmp_anim_isplaying(cmphandle_t hdl);
uint cmp_anim_getcurframe(cmphandle_t hdl);
uint cmp_anim_getfps(cmphandle_t hdl);
