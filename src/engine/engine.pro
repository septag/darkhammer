TEMPLATE = lib
CONFIG += shared
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= warn_on

include(../../qmake-inc/version.pro)
include(../../qmake-inc/compiler.pro)

CONFIG(release, debug|release):TARGET = dhapp
CONFIG(debug, debug|release):TARGET = dhapp-dbg

INCLUDEPATH = \
    ../../include \
    ../../libdhcore/include \
    ../../3rdparty \
    ../

DEFINES += _ENGINE_EXPORT_

unix {
    macx    {
        LIBS += -L/usr/local/lib
        INCLUDEPATH += /usr/local/include
    }

    DEFINES += _GL_
    macx:LIBS += -framework OpenGL
    else:LIBS += -lGL

    SOURCES += \
        gl/gfx-cmdqueue-gl.cpp \
        gl/gfx-device-gl.cpp \
        gl/gfx-shader-gl.cpp
    HEADERS += \
        ../../include/dheng/gl/gfx-types-gl.h
}

SOURCES += \
    gui.cpp \
    prf-mgr.cpp \
    anim.cpp \
    console.cpp \
    gfx-font.cpp \
    lod-scheme.cpp \
    res-mgr.cpp \
    world-mgr.cpp \
    camera.cpp \
    cmp-mgr.cpp \
    cmp-register-main.cpp \
    debug-hud.cpp \
    engine.cpp \
    phx.cpp \
    phx-prefab.cpp \
    scene-mgr.cpp \
    script.cpp \
    gfx.cpp \
    gfx-billboard.cpp \
    gfx-buffers.cpp \
    gfx-canvas.cpp \
    gfx-model.cpp \
    gfx-occ.cpp \
    gfx-postfx.cpp \
    gfx-texture.cpp \
    luabind/script-lua-core.cpp \
    luabind/script-lua-engine.cpp \
    luabind/luacore_wrap.cxx \
    luabind/luaengine_wrap.cxx \
    physx/phx-device-px.cpp \
    gl/gfx-cmdqueue-gl.cpp \
    gl/gfx-device-gl.cpp \
    gl/gfx-shader-gl.cpp \
    components/cmp-anim.cpp \
    components/cmp-animchar.cpp \
    components/cmp-attachdock.cpp \
    components/cmp-bounds.cpp \
    components/cmp-camera.cpp \
    components/cmp-light.cpp \
    components/cmp-lodmodel.cpp \
    components/cmp-model.cpp \
    components/cmp-rbody.cpp \
    components/cmp-trigger.cpp \
    components/cmp-attachment.cpp \
    components/cmp-xform.cpp \
    renderpaths/gfx-csm.cpp \
    renderpaths/gfx-deferred.cpp \
    renderpaths/gfx-fwd.cpp

HEADERS += \
    ../share/h3d-types.h \
    ../share/gfx-input-types.h \
    ../../include/dheng/anim.h \
    ../../include/dheng/cmp-mgr.h \
    ../../include/dheng/cmp-types.h \
    ../../include/dheng/console.h \
    ../../include/dheng/dds-types.h \
    ../../include/dheng/engine-api.h \
    ../../include/dheng/gfx-billboard.h \
    ../../include/dheng/gfx-buffers.h \
    ../../include/dheng/gfx-font.h \
    ../../include/dheng/gfx-occ.h \
    ../../include/dheng/gfx-shader-hashes.h \
    ../../include/dheng/gui.h \
    ../../include/dheng/lod-scheme.h \
    ../../include/dheng/mem-ids.h \
    ../../include/dheng/phx-prefab.h \
    ../../include/dheng/phx-types.h \
    ../../include/dheng/prf-mgr.h \
    ../../include/dheng/res-mgr.h \
    ../../include/dheng/scene-mgr.h \
    ../../include/dheng/world-mgr.h \
    ../../include/dheng/camera.h \
    ../../include/dheng/debug-hud.h \
    ../../include/dheng/engine.h \
    ../../include/dheng/gfx.h \
    ../../include/dheng/phx.h \
    ../../include/dheng/phx-device.h \
    ../../include/dheng/script.h \
    ../../include/dheng/gfx-canvas.h \
    ../../include/dheng/gfx-cmdqueue.h \
    ../../include/dheng/gfx-device.h \
    ../../include/dheng/gfx-model.h \
    ../../include/dheng/gfx-postfx.h \
    ../../include/dheng/gfx-shader.h \
    ../../include/dheng/gfx-texture.h \
    ../../include/dheng/gfx-types.h \
    ../../include/dheng/components/cmp-anim.h \
    ../../include/dheng/components/cmp-animchar.h \
    ../../include/dheng/components/cmp-attachdock.h \
    ../../include/dheng/components/cmp-attachment.h \
    ../../include/dheng/components/cmp-bounds.h \
    ../../include/dheng/components/cmp-camera.h \
    ../../include/dheng/components/cmp-light.h \
    ../../include/dheng/components/cmp-lodmodel.h \
    ../../include/dheng/components/cmp-model.h \
    ../../include/dheng/components/cmp-rbody.h \
    ../../include/dheng/components/cmp-trigger.h \
    ../../include/dheng/components/cmp-xform.h \
    ../../include/dheng/luabind/script-lua-common.h \
    ../../include/dheng/renderpaths/gfx-csm.h \
    ../../include/dheng/renderpaths/gfx-deferred.h \
    ../../include/dheng/renderpaths/gfx-fwd.h

OTHER_FILES += \
    luabind/luacore.i \
    luabind/luaengine.i

# libdhcore
CONFIG(debug, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhcore-dbg
CONFIG(release, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhcore
DEPENDPATH += $$PWD/../../libdhcore/src/core

# dhapp
CONFIG(debug, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhapp-dbg
CONFIG(release, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhapp
DEPENDPATH += $$PWD/../../libdhcore/src/app

# mongoose
CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../3rdparty/mongoose/ -lmongoose-dbg
CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../3rdparty/mongoose/ -lmongoose

DEPENDPATH += $$PWD/../../3rdparty/mongoose

win32:!win32-g++ {
    CONFIG(debug, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/mongoose/mongoose-dbg.lib
    CONFIG(release, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/mongoose/mongoose.lib
}

else:unix|win32-g++  {
    CONFIG(debug, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/mongoose/libmongoose-dbg.a
    CONFIG(release, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/mongoose/libmongoose.a
}
