QT       += core gui opengl widgets

TARGET   = nxsview
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

DEFINES += GL_COMPATIBILITY 
DEFINES += NDEBUG

unix:DEFINES -= USE_CURL
win32:DEFINES += NOMINMAX

INCLUDEPATH += \
    ../../../vcglib \
    ../../../vcglib/eigenlib

win32:INCLUDEPATH += ../../../glew/include ../../../corto/include
win32:LIBS += opengl32.lib GLU32.lib ../../../glew/lib/glew32.lib ../../../corto/lib/corto.lib

unix:INCLUDEPATH += /usr/local/lib ../../../corto/include
unix:LIBS += -lGLEW -lGLU ../../../corto/libcorto.a

SOURCES += \
    ../../../vcglib/wrap/gui/trackmode.cpp \
    ../../../vcglib/wrap/gui/trackball.cpp \
    ../../../vcglib/wrap/system/qgetopt.cpp \
    ../common/controller.cpp \
    ../common/nexus.cpp \
    ../common/cone.cpp \
    ../common/traversal.cpp \
    ../common/renderer.cpp \
    ../common/ram_cache.cpp \
    ../common/frustum.cpp \
    ../common/nexusdata.cpp \
    ../nxszip/abitstream.cpp \
    ../nxszip/atunstall.cpp \
    ../nxszip/meshdecoder.cpp \
    main.cpp \
    gl_nxsview.cpp \
    scene.cpp \
    ../common/qtnexusfile.cpp

HEADERS  += \
    ../../../vcglib/wrap/gcache/token.h \
    ../../../vcglib/wrap/gcache/provider.h \
    ../../../vcglib/wrap/gcache/door.h \
    ../../../vcglib/wrap/gcache/dheap.h \
    ../../../vcglib/wrap/gcache/controller.h \
    ../../../vcglib/wrap/gcache/cache.h \
    ../common/signature.h \
    ../common/nexus.h \
    ../common/cone.h \
    ../common/traversal.h \
    ../common/token.h \
    ../common/renderer.h \
    ../common/ram_cache.h \
    ../common/metric.h \
    ../common/gpu_cache.h \
    ../common/globalgl.h \
    ../common/frustum.h \
    ../common/dag.h \
    ../common/controller.h \
    ../common/nexusdata.h \
    ../nxszip/bitstream.h \
    ../nxszip/tunstall.h \
    ../nxszip/meshcoder.h \
    ../nxszip/cstream.h \
    ../nxszip/zpoint.h \
    ../nxszip/meshdecoder.h \
    gl_nxsview.h \
    scene.h \
    ../common/qtnexusfile.h

FORMS    += \
    nxsview.ui

DESTDIR = "../../bin"
