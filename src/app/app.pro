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
    ../../libdhcore/include

DEFINES += _APP_EXPORT_

unix {
    macx    {
        LIBS += -L/usr/local/lib
        INCLUDEPATH += /usr/local/include
    }

    DEFINES += _GL_
    SOURCES += \
        gl/app-gl.cpp \
        gl/input-glfw.cpp
    HEADERS += \
        gl/glfw-keycodes.h

    macx:LIBS += -lglfw3 -framework OpenGL
    else:LIBS += -lglfw -lGL
}

SOURCES += \
    app.cpp \
    input.cpp
HEADERS += \
    ../../include/dhapp/app-api.h \
    ../../include/dhapp/init-params.h \
    ../../include/dhapp/app.h \
    ../../include/dhapp/input.h


# libdhcore
CONFIG(debug, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhcore-dbg
CONFIG(release, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhcore
DEPENDPATH += $$PWD/../../libdhcore/src/core
