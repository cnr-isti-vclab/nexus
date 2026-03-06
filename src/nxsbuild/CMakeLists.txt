cmake_minimum_required(VERSION 3.10)
project(nxsbuild)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
	main.cpp
	build_parameters.cpp
	merge_simplify_split.cpp
	export_nxs.cpp
	../texture/parametrization.cpp
	../texture/xatlas.cpp
	../texture/pyramid.cpp
	../texture/texture.cpp
	../texture/rasterizer.cpp
	../texture/pushpull.cpp
	../texture/texture_cache.cpp
	../texture/uniform_grid.cpp
	../core/mapped_file.cpp
	../core/basemesh.cpp
	../core/mappedmesh.cpp
	../core/nodemesh.cpp
	../core/mesh_hierarchy.cpp
	../core/vcgmesh.cpp
	../core/spatial_sort.cpp
	../core/adjacency.cpp
	../core/clustering.cpp
	../core/micro_clustering.cpp
	../core/hungarian.cpp
	../loaders/meshloader.cpp
	../loaders/gltfloader.cpp
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
	../texture/rasterizer.h
	../texture/pushpull.h
	../texture/texture_cache.h
	../texture/uniform_grid.h
	../core/mesh_types.h
	../core/basemesh.h
	../core/mappedmesh.h
	../core/nodemesh.h
	../core/mesh_hierarchy.h
	../core/vcgmesh.h
	../core/spatial_sort.h
	../core/adjacency.h
	../core/clustering.h
	../core/micro_clustering.h
	../loaders/meshloader.h
	../loaders/gltfloader.h
	../loaders/objloader.h
	../loaders/objexporter.h
	../loaders/plyloader.h
	../loaders/plyexporter.h
)

add_executable(nxsbuild ${SOURCES} ${HEADERS}
    ../../vcglib/wrap/ply/plylib.cpp ../../vcglib/wrap/ply/plylib.h
    ../core/adjacency.cpp ../core/material.cpp ../core/material.h
    ../texture/jpeg_decoder.cpp ../texture/jpeg_decoder.h
    ../texture/png_decoder.cpp  ../texture/png_decoder.h)

# Find Qt6
find_package(Qt6 COMPONENTS Core Gui REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG  REQUIRED)

# Add METIS library paths
target_include_directories(nxsbuild PRIVATE 
    ../core
    ../../vcglib
    ../
    /usr/include/eigen3
    /home/ponchio/local/include
    ${JPEG_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
)

target_link_directories(nxsbuild PRIVATE /home/ponchio/local/lib)
target_link_libraries(nxsbuild PRIVATE metis Qt6::Core Qt6::Gui ${JPEG_LIBRARIES} ${PNG_LIBRARIES} draco)

install(TARGETS nxsbuild DESTINATION bin)
