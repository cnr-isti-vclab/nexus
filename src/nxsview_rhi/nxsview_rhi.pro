QT += core gui widgets

TARGET = nxsview_rhi
TEMPLATE = app

CONFIG += c++20

DEFINES += NDEBUG
unix:DEFINES -= USE_CURL
win32:DEFINES += NOMINMAX

INCLUDEPATH += \
    ../../../vcglib \
    ../../../vcglib/eigenlib \
    ../corto/include

CORTO_LIBRARY = $$absolute_path(../corto/libcorto.a, $$_PRO_FILE_PWD_)

unix:LIBS += $$CORTO_LIBRARY -pthread
win32:LIBS += ../corto/libcorto.a

SOURCES += \
    main.cpp \
    rhi_view.cpp \
    scene.cpp \
    ../common/nexusdata.cpp \
    ../common/qtnexusfile.cpp \
    ../rhi/residency_manager.cpp \
    ../rhi/traversal_engine.cpp \
    ../rhi/traversal_coordinator.cpp \
    ../rhi/streaming_coordinator.cpp

HEADERS += \
    rhi_view.h \
    scene.h \
    ../common/nexusdata.h \
    ../common/qtnexusfile.h \
    ../rhi/double_heap.h \
    ../rhi/runtime_node.h \
    ../rhi/residency_manager.h \
    ../rhi/traversal_engine.h \
    ../rhi/traversal_coordinator.h \
    ../rhi/streaming_coordinator.h

DESTDIR = "../../bin"
