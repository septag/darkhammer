import sys, os, inspect
from ctypes import *
import math

MY_DIR = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe())[0]))
HELPER_DIR = os.path.abspath(os.path.join(MY_DIR, '..', 'helpers'))
sys.path.append(HELPER_DIR)
from dhlog import Log

class _API:
    is_init = False

    @staticmethod 
    def init():
        if _API.is_init:
            return

        if sys.platform == 'win32':
            shlib = 'dhcore.dll'
        elif sys.platform == 'linux':
            shlib = 'libdhcore.so'

        # load library
        try:
            dhcorelib = cdll.LoadLibrary(shlib)
        except:
            Log.fatal('could not load dynamic library %s' % shlib)
            sys.exit(-1)

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

        _API.is_init = True 

def IS_FAIL(r):
    if r <= 0:  return True
    else:       return False

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
        return Vec3(a.x*b, a.y*b, a.z*b)

    def dot(a, b):
        return a.x*b.x + a.y*b.y + a.z*b.z

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

class Vec2(Structure):
    _fields_ = [('x', c_float), ('y', c_float)]

    def __init__(self, _x = 0, _y = 0):
        self.x = _x
        self.y = _y

    def __add__(a, b):
        return Vec2(a.x + b.x, a.y + b.y)

    def __sub__(a, b):
        return Vec2(a.x - b.x, a.y - b.y)

    def __mul__(a, b):
        return Vec2(a.x*b, a.y*b)

    def __div__(a, b):
        return Vec2(a.x/b, a.y/b)

class Vec4(Structure):
    _fields_ = [('x', c_float), ('y', c_float), ('z', c_float), ('w', c_float)]

    def __init__(self, _x = 0, _y = 0, _z = 0, _w = 1):
        self.x = _x
        self.y = _y
        self.z = _z
        self.w = 1

    def __add__(a, b):
        return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w)

    def __sub__(a, b):
        return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w)

    def __mul__(a, b):
        return Vec4(a.x*b, a.y*b, a.z*b, a.w*b)

    def __div__(a, b):
        return Vec4(a.x/b, a.y/b, a.z/b, a.w/b)


class Color4(Structure):
    _fields_ = [('r', c_float), ('g', c_float), ('b', c_float), ('a', c_float)]

    def __init__(self, _r = 0, _g = 0, _b = 0, _a = 1):
        self.r = _r
        self.g = _g
        self.b = _b
        self.a = _a

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

_API.init()