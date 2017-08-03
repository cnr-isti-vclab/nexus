#-------------------------------------------------
#
# Project created by QtCreator 2012-05-25T23:10:26
#
#-------------------------------------------------

QT       += core gui opengl widgets

TARGET = nxsview
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11

DEFINES += GL_COMPATIBILITY 

unix:DEFINES += USE_CURL
win32:DEFINES += NOMINMAX

INCLUDEPATH += ../../../vcglib ../common/ ../../../vcglib/eigenlib

win32:INCLUDEPATH += /pathtocorto
win32:LIBS += -lopengl32 -lGLU32 -lcorto

unix:INCLUDEPATH += /usr/local/lib
unix:LIBS += -lGLEW -lGLU -lcurl -lcorto

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
    scene.cpp

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
    scene.h 


FORMS    += \
    nxsview.ui



DESTDIR = "../../bin"
