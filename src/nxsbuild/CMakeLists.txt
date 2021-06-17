#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsbuild)

set (CMAKE_CXX_STANDARD 11)

SET(HEADERS
	${VCGDIR}/wrap/system/qgetopt.h
	${VCGDIR}/wrap/ply/plylib.h
	meshstream.h
	meshloader.h
	plyloader.h
	partition.h
	kdtree.h
	trianglesoup.h
	mesh.h
	tsploader.h
	nexusbuilder.h
	objloader.h
	stlloader.h
	tmesh.h
	vertex_cache_optimizer.h
	texpyramid.h
)

SET(SOURCES
	${VCGDIR}/wrap/system/qgetopt.cpp
	${VCGDIR}/wrap/ply/plylib.cpp
	main.cpp
	meshstream.cpp
	meshloader.cpp
	plyloader.cpp
	kdtree.cpp
	mesh.cpp
	tsploader.cpp
	nexusbuilder.cpp
	objloader.cpp
	stlloader.cpp
	tmesh.cpp
	texpyramid.cpp
)

cmake_minimum_required(VERSION 3.1)

project(nxsbuild)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)

add_executable(nxsbuild ${SOURCES} ${HEADERS})

target_link_libraries(nxsbuild PUBLIC nexus)
