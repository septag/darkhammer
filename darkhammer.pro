TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    libdhcore \
    3rdparty \
    src/app \
    src/engine

# Tools
SUBDIRS += \
    src/h3dimport
