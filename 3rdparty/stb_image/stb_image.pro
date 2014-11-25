TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= warn_on
CONFIG += warn_off

CONFIG(release, debug|release):TARGET = stb
CONFIG(debug, debug|release):TARGET = stb-dbg

SOURCES += \
    stb_image.c \
    stb_image_write.c

HEADERS += \
    stb_image.h \
    stb_image_write.h

