#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsview)

set (CMAKE_CXX_STANDARD 11)

add_definitions( -DGL_COMPATIBILITY )

SET(HEADERS
	${VCGDIR}/wrap/gcache/token.h
	${VCGDIR}/wrap/gcache/provider.h
	${VCGDIR}/wrap/gcache/door.h
	${VCGDIR}/wrap/gcache/dheap.h
	${VCGDIR}/wrap/gcache/controller.h
	${VCGDIR}/wrap/gcache/cache.h
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
	../common/nexusfile.h
	../common/qtnexusfile.h
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
	${VCGDIR}/wrap/gui/trackmode.cpp
	${VCGDIR}/wrap/gui/trackball.cpp
	${VCGDIR}/wrap/system/qgetopt.cpp
	../common/controller.cpp
	../common/nexus.cpp
	../common/cone.cpp
	../common/traversal.cpp
	../common/renderer.cpp
	../common/ram_cache.cpp
	../common/frustum.cpp
	../common/nexusdata.cpp
	../common/qtnexusfile.cpp
	../nxszip/abitstream.cpp
	../nxszip/atunstall.cpp
	../nxszip/meshdecoder.cpp
	main.cpp
	gl_nxsview.cpp
	scene.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_definitions( -DGL_COMPATIBILITY )

find_package(Qt5Widgets)
find_package(Qt5OpenGL)
#find_package(glew)

add_executable(nxsview ${SOURCES} ${HEADERS})
if (WIN32)
	target_link_libraries(nxsview vcglib corto Qt5::Widgets Qt5::OpenGL GLEW glu32 OpenGL32 curl)
else()
	target_link_libraries(nxsview vcglib corto Qt5::Widgets Qt5::OpenGL GLEW GLU GL curl)
endif()

