#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsview)

SET(HEADERS
	${VCGDIR}/wrap/gcache/token.h
	${VCGDIR}/wrap/gcache/provider.h
	${VCGDIR}/wrap/gcache/door.h
	${VCGDIR}/wrap/gcache/dheap.h
	${VCGDIR}/wrap/gcache/controller.h
	${VCGDIR}/wrap/gcache/cache.h
	../common/nexus.h
	../common/token.h
	../common/renderer.h
	../common/ram_cache.h
	../common/metric.h
	../common/gpu_cache.h
	../common/globalgl.h
	../common/frustum.h
	../common/dag.h
	../common/controller.h
	../nxszip/meshcoder.h
	../nxszip/cstream.h
	../nxszip/zpoint.h
	gl_nxsview.h
	scene.h
)

SET(SOURCES
	${VCGDIR}/wrap/gui/trackmode.cpp
	${VCGDIR}/wrap/gui/trackball.cpp
	${VCGDIR}/wrap/system/qgetopt.cpp
	../common/controller.cpp
	../common/nexus.cpp
	../common/renderer.cpp
	../common/ram_cache.cpp
	../common/frustum.cpp
	../common/qtnexusfile.cpp
	main.cpp
	gl_nxsview.cpp
	scene.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5OpenGL)

include(${CMAKE_CURRENT_SOURCE_DIR}/glew.cmake)

add_executable(nxsview ${SOURCES} ${HEADERS})
if (WIN32)
	target_link_libraries(nxsview PUBLIC nexus Qt5::OpenGL external-glew glu32 OpenGL32 curl)
else()
	target_link_libraries(nxsview PUBLIC nexus Qt5::OpenGL external-glew GLU GL curl)
endif()

target_compile_definitions(nxsview PRIVATE GL_COMPATIBILITY)

install (TARGETS nxsview)
