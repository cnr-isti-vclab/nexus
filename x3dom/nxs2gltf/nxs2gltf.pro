QT += core
QT += gui

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += \
	../../../vcglib \
	../../../vcglib/eigenlib

win32:INCLUDEPATH += ../../../corto/include
win32:LIBS +=  ../../../corto/lib/corto.lib

unix:INCLUDEPATH += /usr/local/lib
unix:LIBS += -L /usr/local/lib -lcorto


TARGET = nxs2gltf
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ../../src/common/nexusdata.cpp \
    ../../src/nxszip/abitstream.cpp \
    ../../src/nxszip/atunstall.cpp \
    ../../src/nxszip/meshcoder.cpp \
    ../../src/nxszip/meshdecoder.cpp

HEADERS += \
    ../../src/common/dag.h \
    ../../src/common/nexusdata.h \
    ../../src/common/signature.h \
    ../../src/nxszip/bitstream.h \
    ../../src/nxszip/bytestream.h \
    ../../src/nxszip/cstream.h \
    ../../src/nxszip/fpu_precision.h \
    ../../src/nxszip/math_class.h \
    ../../src/nxszip/meshcoder.h \
    ../../src/nxszip/meshdecoder.h \
    ../../src/nxszip/model.h \
    ../../src/nxszip/range.h \
    ../../src/nxszip/tunstall.h \
    ../../src/nxszip/vcg_mesh.h \
    ../../src/nxszip/zpoint.h \
    gltf.h \
    tiny_gltf.h

SUBDIRS += \
    ../../src/nxszip/nxszip.pro

