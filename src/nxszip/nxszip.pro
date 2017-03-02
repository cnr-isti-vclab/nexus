CONFIG += qt
QT -= gui

HEADERS += zpoint.h \
    bytestream.h \
    range.h \
    model.h \
    mesh_coder.h \
    watch.h \
    geometry_coder.h \
    connectivity_coder.h \
    vcg_mesh.h \
    ../../../../vcglib/wrap/ply/plylib.h \
    ../../../../vcglib/wrap/system/qgetopt.h \
    ../nxsbuild/mesh.h


SOURCES += watch.cpp \
    main.cpp \
    mesh_coder.cpp \
    ../../../../vcglib/wrap/ply/plylib.cpp \
    ../../../../vcglib/wrap/system/qgetopt.cpp \
    ../nxsbuild/mesh.cpp

INCLUDEPATH += ../../../../vcglib

DEFINES += MECO_TEST

QMAKE_CXXFLAGS = -O2  -march=core2 -fno-omit-frame-pointer

