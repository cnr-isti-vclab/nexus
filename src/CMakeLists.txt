cmake_minimum_required(VERSION 3.1)
project(nexus)

set(HEADERS
	${VCGDIR}/wrap/ply/plylib.h
	common/cone.h
	common/signature.h
	common/nexusdata.h
	common/nexusfile.h
	common/qtnexusfile.h
	common/traversal.h
	common/virtualarray.h
	nxsbuild/kdtree.h
	nxsbuild/mesh.h
	nxsbuild/meshstream.h
	nxsbuild/meshloader.h
	nxsbuild/nexusbuilder.h
	nxsbuild/objloader.h
	nxsbuild/plyloader.h
	nxsbuild/stlloader.h
	nxsbuild/texpyramid.h
	nxsbuild/tmesh.h
	nxsbuild/tsploader.h
	nxsbuild/vcgloader.h
	nxsedit/extractor.h
	nxszip/bitstream.h
	nxszip/meshcoder.h
	nxszip/meshdecoder.h
	nxszip/tunstall.h
)

set(SOURCES
	${VCGDIR}/wrap/ply/plylib.cpp
	common/cone.cpp
	common/nexusdata.cpp
	common/qtnexusfile.cpp
	common/traversal.cpp
	common/virtualarray.cpp
	nxsbuild/kdtree.cpp
	nxsbuild/mesh.cpp
	nxsbuild/meshstream.cpp
	nxsbuild/meshloader.cpp
	nxsbuild/nexusbuilder.cpp
	nxsbuild/objloader.cpp
	nxsbuild/plyloader.cpp
	nxsbuild/stlloader.cpp
	nxsbuild/texpyramid.cpp
	nxsbuild/tmesh.cpp
	nxsbuild/tsploader.cpp
	nxsedit/extractor.cpp
	nxszip/abitstream.cpp
	nxszip/atunstall.cpp
	nxszip/meshcoder.cpp
	nxszip/meshdecoder.cpp
)

add_library(nexus STATIC
	${SOURCES} ${HEADERS})

target_include_directories(nexus PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(nexus PUBLIC vcglib corto Qt5::Core Qt5::Gui)

if (WIN32)
	target_compile_definitions(nexus PUBLIC NOMINMAX)
endif()
