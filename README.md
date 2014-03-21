darkHAMMER Game Engine
=======================
Version 0.5.0-devel
[http://www.hmrengine.com](http://www.hmrengine.com)  
[http://www.bitbucket.org/sepul/dark-hammer](http://www.bitbucket.org/sepul/dark-hammer)

Description
--------------------
Dark-Hammer is a light-weight, open-source, multi-platform game engine.  
Engine itself is written in C programming language (following C99 standard), works with both 
_OpenGL_ and _Direct3D_ APIs.  
Current supported platforms are _Windows 7_ with _Direct3D 10_ or higher capable graphics card, 
_Linux Ubuntu 12_ with _OpenGL 3.3_  or higher graphics card, _MacOSX 10.8 (Mountain Lion)_ with 
_OpenGL 3.2_ or higher capable graphics card.


Installation
-------------
In order to compile and build the engine you'll need softwares below, to be installed on your 
system :

- __Mercurial (Hg) client__: For pulling source from the server. [link](http://mercurial.selenic.com/)
- __Supported compilers__: Microsoft Visual studio (tested on 2010/2012), ICC on windows, GCC on linux, 
Clang on MacOS
- __Python 2.7__: Used for many tools and _waf_ build system. 
[link](http://www.python.org/download/releases/2.7/)
- __Physx 3.2 SDK__: _Physx_ is the primary physics SDK for the engine.
[link](http://supportcenteronline.com/ics/support/mylogin.asp?splash=1&deptID=1949)
- __Latest Graphics Drivers for your OS__: [nVIDIA](http://www.nvidia.com/Drivers) or
[AMD](http://support.amd.com/us/gpudownload/Pages/index.aspx) drivers.

#### Windows Requirements ####
- __DirectX SDK: For Direct3D build under windows (June2010 or win8 SDK). [link](http://www.microsoft.com/en-us/download/details.aspx?id=23549)
#### Linux Requirements ####
- __Mesa GL libs__: For GLX support (GLFW) compilation. (Package: mesa-libgl)

#### Additional (Not required) ####
- __SWIG__: For rebuilding/modifying lua or python bindings. [link](http://www.swig.org)
- __Python development libs__: For linux (Package: _python-dev_)
- __PyQt4 for python 2.7__: Used in many GUI tools. (Required for python tools) 
[link](http://www.riverbankcomputing.com/software/pyqt/download)

- - -
Preferred build system is [waf](http://code.google.com/p/waf/) which is a python based build system, 
and is included in the source repo under _/path/to/dark-hammer/var/waf/_ directory.  
To build with _waf_, make sure you have installed all above requirements, open up a command terminal 
, go to your desired directory and run these commands:

```
hg clone https://bitbucket.org/sepul/dark-hammer
cd dark-hammer
python var/waf/waf configure --physx_sdk=/path/to/physx_sdk  
python var/waf/waf  
python var/waf/waf install  
```

By default, these set of commands will find an appropreate compiler and build the code in _release_ 
mode, then install the shared libraries, binaries in _dark-hammer/bin_ and static libraries in 
_dark-hammer/lib_ directory. You should also provide a valid path to _Physx SDK_ root directory in 
_--physx\_sdk_ argument.  

#### Debug build
For debug builds (which output files will have _-dbg_ suffix), just add _\_debug_ to build and 
install commands:

```
python var/waf/waf build_debug
python var/waf/waf install_debug  
```

#### Python module build
To build python module, add _\_pymodule_ to build and install commands:  

```
python var/waf/waf build_pymodule
python var/waf/waf install_pymodule
```

This will build and install platform dependent engine python module files under 
_[dark-hammer]/src/pymodules_ directory.  
__Note__ that you must install python development files for your OS, before building python module.

#### Additional Flags
These arguments can be used with `configure` command:  

- `--dx_sdk=PATH`: For __windows__ and __direct3d__ builds, If this argument is empty, compiler will 
use default compiler paths (for ex. Windows sdk) to locate DirectX libraries and headers. 
If you want to provide directx sdk manually, you should provide this argument as example:  
    `python var/waf/waf configure --physx_sdk=/path/to/physx_sdk --dx_sdk=/path/to/dx_sdk`

- If you want to change compilers, for example use 32bit ICC compiler on windows:  
    `python var/waf/waf configure --physx_sdk=/path/to/physx_sdk --dx_sdk=/path/to/dx_sdk`  
    For more information and flags check out _waf_'s documentation

- `--prefix=PREFIX_PATH`: PREFIX_PATH can be a valid path which you want to install the build files, 
instead of default path.

- `--assert`: Enables assertions in the build, recommended for _debug_ builds.
- `--retail`: Enables _retail_ mode build, enables all retail optimizations.
- `--prompt`: Prompts for sdk paths, if not provided by user in command line.
- `--gfxdevice=[D3D|GL]`: Choose a valid rendering device (D3D or GL).
- `--physx_sdk=PATH`: Path to _Physx SDK_ root directory (required).
- `--profile`: Enables profile build, which engine stores profiling information in runtime and can 
be connected via a browser through port 8888 for profiling information.

#### Visual Studio
Additional MSVC2012 projects are maintained separately and included in 
`/path/to/dark-hammer/src/msvc` (for main builds) and `/path/to/dark-hammer/3rdparty/msvc` 
(for 3rdparty library builds).  
First you have to open the solution file for _3rdparty_ libraries and build them successfully. Then 
open main solution file and build those.  
To build python modules, select *Release_pybind* from configurations and build. This will also copy the module files from the bin directory to */src/pymodules*.

#### Media files
To run the test program _(game-test)_, you should download [media files](http://www.hmrengine.com/pages/media/) for this build, 
and copy them into _dark-hammer/test-data_ directory.
 
Issues
------
Some current known issues (help is very much appreciated):
  
- Overriding window management doesn't work for Non-windows versions yet _(app_init with wnd_override)_,
 This is (in part) because of _glfw_ design flaw in decoupling window and context management.
- OpenGL performance suffers, comparing to Direct3D implementation
- Shadows in OSX (OpenGL 3.2) version is not working properly, not tested with Mavericks (GL4.2) 

Questions?
----------
If you have any questions, suggestions or bug reports, please post to developer's 
[forum](http://hmrengine.com/forums/).

Credits
-------
- __Founder/Engine Developer:__ _Sepehr Taghdisian (sep.tagh@gmail.com)_
- __Tools Developer:__ _Amin Valinejad (amin67v@hotmail), [sharphammer](https://bitbucket.org/Amin67v/sharphammer)_
- __MacOSX port:__ _Davide Bacchet (davide.bacchet@gmail.com)_

This software is made possible by the following open-source projects:
  
- *Assimp*: Open asset import library ([link](http://assimp.sourceforge.net/))
- *efsw*: File system monitoring library ([link (my fork)](https://bitbucket.org/sepul/efsw))
- *nvtt*: NVIDIA texture tools ([link](http://code.google.com/p/nvidia-texture-tools))
- *glfw*: An OpenGL library ([link](http://www.glfw.org)) ([link - my fork)[https://github.com/septag/glfw])
- *ezxml*: XML parsing library ([link](http://ezxml.sourceforge.net))
- *mongoose*: Lightweight web server ([link](https://code.google.com/p/mongoose))
- *stb_image*: Sean Barret's C image loading library ([link](http://nothings.org/stb_image.c))
- *lua*: Embedded programming language ([link](http://www.lua.org))
- *gl3w*: Simple OpenGL profile loading library ([link](https://github.com/skaslev/gl3w))
- *cjson*: C JSON library ([link](http://sourceforge.net/projects/cjson))
- *sfmt*: SIMD - Mersenne twister random generator library ([link](http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT))
- *miniz*: mini-zlib (DEFLATE) Compression library ([link](http://code.google.com/p/miniz))
- *Qt*: Qt Application framework ([link](http://qt.digia.com))
- *PyQt4*: Qt4 Python bindings ([link](http://www.riverbankcomputing.com/software/pyqt/download))
- *SWIG*: C/C++ Binding generator tool ([link](http://www.swig.org))

