TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= warn_on

CONFIG(release, debug|release): TARGET=ezxml
CONFIG(debug, debug|release): TARGET=ezxml-dbg

SOURCES = ezxml.c
