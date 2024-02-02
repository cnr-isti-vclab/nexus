QT       += core gui

TARGET   = nxsbuild
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11 -g -fpermissive

INCLUDEPATH += \
    ../../../vcglib \
    ../../../vcglib/eigenlib

DEFINES += _FILE_OFFSET_BITS=64
DEFINES += _USE_MATH_DEFINES

win32-msvc: DEFINES += NOMINMAX

SOURCES += \
    ../../../vcglib/wrap/system/qgetopt.cpp \
    ../../../vcglib/wrap/ply/plylib.cpp \
    ../common/virtualarray.cpp \
    ../common/cone.cpp \
    b3dm.cpp \
    gltfbuilder.cpp \
    main.cpp \
    meshstream.cpp \
    meshloader.cpp \
    plyloader.cpp \
    kdtree.cpp \
    mesh.cpp \
    tileset.cpp \
    tsploader.cpp \
    nexusbuilder.cpp \
    objloader.cpp \
    tmesh.cpp \
    texpyramid.cpp \
    stlloader.cpp

HEADERS += \
    ../../../vcglib/wrap/system/qgetopt.h \
    ../../../vcglib/wrap/ply/plylib.h \
    ../common/signature.h \
    ../common/cone.h \
    ../common/virtualarray.h \
    b3dm.h \
    gltfbuilder.h \
    meshstream.h \
    meshloader.h \
    plyloader.h \
    partition.h \
    kdtree.h \
    tileset.h \
    trianglesoup.h \
    mesh.h \
    tsploader.h \
    nexusbuilder.h \
    objloader.h \
    tmesh.h \
    vertex_cache_optimizer.h \
    texpyramid.h \
    stlloader.h \
    vcgloader.h \
    vcgloadermesh.h \
    vcgloadermesh.h \
    deps/tiny_gltf.h \
    deps/stb_image_write.h \
    deps/stb_image.h \
    deps/json.hpp \


DESTDIR = "../../bin"

OTHER_FILES += \
    textures_plan.txt
