cmake_minimum_required(VERSION 3.1)
project(nexus)

set(HEADERS
	common/cone.h
	common/signature.h
	common/nexusdata.h
	common/nexusfile.h
	common/qtnexusfile.h
	common/traversal.h
	common/virtualarray.h
	nxsbuild/meshstream.h
	nxsbuild/meshloader.h
	nxsbuild/objloader.h
	nxsbuild/plyloader.h
	nxsbuild/stlloader.h
	nxsbuild/tsploader.h
	nxszip/bitstream.h
	nxszip/meshdecoder.h
	nxszip/tunstall.h
)

set(SOURCES
	common/cone.cpp
	common/nexusdata.cpp
	common/qtnexusfile.cpp
	common/traversal.cpp
	common/virtualarray.cpp
	nxsbuild/meshstream.cpp
	nxsbuild/meshloader.cpp
	nxsbuild/objloader.cpp
	nxsbuild/plyloader.cpp
	nxsbuild/stlloader.cpp
	nxsbuild/tsploader.cpp
	nxszip/abitstream.cpp
	nxszip/atunstall.cpp
	nxszip/meshdecoder.cpp
)

add_library(nexus SHARED
	${SOURCES} ${HEADERS})

target_include_directories(nexus PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/common)

target_link_libraries(nexus PUBLIC vcglib corto Qt5::Widgets)
