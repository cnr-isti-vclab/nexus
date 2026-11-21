QT += core gui openglwidgets widgets gui-private

TARGET = nxsview
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++20
DEFINES += GL_COMPATIBILITY NDEBUG
unix:DEFINES -= USE_CURL
win32:DEFINES += NOMINMAX

INCLUDEPATH += \
    ../../vcglib \
    ../../vcglib/eigenlib \
    ../src \
    ../src/core \
    ../src/corto/include

win32:INCLUDEPATH += ../../glew/include
CORTO_LIBRARY = $$absolute_path(../src/corto/libcorto.a, $$_PRO_FILE_PWD_)
win32:LIBS += opengl32.lib GLU32.lib ../../glew/lib/glew32.lib $$CORTO_LIBRARY
unix:LIBS += -lGLEW -lGLU -lGL $$CORTO_LIBRARY -pthread

SOURCES += \
    main.cpp \
    nxsview.cpp \
    scene.cpp \
    controller.cpp \
    nexus.cpp \
    renderer.cpp \
    ram_cache.cpp \
    frustum.cpp \
    renderers/opengl/gl_nxsview.cpp \
    renderers/opengl/gl_renderer.cpp \
    renderers/qrhi/qrhi_window.cpp \
    renderers/qrhi/rhi_nxsview.cpp \
    renderers/qrhi/rhi_renderer.cpp \
    renderers/qrhi/gpu_cache_allocator.cpp \
    ../src/core/nexusdata.cpp \
    ../src/core/qtnexusfile.cpp \
    ../src/core/cone.cpp \
    ../src/core/traversal.cpp

HEADERS += \
    ../src/gcache/token.h \
    ../src/gcache/provider.h \
    ../src/gcache/door.h \
    ../src/gcache/dheap.h \
    ../src/gcache/controller.h \
    ../src/gcache/cache.h \
    ../src/core/signature.h \
    ../src/core/nexusdata.h \
    ../src/core/nexusfile.h \
    ../src/core/qtnexusfile.h \
    ../src/core/cone.h \
    ../src/core/dag.h \
    ../src/core/traversal.h \
    ../src/core/mesh_types.h \
    ../src/meco/cstream.h \
    ../src/meco/zpoint.h \
    controller.h \
    frustum.h \
    globalgl.h \
    gpu_cache.h \
    metric.h \
    nexus.h \
    nxsview.h \
    prioritizer.h \
    ram_cache.h \
    renderer.h \
    scene.h \
    token.h \
    trackball.h \
    renderers/opengl/gl_nxsview.h \
    renderers/opengl/gl_renderer.h \
    renderers/qrhi/rhi_nxsview.h \
    renderers/qrhi/rhi_renderer.h \
    renderers/qrhi/gpu_cache_allocator.h \
    renderers/qrhi/qrhi_window.h

FORMS += nxsview.ui

QSB = $$[QT_HOST_BINS]/qsb
RHI_SHADER_SOURCES = \
    renderers/qrhi/shaders/mesh.vert \
    renderers/qrhi/shaders/mesh.frag
qsb_compiler.input = RHI_SHADER_SOURCES
qsb_compiler.output = $$OUT_PWD/../bin/shaders/${QMAKE_FILE_BASE}${QMAKE_FILE_EXT}.qsb
qsb_compiler.commands = $$QMAKE_MKDIR $$shell_quote($$OUT_PWD/../bin/shaders) && $$QSB --qt6 -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
qsb_compiler.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += qsb_compiler
PRE_TARGETDEPS += \
    $$OUT_PWD/../bin/shaders/mesh.vert.qsb \
    $$OUT_PWD/../bin/shaders/mesh.frag.qsb

DESTDIR = "../bin"
