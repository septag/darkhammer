import sys, os, inspect
from ctypes import *
import math

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

class _InitParams(Structure):
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
    def init():
        if _API.is_init:
            return

        if sys.platform == 'win32':
            shlib = 'dheng.dll'
        elif sys.platform == 'linux':
            shlib = 'libdheng.so'

        # load library
        try:
            dhenglib = cdll.LoadLibrary(shlib)
        except:
            Log.fatal('could not load dynamic library %s' % shlib)
            sys.exit(-1)

        # app.h
        _API.app_defaultconfig = dhenglib.app_defaultconfig
        _API.app_defaultconfig.restype = POINTER(_InitParams)

        _API.app_load_config = dhenglib.app_load_config
        _API.app_load_config.restype = POINTER(_InitParams)
        _API.app_load_config.argtypes = [c_char_p]

        _API.app_config_add_consolecmd = dhenglib.app_config_add_consolecmd
        _API.app_config_add_consolecmd.argtypes = [POINTER(_InitParams), c_char_p]

        _API.app_unload_config = dhenglib.app_unload_config
        _API.app_unload_config.argtypes = [POINTER(_InitParams)]

        _API.app_query_displaymodes = dhenglib.app_query_displaymodes
        _API.app_query_displaymodes.restype = c_char_p

        _API.app_free_displaymodes = dhenglib.app_free_displaymodes
        _API.app_free_displaymodes.argtypes = [c_char_p]

        _API.app_init = dhenglib.app_init
        _API.app_init.restype = c_int
        _API.app_init.argtypes = [c_char_p, POINTER(_InitParams), c_void_p]

        _API.app_release = dhenglib.app_release
        _API.app_update = dhenglib.app_update

        _API.app_set_alwaysactive = dhenglib.app_set_alwaysactive
        _API.argtypes = [c_uint]

        _API.app_swapbuffers = dhenglib.app_swapbuffers

        _API.app_get_wndheight = dhenglib.app_get_wndheight
        _API.app_get_wndheight.restype = c_uint

        _API.app_get_wndwidth = dhenglib.app_get_wndwidth
        _API.app_get_wndwidth.restype = c_uint

        _API.app_resize_window = dhenglib.app_resize_window
        _API.restype = c_int
        _API.argtypes = [c_char_p, c_uint, c_uint]

        _API.app_show_window = dhenglib.app_show_window
        _API.app_show_window.argtypes = [c_char_p]

        _API.app_hide_window = dhenglib.app_hide_window
        _API.app_hide_window.argtypes = [c_char_p]

        # engine.h
        _API.eng_init = dhenglib.eng_init
        _API.eng_init.restype = c_int
        _API.eng_init.argtpyes = [POINTER(_InitParams)]

        _API.eng_release = dhenglib.eng_release

        _API.is_init = True

class Config:
    def __init__(self, json_filepath=''):
        if json_filepath != '':
            self.params = _API.app_load_config(to_cstr(json_filepath))
            if not self.params:
                dhlog.Log.warn(Errors.last_error())
            self.params = _API.app_defaultconfig()
        else:
            self.params = _API.app_defaultconfig()

    def __del__(self):
        if self.params:
            _API.app_unload_config(self.params)
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

class Engine:
    @staticmethod
    def init(title, conf, window_hdl=None):
        r = _API.app_init(to_cstr(title), conf.params, c_void_p(window_hdl))
        if IS_FAIL(r):
            raise Exception('app_init failed')

        r = _API.eng_init(conf.params)
        if IS_FAIL(r):
            raise Exception('eng_init failed')

        _API.app_show_window(None)

    @staticmethod
    def release():
        _API.eng_release()
        _API.app_release()

    @staticmethod
    def run():
        _API.app_update()


_API.init()

