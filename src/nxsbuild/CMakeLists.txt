cmake_minimum_required(VERSION 3.10)
project(nxsbuild)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to choose matching algorithm
option(USE_HUNGARIAN "Use Hungarian algorithm instead of LEMON for matching" OFF)

set(SOURCES
	main.cpp
	build_parameters.cpp
	merge_simplify_split.cpp
	export_nxs.cpp
	../texture/parametrization.cpp
	../texture/xatlas.cpp
	../texture/pyramid.cpp
	../texture/texture.cpp
	../core/mapped_file.cpp
	../core/mesh.cpp
	../core/mesh_hierarchy.cpp
	../core/vcgmesh.cpp
	../core/spatial_sort.cpp
	../core/adjacency.cpp
	../core/clustering.cpp
	../core/micro_clustering.cpp
	../core/hungarian.cpp
	../loaders/meshloader.cpp
	../loaders/objloader.cpp
	../loaders/objexporter.cpp
	../loaders/plyloader.cpp
	../loaders/plyexporter.cpp
)

set(HEADERS
	build_parameters.h
	merge_simplify_split.h
	export_nxs.h
	../texture/parametrization.h
	../texture/xatlas.h
	../texture/pyramid.h
	../texture/texture.h
	../core/mesh_types.h
	../core/mesh.h
	../core/mesh_hierarchy.h
	../core/vcgmesh.h
	../core/spatial_sort.h
	../core/adjacency.h
	../core/clustering.h
	../core/micro_clustering.h
	../loaders/meshloader.h
	../loaders/objloader.h
	../loaders/objexporter.h
	../loaders/plyloader.h
	../loaders/plyexporter.h
)

add_executable(nxsbuild ${SOURCES} ${HEADERS}
    ../../vcglib/wrap/ply/plylib.cpp ../../vcglib/wrap/ply/plylib.h
    ../core/adjacency.cpp ../core/material.cpp ../core/material.h
    ../texture/jpeg_decoder.cpp ../texture/jpeg_decoder.h)

# Apply the matching algorithm choice
if(USE_HUNGARIAN)
    target_compile_definitions(nxsbuild PRIVATE USE_HUNGARIAN)
    message(STATUS "Using Hungarian algorithm for matching")
else()
    message(STATUS "Using LEMON library for matching")
endif()

# Find Qt6
find_package(Qt6 COMPONENTS Core Gui REQUIRED)
find_package(JPEG REQUIRED)
# Add METIS and LEMON library paths
target_include_directories(nxsbuild PRIVATE 
    ../core
    ../../vcglib
    ../
    /usr/include/eigen3
    /home/ponchio/local/include
    ../../lemon-graph-1.3.1
    ../../lemon-graph-1.3.1/build
    ${JPEG_INCLUDE_DIR}
)

target_link_directories(nxsbuild PRIVATE /home/ponchio/local/lib)
target_link_libraries(nxsbuild PRIVATE metis GKlib Qt6::Core ${JPEG_LIBRARIES})

install(TARGETS nxsbuild DESTINATION bin)
