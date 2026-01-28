cmake_minimum_required(VERSION 3.10)
project(nxsbuild)

# Option to choose matching algorithm
option(USE_HUNGARIAN "Use Hungarian algorithm instead of LEMON for matching" OFF)

set(SOURCES
	main.cpp
	build_parameters.cpp
	merge_simplify_split.cpp
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
    ../../vcglib/wrap/ply/plylib.cpp ../../vcglib/wrap/ply/plylib.h)

# Apply the matching algorithm choice
if(USE_HUNGARIAN)
    target_compile_definitions(nxsbuild PRIVATE USE_HUNGARIAN)
    message(STATUS "Using Hungarian algorithm for matching")
else()
    message(STATUS "Using LEMON library for matching")
endif()

# Find Qt5
find_package(Qt5 COMPONENTS Core REQUIRED)

# Add METIS and LEMON library paths
target_include_directories(nxsbuild PRIVATE 
    ../core
    ../../vcglib
	../
    /usr/include/eigen3
    /home/ponchio/local/include
    ../../lemon-graph-1.3.1
    ../../lemon-graph-1.3.1/build
)

target_link_directories(nxsbuild PRIVATE /home/ponchio/local/lib)
target_link_libraries(nxsbuild PRIVATE metis GKlib Qt5::Core)

install(TARGETS nxsbuild DESTINATION bin)
