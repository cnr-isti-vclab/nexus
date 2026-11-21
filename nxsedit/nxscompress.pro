QT       += core gui

TARGET   = nxscompress
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++20

DEFINES += _FILE_OFFSET_BITS=64 TEXTURE
DEFINES += _USE_MATH_DEFINES

INCLUDEPATH += \
    ../../vcglib \
    ../../vcglib/eigenlib \
    ../src \
    ../src/core \
    ../src/corto/include

win32:INCLUDEPATH += ../../glew/include ../src/corto/include
win32:LIBS += opengl32.lib GLU32.lib ../../glew/lib/glew32.lib ../src/corto/lib/corto.lib

unix:INCLUDEPATH += /usr/local/lib
unix:LIBS += -L /usr/local/lib ../src/corto/libcorto.a

SOURCES += \
    ../../vcglib/wrap/system/qgetopt.cpp \
    ../../vcglib/wrap/ply/plylib.cpp \
    ../src/core/virtualarray.cpp \
    ../src/core/nexusdata.cpp \
    ../src/core/traversal.cpp \
    ../src/core/cone.cpp \
    main_compress.cpp \
    extractor.cpp \
    ../src/core/qtnexusfile.cpp

HEADERS += \
    ../../vcglib/wrap/system/qgetopt.h \
    ../src/core/virtualarray.h \
    ../src/core/nexusdata.h \
    ../src/core/traversal.h \
    ../src/core/signature.h \
    ../src/meco/zpoint.h \
    ../src/meco/model.h \
    ../src/meco/range.h \
    ../src/meco/fpu_precision.h \
    ../src/meco/bytestream.h \
    ../src/meco/math_class.h \
    ../src/meco/cstream.h \
    extractor.h \
    ../src/core/qtnexusfile.h

DESTDIR = "../bin"
