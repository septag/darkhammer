TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= warn_on

CONFIG(release, debug|release): TARGET=mongoose
CONFIG(debug, debug|release): TARGET=mongoose-dbg

SOURCES = mongoose.c
