TEMPLATE = lib
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= warn_on

CONFIG(release, debug|release):TARGET = nvtt
CONFIG(debug, debug|release):TARGET = nvtt-dbg

linux-g++|linux-clang|macx-clang {
    QMAKE_CFLAGS += \
        -std=gnu99 \
        -msse -msse2
    QMAKE_CXXFLAGS += \
        -std=c++11

    LIBS *= -lpthread -lm -ldl -lstdc++

    INCLUDEPATH += qmake/linux-g++
}

INCLUDEPATH += . nvtt/squish

DEFINES += NVTT_EXPORTS NVTT_SHARED __SSE2__ __SSE__

SOURCES += \
    nvcore/poshlib/posh.c \
    nvcore/Debug.cpp \
    nvcore/Library.cpp \
    nvcore/Memory.cpp \
    nvcore/Radix.cpp \
    nvcore/StrLib.cpp \
    nvcore/TextReader.cpp \
    nvcore/TextWriter.cpp \
    nvmath/Basis.cpp \
    nvmath/Plane.cpp \
    nvimage/BlockDXT.cpp \
    nvimage/ColorBlock.cpp \
    nvimage/DirectDrawSurface.cpp \
    nvimage/Filter.cpp \
    nvimage/FloatImage.cpp \
    nvimage/HoleFilling.cpp \
    nvimage/Image.cpp \
    nvimage/ImageIO.cpp \
    nvimage/NormalMap.cpp \
    nvimage/Quantize.cpp \
    nvtt/squish/clusterfit.cpp \
    nvtt/squish/colourblock.cpp \
    nvtt/squish/colourfit.cpp \
    nvtt/squish/colourset.cpp \
    nvtt/squish/fastclusterfit.cpp \
    nvtt/squish/maths.cpp \
    nvtt/squish/weightedclusterfit.cpp \
    nvtt/CompressDXT.cpp \
    nvtt/CompressionOptions.cpp \
    nvtt/Compressor.cpp \
    nvtt/CompressRGB.cpp \
    nvtt/InputOptions.cpp \
    nvtt/nvtt.cpp \
    nvtt/nvtt_wrapper.cpp \
    nvtt/OptimalCompressDXT.cpp \
    nvtt/OutputOptions.cpp \
    nvtt/QuickCompressDXT.cpp \
    nvtt/cuda/CudaCompressDXT.cpp \
    nvtt/cuda/CudaUtils.cpp

HEADERS += \
    nvcore/poshlib/posh.h \
    nvcore/BitArray.h \
    nvcore/Containers.h \
    nvcore/Debug.h \
    nvcore/DefsGnucDarwin.h \
    nvcore/DefsGnucLinux.h \
    nvcore/DefsGnucWin32.h \
    nvcore/DefsVcWin32.h \
    nvcore/Library.h \
    nvcore/Memory.h \
    nvcore/nvcore.h \
    nvcore/Prefetch.h \
    nvcore/Ptr.h \
    nvcore/Radix.h \
    nvcore/StdStream.h \
    nvcore/Stream.h \
    nvcore/StrLib.h \
    nvcore/TextReader.h \
    nvcore/TextWriter.h \
    nvmath/Plane.h \
    nvimage/BlockDXT.h \
    nvimage/ColorBlock.h \
    nvimage/DirectDrawSurface.h \
    nvimage/Filter.h \
    nvimage/FloatImage.h \
    nvimage/HoleFilling.h \
    nvimage/Image.h \
    nvimage/ImageIO.h \
    nvimage/NormalMap.h \
    nvimage/nvimage.h \
    nvimage/PixelFormat.h \
    nvimage/PsdFile.h \
    nvimage/Quantize.h \
    nvimage/TgaFile.h \
    nvtt/squish/clusterfit.h \
    nvtt/squish/colourblock.h \
    nvtt/squish/colourfit.h \
    nvtt/squish/colourset.h \
    nvtt/squish/config.h \
    nvtt/squish/fastclusterfit.h \
    nvtt/squish/maths.h \
    nvtt/squish/simd.h \
    nvtt/squish/simd_3dnow.h \
    nvtt/squish/simd_sse.h \
    nvtt/squish/simd_ve.h \
    nvtt/squish/weightedclusterfit.h \
    nvtt/squish/fastclusterlookup.inl \
    nvtt/squish/singlecolourlookup.inl \
    nvtt/CompressDXT.h \
    nvtt/CompressionOptions.h \
    nvtt/Compressor.h \
    nvtt/CompressRGB.h \
    nvtt/InputOptions.h \
    nvtt/nvtt.h \
    nvtt/nvtt_wrapper.h \
    nvtt/OptimalCompressDXT.h \
    nvtt/OutputOptions.h \
    nvtt/QuickCompressDXT.h \
    nvtt/SingleColorLookup.h \
    nvtt/cuda/Bitmaps.h \
    nvtt/cuda/CudaCompressDXT.h \
    nvtt/cuda/CudaMath.h \
    nvtt/cuda/CudaUtils.h

