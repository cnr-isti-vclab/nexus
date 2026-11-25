cmake_minimum_required(VERSION 3.10)
project(nxsbuild)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_VCG_SIMPLIFIER "Use the VCG mesh simplifier instead of Trueform" OFF)

set(SOURCES
	main.cpp
	build_parameters.cpp
	merge_simplify_split.cpp
	simplify_true.cpp
	simplify_edge.cpp
	simplify_vcg.cpp
	export_nxs.cpp
	adjacency.cpp
	clustering.cpp
	mesh_hierarchy.cpp
	micro_clustering.cpp
	spatial_sort.cpp
	vcgmesh.cpp
	texture/parametrization.cpp
	texture/xatlas.cpp
	texture/pyramid.cpp
	texture/texture.cpp
	texture/rasterizer.cpp
	texture/pushpull.cpp
	texture/texture_cache.cpp
	texture/uniform_grid.cpp
	texture/jpeg_decoder.cpp
	texture/png_decoder.cpp
	../src/loaders/meshloader.cpp
	../src/loaders/gltfloader.cpp
	../src/loaders/objloader.cpp
	../src/loaders/objexporter.cpp
	../src/loaders/plyloader.cpp
	../src/loaders/plyexporter.cpp
)

set(HEADERS
	build_parameters.h
	buildmaterial.h
	merge_simplify_split.h
	export_nxs.h
	adjacency.h
	clustering.h
	mesh_hierarchy.h
	micro_clustering.h
	spatial_sort.h
	vcgmesh.h
	texture/parametrization.h
	texture/xatlas.h
	texture/pyramid.h
	texture/texture.h
	texture/rasterizer.h
	texture/pushpull.h
	texture/texture_cache.h
	texture/uniform_grid.h
	texture/jpeg_decoder.h
	texture/png_decoder.h
	../src/core/mesh_types.h
	../src/core/basemesh.h
	../src/core/mapped_file.h
	../src/core/mappedmesh.h
	../src/core/material.h
	../src/core/nodemesh.h
	../src/loaders/meshloader.h
	../src/loaders/gltfloader.h
	../src/loaders/objloader.h
	../src/loaders/objexporter.h
	../src/loaders/plyloader.h
	../src/loaders/plyexporter.h
)

add_executable(nxsbuild ${SOURCES} ${HEADERS}
	"${VCGDIR}/wrap/ply/plylib.cpp"
	"${VCGDIR}/wrap/ply/plylib.h"
)

find_package(Qt6 COMPONENTS Core Gui REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)

set(METIS_ROOT "$ENV{METIS_ROOT}")
if(METIS_ROOT)
	find_path(METIS_INCLUDE_DIR NAMES metis.h HINTS "${METIS_ROOT}/include")
	find_library(METIS_LIBRARY NAMES metis HINTS "${METIS_ROOT}/lib")
else()
	find_path(METIS_INCLUDE_DIR NAMES metis.h)
	find_library(METIS_LIBRARY NAMES metis)
endif()
if(NOT METIS_INCLUDE_DIR OR NOT METIS_LIBRARY)
	message(FATAL_ERROR "METIS headers and library were not found.")
endif()

set(DRACO_ROOT "$ENV{DRACO_ROOT}")
if(DRACO_ROOT)
	find_path(DRACO_INCLUDE_DIR NAMES draco/compression/decode.h HINTS "${DRACO_ROOT}/include")
	find_library(DRACO_LIBRARY NAMES draco HINTS "${DRACO_ROOT}/lib")
else()
	find_path(DRACO_INCLUDE_DIR NAMES draco/compression/decode.h)
	find_library(DRACO_LIBRARY NAMES draco)
endif()
if(NOT DRACO_INCLUDE_DIR OR NOT DRACO_LIBRARY)
	message(FATAL_ERROR "Draco headers and library were not found.")
endif()

include(FetchContent)

FetchContent_Declare(
	trueform
	GIT_REPOSITORY https://github.com/polydera/trueform.git
	GIT_TAG main
)

FetchContent_MakeAvailable(trueform)

target_include_directories(nxsbuild PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../src"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/core"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/loaders"
	"${VCGDIR}"
	"${VCGDIR}/eigenlib"
	/usr/include/eigen3
	/home/ponchio/local/include
	"${METIS_INCLUDE_DIR}"
	"${DRACO_INCLUDE_DIR}"
	${JPEG_INCLUDE_DIRS}
	${PNG_INCLUDE_DIRS}
)

target_link_directories(nxsbuild PRIVATE /home/ponchio/local/lib)
target_link_libraries(nxsbuild PRIVATE
	nexus
	"${METIS_LIBRARY}"
	corto
	Qt6::Core
	Qt6::Gui
	${JPEG_LIBRARIES}
	${PNG_LIBRARIES}
	"${DRACO_LIBRARY}"
	tf::trueform
)

install(TARGETS nxsbuild DESTINATION bin)
