#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (save for bin/qmake)

cmake_minimum_required(VERSION 3.1)
project(nxsbuild)


# replace with ../../../corto/ and ../../../corto/src/ if you didn't install
#set (CORTOLIBDIR /usr/local/lib)
#set (CORTOLIBINCLUDE /usr/local/lib/include)

include_directories(../../../vcglib ../../../vcglib/eigenlib CORTOLIBINCLUDE)

set (CMAKE_CXX_STANDARD 11)

SET(HEADERS
	../../../vcglib/wrap/system/qgetopt.h
	../../../vcglib/wrap/ply/plylib.h
	../common/signature.h
	../common/cone.h
	../common/virtualarray.h
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
	tmesh.h
	vertex_cache_optimizer.h
	texpyramid.h
)

SET(SOURCES
	../../../vcglib/wrap/system/qgetopt.cpp
	../../../vcglib/wrap/ply/plylib.cpp
	../common/virtualarray.cpp
	../common/cone.cpp
	main.cpp
	meshstream.cpp
	meshloader.cpp
	plyloader.cpp
	kdtree.cpp
	mesh.cpp
	tsploader.cpp
	nexusbuilder.cpp
	objloader.cpp
	tmesh.cpp
	texpyramid.cpp
)

cmake_minimum_required(VERSION 3.1)

project(nxsbuild)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)

add_executable(nxsbuild ${SOURCES} ${HEADERS})

link_directories(/usr/local/lib)
target_link_libraries(nxsbuild Qt5::Widgets corto)





