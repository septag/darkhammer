CONFIG -= warn_on

linux-g++|linux-clang|macx-clang   {
    QMAKE_CFLAGS += \
        -std=gnu99 \
        -msse -msse2

    QMAKE_CXXFLAGS += \
        -msse -msse2 \
        -fno-exceptions \
        -fno-rtti \
        -std=c++11

    LIBS *= -lpthread -lm

    unix:DEFINES += HAVE_ALLOCA_H
}


CONFIG(debug, debug|release) {
    DEFINES += _ENABLEASSERT_ _DEBUG_
}


win32 {
    DEFINES *= _WIN_
}

unix:macx {
    DEFINES *= _OSX_
}

unix:!macx {
    DEFINES *= _LINUX_
}
