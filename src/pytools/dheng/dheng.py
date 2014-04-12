import sys, os, inspect
from abc import ABCMeta, abstractmethod
from ctypes import *
import math

MY_DIR = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe())[0]))
HELPER_DIR = os.path.abspath(os.path.join(MY_DIR, '..', 'helpers'))
sys.path.append(HELPER_DIR)
import dhlog
from dhcore import *
from dhapp import *

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
            dhlog.Log.warn(str(sys.exc_info()[1]))
            dhlog.Log.fatal('could not load dynamic library %s' % shlib)
            sys.exit(-1)

        dhlog.Log.msgline('module "%s" loaded' % shlib, dhlog.TERM_GREEN)

        # engine.h
        _API.eng_init = dhenglib.eng_init
        _API.eng_init.restype = c_int
        _API.eng_init.argtypes = [POINTER(InitParams)]

        _API.eng_release = dhenglib.eng_release
        _API.eng_update = dhenglib.eng_update

        _API.eng_send_guimsgs = dhenglib.eng_send_guimsgs
        _API.eng_send_guimsgs.argtypes = [c_byte, c_uint]

        _API.eng_get_frametime = dhenglib.eng_get_frametime
        _API.eng_get_frametime.restype = c_float

        _API.eng_get_sharedir = dhenglib.eng_get_sharedir
        _API.eng_get_sharedir.restype = c_char_p

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
        _API.gfx_set_gridcallback.argtypes = [c_int]

        _API.gfx_resize = dhenglib.gfx_resize
        _API.gfx_resize.argtypes = [c_uint, c_uint]

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
        _API.cmp_value_setb.argtypes = [c_ulonglong, c_char_p, c_int]

        _API.cmp_value_getb = dhenglib.cmp_value_getb
        _API.cmp_value_getb.restype = c_int
        _API.cmp_value_getb.argtypes = [POINTER(c_int), c_ulonglong, c_char_p]

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

class Engine:
    is_init = False
    __active_scene = None

    @staticmethod
    def send_keys(ch, vkey):
        _API.eng_send_guimsgs(ch, vkey)

    @staticmethod
    def update():
        if Engine.is_init:
            ft = _API.eng_get_frametime()
            Input.update(ft)

            if Engine.__active_scene != None:
                Engine.__active_scene.update_objects(ft)

            _API.eng_update()   

    @staticmethod
    def init(conf):
        r = _API.eng_init(conf.params)
        if IS_FAIL(r):
            raise Exception(Errors.last_error())

        _API.gfx_set_gridcallback(c_int(True))

        # register components
        Component.register('transform', 0x7887, Transform) 
        Component.register('camera', 0x8b72, Camera)
        Component.register('bounds', 0x8bbd, Bounds)
        Component.register('model', 0x4e9b, Model)
        Component.register('animation', 0x068b, Animation)
        Component.register('animator', 0x99e4, Animator)
        Component.register('rigidbody', 0xbc2d, RigidBody)
        Component.register('light', 0x4e0e, Light)

        Engine.is_init = True

    @staticmethod
    def release():
        _API.eng_release()
        Engine.is_init = False

    @staticmethod
    def set_active_scene(scene, caller_scene=False):
        if not caller_scene:
            scene.activate()
        else:
            Engine.__active_scene = scene

    @staticmethod
    def get_share_dir():
        return _API.eng_get_sharedir().decode()

    @staticmethod
    def resize_view(width, height):
        if Engine.is_init:
            _API.gfx_resize(c_uint(width), c_uint(height))


class EngineCallbacks:
    __metaclass__ = ABCMeta

    @abstractmethod
    def draw_debug(self):
        pass

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

    def debug(self, dbg = True):
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
        sft = c_float*4
        sf = sft()
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
        b = c_int(0)
        _API.cmp_value_getb(byref(b), self._cmp, to_cstr('active'))
        return bool(b.value)
    def __set_active(self, value):
        _API.cmp_value_setb(self._cmp, to_cstr('active'), c_int(value))
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
        b = c_int(0)
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
        b = c_int()
        _API.cmp_value_getb(byref(b), self._cmp, to_cstr('kinematic'))
        return bool(b.value)
    def __set_kinematic(self, value):
        _API.cmp_value_setb(self._cmp, to_cstr('kinematic'), c_int(value))
    kinematic = property(__get_kinematic, __set_kinematic)

    def __get_disablegravity(self):
        b = c_int()
        _API.cmp_value_getb(byref(b), self._cmp, to_cstr('disablegravity'))
        return bool(b.value)
    def __set_disablegravity(self, value):
        _API.cmp_value_setb(self._cmp, to_cstr('disablegravity'), c_uint(value))
    disable_gravity = property(__get_disablegravity, __set_disablegravity)

class Light(Component):
    class Type:
        POINT = 2
        SPOT = 3

    def __init__(self, name, cmp_type, owner_obj):
        super().__init__(name, cmp_type, owner_obj)

    def __get_type(self):
        n = c_uint()
        _API.cmp_value_getui(byref(n), self._cmp, to_cstr('type'))
        return n.value
    def __set_type(self, t):
        _API.cmp_value_setui(self._cmp, to_cstr('type'), c_uint(t))
    type = property(__get_type, __set_type)

    def __get_color(self):
        fv = c_float*4
        _API.cmp_value_get4f(fv, self._cmp, to_cstr('color'))
        return Color(fv[0].value, fv[1].value, fv[2].value, fv[3].value)
    def __set_color(self, c):
        fvt = c_float*4
        fv = fvt()
        fv[0] = c.r
        fv[1] = c.g
        fv[2] = c.b
        fv[3] = c.a
        _API.cmp_value_set4f(self._cmp, to_cstr('color'), fv)
    color = property(__get_color, __set_color)

    def __get_intensity(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('intensity'))
        return f.value
    def __set_intensity(self, f):
        _API.cmp_value_setf(self._cmp, to_cstr('intensity'), c_float(f))
    intensity = property(__get_intensity, __set_intensity)

    def __get_attennear(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('atten_near'))
    def __set_attennear(self, n):
        _API.cmp_value_setf(self._cmp, to_cstr('atten_near'), c_float(n))
    atten_near = property(__get_attennear, __set_attennear)

    def __get_attenfar(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('atten_far'))
    def __set_attenfar(self, n):
        _API.cmp_value_setf(self._cmp, to_cstr('atten_far'), c_float(n))
    atten_far = property(__get_attenfar, __set_attenfar)

    def __get_attennarrow(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('atten_narrow'))
    def __set_attennarrow(self, n):
        _API.cmp_value_setf(self._cmp, to_cstr('atten_narrow'), c_float(n))
    atten_narrow = property(__get_attennarrow, __set_attennarrow)

    def __get_attenfar(self):
        f = c_float()
        _API.cmp_value_getf(byref(f), self._cmp, to_cstr('atten_far'))
    def __set_attenfar(self, n):
        _API.cmp_value_setf(self._cmp, to_cstr('atten_far'), c_float(n))
    atten_far = property(__get_attenfar, __set_attenfar)

    def __get_lodscheme(self):
        s = create_string_buffer(128)
        _API.cmp_value_gets(s, c_uint(128), self._cmp, to_cstr('lod_scheme'))
        return s.value.decode()
    def __set_lodscheme(self, fpath):
        r = _API.cmp_value_sets(self._cmp, to_cstr('lod_scheme'), to_cstr(fpath))
        if IS_FAIL(r):
            raise Exception(Errors.last_error())
    lod_scheme = property(__get_lodscheme, __set_lodscheme)

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
                Input.lock_cursor()

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
                Input.lock_cursor()

            delta_pos = (mpos - self._lockpos)*self.sensivity
            self._distance += delta_pos.y
            self._lockpos = mpos.copy()
        else:
            self._rightbtn_dwn = False            

        if (not self._rightbtn_dwn) and (not self._leftbtn_dwn):
            Input.unlock_cursor()

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
    class Type:
        MODEL = (1<<0)
        PARTICLE = (1<<1)
        LIGHT = (1<<2)
        DECAL = (1<<3)
        CAMERA = (1<<4)
        TRIGGER = (1<<5)
        ENV = (1<<6)

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

        if obj_type == GameObject.Type.CAMERA:
            self.add_component('camera')
        elif obj_type == GameObject.Type.MODEL:
            self.add_component('model')
        elif obj_type == GameObject.Type.LIGHT:
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

        return self.create_object(name, GameObject.Type.MODEL)

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