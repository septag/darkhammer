import sys, os, inspect
from abc import ABCMeta, abstractmethod
from ctypes import *

MY_DIR = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe())[0]))
HELPER_DIR = os.path.abspath(os.path.join(MY_DIR, '..', 'helpers'))
sys.path.append(HELPER_DIR)
import dhlog
from dhcore import *

class MSAAMode:
    NONE = 0
    X2 = 2
    X4 = 4
    X8 = 8

class TextureQuality:
    HIGH = 1
    NORMAL = 2
    LOW = 3
    HIGHEST = 0

class TextureFilter:
    TRILINEAR = 0
    BILINEAR = 1
    ANISO2X = 2
    ANISO4X = 3
    ANISO8X = 4
    ANISO16X = 5

class ShadingQuality:
    LOW = 2
    NORMAL = 1
    HIGH = 0

class GfxHwVer:
    UNKNOWN = 0
    D3D11_0 = 3
    D3D10_1 = 2
    D3D10_0 = 1
    D3D11_1 = 4
    GL4_4 = 12
    GL4_3 = 11
    GL4_2 = 10
    GL4_1 = 9
    GL4_0 = 8
    GL3_3 = 7
    GL3_2 = 6

class GfxFlags:
    FULLSCREEN = (1<<0)
    VSYNC = (1<<1)
    DEBUG = (1<<2)
    FXAA = (1<<3)
    REBUILDSHADERS = (1<<4)

class EngFlags:
    DEBUG = (1<<0)
    DEV = (1<<1)
    EDITOR = (1<<2)
    CONSOLE = (1<<3)
    DISABLEPHX = (1<<4)
    OPTIMIZEMEMORY = (1<<5)
    DISABLEBGLOAD = (1<<6)

class PhxFlags:
    TRACKMEM = (1<<0)
    PROFILE = (1<<1)

class InitParams(Structure):
    class DevParams(Structure):
        _fields_ = [\
            ('fpsgraph_max', c_int), 
            ('ftgraph_max', c_int),
            ('webserver_port', c_int),
            ('buffsize_data', c_uint),
            ('buffsize_tmp', c_uint)]

    class PhxParams(Structure):
        _fields_ = [\
            ('flags', c_uint),
            ('mem_sz', c_uint),
            ('substeps_max', c_uint),
            ('scratch_sz', c_uint)]

    class SctParams(Structure):
        _fields_ = [('mem_sz', c_uint)]

    class GfxParams(Structure):
        _fields_ = [\
            ('flags', c_uint),
            ('msaa_mode', c_uint),
            ('tex_quality', c_uint),
            ('tex_filter', c_uint),
            ('shading_quality', c_uint),
            ('hwver', c_uint),
            ('adapter_id', c_uint),
            ('width', c_uint),
            ('height', c_uint),
            ('refresh_rate', c_uint)]

    _fields_ = [\
        ('flags', c_uint),
        ('console_lines_max', c_uint),
        ('gfx', GfxParams),
        ('dev', DevParams),
        ('phx', PhxParams),
        ('sct', SctParams),
        ('console_cmds', c_char_p),
        ('console_cmds_cnt', c_uint),
        ('data_path', c_char_p)]

class _API:
    is_init = False

    @staticmethod
    def init(debug = False):
        if _API.is_init:
            return

        postfix = ''
        if debug:
            postfix = '-dbg'
        if sys.platform == 'win32':
            shlib = 'dhapp' + postfix + '.dll'
        elif sys.platform == 'linux':
            shlib = 'libdhapp' + postfix + '.so'

        # load library
        try:
            dhapplib = cdll.LoadLibrary(shlib)
        except:
            dhlog.Log.warn(str(sys.exc_info()[1]))
            dhlog.Log.fatal('could not load dynamic library %s' % shlib)
            sys.exit(-1)

        dhlog.Log.msgline('module "%s" loaded' % shlib, dhlog.TERM_GREEN)

        # app.h
        _API.app_config_default = dhapplib.app_config_default
        _API.app_config_default.restype = POINTER(InitParams)

        _API.app_config_load = dhapplib.app_config_load
        _API.app_config_load.restype = POINTER(InitParams)
        _API.app_config_load.argtypes = [c_char_p]

        _API.app_config_addconsolecmd = dhapplib.app_config_addconsolecmd
        _API.app_config_addconsolecmd.argtypes = [POINTER(InitParams), c_char_p]

        _API.app_config_unload = dhapplib.app_config_unload
        _API.app_config_unload.argtypes = [POINTER(InitParams)]

        _API.app_display_querymodes = dhapplib.app_display_querymodes
        _API.app_display_querymodes.restype = c_char_p

        _API.app_display_freemodes = dhapplib.app_display_freemodes
        _API.app_display_freemodes.argtypes = [c_char_p]

        _API.app_init = dhapplib.app_init
        _API.app_init.restype = c_int
        _API.app_init.argtypes = [c_char_p, POINTER(InitParams)]

        _API.app_release = dhapplib.app_release

        _API.app_window_alwaysactive = dhapplib.app_window_alwaysactive
        _API.argtypes = [c_int]

        _API.app_window_swapbuffers = dhapplib.app_window_swapbuffers

        _API.app_window_getheight = dhapplib.app_window_getheight
        _API.app_window_getheight.restype = c_uint

        _API.app_window_getwidth = dhapplib.app_window_getwidth
        _API.app_window_getwidth.restype = c_uint

        _API.app_window_resize = dhapplib.app_window_resize
        _API.restype = c_int
        _API.argtypes = [c_uint, c_uint]

        _API.app_window_show = dhapplib.app_window_show
        _API.app_window_hide = dhapplib.app_window_hide

        _API.fn_app_update = CFUNCTYPE(None)
        _API.app_window_setupdatefn = dhapplib.app_window_setupdatefn
        _API.app_window_setupdatefn.argtypes = [_API.fn_app_update]

        _API.fn_app_keypress = CFUNCTYPE(None, c_byte, c_uint)
        _API.app_window_setkeypressfn = dhapplib.app_window_setkeypressfn
        _API.app_window_setkeypressfn.argtypes = [_API.fn_app_keypress]

        _API.fn_app_resize = CFUNCTYPE(None, c_uint, c_uint)
        _API.app_window_setresizefn = dhapplib.app_window_setresizefn
        _API.app_window_setresizefn.argtypes = [_API.fn_app_resize]

        _API.app_window_run = dhapplib.app_window_run

        # input.h
        _API.input_update = dhapplib.input_update
        _API.input_kb_getkey = dhapplib.input_kb_getkey
        _API.input_kb_getkey.restype = c_uint
        _API.input_kb_getkey.argtypes = [c_uint, c_int]

        _API.input_mouse_getkey = dhapplib.input_mouse_getkey
        _API.input_mouse_getkey.restype = c_uint
        _API.input_mouse_getkey.argtypes = [c_uint, c_int]

        _API.input_mouse_getpos = dhapplib.input_mouse_getpos
        _API.input_mouse_getpos.restype = POINTER(Vec2i)
        _API.input_mouse_getpos.argtypes = [POINTER(Vec2i)]

        _API.input_mouse_smooth = dhapplib.input_mouse_smooth
        _API.input_mouse_smooth.argtypes = [POINTER(c_float), POINTER(c_float), c_float, c_float,
            c_float, c_float]

        _API.input_mouse_lockcursor = dhapplib.input_mouse_lockcursor
        _API.input_mouse_lockcursor.argtypes = [c_int, c_int]

        _API.input_mouse_unlockcursor = dhapplib.input_mouse_unlockcursor

class Config:
    def __init__(self, json_filepath=''):
        if json_filepath != '':
            self.params = _API.app_config_load(to_cstr(json_filepath))
            if not self.params:
                dhlog.Log.warn(Errors.last_error())
            self.params = _API.app_config_default()
        else:
            self.params = _API.app_config_default()

    def __del__(self):
        if self.params:
            _API.app_config_unload(self.params)
            del self.params

    def __set_datapath(self, path):
        self.params.contents.data_path = path.encode('ascii')
    def __get_datapath(self):
        return self.params.contents.data_path
    data_path = property(__get_datapath, __set_datapath)

    def __set_engineflags(self, flags):
        self.params.contents.flags = flags
    def __get_engineflags(self):
        return self.params.contents.flags
    engine_flags = property(__get_engineflags, __set_engineflags)

    def __set_gfxflags(self, flags):
        self.params.contents.gfx.flags = flags
    def __get_gfxflags(self):
        return self.params.contents.gfx.flags
    gfx_flags = property(__get_gfxflags, __set_gfxflags)

    def __set_gfxhwver(self, hwver):
        self.params.contents.gfx.hwver = hwver
    def __get_gfxhwver(self):
        return self.params.contents.gfx.hwver
    gfx_hwver = property(__get_gfxhwver, __set_gfxhwver)

    def __set_height(self, height):
        self.params.contents.gfx.height = height
    def __get_height(self):
        return self.params.contents.gfx.height
    height = property(__get_height, __set_height)

    def __set_width(self, width):
        self.params.contents.gfx.width = width
    def __get_width(self):
        return self.params.contents.gfx.width
    width = property(__get_width, __set_width)

    def __set_buffsizedata(self, buffsize):
        self.params.contents.dev.buffsize_data = buffsize
    def __get_buffsizedata(self):
        return self.params.contents.dev.buffsize_data
    buffsize_data = property(__get_buffsizedata, __set_buffsizedata)

    def __set_buffsizetmp(self, buffsize):
        self.params.contents.dev.buffsize_tmp = buffsize
    def __get_buffsizetmp(self):
        return self.params.contents.dev.buffsize_tmp
    buffsize_tmp = property(__get_buffsizetmp, __set_buffsizetmp)

    def __set_texturefilter(self, filter):
        self.params.contents.gfx.tex_filter = filter
    def __get_texturefilter(self, filter):
        return self.params.contents.gfx.tex_filter
    texture_filter = property(__get_texturefilter, __set_texturefilter)

    def __set_texturequality(self, quality):
        self.params.contents.tex_quality = quality
    def __get_texturequality(self, quality):
        return self.params.contents.tex_quality
    texture_quality = property(__get_texturequality, __set_texturequality)

    def __set_shadingquality(self, shquality):
        self.params.contents.shading_quality = shquality
    def __get_shadingquality(self, shquality):
        return self.params.contents.shading_quality
    shading_quality = property(__get_shadingquality, __set_shadingquality)

    def add_console_command(self, cmd):
        _API.app_config_addconsolecmd(self.params, to_cstr(cmd))

class Key:
    ESC = 0
    F1 = 1
    F2 = 2
    F3 = 3
    F4 = 4
    F5 = 5
    F6 = 6
    F7 = 7
    F8 = 8
    F9 = 9
    F10 = 10
    F11 = 11
    F12 = 12
    PRINTSCREEN = 13
    BREAK = 14
    TILDE = 15
    N1 = 16
    N2 = 17
    N3 = 18
    N4 = 19
    N5 = 20
    N6 = 21
    N7 = 22
    N8 = 23
    N9 = 24
    N0 = 25
    DASH = 26 
    EQUAL = 27
    BACKSPACE = 28
    TAB = 29
    Q = 30
    W = 31
    E = 32
    R = 33
    T = 34
    Y = 35
    U = 36
    I = 37
    O = 38
    P = 39
    BRACKET_OPEN = 40
    BRACKET_CLOSE = 41
    BACKSLASH = 42
    CAPS = 43
    A = 44
    S = 45
    D = 46
    F = 47
    G = 48
    H = 49
    J = 50
    K = 51
    L = 52
    SEMICOLON = 53
    QUOTE = 54
    ENTER = 55
    LSHIFT = 56
    Z = 57
    X = 58
    C = 59
    V = 60
    B = 61
    N = 62
    M = 63
    COMMA = 64
    DOT = 65
    SLASH = 66
    RSHIFT = 67
    LCTRL = 68
    LALT = 69
    SPACE = 70
    RALT = 71
    RCTRL = 72
    DELETE = 73
    INSERT = 74
    HOME = 75
    END = 76
    PGUP = 77
    PGDWN = 78
    UP = 79
    DOWN = 80
    LEFT = 81
    RIGHT = 82
    NUM_SLASH = 83
    NUM_MULTIPLY = 84
    NUM_MINUS = 85
    NUM_PLUS = 86
    NUM_ENTER = 87
    NUM_DOT = 88
    NUM_1 = 89
    NUM_2 = 90
    NUM_3 = 91
    NUM_4 = 92
    NUM_5 = 93
    NUM_6 = 94
    NUM_7 = 95
    NUM_8 = 96
    NUM_9 = 97
    NUM_0 = 98
    NUM_LOCK = 99 

class MouseKey:
    LEFT = 0
    RIGHT = 1
    MIDDLE = 2
    PGUP = 3
    PGDOWN = 4

class Input:
    __mpos = Vec2()

    @staticmethod
    def is_keydown(key, once = False):
        if not App.is_init:
            return
        return _API.input_kb_getkey(c_uint(key), c_int(once))

    @staticmethod
    def is_mousedown(key, once = False):
        if not App.is_init:
            return
        return _API.input_mouse_getkey(c_uint(key), c_int(once))

    @staticmethod
    def __update_mouse_pos(smooth = True, smoothness = 60, dt = 0):
        if not App.is_init:
            return
        pos = Vec2i()
        _API.input_mouse_getpos(byref(pos))

        if smooth:
            x = c_float(Input.__mpos.x)
            y = c_float(Input.__mpos.y)
            _API.input_mouse_smooth(byref(x), byref(y),
                c_float(pos.x), c_float(pos.y), c_float(smoothness), c_float(dt))
            Input.__mpos = Vec2(x.value, y.value)
        else:
            Input.__mpos.x = pos.x
            Input.__mpos.y = pos.y

    @staticmethod
    def lock_cursor():
        if not App.is_init:
            return
        _API.input_mouse_lockcursor(int(Input.__mpos.x), int(Input.__mpos.y))

    @staticmethod
    def unlock_cursor():
        if not App.is_init:
            return
        _API.input_mouse_unlockcursor()

    @staticmethod
    def get_mousepos():
        if not App.is_init:
            return
        return Input.__mpos

    @staticmethod
    def update(dt):
        if not App.is_init:
            return
        _API.input_update()
        Input.__update_mouse_pos(smooth=True, dt=dt)

class App:
    is_init = False

    @staticmethod
    def init(title, conf):
        r = _API.app_init(to_cstr(title), conf.params)
        if IS_FAIL(r):
            raise Exception(Errors.last_error())
        App.show_window()
        App.is_init = True

    @staticmethod
    def init_d3d_device(native_wnd_handle, name, conf):
        postfix = ''
        if ('--debug' in sys.argv):
            postfix = '-dbg'
        if sys.platform == 'win32':
            shlib = 'dhapp' + postfix + '.dll'
        else:
            raise Exception('d3d device not implemented for this platform')

        # load library
        try:
            hwnd = int(native_wnd_handle)
            dhapplib = cdll.LoadLibrary(shlib)
            dhlog.Log.msgline('module "%s" loaded' % shlib, dhlog.TERM_GREEN)
            # fetch only d3d_initdev function
            app_d3d_initdev = dhapplib.app_d3d_initdev
            app_d3d_initdev.restype = c_int
            app_d3d_initdev.argtypes = [c_void_p, c_char_p, POINTER(InitParams)]
            if IS_FAIL(app_d3d_initdev(c_void_p(hwnd), to_cstr(name), conf.params)):
                raise Exception(Errors.last_error())
        except:
            dhlog.Log.fatal(str(sys.exc_info()[1]))
            sys.exit(-1)

    @staticmethod
    def show_window():
        if App.is_init:
            _API.app_window_show()

    @staticmethod
    def hide_window():
        if App.is_init:
            _API.app_window_hide()

    @staticmethod
    def run():
        if App.is_init:
            _API.app_window_run()

    @staticmethod
    def release():
        _API.app_release()
        App.is_init = False

    @staticmethod
    def set_events(events):
        if App.is_init:
            _API.app_window_setupdatefn(events.get_update())
            _API.app_window_setkeypressfn(events.get_keypress())
            _API.app_window_setresizefn(events.get_resize())

    @staticmethod
    def swapbuffers():
        _API.app_window_swapbuffers()

    @staticmethod
    def resize_view(width, height):
         _API.app_window_resize(c_uint(width), c_uint(height))

class AppEvents:
    def get_update(self):
        def foo():
            self.on_update()
        AppEvents.pfn_update_callback = _API.fn_app_update(foo)
        return AppEvents.pfn_update_callback

    def get_resize(self):
        def foo(width, height):
            self.on_resize(width, height)
        AppEvents.pfn_resize_callback = _API.fn_app_resize(foo)
        return AppEvents.pfn_resize_callback

    def get_keypress(self):
        def foo(ch_code, vkey):
            self.on_keypress(ch_code, vkey)
        AppEvents.pfn_keypress_callback = _API.fn_app_keypress(foo)
        return AppEvents.pfn_keypress_callback

    def on_create(self):
        pass

    def on_destroy(self):
        pass

    def on_resize(self, width, height):
        _API.app_window_resize(c_uint(width), c_uint(height))

    def on_keypress(self, ch_code, vkey):
        pass

    def on_mousedown(self, x, y, mouse_key):
        pass

    def on_mouseup(self, x, y, mouse_key):
        pass

    def on_mousemove(self, x, y):
        pass

    def on_update(self):
        pass

_API.init(debug = ('--debug' in sys.argv))