TEMPLATE =
CONFIG += shared
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= warn_on

include(../../qmake-inc/version.pro)
include(../../qmake-inc/compiler.pro)

CONFIG(release, debug|release):TARGET = dhapp
CONFIG(debug, debug|release):TARGET = dhapp-dbg

INCLUDEPATH = \
    ../../libdhcore/include \
    ../../3rdparty \
    ../

unix:macx {
    LIBS += -L/usr/local/lib
    INCLUDEPATH += /usr/local/include
}

LIBS += -lassimp

SOURCES += \
    anim-import.cpp \
    texture-import.cpp \
    model-import.cpp \
    phx-import.cpp \
    h3dimport.cpp

HEADERS += \
    anim-import.h \
    h3dimport.h \
    math-conv.h \
    model-import.h \
    texture-import.h \
    phx-import.h \
    ../share/h3d-types.h \
    ../share/gfx-input-types.h

# libdhcore
CONFIG(debug, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhcore-dbg
CONFIG(release, debug|release):LIBS += -L$$OUT_PWD/../../libdhcore/src/core/ -ldhcore
DEPENDPATH += $$PWD/../../libdhcore/src/core

# ezxml
CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../3rdparty/ezxml/ -lezxml-dbg
CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../3rdparty/ezxml/ -lezxml

DEPENDPATH += $$PWD/../../3rdparty/ezxml

win32:!win32-g++    {
    CONFIG(debug, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/ezxml/ezxml-dbg.lib
    CONFIG(release, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/ezxml/ezxml.lib
}

else:unix|win32-g++ {
    CONFIG(debug, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/ezxml/libezxml-dbg.a
    CONFIG(release, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/ezxml/libezxml.a
}

# stb lib
CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../3rdparty/stb_image/ -lstb-dbg
CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../3rdparty/stb_image/ -lstb

DEPENDPATH += $$PWD/../../3rdparty/stb_image

win32:!win32-g++    {
    CONFIG(debug, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/stb_image/stb-dbg.lib
    CONFIG(release, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/stb_image/stb.lib
}

else:unix|win32-g++ {
    CONFIG(debug, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/stb_image/libstb-dbg.a
    CONFIG(release, debug|release):PRE_TARGETDEPS += $$OUT_PWD/../../3rdparty/stb_image/libstb.a
}
