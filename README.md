## darkHAMMER Game Engine

Version 0.5.0-devel  
[http://www.hmrengine.com](http://www.hmrengine.com)  
[https://github.com/septag/darkhammer](https://github.com/septag/darkhammer)  

### Description
*darkHAMMER* is a lightweight, open-source, multiplatform game engine. written in C (C99) language.
Supports additional scripting languages like *Python* and *Lua*.
Current supported platforms are Windows (Direct3D10+, OpenGL3.3+) and Linux (OpenGL3.3+) (tested on ArchLinux 3.13), MacOS support
is there, but it's incomplete and have problems in parts of graphics and build systems.

### Installation
#### Linux
You must have the following packages installed before trying to build:

- **git**
- **waf**: main build system
- **cmake**: for building 3rdparty libraries like *assimp*
- **gcc**
- **python**: version 3 and higher
- **wget**: for fetching non-existing 3rdparty libraries from web
- **pkg-config**: for aquiring information from installed packages
- **unzip**: for unzipping 3rdparty repos

*darkHAMMER* requires some 3rdparty libraries for it's tools and engine, which are not included inside engine's repo. Some of them are already available as a package on many linux distros, some are not.

- *Assimp*: Asset loading library (package name: *assimp*)
- *lua*: Scripting library (package name: *lua*)
- *GLEW*: OpenGL extensions library (package name: *glew*)
- *GLFWEXT*: GLFW with EXTensions, my fork of GLFW with some added functionality
- *EFSW*: Cross-platform file-system watcher

Before begin to build the engine and tools, you have to build and install the libraries listed above, There is a tool which does this for you automatically. So start by entering following commands:

```
git clone git@github.com:septag/darkhammer.git
cd darkhammer
sudo ./install_3rdparty --prefix=/path/to/prefix
```

*install_3rdparty* tool will first search for existing 3rdparty packages, if not found, it will try to fetch them from their official repos and build them on your system. There is also an optional *--prefix* argument, which you can define where to install the libraries and their headers.
After successful 3rdparty build, proceed with following commands:

```
waf configure --physx-sdk=/path/to/physx-sdk --prefix=/path/to/prefix
waf build
sudo waf install
```

This is the simplest command for configuring the build, however there are many more options available, like *debug* build, enabling *asserts*, *profile* build and others, which you can see with `waf --help` command.

#### Windows
Just like linux build, you need to acquire (or build) some external 3rdparty libraries. There is a python tool *install_3rdparty-win.py* which helps you with that process. Before using the 3rdparty installer tool, you need to install the softwares listed below:  

- **Microsoft visual studio**: preferred C/C++ compiler on windows. The project isn't tested with any other compiler on windows.
- **git for windows**: [link](http://git-scm.com/download/win) also make sure to include it's binaries into PATH environment variable.
- **wget**: [link](http://users.ugent.be/~bpuype/wget/) This is a linux tool but also is built on windows and 3rdparty installer needs it. Make sure you download and put it somewhere in PATH environment variable.
- **python**: version 3 or higher

Now start by fetching the source from the repo and install 3rdparty libraries: 

```
git clone https://github.com/septag/darkhammer.git
cd darkhammer
python install_3rdparty-win.py --arch=ARCH --msvc=MSVC_VER --prefix=/path/to/prefix
```

*--arch* and *--msvc* arguments are required, but *--prefix* argument is optional. If you don't specify *prefix* path, the libraries will be searched and installed in */path/to/darkhammer/lib* directory.  
*--arch* is the compiler platform you want to build, you have two choices, *x86* for 32bit build and *x64* for 64bit builds.  
*--msvc* is the version of the visual studio, you are building, choices are *9, 10, 11, 12*.  

After successful 3rdparty install, you can start building the engine and it's tools:  

```
waf configure --physx-sdk=/path/to/physx-sdk --prefix=/path/to/prefix
waf build
waf install
```

This is the simplest command for configuring the build, however there are many more options available, like *debug* build, enabling *asserts*, *profile* build and others, which you can see with `waf --help` command.  
*Note* that *prefix* path for 3rdparty libs and engine itself should be the same, or you may get build errors.

### Questions ?
If you have any questions, suggestions or bug reports, please post to developer's
[forum](http://hmrengine.com/forums/).

### Credits
#### Active developers
- *Sepehr Taghdisian*: Founder, Engine developer, [profile](https://github.com/septag)
- *Amin Valinejad*: Tools developer, founder of [sharphammer](https://bitbucket.org/Amin67v/sharphammer)

#### Contributors
- *Davide Bacchet*: Initial OSX port (davide.bacchet@gmail.com)

This software is made possible by the following open-source projects:

- *Assimp*: Open asset import library ([link](http://assimp.sourceforge.net/))
- *efsw*: File system monitoring library ([link](https://bitbucket.org/SpartanJ/efsw) - [my fork](https://bitbucket.org/sepul/efsw))
- *nvtt*: NVIDIA texture tools ([link](http://code.google.com/p/nvidia-texture-tools))
- *glfw*: An OpenGL library ([link](http://www.glfw.org) - [my fork](https://github.com/septag/glfw))
- *ezxml*: XML parsing library ([link](http://ezxml.sourceforge.net))
- *mongoose*: Lightweight web server ([link](https://code.google.com/p/mongoose))
- *stb_image*: Sean Barret's C image loading library ([link](http://nothings.org/stb_image.c))
- *lua*: Embedded programming language ([link](http://www.lua.org))
- *cjson*: C JSON library ([link](http://sourceforge.net/projects/cjson))
- *miniz*: mini-zlib (DEFLATE) Compression library ([link](http://code.google.com/p/miniz))
- *Qt*: Qt Application framework ([link](http://qt.digia.com))
- *PyQt4*: Qt4 Python bindings ([link](http://www.riverbankcomputing.com/software/pyqt/download))
- *SWIG*: C/C++ Binding generator tool ([link](http://www.swig.org))
- *GLEW*: GL extensions library ([link](http://glew.sourceforge.net/))
