#!/usr/bin/env python

import os, sys, subprocess, inspect, shutil, glob, optparse

ROOTDIR = os.path.abspath(os.path.dirname(inspect.getframeinfo(inspect.currentframe())[0]))
WAFPATH = os.path.join(ROOTDIR, 'var', 'waf')
LIBDIR = ''
INCLUDEDIR = ''
BINDIR = ''
PREFIX = ROOTDIR
ARCH = ''
MSVC = ''

def log(msg):
    sys.stdout.write(msg)
    sys.stdout.flush()
    
def get_msvctarget():
    global ARCH
    vctarget = ''
    if ARCH == 'x64':   vctarget='x86_amd64'    # use cross-compiler for compatibility
    elif ARCH == 'x86': vctarget='x86'
    return vctarget
    
def get_msvccompiler():
    global MSVC
    compilers = {'9': 'msvc 9.0', '10': 'msvc 10.0', '11': 'msvc 11.0', '12': 'msvc 12.0'}
    return compilers[MSVC]

def install_lua():
    lua_srcfiles = {\
        'x86-vc12': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win32_dll12_lib.zip/download',
        'x64-vc12': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win64_dll12_lib.zip/download',
        'x64-vc11': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win64_dll11_lib.zip/download',
        'x86-vc11': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win32_dll11_lib.zip/download',
        'x86-vc9': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win32_dll9_lib.zip/download',
        'x64-vc9': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win64_dll9_lib.zip/download',
        'x64-vc10': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win64_dll10_lib.zip/download',
        'x86-vc10': 'http://sourceforge.net/projects/luabinaries/files/5.2.1/Windows%20Libraries/Dynamic/lua-5.2.1_Win32_dll10_lib.zip/download'
        }
    
    luadir = os.path.join(ROOTDIR, '3rdparty', 'tmp', 'lua')
    libfile = os.path.join(LIBDIR, 'lua.lib')
    log('looking for lua...')
    if os.path.isfile(libfile):
        log('\t\tfound\n')
        return True
    log('\t\tnot found\n')        
    
    url = lua_srcfiles[ARCH + '-vc' + MSVC]
    log('downloading lua binaries from "http://sourceforge.net/projects/luabinaries"...\n')
    log('')

    os.makedirs(luadir, exist_ok=True)
    os.chdir(luadir)
    if os.system('wget -N --no-check-certificate {0}'.format(url)) != 0:
        os.chdir(ROOTDIR)
        return False
        
    # extract file name from url
    urlsplit = url.split('/')
    filename = ''
    for u in urlsplit:
        if '.zip' in u:
            filename = u
            break
            
    if os.system('unzip -o ' + filename) != 0:
        os.chdir(ROOTDIR)
        return False

    # copy important files
    shutil.copyfile('lua52.dll', os.path.join(BINDIR, 'lua52.dll'))
    shutil.copyfile('lua52.lib', os.path.join(LIBDIR, 'lua.lib'))

    # headers
    includes = os.path.join(INCLUDEDIR, 'lua')
    headers = glob.glob('include/*.h')
    os.makedirs(includes, exist_ok=True)
    for header in headers:
        shutil.copyfile(header, os.path.join(includes, os.path.basename(header)))

    os.chdir(ROOTDIR)
    return True

def install_assimp():
    log('looking for assimp...')
    if os.path.isfile(os.path.join(LIBDIR, 'assimp.lib')):
        log('\t\tfound\n')
        return True
    log('\t\tnot found\n')

    url = 'http://sourceforge.net/projects/assimp/files/assimp-3.1/assimp-3.1.1-win-binaries.zip/download'
    log('downloading assimp binaries from "http://sourceforge.net/projects/assimp"...\n')

    assimpdir = os.path.join(ROOTDIR, '3rdparty', 'tmp', 'assimp')
    os.makedirs(assimpdir, exist_ok=True)
    os.chdir(assimpdir)
    if os.system('wget -N --no-check-certificate {0}'.format(url)) != 0:
        os.chdir(ROOTDIR)
        return False
        
    # extract file name from url
    urlsplit = url.split('/')
    filename = ''
    for u in urlsplit:
        if '.zip' in u:
            filename = u
            break
            
    if os.system('unzip -o ' + filename) != 0:
        os.chdir(ROOTDIR)
        return False
    
    os.chdir('assimp-3.1.1-win-binaries')

    # copy important files
    # libs
    bindirs = {'x64': 'bin64', 'x86': 'bin32'}
    libdirs = {'x64': 'lib64', 'x64': 'lib32'}
    dlls = glob.glob(os.path.join(bindirs[ARCH], '*.dll'))
    libs = glob.glob(os.path.join(libdirs[ARCH], '*.lib'))
    print()
    for lib in libs:
        shutil.copyfile(lib, os.path.join(LIBDIR, os.path.basename(lib)))
    for dll in dlls:
        shutil.copyfile(dll, os.path.join(BINDIR, os.path.basename(dll)))
    
    # headers
    includes = os.path.join(INCLUDEDIR, 'assimp')
    headers = glob.glob('include/assimp/*')
    os.makedirs(includes, exist_ok=True)
    for header in headers:
        if os.path.isfile(header):
            shutil.copyfile(header, os.path.join(includes, os.path.basename(header)))
    os.makedirs(os.path.join(includes, 'Compiler'), exist_ok=True)
    headers = glob.glob('include/assimp/Compiler/*')
    for header in headers:
        shutil.copyfile(header, os.path.join(includes, 'Compiler', os.path.basename(header)))

    os.chdir(ROOTDIR)
    return True

def install_glfw():
    log('looking for glfw...')
    if os.path.isfile(os.path.join(LIBDIR, 'glfw.lib')) and \
        os.path.isfile(os.path.join(BINDIR, 'glfw3.dll')):
        log('\t\tfound\n')
        return True
    log('\t\tnot found\n')
    
    urls = {\
        'x86': 'http://sourceforge.net/projects/glfw/files/glfw/3.0.4/glfw-3.0.4.bin.WIN32.zip/download',
        'x64': 'http://sourceforge.net/projects/glfw/files/glfw/3.0.4/glfw-3.0.4.bin.WIN64.zip/download'}
    log('downloading glfw binaries from "http://www.glfw.org"...\n')

    glfwdir = os.path.join(ROOTDIR, '3rdparty', 'tmp', 'glfw')
    os.makedirs(glfwdir, exist_ok=True)
    os.chdir(glfwdir)
    if os.system('wget -N --no-check-certificate {0}'.format(urls[ARCH])) != 0:
        os.chdir(ROOTDIR)
        return False

        # extract file name from url
    urlsplit = urls[ARCH].split('/')
    filename = ''
    for u in urlsplit:
        if '.zip' in u:
            filename = u
            break        

    if os.system('unzip -o %s' % filename) != 0:
        os.chdir(ROOTDIR)
        return False
        
    dirname = os.path.splitext(filename)[0]
    os.chdir(dirname)    
    
    # copy important files
    # libs
    shutil.copyfile('lib-msvc%s0/glfw3.dll' % MSVC, os.path.join(BINDIR, 'glfw3.dll'))
    shutil.copyfile('lib-msvc%s0/glfw3dll.lib' % MSVC, os.path.join(LIBDIR, 'glfw.lib'))

    # headers
    includes = os.path.join(INCLUDEDIR, 'GLFW')
    headers = glob.glob('include/GLFW/*.h')
    os.makedirs(includes, exist_ok=True)
    for header in headers:
        shutil.copyfile(header, os.path.join(includes, os.path.basename(header)))

    os.chdir(ROOTDIR)
    return True

def install_glew():
    log('looking for glew...')
    if os.path.isfile(os.path.join(LIBDIR, 'glew.lib')):
        log('\t\tfound\n')
        return True
    log('\t\tnot found\n')

    url = 'https://sourceforge.net/projects/glew/files/glew/1.10.0/glew-1.10.0-win32.zip/download'
    log('downloading glew binaries from "https://sourceforge.net/projects/glew"...\n')

    glewdir = os.path.join(ROOTDIR, '3rdparty', 'tmp', 'glew')
    os.makedirs(glewdir, exist_ok=True)
    os.chdir(glewdir)
    if os.system('wget -N --no-check-certificate {0}'.format(url)) != 0:
        os.chdir(ROOTDIR)
        return False
        
    # extract file name from url
    urlsplit = url.split('/')
    filename = ''
    for u in urlsplit:
        if '.zip' in u:
            filename = u
            break        
    if os.system('unzip -o ' + filename) != 0:
        os.chdir(ROOTDIR)
        return False

    dirs = glob.glob('*')
    for d in dirs:
        if os.path.isdir(d):
            os.chdir(d)
            break

    # copy important files
    dirs = {'x64': 'x64', 'x86': 'Win32'}
    d = dirs[ARCH]
    # libs    
    shutil.copyfile(os.path.join('bin', 'Release', d, 'glew32.dll'), 
        os.path.join(BINDIR, 'glew32.dll'))
    shutil.copyfile(os.path.join('lib', 'Release', d, 'glew32.lib'), 
        os.path.join(LIBDIR, 'glew.lib'))
        
    # headers
    includes = os.path.join(INCLUDEDIR, 'GL')
    headers = glob.glob('include/GL/*.h')
    os.makedirs(includes, exist_ok=True)
    for header in headers:
        shutil.copyfile(header, os.path.join(includes, os.path.basename(header)))

    os.chdir(ROOTDIR)
    return True

def install_efsw():
    log('looking for efsw...')
    if os.path.isfile(os.path.join(LIBDIR, 'efsw.lib')) and \
        os.path.isfile(os.path.join(BINDIR, 'efsw.dll')):
        log('\t\tfound\n')
        return True
    log('\t\tnot found\n')

    url = 'https://bitbucket.org/sepul/efsw/get/5de4baca1a60.zip'
    log('downloading efsw source from "https://bitbucket.org/sepul/efsw"...\n')

    efswdir = os.path.join(ROOTDIR, '3rdparty', 'tmp', 'efsw')
    os.makedirs(efswdir, exist_ok=True)
    os.chdir(efswdir)
    if os.system('wget -N --no-check-certificate {0}'.format(url)) != 0:
        os.chdir(ROOTDIR)
        return False
    if os.system('unzip -o ' + os.path.basename(url)) != 0:
        os.chdir(ROOTDIR)
        return False
    
    name = os.path.splitext(os.path.basename(url))[0]
    dirname = 'sepul-efsw-' + name
    os.chdir(dirname)
    
    if os.system('python {0} configure --msvc_version="{1}" --msvc_targets={2}'.format(\
        WAFPATH, get_msvccompiler(), ARCH)) != 0:
        if os.system('python {0} configure --msvc_version="{1}" --msvc_targets={2}'.format(\
            WAFPATH, get_msvccompiler(), get_msvctarget())) != 0:
            os.chdir(ROOTDIR)
            return False
        
    if os.system('python {0} build install'.format(WAFPATH)) != 0:
        os.chdir(ROOTDIR)
        return False
        
    # copy important files
    # libs
    shutil.copyfile('bin/efsw.dll', os.path.join(BINDIR, 'efsw.dll'))
    shutil.copyfile('build/release/efsw.lib', os.path.join(LIBDIR, 'efsw.lib'))

    # headers
    includes = os.path.join(INCLUDEDIR, 'efsw')
    headers = glob.glob('include/efsw/*.h*')
    os.makedirs(includes, exist_ok=True)
    for header in headers:
        shutil.copyfile(header, os.path.join(includes, os.path.basename(header)))

    os.chdir(ROOTDIR)
    return True

def main():
    parser = optparse.OptionParser()
    parser.add_option('--prefix', action='store', type='string', dest='PREFIX',
        help='prefix path for existing and to be installed libs', default='')
    parser.add_option('--msvc', action='store', type='choice', choices=['9', '10', '11', '12'], 
        dest='MSVC', help='define visual studio version (active compiler)')
    parser.add_option('--arch', action='store', type='choice', choices=['x86', 'x64'], 
        dest='ARCH', help='define target architecture that you want to build')
        
    (options, args) = parser.parse_args()
    if not options.ARCH:
        parser.error('--arch argument is not given')
    if not options.MSVC:
        parser.error('--msvc argument is not given')
    
    global LIBDIR, INCLUDEDIR, PREFIX, MSVC, ARCH, BINDIR
    PREFIX = os.path.abspath(options.PREFIX)
    LIBDIR = os.path.join(PREFIX, 'lib')
    INCLUDEDIR = os.path.join(PREFIX, 'include')
    BINDIR = os.path.join(PREFIX, 'bin')
    ARCH = options.ARCH
    MSVC = options.MSVC

    log('library install path: ' + LIBDIR + '\n')
    log('library install path: ' + BINDIR + '\n')
    log('include install path: ' + INCLUDEDIR + '\n')

    os.makedirs(INCLUDEDIR, exist_ok=True)
    os.makedirs(LIBDIR, exist_ok=True)
    os.makedirs(BINDIR, exist_ok=True)

    if not install_lua():
        log('error: could not install lua\n')
        return False
    
    if not install_assimp():
        log('error: could not install assimp\n')
        return False

    if not install_glfw():
        log('error: could not install glfw\n')
        return False

    if not install_glew():
        log('error: could not install glew\n')
        return False

    if not install_efsw():
        log('error: could not install efsw\n')
        return False
    
    log('ok, ready for build.\n')

r = main()
if r:    sys.exit(0)
else:    sys.exit(-1)