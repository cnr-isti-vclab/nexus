#-------------------------------------------------
#
# Project created by QtCreator 2012-05-20T21:38:32
#
#-------------------------------------------------

QT       += core
QT       += gui 

QTPLUGIN     += qjpeg    # image formats

TARGET = nxsbuild
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11 -g

INCLUDEPATH += ../../../vcglib \
    ../../../vcglib/eigenlib

DEFINES += _FILE_OFFSET_BITS=64 TEXTURE
DEFINES += _USE_MATH_DEFINES

SOURCES += \
    ../../../vcglib/wrap/system/qgetopt.cpp \
    ../../../vcglib/wrap/ply/plylib.cpp \
    ../common/virtualarray.cpp \
    ../common/cone.cpp \
    main.cpp \
    meshstream.cpp \
    meshloader.cpp \
    plyloader.cpp \
    kdtree.cpp \
    mesh.cpp \
    tsploader.cpp \
    nexusbuilder.cpp \
    objloader.cpp \
    tmesh.cpp \
    texpyramid.cpp

win32-msvc: DEFINES += NOMINMAX

HEADERS += \
    ../../../vcglib/wrap/system/qgetopt.h \
    ../../../vcglib/wrap/ply/plylib.h \
    ../common/signature.h \
    ../common/cone.h \
    ../common/virtualarray.h \
    meshstream.h \
    meshloader.h \
    plyloader.h \
    partition.h \
    kdtree.h \
    trianglesoup.h \
    mesh.h \
    tsploader.h \
    nexusbuilder.h \
    objloader.h \
    tmesh.h \
    vertex_cache_optimizer.h \
    texpyramid.h

DESTDIR = "../../bin"

OTHER_FILES += \
    textures_plan.txt
