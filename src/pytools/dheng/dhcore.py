import sys, os, inspect
from ctypes import *
import math

MY_DIR = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe())[0]))
HELPER_DIR = os.path.abspath(os.path.join(MY_DIR, '..', 'helpers'))
sys.path.append(HELPER_DIR)
import dhlog

class _API:
    is_init = False

    @staticmethod 
    def init(debug=False):
        if _API.is_init:
            return

        postfix = ''
        if debug:
            postfix = '-dbg'

        if sys.platform == 'win32':
            shlib = 'dhcore' + postfix + '.dll'
        elif sys.platform == 'linux':
            shlib = 'libdhcore' + postfix + '.so'

        # load library
        try:
            dhcorelib = cdll.LoadLibrary(shlib)
        except:
            dhlog.Log.fatal('could not load dynamic library %s' % shlib)
            sys.exit(-1)

        dhlog.Log.msgline('module "%s" loaded' % shlib, dhlog.TERM_GREEN)
        
        # core.h
        _API.core_init = dhcorelib.core_init
        _API.core_init.restype = c_int
        _API.core_init.argtypes = [c_uint]

        _API.core_release = dhcorelib.core_release
        _API.core_release.argtypes = [c_uint]

        # err.h
        _API.err_getstring = dhcorelib.err_getstring
        _API.err_getstring.restype = c_char_p

        # log.h
        _API.log_outputconsole = dhcorelib.log_outputconsole
        _API.log_outputconsole.restype = c_uint
        _API.log_outputconsole.argtypes = [c_uint]

        _API.log_outputfile = dhcorelib.log_outputfile
        _API.log_outputfile.restype = c_uint
        _API.log_outputfile.argtypes = [c_uint, c_char_p]

        _API.log_isfile = dhcorelib.log_isfile
        _API.log_isfile.restype = c_uint

        _API.log_isconsole = dhcorelib.log_isconsole
        _API.log_isconsole.restype = c_uint

        _API.log_print = dhcorelib.log_print
        _API.log_print.argtypes = [c_uint, c_char_p]

        # file-io.h
        _API.fio_addvdir = dhcorelib.fio_addvdir
        _API.fio_addvdir.restype = c_uint
        _API.fio_addvdir.argtypes = [c_char_p, c_uint]

        # vec-math.h
        _API.mat3_mul = dhcorelib.mat3_mul
        _API.mat3_mul.restype = POINTER(Matrix3)
        _API.mat3_mul.argtypes = [POINTER(Matrix3), POINTER(Matrix3), POINTER(Matrix3)]

        _API.vec3_transformsrt = dhcorelib.vec3_transformsrt
        _API.vec3_transformsrt.restype = POINTER(Vec3)
        _API.vec3_transformsrt.argtypes = [POINTER(Vec3), POINTER(Vec3), POINTER(Matrix3)]

        _API.mat3_muls = dhcorelib.mat3_muls
        _API.mat3_muls.restype = POINTER(Matrix3)
        _API.mat3_muls.argtypes = [POINTER(Matrix3), POINTER(Matrix3), c_float]

        _API.mat3_set_roteuler = dhcorelib.mat3_set_roteuler
        _API.mat3_set_roteuler.restype = POINTER(Matrix3)
        _API.mat3_set_roteuler.argtypes = [POINTER(Matrix3), c_float, c_float, c_float]

        _API.quat_slerp = dhcorelib.quat_slerp
        _API.quat_slerp.restype = POINTER(Quat)
        _API.quat_slerp.argtypes = [POINTER(Quat), POINTER(Quat), POINTER(Quat), c_float]

        _API.quat_fromaxis = dhcorelib.quat_fromaxis
        _API.quat_fromaxis.restype = POINTER(Quat)
        _API.quat_fromaxis.argtypes = [POINTER(Quat), POINTER(Vec3), c_float]

        _API.quat_fromeuler = dhcorelib.quat_fromeuler
        _API.quat_fromeuler.restype = POINTER(Quat)
        _API.quat_fromeuler.argtypes = [POINTER(Quat), c_float, c_float, c_float]

        _API.quat_frommat3 = dhcorelib.quat_frommat3
        _API.quat_frommat3.restype = POINTER(Quat)
        _API.quat_frommat3.argtypes = [POINTER(Quat), POINTER(Matrix3)]

        _API.mat3_inv = dhcorelib.mat3_inv
        _API.mat3_inv.restype = POINTER(Matrix3)
        _API.mat3_inv.argtypes = [POINTER(Matrix3), POINTER(Matrix3)]

        _API.mat3_set_rotaxis = dhcorelib.mat3_set_rotaxis
        _API.mat3_set_rotaxis.restype = POINTER(Matrix3)
        _API.mat3_set_rotaxis.argtypes = [POINTER(Matrix3), POINTER(Vec3), c_float]

        _API.mat3_set_roteuler = dhcorelib.mat3_set_roteuler
        _API.mat3_set_roteuler.restype = POINTER(Matrix3)
        _API.mat3_set_roteuler.argtypes = [POINTER(Matrix3), c_float, c_float, c_float]

        _API.mat3_set_rotquat = dhcorelib.mat3_set_rotquat
        _API.mat3_set_rotquat.restype = POINTER(Matrix3)
        _API.mat3_set_rotquat.argtypes = [POINTER(Matrix3), POINTER(Quat)]

        _API.mat3_inv = dhcorelib.mat3_inv
        _API.mat3_inv.restype = POINTER(Matrix3)
        _API.mat3_inv.argtypes = [POINTER(Matrix3), POINTER(Matrix3)]

        _API.mat3_det = dhcorelib.mat3_det
        _API.mat3_det.restype = c_float
        _API.mat3_det.argtypes = [POINTER(Matrix3)]

        _API.is_init = True 

def IS_FAIL(r):
    if r <= 0:  return True
    else:       return False

INVALID_HANDLE = 0xffffffffffffffff

def to_cstr(s):
    return create_string_buffer(s.encode('ascii'))

class Errors:
    @staticmethod
    def last_error():
        r = _API.err_getstring()
        return r.decode()

class Log:
    class LogType():
        TEXT = 0
        ERROR = 1
        WARNING = 3,
        INFO = 3,
        LOAD = 4

    def get_console_output(self):
        return bool(_API.log_isconsole())
    def set_console_output(self, enable):
        _API.log_outputconsole(c_uint(enable))
    console_output = property(get_console_output, set_console_output)

    def get_file_output(self):
        return bool(_API.log_isfile())
    def set_file_output(self, logfile):
        _API.log_outputfile(c_uint(True), create_string_buffer(logfile.encode('ascii')))
    def del_file_output(self):
        _API.log_outputfile(c_uint(False), c_char_p(None))
    file_output = property(get_file_output, set_file_output, del_file_output)  

    def msg(log_type, msg):
        _API.log_print(c_uint(log_type), create_string_buffer(msg.encode('ascii')))

class Core:
    class InitFlags():
        TRACE_MEM = (1<<0)
        CRASH_DUMP = (1<<1)
        LOGGER = (1<<2)
        ERRORS = (1<<3)
        JSON = (1<<4)
        FILE_IO = (1<<5)
        TIMER = (1<<6)
        ALL = 0xffffffff

    @staticmethod
    def init(flags = InitFlags.ALL):
        if IS_FAIL(_API.core_init(c_uint(flags))):
            raise Exception(_API.err_getstring()) 

    @staticmethod
    def release(report_leaks = True):
        _API.core_release(c_uint(report_leaks))


class Vec3(Structure):
    _fields_ = [('x', c_float), ('y', c_float), ('z', c_float), ('w', c_float)]

    def __init__(self, _x = 0, _y = 0, _z = 0, _w = 1):
        self.x = _x
        self.y = _y
        self.z = _z
        self.w = 1

    def __add__(a, b):
        return Vec3(a.x + b.x, a.y + b.y, a.z + b.z)

    def __mul__(a, b):
        if type(b) is float or type(b) is int:
            return Vec3(a.x*b, a.y*b, a.z*b)
        elif type(b) is Matrix3:
            r = Vec3()
            _API.vec3_transformsrt(byref(r), byref(a), byref(b))
            return r

    def copy(self):
        return Vec3(self.x, self.y, self.z)

    def __div__(a, b):
        return Vec3(a.x/b, a.y/b, a.z/b)

    def __eq__(a, b):
        if a.x == b.x and a.y == b.y and a.z == b.z:
            return True
        else:
            return False

    def __sub__(a, b):
        return Vec3(a.x - b.x, a.y - b.y, a.z - b.z)

    def get_length(self):
        return math.sqrt(self.x*self.x + self.y*self.y + self.z*self.z)
    length = property(get_length)

    @staticmethod
    def dot(a, b):
        return a.x*b.x + a.y*b.y + a.z*b.z

    @staticmethod
    def normalize(v):
        scale = 1.0 / v.length
        return Vec3(v.x*scale, v.y*scale, v.z*scale)

    @staticmethod
    def cross(v1, v2):
        return Vec3(v1.y*v2.z - v1.z*v2.y, v1.z*v2.x - v1.x*v2.z, v1.x*v2.y - v1.y*v2.x)     

    @staticmethod
    def lerp(v1, v2, t):
        return Vec3(\
            v1.x + t*(v2.x - v1.x),
            v1.y + t*(v2.y - v1.y),
            v1.z + t*(v2.z - v1.z))

    def __str__(self):
        return 'Vec3: %f, %f, %f' % (self.x, self.y, self.z)

class Vec2(Structure):
    _fields_ = [('x', c_float), ('y', c_float)]

    def __init__(self, _x = 0, _y = 0):
        self.x = _x
        self.y = _y

    def copy(self):
        return Vec2(self.x, self.y)

    def __add__(a, b):
        return Vec2(a.x + b.x, a.y + b.y)

    def __sub__(a, b):
        return Vec2(a.x - b.x, a.y - b.y)

    def __mul__(a, b):
        return Vec2(a.x*b, a.y*b)

    def __div__(a, b):
        return Vec2(a.x/b, a.y/b)

    def __str__(self):
        return 'Vec2: %f, %f' % (self.x, self.y)    

class Vec2i(Structure):
    _fields_ = [('x', c_int), ('y', c_int)]

    def __init__(self, _x = 0, _y = 0):
        self.x = int(_x)
        self.y = int(_y)
    
    def copy(self):
        return Vec2i(self.x, self.y)

    def __add__(a, b):
        return Vec2(a.x + b.x, a.y + b.y)

    def __sub__(a, b):
        return Vec2(a.x - b.x, a.y - b.y)

    def __mul__(a, b):
        return Vec2(a.x*b, a.y*b)

    def __str__(self):
        return 'Vec2i: %d, %d' % (self.x, self.y)        

class Vec4(Structure):
    _fields_ = [('x', c_float), ('y', c_float), ('z', c_float), ('w', c_float)]

    def __init__(self, _x = 0, _y = 0, _z = 0, _w = 1):
        self.x = _x
        self.y = _y
        self.z = _z
        self.w = 1

    def copy(self):
        return Vec4(self.x, self.y, self.z, self.w)

    def __add__(a, b):
        return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w)

    def __sub__(a, b):
        return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w)

    def __mul__(a, b):
        return Vec4(a.x*b, a.y*b, a.z*b, a.w*b)

    def __div__(a, b):
        return Vec4(a.x/b, a.y/b, a.z/b, a.w/b)

    def __str__(self):
        return 'Vec4: %f, %f, %f, %f' % (self.x, self.y, self.z, self.w)


class Color4(Structure):
    _fields_ = [('r', c_float), ('g', c_float), ('b', c_float), ('a', c_float)]

    def __init__(self, _r = 0, _g = 0, _b = 0, _a = 1):
        self.r = _r
        self.g = _g
        self.b = _b
        self.a = _a

    def copy(self):
        return Color(self.r, self.g, self.b, self.a)

    def __mul__(a, b):
        return Color4(a.r*b, a.g*b, a.g*b, a.a)

    def __mul__(a, b):
        return Color4(a.r*b.r, a.g*b.g, a.g*b.b, min(a.a, b.a))

    def __add__(a, b):
        return Color4(a.r+b.r, a.g+b.g, a.b+b.b, max(a.a, b.a))

    @staticmethod
    def lerp(c1, c2, t):
        tinv = 1 - t
        return Color4(
            c1.r*t + c2.r*tinv,
            c1.g*t + c2.g*tinv,
            c1.b*t + c2.b*tinv,
            c1.a*t + c2.a*tinv)

class Quat(Structure):
    _fields_ = [('x', c_float), ('y', c_float), ('z', c_float), ('w', c_float)]

    def __init__(self, _x = 0, _y = 0, _z = 0, _w = 1):
        self.x = _x
        self.y = _y
        self.z = _z
        self.w = _w

    def copy(self):
        return Color(self.x, self.y, self.z, self.w)

    def __mul__(q1, q2):
        return Quat(\
            q1.w*q2.x + q1.x*q2.w + q1.z*q2.y - q1.y*q2.z,
            q1.w*q2.y + q1.y*q2.w + q1.x*q2.z - q1.z*q2.x,
            q1.w*q2.z + q1.z*q2.w + q1.y*q2.x - q1.x*q2.y,
            q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z)

    def __eq__(q1, q2):
        if q1.x == q2.x and q1.y == q2.y and q1.z == q2.z and q1.w == q2.w:
            return True
        else:
            return False

    def from_axis(self, axis, angle):
        _API.quat_fromaxis(byref(self), byref(axis), c_float(angle))

    def from_euler(self, pitch, yaw, roll):
        _API.quat_fromeuler(byref(self), c_float(pitch), c_float(yaw), c_float(roll))

    def from_matrix3(self, mat):
        _API.quat_frommat3(byref(self), byref(mat))

    @staticmethod
    def inverse(q):
        return Quat(-q.x, -q.y, -q.z, q.w)

    @staticmethod
    def slerp(q1, q2, t):
        q = Quat()
        _API.quat_slerp(byref(q), byref(q1), byref(q2), c_float(t))
        return q

    def __str__(self):
        return 'Quat: %f %f %f %f' % (self.x, self.y, self.z, self.w)

class Matrix3(Structure):
    _fields_ = [\
        ('m11', c_float), ('m12', c_float), ('m13', c_float), ('m14', c_float),
        ('m21', c_float), ('m22', c_float), ('m23', c_float), ('m24', c_float),
        ('m31', c_float), ('m32', c_float), ('m33', c_float), ('m34', c_float),
        ('m41', c_float), ('m42', c_float), ('m43', c_float), ('m44', c_float)]

    def __init__(self, _m11 = 1, _m12 = 0, _m13 = 0, _m21 = 0, _m22 = 1, _m23 = 0, 
        _m31 = 0, _m32 = 0, _m33 = 1, _m41 = 0, _m42 = 0, _m43 = 0):
        self.m11 = _m11
        self.m12 = _m12
        self.m13 = _m13
        self.m21 = _m21
        self.m22 = _m22
        self.m23 = _m23
        self.m31 = _m31
        self.m32 = _m32
        self.m33 = _m33
        self.m41 = _m41
        self.m42 = _m42
        self.m43 = _m43

    def copy(self):
        return Matrix3(\
            self.m11, self.m12, self.m13,
            self.m21, self.m22, self.m23,
            self.m31, self.m32, self.m33,
            self.m41, self.m42, self.m43)

    def __mul__(a, b):
        if type(b) is float or type(b) is int:
            m = Matrix3()
            _API.mat3_muls(byref(m), byref(a), c_float(b))
            return m
        else:
            m = Matrix3()
            _API.mat3_mul(byref(m), byref(a), byref(b))
            return m

    def translate(self, x, y, z):
        self.m41 = x
        self.m42 = y
        self.m43 = z

    def translate(self, v):
        self.m41 = v.x
        self.m42 = v.y
        self.m43 = v.z

    def rotate_euler(self, pitch, yaw, roll):
        _API.mat3_set_roteuler(byref(self), c_float(pitch), c_float(yaw), c_float(roll))

    def rotate_quat(self, q):
        _API.mat3_set_rotquat(byref(self), byref(q))

    def rotate_axis(self, axis, angle):
        _API.mat3_set_rotaxis(byref(self), byref(axis), c_float(angle))

    def scale(self, sx, sy, sz):
        self.m11 = sx
        self.m22 = sy
        self.m33 = sz

    def __get_determinant(self):
        return _API.mat3_det(byref(self))
    determinant = property(__get_determinant)

    def __get_translation(self):
        return Vec3(self.m41, self.m42, self.m43)
    translation = property(__get_translation)

    @staticmethod
    def transpose(m):
        return Matrix3(\
            self.m11, self.m21, self.m31, 
            self.m12, self.m22, self.m32, 
            self.m13, self.m23, self.m33, 
            self.m14, self.m24, self.m34)

    @staticmethod
    def invert(m):
        r = Matrix3()
        _API.mat3_inv(byref(r), byref(m))
        return r

class Matrix4(Structure):
    _fields_ = [\
        ('m11', c_float), ('m12', c_float), ('m13', c_float), ('m14', c_float),
        ('m21', c_float), ('m22', c_float), ('m23', c_float), ('m24', c_float),
        ('m31', c_float), ('m32', c_float), ('m33', c_float), ('m34', c_float),
        ('m41', c_float), ('m42', c_float), ('m43', c_float), ('m44', c_float)]

    def __init__(self,
        _m11 = 1, _m12 = 0, _m13 = 0, _m14 = 0, 
        _m21 = 0, _m22 = 1, _m23 = 0, _m24 = 0, 
        _m31 = 0, _m32 = 0, _m33 = 1, _m34 = 0, 
        _m41 = 0, _m42 = 0, _m43 = 0, _m44 = 1):
        self.m11 = _m11
        self.m12 = _m12
        self.m13 = _m13
        self.m14 = _m14
        self.m21 = _m21
        self.m22 = _m22
        self.m23 = _m23
        self.m24 = _m24
        self.m31 = _m31
        self.m32 = _m32
        self.m33 = _m33
        self.m34 = _m34
        self.m41 = _m41
        self.m42 = _m42
        self.m43 = _m43
        self.m44 = _m44

    def copy(self):
        return Matrix4(\
            self.m11, self.m12, self.m13, self.m14,
            self.m21, self.m22, self.m23, self.m24,
            self.m31, self.m32, self.m33, self.m34,
            self.m41, self.m42, self.m43, self.m44)

class Math:
    PI = 3.14159265

    @staticmethod
    def to_rad(x):
        return x*Math.PI/180.0

    @staticmethod
    def to_deg(x):
        return 180.0*x/Math.PI

class FileIO:
    @staticmethod
    def add_virtual_path(path, monitor=False):
        path = os.path.abspath(os.path.expanduser(path))
        if not _API.fio_addvdir(to_cstr(path), c_uint(monitor)):
            raise Exception(Errors.last_error())

_API.init(debug = ('--debug' in sys.argv))