#-------------------------------------------------
#
# Project created by QtCreator 2012-05-29T18:43:31
#
#-------------------------------------------------

QT       += core
#QT       -= gui

TARGET = nxsedit
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

DEFINES += _FILE_OFFSET_BITS=64 TEXTURE #PARALLELOGRAM
DEFINES += _USE_MATH_DEFINES

INCLUDEPATH += ../../../vcglib \
    ../../../vcglib/eigenlib

				

win32:INCLUDEPATH += ../../../code/lib/glew/include

win32:LIBS += ../../../code/lib/glew/lib/glew32.lib opengl32.lib GLU32.lib user32.lib

SOURCES += \
    ../../../vcglib/wrap/system/qgetopt.cpp \
    ../../../vcglib/wrap/ply/plylib.cpp \
    ../common/virtualarray.cpp \
    ../common/nexusdata.cpp \
    ../common/traversal.cpp \
    ../common/cone.cpp \
    ../nxsbuild/tsploader.cpp \
    ../nxsbuild/plyloader.cpp \
    ../nxsbuild/meshstream.cpp \
    ../nxsbuild/meshloader.cpp \
    ../nxsbuild/kdtree.cpp \
    ../nxsbuild/tmesh.cpp \
    ../nxsbuild/mesh.cpp \
    ../nxsbuild/nexusbuilder.cpp \
    ../nxsbuild/objloader.cpp \
    ../nxszip/bitstream.cpp \
    ../nxszip/tunstall.cpp \
    ../nxszip/meshcoder.cpp \
    ../nxszip/meshdecoder.cpp \
    main.cpp \
    extractor.cpp

HEADERS += \
    ../../../vcglib/wrap/system/qgetopt.h \
    ../common/virtualarray.h \
    ../common/nexusdata.h \
    ../common/traversal.h \
    ../common/signature.h \
    ../nxsbuild/tsploader.h \
    ../nxsbuild/trianglesoup.h \
    ../nxsbuild/plyloader.h \
    ../nxsbuild/partition.h \
    ../nxsbuild/meshstream.h \
    ../nxsbuild/meshloader.h \
    ../nxsbuild/kdtree.h \
    ../nxsbuild/mesh.h \
    ../nxsbuild/nexusbuilder.h \
    ../nxsbuild/objloader.h \
    ../nxsbuild/tmesh.h \
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


DESTDIR = "../../bin"
