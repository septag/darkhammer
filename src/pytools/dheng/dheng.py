import sys, os, inspect
from abc import ABCMeta, abstractmethod
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
    def init(debug = False):
        if _API.is_init:
            return

        postfix = ''
        if debug:
            postfix = '-dbg'
        if sys.platform == 'win32':
            shlib = 'dheng' + postfix + '.dll'
        elif sys.platform == 'linux':
            shlib = 'libdheng' + postfix + '.so'

        # load library
        try:
            dhenglib = cdll.LoadLibrary(shlib)
        except:
            dhlog.Log.fatal('could not load dynamic library %s' % shlib)
            sys.exit(-1)

        dhlog.Log.msgline('module "%s" loaded' % shlib, dhlog.TERM_GREEN)

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

        _API.fn_app_update = CFUNCTYPE(None)
        _API.app_set_updatefunc = dhenglib.app_set_updatefunc
        _API.app_set_updatefunc.argtypes = [_API.fn_app_update]

        _API.fn_app_keypress = CFUNCTYPE(None, c_char_p, c_byte, c_uint)
        _API.app_set_keypressfunc = dhenglib.app_set_keypressfunc
        _API.app_set_keypressfunc.argtypes = [_API.fn_app_keypress]

        _API.fn_app_resize = CFUNCTYPE(None, c_char_p, c_uint, c_uint)
        _API.app_set_resizefunc = dhenglib.app_set_resizefunc
        _API.app_set_resizefunc.argtypes = [_API.fn_app_resize]

        _API.app_update = dhenglib.app_update

        # engine.h
        _API.eng_init = dhenglib.eng_init
        _API.eng_init.restype = c_int
        _API.eng_init.argtypes = [POINTER(_InitParams)]

        _API.eng_release = dhenglib.eng_release
        _API.eng_update = dhenglib.eng_update

        _API.eng_send_guimsgs = dhenglib.eng_send_guimsgs
        _API.eng_send_guimsgs.argtypes = [c_byte, c_uint]

        _API.eng_get_frametime = dhenglib.eng_get_frametime
        _API.eng_get_frametime.restype = c_float

        _API.eng_get_sharedir = dhenglib.eng_get_sharedir
        _API.eng_get_sharedir.restype = c_char_p

        # input.h
        _API.input_update = dhenglib.input_update
        _API.input_kb_getkey = dhenglib.input_kb_getkey
        _API.input_kb_getkey.restype = c_uint
        _API.input_kb_getkey.argtypes = [c_uint, c_uint]

        _API.input_kb_getkey = dhenglib.input_kb_getkey
        _API.input_kb_getkey.restype = c_uint
        _API.input_kb_getkey.argtypes = [c_uint, c_uint]

        _API.input_mouse_getkey = dhenglib.input_mouse_getkey
        _API.input_mouse_getkey.restype = c_uint
        _API.input_mouse_getkey.argtypes = [c_uint, c_uint]

        _API.input_mouse_getpos = dhenglib.input_mouse_getpos
        _API.input_mouse_getpos.restype = POINTER(Vec2i)
        _API.input_mouse_getpos.argtypes = [POINTER(Vec2i)]

        _API.input_mouse_smooth = dhenglib.input_mouse_smooth
        _API.input_mouse_smooth.argtypes = [POINTER(c_float), POINTER(c_float), c_float, c_float,
            c_float, c_float]

        _API.input_mouse_lockcursor = dhenglib.input_kb_unlockkey
        _API.input_mouse_lockcursor.argtypes = [c_int, c_int]

        _API.input_mouse_unlockcursor = dhenglib.input_mouse_unlockcursor

        # scene-mgr.h
        _API.scn_create_scene = dhenglib.scn_create_scene
        _API.scn_create_scene.restype = c_uint
        _API.scn_create_scene.argtypes = [c_char_p]

        _API.scn_destroy_scene = dhenglib.scn_destroy_scene
        _API.scn_destroy_scene.argtypes = [c_uint]

        _API.scn_findscene = dhenglib.scn_findscene
        _API.scn_findscene.restype = c_uint
        _API.scn_findscene.argtypes = [c_char_p]

        _API.scn_create_obj = dhenglib.scn_create_obj
        _API.scn_create_obj.restype = c_void_p
        _API.scn_create_obj.argtypes = [c_uint, c_char_p, c_uint]

        _API.scn_destroy_obj = dhenglib.scn_destroy_obj
        _API.scn_destroy_obj.argtypes = [c_void_p]

        _API.scn_findobj = dhenglib.scn_findobj
        _API.scn_findobj.restype = c_uint
        _API.scn_findobj.argtypes = [c_uint, c_char_p]

        _API.scn_getobj = dhenglib.scn_getobj
        _API.scn_getobj.restype = c_void_p
        _API.scn_getobj.argtypes = [c_uint, c_uint]

        _API.scn_clear = dhenglib.scn_clear
        _API.scn_clear.argtypes = [c_uint]

        _API.scn_setactive = dhenglib.scn_setactive
        _API.scn_setactive.argtypes = [c_uint]

        _API.scn_getactive = dhenglib.scn_getactive
        _API.scn_getactive.restype = c_uint

        _API.scn_setsize = dhenglib.scn_setsize
        _API.scn_setsize.argtypes = [c_uint, POINTER(Vec3), POINTER(Vec3)]

        _API.scn_getsize = dhenglib.scn_getsize
        _API.scn_getsize.argtypes = [c_uint, POINTER(Vec3), POINTER(Vec3)]

        # gfx.h
        _API.gfx_set_gridcallback = dhenglib.gfx_set_gridcallback
        _API.gfx_set_gridcallback.argtypes = [c_uint]

        # cmp-mgr.h
        _API.cmp_findtype = dhenglib.cmp_findtype
        _API.cmp_findtype.restype = c_void_p
        _API.cmp_findtype.argtypes = [c_ushort]

        _API.cmp_getname = dhenglib.cmp_getname
        _API.cmp_getname.restype = c_char_p
        _API.cmp_getname.argtypes = [c_void_p]

        _API.cmp_create_instance = dhenglib.cmp_create_instance
        _API.cmp_create_instance.restype = c_ulonglong
        _API.cmp_create_instance.argtypes = [c_void_p, c_void_p, c_uint, c_ulonglong, c_uint]

        _API.cmp_destroy_instance = dhenglib.cmp_destroy_instance
        _API.cmp_destroy_instance.argtypes = [c_ulonglong]

        _API.cmp_findinstance_bytype_inobj = dhenglib.cmp_findinstance_bytype_inobj
        _API.cmp_findinstance_bytype_inobj.restype = c_ulonglong
        _API.cmp_findinstance_bytype_inobj.argtypes = [c_void_p, c_ushort]

        _API.cmp_debug_add = dhenglib.cmp_debug_add
        _API.cmp_debug_add.argtypes = [c_ulonglong]

        _API.cmp_debug_remove = dhenglib.cmp_debug_remove
        _API.cmp_debug_remove.argtypes = [c_ulonglong]

        _API.cmp_value_set4f = dhenglib.cmp_value_set4f
        _API.cmp_value_set4f.restype = c_int
        _API.cmp_value_set4f.argtypes = [c_ulonglong, c_char_p, POINTER(c_float)]

        _API.cmp_value_get4f = dhenglib.cmp_value_get4f
        _API.cmp_value_get4f.restype = c_int
        _API.cmp_value_get4f.argtypes = [POINTER(c_float), c_ulonglong, c_char_p]

        _API.cmp_value_setf = dhenglib.cmp_value_setf
        _API.cmp_value_setf.restype = c_int
        _API.cmp_value_setf.argtypes = [c_ulonglong, c_char_p, c_float]

        _API.cmp_value_getf = dhenglib.cmp_value_getf
        _API.cmp_value_getf.restype = c_int
        _API.cmp_value_getf.argtypes = [POINTER(c_float), c_ulonglong, c_char_p]

        _API.cmp_value_setb = dhenglib.cmp_value_setb
        _API.cmp_value_setb.restype = c_int
        _API.cmp_value_setb.argtypes = [c_ulonglong, c_char_p, c_uint]

        _API.cmp_value_getb = dhenglib.cmp_value_getb
        _API.cmp_value_getb.restype = c_int
        _API.cmp_value_getb.argtypes = [POINTER(c_uint), c_ulonglong, c_char_p]

        _API.cmp_value_setui = dhenglib.cmp_value_setui
        _API.cmp_value_setui.restype = c_int
        _API.cmp_value_setui.argtypes = [c_ulonglong, c_char_p, c_uint]

        _API.cmp_value_getui = dhenglib.cmp_value_getui
        _API.cmp_value_getui.restype = c_int
        _API.cmp_value_getui.argtypes = [POINTER(c_uint), c_ulonglong, c_char_p]        

        _API.cmp_value_set3f = dhenglib.cmp_value_set3f
        _API.cmp_value_set3f.restype = c_int
        _API.cmp_value_set3f.argtypes = [c_ulonglong, c_char_p, POINTER(c_float)]

        _API.cmp_value_get3f = dhenglib.cmp_value_get3f
        _API.cmp_value_get3f.restype = c_int
        _API.cmp_value_get3f.argtypes = [POINTER(c_float), c_ulonglong, c_char_p]

        _API.cmp_value_set2f = dhenglib.cmp_value_set2f
        _API.cmp_value_set2f.restype = c_int
        _API.cmp_value_set2f.argtypes = [c_ulonglong, c_char_p, POINTER(c_float)]

        _API.cmp_value_get2f = dhenglib.cmp_value_get2f
        _API.cmp_value_get2f.restype = c_int
        _API.cmp_value_get2f.argtypes = [POINTER(c_float), c_ulonglong, c_char_p]

        _API.cmp_value_sets = dhenglib.cmp_value_sets
        _API.cmp_value_sets.restype = c_int
        _API.cmp_value_sets.argtypes = [c_ulonglong, c_char_p, c_char_p]

        _API.cmp_value_gets = dhenglib.cmp_value_gets
        _API.cmp_value_gets.restype = c_int
        _API.cmp_value_gets.argtypes = [c_char_p, c_uint, c_ulonglong, c_char_p]

        # cmp-xform.h
        _API.cmp_xform_setpos = dhenglib.cmp_xform_setpos
        _API.cmp_xform_setpos.argtypes = [c_void_p, POINTER(Vec3)]

        _API.cmp_xform_setrot_quat = dhenglib.cmp_xform_setrot_quat
        _API.cmp_xform_setrot_quat.argtypes = [c_void_p, POINTER(Quat)]

        _API.cmp_xform_getpos = dhenglib.cmp_xform_getpos
        _API.cmp_xform_getpos.restype = POINTER(Vec3)
        _API.cmp_xform_getpos.argtypes = [c_void_p, POINTER(Vec3)]

        _API.cmp_xform_getrot = dhenglib.cmp_xform_getrot
        _API.cmp_xform_getrot.restype = POINTER(Quat)
        _API.cmp_xform_getrot.argtypes = [c_void_p, POINTER(Quat)]

        # cmp-anim.h
        _API.cmp_anim_getclipname = dhenglib.cmp_anim_getclipname
        _API.cmp_anim_getclipname.restype = c_char_p
        _API.cmp_anim_getclipname.argtypes = [c_ulonglong, c_uint]

        _API.cmp_anim_isplaying = dhenglib.cmp_anim_isplaying
        _API.cmp_anim_isplaying.restype = c_uint
        _API.cmp_anim_isplaying.argtypes = [c_ulonglong]

        _API.cmp_anim_getclipcnt = dhenglib.cmp_anim_getclipcnt
        _API.cmp_anim_getclipcnt.restype = c_uint
        _API.cmp_anim_getclipcnt.argtypes = [c_ulonglong]

        _API.cmp_anim_getframecnt = dhenglib.cmp_anim_getframecnt
        _API.cmp_anim_getframecnt.restype = c_uint
        _API.cmp_anim_getframecnt.argtypes = [c_ulonglong]

        _API.cmp_anim_getfps = dhenglib.cmp_anim_getfps
        _API.cmp_anim_getfps.restype = c_uint
        _API.cmp_anim_getfps.argtypes = [c_ulonglong]

        _API.cmp_anim_getcurframe = dhenglib.cmp_anim_getcurframe
        _API.cmp_anim_getcurframe.restype = c_uint
        _API.cmp_anim_getcurframe.argtypes = [c_ulonglong]

        _API.cmp_anim_getbonecnt = dhenglib.cmp_anim_getbonecnt
        _API.cmp_anim_getbonecnt.restype = c_uint
        _API.cmp_anim_getbonecnt.argtypes = [c_ulonglong]

        _API.cmp_anim_getbonename = dhenglib.cmp_anim_getbonename
        _API.cmp_anim_getbonename.restype = c_char_p
        _API.cmp_anim_getbonename.argtypes = [c_ulonglong, c_uint]

        # cmp-animchar.h
        _API.cmp_animchar_getparamtype = dhenglib.cmp_animchar_getparamtype
        _API.cmp_animchar_getparamtype.restype = c_uint
        _API.cmp_animchar_getparamtype.argtypes = [c_ulonglong, c_char_p]

        _API.cmp_animchar_getparamb = dhenglib.cmp_animchar_getparamb
        _API.cmp_animchar_getparamb.restype = c_uint
        _API.cmp_animchar_getparamb.argtypes = [c_ulonglong, c_char_p]

        _API.cmp_animchar_getparami = dhenglib.cmp_animchar_getparami
        _API.cmp_animchar_getparami.restype = c_int
        _API.cmp_animchar_getparami.argtypes = [c_ulonglong, c_char_p]

        _API.cmp_animchar_getparamf = dhenglib.cmp_animchar_getparamf
        _API.cmp_animchar_getparamf.restype = c_float
        _API.cmp_animchar_getparamf.argtypes = [c_ulonglong, c_char_p]

        _API.cmp_animchar_setparamb = dhenglib.cmp_animchar_setparamb
        _API.cmp_animchar_setparamb.argtypes = [c_ulonglong, c_char_p, c_uint]

        _API.cmp_animchar_setparami = dhenglib.cmp_animchar_setparami
        _API.cmp_animchar_setparami.argtypes = [c_ulonglong, c_char_p, c_int]

        _API.cmp_animchar_setparamf = dhenglib.cmp_animchar_setparamf
        _API.cmp_animchar_setparamf.argtypes = [c_ulonglong, c_char_p, c_float]

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
    is_init = False
    __active_scene = None

    @staticmethod
    def keypress_callback(s, ch, vkey):
        _API.eng_send_guimsgs(ch, vkey)

    @staticmethod
    def update_callback():
        ft = _API.eng_get_frametime()
        Input.update(ft)

        if Engine.__active_scene != None:
            Engine.__active_scene.update_objects(ft)

        _API.eng_update()   

    @staticmethod
    def init(title, conf, window_hdl=None):
        r = _API.app_init(to_cstr(title), conf.params, c_void_p(window_hdl))
        if IS_FAIL(r):
            raise Exception(Errors.last_error())

        r = _API.eng_init(conf.params)
        if IS_FAIL(r):
            raise Exception(Errors.last_error())

        _API.app_show_window(None)

        # initialize and keep callbacks
        Engine.pfn_keypress_callback = _API.fn_app_keypress(Engine.keypress_callback)
        Engine.pfn_update_callback = _API.fn_app_update(Engine.update_callback)

        _API.app_set_updatefunc(Engine.pfn_update_callback)
        _API.app_set_keypressfunc(Engine.pfn_keypress_callback)
        _API.gfx_set_gridcallback(c_uint(True))

        # register components
        Component.register('transform', 0x7887, Transform) 
        Component.register('camera', 0x8b72, Camera)
        Component.register('bounds', 0x8bbd, Bounds)
        Component.register('model', 0x4e9b, Model)
        Component.register('animation', 0x068b, Animation)
        Component.register('animator', 0x99e4, Animator)
        Component.register('rigidbody', 0xbc2d, RigidBody)

        Engine.is_init = True

    @staticmethod
    def release():
        _API.eng_release()
        _API.app_release()
        Engine.is_init = False

    @staticmethod
    def run():
        _API.app_update()

    @staticmethod
    def set_active_scene(scene, caller_scene=False):
        if not caller_scene:
            scene.activate()
        else:
            Engine.__active_scene = scene

    @staticmethod
    def get_share_dir():
        return _API.eng_get_sharedir().decode()


class EngineCallbacks(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def draw_debug(self):
        pass

class GameObjectType:
    MODEL = (1<<0)
    PARTICLE = (1<<1)
    LIGHT = (1<<2)
    DECAL = (1<<3)
    CAMERA = (1<<4)
    TRIGGER = (1<<5)
    ENV = (1<<6)

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
    D1 = 16
    D2 = 17
    D3 = 18
    D4 = 19
    D5 = 20
    D6 = 21
    D7 = 22
    D8 = 23
    D9 = 24
    D0 = 25
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
        return _API.input_kb_getkey(c_uint(key), c_uint(once))

    @staticmethod
    def is_mousedown(key, once = False):
        return _API.input_mouse_getkey(c_uint(key), c_uint(once))

    @staticmethod
    def __update_mouse_pos(smooth = True, smoothness = 60, dt = 0):
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
        _API.input_mouse_lockcursor(int(Input.__mpos.x), int(Input.__mpos.y))

    @staticmethod
    def unlock_cursor():
        _API.input_mouse_unlockcursor()

    @staticmethod
    def get_mousepos():
        return Input.__mpos

    @staticmethod
    def update(dt):
        _API.input_update()
        Input.__update_mouse_pos(smooth=True, dt=dt)

class Component:
    __cmps = dict()

    def __init__(self, name, cmp_type, owner_obj):
        self._name = name
        self._type = cmp_type
        self._owner_obj = owner_obj

        c = _API.cmp_findtype(c_ushort(cmp_type))
        if c == None:
            raise Exception('specified component "%s" does not exist' % name)

        self._cmp = _API.cmp_findinstance_bytype_inobj(owner_obj.objptr, c_ushort(cmp_type))
        if self._cmp == INVALID_HANDLE:
            self._cmp = _API.cmp_create_instance(c, owner_obj.objptr, c_uint(0), 
                c_ulonglong(INVALID_HANDLE), c_uint(0))
            if self._cmp == INVALID_HANDLE:
                raise Exception('could not create component "%s"' % name)

    def __get_internalname(self):
        c = _API.cmp_findtype(c_ushort(self.type))
        if c != None:
            return _API.cmp_getname(c)
    internal_name = property(__get_internalname)

    def __get_name(self):
        return self._name
    name = property(__get_name)

    def __get_internaltype(self):
        return self._type
    internal_type = property(__get_internaltype)

    def __get_ownerobj(self):
        return self._owner_obj
    owner_obj = property(__get_ownerobj)

    def destroy(self):
        if self._cmp != INVALID_HANDLE:
            _API.cmp_destroy_instance(self._cmp)
            self_cmp = INVALID_HANDLE

    def debug(self, dbg):
        if dbg:        _API.cmp_debug_add(self._cmp)
        else:          _API.cmp_debug_remove(self._cmp)

    @staticmethod
    def register(name, cmp_type, cls_type):
        Component.__cmps[name] = (cmp_type, cls_type)

    @staticmethod
    def create(name, owner_obj):
        if name in Component.__cmps:
            citem = Component.__cmps[name]
            return citem[1](name, citem[0], owner_obj)
        else:
            raise Exception('component by name "%s" is not registered' % name)

class Transform(Component):
    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __set_position(self, pos):
        _API.cmp_xform_setpos(self._owner_obj.objptr, byref(pos))
    def __get_position(self):
        pos = Vec3()
        _API.cmp_xform_getpos(self._owner_obj.objptr, byref(pos))
        return pos
    position = property(__get_position, __set_position)

    def __set_rotation(self, quat):
        _API.cmp_xform_setrot_quat(self._owner_obj.objptr, byref(quat))
    def __get_rotation(self):
        quat = Quat()
        _API.cmp_xform_getrot(self._owner_obj.objptr, byref(quat))
        return quat
    rotation = property(__get_rotation, __set_rotation)   

class Bounds(Component):
    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __set_sphere(self, s):
        sf = c_float*4
        sf[0].value = s.x
        sf[1].value = s.y
        sf[2].value = s.z
        sf[3].value = s.w
        _API.cmp_value_set4f(self._cmp, to_cstr('sphere'), sf)
    def __get_sphere(self):
        sf = c_float*4
        _API.cmp_value_get4f(sf, self._cmp, to_cstr('sphere'))
        return Vec4(sf[0].value, sf[1].value, sf[2].value, sf[3].value)
    sphere = property(__get_sphere, __set_sphere)

class Camera(Component):
    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __get_fov(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('fov'))
        return f.value
    def __set_fov(self, fov):
        _API.cmp_value_setf(self._cmp, to_cstr('fov'), c_float(fov))
    fov = property(__get_fov, __set_fov)

    def __get_nearclip(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('near_distance'))
        return f.value
    def __set_nearclip(self, d):
        _API.cmp_value_setf(self._cmp, to_cstr('near_distance'), c_float(fov))
    near_clip = property(__get_nearclip, __set_nearclip)

    def __get_farclip(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('far_distance'))
        return f.value
    def __set_farclip(self, d):
        _API.cmp_value_setf(self._cmp, to_cstr('far_distance'), c_float(fov))
    far_clip = property(__get_farclip, __set_farclip)

    def __get_maxpitch(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('max_pitch'))
        return f.value
    def __set_maxpitch(self, pitch):
        _API.cmp_value_setf(self._cmp, to_cstr('max_pitch'), c_float(fov))
    max_pitch = property(__get_maxpitch, __set_maxpitch)

    def __get_minpitch(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('min_pitch'))
        return f.value
    def __set_minpitch(self, pitch):
        _API.cmp_value_setf(self._cmp, to_cstr('min_pitch'), c_float(fov))
    min_pitch = property(__get_minpitch, __set_minpitch)

    def __get_active(self):
        b = c_uint(0)
        _API.cmp_value_getb(byref(b), self._cmp, to_cstr('active'))
        return bool(b.value)
    def __set_active(self, value):
        _API.cmp_value_setb(self._cmp, to_cstr('active'), c_uint(value))
    active = property(__get_active, __set_active)

class Model(Component):
    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __get_filepath(self):
        s = create_string_buffer(128)
        _API.cmp_value_gets(s, c_uint(128), self._cmp, to_cstr('filepath'))
        return s.value.decode()
    def __set_filepath(self, fpath):
        r = _API.cmp_value_sets(self._cmp, to_cstr('filepath'), to_cstr(fpath))
        if IS_FAIL(r):
            raise Exception(Errors.last_error())
    filepath = property(__get_filepath, __set_filepath)

    def __get_excludeshadows(self):
        b = c_uint(0)
        _API.cmp_value_getb(byref(b), self._cmp, to_cstr('exclude_shadows'))
        return bool(b.value)
    def __set_excludeshadows(self, excl):
        _API.cmp_value_setb(self._cmp, to_cstr('exclude_shadows'), c_uint(excl))
    exclude_shadows = property(__get_excludeshadows, __set_excludeshadows)

class Animation(Component):
    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __get_filepath(self):
        s = create_string_buffer(128)
        _API.cmp_value_gets(s, c_uint(128), self._cmp, to_cstr('filepath'))
        return s.value.decode()
    def __set_filepath(self, fpath):
        r = _API.cmp_value_sets(self._cmp, to_cstr('filepath'), to_cstr(fpath))
        if IS_FAIL(r):
            raise Exception(Errors.last_error())
    filepath = property(__get_filepath, __set_filepath)

    def __get_playrate(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('play_rate'))
        return f.value
    def __set_playrate(self, rate):
        _API.cmp_value_setf(self._cmp, to_cstr('play_rate'), c_float(rate))
    play_rate = property(__get_playrate, __set_playrate)

    def __get_clipname(self):
        s = create_string_buffer(128)
        _API.cmp_value_gets(byref(s), self._cmp, to_cstr('clip_name'))
        return s.value.decode()
    def __set_clipname(self, clip_name):
        _API.cmp_value_sets(self._cmp, to_cstr('clip_name'), to_cstr(clip_name))
    clip_name = property(__get_clipname, __set_clipname)

    def __get_frame(self):
        return _API.cmp_anim_getcurframe(self._cmp)
    def __set_frame(self, value):
        _API.cmp_value_setui(self._cmp, to_cstr('frame_idx'), c_uint(value))
    frame = property(__get_frame, __set_frame)

    def __get_isplaying(self):
        return bool(_API.cmp_anim_isplaying(self._cmp))
    is_playing = property(__get_isplaying)

    def __get_clips(self):
        clip_cnt = _API.cmp_anim_getbonecnt(self._cmp)
        clips = []
        for i in range(0, clip_cnt):
            clips.append(_API.cmp_anim_getclipname(self._cmp, c_uint(i)).decode())
    clips = property(__get_clips)

    def __get_bones(self):
        bone_cnt = _API.cmp_anim_getbonecnt(self._cmp)
        bones = []
        for i in range(0, bone_cnt):
            bones.append(_API.cmp_anim_getbonename(self._cmp, c_uint(i)).decode())
    bones = property(__get_bones)

    def __get_fps(self):
        return _API.cmp_anim_getfps(self._cmp)
    fps = property(__get_fps)

    def __get_framecnt(self):
        return _API.cmp_anim_getframecnt(self._cmp)
    frame_count = property(__get_framecnt)

class Animator(Component):
    class ParamType:
        UNKNOWN = 0
        INT = 1
        FLOAT = 2
        BOOLEAN = 3

    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __get_filepath(self):
        s = create_string_buffer(128)
        _API.cmp_value_gets(s, c_uint(128), self._cmp, to_cstr('filepath'))
        return s.value.decode()
    def __set_filepath(self, fpath):
        r = _API.cmp_value_sets(self._cmp, to_cstr('filepath'), to_cstr(fpath))
        if IS_FAIL(r):
            raise Exception(Errors.last_error())
    filepath = property(__get_filepath, __set_filepath)

    def get_param(self, name):
        cname = to_cstr(name)
        t = _API.cmp_animchar_getparamtype(self._cmp, cname)
        if t == Animator.ParamType.UNKNOWN:
            raise Exception('unknown parameter "%s"' % name)
        if t == Animator.ParamType.INT:
            return _API.cmp_animchar_getparami(self._cmp, cname)
        elif t == Animator.ParamType.FLOAT:
            return _API.cmp_animchar_getparamf(self._cmp, cname)
        elif t == Animator.ParamType.BOOLEAN:
            return _API.cmp_animchar_getparamb(self._cmp, cname)

    def set_param(self, name, value):
        cname = to_cstr(name)
        t = _API.cmp_animchar_getparamtype(self._cmp, cname)
        if t == Animator.ParamType.UNKNOWN:
            raise Exception('unknown parameter "%s"' % name)
        if t == Animator.ParamType.INT:
            _API.cmp_animchar_setparami(self._cmp, cname, c_int(value))
        elif t == Animator.ParamType.FLOAT:
            return _API.cmp_animchar_setparamf(self._cmp, cname, c_float(value))
        elif t == Animator.ParamType.BOOLEAN:
            return _API.cmp_animchar_setparamb(self._cmp, cname, c_uint(value))

class RigidBody(Component):
    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __get_filepath(self):
        s = create_string_buffer(128)
        _API.cmp_value_gets(s, c_uint(128), self._cmp, to_cstr('filepath'))
        return s.value.decode()
    def __set_filepath(self, fpath):
        r = _API.cmp_value_sets(self._cmp, to_cstr('filepath'), to_cstr(fpath))
        if IS_FAIL(r):
            raise Exception(Errors.last_error())
    filepath = property(__get_filepath, __set_filepath)

    def __get_kinematic(self):
        b = c_uint()
        _API.cmp_value_getb(byref(b), self._cmp, to_cstr('kinematic'))
        return bool(b.value)
    def __set_kinematic(self, value):
        _API.cmp_value_setb(self._cmp, to_cstr('kinematic'), c_uint(value))
    kinematic = property(__get_kinematic, __set_kinematic)

    def __get_disablegravity(self):
        b = c_uint()
        _API.cmp_value_getb(byref(b), self._cmp, to_cstr('disablegravity'))
        return bool(b.value)
    def __set_disablegravity(self, value):
        _API.cmp_value_setb(self._cmp, to_cstr('disablegravity'), c_uint(value))
    disable_gravity = property(__get_disablegravity, __set_disablegravity)

class Behavior(metaclass=ABCMeta):
    @abstractmethod
    def init(self, game_obj):
        pass

    @abstractmethod
    def update(self, dt):
        pass

class OrbitCam(Behavior):
    def init(self, game_obj):
        self._obj = game_obj

        self._xform = game_obj.transform

        self.target = Vec3()
        self.sensivity = 0.2

        self._distance = 10
        self._x = 0
        self._y = 0
        self._lockpos = Vec2()
        self._leftbtn_dwn = False
        self._rightbtn_dwn = False

        return True

    def update(self, dt):
        if Input.is_mousedown(MouseKey.LEFT):
            mpos = Input.get_mousepos()

            if not self._leftbtn_dwn:
                self._leftbtn_dwn = True
                self._lockpos = mpos.copy()    

            delta_pos = (mpos - self._lockpos)*self.sensivity
            self._x += delta_pos.x
            self._y += delta_pos.y
            self._lockpos = mpos.copy()
        else:
            self._leftbtn_dwn = False

        if Input.is_mousedown(MouseKey.RIGHT):
            mpos = Input.get_mousepos()

            if not self._rightbtn_dwn:
                self._rightbtn_dwn = True
                self._lockpos = mpos.copy()    

            delta_pos = (mpos - self._lockpos)*self.sensivity
            self._distance += delta_pos.y
            self._lockpos = mpos.copy()
        else:
            self._rightbtn_dwn = False            

        q1 = Quat()
        q1.from_axis(Vec3(0, 1, 0), Math.to_rad(self._x))
        q2 = Quat()
        q2.from_axis(Vec3(1, 0, 0), Math.to_rad(self._y))
        q = q2*q1
        self._xform.rotation = q

        m = Matrix3()
        m.rotate_quat(q)
        self._xform.position = Vec3(0, 0, -self._distance)*m + self.target



class GameObject:
    def __init__(self, scene, obj_name, obj_type):
        self.__name = obj_name
        self.__cmps = dict()
        self.__behaviors = dict()
        self.__scene = scene
        self.__obj = _API.scn_create_obj(c_uint(scene.ID), to_cstr(obj_name), c_uint(obj_type))

        if self.__obj == None:
            raise Exception('creating object failed')

        self.__create_components(obj_type)

    def __create_components(self, obj_type):
        self.add_component('transform')
        self.add_component('bounds')

        if obj_type == GameObjectType.CAMERA:
            self.add_component('camera')
        elif obj_type == GameObjectType.MODEL:
            self.add_component('model')
        elif obj_type == GameObjectType.LIGHT:
            self.add_component('light')

    def destroy(self, scene_caller=False):
        if Engine.is_init and self.__obj != None:
            if scene_caller:
                _API.scn_destroy_obj(self.__obj)
                self.__obj = None
            elif self.__scene != None:
                self.__scene.destroy_object(self)

    def update_behaviors(self, dt):
        for b in self.__behaviors.values():
            b.update(dt)

    def add_component(self, name):
        if self.__obj == None:
            raise Exception('object is NULL')
        self.__cmps[name] = Component.create(name, self)

    def add_behavior(self, behavior, name):
        if behavior.init(self):
            self.__behaviors[name] = behavior

    def get_behavior(self, name):
        try:
            return self.__behaviors[name]
        except KeyError:
            raise

    def __getattr__(self, name):
        if self.__obj == None:
            raise Exception('object is NULL')
        try:
            return self.__cmps[name]
        except KeyError:
            raise AttributeError('component "%s" does not exist in GameObject "%s"' % (name, obj_name))

    def __get_name(self):
        if self.__obj == None:
            raise Exception('object is NULL')
        return self.__name
    name = property(__get_name)

    def __get_objptr(self):
        return self.__obj
    objptr = property(__get_objptr)

    def __get_scene(self):
        return self.__scene
    scene = property(__get_scene)

class Scene:
    __scenes = dict()

    def __init__(self, name):
        if name in Scene.__scenes:
            raise Exception('scene already exists')

        self.__id = _API.scn_create_scene(to_cstr(name))
        if self.__id == 0:
            raise Exception('could not create scene "%s"' % name)

        self.__objs = dict()   
        self.__name = name  
        Scene.__scenes[name] = self

    def destroy(self):
        if Engine.is_init and self.__id != 0:
            _API.scn_destroy_scene(c_uint(self.__id)) 
            self.__id = 0

    def create_object(self, name, obj_type):
        if self.__id == 0:
            raise Exception('scene is not valid')
                    
        try:
            if name in self.__objs:
                raise Exception('object already exists')
            obj = GameObject(self, name, obj_type)
        except:
            raise
        else:
            self.__objs[name] = obj
            return obj

    def create_model(self, name):
        if self.__id == 0:
            raise Exception('scene is not valid')

        return self.create_object(name, GameObjectType.MODEL)

    def update_objects(self, dt):
        for obj in self.__objs.values():
            obj.update_behaviors(dt)

    def destroy_object(self, obj):
        if self.__id == 0:
            raise Exception('scene is not valid')

        if Engine.is_init:
            if type(obj) is GameObject:
                if obj.name in self.__objs:
                    self.__objs[obj.name].destroy(scene_caller=True)
                    del self.__objs
            else:
                raise Exception('not a valid object type')

    def clear(self):
        if self.__id == 0:
            raise Exception('scene is not valid')

        _API.scn_clear(self.__id)

    def activate(self):
        if self.__id == 0:
            raise Exception('scene is not valid')
        _API.scn_setactive(self.__id)
        Engine.set_active_scene(self, caller_scene=True)

    def __get_active(self):
        if self.__id == 0:
            raise Exception('scene is not valid')
        return _API.scn_getactive(self.__id) == self.__id
    active = property(__get_active)

    def __get_id(self):
        if self.__id == 0:
            raise Exception('scene is not valid')
        return self.__id
    ID = property(__get_id)

    def find_object(self, name):
        if self.__id == 0:
            raise Exception('scene is not valid')
        return self.__objs[name]

    @staticmethod
    def find(name):
        return __scenes[name]


def init_api(debug=False):  
    _API.init(debug)

_API.init(debug = ('--debug' in sys.argv))