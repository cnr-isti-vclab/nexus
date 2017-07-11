#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsview)

include_directories(../../../vcglib ../../../vcglib/eigenlib ../../../code/lib/glew/include)


SET(HEADERS
	../../../vcglib/wrap/gcache/token.h
	../../../vcglib/wrap/gcache/provider.h
	../../../vcglib/wrap/gcache/door.h
	../../../vcglib/wrap/gcache/dheap.h
	../../../vcglib/wrap/gcache/controller.h
	../../../vcglib/wrap/gcache/cache.h
	../common/signature.h
	../common/nexus.h
	../common/cone.h
	../common/traversal.h
	../common/token.h
	../common/renderer.h
	../common/ram_cache.h
	../common/metric.h
	../common/gpu_cache.h
	../common/globalgl.h
	../common/frustum.h
	../common/dag.h
	../common/controller.h
	../common/nexusdata.h
	../nxszip/bitstream.h
	../nxszip/tunstall.h
	../nxszip/meshcoder.h
	../nxszip/cstream.h
	../nxszip/zpoint.h
	../nxszip/meshdecoder.h
	gl_nxsview.h
	scene.h
)

SET(SOURCES
	../../../code/lib/glew/src/glew.c
	../../../vcglib/wrap/gui/trackmode.cpp
	../../../vcglib/wrap/gui/trackball.cpp
	../../../vcglib/wrap/system/qgetopt.cpp
	../common/controller.cpp
	../common/nexus.cpp
	../common/cone.cpp
	../common/traversal.cpp
	../common/renderer.cpp
	../common/ram_cache.cpp
	../common/frustum.cpp
	../common/nexusdata.cpp
	../nxszip/abitstream.cpp
	../nxszip/atunstall.cpp
	../nxszip/meshdecoder.cpp
	main.cpp
	gl_nxsview.cpp
	scene.cpp
)


cmake_minimum_required(VERSION 3.1)

project(nxsview)


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt5Widgets)
find_package(Qt5OpenGL)
#find_package(glew)

add_executable(nxsview ${SOURCES} ${HEADERS})

target_link_libraries(nxsview Qt5::Widgets Qt5::OpenGL GLEW GLU GL curl)

