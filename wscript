# dark-hammer build tools

import os, sys, platform, inspect, glob
import waflib.Logs

# main global variables
VERSION = "0.5.0"
PROJNAME = "darkhammer"
PYMOD_VERSION = '2.7'

top = "."
out = "build"
ROOTDIR = os.path.dirname(inspect.getfile(inspect.currentframe()))

# setup variant builds (debug/release)
def init(ctx):
    from waflib.Build import BuildContext, CleanContext, InstallContext, UninstallContext
    for y in (BuildContext, CleanContext, InstallContext, UninstallContext):
        name = y.__name__.replace('Context', '').lower()
        class DebugCtx(y):
            cmd = name + '_debug'   # command names like 'build_debug', 'clean_debug', etc
            variant = 'debug'
        class ReleaseCtx(y):
            # if a 'cmd' is not given, the default 'build', 'clean',
            # etc actions will be redirected to the 'release' variant dir
            variant = 'release'

# options command
def options(opt):
    opt.load('compiler_c compiler_cxx')

    gfxapi_default = ''
    if sys.platform == 'win32':
        gfxapi_default = 'D3D'
    else:
        gfxapi_default = 'GL'

    # fetch from environment variables as default
    opt.add_option('--enable-assert', action='store_true', default=False, dest='DASSERT',
        help='Enable assertions (debug build always have assertions)')
    opt.add_option('--enable-profile', action='store_true', default=False, dest='DPROFILE',
        help='Enable profiling (decreases runtime performance, asks for server TCP port)')
    opt.add_option('--retail-build', action='store_true', default=False, dest='DRETAIL',
        help='Retail build (full optimization)')
    opt.add_option('--gfx-api', action='store', default=gfxapi_default, dest='GFX_API',
        type='choice', help='Graphics API', choices=['D3D', 'GL'])
    opt.add_option('--physx-sdk', action='store', default='', dest='PHYSX_PREFIX',
        help='Physx SDK path prefix')
    opt.add_option('--dx-sdk', action='store', default='', dest='DX_PREFIX',
        help='DirectX SDK path prefix (optional)')
    opt.add_option('--cross-compile', action='store', default='', dest='CROSS_COMPILE',
        help='Use a cross-compiler, must provide compiler executable name/path')
    opt.add_option('--prefix', action='store', default='', dest='PREFIX',
        help='Install path prefix')
    opt.add_option('--ignore-tuts', action='store_true', default=False, dest='IGNORE_TUTS',
        help='Do not build tutorials')
    opt.add_option('--ignore-tests', action='store_true', default=False, dest='IGNORE_TESTS',
        help='Do not build test programs')
    opt.add_option('--ignore-tools', action='store_true', default=False, dest='IGNORE_TOOLS',
        help='Do not build tools')

def compiler_load(conf):
    if conf.options.CROSS_COMPILE != '':
        conf.env['CC'] = conf.options.CROSS_COMPILE

def compiler_check_symbol(conf, statement, header, symbol_name, required=False, lib=''):
    frag = '#include <{0}>\nint main() {{{1}; return 0;}}'.format(header, statement)
    r = conf.check_cc(fragment=frag, msg="Checking for symbol '" + symbol_name + "'",
        mandatory=required, lib=lib)
    if r:
        conf.define('HAVE_' + symbol_name.upper(), True)
    return r;

def compiler_is_msvc(conf):
    cc = conf.env['CC'][0].lower()
    if 'cl.exe' in cc or 'icl.exe' in cc:
        return True
    else:
        return False

def compiler_is_icc(conf):
    cc = conf.env['CC'][0].lower()
    if 'icl.exe' in cc:
        return True
    else:
        return False

def compiler_is_clang(conf):
    cc = conf.env['CC'][0].lower()
    if 'clang' in cc:
        return True
    else:
        return False

def compiler_is_x64(conf):
    return (conf.env.DEST_CPU == 'x86_64' or conf.env.DEST_CPU == 'amd64')

def compiler_setup(conf):
    cflags = []
    cxxflags = []

    # general
    if compiler_is_msvc(conf):
        cflags.extend(['/W3', '/fp:fast', '/FC', '/GS-'])
        conf.env.append_unique('LINKFLAGS', ['/MANIFEST', '/NOLOGO'])
        conf.env['WINDOWS_EMBED_MANIFEST'] = True

        if compiler_is_x64(conf):   conf.env.append_unique('LINKFLAGS', '/MACHINE:X64')
        else:                       conf.env.append_unique('LINKFLAGS', '/MACHINE:X86')

        conf.env.append_unique('DEFINES', ['_CRT_SECURE_NO_WARNINGS', '_CRT_NONSTDC_NO_DEPRECATE',
            '_SCL_SECURE_NO_WARNINGS', '_MBCS'])
        if compiler_is_icc(conf):
            cflags.extend(['/Qstd=c99', '/Qintel-extensions-', '/Qfast-transcendentals-',
                '/Qparallel-'])
    else:
        cflags.extend(['-std=c99', '-ffast-math', '-malign-double', '-fPIC',
            '-fvisibility=internal', '-Winline', '-Wall', '-msse', '-msse2', '-msse4.1'])
        conf.env.append_unique('DEFINES', ['_POSIX_C_SOURCE', '_GNU_SOURCE'])
        if compiler_is_clang(conf):
            cflags.extend(['-arch x86_64', '-fms-extensions'])

    cxxflags.extend(cflags)

    if '-std=c99' in cxxflags:  del cxxflags[cxxflags.index('-std=c99')]
    if '/TP' in cxxflags: del cxxflags[cxxflags.index('/TP')]
    if '-fvisibility=internal' in cxxflags:  del cxxflags[cxxflags.index('-fvisibility=internal')]

    conf.env.append_unique('CFLAGS', cflags)
    conf.env.append_unique('CXXFLAGS', cxxflags)
    conf.env.append_unique('DEFINES', '_SIMD_SSE_')

    if conf.options.DASSERT: conf.env.append_unique('DEFINES', '_ENABLEASSERT_')
    if conf.options.DPROFILE:   conf.env.append_unique('DEFINES', '_PROFILE_')
    if conf.options.DRETAIL: conf.env.append_unique('DEFINES', '_RETAIL_')
    conf.env.append_unique('DEFINES', '_' + conf.options.GFX_API + '_')
    conf.define('FULL_VERSION', VERSION)

    if sys.platform == 'win32':
        conf.env.append_unique('DEFINES', ['WIN32', '_WIN_'])
    elif sys.platform == 'linux':
        conf.env.append_unique('DEFINES', '_LINUX_')
    elif sys.platform == 'darwin':
        conf.env.append_unique('DEFINES', '_OSX_')

    # general lib/include path
    conf.env.append_unique('INCLUDES', os.path.join(conf.env.ROOTDIR, 'include'))
    conf.env.append_unique('LIBPATH', os.path.join(conf.env.PREFIX, 'lib'))

    # build specific
    base_env = conf.env.derive()
    # debug
    cflags = []
    conf.setenv('debug', env=base_env.derive())
    if compiler_is_msvc(conf):
        cflags.extend(['/Od', '/Z7', '/RTC1', '/MDd'])
        conf.env.append_unique('LINKFLAGS', '/DEBUG')
    else:
        cflags.extend(['-g', '-O0'])
    conf.env.append_unique('CFLAGS', cflags)
    conf.env.append_unique('CXXFLAGS', cflags)
    conf.env.append_unique('DEFINES', ['_DEBUG', '_DEBUG_', '_ENABLEASSERT_'])
    conf.env.SUFFIX = '-dbg'

    # release
    cflags = []
    conf.setenv('release', env=base_env.derive())
    if compiler_is_msvc(conf):
        cflags.extend(['/O2', '/Oi', '/MD'])
        conf.env.append_unique('LINKFLAGS', '/RELEASE')
    else:
        cflags.extend(['-O2', '-Wno-unused-result'])
    conf.env.append_unique('CFLAGS', cflags)
    conf.env.append_unique('CXXFLAGS', cflags)
    conf.env.append_unique('DEFINES', 'NDEBUG')
    conf.env.SUFFIX = ''

def compiler_print_opts(conf):
    conf.start_msg('C flags:')
    conf.end_msg(str.join(' ', conf.env.CFLAGS))

    conf.start_msg('CXX flags:')
    conf.end_msg(str.join(' ', conf.env.CXXFLAGS))

    conf.start_msg('Linker flags:')
    conf.end_msg(str.join(' ', conf.env.LINKFLAGS))

    conf.start_msg('Defines:')
    conf.end_msg(str.join(', ',  conf.env.DEFINES))

def compiler_setup_deps(conf):
    # backup current env
    base_env = conf.env.derive()

    have_malloc_h = conf.check_cc(header_name='malloc.h', mandatory=False)
    have_alloca_h = conf.check_cc(header_name='alloca.h', mandatory=False)
    have_alloca = False
    if have_alloca_h:
        have_alloca = compiler_check_symbol(conf, 'alloca(0);', 'alloca.h', 'alloca')
    elif have_malloc_h:
        have_alloca = compiler_check_symbol(conf, 'alloca(0);', 'malloc.h', 'alloca')
    if not have_alloca:
        compiler_check_symbol(conf, '_alloca(0);', 'malloc.h', '_alloca', required=True)
        conf.define('alloca', '_alloca', quote=False)

    compiler_check_symbol(conf, 'lrintf(0.0);', 'math.h', 'lrintf', lib='m')

    sse_flags = ''
    if not compiler_is_msvc(conf):
        sse_flags = '-msse -msse2 -msse4.1'
    conf.check_cc(header_name='xmmintrin.h', cflags=sse_flags, define_ret=False)
    conf.check_cc(header_name='emmintrin.h', cflags=sse_flags, define_ret=False)
    conf.check_cc(header_name='smmintrin.h', cflags=sse_flags, define_ret=False)

    if sys.platform == 'win32':
        conf.check_cc(header_name='windows.h', define_ret=False)
    else:
        conf.check_cc(header_name='pthread.h', define_ret=False)

    # check for deps and SDKs

    # PhysX sdk
    physx_env = base_env.derive()

    platform_prefix = ''
    if sys.platform == 'win32': platform_prefix = 'win'
    elif sys.platform == 'linux': platform_prefix = 'linux'
    elif sys.platform == 'darwin': platform_prefix = 'osx'

    physx_prefix = os.path.abspath(os.path.expanduser(conf.options.PHYSX_PREFIX))
    physx_libpath = \
        os.path.join(physx_prefix, 'Lib', platform_prefix + '64') if compiler_is_x64(conf) else \
        os.path.join(physx_prefix, 'Lib', platform_prefix + '32')
    physx_includes = os.path.join(physx_prefix, 'Include')
    
    lib_postfix = ''
    if sys.platform == 'win32':
        if compiler_is_x64(conf):   lib_postfix = '_x64'
        else:                       lib_postfix = '_x86'

    physx_env.append_unique('LIBPATH', physx_libpath)
    physx_env.append_unique('INCLUDES', physx_includes)
    conf.check_cxx(env=physx_env, header_name='PxPhysicsAPI.h', defines='NDEBUG', define_ret=False)
    conf.check_cc(env=physx_env, lib='PhysX3'+lib_postfix, define_ret=False)
    conf.check_cc(env=physx_env, lib='PhysX3Common'+lib_postfix, define_ret=False)
    conf.check_cc(env=physx_env, lib='PhysX3Cooking'+lib_postfix, define_ret=False)

    base_env.PHYSX_INCLUDES = physx_includes
    base_env.PHYSX_LIBPATH = physx_libpath

    # Graphics API
    if conf.options.GFX_API == 'D3D':
        # DX SDK
        dx_env = base_env.derive()
        dx_prefix = ''
        dx_includes = ''
        dx_libpath = ''

        if conf.options.DX_PREFIX != '':
            dx_prefix = os.path.abspath(os.path.expanduser(conf.options.DX_PREFIX))
            dx_libpath = os.path.join(dx_prefix, 'Lib', 'x64') if compiler_is_x64(conf) else \
                os.path.join(dx_prefix, 'Lib', 'x86')
            dx_includes = os.path.join(dx_prefix, 'Include')

            dx_env.append_unique('LIBPATH', dx_libpath)
            dx_env.append_unique('INCLUDES', dx_includes)

        conf.check_cxx(env=dx_env, header_name='D3D11.h', define_ret=False)
        conf.check_cxx(env=dx_env, lib='d3d11')

        base_env.DX_INCLUDES = dx_includes
        base_env.DX_LIBPATH = dx_libpath
        base_env.GFX_API = 'D3D'
    elif conf.options.GFX_API == 'GL':
        # OpenGL
        conf.check_cc(header_name='GL/gl.h', define_ret=False)
        conf.check_cc(lib='GL')
        base_env.GFX_API = 'GL'

    prefix = os.path.abspath(conf.options.PREFIX)
    if prefix != ROOTDIR:
        conf.define('SHARE_DIR', os.path.join(prefix, 'share', PROJNAME).replace('\\', '\\\\'))
    else:
        conf.define('SHARE_DIR', prefix.replace('\\', '\\\\'))

    conf.write_config_header()
    conf.set_env(base_env)

def configure(conf):
    conf.check_waf_version(mini='1.7.11')
    compiler_load(conf)
    conf.load('compiler_c compiler_cxx')

    compiler_setup_deps(conf)
    conf.recurse('3rdparty/nvtt')

    # set remaining option variables
    conf.env.ROOTDIR = ROOTDIR
    conf.env.PREFIX = os.path.abspath(conf.options.PREFIX)
    conf.env.VERSION = VERSION
    conf.env.IGNORE_TUTS = conf.options.IGNORE_TUTS
    conf.env.IGNORE_TOOLS = conf.options.IGNORE_TOOLS
    conf.env.IGNORE_TESTS = conf.options.IGNORE_TESTS
    conf.env.DFILEMON = True

    conf.start_msg('Processor')
    conf.end_msg(conf.env.DEST_CPU)

    ## install directory
    conf.start_msg('Install directory')
    conf.end_msg(conf.env.PREFIX)

    conf.start_msg('Graphics API')
    conf.end_msg(conf.options.GFX_API)

    compiler_setup(conf)

def build(bld):
    if sys.platform == 'linux':
        libdir = os.path.join(bld.env.PREFIX, 'lib')
        os.environ['LD_LIBRARY_PATH'] = libdir
        delfiles = glob.glob(os.path.join(libdir, 'libdhcore' + bld.env.SUFFIX + '*'))
        delfiles.extend(glob.glob(os.path.join(libdir, 'libdheng' + bld.env.SUFFIX + '.*')))
        delfiles.extend(glob.glob(os.path.join(libdir, 'libdhapp' + bld.env.SUFFIX + '.*')))
        for f in delfiles:
            os.remove(f)

    bld.recurse('3rdparty')
    bld.recurse('src')
    if not bld.env.IGNORE_TUTS:
        bld.recurse('tutorials')

    # install headers
    if bld.env.PREFIX != bld.env.ROOTDIR:
        bld.install_files('${PREFIX}', bld.path.ant_glob('include/**/*.h'), relative_trick=True)
        bld.install_files('${PREFIX}/share/darkhammer', bld.path.ant_glob('data/**'),
            relative_trick=True)
        if not bld.env.IGNORE_TUTS:
            bld.install_files('${PREFIX}/share/darkhammer', bld.path.ant_glob('tutorials/data/**'),
                relative_trick=True)
            bld.install_files('${PREFIX}/share/darkhammer', bld.path.ant_glob('tutorials/docs/**'),
                relative_trick=True)
