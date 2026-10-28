QT       += core gui openglwidgets widgets

TARGET   = nxsview
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++20

DEFINES += GL_COMPATIBILITY 
DEFINES += NDEBUG

unix:DEFINES -= USE_CURL
win32:DEFINES += NOMINMAX

INCLUDEPATH += \
    ../../../vcglib/eigenlib \
    ../corto/include

win32:INCLUDEPATH += ../../../glew/include
CORTO_LIBRARY = $$absolute_path(../corto/libcorto.a, $$_PRO_FILE_PWD_)
win32:LIBS += opengl32.lib GLU32.lib ../../../glew/lib/glew32.lib $$CORTO_LIBRARY

unix:LIBS += -lGLEW -lGLU -lGL $$CORTO_LIBRARY

SOURCES += \
    ../common/controller.cpp \
    ../common/nexus.cpp \
    ../common/cone.cpp \
    ../common/traversal.cpp \
    ../common/renderer.cpp \
    ../common/ram_cache.cpp \
    ../common/frustum.cpp \
    ../common/nexusdata.cpp \
    main.cpp \
    gl_nxsview.cpp \
    nxsview.cpp \
    scene.cpp \
    ../common/qtnexusfile.cpp

HEADERS  += \
    ../gcache/token.h \
	../gcache/provider.h \
	../gcache/door.h \
	../gcache/dheap.h \
	../gcache/controller.h \
	../gcache/cache.h \
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
    ../nxszip/cstream.h \
    ../nxszip/zpoint.h \
    gl_nxsview.h \
    nxsview.h \
    ../gui/trackball.h \
    scene.h \
    ../common/qtnexusfile.h

FORMS    += \
    nxsview.ui

DESTDIR = "../../bin"
