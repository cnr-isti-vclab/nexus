QT       += core gui

TARGET   = nxsedit
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

DEFINES += _FILE_OFFSET_BITS=64 TEXTURE
DEFINES += _USE_MATH_DEFINES
DEFINES += NDEBUG

INCLUDEPATH += \
    ../../../vcglib \
	../../../vcglib/eigenlib \
	../corto/include

win32:INCLUDEPATH += ../../../glew/include ../../../corto/include
win32:LIBS += opengl32.lib GLU32.lib ../../../glew/lib/glew32.lib ../../../corto/lib/corto.lib

#unix:INCLUDEPATH += /usr/local/lib
#unix:LIBS += -L /usr/local/lib -lcorto
unix:LIBS += ../corto/libcorto.a

SOURCES += \
    ../../../vcglib/wrap/system/qgetopt.cpp \
    ../../../vcglib/wrap/ply/plylib.cpp \
    ../common/virtualarray.cpp \
    ../common/nexusdata.cpp \
    ../common/traversal.cpp \
    ../common/cone.cpp \
    ../nxszip/meshcoder.cpp \
    ../nxszip/meshdecoder.cpp \
    ../nxszip/abitstream.cpp \
    ../nxszip/atunstall.cpp \
    main.cpp \
    extractor.cpp \
    ../common/qtnexusfile.cpp

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
    ../common/qtnexusfile.h

DESTDIR = "../../bin"
