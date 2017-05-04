#-------------------------------------------------
#
# Project created by QtCreator 2012-05-25T23:10:26
#
#-------------------------------------------------

QT       += core gui opengl widgets

TARGET = nxsview
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

DEFINES += GL_COMPATIBILITY USE_CORTO
unix:DEFINES += USE_CURL
win32:DEFINES += NOMINMAX

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
    main.cpp\
    gl_nxsview.cpp \
    scene.cpp \
    ../../../corto/src/bitstream.cpp \
    ../../../corto/src/color_attribute.cpp \
    ../../../corto/src/cstream.cpp \
    ../../../corto/src/decoder.cpp \
    ../../../corto/src/tunstall.cpp \
    ../../../corto/src/normal_attribute.cpp

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
    ../../../corto/src/bitstream.h \
    ../../../corto/src/color_attribute.h \
    ../../../corto/src/cstream.h \
    ../../../corto/src/decoder.h \
    ../../../corto/src/tunstall.h \
    ../../../corto/src/vertex_attribute.h \
    ../../../corto/src/normal_attribute.h


FORMS    += \
    nxsview.ui

INCLUDEPATH += ../../../vcglib ../common/ ../../../vcglib/eigenlib
			  
win32-msvc2013:  LIBS += -lopengl32 -lGLU32
win32-msvc2015:  LIBS += -lopengl32 -lGLU32

unix:LIBS += -lGLEW -lGLU -lcurl

DESTDIR = "../../bin"
