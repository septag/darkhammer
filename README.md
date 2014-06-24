## darkHAMMER Game Engine

Version 0.5.1-devel  
[http://www.hmrengine.com](http://www.hmrengine.com)  
[https://github.com/septag/darkhammer](https://github.com/septag/darkhammer)  
[https://github.com/septag/libdhcore](https://github.com/septag/libdhcore) - dakHAMMER Core library

### Description
*darkHAMMER* is a lightweight, open-source, multiplatform game engine. written in C (C99) language.
Supports additional scripting languages like *Python*, *C#* and *Lua*.  
Current supported platforms are Windows (Direct3D10+, OpenGL3.3+) and Linux (OpenGL3.3+) (tested on ArchLinux 3.13), MacOS support is there, but it's incomplete and have problems in some parts of graphics and build system.

### Installation

*darkHAMMER* requires some 3rdparty libraries for it's tools and engine, which are not included inside engine's repo. Some of them are already available as a package on many linux distros, some are not.  
These are the main external 3rdparty libraries that needs to be fetched from their official repos, other 3rdparty libs are included with the engine.

- *Assimp*: Asset loading library (package name: *assimp*)
- *lua*: Scripting library (package name: *lua*)
- *GLEW*: OpenGL extensions library (package name: *glew*)
- *GLFW*: OpenGL context management library
- *EFSW*: Cross-platform file-system watcher

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

Before begin to build the engine and tools, you have to build and install the 3rdparty libraries listed above, There is a tool which does this for you automatically. So start by entering following commands:

```
git clone git@github.com:septag/darkhammer.git
cd darkhammer
sudo ./install_3rdparty --prefix=/path/to/prefix
```

*install_3rdparty* tool will first search for existing 3rdparty packages, if not found, it will try to fetch them from their official repos and build them on your system. There is also an optional *--prefix* argument, which you can define where to install the libraries and their headers.  
After successful 3rdparty build, proceed with following commands:

```
# assume that we are in darkhammer's root directory
# update libdhcore from repo
git submodule init
git submodule update

# build and install libdhcore
cd libdhcore
waf configure --prefix=/path/to/prefix
waf build
sudo waf install
cd ..

# build darkhammer engine
waf configure --physx-sdk=/path/to/physx-sdk --prefix=/path/to/prefix
waf build
sudo waf install
```

*Note* that *darkHAMMER* uses *libdhcore* as a submodule, so you have to clone that one into darkHAMMER and build it separately. If you check the above commands, you will notice that I installed *libdhcore* library/header files into the same prefix for the main project, but if you want to build locally (no prefix path), then you should configure `waf configure --prefix=..` for *libdhcore* and no prefix path for the main project.  
Here's an example of building the project in local path (darkhammer's own directory) from scratch: 

```
git clone git@github.com:septag/darkhammer.git
cd darkhammer
./install_3rdparty
git submodule init
git submodule update
cd libdhcore
waf configure --prefix=..
waf build install
cd ..
waf configure --physx-sdk=/path/to/physx-3.2-SDK 
waf build install
```

After running these commands, library files will be in `lib` and binaries will be created in `bin` path.

There are also more options available, like *debug* build, enabling *asserts*, *profile* build and others, which you can see with `waf --help` command.

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

For example if you are using Visual Studio 11 and want to build 64bit binaries, run `python install_3rdparty-win.py --arch=x64 --msvc=11` command.  
After successful 3rdparty install, you can start building the engine and it's tools:  

```
waf configure --physx-sdk=/path/to/physx-sdk --prefix=/path/to/prefix
waf build
waf install
```

This is the simplest command for configuring the build, however there are many more options available, like *debug* build, enabling *asserts*, *profile* build and others, which you can see with `waf --help` command.  
*Note* that *prefix* path for 3rdparty libs and engine itself should be the same, or you may get build errors.

##### Visual studio project
I also maintain visual studio project files separately for windows developers. To build and debug with visual studio environment, you must first follow the instructions for installing *3rdparty* libraries described in the section above, and you have to successfully run *install_3rdparty-win.py* (see above).  
After successful 3rdparty install, open *msvc/darkhammer.sln* in visual studio, and choose your preferred build configuration and proceed. Before build you should consider the following notes:  

* Remember to define *Physx SDK* header and library path in your VC's search paths.
* I had to use some kind of hack to integrate default data path (*SHARE_DIR* preprocessor), into file *msvc/sharedir.h* when building with visual studio in *pre-build events*, **so** if you copy the whole project to another path on your hard-drive, make sure you delete *sharedir.h* to reset that variable, or else, data files may not be found during runtime.

### SharpHammer
For C# API and some nifty tools, check out [sharphammer](https://bitbucket.org/Amin67v/sharphammer) project, currently Amin developed a tool called *DarkMotion* which is very much influenced by the *Unity's Meckanim* tool. It is used to make and edit character animattion controllers for the engine. It currently runs on windows and requires .NET runtime, but we are planning to port it to other platforms like linux too.

### Questions ?
If you have any questions, suggestions, please post to developer's
[forum](http://hmrengine.com/forums/). If you encountered a bug you can fill an issue right here on *github*.

### Credits
#### Active developers
- *Sepehr Taghdisian*: Founder, Engine developer, [profile](https://github.com/septag)
- *Amin Valinejad*: Tools developer, founder of [sharphammer](https://bitbucket.org/Amin67v/sharphammer)

#### Contributors
- *Davide Bacchet*: Initial OSX port (davide.bacchet@gmail.com)

This software is made possible by the following open-source projects:

- *libdhcore*: darkHAMMER core library ([link](https://github.com/septag/libdhcore))
- *Assimp*: Open asset import library ([link](http://assimp.sourceforge.net/))
- *efsw*: File system monitoring library ([link](https://bitbucket.org/SpartanJ/efsw) - [my fork](https://bitbucket.org/sepul/efsw))
- *nvtt*: NVIDIA texture tools ([link](http://code.google.com/p/nvidia-texture-tools))
- *glfw*: An OpenGL library ([link](http://www.glfw.org)
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
