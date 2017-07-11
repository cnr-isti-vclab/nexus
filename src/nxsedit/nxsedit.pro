QT       += core

TARGET = nxsedit
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

DEFINES += USE_CORTO

DEFINES += _FILE_OFFSET_BITS=64 TEXTURE
DEFINES += _USE_MATH_DEFINES

INCLUDEPATH += ../../../vcglib ../../../vcglib/eigenlib

#    ../../../corto/src
#LIBS += ../../../corto/lib/libcorto.a

win32:INCLUDEPATH += ../../../code/lib/glew/include

win32:LIBS += ../../../code/lib/glew/lib/glew32.lib opengl32.lib GLU32.lib user32.lib

SOURCES += \
    ../../../vcglib/wrap/system/qgetopt.cpp \
    ../../../vcglib/wrap/ply/plylib.cpp \
    ../common/virtualarray.cpp \
    ../common/nexusdata.cpp \
    ../common/traversal.cpp \
    ../common/cone.cpp \
    ../nxszip/meshcoder.cpp \
    ../nxszip/meshdecoder.cpp \
    main.cpp \
    extractor.cpp \
    ../../../corto/src/bitstream.cpp \
    ../../../corto/src/color_attribute.cpp \
    ../../../corto/src/cstream.cpp \
    ../../../corto/src/decoder.cpp \
    ../../../corto/src/encoder.cpp \
    ../../../corto/src/normal_attribute.cpp \
    ../../../corto/src/tunstall.cpp \
    ../nxszip/abitstream.cpp \
    ../nxszip/atunstall.cpp

HEADERS += \
    ../../../vcglib/wrap/system/qgetopt.h \
    ../common/virtualarray.h \
    ../common/nexusdata.h \
    ../common/traversal.h \
    ../common/signature.h \
    ../nxszip/zpoint.h \
    ../nxszip/model.h \
    ../nxszip/range.h \
    ../nxszip/fpu_precision.h \
    ../nxszip/bytestream.h \
    ../nxszip/math_class.h \
    ../nxszip/bitstream.h \
    ../nxszip/tunstall.h \
    ../nxszip/cstream.h \
    ../nxszip/meshcoder.h \
    ../nxszip/meshdecoder.h \
    extractor.h \
    ../../../corto/src/bitstream.h \
    ../../../corto/src/color_attribute.h \
    ../../../corto/src/corto.h \
    ../../../corto/src/cstream.h \
    ../../../corto/src/decoder.h \
    ../../../corto/src/encoder.h \
    ../../../corto/src/index_attribute.h \
    ../../../corto/src/normal_attribute.h \
    ../../../corto/src/tunstall.h \
    ../../../corto/src/vertex_attribute.h


DESTDIR = "../../bin"
